#include "plugin.hpp"
#include "SpectroFXModule.hpp"

Plugin* pluginInstance; // Ponteiro global para o plugin

// Função obrigatória de inicialização do plugin
void init(Plugin* p) {
    pluginInstance = p; // Guarda instância do plugin
    p->addModel(modelSpectroFXModule); // Regista o modelo SpectroFX
}
