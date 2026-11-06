"""Extract all unchanged UI resources for the external-Flash factory package.

The original source arrays remain as host-test fixtures behind the production
external-resource switch; this never quantizes a glyph or changes a pixel.
"""
from pathlib import Path
from asset_catalog import RESOURCE_SPECS
from font_resources import resources as font_resources
from visual_resources import resources as visual_resources
import hashlib
import json
import re
import struct
import zlib

APP = Path(__file__).resolve().parents[1]


def array(text, name, width):
    match = re.search(r"\b" + name + r"\s*\[[^\]]*\]\s*=\s*\{(.*?)\};", text, re.S)
    if not match:
        raise ValueError("Missing original bitmap array " + name)
    values = [int(x, 16) for x in re.findall(r"0x([0-9a-fA-F]+)", match[1])]
    return bytes(values) if width == 1 else struct.pack("<" + "H" * len(values), *values)


def main():
    font = array((APP / "ui/font/tirtc_font_14.c").read_text(encoding="utf-8"), "glyph_bitmap", 1)
    background = array((APP / "ui/assets/ui_face_background.c").read_text(encoding="utf-8"), "ui_face_background_pixels", 2)
    assert len(font) == 631201 and len(background) == 320 * 240 * 2
    assert hashlib.sha256(font).hexdigest() == "fbfbc213d8457d56d200937f56bf9b94b0856103de90f45fb2abf71cabe7e398"
    target = APP / "resource_store"
    target.mkdir(parents=True, exist_ok=True)
    header = "#ifndef TIRTC_ASSET_MANIFEST_H\n#define TIRTC_ASSET_MANIFEST_H\n#include <stdint.h>\n"
    source = '/* Original UI resources. Installer build only. */\n#include "asset_manifest.h"\n'
    report = {"format": "unaltered bytes; no compression or quantization", "files": []}
    output = APP / "resource_store/assets14"
    output.mkdir(parents=True, exist_ok=True)
    entries = [("FONT", "font", "ui14-font.bin", font),
               ("BACKGROUND", "background", "ui13-bg.bin", background)]
    entries += font_resources(APP) + visual_resources(APP)
    by_key = {entry[0]: entry for entry in entries}
    if len(entries) != len(RESOURCE_SPECS) or set(by_key) != {x[0] for x in RESOURCE_SPECS}:
        raise ValueError("Incomplete or duplicate UI resource set")
    for key, name, filename in RESOURCE_SPECS:
        actual_key, actual_name, actual_file, value = by_key[key]
        if (actual_key, actual_name, actual_file) != (key, name, filename):
            raise ValueError("Resource identity mismatch: " + key)
        header += f"#define TIRTC_{key}_BYTES {len(value)}U\n#define TIRTC_{key}_CRC32 0x{zlib.crc32(value):08x}U\n"
        header += f"extern const uint8_t tirtc_install_{name}[TIRTC_{key}_BYTES];\n"
        source += f"const uint8_t tirtc_install_{name}[TIRTC_{key}_BYTES] = {{\n"
        source += "\n".join("    " + ",".join(f"0x{x:02x}" for x in value[i:i+24]) + "," for i in range(0, len(value), 24))
        source += "\n};\n"
        (output / filename).write_bytes(value)
        report["files"].append({"name": filename, "size": len(value), "crc32": f"{zlib.crc32(value):08x}", "sha256": hashlib.sha256(value).hexdigest()})
    header += f"#define TIRTC_UI_ASSET_COUNT {len(RESOURCE_SPECS)}U\n"
    header += f"#define TIRTC_UI_ASSET_TOTAL_BYTES {sum(x['size'] for x in report['files'])}U\n"
    header += "/* X(macro prefix, installer symbol suffix, absolute resource path). */\n"
    header += "#define TIRTC_UI_ASSET_LIST(X) " + chr(92) + "\n"
    for index, (key, name, filename) in enumerate(RESOURCE_SPECS):
        tail = " " + chr(92) if index + 1 < len(RESOURCE_SPECS) else ""
        header += f'    X({key}, {name}, "/{filename}"){tail}\n'
    (target / "asset_manifest.h").write_text(header + "#endif\n", encoding="utf-8", newline="\n")
    (target / "installer_blob.c").write_text(source, encoding="utf-8")
    (output / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8")
    print(json.dumps(report, ensure_ascii=False))


if __name__ == "__main__":
    main()
