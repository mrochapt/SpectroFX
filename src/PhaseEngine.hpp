#pragma once
#include <cstdint>
#include <vector>
#include <cmath>
#include <algorithm>

/**
 * PhaseEngine
 * ----------------------------------------------------------------------------
 * Implementa os 3 modos de fase suportados pelo SpectroFX após a remoção do
 * Griffin–Lim e RTISI-LA:
 *
 *   RAW (0)     – Reutiliza a fase da análise.
 *   PV  (1)     – Phase Vocoder “clássico”: estima frequência instantânea
 *                 por bin e propaga fase de síntese de forma contínua.
 *   PV_LOCK (2) – Identity Phase Locking (McAulay–Quatieri): após calcular
 *                 as fases PV, bloqueia a fase dos bins vizinhos à fase do
 *                 pico espectral mais próximo (preserva harmónicos).
 *
 * Esta classe é *stateless* no interface público, mas guarda internamente o
 * histórico de fase por canal/bin, necessário para PV e PV_LOCK.
 *
 * Convenções:
 *   - K: número de bins (N/2 + 1).
 *   - H: hop size (amostras).
 *   - Todas as magnitudes e fases são arrays de tamanho K.
 */
class PhaseEngine {
public:
    enum class Mode : uint8_t { RAW = 0, PV = 1, PV_LOCK = 2 };

    /** Inicializa estrutura interna (numCh canais, K bins, hop H) */
    void setup(int numCh, int bins, int hop);

    /** Limpa histórico de fase (útil ao alterar N/H ou em reset do módulo) */
    void reset();
    

    /**
     * Reconstrói o espectro de 1 frame (canal ch) segundo o modo pedido.
     * - magProc[K] : magnitude processada (após efeitos)
     * - phaseIn[K] : fase da análise (do frame atual)
     * - outRe/outIm[K] : escrita do espectro complexo de síntese
     */
    void processFrame(int ch, Mode mode, const float* magProc, const float* phaseIn, float* outRe, float* outIm);

private:
    int channels = 0;
    int K        = 0;
    int H        = 0; // hop size (samples)

    std::vector<std::vector<float>> prevAnalysisPhase; // [ch][K]
    std::vector<std::vector<float>> prevSynthPhase;    // [ch][K]

    /** devolve ângulo principal em (-π, π] */
    static inline float princarg(float x) {
        x = std::fmod(x + (float)M_PI, 2.f * (float)M_PI);
        if (x < 0.f) x += 2.f * (float)M_PI;
        return x - (float)M_PI;
    }
};
