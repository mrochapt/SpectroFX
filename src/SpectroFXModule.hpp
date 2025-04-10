#pragma once

#include "rack.hpp"
#include <fftw3.h>  

using namespace rack;

extern Plugin* pluginInstance;

struct SpectroFXModule : Module {
    enum ParamIds {
        BLUR_AMOUNT_PARAM,
        NUM_PARAMS
    };
    enum InputIds {
        AUDIO_IN,
        NUM_INPUTS
    };
    enum OutputIds {
        AUDIO_OUT,
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // Configurações STFT
    static const int N = 1024;
    static const int overlapFactor = 4;
    static const int hopSize = N / overlapFactor;

    // Construtor e destrutor
    SpectroFXModule();
    ~SpectroFXModule() override;

    void process(const ProcessArgs& args) override;

    // Buffers
    float window[N];
    float inBuffer[N];   // Bloco de entrada (time-domain)
    float outBuffer[N];  // Resultado da IFFT (time-domain)

    std::vector<float> inputFIFO;
    int writeIndex = 0;
    int readIndex = 0;
    int samplesInFIFO = 0;

    std::vector<float> overlapAddBuffer; // Buffer para overlap-add

    // Espectro
    std::vector<float> fftReal; // Vamos armazenar aqui após a FFT
    std::vector<float> fftImag;

    // FFTW
    fftwf_plan pForward = nullptr;
    fftwf_plan pInverse = nullptr;
    float* fftwIn = nullptr;                // Buffer de entrada para FFT
    fftwf_complex* fftwOut = nullptr;       // Buffer de saída da FFT

    // Parâmetros
    float blurAmount = 1.f;

    // Métodos auxiliares
    void initFFT();
    void doSTFT();
    void doISTFT();
    void blurSpectrum(std::vector<float>& magnitude, int bins, float amount);
};
