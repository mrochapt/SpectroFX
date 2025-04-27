#include "plugin.hpp"
#include "SpectroFXModule.hpp"

struct SpectroFXModuleWidget : ModuleWidget {
    SpectroFXModuleWidget(SpectroFXModule* module) {
        setModule(module);
        setPanel(APP->window->loadSvg(asset::plugin(pluginInstance, "res/SpectroFXModule.svg")));

        addInput(createInputCentered<PJ301MPort>(mm2px(Vec(10, 30)), module, SpectroFXModule::AUDIO_INPUT));
        addOutput(createOutputCentered<PJ301MPort>(mm2px(Vec(10, 70)), module, SpectroFXModule::AUDIO_OUTPUT));
    }
};

Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFXModule");
