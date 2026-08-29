from pathlib import Path
import subprocess
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


class PathRulesTest(unittest.TestCase):
    def test_native_path_rules(self):
        with tempfile.TemporaryDirectory(prefix="cyd-path-test-") as directory:
            executable = Path(directory) / "path_utils_test"
            subprocess.run(
                [
                    "g++",
                    "-std=c++17",
                    "-Wall",
                    "-Wextra",
                    "-Werror",
                    "-I",
                    str(ROOT / "components/cyd_sd_http_server"),
                    str(ROOT / "components/cyd_sd_http_server/path_utils.cpp"),
                    str(ROOT / "tests/path_utils_test.cpp"),
                    "-o",
                    str(executable),
                ],
                check=True,
            )
            subprocess.run([str(executable)], check=True)


if __name__ == "__main__":
    unittest.main()

