# Design decisions and open hardware questions

## Fixed for the first release

- FNK0104S / ESP32-S3 / ESP-IDF only.
- Native ESP-IDF `esp_http_server` on port 8080; ESPHome `web_server` remains on 80.
- Raw `application/octet-stream` uploads with a required `Content-Length`.
- FAT/SDMMC, one active upload, 8 KiB streaming buffers, no recursive delete.
- Empty files are accepted as valid uploads.
- `max_upload_size` defaults to 200 MB and must be non-zero.
- The API key is required and is sent in clear text. Deployment is restricted to
  a trusted, segmented LAN; the device must not be exposed to the internet.
- Internal SDMMC pull-ups are enabled by default only as a supplement to the
  board wiring. They are not a replacement for suitable external pull-ups.
- FAT rename cannot provide a complete power-loss transaction. Overwrite uses a
  recoverable same-directory backup, and boot cleanup restores it when needed.
- FATFS is assumed not to support symlinks. Every external path is still parsed
  centrally and traversal segments are rejected.
- Downloads use a raw HTTP response with an exact `Content-Length`, followed by
  the file body in 8 KiB blocks. This avoids the incompatible combination of
  `Content-Length` and `Transfer-Encoding: chunked`, retains bounded memory use,
  and supports clients that require a known response size to finish decoding.
- The optional LVGL path uses a per-open-file read cache (16 kB by default) to
  combine small decoder reads before they reach FATFS/SDMMC.
- Decoded image caching is opt-out, allocated on demand, strictly PSRAM-only,
  defaults to a 1 MB budget, is user-configurable up to 4 MB, and uses
  least-recently-used eviction. Cache entries are shared by normalized SD path
  across all configured widgets, and no descriptor referenced by any widget is
  ever evicted. A failed allocation or missing PSRAM falls back to the original
  `S:/` streaming path instead of consuming internal RAM.
- Multi-widget configuration uses one compiled image allow-list. Every widget
  has its own initial selection and optional Home Assistant Select; omitting
  `select` creates a static SD-backed widget without an HA entity.
- Cache invalidation uses a generation counter for mapped upload, overwrite,
  and delete operations. The HTTP response format and browser transfer path do
  not depend on the decoded cache.

## Confirmed from `ESPHOME_PINOUT.md`, not yet tested on hardware

CLK GPIO38, CMD GPIO40, D0 GPIO39, D1 GPIO41, D2 GPIO48 and D3 GPIO47 are the
documented FNK0104S SDMMC pins and do not overlap its display, touch or audio
signals.

## Still requires real hardware

- External pull-up population and stable 3.3 V supply during writes.
- SDMMC four-bit stability and the exact card/FAT32 combinations in use.
- Rename and power-loss behavior of the deployed ESP-IDF/FATFS version.
- Socket/heap behavior with both HTTP servers active for at least one hour.
- Page-switch timing for PSRAM cache misses and hits, including whether the
  300-500 ms target is reached after LVGL and display-SPI flushing.
- PSRAM fragmentation and LRU behavior during repeated image replacement and
  selection.
- Whether 200 MB is the desired production upload limit.
- File timestamps are best-effort only until time synchronization is configured.
