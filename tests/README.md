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

`test_image_select.py` compiles the real `image_select.cpp` with small dependency
fakes. It checks one-time action registration, widget isolation, allow-list
rejection, failed selection, repeated selection, local calls, and synchronous
Select callbacks without stale text status. The controller fake deliberately
does not simulate actual SD storage or LVGL rendering.

`test_hidden_image_config.py` runs real ESPHome validation and source generation
against temporary configurations with dummy credentials. It covers old and new
schemas, mixed modes, existing API actions, generated ID/name conflicts, and
builds without API/display support. Use `.venv/bin/python -m unittest discover
-s tests -v` when ESPHome is installed in the virtual environment. These tests
are skipped when the selected Python has no ESPHome installation.

Validation recorded on 2026-10-10 with ESPHome 2026.8.0 / ESP-IDF 5.5.5:

- All 16 host/configuration/code-generation tests passed.
- Full firmware builds passed for `examples/fnk0104s-esp-idf.yaml` and
  `examples/fnk0104s-hidden-images.yaml`.
- The maintainer reported a successful physical-device test of the hidden image
  Select and its HA image action on 2026-10-10. See
  `docs/HARDWARE_TEST_MATRIX.md` for the remaining hardware checks.

`scripts/hardware_smoke_test.py` is intentionally excluded from this suite. It
requires explicit device access and writes only a random `.cyd-smoke-*.bin` file,
then downloads and removes that same file.
