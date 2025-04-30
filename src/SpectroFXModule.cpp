#include "plugin.hpp"
#include "SpectroFXModule.hpp"

// Widget para desenhar o espectrograma
struct SpectrogramDisplay : Widget {
    SpectroFXModule* module; // Referência ao módulo para aceder aos dados da FFT

    SpectrogramDisplay(SpectroFXModule* module) {
        this->module = module;
        box.pos = mm2px(Vec(20, 15));    // Posição do widget dentro do painel
        box.size = mm2px(Vec(195, 98));  // Tamanho do widget do espectrograma
    }

    // Função que desenha o espectrograma no painel
    void draw(const DrawArgs& args) override {
        if (!module || !module->fftPlan) return;
        constexpr int N = SpectroFXModule::N;

        // Copia os dados do buffer circular para o array usado pelo FFT
        for (int i = 0; i < N; i++) {
            module->input[i] = module->inputBuffer[(module->bufferIndex + i) % N];
        }

        // Executa o plano FFT usando FFTW
        fftw_execute(module->fftPlan);

        // Inicia o desenho do caminho gráfico do espectrograma
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, box.size.y); // Começa a desenhar na base do espectrograma

        // Percorre todos os bins da FFT (N/2 é suficiente para sinal real)
        for (int i = 0; i < N / 2; i++) {
            double re = module->output[i][0];  // Parte real do bin
            double im = module->output[i][1];  // Parte imaginária do bin

            // Calcula magnitude logarítmica
            double mag = std::log(std::sqrt(re * re + im * im) + 1e-6) * 10.0;

            float x = (float)i / (N / 2 - 1) * box.size.x;              // Posição horizontal proporcional
            float y = box.size.y - clamp(mag * 3.0, 0.0, box.size.y);   // Posição vertical invertida
            nvgLineTo(args.vg, x, y);                                   // Adiciona ponto à linha
        }

        // Fecha a forma visual e preenche com cor
        nvgLineTo(args.vg, box.size.x, box.size.y);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(50, 200, 255)); // Azul claro
        nvgFill(args.vg);
    }
};

// Widget visual do módulo no Rack
struct SpectroFXModuleWidget : ModuleWidget {
    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module); // Associa a lógica do módulo à interface

        // Define o painel SVG
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFX.svg")));

        // Entradas e saídas de áudio
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 30)), module, SpectroFXModule::AUDIO_INPUT_1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 50)), module, SpectroFXModule::AUDIO_OUTPUT_1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 70)), module, SpectroFXModule::AUDIO_INPUT_2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 90)), module, SpectroFXModule::AUDIO_OUTPUT_2));

        // Adiciona o espectrograma ao painel
        SpectrogramDisplay* spectrogram = new SpectrogramDisplay(module);
        addChild(spectrogram);
    }
};

// Cria o modelo para ser registado no plugin.cpp
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
