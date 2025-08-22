#pragma once
#include "rack.hpp"
#include "SpectroFXModule.hpp"
#include <cmath>

using namespace rack;
using namespace rack::widget;

/*----------------------- Helpers mm → px ------------------------*/
static inline float mm2pxf(float mm) {
    return mm * RACK_GRID_WIDTH / 5.08f;   // RACK_GRID_WIDTH = 15.24 px
}

/*---------------- Painel desenhado em código -------------------*/
struct SpectroFXPanel : Widget {
    int fontHead = 0, fontSmall = 0;

    SpectroFXPanel(Vec size) {  // construtor com tamanho do painel
        box.size = size;
        if (auto f = APP->window->loadFont(asset::system("res/fonts/DejaVuSans-Bold.ttf")))
            fontHead = f->handle;
        if (auto f = APP->window->loadFont(asset::system("res/fonts/DejaVuSans.ttf")))
            fontSmall = f->handle;
    }

    void draw(const DrawArgs& args) override {  // desenha o painel
        NVGcontext* vg = args.vg;   
        float W = box.size.x, H = box.size.y;

        /* fundo gradiente */
        NVGpaint grad = nvgLinearGradient(vg, 0, 0, 0, H, nvgRGB(0x20,0x24,0x28), nvgRGB(0x18,0x1c,0x1f));
        nvgBeginPath(vg);
        nvgRect(vg, 0, 0, W, H);
        nvgFillPaint(vg, grad);
        nvgFill(vg);

        /* painéis L/R (barras) */
        const float colW   = 22.f;  // largura dos painéis L/R
        const float colL_x = 17.f;  // left edge painel L
        const float colR_x = 41.f;  // left edge painel R
        auto drawPanel = [&](float leftMm) {
            nvgBeginPath(vg);
            nvgRect(vg, mm2pxf(leftMm), mm2pxf(5), mm2pxf(colW), H - mm2pxf(10));
            nvgFillColor(vg, nvgRGBA(0x2b,0x30,0x36, 102));
            nvgFill(vg);
        };
        drawPanel(colL_x);
        drawPanel(colR_x);

        /* linha divisória para a área do espectrograma */
        nvgBeginPath(vg);
        float xLine = mm2pxf(40);
        nvgMoveTo(vg, xLine, mm2pxf(8));
        nvgLineTo(vg, xLine, H - mm2pxf(8));
        nvgStrokeColor(vg, nvgRGB(0xd0,0xd0,0xd0));
        nvgStrokeWidth(vg, 0.6f);
        nvgStroke(vg);

        /* logótipo */
        nvgFontSize(vg, 16.f);
        if (fontHead) nvgFontFaceId(vg, fontHead);
        nvgFillColor(vg, nvgRGB(0xc8,0xcf,0xd4));
        nvgText(vg, mm2pxf(100), mm2pxf(12), "SpectroFX", nullptr);

        /* cabeçalhos L/R centrados nos painéis */
        const float cxL = colL_x + colW * 0.5f;
        const float cxR = colR_x + colW * 0.5f;
        const float headY = 11.f;
        nvgFontSize(vg, 10.f);
        if (fontHead) nvgFontFaceId(vg, fontHead);
        nvgFillColor(vg, nvgRGB(0xe0,0xe6,0xea));
        nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_MIDDLE);
        nvgText(vg, mm2pxf(cxL), mm2pxf(headY), "L", nullptr);
        nvgText(vg, mm2pxf(cxR), mm2pxf(headY), "R", nullptr);

        /* etiquetas à esquerda dos inputs CV (ambas as colunas) */
        const float xL_CV = 22.f;
        const float y0    = 22.f;
        const float dy    = 15.4f;
        const float xLabelL = xL_CV - 6.f;
        const char* names[] = { "BLUR", "SHARPEN", "EDGE", "EMBOSS", "MIRROR", "GATE", "STRETCH" };

        nvgFontSize(vg, 7.5f);
        if (fontSmall) nvgFontFaceId(vg, fontSmall);
        nvgFillColor(vg, nvgRGB(0xc8,0xcf,0xd4));
        nvgTextAlign(vg, NVG_ALIGN_RIGHT | NVG_ALIGN_MIDDLE);
        for (int i = 0; i < 7; ++i) {
            float y = y0 + dy * i;
            nvgText(vg, mm2pxf(xLabelL), mm2pxf(y), names[i], nullptr);
        }

