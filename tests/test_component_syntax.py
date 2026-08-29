from pathlib import Path
import subprocess
import unittest


ROOT = Path(__file__).resolve().parents[1]


class ComponentSyntaxTest(unittest.TestCase):
    def test_all_component_translation_units(self):
        component = ROOT / "components/cyd_sd_http_server"
        subprocess.run(
            [
                "g++",
                "-std=c++17",
                "-Wall",
                "-Wextra",
                "-Werror",
                "-fsyntax-only",
                "-I",
                str(ROOT / "tests/stubs"),
                "-I",
                str(component),
                str(component / "path_utils.cpp"),
                str(component / "sd_storage.cpp"),
                str(component / "cyd_sd_http_server.cpp"),
            ],
            check=True,
        )


if __name__ == "__main__":
    unittest.main()

