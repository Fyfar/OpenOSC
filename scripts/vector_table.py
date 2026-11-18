#!/usr/bin/env python3
"""The Cortex-M7 vector table at the start of an OpenOSC image (src/startup/vectors.S), as a loader
hands it over: the factory boot checks word 0 (0x0800EB50: not 0/FFFFFFFF, top byte 0x20) and calls
word 1 without setting VTOR; our Handoff also sets VTOR to the table. So before Reset_Handler installs
its runtime table, every non-reserved entry must lead into the image itself (Thumb), and the
architecture's reserved entries 7..10 and 13 are 0 (issue #16).

Used by scripts/build.py on every Recovery and application image; also a command:
  vector_table.py firmware.elf firmware.bin
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ENTRIES = 171  # 16 core entries, IRQ 0..154 (vectors.S).
RESERVED = (7, 8, 9, 10, 13)


def section_address(elf: bytes, name: str) -> int:
    """The address of an ELF32 little-endian section (the image's origin: .isr_vector)."""
    if elf[:4] != b"\x7fELF" or elf[4] != 1 or elf[5] != 1:
        raise ValueError("Not an ELF32 little-endian file")
    shoff, = struct.unpack_from("<I", elf, 0x20)
    shentsize, shnum, shstrndx = struct.unpack_from("<HHH", elf, 0x2E)
    def header(index):
        return struct.unpack_from("<IIIIIIIIII", elf, shoff + index * shentsize)
    strings = header(shstrndx)[4]
    for index in range(shnum):
        entry = header(index)
        end = elf.index(b"\0", strings + entry[0])
        if elf[strings + entry[0]:end].decode() == name:
            return entry[3]
    raise ValueError(f"No {name} section")


def problems(image: bytes, origin: int) -> list[str]:
    if len(image) < ENTRIES * 4:
        return [f"image shorter than its {ENTRIES}-entry vector table"]
    words = struct.unpack_from(f"<{ENTRIES}I", image)
    found = []
    sp = words[0]
    if sp in (0, 0xFFFFFFFF) or sp >> 24 != 0x20 or sp & 7:
        found.append(f"initial MSP 0x{sp:08X}: factory boot needs 0x20xxxxxx, the ABI 8-byte alignment")
    end = origin + len(image)
    for index, word in enumerate(words[1:], 1):
        if index in RESERVED:
            if word:
                found.append(f"reserved entry {index} is 0x{word:08X}, not 0")
        elif not (word & 1 and origin <= (word & ~1) < end):
            found.append(f"entry {index} 0x{word:08X} is not Thumb code inside the image 0x{origin:08X}..0x{end:08X}")
    return found


def check(elf: Path, image: Path) -> None:
    found = problems(image.read_bytes(), section_address(elf.read_bytes(), ".isr_vector"))
    if found:
        raise ValueError(f"{image}: vector table: " + "; ".join(found))


if __name__ == "__main__":
    if len(sys.argv) != 3:
        sys.exit(__doc__)
    try:
        check(Path(sys.argv[1]), Path(sys.argv[2]))
    except ValueError as error:
        sys.exit(str(error))
    print(f"{sys.argv[2]}: vector table OK")
