#!/usr/bin/env python3
"""Package an application linked at 0x08040400 for OpenOSC Recovery."""
import argparse
from pathlib import Path
import struct
import zlib

import board_config

HEADER_BYTES = 1024
LOAD_ADDRESS = board_config.VECTOR_BASE
TRAILER_MAGIC = b"OSCTRLR1"
TRAILER = struct.Struct("<8s5I")


def read_trailer(image):
    """Return (board_id, program_bytes) of a valid trailer, or None for an unmarked image."""
    if len(image) < TRAILER.size:
        return None
    magic, board_id, load, program_bytes, _program_crc, crc = TRAILER.unpack_from(image, len(image) - TRAILER.size)
    if magic != TRAILER_MAGIC or crc != zlib.crc32(image[-TRAILER.size:-4]) or load != LOAD_ADDRESS:
        return None
    return board_id, program_bytes


def add_trailer(program, board):
    """Name the target board after the linked program; the program's own bytes do not move."""
    if read_trailer(program):
        raise ValueError("image already has a board marker")
    body = TRAILER_MAGIC + struct.pack("<4I", board.image_board_id, LOAD_ADDRESS, len(program), zlib.crc32(program))
    return program + body + struct.pack("<I", zlib.crc32(body))


def package(payload, version, board):
    marker = read_trailer(payload)
    if marker and marker[0] != board.image_board_id:
        raise ValueError(f"image is marked for another board, not {board.name}")
    encoded = version.encode("ascii")
    if not encoded or len(encoded) > 31 or any(c < 32 or c > 126 for c in encoded):
        raise ValueError("version must contain 1..31 printable ASCII characters")
    if not 8 <= len(payload) <= board.max_payload:
        raise ValueError("application does not fit in the application region")
    sp, pc = struct.unpack_from("<II", payload)
    if sp & 7 or not 0x20000000 < sp <= 0x2001FF00:
        raise ValueError("initial stack must be 8-byte aligned in DTCM below 0x2001FF00")
    if not pc & 1 or not LOAD_ADDRESS + 8 <= pc & ~1 < LOAD_ADDRESS + len(payload):
        raise ValueError(f"reset vector must point inside this payload at 0x{LOAD_ADDRESS:08X}")
    header = bytearray(HEADER_BYTES)
    struct.pack_into("<8s6I32s", header, 0, b"OPENOSC1", 1, HEADER_BYTES, board.image_board_id,
                     LOAD_ADDRESS, len(payload), zlib.crc32(payload), encoded)
    struct.pack_into("<I", header, 64, zlib.crc32(header))
    return bytes(header) + payload


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("input", type=Path)
    parser.add_argument("output", type=Path)
    parser.add_argument("--version", required=True)
    parser.add_argument("--board", required=True, choices=board_config.available())
    args = parser.parse_args()
    try:
        image = package(args.input.read_bytes(), args.version, board_config.load(args.board))
    except (ValueError, UnicodeError) as error:
        parser.error(str(error))
    args.output.write_bytes(image)
    print(f"{args.output}: {len(image)} bytes, payload CRC32 {zlib.crc32(image[HEADER_BYTES:]):08X}")


if __name__ == "__main__":
    main()
