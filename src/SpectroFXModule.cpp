#include "plugin.hpp"
#include "SpectroFXModule.hpp"
#include "SpectroFXWidget.hpp"
#include "PhaseEngine.hpp"
#include <thread>
#include <opencv2/opencv.hpp>

// Lê knob (L/R) + CV correspondente e mapeia para [0..1]
static inline float readCV(SpectroFXModule* m, int ch, int paramL, int paramR, int cvL, int cvR) {
    float base = m->params[ch==0 ? paramL : paramR].getValue();
    int cvId   = (ch==0 ? cvL : cvR);
    if (m->inputs[cvId].isConnected())
        base += 0.1f * m->inputs[cvId].getVoltage(); // 10 V -> +1.0
    return clamp(base, 0.f, 1.f);
}

// Atalho compatível com as chamadas existentes
#define CV(CH, PBASE, CBASE) readCV(this, (CH), PBASE##_L, PBASE##_R, CBASE##_L, CBASE##_R)

// Construtor: inicializa FFTW, janela √Hann, buffers e estado
SpectroFXModule::SpectroFXModule() {
    static bool fftw_threads_initialized = false;
    if (!fftw_threads_initialized) {
        fftw_init_threads();                // inicializa suporte a threads
        fftw_threads_initialized = true;    // apenas 1× globalmente
    }
    fftw_plan_with_nthreads(2);             // usa 2 threads por plano FFTW

    // Configuração de parâmetros/entradas/saídas/luzes
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(BLUR_PARAM_L,     0.f, 1.f, 0.f, "Blur (L)");
    configParam(BLUR_PARAM_R,     0.f, 1.f, 0.f, "Blur (R)");
    configParam(SHARPEN_PARAM_L,  0.f, 1.f, 0.f, "Sharpen (L)");
    configParam(SHARPEN_PARAM_R,  0.f, 1.f, 0.f, "Sharpen (R)");
    configParam(EDGE_PARAM_L,     0.f, 1.f, 0.f, "Edge Enhance (L)");
    configParam(EDGE_PARAM_R,     0.f, 1.f, 0.f, "Edge Enhance (R)");
    configParam(EMBOSS_PARAM_L,   0.f, 1.f, 0.f, "Emboss (L)");
    configParam(EMBOSS_PARAM_R,   0.f, 1.f, 0.f, "Emboss (R)");
    configParam(MIRROR_PARAM_L,   0.f, 1.f, 0.f, "Mirror (L)");
    configParam(MIRROR_PARAM_R,   0.f, 1.f, 0.f, "Mirror (R)");
    configParam(GATE_PARAM_L,     0.f, 1.f, 0.f, "Spectral Gate (L)");
    configParam(GATE_PARAM_R,     0.f, 1.f, 0.f, "Spectral Gate (R)");
    configParam(STRETCH_PARAM_L,  0.f, 1.f, 0.5f, "Spectral Stretch (L)");
    configParam(STRETCH_PARAM_R,  0.f, 1.f, 0.5f, "Spectral Stretch (R)");
    configParam(PHASE_MODE_PARAM, 0.f, 2.f, 0.f, "Phase mode (0=RAW, 1=PV, 2=PV-Lock)");

    // Planos FFTW
    for (int ch = 0; ch < 2; ++ch) {
        fftPlan[ch]  = fftw_plan_dft_r2c_1d(N, input[ch], output[ch], FFTW_MEASURE);    // FFT
        ifftPlan[ch] = fftw_plan_dft_c2r_1d(N, output[ch], input[ch], FFTW_MEASURE);    // IFFT
        inputWritePos[ch] = 0;              // posição de escrita no buffer circular
        outputWritePos[ch] = 0;             // posição de escrita no buffer circular
        outputReadPos[ch] = N;              // latência inicial ≈ N samples
        samplesSinceLastBlock[ch] = 0;      // contagem de amostras desde o último bloco
    }
    
    // PhaseEngine e buffers internos
    const int K = N / 2 + 1;                    // 513 bins com FFT de 1024
    phaseEngine.setup(2 /* canais */, K, H);    // hop H=N/2   

    // Máscara 2D
    mask2d.setup(HIST, K);              // HIST colunas, K bins (=N/2+1)

    magIn .assign(2, std::vector<float>(K, 0.f));   // magnitude da análise
    phaseIn.assign(2, std::vector<float>(K, 0.f));  // fase da análise
    magProc.assign(2, std::vector<float>(K, 0.f));  // magnitude processada
    specRe .assign(2, std::vector<float>(K, 0.f));  // espectro real da síntese
    specIm .assign(2, std::vector<float>(K, 0.f));  // espectro imag. da síntese

    // Janela √Hann periódica (análise + síntese) garantindo COLA (Constant OverLap Add) para H=N/2
    // significa que a soma das janelas sobrepostas é constante.    
    for (int i = 0; i < N; ++i) {
        double h = 0.5 * (1 - std::cos(2 * M_PI * i / N));
        hann[i] = std::sqrt(h);
    }
}

