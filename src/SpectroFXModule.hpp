#pragma once
#include "rack.hpp"
#include <fftw3.h>
#include <vector>
#include <array>
#include <cmath>
#include <algorithm>
#include <cstring>
#include "PhaseEngine.hpp"
#include "Mask2D.hpp"

using namespace rack;

/*
SpectroFXModule (sem Griffin–Lim)

Entradas:  L/R áudio
Saídas  :  L/R áudio (bypass e processado)

Efeitos sobre a magnitude por bin (1D):
   BLUR, SHARPEN, EDGE, EMBOSS, MIRROR, GATE, STRETCH.
Cada efeito tem knob L/R [0..1] e CV opcional (±10 V -> ±1.0).

Modos de fase (PhaseEngine):
   RAW, PV, PV-Lock.

STFT: janela √Hann, N=1024, H=N/2 (COLA garantido). Reconstrução por
overlap‑add com IFFT escalada por 1/N. Latência ≈ N amostras.

A implementação está em SpectroFXModule.cpp. UI em SpectroFXWidget.hpp.
*/
struct SpectroFXModule : Module {
    // Parâmetros
    enum ParamIds {
        BLUR_PARAM_L, BLUR_PARAM_R,
        SHARPEN_PARAM_L, SHARPEN_PARAM_R,
        EDGE_PARAM_L, EDGE_PARAM_R,
        EMBOSS_PARAM_L, EMBOSS_PARAM_R,
        MIRROR_PARAM_L, MIRROR_PARAM_R,
        GATE_PARAM_L,   GATE_PARAM_R,
        STRETCH_PARAM_L,STRETCH_PARAM_R,
        PHASE_MODE_PARAM,
        NUM_PARAMS
    };

    // Entradas
    enum InputIds {
        AUDIO_INPUT_L, AUDIO_INPUT_R,
        BLUR_CV_L, BLUR_CV_R,
        SHARPEN_CV_L, SHARPEN_CV_R,
        EDGE_CV_L, EDGE_CV_R,
        EMBOSS_CV_L, EMBOSS_CV_R,
        MIRROR_CV_L, MIRROR_CV_R,
        GATE_CV_L,   GATE_CV_R,
        STRETCH_CV_L,STRETCH_CV_R,
        NUM_INPUTS
    };

    // Saídas
    enum OutputIds {
        BYPASS_OUTPUT_L, BYPASS_OUTPUT_R,
        PROCESSED_OUTPUT_L, PROCESSED_OUTPUT_R,
        NUM_OUTPUTS
    };

    // Luzes
    enum LightIds { NUM_LIGHTS };

    // Constantes STFT 
    static constexpr int N = 1024;              // Tamanho FFT
    static constexpr int H = N / 2;             // hop (50% overlap, COLA com sqrt-Hann)
    fftw_plan fftPlan[2]  = {nullptr, nullptr};

    // Magnitude pós‑efeitos (exposta ao espectrograma do Widget).
    std::vector<std::vector<float>> processedMagnitude = std::vector<std::vector<float>>(2, std::vector<float>(N/2+1, 0.f));

    // Máscara 2D (mesma largura do histórico do espectrograma).
    static constexpr int HIST = 256; // nº de colunas (tempo)
    Mask2D mask2d;  

    SpectroFXModule();              // construtor
    ~SpectroFXModule() override;    // destrutor

    void process(const ProcessArgs& args) override; // Chamada por áudio thread
    void processChannel(int ch);                    // processa canal L(0)/R(1) 
    void analyzeFFT(int ch);                        // FFT + extração mag/fase
    void synthesizeWithPhase(int ch);               // IFFT + overlap-add

private:
    // FFTW buffers/plans
    double input[2][N] = {{0}};                     // time-domain in/out (ver .cpp)
    fftw_complex output[2][N/2 + 1] = {};           // espectro complexo
    fftw_plan ifftPlan[2] = {nullptr, nullptr};     // IFFT

    // Buffers circulares + posições
    double inputBuffer[2][N*2] = {{0}};             
    double outputBuffer[2][N*2] = {{0}};            
    int inputWritePos[2] = {0,0};                   
    int outputWritePos[2] = {0,0};            
    int outputReadPos[2]  = {0,0};      
    int samplesSinceLastBlock[2] = {0,0};

    // Janela √Hann (análise+síntese).
    double hann[N];

    // Fase / magnitude
    std::vector<std::vector<float>> magIn, phaseIn, magProc, specRe, specIm;

    // DC‑block (1ª ordem)
    double dc_x1[2] = {0,0}, dc_y1[2] = {0,0};

    // Motor de fase
    PhaseEngine phaseEngine;
};
