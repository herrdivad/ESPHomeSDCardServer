"""Run the real image Select/API adapter with small host-side dependency fakes."""

from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ImageSelectTest(unittest.TestCase):
    def test_hidden_action_and_status(self):
        with tempfile.TemporaryDirectory(prefix="cyd-select-test-") as directory:
            executable = Path(directory) / "image_select_test"
            subprocess.run([
                "g++", "-std=c++17", "-Wall", "-Wextra", "-Werror",
                "-DUSE_CYD_SD_LVGL", "-DUSE_CYD_SD_HIDDEN_SELECT",
                "-I", str(ROOT / "tests/stubs"),
                "-I", str(ROOT / "components/cyd_sd_http_server"),
                str(ROOT / "tests/image_select_test.cpp"),
                str(ROOT / "components/cyd_sd_http_server/image_select.cpp"),
                "-o", str(executable),
            ], check=True)
            subprocess.run([str(executable)], check=True)
