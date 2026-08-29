# CYD SD HTTP Server for ESPHome

Native, streaming SD-card file transfer for the Freenove FNK0104S. The ESPHome
web server remains on port 80 and this component runs an independent ESP-IDF
HTTP server on port 8080. Firmware OTA remains the normal ESPHome OTA service;
file uploads never use multipart handling or firmware-OTA internals.

## Support status

This first implementation targets only:

- Freenove FNK0104S / ESP32-S3R8
- ESPHome 2026.8.0
- ESP-IDF 5.5.5 selected by that ESPHome release
- 8 MB octal PSRAM at 80 MHz
- four-bit SDMMC: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47

It has been validated and host-tested locally, but **has not been run on real
hardware yet**. It must not be described as production-ready until the open
[hardware matrix](docs/HARDWARE_TEST_MATRIX.md) has been completed. Arduino,
other ESP32 variants, other boards, SPI SD cards, hotplug, TLS and public
internet exposure are unsupported.

## Features

- Native `esp_http_server`, independent of `web_server_base` and `USE_WEBSERVER_OTA`
- required `X-API-Key` with a constant-work comparison
- health, paginated directory listing, streamed download, streamed raw upload,
  file delete, directory create and empty-directory delete
- 8 KiB transfer buffers; files are never loaded completely into RAM/PSRAM
- centralized percent decoding and path validation
- one upload at a time, explicit `Content-Length`, configurable size limit and
  preflight free-space check
- same-volume temporary file, checked write/flush/fsync/close/size, then rename
- recoverable overwrite backup; stale uploads are removed and interrupted
  overwrite backups are restored during boot
- embedded, same-origin UI without CDNs; API key lives only in `sessionStorage`
- streaming Python uploader and an explicit real-device smoke-test script

## Configuration

Copy [the FNK0104S example](examples/fnk0104s-esp-idf.yaml) into your ESPHome
configuration directory and point the local external component path at
`components`. Copy `examples/secrets.example.yaml` to the ignored
`examples/secrets.yaml`, then replace every placeholder—especially the API key
with a random value of at least 32 bytes.

```yaml
external_components:
  - source:
      type: local
      path: ../components

cyd_sd_http_server:
  port: 8080
  control_port: 32769
  mount_point: /sdcard
  format_if_mount_failed: false
  internal_pullups: true
  bus_width: 4
  clk_pin: GPIO38
  cmd_pin: GPIO40
  data0_pin: GPIO39
  data1_pin: GPIO41
  data2_pin: GPIO48
  data3_pin: GPIO47
  max_upload_size: 200MB
  api_key: !secret sd_server_api_key
```

`format_if_mount_failed` defaults to false. Do not enable it casually: an
unknown or damaged filesystem could otherwise be reformatted. Internal pull-ups
only supplement suitable board-level pull-ups. The example upload limit is a
starting value, not a hardware-tested recommendation.

The component registers its second listener, control socket and four client
sockets with ESPHome's socket accounting. With the complete example ESPHome
therefore calculates `CONFIG_LWIP_MAX_SOCKETS` rather than silently relying on
the smaller default.

## HTTP API

Every `/sd/api/*` request requires `X-API-Key`. The UI is at
`http://DEVICE:8080/sd/`.

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/sd/api/health` | mount/card/capacity/version/upload/uptime status |
| `GET` | `/sd/api/list?path=/&offset=0&limit=100` | one directory, max 100 rows per page |
| `GET` | `/sd/api/file?path=/file.bin` | streamed download |
| `POST` | `/sd/api/file?path=/file.bin&overwrite=false` | raw binary upload |
| `DELETE` | `/sd/api/file?path=/file.bin` | delete a regular file |
| `POST` | `/sd/api/directory?path=/media` | create one directory |
| `DELETE` | `/sd/api/directory?path=/media` | delete an empty directory only |

Upload example:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @./photo.jpg \
  "http://device.local:8080/sd/api/file?path=%2Fphoto.jpg&overwrite=false"
```

Responses use a stable JSON envelope. Errors include the operation, virtual
path, machine-readable storage/request error, message, `errno` where applicable
and processed/expected byte counts for uploads. Statuses include 400, 401, 404,
409, 411, 413, 422, 500, 503 and 507. A handler never returns JSON success before
the related filesystem operation is complete.

Downloads send the exact file size as HTTP `Content-Length` while the body is
still read and transmitted in 8 KiB blocks. This fixed-length streaming keeps
memory usage bounded, works with browsers and command-line clients, and lets
clients such as ESPHome `online_image` reliably detect the end of a file.

## Python uploader

The uploader reads `SD_API_KEY` or asks without echoing the key:

```bash
SD_API_KEY='…' python scripts/upload_file.py \
  http://device.local:8080 ./photo.jpg /photos/photo.jpg --verify
```

`--verify` downloads to an OS temporary file, compares SHA-256 and removes only
that local temporary file. It does not delete the remote upload.

## Local validation

No hardware is touched by these commands:

```bash
python -m unittest discover -s tests -v
esphome config examples/fnk0104s-esp-idf.yaml
esphome compile examples/fnk0104s-esp-idf.yaml
```

The native path suite compiles the exact firmware C++ implementation. The
separate `scripts/hardware_smoke_test.py` is never part of local tests and must
only be invoked deliberately against a physical device.

Only `secrets.example.yaml` is checked in; a real `secrets.yaml` remains ignored.
Build details and design constraints are recorded in
[BUILD_ENVIRONMENT.md](docs/BUILD_ENVIRONMENT.md) and
[DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md).

## Security and known limitations

HTTP exposes the API key and data in clear text. Use only a trusted, segmented
LAN, firewall port 8080 and never expose the server to the internet. There is no
per-user authorization, TLS, brute-force throttling, recursive delete or SD
hotplug support.

FATFS is not a transactional filesystem. The implementation prevents a failed
or interrupted body transfer from replacing an existing target and restores a
renamed backup on the next boot when possible, but complete power-loss atomicity
cannot be promised until it is characterized on the real card and deployed IDF
build. Directory timestamps are best-effort without configured system time.
