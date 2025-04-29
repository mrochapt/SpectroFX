#include "plugin.hpp"
#include "SpectroFXModule.hpp"

Plugin* pluginInstance;

void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelSpectroFXModule);
}
