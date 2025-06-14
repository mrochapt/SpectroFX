#pragma once
#include "rack.hpp"
#include "SpectroFXModule.hpp"

using namespace rack;

/**
 * Widget para desenhar o espectrograma como gráfico.
 * Mostra a magnitude do espectro (FFT) em cada ciclo de janela.
 */
struct SpectrogramDisplay : Widget {
    SpectroFXModule* module;
    SpectrogramDisplay(SpectroFXModule* module) {
        this->module = module;
        // Alinha com a área do espectrograma no SVG
        box.pos = mm2px(Vec(10, 20));
        box.size = mm2px(Vec(198.44, 48));
    }
    void draw(const DrawArgs& args) override {
        if (!module || !module->fftPlan) return;
        constexpr int N = SpectroFXModule::N;
        // Preenche o buffer da FFT com as amostras atuais
        for (int i = 0; i < N; i++) {
            module->input[i] = module->inputBuffer[(module->bufferIndex + i) % N];
        }
        fftw_execute(module->fftPlan);

        // Desenha a curva do espectrograma (magnitude das frequências)
        nvgBeginPath(args.vg);
        nvgMoveTo(args.vg, 0, box.size.y);
        for (int i = 0; i < N / 2; i++) {
            double re = module->output[i][0];
            double im = module->output[i][1];
            double mag = std::log(std::sqrt(re * re + im * im) + 1e-6) * 10.0;
            float x = (float)i / (N / 2 - 1) * box.size.x;
            float y = box.size.y - clamp(mag * 3.0, 0.0, box.size.y);
            nvgLineTo(args.vg, x, y);
        }
        nvgLineTo(args.vg, box.size.x, box.size.y);
        nvgClosePath(args.vg);
        nvgFillColor(args.vg, nvgRGB(50, 200, 255));
        nvgFill(args.vg);
    }
};

/**
 * Widget do painel principal do módulo.
 * Adiciona os knobs, entradas/saídas e o espectrograma.
 */
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
