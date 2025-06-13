#pragma once
#include "rack.hpp"
#include "SpectroFXModule.hpp"

using namespace rack;

struct SpectrogramDisplay : Widget {
    SpectroFXModule* module;
    SpectrogramDisplay(SpectroFXModule* module) {
        this->module = module;
        box.pos = mm2px(Vec(10, 20)); // (x, y) do canto superior esquerdo
        box.size = mm2px(Vec(198.44, 48)); // largura x altura
    }
    void draw(const DrawArgs& args) override {
        if (!module || !module->fftPlan) return;
        constexpr int N = SpectroFXModule::N;
        for (int i = 0; i < N; i++) {
            module->input[i] = module->inputBuffer[(module->bufferIndex + i) % N];
        }
        fftw_execute(module->fftPlan);

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

struct SpectroFXModuleWidget : ModuleWidget {
    static constexpr const char* effectNames[6] = {
        "Normal", "Pixelate", "Gate", "Invert", "Mirror", "Freeze"
    };

    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFX.svg")));

        // Knobs BLUR / SHARP
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(38, 85)), module, SpectroFXModule::BLUR_PARAM));
        addParam(createParamCentered<RoundLargeBlackKnob>(mm2px(Vec(80, 85)), module, SpectroFXModule::SHARPEN_PARAM));

        // Linha de botões LED dos efeitos com labels extensos
        addParam(createParamCentered<LEDButton>(mm2px(Vec(112, 80)), module, SpectroFXModule::EFFECT0_PARAM)); // Normal
        addParam(createParamCentered<LEDButton>(mm2px(Vec(130, 80)), module, SpectroFXModule::EFFECT1_PARAM)); // Pixelate
        addParam(createParamCentered<LEDButton>(mm2px(Vec(148, 80)), module, SpectroFXModule::EFFECT2_PARAM)); // Gate
        addParam(createParamCentered<LEDButton>(mm2px(Vec(166, 80)), module, SpectroFXModule::EFFECT3_PARAM)); // Invert
        addParam(createParamCentered<LEDButton>(mm2px(Vec(184, 80)), module, SpectroFXModule::EFFECT4_PARAM)); // Mirror
        addParam(createParamCentered<LEDButton>(mm2px(Vec(202, 80)), module, SpectroFXModule::EFFECT5_PARAM)); // Freeze

        // Jacks IN, BYP, FX
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(30, 115)), module, SpectroFXModule::AUDIO_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(109.22, 115)), module, SpectroFXModule::BYPASS_OUTPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(190, 115)), module, SpectroFXModule::PROCESSED_OUTPUT));

        // Espectrograma visual (mesmas dimensões do retângulo SVG)
        addChild(new SpectrogramDisplay(module));

        // Label do efeito ativo
        addChild(new LabelDisplay(module));
    }

    struct LabelDisplay : Widget {
        SpectroFXModule* module;
        LabelDisplay(SpectroFXModule* module) : module(module) {
            box.pos = mm2px(Vec(157, 97)); // Fica por cima dos botões de efeito, ajusta a gosto!
            box.size = Vec(60, 15);
        }
        void draw(const DrawArgs& args) override {
            int active = 0;
            if (module) {
                for (int i = 0; i < 6; ++i)
                    if (module->params[SpectroFXModule::EFFECT0_PARAM + i].getValue() > 0.5f)
                        active = i;
            }
            nvgFontSize(args.vg, 13.f);
            nvgFontFaceId(args.vg, APP->window->uiFont->handle);
            nvgFillColor(args.vg, nvgRGB(220, 220, 220));
            nvgTextAlign(args.vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
            nvgText(args.vg, box.pos.x + 30, box.pos.y + 7, effectNames[active], nullptr);
        }
    };
};
constexpr const char* SpectroFXModuleWidget::effectNames[];
