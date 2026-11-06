#!/usr/bin/env python3
"""Build/verify the LVGL font entirely from packaged glyph sources.
No image generation, downloads, installed fonts, Node or Pillow are required.
Run: python ui/tools/prepare_font.py --strict
Optional --verify-lvgl requires a native host C compiler (TIRTC_HOST_CC).
"""
from __future__ import annotations

import argparse
import gzip
import hashlib
import json
import re
from pathlib import Path
import functools
import subprocess
import tempfile
import os
import shutil
import sys

UI_ROOT = Path(__file__).resolve().parent.parent
SOURCE_ROOT = Path(__file__).resolve().parent / "font-sources"
COMMIT = "f7186630ed63f1e26838b8394e3e83a7e032646c"
PROJECT_ROOT = UI_ROOT.parents[3]
LVGL_ROOT = PROJECT_ROOT / "components/thirdparty/lvgl/lvgl-8.3.10"
sys.path.insert(0, str(UI_ROOT.parent / "resource_store"))
from font_resources import externalize_chinese
TOKEN = re.compile(r'"(?:\\.|[^"\\])*"|/\*.*?\*/|//[^\n]*', re.S)
NUMBER = re.compile(r'-?0x[0-9a-fA-F]+|-?\d+')
FIELDS = ("bitmap_index", "adv_w", "box_w", "box_h", "ofs_x", "ofs_y")

# Captions arrive at runtime and cannot be covered by scanning C literals.
# Keep this small, explicit baseline in addition to all GB2312 Hanzi: Chinese
# punctuation, typographic quotes/dashes, fullwidth ASCII, units and operators.
CAPTION_CHARACTERS = (
    "、，。！？：；（）【】《》〈〉「」『』〔〕〖〗"
    "“”‘’—–‐―…‥·•　"
    "°℃℉±×÷≤≥≠≈∞‰￥€§※→←↑↓✓"
)
# These Unicode variants have the same readable meaning. Reuse the packaged
# glyph pixels rather than introducing another font or a large Unicode atlas.
# Existing source glyphs always take precedence over these fallback aliases.
CAPTION_GLYPH_ALIASES = {
    0x00A0: 0x0020,  # no-break space
    0x202F: 0x0020,  # narrow no-break space
    0x2011: 0x2010,  # non-breaking hyphen
    0x2012: 0x2013,  # figure dash
    0x2027: 0x00B7,  # hyphenation point
    0x2212: 0x2013,  # minus sign
    0x301C: 0xFF5E,  # wave dash
    0x00A5: 0xFFE5,  # yen/yuan sign
    0xFF3E: 0x005E,  # fullwidth circumflex
    0xFF40: 0x0060,  # fullwidth grave accent
}


def caption_characters() -> set[int]:
    return (set(map(ord, CAPTION_CHARACTERS)) | set(range(0xFF01, 0xFF5F)) |
            set(CAPTION_GLYPH_ALIASES))

@functools.lru_cache(maxsize=2)
def read_source(name: str) -> str:
    raw = gzip.decompress((SOURCE_ROOT / (name + ".gz")).read_bytes())
    manifest = json.loads((SOURCE_ROOT / "MANIFEST.json").read_text(encoding="utf-8"))
    if hashlib.sha256(raw).hexdigest() != manifest[name]["sha256"]:
        raise ValueError("Packaged font source hash mismatch: " + name)
    return raw.decode("utf-8-sig")


def without_comments(value: str) -> str:
    return re.sub(r'/\*.*?\*/|//[^\n]*', '', value, flags=re.S)


def numbers(value: str) -> list[int]:
    return [int(v, 16) if "x" in v.lower() else int(v) for v in NUMBER.findall(without_comments(value))]


def array_body(source: str, name: str) -> str:
    match = re.search(r'\b' + re.escape(name) + r'\s*\[\s*\]\s*=\s*\{(.*?)\};', source, re.S)
    if not match:
        raise ValueError("Missing upstream array: " + name)
    return match.group(1)


