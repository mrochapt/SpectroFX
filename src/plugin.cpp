#include "plugin.hpp"
#include "SpectroFXModule.hpp"

// Ponteiro global para a instância do plugin.
Plugin* pluginInstance;

// Chamado pelo Rack ao carregar o plugin.
void init(Plugin* p) {
    pluginInstance = p;
    p->addModel(modelSpectroFXModule); // Regista o módulo "SpectroFX"
}
