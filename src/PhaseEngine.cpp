#include "PhaseEngine.hpp"

// Inicializa estrutura interna (numCh canais, K bins, hop H).
void PhaseEngine::setup(int numCh, int bins, int hop) {
    channels = numCh;   // nº de canais (1 ou 2)
    K        = bins;    // nº de bins (N/2 + 1)
    H        = hop  ;   // hop size (samples)
    prevAnalysisPhase.assign(channels, std::vector<float>(K, 0.f)); // histórico de fase da análise
    prevSynthPhase  .assign(channels, std::vector<float>(K, 0.f));  // histórico de fase da síntese
}

// Limpa histórico de fase (usar ao alterar N/H ou no reset do módulo).
void PhaseEngine::reset() {
    for (int ch = 0; ch < channels; ++ch) {
        std::fill(prevAnalysisPhase[ch].begin(), prevAnalysisPhase[ch].end(), 0.f);
        std::fill(prevSynthPhase  [ch].begin(), prevSynthPhase  [ch].end(), 0.f);
    }
}

// Reconstrói o espectro de 1 frame (canal ch) segundo o modo pedido.
void PhaseEngine::processFrame(int ch, Mode mode, const float* magProc, const float* phaseIn, float* outRe, float* outIm) {
    // Modo RAW: fase direta da análise (sem estimação)
    if (mode == Mode::RAW) {
        // Reconstrução direta: usa a fase de análise do próprio frame.
        for (int k = 0; k < K; ++k) {
            float phi = phaseIn[k];
            outRe[k]  = magProc[k] * std::cos(phi);
            outIm[k]  = magProc[k] * std::sin(phi);
        }
        // Atualiza histórico para continuidade quando alternar de modo.
        std::copy(phaseIn, phaseIn + K, prevAnalysisPhase[ch].begin()); // última fase de análise
        std::copy(prevAnalysisPhase[ch].begin(), prevAnalysisPhase[ch].end(), prevSynthPhase[ch].begin());  // última fase de síntese
        return;
    }

    // Phase‑Vocoder (frequência instantânea por bin)
    if (mode == Mode::PV) {
        for (int k = 0; k < K; ++k) {
            float phi_a      = phaseIn[k];  // fase da análise atual        
            float phi_prev_a = prevAnalysisPhase[ch][k]; // fase da análise anterior
            // Avanço de fase "esperado" entre frames para o bin k:
            // 2π * k * H / N, notando que N = 2*(K-1).
            float dphi_exp = 2.f * (float)M_PI * k * (float)H / (float)(2*(K-1));

            // Desvio observado (removido o esperado) e "wrapped" para (-π, π].
            float dphi     = princarg((phi_a - phi_prev_a) - dphi_exp);

            // Frequência instantânea (rad/amostra).
            float omega    = (dphi_exp + dphi) / (float)H;

            // Acumula fase de síntese para continuidade temporal.
            float phi_s    = prevSynthPhase[ch][k] + omega * (float)H;

            // Guarda históricos para a próxima iteração.
            prevAnalysisPhase[ch][k] = phi_a;
            prevSynthPhase  [ch][k] = phi_s;

            // Espectro de saída.
            outRe[k] = magProc[k] * std::cos(phi_s);
            outIm[k] = magProc[k] * std::sin(phi_s);
        }
    }

    // PV‑Lock (Identity Phase Locking) - fase bloqueada a partir dos picos
    if (mode == Mode::PV_LOCK) {
        // 1. Fase PV base -> phi_s[k]
        static thread_local std::vector<float> phi_s;
        static thread_local std::vector<float> phi_lock;
        if ((int)phi_s.size() != K) {
            phi_s.assign(K, 0.f);
            phi_lock.assign(K, 0.f);
        }

        for (int k = 0; k < K; ++k) {
            float phi_a      = phaseIn[k];
            float phi_prev_a = prevAnalysisPhase[ch][k];
            float dphi_exp   = 2.f * (float)M_PI * k * (float)H / (float)(2*(K-1));
            float dphi       = princarg((phi_a - phi_prev_a) - dphi_exp);
            float omega      = (dphi_exp + dphi) / (float)H;
            float phi        = prevSynthPhase[ch][k] + omega * (float)H;

            prevAnalysisPhase[ch][k] = phi_a;
            prevSynthPhase  [ch][k]  = phi;
            phi_s[k] = phi;
        }

        // 2. Detetar picos (threshold relativo simples).
        std::vector<char> isPeak(K, 0);
        float maxMag = 0.f;
        for (int k = 0; k < K; ++k) if (magProc[k] > maxMag) maxMag = magProc[k];
        const float thresh = 0.001f * maxMag;
        for (int k = 1; k < K-1; ++k)
            if (magProc[k] > thresh &&
                magProc[k] > magProc[k-1] &&
                magProc[k] >= magProc[k+1])
                isPeak[k] = 1;

        // 3. Propagar fase bloqueada a partir do pico mais próximo.
        for (int k = 0; k < K; ++k) {
            if (isPeak[k])
                phi_lock[k] = phi_s[k];
            else if (k == 0)
                phi_lock[k] = phi_s[k]; // DC bin
            else
                // Integra diferença principal para evitar saltos.
                phi_lock[k] = phi_lock[k-1] + princarg(phi_s[k] - phi_s[k-1]);
        }

        // 4. Escreve espectro bloqueado.
        for (int k = 0; k < K; ++k) {
            outRe[k] = magProc[k] * std::cos(phi_lock[k]);
            outIm[k] = magProc[k] * std::sin(phi_lock[k]);
        }
        return;
    }
}