def c_bytes(data: bytes, indent: str = "    ") -> str:
    return "\n".join(indent + ", ".join(f"0x{x:02x}" for x in data[i:i+16]) + "," for i in range(0, len(data), 16))


def write(path: Path, content: str) -> None:
    path.parent.mkdir(parents=True, exist_ok=True)
    temporary = path.with_suffix(path.suffix + ".tmp")
    temporary.write_text(content, encoding="utf-8", newline="\n")
    temporary.replace(path)


def decode_c_string(token: str) -> str:
    value = token[1:-1]
    output = bytearray()
    index = 0
    escapes = {"n": 10, "r": 13, "t": 9, "b": 8, "f": 12, "v": 11, "a": 7,
               "\\": 92, '"': 34, "'": 39, "?": 63}
    while index < len(value):
        char = value[index]
        index += 1
        if char != "\\":
            output.extend(char.encode("utf-8"))
            continue
        if index == len(value):
            break
        escape = value[index]
        index += 1
        if escape in escapes:
            output.append(escapes[escape])
        elif escape in "uU":
            count = 4 if escape == "u" else 8
            output.extend(chr(int(value[index:index+count], 16)).encode("utf-8"))
            index += count
        elif escape == "x":
            match = re.match(r'[0-9A-Fa-f]+', value[index:])
            if match:
                output.append(int(match.group(), 16) & 0xff)
                index += len(match.group())
        elif escape in "01234567":
            match = re.match(r'[0-7]{0,2}', value[index:])
            output.append(int(escape + match.group(), 8) & 0xff)
            index += len(match.group())
        elif escape == "\n":
            pass
        else:
            output.extend(escape.encode("utf-8"))
    return output.decode("utf-8", errors="replace")


def literal_characters(source: str) -> set[int]:
    result = set()
    for token in TOKEN.finditer(source):
        if token.group().startswith('"'):
            result.update(map(ord, decode_c_string(token.group())))
    return {code for code in result if code >= 32 and code != 127 and code != 0xfeff}


def gb2312_hanzi() -> set[int]:
    """GB2312 rows 16..87: 3755 first-level + 3008 second-level Hanzi.

    Python's bundled codec supplies the code point table, so no font/network
    dependency is needed to reproduce the same 6763-character baseline.
    """
    result = set()
    for lead in range(0xb0, 0xf8):
        for trail in range(0xa1, 0xff):
            try:
                result.add(ord(bytes((lead, trail)).decode("gb2312")))
            except UnicodeDecodeError:
                pass  # The final five entries of row 55 are unassigned.
    if len(result) != 6763:
        raise ValueError("GB2312 codec did not produce the expected 6763 Hanzi")
    return result


def requested_characters(args) -> tuple[set[int], list[str]]:
    requested = set(range(32, 127)) | gb2312_hanzi()
    requested.update(caption_characters())
    scanned, symbols = [], set()
    # Board status/events are formatted outside ui/ and still appear on screen.
    # Include the whole active tirtc app, keeping packaged source tools out.
    scan_root = UI_ROOT.parent
    for path in sorted(scan_root.rglob("*")):
        if path.suffix.lower() not in (".c", ".h", ".inc"):
            continue
        relative = path.relative_to(scan_root)
        if relative.parts[:2] == ("ui", "tools"):
            continue
        source = path.read_text(encoding="utf-8-sig")
        requested.update(literal_characters(source))
        symbols.update(re.findall(r'\bLV_SYMBOL_[A-Z0-9_]+\b', without_comments(source)))
        scanned.append(relative.as_posix())
    # Expand symbols used in C into their real UTF-8 glyphs, including those
    # supplied by the existing Montserrat fallback, instead of trusting names.
    definitions = (LVGL_ROOT / "src/font/lv_symbol_def.h").read_text(encoding="utf-8")
    macro_strings = dict(re.findall(r'^#define\s+(LV_SYMBOL_\w+)\s+("(?:\\.|[^"\\])*\")', definitions, re.M))
    for symbol in sorted(symbols):
        if symbol not in macro_strings:
            raise ValueError("Unknown LVGL symbol: " + symbol)
        requested.update(literal_characters(macro_strings[symbol]))
    for path in args.extra_text:
        requested.update(map(ord, path.read_text(encoding="utf-8-sig")))
    extra = UI_ROOT / "font/extra_characters.txt"
    if extra.exists():
        requested.update(map(ord, extra.read_text(encoding="utf-8-sig")))
    args.symbols_checked = sorted(symbols)
    return {cp for cp in requested if cp >= 32 and cp not in (127, 0xFEFF)}, scanned


