#include "plugin.hpp"
#include "SpectroFXModule.hpp"

// Ponteiro global para a instância do plugin.
Plugin* pluginInstance;

// Função obrigatória chamada pelo Rack ao carregar o plugin.
void init(Plugin* p) {
    pluginInstance = p;                         // Armazena a instância do plugin globalmente
    p->addModel(modelSpectroFXModule);          // Regista o módulo "SpectroFX" no sistema do VCV Rack
}
