"""Build the pro factory LittleFS image with the bundled SDK utility.

Requires Python 3.9+ and the Windows tools/lfsutil/lfsutil.exe.  The generated
image matches storage/tirtc_storage.c: 4 MiB, 4096-byte blocks, 256-byte
read/program/cache units, and LittleFS disk version 2.0.  This only creates a
host file; it does not access hardware or alter the SDK's example image.
"""

from __future__ import annotations

import argparse
import hashlib
import json
from pathlib import Path
import re
import struct
import subprocess
import sys
import tempfile
import zlib
from asset_catalog import RESOURCE_SPECS


IMAGE_SIZE = 4 * 1024 * 1024
BLOCK_SIZE = 4096
PAGE_SIZE = 256
DISK_VERSION = 0x00020000
RESOURCE_NAMES = tuple(item[2] for item in RESOURCE_SPECS)
DEFAULT_REPO = Path(__file__).resolve().parents[4]
GEOMETRY_ARGS = (
    "-B", str(BLOCK_SIZE), "-R", str(PAGE_SIZE),
    "-P", str(PAGE_SIZE), "-C", str(PAGE_SIZE),
)


def _digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def _paths(repo: Path) -> tuple[Path, Path]:
    tool_name = "lfsutil" if sys.platform != "win32" else "lfsutil.exe"
    tool = repo / "tools/lfsutil" / tool_name
    assets = Path(__file__).resolve().parent / "assets14"
    if not tool.is_file():
        raise ValueError(f"Bundled LittleFS utility is missing: {tool}")
    return tool, assets


def _run(tool: Path, *args: str, cwd: Path) -> str:
    result = subprocess.run(
        [str(tool), *args], check=False, capture_output=True,
        text=True, errors="replace", timeout=120, cwd=cwd,
    )
    if result.returncode:
        raise ValueError(
            f"LittleFS utility failed ({result.returncode}): "
            f"{result.stdout.strip()} {result.stderr.strip()}"
        )
    # A zero exit code alone is insufficient.  Callers independently validate
    # the directory, superblocks, and extracted file contents.
    return result.stdout


def _load_resources(repo: Path, assets: Path) -> list[dict]:
    manifest = json.loads((assets / "manifest.json").read_text(encoding="utf-8"))
    entries = manifest.get("files", [])
    if len(entries) != len(RESOURCE_NAMES):
        raise ValueError("The resource manifest must describe the complete UI resource set")
    by_name = {entry["name"]: entry for entry in entries}
    if set(by_name) != set(RESOURCE_NAMES):
        raise ValueError("The resource manifest must contain all matching Pro UI resources")
    header_path = Path(__file__).resolve().parent / "asset_manifest.h"
    header = header_path.read_text(encoding="utf-8")
    checked = []
    for name in RESOURCE_NAMES:
        data = (assets / name).read_bytes()
        actual = {
            "name": name, "size": len(data),
            "crc32": f"{zlib.crc32(data):08x}", "sha256": _digest(data),
        }
        if any(str(by_name[name].get(key)).lower() != str(actual[key]).lower()
               for key in ("size", "crc32", "sha256")):
            raise ValueError(f"Resource does not match its manifest: {name}")
        prefix = next(key for key, _, filename in RESOURCE_SPECS if filename == name)
        for suffix, expected in (("BYTES", len(data)), ("CRC32", zlib.crc32(data))):
            macro = f"TIRTC_{prefix}_{suffix}"
            match = re.search(r"^#define\s+" + macro + r"\s+(0x[0-9a-fA-F]+|[0-9]+)[Uu]?\s*$",
                              header, re.MULTILINE)
            if not match or int(match.group(1), 0) != expected:
                raise ValueError(f"Resource does not match the application's {macro}")
        checked.append(actual)
    return checked