def parse_font(source: str, allow_small: bool = False):
    if not re.search(r'\.bpp\s*=\s*4\b', source) or not re.search(r'\.bitmap_format\s*=\s*0\b', source):
        raise ValueError("Only original uncompressed 4bpp font is supported")
    bitmap = bytes(numbers(array_body(source, "glyph_bitmap")))
    glyphs = []
    for descriptor in re.findall(r'\{([^{}]+)\}', array_body(source, "glyph_dsc")):
        fields = {key: int(value) for key, value in re.findall(r'\.(\w+)\s*=\s*(-?\d+)', descriptor)}
        if all(key in fields for key in FIELDS):
            glyphs.append(fields)
    cmap_source = array_body(source, "cmaps")
    mapping = {}
    for descriptor in re.findall(r'\{([^{}]+)\}', cmap_source):
        fields = dict(re.findall(r'\.(\w+)\s*=\s*([\w]+)', descriptor))
        start, length, glyph_start = (int(fields[k]) for k in ("range_start", "range_length", "glyph_id_start"))
        kind = fields["type"]
        if kind.endswith("FORMAT0_TINY"):
            offsets, glyph_offsets = range(length), range(length)
        elif kind.endswith("FORMAT0_FULL"):
            offsets = range(length)
            glyph_offsets = numbers(array_body(source, fields["glyph_id_ofs_list"]))
        elif kind.endswith("SPARSE_TINY"):
            offsets = numbers(array_body(source, fields["unicode_list"]))
            glyph_offsets = range(len(offsets))
        elif kind.endswith("SPARSE_FULL"):
            offsets = numbers(array_body(source, fields["unicode_list"]))
            glyph_offsets = numbers(array_body(source, fields["glyph_id_ofs_list"]))
        else:
            raise ValueError("Unsupported cmap " + kind)
        for relative, glyph_offset in zip(offsets, glyph_offsets):
            mapping[start + relative] = glyph_start + glyph_offset
    if not allow_small and (len(glyphs) < 100 or len(mapping) < 100):
        raise ValueError("Upstream font parsing produced incomplete glyph tables")
    return bitmap, glyphs, mapping


EXTERNAL_BITMAP_READER = "#ifdef TIRTC_EXTERNAL_UI_ASSETS\nstatic tirtc_font_bitmap_reader_t external_bitmap_reader;\n/* Original descriptors/metrics/fallback remain LVGL's. Only bitmap delivery\n * changes; the single software-render owner consumes each pointer synchronously. */\nstatic const uint8_t *tirtc_font_get_bitmap_external(const lv_font_t *font, uint32_t letter)\n{\n    static const uint8_t empty_bitmap = 0;\n    if (!external_bitmap_reader) {\n        return font_dsc.glyph_bitmap ? lv_font_get_bitmap_fmt_txt(font, letter) : NULL;\n    }\n    if (letter == '\\t') letter = ' ';\n    for (unsigned map = 0; map < sizeof(cmaps) / sizeof(cmaps[0]); ++map) {\n        const lv_font_fmt_txt_cmap_t *cmap = &cmaps[map];\n        if (letter < cmap->range_start || letter - cmap->range_start >= cmap->range_length) continue;\n        uint32_t relative = letter - cmap->range_start;\n        unsigned low = 0, high = cmap->list_length;\n        while (low < high) {\n            unsigned middle = low + (high - low) / 2U;\n            if (cmap->unicode_list[middle] < relative) low = middle + 1U;\n            else high = middle;\n        }\n        if (low == cmap->list_length || cmap->unicode_list[low] != relative) return NULL;\n        const lv_font_fmt_txt_glyph_dsc_t *glyph = &glyph_dsc[cmap->glyph_id_start + low];\n        uint32_t bytes = ((uint32_t)glyph->box_w * glyph->box_h * 4U + 7U) / 8U;\n        if (!bytes) return &empty_bitmap;\n        return external_bitmap_reader(glyph->bitmap_index, bytes);\n    }\n    return NULL;\n}\n#endif"