        /* ===== Painéis de grupo para I/O (L e R) + rótulos ===== */
        {
            // Mesmas coordenadas dos jacks usadas no construtor
            const float ioLx = 130.f, ioRx = 185.f, ioY = 115.f, iodX = 15.f;

            // Dimensões do painel de grupo
            const float padX = 7.f;     // margem horizontal extra
            const float boxH = 18.f;    // altura do painel

            auto drawIOPanel = [&](float baseX, const char* label) {
                float x0 = baseX - padX;
                float x1 = baseX + 2.f * iodX + padX;     // cobre IN + 2 OUTs
                float y0 = ioY - boxH * 0.5f;

                // ESTILO IGUAL AOS PAINÉIS DE KNOBS/CV: retângulo simples + mesma cor
                nvgBeginPath(vg);
                nvgRect(vg, mm2pxf(x0), mm2pxf(y0),
                            mm2pxf(x1 - x0), mm2pxf(boxH));
                nvgFillColor(vg, nvgRGBA(0x2b,0x30,0x36, 102)); // igual às colunas L/R
                nvgFill(vg);

                // legenda discreta (mantida)
                if (fontSmall) nvgFontFaceId(vg, fontSmall);
                nvgFontSize(vg, 7.0f);
                nvgFillColor(vg, nvgRGB(0xc8,0xcf,0xd4));
                nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_TOP);
                nvgText(vg, mm2pxf(x0 + 2.f), mm2pxf(y0 + 1.2f), label, nullptr);
            };

            // Desenha dois painéis: um para o canal L e outro para o R
            drawIOPanel(ioLx, "I/O L");
            drawIOPanel(ioRx, "I/O R");

            // — Rótulos por baixo dos jacks (mantidos) —
            const float yUnder = ioY + 6.f;  // ~6 mm abaixo do centro do jack
            nvgTextAlign(vg, NVG_ALIGN_CENTER | NVG_ALIGN_TOP);
            nvgFontSize(vg, 7.0f);
            if (fontSmall) nvgFontFaceId(vg, fontSmall);
            nvgFillColor(vg, nvgRGB(0xc8,0xcf,0xd4));

            // Inputs
            nvgText(vg, mm2pxf(ioLx),               mm2pxf(yUnder), "IN L",   nullptr);
            nvgText(vg, mm2pxf(ioRx),               mm2pxf(yUnder), "IN R",   nullptr);

            // Outputs (bypass, processed)
            nvgText(vg, mm2pxf(ioLx + iodX),        mm2pxf(yUnder), "BYP L",  nullptr);
            nvgText(vg, mm2pxf(ioRx + iodX),        mm2pxf(yUnder), "BYP R",  nullptr);
            nvgText(vg, mm2pxf(ioLx + 2.f*iodX),    mm2pxf(yUnder), "PROC L", nullptr);
            nvgText(vg, mm2pxf(ioRx + 2.f*iodX),    mm2pxf(yUnder), "PROC R", nullptr);
        }

        /* ==== Escalas dos knobs (sem números, zona ativa em cima) ==== */
        const float pi = 3.14159265f;

        // centros dos teus knobs (mesmos do construtor)
        const float kxL = 34.f, kxR = 47.f;
        const float ky0 = 22.f, kdy = 15.4f;

        // aproximar ao knob
        const float r_mm       = 5.0f;   // raio da escala (ajusta 4.8–5.5)
        const float tick_lenmm = 0.7f;   // comprimento das marcas

        // zona ativa = 300°, gap ~60° em baixo → varremos [300° .. 600°]
        const float aStart = 5.f * pi / 3.f;              // 300°
        const float aEnd   = aStart + 5.f * pi / 3.f;     // 600° (= 240° + 360°)
        const float aMid   = 0.5f * (aStart + aEnd);      // 450° (= 90° topo)

        // desenha uma escala (arco + 3 marcas)
        auto drawScale = [&](float cx_mm, float cy_mm) {
            float cx = mm2pxf(cx_mm), cy = mm2pxf(cy_mm);
            float r  = mm2pxf(r_mm),  tl = mm2pxf(tick_lenmm);

            // arco (polyline), com y para cima: y = cy - r*sin(a)
            nvgBeginPath(vg);
            const int SEG = 64;
            for (int i = 0; i <= SEG; ++i) {
                float t = (float)i / (float)SEG;
                float a = aStart + t * (aEnd - aStart);
                float x = cx + r * std::cos(a);
                float y = cy - r * std::sin(a);
                if (i == 0) nvgMoveTo(vg, x, y); else nvgLineTo(vg, x, y);
            }
            nvgStrokeColor(vg, nvgRGBA(255,255,255, 96));
            nvgStrokeWidth(vg, 1.0f);
            nvgStroke(vg);

            // marcas: início / meio / fim
            float angs[3] = { aStart, aMid, aEnd };
            for (int i = 0; i < 3; ++i) {
                float a = angs[i];
                float cs = std::cos(a), sn = std::sin(a);
                float x0 = cx + r  * cs, y0 = cy - r  * sn;
                float x1 = cx + (r+tl) * cs, y1 = cy - (r+tl) * sn;
                nvgBeginPath(vg);
                nvgMoveTo(vg, x0, y0);
                nvgLineTo(vg, x1, y1);
                nvgStrokeColor(vg, nvgRGBA(255,255,255, 160));
                nvgStrokeWidth(vg, 1.0f);
                nvgStroke(vg);
            }
        };

        // aplica às 7 linhas de knobs
        for (int row = 0; row < 7; ++row) {
            float cy = ky0 + kdy * row;
            drawScale(kxL, cy);
            drawScale(kxR, cy);
        }
    }
};

