#pragma once
#include "rack.hpp"
#include "SpectroFXModule.hpp"

using namespace rack;

// Widget para desenhar o espectrograma como gráfico.
// Mostra a magnitude do espectro (FFT) em cada ciclo de janela.
struct SpectrogramDisplay : Widget {
    SpectroFXModule* module;

    static constexpr int HISTORY_SIZE = 256; // número de frames de histórico
    std::vector<std::vector<float>> spectroHistory;
    int spectroPos = 0;

    SpectrogramDisplay(SpectroFXModule* module) 
        : spectroHistory(HISTORY_SIZE, std::vector<float>(SpectroFXModule::N / 2 + 1, 0.f)) {
        this->module = module;
        // Alinha com a área do espectrograma no SVG
        box.pos = mm2px(Vec(10, 20));
        box.size = mm2px(Vec(198.44, 48));
    }

    void draw(const DrawArgs& args) override {
        if (!module || !module->fftPlan) return;
        constexpr int N = SpectroFXModule::N;

        // Guarda a magnitude mais recente no histórico
        for (int i = 0; i < N/2 + 1; ++i) {
            spectroHistory[spectroPos][i] = module->processedMagnitude[i];
        }
        spectroPos = (spectroPos + 1) % HISTORY_SIZE;

        // Desenha o heatmap 2D (azul a vermelho)
        for (int t = 0; t < HISTORY_SIZE; ++t) {
            int idx = (spectroPos + t) % HISTORY_SIZE;
            float x = (float)t / HISTORY_SIZE * box.size.x;
            for (int f = 0; f < N / 2; ++f) {
                float mag = spectroHistory[idx][f];
                float norm = std::log(mag + 1e-6f) * 0.18f;
                norm = clamp(norm, 0.f, 1.f);

                // HSV: azul (h=0.66) até vermelho (h=0.0)
                NVGcolor c = nvgHSLA(0.66f - norm * 0.66f, 1.0f, norm * 0.6f + 0.15f, 255);
                float y = box.size.y - ((float)f / (N / 2) * box.size.y);

                nvgBeginPath(args.vg);
                nvgRect(args.vg, x, y, box.size.x / HISTORY_SIZE + 1, box.size.y / (N / 2) + 1);
                nvgFillColor(args.vg, c);
                nvgFill(args.vg);
            }
        }

        // Linha branca opcional para o espectro atual (última coluna)
        nvgBeginPath(args.vg);
        for (int i = 0; i < N / 2; i++) {
            float mag = module->processedMagnitude[i];
            float x = (float)(HISTORY_SIZE - 1) / HISTORY_SIZE * box.size.x;
            float y = box.size.y - clamp(std::log(mag + 1e-6f) * 30.0f, 0.0f, box.size.y);
            if (i == 0)
                nvgMoveTo(args.vg, x, y);
            else
                nvgLineTo(args.vg, x, y);
        }
        nvgStrokeColor(args.vg, nvgRGB(255, 255, 255));
        nvgStrokeWidth(args.vg, 1.0f);
        nvgStroke(args.vg);
    }
};


// Widget do painel principal do módulo.
// Adiciona os knobs, entradas/saídas e o espectrograma.
struct SpectroFXModuleWidget : ModuleWidget {
    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFX.svg")));

        // Knobs para cada efeito (posicionados de acordo com o SVG)
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(25, 85)), module, SpectroFXModule::BLUR_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(52, 85)), module, SpectroFXModule::SHARPEN_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(79, 85)), module, SpectroFXModule::EDGE_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(106, 85)), module, SpectroFXModule::EMBOSS_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(133, 85)), module, SpectroFXModule::MIRROR_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(160, 85)), module, SpectroFXModule::GATE_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(187, 85)), module, SpectroFXModule::STRETCH_PARAM));

        // Entradas/saídas (audio in, bypass out, processed out)
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(35, 115)), module, SpectroFXModule::AUDIO_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(109.22, 115)), module, SpectroFXModule::BYPASS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(183, 115)), module, SpectroFXModule::PROCESSED_OUTPUT));

        // Espectrograma visual (widget desenhado acima)
        addChild(new SpectrogramDisplay(module));
    }
};
