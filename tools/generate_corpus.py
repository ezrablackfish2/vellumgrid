#!/usr/bin/env python3
"""Generate minimal valid VGD2 corpus seeds for VellumGrid fuzzing."""

from __future__ import annotations

import struct
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
CORPUS = ROOT / "corpus"


def pack_u16(v: int) -> bytes:
    return struct.pack("<H", v)


def pack_u32(v: int) -> bytes:
    return struct.pack("<I", v)


def job_header(count: int, nonce: int = 1, flags: int = 0) -> bytes:
    return b"VGD2" + pack_u16(2) + pack_u16(flags) + pack_u32(count) + pack_u32(nonce)


def command(opcode: int, payload: bytes, epoch: int = 1, flags: int = 1) -> bytes:
    return bytes([opcode, flags]) + pack_u16(len(payload)) + pack_u32(epoch) + payload


def write(name: str, data: bytes) -> None:
    CORPUS.mkdir(parents=True, exist_ok=True)
    path = CORPUS / name
    path.write_bytes(data)
    print(f"wrote {path} ({len(data)} bytes)")


def main() -> None:
    # Single BeginDocument
    write(
        "minimal_document.bin",
        job_header(1)
        + command(1, pack_u32(1)),  # BeginDocument id=1
    )

    # Document + page + font + style + text (deep replay path)
    body = b"".join(
        [
            command(1, pack_u32(100), epoch=1),
            command(2, pack_u32(1) + bytes([2]) + bytes([5]) + b"Serif", epoch=2),
            command(
                3,
                pack_u32(1) + pack_u32(1) + bytes([0, 0, 0xFF, 0x00]),
                epoch=3,
            ),
            command(4, pack_u32(1), epoch=4),
            command(5, pack_u32(1) + bytes([1, 0]), epoch=5),
            command(6, pack_u32(1) + pack_u32(1) + bytes([4]) + b"test", epoch=6),
        ]
    )
    write("booklet_typography.bin", job_header(6) + body)

    # Scheduled render path
    write(
        "scheduled_pages.bin",
        job_header(3)
        + command(1, pack_u32(7), epoch=1)
        + command(4, pack_u32(1), epoch=2)
        + command(37, pack_u32(1) + bytes([1, 0]), epoch=3),
    )

    # Clipboard roundtrip style job
    write(
        "clipboard_roundtrip.bin",
        job_header(4)
        + command(1, pack_u32(9), epoch=1)
        + command(43, pack_u32(1) + bytes([1, 0, 0, 0]), epoch=2)
        + command(45, pack_u32(1), epoch=3)
        + command(54, pack_u32(0), epoch=4),
    )


if __name__ == "__main__":
    main()