def _check_geometry(data: bytes) -> None:
    if len(data) != IMAGE_SIZE:
        raise ValueError(f"Expected a {IMAGE_SIZE}-byte image, got {len(data)}")
    # lfsutil 1.2.3.7 creates the root metadata pair in blocks 0 and 1.
    # Its initial commit contains an 8-byte 'littlefs' name, a 4-byte tag,
    # then the six little-endian superblock fields.  This deliberately checks
    # the canonical layout generated here, rather than guessing arbitrary
    # filesystem layouts or repairing malformed SDK images.
    expected = (DISK_VERSION, BLOCK_SIZE, IMAGE_SIZE // BLOCK_SIZE)
    for offset in (0, BLOCK_SIZE):
        if data[offset + 8:offset + 16] != b"littlefs":
            raise ValueError(f"Missing canonical LittleFS superblock at {offset}")
        fields = struct.unpack_from("<6I", data, offset + 20)
        if fields[:3] != expected:
            raise ValueError(
                f"Incompatible LittleFS geometry at {offset}: {fields[:3]}"
            )
        if fields[3:] != (63, 0x7FFFFFFF, 1022):
            raise ValueError(f"Unexpected LittleFS file limits at {offset}: {fields[3:]}")


def _verify(tool: Path, image: Path, resources: list[dict], scratch: Path) -> dict:
    original = image.read_bytes()
    _check_geometry(original)
    listing = _run(tool, "-i", str(image), "-l", "-d", "/", *GEOMETRY_ARGS, cwd=scratch)
    found = {}
    for line in listing.splitlines():
        if not line.strip():
            continue
        match = re.fullmatch(r"(reg|dir)\s+(\d+)B\s+(.+?)\s*", line)
        if not match:
            raise ValueError(f"Unexpected LittleFS directory output: {line}")
        kind, length, name = match.groups()
        if kind == "dir" and name in (".", ".."):
            continue
        if kind != "reg" or name in found:
            raise ValueError(f"Unexpected LittleFS directory entry: {line}")
        found[name] = int(length)
    if found != {item["name"]: item["size"] for item in resources}:
        raise ValueError(f"Unexpected image contents: {found}")
    for item in resources:
        extracted = scratch / (item["name"] + ".readback")
        _run(tool, "-i", str(image), "-l", "-f", "/" + item["name"],
             "-o", str(extracted), *GEOMETRY_ARGS, cwd=scratch)
        data = extracted.read_bytes()
        if len(data) != item["size"] or _digest(data) != item["sha256"]:
            raise ValueError(f"LittleFS readback mismatch: {item['name']}")
    if image.read_bytes() != original:
        raise ValueError("Readback verification unexpectedly changed the image")
    return {
        "size": len(original), "sha256": _digest(original),
        "disk_version": "2.0", "block_size": BLOCK_SIZE,
        "block_count": IMAGE_SIZE // BLOCK_SIZE,
        "read_size": PAGE_SIZE, "prog_size": PAGE_SIZE, "cache_size": PAGE_SIZE,
        "files": resources, "geometry_verified": True, "readback_verified": True,
        "lfsutil_version": _run(tool, "--version", cwd=scratch).strip(),
        "lfsutil_sha256": _digest(tool.read_bytes()),
    }


def build_image(repo: Path, output: Path) -> dict:
    """Create/replace only output, after validating a new image in private temp.

    A relative output path is relative to repo.  The supplied resource files
    and the shared SDK files are never overwritten.  Failure before publication
    leaves an existing output intact.
    """
    repo = Path(repo).resolve()
    output = Path(output)
    output = (output if output.is_absolute() else repo / output).resolve()
    tool, assets = _paths(repo)
    protected = (repo / "components", repo / "tools", repo / "examples", assets)
    if any(output == path or path in output.parents for path in protected):
        raise ValueError("Output must be outside the shared SDK, application source, and input assets")
    if output.suffix.lower() != ".bin":
        raise ValueError("The output image must have a .bin extension")
    resources = _load_resources(repo, assets)
    output.parent.mkdir(parents=True, exist_ok=True)
    with tempfile.TemporaryDirectory(prefix=".pro-lfs-", dir=output.parent) as temporary:
        scratch = Path(temporary)
        image = scratch / "ext_lfs.bin"
        _run(tool, "-i", str(image), "-c", "-S", str(IMAGE_SIZE), *GEOMETRY_ARGS, cwd=scratch)
        for item in resources:
            _run(tool, "-i", str(image), "-w", "-f", "/" + item["name"],
                 "-F", str(assets / item["name"]), *GEOMETRY_ARGS, cwd=scratch)
        report = _verify(tool, image, resources, scratch)
        image.replace(output)
    report["path"] = str(output)
    return report


def verify_image(repo: Path, image: Path) -> dict:
    """Read back an existing generated image without changing its contents."""
    repo = Path(repo).resolve()
    image = Path(image)
    image = (image if image.is_absolute() else repo / image).resolve()
    tool, assets = _paths(repo)
    resources = _load_resources(repo, assets)
    with tempfile.TemporaryDirectory(prefix="pro-lfs-verify-") as temporary:
        report = _verify(tool, image, resources, Path(temporary))
    report["path"] = str(image)
    return report


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=DEFAULT_REPO)
    parser.add_argument("--output", type=Path, default=Path("gccout/tirtc_pro_factory/ext_lfs.bin"))
    parser.add_argument("--verify-only", action="store_true")
    args = parser.parse_args()
    action = verify_image if args.verify_only else build_image
    try:
        report = action(args.repo, args.output)
    except (OSError, ValueError, KeyError, subprocess.SubprocessError) as error:
        parser.exit(1, f"Factory image failed: {error}\n")
    print(json.dumps(report, ensure_ascii=False, indent=2))


if __name__ == "__main__":
    main()