def build_font(args) -> dict:
    bitmap, glyphs, mapping = parse_font(read_source("lv_font_cn_14.c"))
    # Upstream's multi-font command omits U+76CA (Yi/benefit). Its source
    # bitmap jumps from U+76C9 to U+76CB. Restore the matching SimHei 14px
    # glyph from an offline supplement, without changing existing glyphs.
    supplemental = []
    if (SOURCE_ROOT / "supplemental_font_14.c.gz").exists():
        patch_bitmap, patch_glyphs, patch_map = parse_font(read_source("supplemental_font_14.c"), allow_small=True)
        base = len(bitmap)
        bitmap += patch_bitmap
        for code, glyph_id in sorted(patch_map.items()):
            if code not in mapping:
                glyph = dict(patch_glyphs[glyph_id])
                glyph["bitmap_index"] += base
                mapping[code] = len(glyphs)
                glyphs.append(glyph)
                supplemental.append(f"U+{code:04X} {chr(code)}")
    aliases = {}
    for code, target in CAPTION_GLYPH_ALIASES.items():
        if code not in mapping:
            if target not in mapping:
                raise ValueError(f"Missing caption alias source U+{target:04X}")
            mapping[code] = mapping[target]
            aliases[f"U+{code:04X}"] = f"U+{target:04X}"
    requested, scanned = requested_characters(args)
    _, _, fallback_mapping = parse_font((LVGL_ROOT / "src/font/lv_font_montserrat_14.c").read_text(encoding="utf-8"))
    selected = sorted(requested & mapping.keys())
    fallback_only = sorted((requested - mapping.keys()) & fallback_mapping.keys())
    missing = sorted(requested - mapping.keys() - fallback_mapping.keys())
    if missing:
        raise ValueError("Missing source glyphs: " + ", ".join(f"U+{c:04X}" for c in missing))
    new_bitmap = bytearray()
    new_glyphs = [{key: 0 for key in FIELDS}]
    for code in selected:
        glyph = dict(glyphs[mapping[code]])
        count = (glyph["box_w"] * glyph["box_h"] * 4 + 7) // 8
        start = glyph["bitmap_index"]
        pixels = bitmap[start:start+count]
        if len(pixels) != count:
            raise ValueError(f"Glyph U+{code:04X} bitmap bounds invalid")
        glyph["bitmap_index"] = len(new_bitmap)
        new_bitmap.extend(pixels)
        if glyph["adv_w"] > 4095 or glyph["box_w"] > 255 or glyph["box_h"] > 255 or not (-128 <= glyph["ofs_x"] <= 127 and -128 <= glyph["ofs_y"] <= 127):
            raise ValueError(f"Glyph U+{code:04X} exceeds LV_FONT_FMT_TXT_LARGE=0")
        new_glyphs.append(glyph)
    if len(new_bitmap) >= 1048576 or len(new_glyphs) > 65535:
        raise ValueError("Subset too large; reduce extra characters for embedded Flash")
    # LVGL stores range length/offset in uint16_t, so split Unicode planes.
    groups = []
    for index, code in enumerate(selected, start=1):
        if not groups or code - groups[-1][0][1] >= 65535:
            groups.append([])
        groups[-1].append((index, code))
    out = ['/* Generated by tools/prepare_font.py from upstream lv_font_cn_14; see tools/font-sources/SOURCE.md. */',
           '#include "tirtc_font_14.h"', '#if LVGL_VERSION_MAJOR != 8',
           '#error "tirtc_font_14 is generated for LVGL 8"', '#endif',
           '#ifndef TIRTC_EXTERNAL_UI_ASSETS',
           'static const uint8_t glyph_bitmap[] = {', c_bytes(bytes(new_bitmap)), '};', '#endif',
           'static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[] = {']
    for glyph in new_glyphs:
        out.append('    {' + ', '.join('.' + key + ' = ' + str(glyph[key]) for key in FIELDS) + '},')
    out.append('};')
    for index, group in enumerate(groups):
        start = group[0][1]
        offsets = [code - start for _, code in group]
        out.extend([f'static const uint16_t unicode_list_{index}[] = {{'])
        for pos in range(0, len(offsets), 12):
            out.append('    ' + ', '.join(str(value) for value in offsets[pos:pos+12]) + ',')
        out.append('};')
    out.append('static const lv_font_fmt_txt_cmap_t cmaps[] = {')
    for index, group in enumerate(groups):
        out.append('    {' + f'.range_start = {group[0][1]}, .range_length = {group[-1][1]-group[0][1]+1}, .glyph_id_start = {group[0][0]}, '
                   + f'.unicode_list = unicode_list_{index}, .glyph_id_ofs_list = NULL, .list_length = {len(group)}, .type = LV_FONT_FMT_TXT_CMAP_SPARSE_TINY' + '},')
    out.extend(['};', 'static lv_font_fmt_txt_glyph_cache_t cache;',
                'static lv_font_fmt_txt_dsc_t font_dsc = {',
                '#ifdef TIRTC_EXTERNAL_UI_ASSETS', '    .glyph_bitmap = NULL,',
                '#else', '    .glyph_bitmap = glyph_bitmap,', '#endif',
                '    .glyph_dsc = glyph_dsc, .cmaps = cmaps,',
                f'    .kern_dsc = NULL, .kern_scale = 0, .cmap_num = {len(groups)}, .bpp = 4,',
                '    .kern_classes = 0, .bitmap_format = 0, .cache = &cache,', '};',
                EXTERNAL_BITMAP_READER, 'const lv_font_t tirtc_font_14 = {',
                '    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,',
                '#ifdef TIRTC_EXTERNAL_UI_ASSETS', '    .get_glyph_bitmap = tirtc_font_get_bitmap_external,', '#else', '    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,', '#endif',
                '    .line_height = 20, .base_line = 6, .subpx = LV_FONT_SUBPX_NONE,',
                '    .underline_position = -1, .underline_thickness = 1,',
                '    .dsc = &font_dsc, .fallback = &lv_font_montserrat_14, .user_data = NULL,', '};', '',
                '#ifdef TIRTC_EXTERNAL_UI_ASSETS',
                'int tirtc_font_set_reader(tirtc_font_bitmap_reader_t reader, uint32_t length)', '{',
                f'    if (!reader || length != {len(new_bitmap)}U) return -1;',
                '    font_dsc.glyph_bitmap = NULL;', '    external_bitmap_reader = reader;', '    return 0;', '}',
                'int tirtc_font_set_bitmap(const uint8_t *bitmap, uint32_t length)', '{',
                f'    if (!bitmap || length != {len(new_bitmap)}U) return -1;',
                '    external_bitmap_reader = NULL;', '    font_dsc.glyph_bitmap = bitmap;', '    return 0;', '}', '#endif', ''])
    write(UI_ROOT / "font/tirtc_font_14.c", externalize_chinese('\n'.join(out)))
    write(UI_ROOT / "font/tirtc_font_14.h", '#pragma once\n#include "lvgl.h"\n#ifdef __cplusplus\nextern "C" {\n#endif\nLV_FONT_DECLARE(tirtc_font_14);\n#ifdef TIRTC_EXTERNAL_UI_ASSETS\n/* Resource worker only, before exposing this font to LVGL. */\nint tirtc_font_load_external(void (*progress)(uint32_t));\ntypedef const uint8_t *(*tirtc_font_bitmap_reader_t)(uint32_t offset, uint32_t bytes);\n/* LVGL owner only; install once before attaching Chinese labels. */\nint tirtc_font_set_reader(tirtc_font_bitmap_reader_t reader, uint32_t length);\nint tirtc_font_set_bitmap(const uint8_t *bitmap, uint32_t length);\n#endif\n#ifdef __cplusplus\n}\n#endif\n')
    write(UI_ROOT / "font/included_characters.txt", ''.join(map(chr, selected)) + '\n')
    return {"font_glyphs": len(selected), "font_bitmap_bytes": len(new_bitmap),
            "font_bpp": 4, "font_bitmap_format": "uncompressed",
            "font_gb2312_hanzi_required": 6763,
            "font_gb2312_hanzi_included": len(gb2312_hanzi() & set(selected)),
            "font_caption_codepoints": sorted(caption_characters()),
            "font_caption_glyph_aliases": aliases,
            "font_descriptor_bytes_estimate": len(new_glyphs) * 8 + len(selected) * 2 + len(groups) * 20,
            "source_files_scanned": scanned,
            "supplemental_characters": supplemental,
            "missing_characters": [f"U+{code:04X} {chr(code)}" for code in missing],
            "required_codepoints": sorted(requested),
            "fallback_codepoints": fallback_only,
            "lvgl_symbols_checked": args.symbols_checked,
            "glyph_source_sha256": hashlib.sha256(read_source("lv_font_cn_14.c").encode("utf-8")).hexdigest()}