/*----------------- LED + Texto do modo -------------------------*/
struct ModeLED : LightWidget {
    SpectroFXModule* mod = nullptr;
    ModeLED() { box.size = Vec(mm2pxf(4), mm2pxf(4)); }
    void drawLight(const DrawArgs& args) override {
        if (!mod) return;
        int m = (int)mod->params[SpectroFXModule::PHASE_MODE_PARAM].getValue();
        NVGcolor c = (m==1) ? nvgRGB(60,190,255) : (m==2) ? nvgRGB(255,180,60) : nvgRGB(180,180,180);
        nvgBeginPath(args.vg);
        nvgCircle(args.vg, box.size.x/2, box.size.y/2, box.size.x/2);
        nvgFillColor(args.vg, c);
        nvgFill(args.vg);
    }
};

// Texto do modo de fase (widget separado para manter o texto pequeno)
struct ModeText : Widget {
    SpectroFXModule* mod = nullptr;
    void draw(const DrawArgs& args) override {
        if (!mod) return;
        int m = (int)mod->params[SpectroFXModule::PHASE_MODE_PARAM].getValue();
        const char* txt = (m==0) ? "RAW" : (m==1) ? "PV" : "PV-Lock";
        NVGcontext* vg = args.vg;
        nvgFontSize(vg, 8.f);
        nvgFillColor(vg, nvgRGB(0xc8,0xcf,0xd4));
        nvgTextAlign(vg, NVG_ALIGN_LEFT | NVG_ALIGN_MIDDLE);
        nvgText(vg, 0.f, mm2pxf(2.f), txt, nullptr);
    }
};

/*--------------------- Espectrograma ---------------------------*/
struct SpectrogramDisplay : Widget {
    SpectroFXModule* module;
    std::vector<std::vector<float>> hist;
    int pos = 0;

    SpectrogramDisplay(SpectroFXModule* m) : module(m), hist(SpectroFXModule::HIST, std::vector<float>(SpectroFXModule::N/2 + 1, 0.f)) {
        box.pos  = Vec(mm2pxf(67),  mm2pxf(17));
        box.size = Vec(mm2pxf(154), mm2pxf(81));
    }

