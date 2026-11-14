#pragma once
#include "openosc/ui/components/generator_preview.hpp"
#include "openosc/ui/components/parameter_bar.hpp"
#include "openosc/ui/components/meter_summary.hpp"
#include "openosc/ui/components/scope_footer.hpp"
#include "openosc/ui/components/meter_trend.hpp"
#include "openosc/ui/components/analog_dial.hpp"
#include "openosc/ui/components/function_icon.hpp"
#include "openosc/ui/components/meter_cards.hpp"
#include "openosc/ui/components/status_bar_layout.hpp"
#include "openosc/ui/waveform.hpp"

namespace openosc::ui {
// A single retained LVGL tree is shared by the three screen presenters.
enum class StatusTone : uint8_t { Neutral, Good, Warning, Alert };
class ScreenHost {
public:
    void status(const char *text, StatusTone tone); // The coloured state pill, right of the battery.
    void statusX(int x);
    int statusRight() const { return statusRight_; } // First pixel column right of the pill.
    void clear() { if (root_) lv_obj_delete(root_); }
    void build(lv_obj_t *parent, const Theme &theme);
    void activate(MeasurementMode mode);
    void overlay(bool parameters);
    const Theme &theme() const { return *theme_; }
    StatusTone tone_ = StatusTone(255);
    Elements &elements() { return elements_; }
    ParameterBar &parameters() { return parameters_; }
    WaveformWidget &waveform() { return waveform_; }
    GeneratorPreview &preview() { return preview_; }
    MeterSummary &meterSummary() { return meterSummary_; }
    ScopeFooter &scopeFooter() { return scopeFooter_; }
    const ScopeFooter &scopeFooter() const { return scopeFooter_; }
    MeterTrend &meterTrend() { return meterTrend_; }
    FunctionIcon &functionIcon() { return functionIcon_; }
    MeterCards &meterCards() { return meterCards_; }
    AnalogDial &dial() { return dial_; }
    lv_obj_t *meterPanel() const { return meterPanel_; } // The DMM body's background, behind every element.
    bool overlayActive() const { return overlay_; }

private:
    void placeStatus();
    lv_obj_t *root_ = nullptr, *chrome_ = nullptr, *meterPanel_ = nullptr;
    int statusX_ = StatusBarLayout::StatusX, statusWidth_ = 0, statusRight_ = 0;
    bool overlay_ = false;
    const Theme *theme_ = nullptr;
    Elements elements_;
    ParameterBar parameters_;
    WaveformWidget waveform_;
    GeneratorPreview preview_;
    MeterSummary meterSummary_;
    ScopeFooter scopeFooter_;
    MeterTrend meterTrend_;
    FunctionIcon functionIcon_;
    MeterCards meterCards_;
    AnalogDial dial_;
};
} // namespace openosc::ui
