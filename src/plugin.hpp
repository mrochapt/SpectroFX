#pragma once

#include "rack.hpp"

using namespace rack;

// Instância de plugin visível globalmente (fornecida por `init()`).
extern rack::Plugin* pluginInstance;

// Modelo do módulo principal (definido em plugin.cpp).
extern rack::Model* modelSpectroFXModule;