    void draw(const DrawArgs& args) override {
        if (!module || !module->fftPlan[0]) return;
        const int N = SpectroFXModule::N;
        const int K = N/2 + 1;
        const int HISTORY_SIZE = SpectroFXModule::HIST;

        for (int i = 0; i < K; ++i)
            hist[pos][i] = module->processedMagnitude[0][i];

        pos = (pos + 1) % HISTORY_SIZE;     // avança posição circularmente

        if (module) { 
            int latest = (pos + HISTORY_SIZE - 1) % HISTORY_SIZE; // coluna mais recente (direita)
            module->mask2d.head.store(latest, std::memory_order_relaxed);   // atualiza posição "head" da máscara 2D
        }

        for (int t = 0; t < HISTORY_SIZE; ++t) {
            int idx = (pos + t) % HISTORY_SIZE;
            float x  = (float)t / HISTORY_SIZE * box.size.x;
            float bw = box.size.x / HISTORY_SIZE + 1.f;

            for (int f = 0; f < K - 1; ++f) {
                float mag  = hist[idx][f];
                float norm = clamp(std::log(mag + 1e-6f) * 0.18f, 0.f, 1.f);
                NVGcolor col = nvgHSLA(0.66f - norm * 0.66f, 1.0f, norm * 0.6f + 0.15f, 255);
                float y  = box.size.y - ((float)f / (K - 1) * box.size.y);
                float bh = box.size.y / (K - 1) + 1.f;

                nvgBeginPath(args.vg);
                nvgRect(args.vg, x, y, bw, bh);
                nvgFillColor(args.vg, col);
                nvgFill(args.vg);
            }
        }
    }
};

/*--------------------- Máscara 2D Overlay ----------------------*/
// (widget para desenhar a máscara 2D no espectrograma)
struct MaskOverlay : Widget {
    SpectroFXModule* module = nullptr; 
    bool dragging = false;      // true arrasta a seleção, false arrasta a posição do mouse
    bool painting = true;     // true pinta 1.0, Alt apaga 0.0
    Vec a, b;                 // rect seleção

    MaskOverlay(SpectroFXModule* m, Vec pos, Vec size) : module(m) {
        box.pos = pos; box.size = size; // posição e tamanho do widget
    }

    inline int binFromY(float y) const {                // converte coordenada Y do mouse para bin
        float t = clamp(y / box.size.y, 0.f, 1.f);      // t ∈ [0,1] → bin ∈ [0, K-1]
        int K = SpectroFXModule::N/2 + 1;               // número de bins (frequências)
        int k = (int) std::round((1.f - t) * (K - 1));  // converte para bin
        return std::clamp(k, 0, K-1);                   // clampa para [0, K-1]
    }

    // converte coluna do espectrograma para coluna do anel da máscara 2D
    void onButton(const event::Button& e) override {
        if (!module || e.button != GLFW_MOUSE_BUTTON_LEFT) return;
        if (e.action == GLFW_PRESS) {
            dragging = true; a = b = e.pos; e.consume(this);
        } else if (e.action == GLFW_RELEASE) {
            dragging = false;
            int k0 = binFromY(a.y), k1 = binFromY(b.y);
            if (k0 > k1) std::swap(k0, k1);
            module->mask2d.setBounds(k0, k1);   // <— só isto
            e.consume(this);
        }
    }

    //  arrasta a seleção ou a posição do mouse
    void onDragMove(const event::DragMove& e) override {
        if (!dragging) return;
        b = b.plus(e.mouseDelta);
        b.x = rack::clamp(b.x, 0.f, box.size.x);
        b.y = rack::clamp(b.y, 0.f, box.size.y);
        e.consume(this);
    }
    
    void draw(const DrawArgs& args) override {
        const bool enabled = module && module->mask2d.enabled.load();

        // 1) Banda verde (quando ON)
        if (enabled) {
            int K = SpectroFXModule::N/2 + 1;
            int lo = module->mask2d.lowBin.load();
            int hi = module->mask2d.highBin.load();

            float yTop    = (1.f - (float)(hi+1) / K) * box.size.y;
            float yBottom = (1.f - (float)lo      / K) * box.size.y;

            nvgBeginPath(args.vg);
            nvgRect(args.vg, 0.f, yTop, box.size.x, yBottom - yTop);
            nvgFillColor(args.vg, nvgRGBA(30, 200, 60, 64));
            nvgFill(args.vg);

            // 2) Linhas de bounds (apenas quando ON)
            auto yForBin = [&](int k){ return (1.f - (float)k / (SpectroFXModule::N/2)) * box.size.y; };
            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0.f, yForBin(lo));
            nvgLineTo(args.vg, box.size.x, yForBin(lo));
            nvgStrokeColor(args.vg, nvgRGBA(0,255,0,180));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);

            nvgBeginPath(args.vg);
            nvgMoveTo(args.vg, 0.f, yForBin(hi));
            nvgLineTo(args.vg, box.size.x, yForBin(hi));
            nvgStrokeColor(args.vg, nvgRGBA(255,0,0,180));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }

        // 3) Retângulo de seleção (apenas durante drag — mantém)
        if (dragging) {
            nvgBeginPath(args.vg);
            nvgRect(args.vg, std::min(a.x,b.x), std::min(a.y,b.y), std::fabs(b.x-a.x), std::fabs(b.y-a.y));
            nvgStrokeColor(args.vg, nvgRGBA(255,255,255,160));
            nvgStrokeWidth(args.vg, 1.f);
            nvgStroke(args.vg);
        }
    }
};

