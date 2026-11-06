"""Lossless font resources and matching generated-C external storage wrappers.

FONT_META wire format is LE32(index[19:0], advance[31:20]), u8 width/height,
i8 x/y for each glyph (including glyph zero), followed by LE16 cmap offsets.
It is explicitly decoded into native LVGL fields; C bitfield layout is not a
file format. Original arrays stay available in non-external host builds.
"""
from pathlib import Path
import re
import struct

FIELDS = ("bitmap_index", "adv_w", "box_w", "box_h", "ofs_x", "ofs_y")


def body(source: str, name: str) -> str:
    match = re.search(r"\b" + name + r"\s*\[\s*\]\s*=\s*\{(.*?)\};", source, re.S)
    if not match:
        raise ValueError("Missing original font array: " + name)
    return re.sub(r"/\*.*?\*/|//[^\n]*", "", match[1], flags=re.S)


def numbers(source: str, name: str) -> list[int]:
    return [int(v, 0) for v in re.findall(r"-?0x[0-9a-fA-F]+|-?\d+", body(source, name))]


def descriptors(source: str) -> list[dict[str, int]]:
    result = []
    for row in re.findall(r"\{([^{}]+)\}", body(source, "glyph_dsc")):
        values = {k: int(v) for k, v in re.findall(r"\.(\w+)\s*=\s*(-?\d+)", row)}
        if set(values) != set(FIELDS):
            raise ValueError("Unexpected glyph descriptor fields")
        result.append(values)
    return result


def metadata(source: str) -> bytes:
    glyphs = descriptors(source)
    offsets = numbers(source, "unicode_list_0")
    bitmap = numbers(source, "glyph_bitmap")
    cmaps = body(source, "cmaps")
    if len(re.findall(r"\.range_start", cmaps)) != 1:
        raise ValueError("External metadata currently requires one sparse cmap")
    length = int(re.search(r"\.range_length\s*=\s*(\d+)", cmaps)[1])
    if not glyphs or any(glyphs[0].values()) or len(glyphs) != len(offsets) + 1:
        raise ValueError("Invalid dummy glyph/count")
    if offsets != sorted(set(offsets)) or not offsets or offsets[-1] >= length:
        raise ValueError("Invalid sparse Unicode offsets")
    data, end = bytearray(), 0
    for glyph in glyphs:
        index, advance, width, height, x, y = (glyph[k] for k in FIELDS)
        if (index != end or not 0 <= index < (1 << 20) or not 0 <= advance < (1 << 12)
                or not 0 <= width <= 255 or not 0 <= height <= 255
                or not -128 <= x <= 127 or not -128 <= y <= 127):
            raise ValueError("Glyph cannot be represented by FONT_META wire format")
        end = index + (width * height + 1) // 2
        if end > len(bitmap):
            raise ValueError("Glyph bitmap exceeds original pixels")
        data.extend(struct.pack("<IBBbb", index | (advance << 20), width, height, x, y))
    if end != len(bitmap):
        raise ValueError("Glyph bitmap length mismatch")
    data.extend(struct.pack("<" + "H" * len(offsets), *offsets))
    return bytes(data)


def resources(app: Path) -> list[tuple[str, str, str, bytes]]:
    font = (app / "ui/font/tirtc_font_14.c").read_text(encoding="utf-8")
    digits = (app / "ui/font/tirtc_font_digits_48.c").read_text(encoding="utf-8")
    return [("FONT_META", "font_meta", "ui14-meta.bin", metadata(font)),
            ("FONT_DIGITS", "font_digits", "ui48-digits.bin", bytes(numbers(digits, "glyph_bitmap")))]


