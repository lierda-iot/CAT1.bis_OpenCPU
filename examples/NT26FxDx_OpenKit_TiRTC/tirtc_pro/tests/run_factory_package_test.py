"""Validate the F6D_D factory package boundary without building/flashing firmware."""

from copy import deepcopy
from pathlib import Path
import struct
import sys
import tempfile
import unittest
from unittest.mock import patch

sys.dont_write_bytecode = True
sys.path.insert(0, str(Path(__file__).resolve().parents[1] / "resource_store"))
from package_factory import FIRMWARE_LAYOUT, digest, publish_package, read_package, validate_firmware_layout


def make_package() -> bytes:
    header = bytearray(52)
    header[:12] = b"111\0\0\0\0\0bpkg"
    struct.pack_into("<I", header, 16, 4)
    header[20:26] = b"SHA256"
    data = header
    for index, (address, capacity) in enumerate(FIRMWARE_LAYOUT):
        payload = f"firmware-{index}".encode("ascii")
        image = bytearray(364)
        name = f"image{index}.bin".encode("ascii")
        image[:len(name)] = name
        struct.pack_into("<4I", image, 64, address, capacity, len(data), len(payload))
        image[80:144] = digest(payload).encode("ascii")
        data += image + payload
    return bytes(data)


class FactoryPackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="f6dd-package-test-")
        self.addCleanup(self.temporary.cleanup)
        self.package = Path(self.temporary.name) / "normal.binpkg"
        self.package.write_bytes(make_package())
        self.images = read_package(self.package)

    def test_accepts_d_pro_layout_and_preserves_internal_fs_boundary(self):
        validate_firmware_layout(self.images)
        app = self.images[3]
        self.assertEqual(app["address"] - 0x800000 + app["capacity"], 0x3BC000)
        self.assertEqual(0x3E4000 - 0x3BC000, 160 * 1024)

    def test_rejects_stale_a_application_layout(self):
        images = deepcopy(self.images)
        images[3].update(address=0xA56000, capacity=0x166000)
        with self.assertRaisesRegex(ValueError, "rebuild"):
            validate_firmware_layout(images)

    def test_rejects_default_d_layout_without_pro_expansion(self):
        images = deepcopy(self.images)
        images[3]["capacity"] = 0xCB000
        with self.assertRaises(ValueError):
            validate_firmware_layout(images)

    def test_rejects_old_expansion_that_overlaps_internal_fs(self):
        images = deepcopy(self.images)
        images[3]["capacity"] = 0x166000
        with self.assertRaises(ValueError):
            validate_firmware_layout(images)

    def test_rejects_wrong_base_layout(self):
        images = deepcopy(self.images)
        images[1]["capacity"] -= 0x10000
        with self.assertRaises(ValueError):
            validate_firmware_layout(images)

    def test_rejects_oversize_or_extra_images(self):
        images = deepcopy(self.images)
        images[3]["size"] = images[3]["capacity"] + 1
        with self.assertRaises(ValueError):
            validate_firmware_layout(images)
        with self.assertRaises(ValueError):
            validate_firmware_layout(self.images + self.images[:1])

    def test_rejects_corrupt_payload_and_trailing_bytes(self):
        corrupted = bytearray(make_package())
        corrupted[-1] ^= 1
        self.package.write_bytes(corrupted)
        with self.assertRaisesRegex(ValueError, "SHA256"):
            read_package(self.package)
        self.package.write_bytes(make_package() + b"trailing")
        with self.assertRaisesRegex(ValueError, "Unexpected bytes"):
            read_package(self.package)


