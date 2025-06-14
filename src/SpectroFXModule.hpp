#pragma once

#include "rack.hpp"
#include <fftw3.h>

using namespace rack;

/**
 * Classe principal do módulo SpectroFX.
 * Aplica vários efeitos espectrais (imagem) controlados por knobs graduais.
 */
struct SpectroFXModule : Module {
    // Enumeração dos parâmetros de controlo (knobs)
    enum ParamIds {
        BLUR_PARAM,
        SHARPEN_PARAM,
        EDGE_PARAM,
        EMBOSS_PARAM,
        MIRROR_PARAM,
        GATE_PARAM,     
        STRETCH_PARAM,  
        NUM_PARAMS
    };

    // Entradas de áudio
    enum InputIds {
        AUDIO_INPUT, // Única entrada de áudio
        NUM_INPUTS
    };

    // Saídas de áudio
    enum OutputIds {
        BYPASS_OUTPUT,    // Saída de bypass (áudio sem processar)
        PROCESSED_OUTPUT, // Saída processada
        NUM_OUTPUTS
    };

    // (Não usamos LEDs neste layout)
    enum LightIds {
        NUM_LIGHTS
    };

    // Constante: tamanho da janela FFT
    static const int N = 512;

    // Buffers para processamento FFT/IFFT
    double input[N] = {0.0};                // Buffer de amostras para FFTW (entrada)
    fftw_complex output[N / 2 + 1] = {};    // Buffer de saída FFTW (domínio da frequência)
    double inputBuffer[N] = {0.0};          // Buffer circular para janela de entrada
    double processedBuffer[N] = {0.0};      // Buffer circular para janela de saída
    int bufferIndex = 0;                    // Posição atual no buffer circular
    int readPtr = 0;                        // Ponteiro de leitura do output processado

    // Planos FFT/IFFT (fftw)
    fftw_plan fftPlan = nullptr;
    fftw_plan ifftPlan = nullptr;

    // Construtor: inicializa parâmetros e planos FFT
    SpectroFXModule() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
        configParam(BLUR_PARAM,     0.f, 1.f, 0.f, "Blur");
        configParam(SHARPEN_PARAM,  0.f, 1.f, 0.f, "Sharpen");
        configParam(EDGE_PARAM,     0.f, 1.f, 0.f, "Edge Enhance");
        configParam(EMBOSS_PARAM,   0.f, 1.f, 0.f, "Emboss");
        configParam(GATE_PARAM, 0.f, 1.f, 0.f, "Spectral Gate");
        configParam(MIRROR_PARAM,   0.f, 1.f, 0.f, "Mirror");
        configParam(STRETCH_PARAM, 0.f, 1.f, 0.5f, "Spectral Stretch");
        fftPlan  = fftw_plan_dft_r2c_1d(N, input, output, FFTW_ESTIMATE);
        ifftPlan = fftw_plan_dft_c2r_1d(N, output, input, FFTW_ESTIMATE);
    }

    // Destrutor: liberta planos FFTW
    ~SpectroFXModule() {
        fftw_destroy_plan(fftPlan);
        fftw_destroy_plan(ifftPlan);
    }

    // Função principal de processamento de áudio
    void process(const ProcessArgs& args) override;
};
