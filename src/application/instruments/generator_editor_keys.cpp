#include "openosc/application/controller.hpp"

namespace openosc {
// A committed WAVEFORM/PARAMETERS field: shape, frequency, level, codes and amplitude as the
// editor normalised them, together; refused, the editor shows the generator in force. The output
// switch stays its own action.
void ApplicationController::generatorEditorKeys(uint32_t keys, uint32_t now) {
    if (fieldKeys(keys, generatorEditor_, now) != FieldKey::Committed) return;
    if (!applyGenerator(generatorEditor_.settings(), now)) view_.notify("Unsupported waveform/frequency/duty", now);
    generatorEditor_.begin(data_.generator().settings(), generatorEditor_.wave());
    view_.properties(generatorEditor_, navigator_);
}
} // namespace openosc
