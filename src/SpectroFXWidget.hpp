#pragma once

#include "SpectroFXModule.hpp"
#include <rack.hpp>

// Declaramos a classe do nosso widget aqui
struct SpectroFXModuleWidget : rack::ModuleWidget {
    // Construtor que recebe ponteiro para o módulo DSP
    SpectroFXModuleWidget(SpectroFXModule* module);
};
