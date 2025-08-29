#pragma once
#include <vector>
#include <atomic>
#include <algorithm>
#include <cmath>

/*
 Mask2D
 
 Máscara 2D leve para aplicar/pesar efeitos espectrais por bin e por coluna
 do espectrograma. É desenhada no thread de UI (em 'back') e lida no thread
 de áudio (a partir de 'front'), com troca lock‑free por flag 'dirty'.
 
 Convenções
    - HIST  : nº de colunas (histórico temporal do espectrograma).
    - K     : nº de bins (frequências, N/2+1).
    - 'head': índice (ring) da coluna mais recente.
 
 Segurança de threads
    - A UI escreve apenas em 'back' e depois chama 'markDirty()'.
    - O áudio chama 'swapIfDirty()' no início do frame para copiar 'back->front'.
    - 'std::atomic' protege apenas os índices/flags; os buffers usam cópia inteira (custo baixo para HIST≈256 e K≈513 ≈ 130k floats).
 */
struct Mask2D {
    // Dimensões (configuráveis via setup())
    int HIST = 256;   // nº de colunas (tempo)
    int K    = 513;   // nº de bins (freq)

    // Buffers (UI -> Back; DSP -> Front)
    std::vector<std::vector<float>> front; // [HIST][K]
    std::vector<std::vector<float>> back;  // [HIST][K]

    // Estado 
    std::atomic<bool> dirty {false};   // back foi modificado pela UI
    std::atomic<int>  head  {0};       // coluna mais recente (ring)
    std::atomic<int>  lowBin  {0};     // bin mínimo ativo    
    std::atomic<int>  highBin {512};   // bin máximo ativo
    std::atomic<bool> enabled {true};  // máscara ativa/inativa

    /** Inicializa dimensões e limpa buffers/estado. */
    void setup(int hist, int k) {
        HIST = hist; K = k;                             // dimensões
        front.assign(HIST, std::vector<float>(K, 0.f)); // limpa
        back  = front;                                  // limpa                          
        head.store(HIST - 1);                           // início (última coluna)
        lowBin.store(0);                                // todo o espectro
        highBin.store(K - 1);                           // todo o espectro
        enabled.store(true);                            // máscara ativa
        dirty.store(false);                             // limpa flag  
    }

    /* UI -> marcar sujo após escrever em 'back'. */
    inline void markDirty() { dirty.store(true, std::memory_order_release); }

    /*
    Áudio thread: copiar back->front (sem locks) apenas quando sujo.
    Usa 'exchange' para limpar a flag de forma atómica.
    */
    inline void swapIfDirty() {
        if (dirty.exchange(false, std::memory_order_acq_rel)) {
            // HIST×K — custo reduzido (≈130k floats com HIST=256, K≈513).
            front = back;
        }
    }

    // Avança "head" (chamar 1× por frame). Retorna o novo valor.
    inline int advanceHead() {
        int h = head.load(std::memory_order_relaxed);
        h = (h + 1) % HIST;
        head.store(h, std::memory_order_relaxed);
        return h;
    }

    /*
    Peso da máscara para (coluna atual, bin k) respeitando limites.
    Retorna 0 fora de [lowBin, highBin], clamp [0..1] no interior.
    */
    inline float weightNow(int k) const {
        int h = head.load(std::memory_order_relaxed);
        int lo = lowBin.load(std::memory_order_relaxed);
        int hi = highBin.load(std::memory_order_relaxed);
        if (k < lo || k > hi) return 0.f;
        return std::clamp(front[h][k], 0.f, 1.f);
    }

    // Utilitários de UI
    inline void clearBack(float value) {    // limpa 'back' para um valor
        for (auto& col : back) std::fill(col.begin(), col.end(), value);
        markDirty();
    }
    // Definir peso na coluna atual (head) para todos os bins
    inline void setBounds(int lo, int hi) {  // definir limites (UI)
        lo = std::clamp(lo, 0, K-1); hi = std::clamp(hi, 0, K-1);       
        if (lo > hi) std::swap(lo, hi);     
        lowBin.store(lo); highBin.store(hi);      
    }

    /*
    Conversão: coluna do ecrã -> coluna no ring (direita = mais recente).
    Útil para mapear interações da UI para a posição cronológica correta.
    */
    inline int ringColFromDisplayCol(int displayCol) const {    // 0=esquerda, HIST-1=direita
        int h = head.load(std::memory_order_relaxed);           // coluna mais recente
        return (h - (HIST - 1 - displayCol) + HIST) % HIST;     // ring
    }
};
