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
    // Lê a amostra de entrada
    float in = inputs[AUDIO_INPUT].isConnected() ? inputs[AUDIO_INPUT].getVoltage() : 0.f;
    // Envia sempre o input para a saída BYPASS
    outputs[BYPASS_OUTPUT].setVoltage(in);

    // Escreve a amostra no buffer circular de entrada
    inputBuffer[bufferIndex] = in;
    bufferIndex = (bufferIndex + 1) % N;

    // Apenas processa o bloco FFT quando o buffer circular está cheio (N amostras)
    if (bufferIndex == 0) {
        // Preenche o buffer da FFTW com as amostras atuais
        for (int i = 0; i < N; i++)
            input[i] = inputBuffer[i];

        // Lê os valores dos knobs dos efeitos
        float blurAmt     = params[BLUR_PARAM].getValue();
        float sharpAmt    = params[SHARPEN_PARAM].getValue();
        float edgeAmt     = params[EDGE_PARAM].getValue();
        float embossAmt   = params[EMBOSS_PARAM].getValue();
        float gateAmt     = params[GATE_PARAM].getValue();
        float mirrorAmt   = params[MIRROR_PARAM].getValue();
        float stretchAmt  = params[STRETCH_PARAM].getValue();

        // Se TODOS os knobs estão a zero, faz bypass real (direto)
        if (blurAmt == 0.f && sharpAmt == 0.f && edgeAmt == 0.f && embossAmt == 0.f &&
            gateAmt == 0.f && mirrorAmt == 0.f && stretchAmt == 0.f) {
            // Bypass: cópia direta do input para o buffer de saída FX
            for (int i = 0; i < N; ++i)
                processedBuffer[i] = inputBuffer[i];
        } else {
            // Processamento FFT/IFFT
            fftw_execute(fftPlan);

            // Extrai magnitude e fase para cada bin
            cv::Mat mag(1, N/2+1, CV_32F), phase(1, N/2+1, CV_32F);
            for (int i = 0; i < N/2+1; i++) {
                float re = output[i][0];
                float im = output[i][1];
                mag.at<float>(0,i)   = std::sqrt(re*re + im*im);
                phase.at<float>(0,i) = std::atan2(im, re);
            }

            // Guarda magnitude original para blends
            cv::Mat origMag = mag.clone();

            // --------- EFEITOS GRADUAIS ---------

            // 1. Blur
            if (blurAmt > 0.f)
                cv::GaussianBlur(mag, mag, cv::Size(0,0), blurAmt * 12.0);

            // 2. Sharpen
            if (sharpAmt > 0.f) {
                cv::Mat sharp;
                cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                    0, -sharpAmt, 0,
                    -sharpAmt, 1+4*sharpAmt, -sharpAmt,
                    0, -sharpAmt, 0);
                cv::filter2D(mag, sharp, -1, kernel);
                mag = mag * (1.0f - sharpAmt) + sharp * sharpAmt;
            }

            // 3. Edge Enhance
            if (edgeAmt > 0.f) {
                cv::Mat edge;
                cv::Sobel(mag, edge, CV_32F, 1, 1, 3);
                mag = mag * (1.0f - edgeAmt) + edge * edgeAmt;
            }

            // 4. Emboss
            if (embossAmt > 0.f) {
                cv::Mat emboss;
                cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                    -2, -1, 0,
                    -1,  1, 1,
                     0,  1, 2);
                cv::filter2D(mag, emboss, -1, kernel);
                mag = mag * (1.0f - embossAmt) + emboss * embossAmt;
            }

            // 5. Spectral Gate
            float gateAmt = params[GATE_PARAM].getValue();
            if (gateAmt > 0.f) {
                double maxv;
                cv::minMaxLoc(mag, nullptr, &maxv);
                float threshold = gateAmt * maxv;
                for (int i = 0; i < mag.cols; ++i) {
                    float& v = mag.at<float>(0, i);
                    if (v < threshold)
                        v *= 1.f - gateAmt; // atenua bandas fracas
                }
            }

            // 6. Mirror
            if (mirrorAmt > 0.f) {
                cv::Mat mirrored = mag.clone();
                int n = mag.cols;
                for (int i = 0; i < n/2; ++i) {
                    float tmp = mirrored.at<float>(0,i);
                    mirrored.at<float>(0,i) = mirrored.at<float>(0,n-1-i);
                    mirrored.at<float>(0,n-1-i) = tmp;
                }
                mag = mag * (1.0f - mirrorAmt) + mirrored * mirrorAmt;
            }

            // 7. Spectral Stretch
            float stretchAmt = params[STRETCH_PARAM].getValue();
            if (std::abs(stretchAmt - 0.5f) > 1e-3) { // 0.5 = sem efeito
                float factor = 0.5f + stretchAmt; // 0.5x a 1.5x
                cv::Mat stretched;
                cv::resize(mag, stretched, cv::Size(), factor, 1.0, cv::INTER_LINEAR);
                cv::resize(stretched, mag, mag.size(), 0, 0, cv::INTER_LINEAR);
            }

            // --------- RECONSTRUÇÃO DO ÁUDIO ---------
            // Recombina magnitude e fase e executa a IFFT
            for (int i = 0; i < N/2+1; i++) {
                float m = mag.at<float>(0,i);
                float ph = phase.at<float>(0,i);
                output[i][0] = m * std::cos(ph);
                output[i][1] = m * std::sin(ph);
            }
            fftw_execute(ifftPlan);

            // Normaliza e guarda no buffer de saída processada
            for (int i = 0; i < N; ++i)
                processedBuffer[i] = input[i] / N;
        }
        // Após processar um bloco, reinicia ponteiro de leitura
        readPtr = 0;
    }

    // Lê o próximo sample do buffer processado e envia para a saída FX
    float outProc = processedBuffer[readPtr];
    outputs[PROCESSED_OUTPUT].setVoltage(outProc);
    readPtr = (readPtr + 1) % N;
}

// Regista o módulo na framework do VCV Rack
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
