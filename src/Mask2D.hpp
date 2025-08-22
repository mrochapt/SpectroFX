#pragma once
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>

struct Mask2D {
    // Constantes fixas
    int HIST = 256;   // nº de colunas (tempo)
    int K    = 513;   // nº de bins (freq)

    // Buffers (UI -> Back; DSP -> Front)
    std::vector<std::vector<float>> front; // [HIST][K]
    std::vector<std::vector<float>> back;  // [HIST][K]

    // Estado
    std::atomic<bool> dirty {false};
    std::atomic<int>  head  {0};          // coluna mais recente (ring)
    std::atomic<int>  lowBin  {0};
    std::atomic<int>  highBin {512};
    std::atomic<bool> enabled {true};

    void setup(int hist, int k) {
        HIST = hist; K = k;
        front.assign(HIST, std::vector<float>(K, 0.f));
        back  = front;
        head.store(HIST - 1);
        lowBin.store(0);
        highBin.store(K - 1);
        enabled.store(true);
        dirty.store(false);
    }

    // UI → marcar sujo após escrever em 'back'
    inline void markDirty() { dirty.store(true, std::memory_order_release); }

    // Áudio thread: copiar back→front (sem locks) apenas quando sujo
    inline void swapIfDirty() {
        if (dirty.exchange(false, std::memory_order_acq_rel)) {
            front = back; // HIST×K — custo baixo (≈ 130k floats com HIST=256, K≈513)
        }
    }

    // Avança "head" (chamar 1× por frame)
    inline int advanceHead() {
        int h = head.load(std::memory_order_relaxed);
        h = (h + 1) % HIST;
        head.store(h, std::memory_order_relaxed);
        return h;
    }

    // Peso da máscara para (coluna atual, bin k) já respeitando os limites
    inline float weightNow(int k) const {
        int h = head.load(std::memory_order_relaxed);
        int lo = lowBin.load(std::memory_order_relaxed);
        int hi = highBin.load(std::memory_order_relaxed);
        if (k < lo || k > hi) return 0.f;
        return std::clamp(front[h][k], 0.f, 1.f);
    }

    // Utilitários de UI
    inline void clearBack(float value) {
        for (auto& col : back) std::fill(col.begin(), col.end(), value);
        markDirty();
    }
    inline void setBounds(int lo, int hi) {
        lo = std::clamp(lo, 0, K-1); hi = std::clamp(hi, 0, K-1);
        if (lo > hi) std::swap(lo, hi);
        lowBin.store(lo); highBin.store(hi);
    }

    // Conversão coluna do ecrã → ring (assumindo direita=mais recente=head)
    inline int ringColFromDisplayCol(int displayCol) const {
        int h = head.load(std::memory_order_relaxed);
        return (h - (HIST - 1 - displayCol) + HIST) % HIST;
    }
};
