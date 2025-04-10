#include "SpectroFXModule.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;
    // Registra o módulo (ainda falta o widget e etc.)
    p->addModel(createModel<SpectroFXModule, ModuleWidget>("SpectroFXModule"));
}
