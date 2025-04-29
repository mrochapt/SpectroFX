#include "plugin.hpp"
#include "SpectroFXModule.hpp"

struct SpectrogramDisplay : Widget {
    SpectroFXModule* module;
    SpectrogramDisplay(SpectroFXModule* module) {
        this->module = module;
        box.pos = mm2px(Vec(20, 15));
        box.size = mm2px(Vec(195, 98));
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
    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFX.svg")));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 30)), module, SpectroFXModule::AUDIO_INPUT_1));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 50)), module, SpectroFXModule::AUDIO_OUTPUT_1));
        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 70)), module, SpectroFXModule::AUDIO_INPUT_2));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 90)), module, SpectroFXModule::AUDIO_OUTPUT_2));

        SpectrogramDisplay* spectrogram = new SpectrogramDisplay(module);
        addChild(spectrogram);
    }
};

Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
