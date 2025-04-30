#pragma once

#include "rack.hpp" // VCV Rack SDK
#include <fftw3.h>  // Biblioteca externa para cálculo da FFT (Fast Fourier Transform)

using namespace rack;

// Classe principal que representa o módulo SpectroFX no VCV Rack
struct SpectroFXModule : Module {
    // Identificadores para organização de parâmetros
    enum ParamIds {
        NUM_PARAMS  
    };

    // Identificadores das portas de entrada
    enum InputIds {
        AUDIO_INPUT_1,
        AUDIO_INPUT_2,
        NUM_INPUTS
    };

    // Identificadores das portas de saída
    enum OutputIds {
        AUDIO_OUTPUT_1,
        AUDIO_OUTPUT_2,
        NUM_OUTPUTS
    };

    // Identificadores das luzes (não usadas neste módulo)
    enum LightIds {
        NUM_LIGHTS
    };

    // Número de amostras da FFT (2^9 = 512)
    static const int N = 512;

    // Buffer usado para preparar dados para FFT
    double input[N];

    // Resultado da FFT: vetor de valores complexos (real + imag)
    fftw_complex output[N / 2 + 1];

    // Buffer circular para armazenar amostras de entrada em tempo real
    double inputBuffer[N];

    // Índice de escrita no buffer circular
    int bufferIndex = 0;

    // Plano optimizado FFTW (pré-computado)
    fftw_plan fftPlan = nullptr;

    // Construtor do módulo
    SpectroFXModule() {
        config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);            // Configura portas
        fftPlan = fftw_plan_dft_r2c_1d(N, input, output, FFTW_ESTIMATE);    // Cria plano FFT
    }

    // Destrutor — liberta recursos do plano FFT
    ~SpectroFXModule() {
        fftw_destroy_plan(fftPlan);
    }

    // Função principal chamada a cada ciclo de processamento
    void process(const ProcessArgs& args) override {
        // Copia entradas de áudio e transmite às saídas (bypass simples)
        for (int i = 0; i < 2; i++) {
            float in = inputs[AUDIO_INPUT_1 + i].getVoltage();
            outputs[AUDIO_OUTPUT_1 + i].setVoltage(in);

            // Armazena sinal no buffer circular
            inputBuffer[bufferIndex] = in;
            bufferIndex = (bufferIndex + 1) % N;
        }
    }
};
