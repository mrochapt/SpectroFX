#pragma once

#include "rack.hpp"

using namespace rack;

// Declaração externa usada por vários ficheiros para aceder ao plugin
extern rack::Plugin* pluginInstance;

// Registo do modelo principal (definido em plugin.cpp)
extern rack::Model* modelSpectroFXModule;
