#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>

/*
 PhaseEngine
 
 Implementa os modos de fase usados pelo SpectroFX:
 
    RAW (0)     – Fase direta da análise (sem estimação).
    PV  (1)     – Phase‑Vocoder clássico: estima frequência instantânea
                  por bin e acumula fase de síntese continuamente.
    PV_LOCK (2) – Identity Phase Locking: após a etapa PV, fixa a fase
                  dos bins vizinhos à fase do pico espectral mais próximo.
 
 Interface público é "stateless" (por frame), mas o motor mantém histórico
 de fase por canal/bin para PV e PV_LOCK.
 
 Convenções:
    - K: número de bins (N/2 + 1).
    - H: hop size (amostras).
    - magnitudes e fases são arrays de tamanho K.
 */
class PhaseEngine {
public:
    enum class Mode : uint8_t { RAW = 0, PV = 1, PV_LOCK = 2 };

    // Inicializa estrutura interna (numCh canais, K bins, hop H). 
    void setup(int numCh, int bins, int hop);

    // Limpa histórico de fase (usar ao alterar N/H ou no reset do módulo). 
    void reset();
    
    /*
    Reconstrói o espectro de 1 frame (canal ch) segundo o modo pedido.
     - magProc[K] : magnitude processada (após efeitos).
     - phaseIn[K] : fase da análise (do frame atual).
     - outRe/outIm[K] : escrita do espectro complexo de síntese.
    */
    void processFrame(int ch, Mode mode, const float* magProc, const float* phaseIn, float* outRe, float* outIm);

private:
    int channels = 0;   // nº de canais (1 ou 2)
    int K        = 0;   // nº de bins (N/2 + 1)
    int H        = 0;   // hop size (samples)

    std::vector<std::vector<float>> prevAnalysisPhase; // [ch][K]
    std::vector<std::vector<float>> prevSynthPhase;    // [ch][K]

    /** Ângulo principal em (-π, π]. */
    static inline float princarg(float x) {
        x = std::fmod(x + (float)M_PI, 2.f * (float)M_PI);  // x em (-π, 3π]
        if (x < 0.f) x += 2.f * (float)M_PI;                // x em [0, 3π]
        return x - (float)M_PI;                             // x em (-π, π]
    }
};
