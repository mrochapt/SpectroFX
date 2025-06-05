#pragma once

#include "rack.hpp"          // Inclusão do header principal do VCV Rack
#include <fftw3.h>           // Biblioteca FFTW para FFT/IFFT
#include <opencv2/opencv.hpp> // OpenCV para manipulação de imagem
#include <random>            // Geração de números aleatórios (futuro: efeitos com fase aleatória)

using namespace rack;

// Constantes principais do módulo
static constexpr int N = 512;                          // Tamanho do bloco FFT
static constexpr int SPECTRO_WIDTH = N / 2 + 1;        // Largura do espectrograma (bins FFT válidos)
static constexpr int SPECTRO_HEIGHT = 100;             // Altura do espectrograma (nº de linhas históricas)

// Classe principal do módulo
struct SpectroFXModule : Module {
    // Enumeração dos parâmetros (knobs, switches, etc)
    enum ParamIds {
        EFFECT_TYPE_PARAM,   // Selector de efeito
        NUM_PARAMS
    };
    // Enumeração das entradas do módulo
    enum InputIds {
        AUDIO_INPUT_1,      // Entrada de áudio principal
        AUDIO_INPUT_2,      // (Opcional, não usado atualmente)
        NUM_INPUTS
    };
    // Enumeração das saídas do módulo
    enum OutputIds {
        AUDIO_OUTPUT_1,     // Saída processada (FFT/IFFT)
        AUDIO_OUTPUT_2,     // Saída bypass (direta)
        NUM_OUTPUTS
    };
    enum LightIds {
        NUM_LIGHTS
    };

    // Buffers de áudio e espectro
    double inputBuffer[N];               // Buffer circular de entrada de áudio
    double outputBuffer[N];              // Buffer circular de saída (depois da IFFT)
    double magnitude[SPECTRO_WIDTH];     // Magnitude dos bins FFT (para espectrograma)
    int bufferIndex = 0;                 // Posição atual de escrita no buffer de entrada
    int outputIndex = 0;                 // Posição atual de leitura no buffer de saída

    // Buffers e planos FFTW (bins válidos apenas SPECTRO_WIDTH)
    fftw_complex fftOut[SPECTRO_WIDTH];  // Saída da FFT
    fftw_complex fftIn[SPECTRO_WIDTH];   // Entrada da IFFT
    fftw_plan fftPlan;                   // Plano FFT real->complexo
    fftw_plan ifftPlan;                  // Plano IFFT complex->real

    // Janela de Hanning para suavizar artefactos de FFT
    double hanning[N];
    
    // Espectrograma (imagem OpenCV)
    cv::Mat spectrogramImage = cv::Mat::zeros(SPECTRO_HEIGHT, SPECTRO_WIDTH, CV_8UC1); // Imagem de espectrograma (acumulada)
    cv::Mat processedImage;             // Imagem após efeitos
    std::mt19937 rng{std::random_device{}()}; // Gerador aleatório (para efeitos futuros)

    SpectroFXModule();                  // Construtor
    ~SpectroFXModule();                 // Destrutor

    void process(const ProcessArgs& args) override; // Função principal de processamento de áudio
};
