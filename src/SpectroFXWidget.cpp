#include "SpectroFXWidget.hpp"
#include "componentlibrary.hpp"   // Para RoundBlackKnob, PJ301MPort etc.

// Se quiser usar namespace rack sem precisar de prefixo:
using namespace rack;

SpectroFXModuleWidget::SpectroFXModuleWidget(SpectroFXModule* module) {
    // Define o "Module" DSP atrelado a este widget
    setModule(module);

    // Dimensão do painel: por exemplo, 6 HP de largura
    box.size = Vec(6 * RACK_GRID_WIDTH, RACK_GRID_HEIGHT);

    // Carrega o painel SVG
    setPanel(APP->window->loadSvg(
        asset::plugin(pluginInstance, "res/SpectroFXPanel.svg")
    ));

    // -----------------
    // Exemplo: adiciona um knob (Blur Amount)
    addParam(createParam<RoundBlackKnob>(
        Vec(20, 60),                // Posição X=20, Y=60 (exemplo)
        module,
        SpectroFXModule::BLUR_AMOUNT_PARAM
    ));

    // -----------------
    // Exemplo: portas de entrada e saída
    addInput(createInput<PJ301MPort>(
        Vec(10, 120),
        module,
        SpectroFXModule::AUDIO_IN
    ));

    addOutput(createOutput<PJ301MPort>(
        Vec(40, 120),
        module,
        SpectroFXModule::AUDIO_OUT
    ));
}
