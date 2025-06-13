#pragma once

#include "rack.hpp"
#include <fftw3.h>

using namespace rack;

struct SpectroFXModule : Module {
    enum ParamIds {
        BLUR_PARAM,
        SHARPEN_PARAM,
        EFFECT0_PARAM,
        EFFECT1_PARAM,
        EFFECT2_PARAM,
        EFFECT3_PARAM,
        EFFECT4_PARAM,
        EFFECT5_PARAM,
        NUM_PARAMS
    };

    enum InputIds {
        AUDIO_INPUT,
        NUM_INPUTS
    };

    enum OutputIds {
        BYPASS_OUTPUT,
        PROCESSED_OUTPUT,
        NUM_OUTPUTS
    };

    enum LightIds {
        NUM_LIGHTS
    };

    static const int N = 512;

    double input[N] = {0.0};
    fftw_complex output[N / 2 + 1] = {};
    double inputBuffer[N] = {0.0};
    double processedBuffer[N] = {0.0};
    int bufferIndex = 0;
    int readPtr = 0;

    fftw_plan fftPlan = nullptr;
    fftw_plan ifftPlan = nullptr;

    SpectroFXModule() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(BLUR_PARAM,    0.f, 1.f, 0.f, "Blur");
        configParam(SHARPEN_PARAM, 0.f, 1.f, 0.f, "Sharpen");
        for (int i = EFFECT0_PARAM; i <= EFFECT5_PARAM; ++i)
            configParam(i, 0.f, 1.f, i == EFFECT0_PARAM ? 1.f : 0.f, std::string("Effect ") + std::to_string(i - EFFECT0_PARAM));
        fftPlan  = fftw_plan_dft_r2c_1d(N, input, output, FFTW_ESTIMATE);
        ifftPlan = fftw_plan_dft_c2r_1d(N, output, input, FFTW_ESTIMATE);
    }

    ~SpectroFXModule() {
        fftw_destroy_plan(fftPlan);
        fftw_destroy_plan(ifftPlan);
    }

    void process(const ProcessArgs& args) override;
};