def externalize_chinese(source: str) -> str:
    """Also used by prepare_font.py so regeneration preserves the loader."""
    if "TIRTC_FONT_META_WIRE" in source:
        return source
    glyph_count = len(descriptors(source))
    unicode_count = len(numbers(source, "unicode_list_0"))
    wire_bytes = len(metadata(source))
    start = source.index("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[]")
    end = source.index("static const lv_font_fmt_txt_cmap_t cmaps[]")
    storage = f'''#define TIRTC_GLYPH_DSC glyph_dsc
#else
#include "../../resource_store/asset_io.h"
#include "../../resource_store/asset_manifest.h"
#include <stddef.h>
#include <string.h>
/* TIRTC_FONT_META_WIRE: fixed BSS, no Cust heap and no duplicate raw buffer. */
typedef struct {{
    lv_font_fmt_txt_glyph_dsc_t glyphs[{glyph_count}];
    uint16_t unicode[{unicode_count}];
}} tirtc_font_meta_storage_t;
static tirtc_font_meta_storage_t font_meta;
static bool font_meta_ready;
typedef char font_meta_glyph_size[(sizeof(lv_font_fmt_txt_glyph_dsc_t) == 8U && LV_FONT_FMT_TXT_LARGE == 0) ? 1 : -1];
typedef char font_meta_unicode_offset[(offsetof(tirtc_font_meta_storage_t, unicode) == {glyph_count * 8}U) ? 1 : -1];
typedef char font_meta_wire_size[(offsetof(tirtc_font_meta_storage_t, unicode) + sizeof(font_meta.unicode) == {wire_bytes}U && TIRTC_FONT_META_BYTES == {wire_bytes}U) ? 1 : -1];
#define TIRTC_GLYPH_DSC font_meta.glyphs
#define unicode_list_0 font_meta.unicode
#endif
'''
    source = source[:start] + "#ifndef TIRTC_EXTERNAL_UI_ASSETS\n" + source[start:end] + storage + source[end:]
    marker = "#ifdef TIRTC_EXTERNAL_UI_ASSETS\nstatic tirtc_font_bitmap_reader_t external_bitmap_reader;"
    # Existing sources may have CRLF; only inserted/changed lines use LF.
    actual = marker if marker in source else marker.replace("\n", "\r\n")
    loader = f'''#ifdef TIRTC_EXTERNAL_UI_ASSETS
/* Resource worker only, before fonts are exposed to the LVGL owner. */
int tirtc_font_load_external(void (*progress)(uint32_t))
{{
    uint8_t *raw = (uint8_t *)&font_meta;
    uint32_t expected_offset = 0;
    font_meta_ready = false;
    memset(&cache, 0, sizeof(cache));
    int result = tirtc_asset_read_verified("/ui14-meta.bin", raw,
        TIRTC_FONT_META_BYTES, TIRTC_FONT_META_CRC32, progress);
    if (result) return result;
    for (unsigned i = 0; i < {glyph_count}U; ++i) {{
        const uint8_t *p = raw + i * 8U;
        uint32_t word = (uint32_t)p[0] | ((uint32_t)p[1] << 8) |
                        ((uint32_t)p[2] << 16) | ((uint32_t)p[3] << 24);
        lv_font_fmt_txt_glyph_dsc_t value = {{0}};
        value.bitmap_index = word & 0xfffffU;
        value.adv_w = word >> 20;
        value.box_w = p[4]; value.box_h = p[5];
        value.ofs_x = p[6] < 128U ? (int)p[6] : (int)p[6] - 256;
        value.ofs_y = p[7] < 128U ? (int)p[7] : (int)p[7] - 256;
        uint32_t bytes = ((uint32_t)value.box_w * value.box_h + 1U) / 2U;
        if (value.bitmap_index != expected_offset || expected_offset > TIRTC_FONT_BYTES ||
            bytes > TIRTC_FONT_BYTES - expected_offset ||
            (!i && (word || value.box_w || value.box_h || value.ofs_x || value.ofs_y)))
            return TIRTC_ASSET_E_MISMATCH;
        expected_offset += bytes;
        TIRTC_GLYPH_DSC[i] = value;
    }}
    if (expected_offset != TIRTC_FONT_BYTES) return TIRTC_ASSET_E_MISMATCH;
    for (unsigned i = 0; i < {unicode_count}U; ++i) {{
        const uint8_t *p = raw + {glyph_count * 8}U + i * 2U;
        uint16_t value = (uint16_t)((uint16_t)p[0] | ((uint16_t)p[1] << 8));
        if (value >= cmaps[0].range_length || (i && value <= unicode_list_0[i - 1U]))
            return TIRTC_ASSET_E_MISMATCH;
        unicode_list_0[i] = value;
    }}
    font_meta_ready = true;
    return 0;
}}
static bool tirtc_font_get_dsc_external(const lv_font_t *font,
    lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t next)
{{
    return font_meta_ready && lv_font_get_glyph_dsc_fmt_txt(font, dsc, letter, next);
}}
static tirtc_font_bitmap_reader_t external_bitmap_reader;'''
    if actual not in source:
        raise ValueError("Missing external bitmap reader insertion point")
    source = source.replace(actual, loader, 1)
    source = source.replace("    static const uint8_t empty_bitmap = 0;", "    static const uint8_t empty_bitmap = 0;\n    if (!font_meta_ready) return NULL;", 1)
    source = source.replace(".glyph_dsc = glyph_dsc,", ".glyph_dsc = TIRTC_GLYPH_DSC,", 1)
    source = source.replace("&glyph_dsc[cmap->glyph_id_start + low]", "&TIRTC_GLYPH_DSC[cmap->glyph_id_start + low]", 1)
    source = source.replace("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,", "#ifdef TIRTC_EXTERNAL_UI_ASSETS\n    .get_glyph_dsc = tirtc_font_get_dsc_external,\n#else\n    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,\n#endif", 1)
    source = source.replace("if (!reader || length", "if (!font_meta_ready || !reader || length", 1)
    source = source.replace("if (!bitmap || length", "if (!font_meta_ready || !bitmap || length", 1)
    source = re.sub(r"/\* Original descriptors/metrics/fallback remain LVGL's\. Only bitmap delivery\r?\n \* changes; the single software-render owner consumes each pointer synchronously\. \*/",
                    "/* Native descriptors and cached bitmaps preserve the original LVGL metrics.\n * The single software-render owner consumes each pointer synchronously. */", source)
    return source


