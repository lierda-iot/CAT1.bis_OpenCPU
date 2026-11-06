"""Resolve the existing audio guard's private entries from the pinned ELF.

Reads only ELF headers, sections and symbol/string tables, never instructions.
This relocates the existing guard; it does not establish runtime ABI equivalence.
"""

from __future__ import annotations

import hashlib
from pathlib import Path
import struct
import sys


MASTER_SHA256 = "b2db1f908b409648e358f5211d50410a87b8b9381d9b8bcedeb802409860fa7f"
FUNCTIONS = (
    "ccioGetAudioServEnt",
    "ccioRbufGetFreePcbCnt",
    "ccioRbufUngetPtUlPcb",
    "halSetVoiceEngRetCallback",
)
POOL = "gRbufPtUlPcbMan"
POOL_SIZE = 528  # sizeof(pro_pcb_pool_t), including the sixteen 32-byte slots.


def read_symbols(data: bytes, expected_hash: str) -> dict[str, tuple[int, int, int, int]]:
    if hashlib.sha256(data).hexdigest() != expected_hash:
        raise ValueError("F6D_D ELF does not match the selected audio-guard baseline")
    if len(data) < 52:
        raise ValueError("Truncated ELF header")
    header = struct.unpack_from("<16sHHIIIIIHHHHHH", data)
    if header[0][:7] != b"\x7fELF\x01\x01\x01" or header[2] != 40:
        raise ValueError("Expected a little-endian ELF32 ARM file")
    offset, entry_size, count = header[6], header[11], header[12]
    if entry_size != 40 or count == 0 or offset + entry_size * count > len(data):
        raise ValueError("Invalid ELF section table")
    sections = [struct.unpack_from("<10I", data, offset + i * entry_size)
                for i in range(count)]
    wanted = set(FUNCTIONS) | {POOL}
    result = {}
    for section in sections:
        if section[1] != 2:  # SHT_SYMTAB
            continue
        start, size, link, stride = section[4], section[5], section[6], section[9]
        if stride != 16 or size % stride or start + size > len(data) or link >= count:
            raise ValueError("Invalid ELF symbol table")
        strings = sections[link]
        if strings[1] != 3 or strings[4] + strings[5] > len(data):
            raise ValueError("Invalid ELF string table")
        names = data[strings[4]:strings[4] + strings[5]]
        for pos in range(start, start + size, stride):
            name, value, length, info, _, index = struct.unpack_from("<IIIBBH", data, pos)
            if name >= len(names):
                raise ValueError("Invalid ELF symbol name offset")
            end = names.find(b"\0", name)
            if end < 0:
                raise ValueError("Unterminated ELF symbol name")
            symbol = names[name:end].decode("ascii", errors="replace")
            if symbol not in wanted or index == 0:
                continue
            if symbol in result or index >= count:
                raise ValueError(f"Duplicate or invalid symbol: {symbol}")
            owner = sections[index]
            address = value & ~1 if info & 15 == 2 else value
            if not length or address < owner[3] or address + length > owner[3] + owner[5]:
                raise ValueError(f"Symbol is outside its section: {symbol}")
            result[symbol] = value, length, info & 15, owner[2]
    return result


def link_flags(symbols: dict[str, tuple[int, int, int, int]]) -> str:
    flags = []
    for name in FUNCTIONS:
        if name not in symbols:
            raise ValueError(f"Missing audio-guard function: {name}")
        value, _, kind, section_flags = symbols[name]
        if kind != 2 or section_flags & 6 != 6:  # STT_FUNC, SHF_ALLOC|SHF_EXECINSTR
            raise ValueError(f"Audio-guard entry is not executable code: {name}")
        flags.append(f"-Wl,--defsym={name}=0x{value | 1:08x}")  # ARM Thumb entry
    if POOL not in symbols:
        raise ValueError(f"Missing audio-guard pool: {POOL}")
    value, size, kind, section_flags = symbols[POOL]
    if kind != 1 or size != POOL_SIZE or value % 4 or section_flags & 3 != 3:
        raise ValueError("Audio-guard pool type, size, alignment or section changed")
    flags.append(f"-Wl,--defsym=pro_audio_pcb_pool=0x{value:08x}")
    return " ".join(flags)


def main() -> None:
    if len(sys.argv) != 2:
        raise ValueError("Usage: resolve_audio_symbols.py <F6D_D/ap_lierda_app.elf>")
    print(link_flags(read_symbols(Path(sys.argv[1]).read_bytes(), MASTER_SHA256)))


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, struct.error) as error:
        print(f"Audio symbol resolution failed: {error}", file=sys.stderr)
        raise SystemExit(1)
