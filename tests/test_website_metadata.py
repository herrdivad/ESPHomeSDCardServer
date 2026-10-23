"""Privacy regressions for lossless website JPEG metadata removal."""

import importlib.util
from pathlib import Path
import unittest

ROOT = Path(__file__).resolve().parents[1]
SPEC = importlib.util.spec_from_file_location("website_build", ROOT / "scripts/build_website.py")
MODULE = importlib.util.module_from_spec(SPEC)
SPEC.loader.exec_module(MODULE)


def segment(marker, payload):
    return bytes((0xFF, marker)) + (len(payload) + 2).to_bytes(2, "big") + payload


class WebsiteMetadataTest(unittest.TestCase):
    def test_removes_all_application_segments_and_comments(self):
        metadata = b"".join(segment(marker, b"private metadata") for marker in (*range(0xE0, 0xF0), 0xFE))
        image = b"\xff\xd8" + segment(0xDB, b"quantization") + segment(0xDA, b"scan header") + b"pixels\xff\xd9"
        source = image[:2] + metadata + image[2:]
        self.assertEqual(MODULE.public_photo(source), image)

    def test_preserves_stuffed_bytes_restart_markers_and_multiple_scans(self):
        scan = segment(0xDA, b"header") + b"pixels\xff\x00data\xff\xd0restart"
        image = b"\xff\xd8" + scan + scan + b"\xff\xd9"
        source = b"\xff\xd8" + scan + segment(0xE1, b"EXIF GPS") + scan + b"\xff\xd9"
        self.assertEqual(MODULE.public_photo(source), image)

    def test_discards_trailing_data(self):
        image = b"\xff\xd8" + segment(0xDA, b"header") + b"pixels\xff\xd9"
        self.assertEqual(MODULE.public_photo(image + b"private trailer"), image)

    def test_rejects_truncated_images(self):
        for source in (b"not JPEG", b"\xff\xd8\xff\xe1\x00\xffshort", b"\xff\xd8\xff\xda\x00\x02pixels"):
            with self.subTest(source=source), self.assertRaises(ValueError):
                MODULE.public_photo(source)

    def test_repository_photo_is_already_clean(self):
        for name in MODULE.PHOTOS:
            with self.subTest(name=name):
                data = (ROOT / "examples" / name).read_bytes()
                self.assertEqual(MODULE.public_photo(data), data)


if __name__ == "__main__":
    unittest.main()
