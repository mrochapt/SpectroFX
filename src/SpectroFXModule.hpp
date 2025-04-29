#pragma once
#include "rack.hpp"
#include <fftw3.h>

using namespace rack;

struct SpectroFXModule : Module {
    enum ParamIds {
        NUM_PARAMS
    };
    enum InputIds {
        AUDIO_INPUT_1,
        AUDIO_INPUT_2,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUTPUT_1,
        AUDIO_OUTPUT_2,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    static constexpr int N = 512;

    std::vector<double> inputBuffer;
    int bufferIndex = 0;

    double input[N] = {};
    fftw_complex output[N/2 + 1] = {};
    fftw_plan fftPlan = nullptr;

    SpectroFXModule() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        inputBuffer.resize(N, 0.0);
        fftPlan = fftw_plan_dft_r2c_1d(N, input, output, FFTW_ESTIMATE);
    }

    ~SpectroFXModule() {
        if (fftPlan) {
            fftw_destroy_plan(fftPlan);
        }
    }

    void process(const ProcessArgs& args) override {
        double in = inputs[AUDIO_INPUT_1].getVoltage();
        outputs[AUDIO_OUTPUT_1].setVoltage(in);
        outputs[AUDIO_OUTPUT_2].setVoltage(inputs[AUDIO_INPUT_2].getVoltage());
        inputBuffer[bufferIndex] = in;
        bufferIndex = (bufferIndex + 1) % N;
    }
};

extern Model* modelSpectroFXModule;