// Destrutor: limpa planos FFTW
SpectroFXModule::~SpectroFXModule() {
    for (int ch = 0; ch < 2; ++ch) {
        if (fftPlan[ch]) fftw_destroy_plan(fftPlan[ch]);
        if (ifftPlan[ch]) fftw_destroy_plan(ifftPlan[ch]);
    }
    fftw_cleanup_threads();
}

// Processamento principal por amostra com overlap‑add
void SpectroFXModule::process(const ProcessArgs& args) {
    float in[2];
    in[0] = inputs[AUDIO_INPUT_L].isConnected() ? inputs[AUDIO_INPUT_L].getVoltage() : 0.f;
    in[1] = inputs[AUDIO_INPUT_R].isConnected() ? inputs[AUDIO_INPUT_R].getVoltage() : 0.f;

    for (int ch = 0; ch < 2; ++ch) {
        // Entrada: escreve amostra no buffer circular
        inputBuffer[ch][inputWritePos[ch]] = in[ch];
        inputWritePos[ch] = (inputWritePos[ch] + 1) % (N * 2);
        samplesSinceLastBlock[ch]++;

        // Quando H amostras novas -> processa bloco
        if (samplesSinceLastBlock[ch] >= H) {
            // Prepara bloco de N amostras (com wrap-around) e aplica janela
            int start = (inputWritePos[ch] + (N * 2) - N) % (N * 2);
            for (int i = 0; i < N; ++i)
                input[ch][i]  = inputBuffer[ch][(start + i) % (N * 2)] * hann[i];    

            if (ch == 0) {
                mask2d.swapIfDirty();   // UI->DSP sem locks
            }

            // FFT -> FX -> IFFT
            fftw_execute(fftPlan[ch]);
            processChannel(ch);
            fftw_execute(ifftPlan[ch]);

            // Overlap‑add (IFFT já escalada por 1/N abaixo)
            for (int i = 0; i < N; ++i) {
                int pos = (outputWritePos[ch] + i) % (N * 2);   // posição circular
                double windowed = input[ch][i] * hann[i];       // reaplica janela √Hann
                outputBuffer[ch][pos] += windowed / N;          // escala 1/N
            }

            outputWritePos[ch] = (outputWritePos[ch] + H) % (N * 2);    // avança posição de escrita
            samplesSinceLastBlock[ch] = 0;                              // reinicia contagem    
        }

        // Saída processada (lê, zera, avança)
        double y = outputBuffer[ch][outputReadPos[ch]];
        outputBuffer[ch][outputReadPos[ch]] = 0;
        outputReadPos[ch] = (outputReadPos[ch] + 1) % (N * 2);

        // Headroom (-6 dB) para evitar clip em transientes
        y *= 0.5;

        // Soft‑limiter suave (tanh); desligável se não necessário
        const double drive = 1.2;                // 1.1–1.5
        y = std::tanh(drive * y) / std::tanh(drive);

        // DC‑block (HPF 1ª ordem): y[n] = x[n] − x[n−1] + R·y[n−1]
        // Corte ~ (1−R)*fs/(2π). Com R=0.995: ≈38 Hz @48 kHz; ≈35 Hz @44.1 kHz.
        const double R = 0.995; 
        double x0 = y;
        y = y - dc_x1[ch] + R * dc_y1[ch];  // y[n] = x[n] - x[n-1] + R*y[n-1]
        dc_x1[ch] = x0;                     // x[n-1] = x[n]
        dc_y1[ch] = y;                      // y[n-1] = y[n]    

        // Saídas: BYPASS entrega a entrada; PROCESSED entrega y
        float out = (float)y;       // conversão double->float
        outputs[ch == 0 ? PROCESSED_OUTPUT_L : PROCESSED_OUTPUT_R].setVoltage(out); // saída processada
        outputs[ch == 0 ? BYPASS_OUTPUT_L : BYPASS_OUTPUT_R].setVoltage(in[ch]);    // bypass
    }
}

