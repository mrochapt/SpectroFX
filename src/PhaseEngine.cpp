#include "PhaseEngine.hpp"

void PhaseEngine::setup(int numCh, int bins, int hop) {
    channels = numCh;
    K        = bins;
    H        = hop;
    prevAnalysisPhase.assign(channels, std::vector<float>(K, 0.f));
    prevSynthPhase  .assign(channels, std::vector<float>(K, 0.f));
}

void PhaseEngine::reset() {
    for (int ch = 0; ch < channels; ++ch) {
        std::fill(prevAnalysisPhase[ch].begin(), prevAnalysisPhase[ch].end(), 0.f);
        std::fill(prevSynthPhase  [ch].begin(), prevSynthPhase  [ch].end(), 0.f);
    }
}

void PhaseEngine::processFrame(int ch, Mode mode,
                               const float* magProc,
                               const float* phaseIn,
                               float* outRe,
                               float* outIm) {
    if (mode == Mode::RAW) {
        for (int k = 0; k < K; ++k) {
            float phi = phaseIn[k];
            outRe[k]  = magProc[k] * std::cos(phi);
            outIm[k]  = magProc[k] * std::sin(phi);
        }
        std::copy(phaseIn, phaseIn + K, prevAnalysisPhase[ch].begin());
        std::copy(prevAnalysisPhase[ch].begin(), prevAnalysisPhase[ch].end(), prevSynthPhase[ch].begin());
        return;
    }

    // ---------- Phase‑Vocoder simples ----------
    if (mode == Mode::PV) {
        for (int k = 0; k < K; ++k) {
            float phi_a      = phaseIn[k];
            float phi_prev_a = prevAnalysisPhase[ch][k];

            // avanço esperado
            float dphi_exp = 2.f * (float)M_PI * k * (float)H / (float)(2*(K-1));

            // desvio observado
            float dphi     = princarg((phi_a - phi_prev_a) - dphi_exp);

            // frequência instantânea
            float omega    = (dphi_exp + dphi) / (float)H;

            // acumula fase de síntese
            float phi_s    = prevSynthPhase[ch][k] + omega * (float)H;

            // guarda para próxima iteração
            prevAnalysisPhase[ch][k] = phi_a;
            prevSynthPhase  [ch][k] = phi_s;

            // output
            outRe[k] = magProc[k] * std::cos(phi_s);
            outIm[k] = magProc[k] * std::sin(phi_s);
        }
    }

    // ---------- PV‑Lock (Identity Phase Locking) ----------
    if (mode == Mode::PV_LOCK) {
        /* (1) Phase‑Vocoder base → phi_s[k] */
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

        /* (2) detectar picos */
        std::vector<char> isPeak(K, 0);
        float maxMag = 0.f;
        for (int k = 0; k < K; ++k) if (magProc[k] > maxMag) maxMag = magProc[k];
        const float thresh = 0.001f * maxMag;
        for (int k = 1; k < K-1; ++k)
            if (magProc[k] > thresh &&
                magProc[k] > magProc[k-1] &&
                magProc[k] >= magProc[k+1])
                isPeak[k] = 1;

        /* (3) fase bloqueada */
        for (int k = 0; k < K; ++k) {
            if (isPeak[k])
                phi_lock[k] = phi_s[k];
            else if (k == 0)
                phi_lock[k] = phi_s[k]; // DC bin
            else
                phi_lock[k] = phi_lock[k-1] +
                            princarg(phi_s[k] - phi_s[k-1]);
        }

        /* (4) escreve espectro */
        for (int k = 0; k < K; ++k) {
            outRe[k] = magProc[k] * std::cos(phi_lock[k]);
            outIm[k] = magProc[k] * std::sin(phi_lock[k]);
        }
        return;
    }

}
