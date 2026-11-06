"""Package the unchanged pro firmware plus its SPI1 factory filesystem.

Run after the pro build_app.bat. This Windows host tool never flashes hardware.
The published package overwrites the external 4 MiB filesystem when flashed.
"""

from __future__ import annotations

import argparse
from datetime import datetime, timedelta, timezone
import hashlib
import json
from pathlib import Path
import re
import shutil
import struct
import subprocess
import sys
import tempfile

# Keep host build artifacts out of the application source directory.
sys.dont_write_bytecode = True
from build_factory_image import DEFAULT_REPO, IMAGE_SIZE, build_image


MODEM_PACKAGE = "F6D_D"
# SDK 1.7.1 F6D_D layout with CODE_REGION_ADJUST=0x5F000. The application
# ends at physical 0x3BC000, preserving the existing 160 KiB internal FS.
# Reject packages with a different layout before publishing factory assets.
FIRMWARE_LAYOUT = (
    (0x00803000, 0x16000),   # Bootloader
    (0x008BE000, 0x1BC000),  # AP
    (0x0081E000, 0xA0000),   # CP
    (0x00A92000, 0x12A000),  # Pro application, 1192 KiB
)


def validate_firmware_layout(images: list[dict]) -> None:
    if len(images) != len(FIRMWARE_LAYOUT):
        raise ValueError("Build the normal four-image F6D_D pro package first")
    for item, (address, capacity) in zip(images, FIRMWARE_LAYOUT):
        if (item["address"], item["capacity"]) != (address, capacity):
            raise ValueError("Normal package does not match the F6D_D pro layout; rebuild it")
        if not 0 < item["size"] <= capacity:
            raise ValueError("Normal firmware image exceeds its F6D_D partition")


def digest(data: bytes) -> str:
    return hashlib.sha256(data).hexdigest()


def read_package(path: Path) -> list[dict]:
    """Check the bundled fcelf format, every payload hash and exact EOF.

    Only this SDK's unsigned SHA256 binpkg layout is accepted. No payload is
    executed or extracted using names supplied by the package.
    """
    data = path.read_bytes()
    if len(data) < 52 or data[:12] != b"111\0\0\0\0\0bpkg" or data[20:26] != b"SHA256":
        raise ValueError(f"Unsupported or incomplete package: {path}")
    count = struct.unpack_from("<I", data, 16)[0]
    offset = 52 + struct.unpack_from("<H", data, 30)[0]
    if not 1 <= count <= 16:
        raise ValueError("Unexpected package image count")
    images = []
    for _ in range(count):
        if offset + 364 > len(data):
            raise ValueError("Truncated package image header")
        address, capacity, position, length = struct.unpack_from("<4I", data, offset + 64)
        if position != offset or length > capacity or length == 0:
            raise ValueError("Invalid package image bounds")
        end = offset + 364 + length
        payload = data[offset + 364:end]
        expected_hash = data[offset + 80:offset + 144].decode("ascii").lower()
        if end > len(data) or digest(payload) != expected_hash:
            raise ValueError("Package image SHA256 mismatch")
        images.append({
            "name": data[offset:offset + 64].split(b"\0", 1)[0].decode("ascii"),
            "type": data[offset + 336:offset + 364].split(b"\0", 1)[0].decode("ascii"),
            "address": address, "capacity": capacity,
            "size": length, "sha256": expected_hash,
        })
        offset = end
    if offset != len(data):
        raise ValueError("Unexpected bytes after final package image")
    return images


