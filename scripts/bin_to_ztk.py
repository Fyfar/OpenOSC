#!/usr/bin/env python3
"""Pack and unpack ZT703S/ZT706S factory ZTK updates using only Python's standard library."""
from __future__ import annotations

import argparse
from dataclasses import dataclass
import hashlib
from pathlib import Path
import struct

# DES permutations and S-boxes: NIST FIPS 46-3, Appendix 1.
IP = (
    58, 50, 42, 34, 26, 18, 10, 2, 60, 52, 44, 36, 28, 20, 12, 4,
    62, 54, 46, 38, 30, 22, 14, 6, 64, 56, 48, 40, 32, 24, 16, 8,
    57, 49, 41, 33, 25, 17, 9, 1, 59, 51, 43, 35, 27, 19, 11, 3,
    61, 53, 45, 37, 29, 21, 13, 5, 63, 55, 47, 39, 31, 23, 15, 7,
)
FP = (
    40, 8, 48, 16, 56, 24, 64, 32, 39, 7, 47, 15, 55, 23, 63, 31,
    38, 6, 46, 14, 54, 22, 62, 30, 37, 5, 45, 13, 53, 21, 61, 29,
    36, 4, 44, 12, 52, 20, 60, 28, 35, 3, 43, 11, 51, 19, 59, 27,
    34, 2, 42, 10, 50, 18, 58, 26, 33, 1, 41, 9, 49, 17, 57, 25,
)
EXPANSION = (
    32, 1, 2, 3, 4, 5, 4, 5, 6, 7, 8, 9, 8, 9, 10, 11, 12, 13,
    12, 13, 14, 15, 16, 17, 16, 17, 18, 19, 20, 21, 20, 21, 22,
    23, 24, 25, 24, 25, 26, 27, 28, 29, 28, 29, 30, 31, 32, 1,
)
P = (16, 7, 20, 21, 29, 12, 28, 17, 1, 15, 23, 26, 5, 18, 31, 10,
     2, 8, 24, 14, 32, 27, 3, 9, 19, 13, 30, 6, 22, 11, 4, 25)
