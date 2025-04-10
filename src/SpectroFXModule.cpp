#include "SpectroFXModule.hpp"
#include <cmath>
#include <algorithm>

// Construtor
SpectroFXModule::SpectroFXModule() {
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(BLUR_AMOUNT_PARAM, 0.f, 10.f, 1.f, "Blur Amount");

    // Aloca buffers de uso geral
    inputFIFO.resize(N * 4, 0.f); // buffer circular maior que N
    overlapAddBuffer.resize(N, 0.f);

    // janelas (Hann)
    for (int i = 0; i < N; i++) {
        window[i] = 0.5f * (1.f - std::cos((2.f * M_PI * i) / (N - 1)));
    }

    // Vetores que vão guardar a parte real/imag após a FFT
    fftReal.resize(N, 0.f);
    fftImag.resize(N, 0.f);

    // Inicializa FFTW
    initFFT();
}

// Destrutor: destruir planos e liberar memória FFTW
SpectroFXModule::~SpectroFXModule() {
    if (pForward) {
        fftwf_destroy_plan(pForward);
        pForward = nullptr;
    }
    if (pInverse) {
        fftwf_destroy_plan(pInverse);
        pInverse = nullptr;
    }
    if (fftwIn) {
        fftwf_free(fftwIn);
        fftwIn = nullptr;
    }
    if (fftwOut) {
        fftwf_free(fftwOut);
        fftwOut = nullptr;
    }
}

void SpectroFXModule::initFFT() {
    // Aloca memória para FFTW
    // fftwIn é um array real de tamanho N
    // fftwOut é um array complexo de tamanho (N/2 + 1)
    fftwIn = (float*) fftwf_malloc(sizeof(float) * N);
    fftwOut = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));

    // Cria planos (forward: real->complex, inverse: complex->real)
    // Usamos FFTW_ESTIMATE para simplificar (melhor performance com FFTW_MEASURE, mas mais lento na init)
    pForward = fftwf_plan_dft_r2c_1d(N, fftwIn, fftwOut, FFTW_ESTIMATE);
    pInverse = fftwf_plan_dft_c2r_1d(N, fftwOut, fftwIn, FFTW_ESTIMATE);
}

void SpectroFXModule::process(const ProcessArgs& args) {
    // Lê parâmetro de blur
    blurAmount = params[BLUR_AMOUNT_PARAM].getValue();

    // Captura áudio de entrada
    float in = 0.f;
    if (inputs[AUDIO_IN].isConnected()) {
        in = inputs[AUDIO_IN].getVoltage();
    }

    // Escreve no FIFO circular
    inputFIFO[writeIndex] = in;
    writeIndex = (writeIndex + 1) % inputFIFO.size();
    samplesInFIFO++;

    // Se temos ao menos N amostras, podemos processar um bloco
    if (samplesInFIFO >= N) {
        // Copia N amostras para inBuffer, aplicando a janela
        for (int i = 0; i < N; i++) {
            int idx = (readIndex + i) % inputFIFO.size();
            inBuffer[i] = inputFIFO[idx] * window[i];
        }

        // Avança no FIFO em hopSize
        readIndex = (readIndex + hopSize) % inputFIFO.size();
        samplesInFIFO -= hopSize;

        // STFT
        doSTFT();

        // Extração de magnitude e fase
        int bins = N/2 + 1;
        std::vector<float> magnitude(bins);
        std::vector<float> phase(bins);

        for (int k = 0; k < bins; k++) {
            float r = fftReal[k];
            float im = fftImag[k];
            magnitude[k] = std::sqrt(r*r + im*im);
            phase[k]     = std::atan2(im, r);
        }

        // Aplica blur 1D na frequência
        blurSpectrum(magnitude, bins, blurAmount);

        // Reconstrói fftReal/fftImag
        for (int k = 0; k < bins; k++) {
            fftReal[k] = magnitude[k] * std::cos(phase[k]);
            fftImag[k] = magnitude[k] * std::sin(phase[k]);
        }
        // Lembre-se: para FFT real, a parte k>bins-1 é "espelhada" no array complexo,
        // mas com FFTW também existe um layout específico. Aqui estamos simplificando.

        // ISTFT
        doISTFT();

        // Overlap-add
        for (int i = 0; i < N; i++) {
            overlapAddBuffer[i] += outBuffer[i];
        }

        // Sai uma amostra (exemplo didático)
        float out = overlapAddBuffer[0];
        overlapAddBuffer.erase(overlapAddBuffer.begin());
        overlapAddBuffer.push_back(0.f);

        outputs[AUDIO_OUT].setVoltage(out);
    }
    else {
        // Se não temos bloco completo, sai 0
        outputs[AUDIO_OUT].setVoltage(0.f);
    }
}

void SpectroFXModule::doSTFT() {
    // Copia inBuffer -> fftwIn
    for (int i = 0; i < N; i++) {
        fftwIn[i] = inBuffer[i];
    }

    // Executa plano forward
    fftwf_execute(pForward);

    // Copia resultados de fftwOut para fftReal/fftImag
    // fftwOut[k][0] => parte real, fftwOut[k][1] => parte imag
    int bins = N/2 + 1;
    for (int k = 0; k < bins; k++) {
        fftReal[k] = fftwOut[k][0];
        fftImag[k] = fftwOut[k][1];
    }
    // Se quiser armazenar a parte "espelhada" (k>bins-1), terá que reconstruir
    // manualmente de acordo com as convenções da FFT real->complex.
}

void SpectroFXModule::doISTFT() {
    // Copia fftReal/fftImag de volta para fftwOut
    int bins = N/2 + 1;
    for (int k = 0; k < bins; k++) {
        fftwOut[k][0] = fftReal[k]; // real
        fftwOut[k][1] = fftImag[k]; // imag
    }
    // Se necessário, preencher o espelhamento (para k>bins-1), mas com
    // `fftw_plan_dft_c2r_1d()` real->complex, normalmente não é estritamente preciso
    // pois o FFTW se apoia no layout "halfcomplex" interno. Depende do caso.

    // Executa plano inverse
    fftwf_execute(pInverse);

    // fftwIn agora contém o sinal no domínio do tempo, mas sem normalização
    // Por padrão, a FFT não divide por N
    for (int i = 0; i < N; i++) {
        // outBuffer = fftwIn / N
        outBuffer[i] = fftwIn[i] / float(N);
    }
}

void SpectroFXModule::blurSpectrum(std::vector<float>& magnitude, int bins, float amount) {
    int radius = std::min((int)amount, bins-1);
    std::vector<float> temp(magnitude.size());

    for (int k = 0; k < bins; k++) {
        float sum = 0.f;
        int count = 0;
        for (int n = -radius; n <= radius; n++) {
            int idx = k + n;
            if (idx >= 0 && idx < bins) {
                sum += magnitude[idx];
                count++;
            }
        }
        temp[k] = (count > 0) ? sum / (float)count : magnitude[k];
    }

    magnitude = temp;
}
