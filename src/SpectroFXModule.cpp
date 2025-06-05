#include "plugin.hpp"
#include "SpectroFXModule.hpp"

// Função utilitária para limitar valores entre dois extremos
template <typename T>
T clamp(T v, T lo, T hi) {
    if (v < lo) return lo;
    if (v > hi) return hi;
    return v;
}

// --- Widget de visualização do espectrograma (painel gráfico) ---
struct SpectrogramDisplay : Widget {
    SpectroFXModule* module; // Ponteiro para o módulo associado

    SpectrogramDisplay(SpectroFXModule* module) {
        this->module = module;
        // Define posição e tamanho do widget no painel (em mm)
        box.pos = mm2px(Vec(20, 15));
        box.size = mm2px(Vec(195, 98));
    }

    void draw(const DrawArgs& args) override {
        if (!module)
            return;
        // Desenha a última linha do processedImage como espectro
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, box.size.y);
        for (int i = 0; i < SPECTRO_WIDTH; i++) {
            float mag = 0;
            if (!module->processedImage.empty())
                mag = module->processedImage.at<uchar>(SPECTRO_HEIGHT - 1, i) / 255.f;
            float x = (float)i / (SPECTRO_WIDTH - 1) * box.size.x;
            float y = box.size.y - mag * box.size.y;
            nvgLineTo(args.vg, x, y);
        }
        nvgLineTo(args.vg, box.size.x, box.size.y);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(50, 200, 255));
        nvgFill(args.vg);
    }
};

// --- Construtor: inicializa buffers, FFTW e janela Hanning ---
SpectroFXModule::SpectroFXModule() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS); // Inicializa parâmetros básicos do módulo
    configParam(EFFECT_TYPE_PARAM, 0.f, 3.f, 0.f, "Effect Type"); // Configura knob de efeito

    // Cria planos FFTW para FFT e IFFT
    fftPlan = fftw_plan_dft_r2c_1d(N, inputBuffer, fftOut, FFTW_ESTIMATE);
    ifftPlan = fftw_plan_dft_c2r_1d(N, fftIn, outputBuffer, FFTW_ESTIMATE);

    // Inicializa buffers de entrada, saída e magnitude
    std::fill(inputBuffer, inputBuffer + N, 0.0);
    std::fill(outputBuffer, outputBuffer + N, 0.0);
    std::fill(magnitude, magnitude + SPECTRO_WIDTH, 0.0);

    // Inicializa janela Hanning 
    for (int i = 0; i < N; ++i)
        hanning[i] = 0.5 - 0.5 * cos(2 * M_PI * i / (N - 1));

    outputIndex = 0; // Posição inicial do buffer de saída
    bufferIndex = 0; // Posição inicial do buffer de entrada
}

// --- Destrutor: Liberta planos FFTW ---
SpectroFXModule::~SpectroFXModule() {
    fftw_destroy_plan(fftPlan);
    fftw_destroy_plan(ifftPlan);
}

