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
schemas, per-widget image fit modes, generated RGB565A8 transform support,
mixed modes, existing API actions, automatic PSRAM/cache diagnostic entities,
generated ID/name conflicts, and builds without API/display support. Use
`.venv/bin/python -m unittest discover -s tests -v` when ESPHome is installed
in the virtual environment. These tests are skipped when the selected Python
has no ESPHome installation.

Validation recorded on 2026-10-10 with ESPHome 2026.8.0 / ESP-IDF 5.5.5:

- All 20 host/configuration/code-generation tests passed.
- A full firmware build passed for `examples/fnk0104s-esp-idf.yaml` with the
  RGB565A8 transform configuration fix. Source generation passed for
  `examples/fnk0104s-hidden-images.yaml`; its full build passed before this
  configuration fix and was not rerun afterward.
- The maintainer reported successful physical-device scaling without an
  additional image buffer after flashing the RGB565A8 fix on 2026-10-10.
  The old RGB565 color-format warning no longer appears. Occasional long
  computation warnings of roughly 150-850 ms during new uncached operations
  were already present before this fix. Exact PSRAM figures were not reported.
- The maintainer reported a successful physical-device test of the hidden image
  Select and its HA image action on 2026-10-10. See
  `docs/HARDWARE_TEST_MATRIX.md` for the remaining hardware checks.

Validation recorded on 2026-10-11 for native PSRAM/cache diagnostics:

- All 21 host, configuration, and code-generation tests passed.
- The full `examples/fnk0104s-esp-idf.yaml` firmware build passed with six
  generated diagnostic sensors and the cache-release button. The final image
  used 61% of the application partition.
- The full `examples/fnk0104s-hidden-images.yaml` firmware build passed with
  the generated diagnostics and used 61% of its application partition. Source
  generation also passed for a legacy singular-image configuration.
- On 2026-10-12, the maintainer confirmed on the physical FNK0104S that the
  diagnostic values reach the HA UI correctly and the cache-release button
  works. The HA history plot shows the values, and assigned images remain
  visible after release. The maintainer confirmed that the remaining diagnosis
  checks, including hidden-page rendering and released-byte accounting, are
  complete; no numeric measurements were supplied for this record.

`scripts/hardware_smoke_test.py` is intentionally excluded from this suite. It
requires explicit device access and writes only a random `.cyd-smoke-*.bin` file,
then downloads and removes that same file.