def externalize_digits(source: str) -> str:
    """Keep metrics and kerning internal; only the unchanged pixels move."""
    if "TIRTC_FONT_DIGITS_WIRE" in source:
        return source
    count = len(numbers(source, "glyph_bitmap"))
    start = source.index("static const uint8_t glyph_bitmap[]")
    end = source.index("static const lv_font_fmt_txt_glyph_dsc_t glyph_dsc[]")
    storage = f'''#define TIRTC_DIGIT_BITMAP glyph_bitmap
#else
#include "../../resource_store/asset_io.h"
#include "../../resource_store/asset_manifest.h"
/* TIRTC_FONT_DIGITS_WIRE: aligned BSS, not a dynamic allocation. */
static union {{ uint32_t align; uint8_t bytes[{count}]; }} digit_pixels;
static bool digit_pixels_ready;
typedef char font_digits_wire_size[(TIRTC_FONT_DIGITS_BYTES == {count}U) ? 1 : -1];
#define TIRTC_DIGIT_BITMAP digit_pixels.bytes
int tirtc_font_digits_load_external(void (*progress)(uint32_t))
{{
    digit_pixels_ready = false;
    int result = tirtc_asset_read_verified("/ui48-digits.bin", digit_pixels.bytes,
        TIRTC_FONT_DIGITS_BYTES, TIRTC_FONT_DIGITS_CRC32, progress);
    if (!result) digit_pixels_ready = true;
    return result;
}}
static bool tirtc_digits_get_dsc_external(const lv_font_t *font,
    lv_font_glyph_dsc_t *dsc, uint32_t letter, uint32_t next)
{{
    return digit_pixels_ready && lv_font_get_glyph_dsc_fmt_txt(font, dsc, letter, next);
}}
static const uint8_t *tirtc_digits_get_bitmap_external(const lv_font_t *font, uint32_t letter)
{{
    return digit_pixels_ready ? lv_font_get_bitmap_fmt_txt(font, letter) : NULL;
}}
#endif
'''
    source = source[:start] + "#ifndef TIRTC_EXTERNAL_UI_ASSETS\n" + source[start:end] + storage + source[end:]
    source = source.replace(".glyph_bitmap = glyph_bitmap,", ".glyph_bitmap = TIRTC_DIGIT_BITMAP,", 1)
    source = source.replace("    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,", "#ifdef TIRTC_EXTERNAL_UI_ASSETS\n    .get_glyph_dsc = tirtc_digits_get_dsc_external,\n    .get_glyph_bitmap = tirtc_digits_get_bitmap_external,\n#else\n    .get_glyph_dsc = lv_font_get_glyph_dsc_fmt_txt,", 1)
    source = source.replace("    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,", "    .get_glyph_bitmap = lv_font_get_bitmap_fmt_txt,\n#endif", 1)
    return source