// --- Função principal de processamento por bloco ---
void SpectroFXModule::process(const ProcessArgs& args) {
    float in1 = inputs[AUDIO_INPUT_1].getVoltage(); // Lê amostra de áudio do input

    // Output 2: bypass puro (input direto, sample a sample)
    outputs[AUDIO_OUTPUT_2].setVoltage(in1);

    // Adiciona a amostra ao buffer circular de entrada
    inputBuffer[bufferIndex] = static_cast<double>(in1);
    bufferIndex++;

    // Lê valor do knob de efeitos (param)
    int effectType = static_cast<int>(params[EFFECT_TYPE_PARAM].getValue());

    // Quando o buffer atinge N amostras, processa o bloco
    if (bufferIndex == N) {
        // Aplica janela Hanning ao bloco
        double windowedBuffer[N];
        for (int i = 0; i < N; ++i)
            windowedBuffer[i] = inputBuffer[i] * hanning[i];

        // Executa FFT real->complexo
        fftw_execute_dft_r2c(fftPlan, windowedBuffer, fftOut);

        // Calcula a magnitude dos bins válidos (0 ... SPECTRO_WIDTH-1)
        for (int i = 0; i < SPECTRO_WIDTH; i++)
            magnitude[i] = std::sqrt(fftOut[i][0] * fftOut[i][0] + fftOut[i][1] * fftOut[i][1]);

        // Atualiza espectrograma (shift + insere nova linha de magnitudes)
        cv::Mat newRow(1, SPECTRO_WIDTH, CV_8UC1);
        for (int i = 0; i < SPECTRO_WIDTH; i++) {
            double val = std::log(magnitude[i] + 1e-6) * 20.0; // Escala logarítmica
            val = clamp(val, 0.0, 10.0);
            newRow.at<uchar>(0, i) = static_cast<uchar>(val * 25.5); // Escala para [0,255]
        }
        spectrogramImage.rowRange(1, SPECTRO_HEIGHT).copyTo(spectrogramImage.rowRange(0, SPECTRO_HEIGHT - 1));
        newRow.copyTo(spectrogramImage.row(SPECTRO_HEIGHT - 1));

        // Aplica efeito OpenCV ou só clone (consoante o efeito)
        switch (effectType) {
            case 1:
                cv::GaussianBlur(spectrogramImage, processedImage, cv::Size(7, 7), 1.5);
                break;
            case 2: {
                cv::Mat kernel = (cv::Mat_<float>(3, 3) <<
                     0, -1,  0,
                    -1,  5, -1,
                     0, -1,  0);
                cv::filter2D(spectrogramImage, processedImage, -1, kernel);
                break;
            }
            case 3:
                cv::Laplacian(spectrogramImage, processedImage, CV_8U);
                break;
            default:
                spectrogramImage.copyTo(processedImage);
        }
        // Garante tipo de imagem correto (8 bits por pixel)
        if (processedImage.type() != CV_8UC1)
            processedImage.convertTo(processedImage, CV_8UC1);

        // Reconstrói espectro: magnitude manipulada + fase original 
        for (int i = 0; i < SPECTRO_WIDTH; i++) {
            double mag = static_cast<double>(processedImage.at<uchar>(SPECTRO_HEIGHT - 1, i)) / 255.0;
            double phase = std::atan2(fftOut[i][1], fftOut[i][0]);
            fftIn[i][0] = mag * std::cos(phase);
            fftIn[i][1] = mag * std::sin(phase);
        }

        // Executa IFFT (complexo->real)
        fftw_execute_dft_c2r(ifftPlan, fftIn, outputBuffer);

        // Normaliza o resultado (FFTW não normaliza)
        for (int i = 0; i < N; i++)
            outputBuffer[i] /= N;

        outputIndex = 0;   // Reinicia leitura do bloco de saída
        bufferIndex = 0;   // Reinicia escrita no buffer de entrada
    }

    // Output 1: fornece sample a sample do bloco processado
    if (outputIndex < N) {
        outputs[AUDIO_OUTPUT_1].setVoltage(outputBuffer[outputIndex]);
        outputIndex++;
    } else {
        outputs[AUDIO_OUTPUT_1].setVoltage(0.0f); // Zero se bloco acabou
    }
}

// --- Widget/interface do módulo ---
struct SpectroFXModuleWidget : ModuleWidget {
    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFX.svg")));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 30)), module, SpectroFXModule::AUDIO_INPUT_1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 50)), module, SpectroFXModule::AUDIO_OUTPUT_1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 90)), module, SpectroFXModule::AUDIO_OUTPUT_2));

        // Botão para escolher efeito
        addParam(createParamCentered<RoundBlackKnob>(
            mm2px(Vec(10, 110)), module, SpectroFXModule::EFFECT_TYPE_PARAM));

        // Widget do espectrograma (adiciona painel gráfico ao módulo)
        SpectrogramDisplay* spectrogram = new SpectrogramDisplay(module);
        addChild(spectrogram);
    }
};

// Declaração do modelo do plugin para o VCV Rack
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
