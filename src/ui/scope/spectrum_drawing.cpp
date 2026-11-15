#include "openosc/ui/scope/painter.hpp"
#include "openosc/ui/scope/format.hpp"
#include "openosc/ui/scope/display_scale.hpp"
#include <cstdio>

namespace openosc::plot {
void Painter::spectrum() {
    const auto &result=model_.spectrum();
    const auto &options=model_.options().spectrum;
    const int width=model_.viewport().width,height=model_.viewport().height;
    if (!result.valid) { text("FFT SOURCE UNAVAILABLE",4,18,palette_.readout,width-8,true); return; }
    const auto color=palette_.channels[options.channel];
    trace(model_.storage().fft,model_.options().mode==Mode::SpectrumOverlay ? palette_.math : color,255,1,Style::Vectors);
    if (width<160 || height<60) return;
    constexpr const char *windows[]={"Rect","Hann","Hamming","Blackman"};
    char label[64],value[32];
    std::snprintf(label,sizeof label,"FFT CH%u %s %s N%u",options.channel+1,DisplayScale::coupling(model_.metadata().channels[options.channel]),windows[unsigned(options.window)],result.count);
    text(label,4,2,color,190,true);
    if (options.scale==scope::SpectrumScale::Dbfs) text("0 dBFS",width-60,17,palette_.readout,60,true);
    else {
        const auto &channel=model_.metadata().channels[options.channel];
        const double factor=channel.calibrated ? channel.voltsPerCode*channel.probeNumerator/channel.probeDenominator : 1;
        Format::quantity(result.maximum/.85*factor,channel.calibrated ? "Vpk" : "cnt",value,sizeof value,3);
        text(value,width-80,17,palette_.readout,80,true);
    }
    Format::quantity(model_.spectrumFrequency(0),"Hz",value,sizeof value);
    text(value,4,height-14,palette_.readout,90,true);
    Format::quantity(model_.spectrumFrequency(1000),"Hz",value,sizeof value);
    text(value,width-80,height-14,palette_.readout,78,true);
    if (options.scale==scope::SpectrumScale::Dbfs) text("-80",width-28,height-28,palette_.readout,28,true);
    if (model_.options().mode==Mode::Spectrum && (model_.options().cursors.time || model_.options().cursors.voltage)) cursors();
}
} // namespace openosc::plot
