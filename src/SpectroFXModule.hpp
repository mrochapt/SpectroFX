#pragma once

#include "rack.hpp"
#include <vector>
#include <fftw3.h>  // FFTW single precision

using namespace rack;

extern Plugin* pluginInstance;

struct SpectroFXModule : Module {
    enum ParamIds {
        BLUR_AMOUNT_PARAM, // Nosso knob para blur
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

    // Config STFT
    static const int N = 1024;
    static const int overlapFactor = 4;
    static const int hopSize = N / overlapFactor;

    // Buffers de tempo
    float window[N];
    float inBuffer[N];   // Bloco de entrada
    float outBuffer[N];  // Bloco de saída (IFFT)

    // FIFO para armazenar amostras de entrada
    std::vector<float> inputFIFO;
    int writeIndex = 0;
    int readIndex = 0;
    int samplesInFIFO = 0;

    // Overlap-add buffer
    std::vector<float> overlapAddBuffer;

    // Arrays para espectro (real e imag)
    std::vector<float> fftReal;
    std::vector<float> fftImag;

    // FFTW
    fftwf_plan pForward = nullptr;
    fftwf_plan pInverse = nullptr;
    float* fftwIn = nullptr;                // buffer time-domain p/ forward
    fftwf_complex* fftwOut = nullptr;       // buffer freq-domain p/ forward

    // Parâmetro do blur
    float blurAmount = 1.f;

    // Construtor / Destrutor
    SpectroFXModule();
    ~SpectroFXModule() override;

    // Chamado a cada ciclo de áudio (em blocos ou amostras, dependendo do Rack)
    void process(const ProcessArgs& args) override;

    // Métodos auxiliares
    void initFFT();
    void doSTFT();
    void doISTFT();
    void blurSpectrum(std::vector<float>& magnitude, int bins, float amount);
};
