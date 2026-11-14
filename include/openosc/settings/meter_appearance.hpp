#pragma once
#include <cstdint>

namespace openosc {
// The DMM's elements whose look the user sets one by one (Measurement > Appearance), each over the
// base palette and reading face (MeterPreferences::palette, face): a colour from MeterSwatches, a
// font for every element that is text (never for the needle, the ticks, the arc or the signs), a
// style where its drawing has more than one form. Inherit everywhere is exactly the base look.
enum class MeterRole : uint8_t {
    Background,                       // The DMM body, the dial face and the trend's chart.
    Reading, Unit, Function,          // The main reading, its unit, the function sign.
    Arc, Ticks, ScaleNumbers, Needle, // The analog dial (ScaleNumbers: the numbers and their legend).
    StatNames, Minimum, Maximum, Average,
    Range,                            // Range / REL reference.
    Secondary,                        // The secondary reading (Hz beside V AC, ...).
    Mode,                             // AUTO / MANUAL.
    Details,                          // The info line: jacks, RMS, MEAN.
    Bar,                              // The bargraph's lit segments and the trend's line.
    CardActive, CardInactive,         // The F1..F4 cards' function signs: the decoded one, the others.
    CardText,                         // The cards' text (F1..F4, the unit on the selected one).
    CardSelection,                    // The selected card's frame and text (DMM only).
    Count
};
struct MeterRoleStyle {
    static constexpr uint8_t Inherit = 0xff;
    uint8_t color = Inherit, font = Inherit, style = Inherit; // Indices into the role's lists.
    bool operator==(const MeterRoleStyle &b) const { return color == b.color && font == b.font && style == b.style; }
};
struct MeterAppearance {
    static constexpr unsigned Roles = unsigned(MeterRole::Count);
    MeterRoleStyle roles[Roles]{};
    const MeterRoleStyle &operator[](MeterRole role) const { return roles[unsigned(role)]; }
    MeterRoleStyle &operator[](MeterRole role) { return roles[unsigned(role)]; }
    bool custom() const; // Any override.
    bool valid() const;  // Every override within its role's lists.
    bool operator==(const MeterAppearance &b) const;
    bool operator!=(const MeterAppearance &b) const { return !(*this == b); }
};
// The named colours a role may take, and what the user calls them.
struct MeterSwatch { const char *name, *key; uint32_t rgb; };
// One role: its menu name, its file key, its fonts and styles (null: none) and their file keys.
struct MeterRoleInfo {
    const char *name, *key;
    unsigned fonts;  const char *const *fontNames, *const *fontKeys;
    unsigned styles; const char *const *styleNames, *const *styleKeys;
};
class MeterAppearanceCatalog {
public:
    static constexpr unsigned Swatches = 16;
    static const MeterSwatch &swatch(unsigned index);
    static const MeterRoleInfo &role(MeterRole role);
    // A key's index in `keys` (a role's fonts or styles) or among the swatches; Inherit if none.
    static uint8_t find(const char *const *keys, unsigned count, const char *key);
    static uint8_t swatchByKey(const char *key);
    // Reading fonts: IBM, Mono, Segment (Default: the legacy face, MeterPreferences::face). Every
    // other text: IBM (IBM Plex Sans) or Mono (Noto Sans Mono), both with Ω, µ, ° and the minus at
    // each size the DMM draws (12, 14, 24).
    enum ReadingFont : uint8_t { ReadingIbm, ReadingMono, ReadingSegment };
    enum TextFont : uint8_t { TextIbm, TextMono };
    enum LineStyle : uint8_t { Thin, Bold }; // Arc, ticks, needle.
    // The look of the user's photo (dark body, white dial, light needle, amber reading and unit,
    // muted statistics, cyan cards), as overrides: applying it leaves every element editable.
    static MeterAppearance hybrid();
    static constexpr unsigned HybridPalette = 0, HybridFace = ReadingIbm; // Dark, IBM.
};
} // namespace openosc
