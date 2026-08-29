# Tests

Run the host-side path tests without hardware:

```bash
python -m unittest discover -s tests -v
```

`test_path_rules.py` compiles the exact C++ path implementation used by the
firmware and exercises normalization, traversal rejection, control characters,
reserved upload names, path limits and percent decoding.

`test_component_syntax.py` runs every component translation unit through g++
with a deliberately small ESP-IDF/ESPHome declaration shim. It catches C++
syntax, type and warning regressions when the complete ESP-IDF toolchain is not
installed; it does not replace the real ESPHome firmware build.

`scripts/hardware_smoke_test.py` is intentionally excluded from this suite. It
requires explicit device access and writes only a random `.cyd-smoke-*.bin` file,
then downloads and removes that same file.