class PublishPackageTest(unittest.TestCase):
    def setUp(self) -> None:
        self.temporary = tempfile.TemporaryDirectory(prefix="f6dd-publish-test-")
        self.addCleanup(self.temporary.cleanup)
        self.release = Path(self.temporary.name) / "release"
        self.release.mkdir()
        self.old_name = "2026_10_11_21_26_44_pro.binpkg"
        self.new_name = "2026_10_12_10_00_00_pro.binpkg"
        self.old_package = make_package()
        changed = bytearray(self.old_package)
        changed[12] = 1  # fcelf can change container metadata without changing images.
        self.new_package = bytes(changed)
        (self.release / self.old_name).write_bytes(self.old_package)
        self.old_sums = f"{digest(self.old_package)}  {self.old_name}\n".encode("ascii")
        (self.release / "SHA256SUMS.txt").write_bytes(self.old_sums)

    def assert_old_release(self) -> None:
        self.assertEqual((self.release / self.old_name).read_bytes(), self.old_package)
        self.assertEqual((self.release / "SHA256SUMS.txt").read_bytes(), self.old_sums)
        self.assertEqual(sorted(path.name for path in self.release.iterdir()),
                         sorted([self.old_name, "SHA256SUMS.txt"]))

    def fail_checksum_replace(self, path, target):
        if Path(target) == self.release / "SHA256SUMS.txt":
            raise PermissionError("checksum file is locked")
        return self.original_replace(path, target)

    def test_same_name_repack_updates_package_and_checksum(self):
        publish_package(self.release, self.old_name, self.new_package)
        self.assertEqual((self.release / self.old_name).read_bytes(), self.new_package)
        self.assertEqual((self.release / "SHA256SUMS.txt").read_text(),
                         f"{digest(self.new_package)}  {self.old_name}\n")
        self.assertFalse(list(self.release.glob("*.tmp")))

    def test_new_name_success_keeps_previous_package_until_caller_archives(self):
        publish_package(self.release, self.new_name, self.new_package)
        self.assertEqual((self.release / self.old_name).read_bytes(), self.old_package)
        self.assertEqual((self.release / self.new_name).read_bytes(), self.new_package)
        self.assertEqual((self.release / "SHA256SUMS.txt").read_text(),
                         f"{digest(self.new_package)}  {self.new_name}\n")
        self.assertFalse(list(self.release.glob("*.tmp")))

    def test_same_name_checksum_failure_restores_previous_package(self):
        self.original_replace = Path.replace
        with patch.object(Path, "replace", lambda path, target: self.fail_checksum_replace(path, target)):
            with self.assertRaises(PermissionError):
                publish_package(self.release, self.old_name, self.new_package)
        self.assert_old_release()

    def test_new_name_checksum_failure_removes_unlisted_package(self):
        self.original_replace = Path.replace
        with patch.object(Path, "replace", lambda path, target: self.fail_checksum_replace(path, target)):
            with self.assertRaises(PermissionError):
                publish_package(self.release, self.new_name, self.new_package)
        self.assert_old_release()

    def test_first_publication_checksum_failure_leaves_no_package(self):
        empty = self.release / "first"
        original_replace = Path.replace

        def replace(path, target):
            if Path(target) == empty / "SHA256SUMS.txt":
                raise PermissionError("checksum file is locked")
            return original_replace(path, target)

        with patch.object(Path, "replace", replace):
            with self.assertRaises(PermissionError):
                publish_package(empty, self.new_name, self.new_package)
        self.assertEqual(list(empty.iterdir()), [])

    def test_package_replace_failure_preserves_previous_release(self):
        original_replace = Path.replace

        def replace(path, target):
            if Path(target) == self.release / self.old_name:
                raise PermissionError("package file is locked")
            return original_replace(path, target)

        with patch.object(Path, "replace", replace):
            with self.assertRaises(PermissionError):
                publish_package(self.release, self.old_name, self.new_package)
        self.assert_old_release()

    def test_checksum_staging_failure_does_not_publish_package(self):
        import package_factory
        original_temporary_file = package_factory.tempfile.NamedTemporaryFile
        created = 0

        def temporary_file(*args, **kwargs):
            nonlocal created
            created += 1
            if created == 2:
                raise PermissionError("cannot stage checksum")
            return original_temporary_file(*args, **kwargs)

        with patch.object(package_factory.tempfile, "NamedTemporaryFile", temporary_file):
            with self.assertRaises(PermissionError):
                publish_package(self.release, self.old_name, self.new_package)
        self.assert_old_release()

    def test_rollback_failure_retains_recoverable_previous_package(self):
        original_replace = Path.replace
        package_replaces = 0

        def replace(path, target):
            nonlocal package_replaces
            if Path(target) == self.release / "SHA256SUMS.txt":
                raise PermissionError("checksum file is locked")
            if Path(target) == self.release / self.old_name:
                package_replaces += 1
                if package_replaces == 2:
                    raise PermissionError("package rollback is locked")
            return original_replace(path, target)

        with patch.object(Path, "replace", replace):
            with self.assertRaisesRegex(OSError, "recovery package"):
                publish_package(self.release, self.old_name, self.new_package)
        backups = list(self.release.glob("*.tmp"))
        self.assertEqual(len(backups), 1)
        self.assertEqual(backups[0].read_bytes(), self.old_package)
        self.assertEqual((self.release / "SHA256SUMS.txt").read_bytes(), self.old_sums)


if __name__ == "__main__":
    unittest.main(verbosity=2)