PC1 = (
    57, 49, 41, 33, 25, 17, 9, 1, 58, 50, 42, 34, 26, 18,
    10, 2, 59, 51, 43, 35, 27, 19, 11, 3, 60, 52, 44, 36,
    63, 55, 47, 39, 31, 23, 15, 7, 62, 54, 46, 38, 30, 22,
    14, 6, 61, 53, 45, 37, 29, 21, 13, 5, 28, 20, 12, 4,
)
PC2 = (
    14, 17, 11, 24, 1, 5, 3, 28, 15, 6, 21, 10, 23, 19, 12, 4,
    26, 8, 16, 7, 27, 20, 13, 2, 41, 52, 31, 37, 47, 55, 30, 40,
    51, 45, 33, 48, 44, 49, 39, 56, 34, 53, 46, 42, 50, 36, 29, 32,
)
ZTK_PC2 = PC2[:33] + (34,) + PC2[34:]
ZTK_SBOX_OVERRIDES = {(5, 1, 6): 0, (6, 1, 5): 0}
SHIFTS = (1, 1, 2, 2, 2, 2, 2, 2, 1, 2, 2, 2, 2, 2, 2, 1)
SBOXES = (
    (14,4,13,1,2,15,11,8,3,10,6,12,5,9,0,7,
     0,15,7,4,14,2,13,1,10,6,12,11,9,5,3,8,
     4,1,14,8,13,6,2,11,15,12,9,7,3,10,5,0,
     15,12,8,2,4,9,1,7,5,11,3,14,10,0,6,13),
    (15,1,8,14,6,11,3,4,9,7,2,13,12,0,5,10,
     3,13,4,7,15,2,8,14,12,0,1,10,6,9,11,5,
     0,14,7,11,10,4,13,1,5,8,12,6,9,3,2,15,
     13,8,10,1,3,15,4,2,11,6,7,12,0,5,14,9),
    (10,0,9,14,6,3,15,5,1,13,12,7,11,4,2,8,
     13,7,0,9,3,4,6,10,2,8,5,14,12,11,15,1,
     13,6,4,9,8,15,3,0,11,1,2,12,5,10,14,7,
     1,10,13,0,6,9,8,7,4,15,14,3,11,5,2,12),
    (7,13,14,3,0,6,9,10,1,2,8,5,11,12,4,15,
     13,8,11,5,6,15,0,3,4,7,2,12,1,10,14,9,
     10,6,9,0,12,11,7,13,15,1,3,14,5,2,8,4,
     3,15,0,6,10,1,13,8,9,4,5,11,12,7,2,14),
    (2,12,4,1,7,10,11,6,8,5,3,15,13,0,14,9,
     14,11,2,12,4,7,13,1,5,0,15,10,3,9,8,6,
     4,2,1,11,10,13,7,8,15,9,12,5,6,3,0,14,
     11,8,12,7,1,14,2,13,6,15,0,9,10,4,5,3),
    (12,1,10,15,9,2,6,8,0,13,3,4,14,7,5,11,
     10,15,4,2,7,12,9,5,6,1,13,14,0,11,3,8,
     9,14,15,5,2,8,12,3,7,0,4,10,1,13,11,6,
     4,3,2,12,9,5,15,10,11,14,1,7,6,0,8,13),
    (4,11,2,14,15,0,8,13,3,12,9,7,5,10,6,1,
     13,0,11,7,4,9,1,10,14,3,5,12,2,15,8,6,
     1,4,11,13,12,3,7,14,10,15,6,8,0,5,9,2,
     6,11,13,8,1,4,10,7,9,5,0,15,14,2,3,12),
    (13,2,8,4,6,15,11,1,10,9,3,14,5,0,12,7,
     1,15,13,8,10,3,7,4,12,5,6,11,0,14,9,2,
     7,11,4,1,9,12,14,2,0,6,10,13,15,3,5,8,
     2,1,14,7,4,10,8,13,15,12,9,0,3,5,6,11),
)
REVERSE_BYTE = bytes(int(f"{value:08b}"[::-1], 2) for value in range(256))


def _permute(value: int, width: int, table: tuple[int, ...]) -> int:
    output = 0
    for position in table:
        output = (output << 1) | ((value >> (width - position)) & 1)
    return output


class ZtkCipher:
    """The factory DES variant, including its bit order, PC-2 and S-box changes."""

    def __init__(self, key: bytes):
        if len(key) != 8:
            raise ValueError("ZTK key must contain eight bytes")
        value = _permute(int.from_bytes(key.translate(REVERSE_BYTE), "big"), 64, PC1)
        left, right = value >> 28, value & 0xfffffff
        self.keys = []
        for shift in SHIFTS:
            left = ((left << shift) | (left >> (28 - shift))) & 0xfffffff
            right = ((right << shift) | (right >> (28 - shift))) & 0xfffffff
            self.keys.append(_permute((left << 28) | right, 56, ZTK_PC2))
        self.sp_boxes = []
        for box_index, box in enumerate(SBOXES):
            table = []
            for value in range(64):
                row = ((value >> 4) & 2) | (value & 1)
                column = (value >> 1) & 15
                result = ZTK_SBOX_OVERRIDES.get((box_index, row, column), box[row * 16 + column])
                result = int(f"{result:04b}"[::-1], 2)
                table.append(_permute(result << (28 - box_index * 4), 32, P))
            self.sp_boxes.append(table)

    def _block(self, block: bytes, *, decrypt: bool) -> bytes:
        value = _permute(int.from_bytes(block.translate(REVERSE_BYTE), "big"), 64, IP)
        left, right = value >> 32, value & 0xffffffff
        if decrypt:
            left, right = right, left
        for key in reversed(self.keys) if decrypt else self.keys:
            mixed = _permute(right, 32, EXPANSION) ^ key
            output = 0
            for index, table in enumerate(self.sp_boxes):
                output ^= table[(mixed >> (42 - index * 6)) & 63]
            left, right = right, left ^ output
        if decrypt:
            left, right = right, left
        result = _permute((left << 32) | right, 64, FP).to_bytes(8, "big")
        return result.translate(REVERSE_BYTE)

    def _transform(self, data: bytes, *, decrypt: bool) -> bytes:
        if len(data) % 8:
            raise ValueError("ZTK payload must be a multiple of eight bytes")
        cache = {}
        output = bytearray()
        for offset in range(0, len(data), 8):
            block = data[offset:offset + 8]
            if block not in cache:
                cache[block] = self._block(block, decrypt=decrypt)
            output.extend(cache[block])
        return bytes(output)

    def encrypt_ecb(self, plaintext: bytes) -> bytes:
        return self._transform(plaintext, decrypt=False)

    def decrypt_ecb(self, ciphertext: bytes) -> bytes:
        return self._transform(ciphertext, decrypt=True)


