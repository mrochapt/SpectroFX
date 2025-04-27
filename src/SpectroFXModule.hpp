#pragma once
#include "rack.hpp"
using namespace rack;

struct SpectroFXModule : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        AUDIO_INPUT,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUTPUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    SpectroFXModule() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    }

    void process(const ProcessArgs& args) override {
        float in = inputs[AUDIO_INPUT].getVoltage();
        outputs[AUDIO_OUTPUT].setVoltage(in);
    }
};

extern Model* modelSpectroFXModule;