#include "plugin.hpp"
#include "SpectroFXModule.hpp"
#include "SpectroFXWidget.hpp"
#include <opencv2/opencv.hpp>

void SpectroFXModule::process(const ProcessArgs& args) {
    // Saída bypass direta
    float in = inputs[AUDIO_INPUT].isConnected() ? inputs[AUDIO_INPUT].getVoltage() : 0.f;
    outputs[BYPASS_OUTPUT].setVoltage(in);

    // Buffer circular
    inputBuffer[bufferIndex] = in;
    bufferIndex = (bufferIndex + 1) % N;

    // Efeito ativo (apenas um pode estar ON)
    int effect = 0;
    for (int i = 0; i < 6; ++i) {
        if (params[EFFECT0_PARAM + i].getValue() > 0.5f)
            effect = i;
    }
    // Forçar só um ativo
    for (int i = 0; i < 6; ++i)
        params[EFFECT0_PARAM + i].setValue(i == effect ? 1.f : 0.f);

    // FFT e efeitos a cada janela
    static cv::Mat frozenMag, frozenPhase;
    if (bufferIndex == 0) {
        for (int i = 0; i < N; i++)
            input[i] = inputBuffer[i];
        fftw_execute(fftPlan);

        // Magnitude/fase
        cv::Mat mag(1, N/2+1, CV_32F), phase(1, N/2+1, CV_32F);
        for (int i = 0; i < N/2+1; i++) {
            float re = output[i][0];
            float im = output[i][1];
            mag.at<float>(0,i)   = std::sqrt(re*re + im*im);
            phase.at<float>(0,i) = std::atan2(im, re);
        }

        // Efeitos blur/sharpen
        float blurAmt   = params[BLUR_PARAM].getValue();
        float sharpAmt  = params[SHARPEN_PARAM].getValue();
        if (blurAmt > 0.f)
            cv::GaussianBlur(mag, mag, cv::Size(0,0), blurAmt*10.f);
        if (sharpAmt > 0.f) {
            cv::Mat kernel = (cv::Mat_<float>(3,3) <<
                0, -sharpAmt, 0,
                -sharpAmt, 1+4*sharpAmt, -sharpAmt,
                0, -sharpAmt, 0);
            cv::filter2D(mag, mag, -1, kernel);
        }

        // Seleção do efeito extra via botões
        switch (effect) {
            case 1: { // Pixelate
                cv::Mat tmp;
                cv::resize(mag, tmp, cv::Size(8, 1), 0, 0, cv::INTER_NEAREST);
                cv::resize(tmp, mag, cv::Size(mag.cols, mag.rows), 0, 0, cv::INTER_NEAREST);
                break;
            }
            case 2: { // Gate
                double maxv;
                cv::minMaxLoc(mag, nullptr, &maxv);
                for (int i = 0; i < mag.cols; i++)
                    if (mag.at<float>(0,i) < 0.5 * maxv)
                        mag.at<float>(0,i) = 0.0f;
                break;
            }
            case 3: { // Invert
                double maxv;
                cv::minMaxLoc(mag, nullptr, &maxv);
                for (int i = 0; i < mag.cols; i++)
                    mag.at<float>(0,i) = maxv - mag.at<float>(0,i);
                break;
            }
            case 4: { // Mirror
                for (int i = 0; i < mag.cols/2; i++) {
                    float tmp = mag.at<float>(0,i);
                    mag.at<float>(0,i) = mag.at<float>(0,mag.cols-1-i);
                    mag.at<float>(0,mag.cols-1-i) = tmp;
                }
                break;
            }
            case 5: { // Freeze
                if (frozenMag.empty()) {
                    frozenMag = mag.clone();
                    frozenPhase = phase.clone();
                }
                mag = frozenMag.clone();
                // phase = frozenPhase.clone(); // opcional: congelar fase
                break;
            }
            default:
                frozenMag.release();
                frozenPhase.release();
                break;
        }

        // Reconstrução e IFFT
        for (int i = 0; i < N/2+1; i++) {
            float m = mag.at<float>(0,i);
            float ph = phase.at<float>(0,i);
            output[i][0] = m * std::cos(ph);
            output[i][1] = m * std::sin(ph);
        }
        fftw_execute(ifftPlan);
        for (int i = 0; i < N; ++i)
            processedBuffer[i] = input[i] / N;
        readPtr = 0;
    }

    float outProc = processedBuffer[readPtr];
    outputs[PROCESSED_OUTPUT].setVoltage(outProc);
    readPtr = (readPtr + 1) % N;
}

// Regista o módulo
Model* modelSpectroFXModule = createModel<SpectroFXModule, SpectroFXModuleWidget>("SpectroFX");