class ZtkChecksum:
    """Reproduce the loader's truncated length, skipped byte and final lookup."""

    TABLE = bytes.fromhex(
        "327c8900edd96877000000590000000000000000d69fdca81817aa90437327f6"
        "b968edf4ded409fee61c1c84dde80b992927000000000f0300e600000099b733"
        "5276941500f37c000026149e988f0000006a31168f8c059600e7b73f36000000"
        "00000000af669ed0385d8a6c20b95422007ced044c80387486edcbfa57cbdc19"
        "e22e1ddf5506e5cb01000000000000000000009a310086dbd0bc541613158600"
        "4e44bd7f9a15159001ea49b51653a78aeaf02fbd340000000000000000000000"
        "0000000000000000000000000000000000000000000000000000000000000000"
        "00000000000000000000000000000000000000000000000000cc31aa2c532e00"
    )

    @staticmethod
    def update(crc: int, value: int) -> int:
        mixed = (value ^ (crc & 0xff))
        mixed = (mixed ^ (mixed << 4)) & 0xff
        return (crc >> 8) ^ (mixed << 8) ^ (mixed << 3) ^ (mixed >> 4)

    @classmethod
    def compute(cls, plaintext: bytes) -> int:
        if len(plaintext) < 6:
            raise ValueError("ZTK checksum requires at least six plaintext bytes")
        crc = 0xf00f
        # The ARM wrapper explicitly narrows the full byte count to uint16_t.
        for value in plaintext[1:len(plaintext) & 0xffff]:
            crc = cls.update(crc, value)
        return cls.update(crc, cls.TABLE[plaintext[5]])


@dataclass(frozen=True)
class ModelProfile:
    name: str
    key: bytes

    @property
    def filename(self) -> str:
        return f"OpenOSC-Recovery-ZT{self.name}.ZTK"


MODELS = {
    "703": ModelProfile("703S", b"OSC7EF91"),
    "706": ModelProfile("706S", b"O706EF91"),
}


@dataclass(frozen=True)
class ContainerHeader:
    size: int
    checksum: int
    model: ModelProfile


