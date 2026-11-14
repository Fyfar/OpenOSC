#!/usr/bin/env python3
"""Board manifests: the single source of parameters shared by C++, PlatformIO and tools."""
from dataclasses import dataclass
import json
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
MANIFESTS = ROOT / "config/boards"
VECTOR_BASE = 0x08040400
SECTOR_BYTES = 8192


@dataclass(frozen=True)
class Board:
    id: str
    name: str
    model: int
    image_board_id: int
    flash_end: int

    @property
    def max_payload(self):
        return self.flash_end - VECTOR_BASE


def available():
    return tuple(sorted(path.stem for path in MANIFESTS.glob("*.json")))


def load(board_id):
    if board_id not in available():
        raise ValueError(f"unknown board {board_id!r}; expected one of {', '.join(available())}")
    data = json.loads((MANIFESTS / f"{board_id}.json").read_text())
    board = Board(data["id"], data["name"], int(data["model"]), int(data["image_board_id"], 16),
                  int(data["flash_end"], 16))
    if board.id != board_id:
        raise ValueError(f"inconsistent manifest for {board_id}")
    if board.flash_end % SECTOR_BYTES or board.flash_end <= VECTOR_BASE:
        raise ValueError(f"invalid Flash limit for {board_id}")
    return board
