#include "SpectroFXModule.hpp"
#include "SpectroFXWidget.hpp"

Plugin* pluginInstance = nullptr;

void init(Plugin* p) {
    pluginInstance = p;
   
    p->addModel(createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFXModule"));
}