class ZtkContainer:
    MIN_SIZE = 8
    MAX_SIZE = 0xc0000
    BLOCK_SIZE = 8
    HEADER = struct.Struct("<II8s")

    @classmethod
    def _check_size(cls, size: int) -> None:
        if not cls.MIN_SIZE <= size <= cls.MAX_SIZE:
            raise ValueError(f"Payload size must be {cls.MIN_SIZE}..{cls.MAX_SIZE} bytes")

    @classmethod
    def inspect(cls, container: bytes, expected_model: ModelProfile | None = None) -> ContainerHeader:
        if len(container) < cls.HEADER.size:
            raise ValueError("Truncated ZTK header")
        size, checksum, key = cls.HEADER.unpack_from(container)
        model = next((profile for profile in MODELS.values() if profile.key == key), None)
        if model is None:
            raise ValueError("Unsupported ZTK model/key")
        cls._check_size(size)
        if size % cls.BLOCK_SIZE:
            raise ValueError("ZTK payload size is not a multiple of eight bytes")
        if size != len(container) - cls.HEADER.size:
            raise ValueError("ZTK payload size does not match the file")
        if expected_model is not None and model != expected_model:
            raise ValueError(f"ZTK model is {model.name}, expected {expected_model.name}")
        return ContainerHeader(size, checksum, model)

    @classmethod
    def pack(cls, plaintext: bytes, model: ModelProfile) -> bytes:
        cls._check_size(len(plaintext))
        plaintext += b"\xff" * (-len(plaintext) % cls.BLOCK_SIZE)
        checksum = ZtkChecksum.compute(plaintext)
        cipher = ZtkCipher(model.key)
        encrypted = cipher.encrypt_ecb(plaintext)
        if cipher.decrypt_ecb(encrypted) != plaintext:
            raise ValueError("ZTK encryption round-trip verification failed")
        return cls.HEADER.pack(len(plaintext), checksum, model.key) + encrypted

    @classmethod
    def unpack(cls, container: bytes, expected_model: ModelProfile | None = None) -> bytes:
        header = cls.inspect(container, expected_model)
        plaintext = ZtkCipher(header.model.key).decrypt_ecb(container[cls.HEADER.size:])
        calculated = ZtkChecksum.compute(plaintext)
        if calculated != header.checksum:
            raise ValueError(f"ZTK checksum mismatch: header=0x{header.checksum:08x}, calculated=0x{calculated:04x}")
        return plaintext


def main() -> None:
    parser = argparse.ArgumentParser(
        description=__doc__,
        epilog="Default: BIN to ZTK (--model required). Use --unpack for ZTK to BIN; model is detected from the header.")
    parser.add_argument("source", type=Path, help="Application BIN to pack, or ZTK to unpack")
    parser.add_argument("--model", type=str.upper, choices=("703", "703S", "706", "706S"),
                        help="Target model for packing; optional expected model for unpacking")
    parser.add_argument("--unpack", action="store_true", help="Decrypt ZTK into BIN and verify its native checksum")
    parser.add_argument("-o", "--output", type=Path, help="Output path (default: beside the source file)")
    parser.add_argument("--force", action="store_true", help="Replace an existing output file")
    args = parser.parse_args()
    if not args.unpack and not args.model:
        parser.error("--model is required for packing")
    model = MODELS[args.model.removesuffix("S")] if args.model else None
    output = args.output or (args.source.with_suffix(".bin") if args.unpack
                             else args.source.with_name(model.filename))
    try:
        if output.resolve() == args.source.resolve() or (output.exists() and args.source.samefile(output)):
            raise ValueError("Output must differ from the input file")
        if output.exists() and not args.force:
            raise ValueError(f"Output already exists: {output}; use --force to replace it")
        limit = ZtkContainer.MAX_SIZE + (ZtkContainer.HEADER.size if args.unpack else 0)
        with args.source.open("rb") as source:
            data = source.read(limit + 1)
        if args.unpack:
            result = ZtkContainer.unpack(data, model)
            header = ZtkContainer.inspect(data, model)
        else:
            result = ZtkContainer.pack(data, model)
            header = ZtkContainer.inspect(result)
        output.parent.mkdir(parents=True, exist_ok=True)
        with output.open("wb" if args.force else "xb") as destination:
            destination.write(result)
    except (OSError, ValueError) as error:
        parser.error(str(error))

    print(f"Model: {header.model.name}; key: {header.model.key.decode('ascii')}")
    if args.unpack:
        print(f"Unpacked {len(data)} ZTK bytes into {len(result)} BIN bytes; native checksum: OK")
    else:
        print(f"BIN: {len(data)} bytes; payload: {header.size}; FF padding: {header.size - len(data)}")
        print("Encryption round-trip: OK")
    print(f"Native checksum: 0x{header.checksum:04x}")
    print(f"{'BIN' if args.unpack else 'ZTK'}: {output} ({len(result)} bytes)")
    print(f"SHA256: {hashlib.sha256(result).hexdigest()}")


if __name__ == "__main__":
    main()
