#include "plugin.hpp"
#include "SpectroFXModule.hpp"
#include "SpectroFXWidget.hpp"
#include <opencv2/opencv.hpp>

/**
 * Função principal de processamento de áudio.
 * Aplica FFT ao sinal de entrada, manipula o espectrograma com efeitos,
 * reconstrói o áudio via IFFT e atualiza as saídas.
 */
void SpectroFXModule::process(const ProcessArgs& args) {
    // Passa diretamente o áudio para o output BYPASS (não processado)
    float in = inputs[AUDIO_INPUT].isConnected() ? inputs[AUDIO_INPUT].getVoltage() : 0.f;
    outputs[BYPASS_OUTPUT].setVoltage(in);

    // Escreve a amostra de entrada no buffer circular
    inputBuffer[bufferIndex] = in;
    bufferIndex = (bufferIndex + 1) % N;

    // Só processa uma janela quando o buffer está cheio (block processing)
    if (bufferIndex == 0) {
        // Preenche o buffer para a FFT
        for (int i = 0; i < N; i++)
            input[i] = inputBuffer[i];

        // Executa a FFT
        fftw_execute(fftPlan);

        // Extrai magnitude e fase para cada bin do espectro
        cv::Mat mag(1, N/2+1, CV_32F), phase(1, N/2+1, CV_32F);
        for (int i = 0; i < N/2+1; i++) {
            float re = output[i][0];
            float im = output[i][1];
            mag.at<float>(0,i)   = std::sqrt(re*re + im*im);
            phase.at<float>(0,i) = std::atan2(im, re);
        }
        // Guarda uma cópia da magnitude original (para blends)
        cv::Mat origMag = mag.clone();

        // --------- EFEITOS GRADUAIS APLICADOS POR ORDEM ---------

        // 1. Blur (Desfoque Gaussiano)
        float blurAmt = params[BLUR_PARAM].getValue();
        if (blurAmt > 0.f)
            cv::GaussianBlur(mag, mag, cv::Size(0,0), blurAmt * 12.0);

        // 2. Sharpen (Aguçar)
        float sharpAmt = params[SHARPEN_PARAM].getValue();
        if (sharpAmt > 0.f) {
            cv::Mat sharp;
            // Kernel clássico de Sharpen com ganho ajustável
            cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                0, -sharpAmt, 0,
                -sharpAmt, 1+4*sharpAmt, -sharpAmt,
                0, -sharpAmt, 0);
            cv::filter2D(mag, sharp, -1, kernel);
            // Blend gradual
            mag = mag * (1.0f - sharpAmt) + sharp * sharpAmt;
        }

        // 3. Edge Enhance (Realce de contornos)
        float edgeAmt = params[EDGE_PARAM].getValue();
        if (edgeAmt > 0.f) {
            cv::Mat edge;
            // Filtro Sobel: realça diferenças abruptas
            cv::Sobel(mag, edge, CV_32F, 1, 1, 3);
            mag = mag * (1.0f - edgeAmt) + edge * edgeAmt;
        }

        // 4. Emboss (Relevo)
        float embossAmt = params[EMBOSS_PARAM].getValue();
        if (embossAmt > 0.f) {
            cv::Mat emboss;
            // Kernel clássico de Emboss
            cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                -2, -1, 0,
                -1,  1, 1,
                 0,  1, 2);
            cv::filter2D(mag, emboss, -1, kernel);
            mag = mag * (1.0f - embossAmt) + emboss * embossAmt;
        }

        // 5. Invert (Inverter espectro)
        float invertAmt = params[INVERT_PARAM].getValue();
        if (invertAmt > 0.f) {
            double maxv;
            cv::minMaxLoc(mag, nullptr, &maxv);
            cv::Mat inv = maxv - mag; // Espectro invertido
            mag = mag * (1.0f - invertAmt) + inv * invertAmt;
        }

        // 6. Mirror (Espelhar espectro)
        float mirrorAmt = params[MIRROR_PARAM].getValue();
        if (mirrorAmt > 0.f) {
            cv::Mat mirrored = mag.clone();
            int n = mag.cols;
            // Espelha as colunas da matriz (espectro)
            for (int i = 0; i < n/2; ++i) {
                float tmp = mirrored.at<float>(0,i);
                mirrored.at<float>(0,i) = mirrored.at<float>(0,n-1-i);
                mirrored.at<float>(0,n-1-i) = tmp;
            }
            mag = mag * (1.0f - mirrorAmt) + mirrored * mirrorAmt;
        }

        // 7. Quantize (Quantização do espectro)
        float quantizeAmt = params[QUANTIZE_PARAM].getValue();
        if (quantizeAmt > 0.f) {
            double minv, maxv;
            cv::minMaxLoc(mag, &minv, &maxv);
            int levels = 1 + (int)(quantizeAmt * 15); // de 1 (sem efeito) a 16 níveis (max effect)
            // Quantização linear dos valores de magnitude
            cv::Mat quantized = ((mag - minv) / (maxv - minv + 1e-9)) * (levels-1);
            quantized = quantized.mul(1.0/(levels-1));
            quantized = quantized * (maxv - minv) + minv;
            mag = mag * (1.0f - quantizeAmt) + quantized * quantizeAmt;
        }

        // --------- RECONSTRUÇÃO DO ÁUDIO ---------
        // Combina as magnitudes processadas com as fases originais,
        // para cada bin da FFT, e faz IFFT para obter áudio
        for (int i = 0; i < N/2+1; i++) {
            float m = mag.at<float>(0,i);
            float ph = phase.at<float>(0,i);
            output[i][0] = m * std::cos(ph);
            output[i][1] = m * std::sin(ph);
        }
        // Executa a IFFT
        fftw_execute(ifftPlan);

        // Copia o áudio processado para o buffer de saída, normalizando
        for (int i = 0; i < N; ++i)
            processedBuffer[i] = input[i] / N;
        readPtr = 0; // Reinicia ponteiro de leitura
    }

    // Atualiza saída processada sample a sample (interpolação entre blocos)
    float outProc = processedBuffer[readPtr];
    outputs[PROCESSED_OUTPUT].setVoltage(outProc);
    readPtr = (readPtr + 1) % N;
}

// Regista o módulo na framework do VCV Rack
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
