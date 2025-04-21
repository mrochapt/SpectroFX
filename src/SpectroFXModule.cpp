#include "SpectroFXModule.hpp"
#include <cmath>
#include <algorithm>

SpectroFXModule::SpectroFXModule() {
    // Configura parâmetros, entradas, saídas, etc.
    config(NUM_PARAMS, NUM_INPUTS, NUM_OUTPUTS, NUM_LIGHTS);
    configParam(BLUR_AMOUNT_PARAM, 0.f, 10.f, 1.f, "Blur Amount");

    // Aloca buffer FIFO de entrada
    inputFIFO.resize(N * 4, 0.f); // um tamanho maior que N
    overlapAddBuffer.resize(N, 0.f);

    // Janela (Hann)
    for (int i = 0; i < N; i++) {
        window[i] = 0.5f * (1.f - std::cos((2.f * M_PI * i) / (N - 1)));
    }

    // Vetores que vão receber o espectro
    fftReal.resize(N, 0.f);
    fftImag.resize(N, 0.f);

    // Inicializa FFTW
    initFFT();
}

SpectroFXModule::~SpectroFXModule() {
    // Destrói planos e libera memória FFTW
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
    // Aloca buffers para FFTW
    fftwIn = (float*) fftwf_malloc(sizeof(float) * N);
    fftwOut = (fftwf_complex*) fftwf_malloc(sizeof(fftwf_complex) * (N/2 + 1));

    // Cria planos (forward e inverse)
    pForward = fftwf_plan_dft_r2c_1d(N, fftwIn, fftwOut, FFTW_ESTIMATE);
    pInverse = fftwf_plan_dft_c2r_1d(N, fftwOut, fftwIn, FFTW_ESTIMATE);
}

void SpectroFXModule::process(const ProcessArgs& args) {
    // Lê o parâmetro do knob
    blurAmount = params[BLUR_AMOUNT_PARAM].getValue();

    // Lê a amostra de entrada (mono)
    float in = 0.f;
    if (inputs[AUDIO_IN].isConnected()) {
        in = inputs[AUDIO_IN].getVoltage();
    }

    // Coloca no FIFO
    inputFIFO[writeIndex] = in;
    writeIndex = (writeIndex + 1) % inputFIFO.size();
    samplesInFIFO++;

    // Se tivermos ao menos N amostras disponíveis, processamos
    if (samplesInFIFO >= N) {
        // Copiar N amostras para inBuffer, aplicando janela
        for (int i = 0; i < N; i++) {
            int idx = (readIndex + i) % inputFIFO.size();
            inBuffer[i] = inputFIFO[idx] * window[i];
        }
        // Avança readIndex
        readIndex = (readIndex + hopSize) % inputFIFO.size();
        samplesInFIFO -= hopSize;

        // STFT
        doSTFT();

        // Extrai magnitude e fase
        int bins = N/2 + 1;
        std::vector<float> magnitude(bins);
        std::vector<float> phase(bins);

        for (int k = 0; k < bins; k++) {
            float r = fftReal[k];
            float im = fftImag[k];
            magnitude[k] = std::sqrt(r*r + im*im);
            phase[k] = std::atan2(im, r);
        }

        // Aplica blur (1D em frequência)
        blurSpectrum(magnitude, bins, blurAmount);

        // Reconstrói fftReal/fftImag
        for (int k = 0; k < bins; k++) {
            fftReal[k] = magnitude[k] * std::cos(phase[k]);
            fftImag[k] = magnitude[k] * std::sin(phase[k]);
        }

        // ISTFT
        doISTFT();

        // Overlap-add
        for (int i = 0; i < N; i++) {
            overlapAddBuffer[i] += outBuffer[i];
        }

        // Nesse exemplo simples, mandamos a primeira amostra do overlapAddBuffer pro output
        float out = overlapAddBuffer[0];

        // Remove a primeira amostra e empurra
        overlapAddBuffer.erase(overlapAddBuffer.begin());
        overlapAddBuffer.push_back(0.f);

        outputs[AUDIO_OUT].setVoltage(out);
    }
    else {
        // Ainda não temos bloco suficiente, sai 0
        outputs[AUDIO_OUT].setVoltage(0.f);
    }
}

void SpectroFXModule::doSTFT() {
    // Copia inBuffer -> fftwIn
    for (int i = 0; i < N; i++) {
        fftwIn[i] = inBuffer[i];
    }

    // Executa FFT
    fftwf_execute(pForward);

    // Copia resultado em fftReal/fftImag
    int bins = N/2 + 1;
    for (int k = 0; k < bins; k++) {
        fftReal[k] = fftwOut[k][0]; // real
        fftImag[k] = fftwOut[k][1]; // imag
    }
}

void SpectroFXModule::doISTFT() {
    // Copia de fftReal/fftImag para fftwOut
    int bins = N/2 + 1;
    for (int k = 0; k < bins; k++) {
        fftwOut[k][0] = fftReal[k];
        fftwOut[k][1] = fftImag[k];
    }

    // Executa IFFT
    fftwf_execute(pInverse);

    // fftwIn agora contém o sinal tempo, mas sem normalizar
    for (int i = 0; i < N; i++) {
        outBuffer[i] = fftwIn[i] / float(N);
    }
}

void SpectroFXModule::blurSpectrum(std::vector<float>& magnitude, int bins, float amount) {
    // Blur 1D simples com média de vizinhos
    int radius = std::min((int)amount, bins - 1);
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
        temp[k] = (count > 0) ? sum / count : magnitude[k];
    }

    magnitude = temp;
}
