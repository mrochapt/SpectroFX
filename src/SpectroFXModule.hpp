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

/**
 * SpectroFXModule (GL-free)
 * -------------------------------------------------------------------------
 * - Entradas: L/R áudio
 * - Saídas  : L/R áudio
 * - Efeitos sobre a magnitude do espectro (1D, por bin): BLUR, SHARPEN,
 *   EDGE, EMBOSS, MIRROR (mix com espectro invertido). Cada efeito tem
 *   knob para L e R (0..1) e entrada CV opcional (+/-10V mapeado para 0..1).
 * - Modos de fase: RAW, PV, PV-Lock (enum PhaseEngine::Mode).
 * - STFT: janela sqrt-Hann, N=1024, H=N/2 (COLA garantido).
 *
 * Nota: Este cabeçalho descreve a API e a memória do módulo. A implementação
 * encontra-se em SpectroFXModule.cpp. O widget (UI) está em SpectroFXWidget.hpp.
 */
struct SpectroFXModule : Module {
    // ---------- Parâmetros ----------
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

    // ---------- Entradas ----------
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

    // ---------- Saídas ----------
    enum OutputIds {
        BYPASS_OUTPUT_L, BYPASS_OUTPUT_R,  // +++
        PROCESSED_OUTPUT_L, PROCESSED_OUTPUT_R, // +++
        NUM_OUTPUTS    };

    // ---------- Luzes (opcional) ----------
    enum LightIds {
        NUM_LIGHTS
    };

    // ---------- Constantes STFT ----------
    static constexpr int N = 1024;              // FFT size
    static constexpr int H = N / 2;             // hop (50% overlap, COLA com sqrt-Hann)
    fftw_plan fftPlan[2]  = {nullptr, nullptr};
    // para o espectrograma do Widget
    std::vector<std::vector<float>> processedMagnitude = std::vector<std::vector<float>>(2, std::vector<float>(N/2+1, 0.f));

    // <<< NOVO: máscara 2D modular >>>
    static constexpr int HIST = 256;   // usa o mesmo valor do espectrograma
    Mask2D mask2d;


    SpectroFXModule();
    ~SpectroFXModule() override;

    void process(const ProcessArgs& args) override;
    
    void processChannel(int ch);
    void analyzeFFT(int ch);
    void applySpectralFX(int ch);      // (usada/placeholder no .cpp)
    void synthesizeWithPhase(int ch);
    json_t* dataToJson() override;
    void dataFromJson(json_t* rootJ) override;


private:
    // FFTW buffers/plans
    double input[2][N] = {{0}};              // usado como time-domain in/out no .cpp
    fftw_complex output[2][N/2 + 1] = {};    // espectro
    fftw_plan ifftPlan[2] = {nullptr, nullptr};

    // buffers circulares + posições
    double inputBuffer[2][N*2] = {{0}};
    double outputBuffer[2][N*2] = {{0}};
    int inputWritePos[2] = {0,0};
    int outputWritePos[2] = {0,0};
    int outputReadPos[2]  = {0,0};
    int samplesSinceLastBlock[2] = {0,0};

    // janela √Hann
    double hann[N];

    // fase / magnitude
    std::vector<std::vector<float>> magIn, phaseIn, magProc, specRe, specIm;

    // DC-block
    double dc_x1[2] = {0,0}, dc_y1[2] = {0,0};

    // motor de fase
    PhaseEngine phaseEngine;
};