// Pipeline FFT -> efeitos -> IFFT para um canal (ch=0 L, ch=1 R)
void SpectroFXModule::processChannel(int ch) {
    // Extrai magnitude e fase da FFT atual
    analyzeFFT(ch);

    const int K = N / 2 + 1;    // 513 bins com FFT de 1024

    // Copia magIn para cv::Mat para aplicar efeitos com OpenCV
    cv::Mat mag(1, K, CV_32F);
    for (int k = 0; k < K; ++k)
        mag.at<float>(0,k) = magIn[ch][k];

    // Leitura de parâmetros (com CV) mapeados para [0..1]
    float blurAmt     = CV(ch, BLUR_PARAM, BLUR_CV);
    float sharpAmt    = CV(ch, SHARPEN_PARAM, SHARPEN_CV);
    float edgeAmt     = CV(ch, EDGE_PARAM, EDGE_CV);
    float embossAmt   = CV(ch, EMBOSS_PARAM, EMBOSS_CV);
    float gateAmt     = CV(ch, GATE_PARAM, GATE_CV);
    float mirrorAmt   = CV(ch, MIRROR_PARAM, MIRROR_CV);
    float stretchAmt  = CV(ch, STRETCH_PARAM, STRETCH_CV);

    cv::Mat origMag = mag.clone();  // cópia para misturas

    // Função lambda que retorna 1 se o bin k estiver dentro da banda da máscara 2D
    auto inBand = [&](int k) -> float {
        if (!mask2d.enabled.load()) return 1.f;   // sem máscara -> aplica a toda a banda
        int lo = mask2d.lowBin.load(), hi = mask2d.highBin.load();  // limites                
        return (k >= lo && k <= hi) ? 1.f : 0.f;    // dentro da banda = 1, fora = 0
    };

    // --- EFEITOS ---
    // Blur: Gaussian blur, mistura ponderada pela máscara 2D
    if (blurAmt > 0.f) {
        cv::Mat before = mag.clone();
        cv::Mat blurred; 
        cv::GaussianBlur(mag, blurred, cv::Size(0,0), blurAmt * 12.0);
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = blurred.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Sharpen: kernel simples de afiação, mistura por máscara
    if (sharpAmt > 0.f) {
        cv::Mat before = mag.clone();
        cv::Mat sharp, kernel = (cv::Mat_<float>(3,3) << 0, -sharpAmt, 0, -sharpAmt, 1+4*sharpAmt, -sharpAmt, 0, -sharpAmt, 0);
        cv::filter2D(mag, sharp, -1, kernel);
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = sharp.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Edge Enhance: Sobel + mistura
    if (edgeAmt > 0.f) {
        cv::Mat before = mag.clone();
        cv::Mat edge; cv::Sobel(mag, edge, CV_32F, 1, 1, 3);
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = (1.f - edgeAmt) * a + edgeAmt * edge.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Emboss: relevo + mistura
    if (embossAmt > 0.f) {
        cv::Mat before = mag.clone(), emboss;
        cv::Mat kernel = (cv::Mat_<float>(3,3) << -2,-1,0, -1,1,1, 0,1,2);
        cv::filter2D(mag, emboss, -1, kernel);
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = (1.f - embossAmt)*a + embossAmt*emboss.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Gate: atenua magnitudes abaixo de um limiar relativo
    if (gateAmt > 0.f) {
        double maxv; cv::minMaxLoc(mag, nullptr, &maxv);
        float th = gateAmt * (float)maxv;
        for (int k = 0; k < K; ++k) {
            if (mag.at<float>(0,k) < th) {
                float w = inBand(k);
                mag.at<float>(0,k) *= (1.f - gateAmt * w);
            }
        }
    }

    // Mirror: espelha a magnitude e mistura por máscara
    if (mirrorAmt > 0.f) {
        cv::Mat before = mag.clone(), mirrored = mag.clone();
        int n = mag.cols;
        for (int i = 0; i < n/2; ++i) std::swap(mirrored.at<float>(0,i), mirrored.at<float>(0,n-1-i));
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = (1.f - mirrorAmt)*a + mirrorAmt*mirrored.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Stretch: estica/comprime no eixo de frequência e reamostra
    if (std::abs(stretchAmt - 0.5f) > 1e-3) {
        cv::Mat before = mag.clone(), stretched;
        float factor = 0.5f + stretchAmt;
        cv::resize(mag, stretched, cv::Size(), factor, 1.0, cv::INTER_LINEAR);
        cv::resize(stretched, mag, mag.size(), 0, 0, cv::INTER_LINEAR);
        for (int k = 0; k < K; ++k) {
            float w = inBand(k);
            float a = before.at<float>(0,k), b = mag.at<float>(0,k);
            mag.at<float>(0,k) = a * (1.f - w) + b * w;
        }
    }

    // Piso mínimo evita zeros que podem causar instabilidades de fase
    cv::threshold(mag, mag, 0.0, 0.0, cv::THRESH_TOZERO);
    const float eps = 1e-6f;
    for (int k = 0; k < K; ++k) {
        float m = mag.at<float>(0, k) + eps;
        processedMagnitude[ch][k] = m; // exposto ao widget
        magProc[ch][k]            = m;
    }

    // Modos RAW / PV / PV‑Lock: sintetiza com PhaseEngine
    synthesizeWithPhase(ch);

    // Copia specRe/specIm para 'output[ch]' para a IFFT deste hop
    for (int i = 0; i < K; ++i) {
        output[ch][i][0] = specRe[ch][i];
        output[ch][i][1] = specIm[ch][i];
    }
}

// Extrai magnitude e fase do espectro FFT atual
void SpectroFXModule::analyzeFFT(int ch) {
    const int K = N / 2 + 1;
    // Recolhe magnitude e fase do espectro atual
    for (int k = 0; k < K; ++k) {
        float re = output[ch][k][0];
        float im = output[ch][k][1];
        magIn[ch][k]   = std::sqrt(re*re + im*im);
        phaseIn[ch][k] = std::atan2(im, re);
    }
}

// Síntese com PhaseEngine segundo o modo selecionado
void SpectroFXModule::synthesizeWithPhase(int ch) {
    int modeIdx = (int) params[PHASE_MODE_PARAM].getValue();       
    PhaseEngine::Mode mode = PhaseEngine::Mode((uint8_t)modeIdx);   // 0=RAW, 1=PV, 2=PV-Lock
    phaseEngine.processFrame(ch, mode, magProc[ch].data(), phaseIn[ch].data(), specRe[ch].data(), specIm[ch].data());   // espectro complexo
}

// Registo do módulo na framework do VCV Rack
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXWidget>("SpectroFX");