def host_compiler() -> Path:
    # Optional host-side glyph check; normal generation needs only Python.
    # Do not depend on retired test archives or download a compiler implicitly.
    configured = os.environ.get("TIRTC_HOST_CC")
    executable = configured or shutil.which("tcc") or shutil.which("gcc") or shutil.which("clang")
    if executable and Path(executable).is_file():
        return Path(executable)
    raise RuntimeError("--verify-lvgl requires a native host C compiler; set TIRTC_HOST_CC, "
                       "or omit --verify-lvgl to use the built-in font table validation")


def verify_lvgl(report: dict) -> str:
    # Compile a temporary checker so no test main() can enter the firmware's
    # ui/*/*.c wildcard. All inputs are the generated font and bundled LVGL.
    codepoints = report["required_codepoints"]
    c = '''#include <stdio.h>
#include <stdint.h>
#include <string.h>
#include "font/tirtc_font_14.h"
static const uint32_t required[] = {''' + ",".join(str(cp) for cp in codepoints) + '''};
int main(void) {
    unsigned checked = 0, fallback = 0;
    lv_init();
    for (unsigned i = 0; i < sizeof(required)/sizeof(required[0]); ++i) {
        lv_font_glyph_dsc_t glyph;
        memset(&glyph, 0, sizeof(glyph));
        uint32_t cp = required[i];
        if (!lv_font_get_glyph_dsc(&tirtc_font_14, &glyph, cp, 0) ||
            glyph.is_placeholder || !glyph.resolved_font) {
            fprintf(stderr, "MISSING U+%04X\\n", (unsigned)cp); return 1;
        }
        if (glyph.resolved_font != &tirtc_font_14) ++fallback;
        if (cp != 32 && cp != 160 && cp != 0x202f && cp != 0x3000) {
            const uint8_t *pixels = lv_font_get_glyph_bitmap(glyph.resolved_font, cp);
            if (!pixels || !glyph.box_w || !glyph.box_h) {
                fprintf(stderr, "EMPTY U+%04X\\n", (unsigned)cp); return 2;
            }
            unsigned count = (glyph.box_w * glyph.box_h * glyph.bpp + 7) / 8;
            unsigned ink = 0;
            for (unsigned n = 0; n < count; ++n) ink |= pixels[n];
            if (!ink) { fprintf(stderr, "BLANK U+%04X\\n", (unsigned)cp); return 3; }
        }
        ++checked;
    }
    printf("PASS glyphs=%u fallback=%u placeholder=0 blank_printable=0\\n", checked, fallback);
    return 0;
}
'''
    with tempfile.TemporaryDirectory(prefix="tirtc-font-verify-") as temporary:
        directory = Path(temporary).resolve()
        if not directory.is_relative_to(Path(tempfile.gettempdir()).resolve()):
            raise ValueError("Unexpected font check temporary directory")
        source = directory / "verify.c"
        source.write_text(c, encoding="utf-8")
        executable = directory / "verify.exe"
        files = [source, UI_ROOT / "font/tirtc_font_14.c", *sorted((LVGL_ROOT / "src").rglob("*.c"))]
        options = ["-std=c99", "-DLV_CONF_INCLUDE_SIMPLE", "-I" + str(UI_ROOT), "-I" + str(LVGL_ROOT),
                   *map(str, files), "-o", str(executable)]
        response = directory / "compile.rsp"
        response.write_text("\n".join('"' + arg.replace("\\", "/") + '"' for arg in options), encoding="utf-8")
        compile_result = subprocess.run([str(host_compiler()), "@" + str(response)],
                                        capture_output=True, text=True, timeout=120)
        if compile_result.returncode:
            raise RuntimeError(compile_result.stdout + compile_result.stderr)
        run = subprocess.run([str(executable)], capture_output=True, text=True, timeout=60)
        if run.returncode:
            raise RuntimeError(run.stdout + run.stderr)
        return run.stdout.strip()


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--extra-text", type=Path, action="append", default=[])
    parser.add_argument("--strict", action="store_true", help="Missing glyphs always fail; retained for explicit validation workflows")
    parser.add_argument("--verify-lvgl", action="store_true", help="Query every glyph through the project's real LVGL on Windows")
    args = parser.parse_args()
    report = {"upstream_commit": COMMIT, "generation": "font only; packaged glyph sources; no installed font dependency"}
    report.update(build_font(args))
    # Parse the generated output independently and validate every cmap entry,
    # descriptor and byte span before relying on its manifest.
    bitmap, glyphs, mapping = parse_font((UI_ROOT / "font/tirtc_font_14.c").read_text(encoding="utf-8"))
    assert len(mapping) == report["font_glyphs"]
    assert len(gb2312_hanzi() & mapping.keys()) == 6763
    assert caption_characters() <= mapping.keys()
    for cp, index in mapping.items():
        assert 0 < index < len(glyphs), f"Bad glyph index U+{cp:04X}"
        glyph = glyphs[index]
        count = (glyph["box_w"] * glyph["box_h"] * 4 + 7) // 8
        assert 0 <= glyph["bitmap_index"] <= len(bitmap) - count, f"Bad bitmap span U+{cp:04X}"
    quote_info = {}
    for cp in (0x2018, 0x2019, 0x201C, 0x201D):
        glyph = glyphs[mapping[cp]]
        start = glyph["bitmap_index"]
        count = (glyph["box_w"] * glyph["box_h"] * 4 + 7) // 8
        quote_info[f"U+{cp:04X}"] = {**glyph, "bitmap_sha256": hashlib.sha256(bitmap[start:start+count]).hexdigest()}
    report["quote_glyphs"] = quote_info
    report["generated_font_sha256"] = hashlib.sha256((UI_ROOT / "font/tirtc_font_14.c").read_bytes()).hexdigest()
    report["table_validation"] = "PASS: every glyph index/bitmap span; all 6763 GB2312 Hanzi; curly single/double quotes"
    if args.verify_lvgl:
        report["lvgl_validation"] = verify_lvgl(report)
    write(UI_ROOT / "font/generation_report.json", json.dumps(report, ensure_ascii=False, indent=2) + "\n")
    print("FONT", report["font_glyphs"], "GB2312", report["font_gb2312_hanzi_included"],
          "BITMAP_BYTES", report["font_bitmap_bytes"], "MISSING", len(report["missing_characters"]))
    print(report.get("lvgl_validation", report["table_validation"]))
    print("SHA256", report["generated_font_sha256"])
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