/*---------------------- Widget principal -----------------------*/
struct SpectroFXWidget : ModuleWidget {
    SpectroFXWidget(SpectroFXModule* module) {
        setModule(module);

        /* painel 45 HP (228.6 mm × 128.5 mm) */
        box.size = Vec(mm2pxf(228.6f), mm2pxf(128.5f));
        addChild(new SpectroFXPanel(box.size));

        /* parafusos nos 4 cantos */
        const float M = 1.0f; // margem em mm
        addChild(createWidget<ScrewBlack>(Vec(mm2pxf(M), mm2pxf(M))));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - mm2pxf(M) - RACK_GRID_WIDTH, mm2pxf(M))));
        addChild(createWidget<ScrewBlack>(Vec(mm2pxf(M), box.size.y - mm2pxf(M) - RACK_GRID_WIDTH)));
        addChild(createWidget<ScrewBlack>(Vec(box.size.x - mm2pxf(M) - RACK_GRID_WIDTH, box.size.y - mm2pxf(M) - RACK_GRID_WIDTH)));

        /* LED + texto sob o espectrograma (à esquerda) */
        const float specX = 67.f, specY = 17.f, specH = 81.f;
        const float ledX  = specX;
        const float ledY  = specY + specH + 2.f;

        auto* led = createWidget<ModeLED>(Vec(mm2pxf(ledX), mm2pxf(ledY)));
        ((ModeLED*)led)->mod = module;
        addChild(led);

        auto* modeTxt = new ModeText();
        modeTxt->mod = module;
        modeTxt->box.pos  = Vec(mm2pxf(ledX + 6.f), mm2pxf(ledY));
        modeTxt->box.size = Vec(mm2pxf(40.f), mm2pxf(4.f));
        addChild(modeTxt);

        /* knobs / portas (posições existentes) */
        float xL=34, xR=47, xL_CV=22, xR_CV=58, y0=22, dy=15.4;
        float ioLx=130, ioRx=185, ioY=115, iodX=15;
        auto mm = [](float x, float y) { return Vec(mm2pxf(x), mm2pxf(y)); };

        // Knobs L
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*0), module, SpectroFXModule::BLUR_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*1), module, SpectroFXModule::SHARPEN_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*2), module, SpectroFXModule::EDGE_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*3), module, SpectroFXModule::EMBOSS_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*4), module, SpectroFXModule::MIRROR_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*5), module, SpectroFXModule::GATE_PARAM_L));
        addParam(createParamCentered<RoundBlackKnob>(mm(xL,y0+dy*6), module, SpectroFXModule::STRETCH_PARAM_L));

        // CV L
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*0), module, SpectroFXModule::BLUR_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*1), module, SpectroFXModule::SHARPEN_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*2), module, SpectroFXModule::EDGE_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*3), module, SpectroFXModule::EMBOSS_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*4), module, SpectroFXModule::MIRROR_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*5), module, SpectroFXModule::GATE_CV_L));
        addInput(createInputCentered<PJ301MPort>(mm(xL_CV,y0+dy*6), module, SpectroFXModule::STRETCH_CV_L));

        // Knobs R
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*0), module, SpectroFXModule::BLUR_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*1), module, SpectroFXModule::SHARPEN_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*2), module, SpectroFXModule::EDGE_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*3), module, SpectroFXModule::EMBOSS_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*4), module, SpectroFXModule::MIRROR_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*5), module, SpectroFXModule::GATE_PARAM_R));
        addParam(createParamCentered<RoundBlackKnob>(mm(xR,y0+dy*6), module, SpectroFXModule::STRETCH_PARAM_R));

        // CV R
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*0), module, SpectroFXModule::BLUR_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*1), module, SpectroFXModule::SHARPEN_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*2), module, SpectroFXModule::EDGE_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*3), module, SpectroFXModule::EMBOSS_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*4), module, SpectroFXModule::MIRROR_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*5), module, SpectroFXModule::GATE_CV_R));
        addInput(createInputCentered<PJ301MPort>(mm(xR_CV,y0+dy*6), module, SpectroFXModule::STRETCH_CV_R));

        // Áudio I/O
        addInput(createInputCentered<PJ301MPort>(mm(ioLx,ioY), module, SpectroFXModule::AUDIO_INPUT_L));
        addInput(createInputCentered<PJ301MPort>(mm(ioRx,ioY), module, SpectroFXModule::AUDIO_INPUT_R));
        addOutput(createOutputCentered<PJ301MPort>(mm(ioLx+iodX, ioY), module, SpectroFXModule::BYPASS_OUTPUT_L));
        addOutput(createOutputCentered<PJ301MPort>(mm(ioRx+iodX, ioY), module, SpectroFXModule::BYPASS_OUTPUT_R));
        addOutput(createOutputCentered<PJ301MPort>(mm(ioLx+2*iodX, ioY), module, SpectroFXModule::PROCESSED_OUTPUT_L));
        addOutput(createOutputCentered<PJ301MPort>(mm(ioRx+2*iodX, ioY), module, SpectroFXModule::PROCESSED_OUTPUT_R));

        // Espectrograma (guarda ponteiro)
        auto* spec = new SpectrogramDisplay(module);
        addChild(spec);

        // Máscara 2D Overlay (usa o mesmo spec)
        auto specRect = Vec(mm2pxf(67),  mm2pxf(17));
        auto specSize = Vec(mm2pxf(154), mm2pxf(81));
        auto* mask = new MaskOverlay(module, specRect, specSize);
        addChild(mask);
    }

    // menu de contexto RAW / PV / PV-Lock
    void appendContextMenu(Menu* menu) override {
        auto* mod = dynamic_cast<SpectroFXModule*>(module);
        menu->addChild(new MenuSeparator());
        const char* lbl[] = {"RAW", "PV", "PV-Lock"};
        struct MI : MenuItem { SpectroFXModule* m=nullptr; int v=0;
            void onAction(const event::Action&) override {
                if (m) m->params[SpectroFXModule::PHASE_MODE_PARAM].setValue((float)v);
            }
            void step() override {
                rightText = (m && (int)m->params[SpectroFXModule::PHASE_MODE_PARAM].getValue()==v) ? "✔" : "";
                MenuItem::step();
            }
        };
        for (int i=0;i<3;++i) {
            auto* it = new MI; it->text = lbl[i]; it->m = mod; it->v = i; menu->addChild(it);
        }

        menu->addChild(new MenuSeparator());

        // Opções da máscara 2D
        struct ToggleMask : MenuItem { SpectroFXModule* m=nullptr;
            void onAction(const event::Action&) override { if (m) m->mask2d.enabled.store(!m->mask2d.enabled.load()); }
            void step() override { rightText = (m && m->mask2d.enabled.load()) ? "ON" : "OFF"; MenuItem::step(); }
        };
        auto* tm = new ToggleMask; tm->text = "Mask 2D"; tm->m = mod; menu->addChild(tm);

        struct Bounds : MenuItem { SpectroFXModule* m=nullptr; bool high=false;
            void onAction(const event::Action&) override {
                if (!m) return;
                int K = SpectroFXModule::N/2 + 1;
                // Exemplo simples: “snap” a 1/4 e 3/4 da banda (troca isto por um submenu com encoder se quiseres)
                int lo = K/4, hi = 3*K/4;
                m->mask2d.setBounds(lo, hi);
            }
        };
        auto* b = new Bounds; b->text = "Set bounds 25%..75%"; b->m = mod; menu->addChild(b);

        // Clear/Fill — versão full-width
        struct ClearMask : MenuItem { SpectroFXModule* m=nullptr;
            void onAction(const event::Action&) override { if (m) m->mask2d.enabled.store(false); }
        };
        auto* cl = new ClearMask; cl->text = "Clear mask (disable)"; cl->m = mod; menu->addChild(cl);

        struct FillMask : MenuItem { SpectroFXModule* m=nullptr;
            void onAction(const event::Action&) override {
                if (!m) return;
                int K = SpectroFXModule::N/2 + 1;
                m->mask2d.setBounds(0, K-1);       // toda a banda
                m->mask2d.enabled.store(true);
            }
        };
        auto* fl = new FillMask; fl->text = "Fill mask (full band)"; fl->m = mod; menu->addChild(fl);
    }
};
