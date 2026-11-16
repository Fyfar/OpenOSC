#pragma once
#include <cstddef>
#include <new>

namespace openosc {
// The scratch memory of every JSON file operation (theme, settings, calibration profiles): the
// file's text and its ArduinoJson document, in SRAM4 (.settings_workspace), never on the DTCM stack.
// main() and the controller's loop hold about 40 KiB of that stack for good, so what is left under
// the main loop (about 12 KiB) does not take a 4 KiB text and a 6 KiB document besides the FAT calls:
// a background settings save ran the stack into DataAccess that way (0.8.0).
// These operations run one at a time in the foreground. Nothing in the workspace outlives the
// operation that put it there: a parse keeps its result in the caller's own profile, and a text is
// written out before the next one is read or encoded.
class JsonWorkspace {
public:
    static constexpr size_t TextBytes = 10241;     // The largest file (the theme) and its terminator.
    static constexpr size_t DocumentBytes = 8832;  // The largest document (the theme's, checked here).
    static char *text();
    // A new, empty document of the caller's type each time: SRAM4 is NOLOAD and need not be zeroed.
    template <class Document> static Document &document() {
        static_assert(sizeof(Document) <= DocumentBytes && alignof(Document) <= 8, "Document exceeds the JSON workspace");
        return *new (documentMemory()) Document;
    }
private:
    static void *documentMemory();
};
}