def publish_package(release: Path, package_name: str, package_bytes: bytes) -> None:
    """Keep the previous package/checksum pair if checksum publication fails."""
    release.mkdir(parents=True, exist_ok=True)
    published = release / package_name
    checksum = release / "SHA256SUMS.txt"
    temporary_paths: list[Path] = []
    backup = None

    def stage(content: bytes) -> Path:
        with tempfile.NamedTemporaryFile(prefix=".pro-", suffix=".tmp", dir=release,
                                         delete=False) as stream:
            path = Path(stream.name)
            temporary_paths.append(path)
            stream.write(content)
        return path

    try:
        expected_hash = digest(package_bytes)
        staged = stage(package_bytes)
        if digest(staged.read_bytes()) != expected_hash:
            raise ValueError("Published package SHA256 mismatch")
        staged_sums = stage(f"{expected_hash}  {package_name}\n".encode("ascii"))
        if published.exists():
            backup = stage(published.read_bytes())
        staged.replace(published)
        try:
            staged_sums.replace(checksum)
        except OSError:
            try:
                if backup is None:
                    published.unlink()
                else:
                    backup.replace(published)
            except OSError as restore_error:
                # Retain the recoverable old bytes when the OS also blocks rollback.
                if backup is not None:
                    temporary_paths.remove(backup)
                recovery = backup if backup is not None else published
                raise OSError(f"Checksum publication and rollback failed; recovery package: {recovery}") from restore_error
            raise
    finally:
        for path in temporary_paths:
            path.unlink(missing_ok=True)


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--repo", type=Path, default=DEFAULT_REPO)
    parser.add_argument("--app", type=Path, default=Path(__file__).resolve().parent.parent)
    parser.add_argument("--build-dir", type=Path)
    parser.add_argument("--normal-package", type=Path)
    parser.add_argument("--app-binary", type=Path)
    parser.add_argument("--output-package", type=Path)
    parser.add_argument("--fcelf", type=Path)
    parser.add_argument("--lfsutil", type=Path)
    args = parser.parse_args()
    repo = args.repo.resolve()
    app = args.app.resolve()
    build = (args.build_dir or repo / "gccout/tirtc_pro").resolve()
    output = (build / "factory" if args.build_dir else repo / "gccout/tirtc_pro_factory")
    release = repo / "firmware/pro"
    match = re.search(r"^APP_VERSION\s*:?=\s*(\w+)\s*$", (app / "config").read_text(), re.M)
    if not match:
        raise ValueError("Missing pro APP_VERSION")
    normal_package = args.normal_package or build / f"TiRTC_pro_NT26F6D0_{match.group(1)}.binpkg"
    normal_package = normal_package.resolve()
    normal_images = read_package(normal_package)
    validate_firmware_layout(normal_images)
    base = repo / "components/basePkg" / MODEM_PACKAGE
    product_match = re.search(r"^#define\s+PKG_PRODUCT\s+(\w+)\s*$",
                              (base / "mem_map.txt").read_text(), re.M)
    if not product_match or product_match.group(1) != "EC718PM_PRD":
        raise ValueError("Unexpected base package for this pro board")
    sources = [
        (base / "ap_bootloader.bin", "BL_PKGIMG_LNA", "BOOTLOADER_PKGIMG_LIMIT_SIZE"),
        (base / "ap_lierda_app.bin", "AP_PKGIMG_LNA", "AP_PKGIMG_LIMIT_SIZE"),
        (base / "cp-demo-flash.bin", "CP_PKGIMG_LNA", "CP_PKGIMG_LIMIT_SIZE"),
        ((args.app_binary or build / "TiRTC_pro_NT26F6D0.bin").resolve(), "PKGFLXAPP_APP0_LNA", "PKGFLXAPP_APP0_SIZE"),
    ]
    for item, (source, _, _) in zip(normal_images, sources):
        if digest(source.read_bytes()) != item["sha256"]:
            raise ValueError(f"Input differs from normal firmware package: {source}")
    # Use the application's actual build time, independent of the host timezone.
    compiled_at = datetime.fromtimestamp(sources[3][0].stat().st_mtime,
                                         timezone(timedelta(hours=8)))
    package_name = compiled_at.strftime("%Y_%m_%d_%H_%M_%S_pro.binpkg")

    output.mkdir(parents=True, exist_ok=True)
    image_path = output / "ext_lfs.bin"
    filesystem = build_image(repo, image_path)
    memory_map = (build / "mem_map.txt").read_text()
    if re.search(r"^#define\s+EFID01_IMG_LFS_", memory_map, re.M):
        raise ValueError("Normal pro build already defines a SPI1 filesystem image")
    # Packaging-only map: no application CFLAGS, linker layout or SDK changes.
    memory_map += "\n#define EFID01_IMG_LFS_LNA (0x80000000)\n#define EFID01_IMG_LFS_SIZE (0x400000)\n"
    sources.append((image_path, "EFID01_IMG_LFS_LNA", "EFID01_IMG_LFS_SIZE"))
    with tempfile.TemporaryDirectory(prefix=".package-", dir=output) as temporary:
        scratch = Path(temporary)
        definition = scratch / "mem_map.txt"
        definition.write_text(memory_map, encoding="ascii", newline="\n")
        package = scratch / "factory.binpkg"
        command = [str(args.fcelf or repo / "tools/fcelf/fcelf.exe"), "-M"]
        for source, address, capacity in sources:
            command += ["-input", str(source), "-addrname", address, "-flashsize", capacity]
        command += ["-pkgmode", "1", "-banoldtool", "1", "-productname", product_match.group(1),
                    "-def", str(definition), "-outfile", str(package)]
        result = subprocess.run(command, capture_output=True, text=True, errors="replace", timeout=120)
        (output / "package.log").write_text(result.stdout + result.stderr, encoding="utf-8", newline="\n")
        if result.returncode:
            raise ValueError("fcelf packaging failed; see gccout/tirtc_pro_factory/package.log")
        images = read_package(package)
        if len(images) != 5 or images[:4] != normal_images:
            raise ValueError("Factory package changed the normal firmware images")
        external = images[4]
        if (external["address"] != 0x80000000 or external["capacity"] != IMAGE_SIZE
                or external["size"] != IMAGE_SIZE or external["sha256"] != filesystem["sha256"]):
            raise ValueError("Factory package external filesystem mismatch")
        # The tool's port-specific image type is checked below after packaging.
        if external["type"] != "EFID01":
            raise ValueError(f"Expected SPI1 external image, got {external['type']!r}")
        if read_package(normal_package) != normal_images:
            raise ValueError("Normal firmware package changed during factory packaging")
        normal_bytes = normal_package.read_bytes()
        package_bytes = package.read_bytes()
        report = {
            "product": "pro", "purpose": "factory initialization",
            "modem_package": MODEM_PACKAGE,
            "app_version": match.group(1),
            "external_flash_files_overwritten": True,
            "compiled_at": compiled_at.isoformat(),
            "package": package_name, "size": len(package_bytes), "sha256": digest(package_bytes),
            "normal_firmware_payloads_unchanged": True, "images": images,
            "maintenance_package": {
                "package": normal_package.relative_to(repo).as_posix(), "size": len(normal_bytes),
                "sha256": digest(normal_bytes), "external_flash_files_overwritten": False,
            },
            "external_filesystem": {key: value for key, value in filesystem.items() if key != "path"},
            "hardware_flash_verified": False,
        }
        # Publish only after the complete package and resource readback pass.
        destination = args.output_package.resolve() if args.output_package else release / package_name
        destination.parent.mkdir(parents=True, exist_ok=True)
        staged_package = destination.with_name(destination.name + ".tmp")
        staged_package.write_bytes(package_bytes)
        staged_package.replace(destination)
        if args.output_package is None:
            publish_package(release, package_name, package_bytes)
            previous = [path for path in release.iterdir() if path.is_file()
                        and path.name != package_name
                        and (re.fullmatch(r"\d{4}(?:_\d{2}){5}_pro\.binpkg", path.name)
                             or path.name in {"latest.binpkg", "maintenance.binpkg", "manifest.json"})]
            if previous:
                archive = Path(tempfile.mkdtemp(prefix="tirtc-pro-previous-firmware-"))
                for path in previous:
                    shutil.copy2(path, archive / path.name)
                    path.unlink()
                print(f"Previous published files saved outside the project: {archive}")
        (output / "manifest.json").write_text(json.dumps(report, ensure_ascii=False, indent=2) + "\n", encoding="utf-8", newline="\n")
        (output / "mem_map.txt").write_text(memory_map, encoding="ascii", newline="\n")
    print(f"Ready: {destination} (firmware + complete UI resources)")
    print("Factory initialization: flashing this package overwrites external Flash files.")


if __name__ == "__main__":
    try:
        main()
    except (OSError, ValueError, subprocess.SubprocessError) as error:
        raise SystemExit(f"Factory packaging failed: {error}")
