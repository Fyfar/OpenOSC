#pragma once
#include "ff.h"

namespace openosc::storage {
// `directory` and every missing parent, idempotently. Something that exists but is not a directory
// fails (FR_EXIST): nothing is removed or renamed. FR_OK when the whole chain is a directory.
FRESULT ensureDirectory(const char *directory);
// Layout::FactoryRootDirectories removed where they are empty directories (a non-empty one, a file of
// that name or anything else is kept), then the whole /openosc tree (Layout::Directories): every
// mount, the bootstrap, a fresh format. Storage errors are returned, not skipped.
FRESULT prepareLayout();
} // namespace openosc::storage
