# CYD SD HTTP Server for ESPHome

Native, streaming SD-card file transfer for the Freenove FNK0104S. The ESPHome
web server remains on port 80 and this component runs an independent ESP-IDF
HTTP server on port 8080. Firmware OTA remains the normal ESPHome OTA service;
file uploads never use multipart handling or firmware-OTA internals.

## Support status

This implementation targets only:

- Freenove FNK0104S / ESP32-S3R8
- ESPHome 2026.8.0
- ESP-IDF 5.5.5 selected by that ESPHome release
- 8 MB octal PSRAM at 80 MHz
- four-bit SDMMC: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47

It has been host-tested locally and has reached the first FNK0104S hardware
boot test. That test identified the required 16 KiB `loopTask` stack for direct
JPEG rendering; functional and endurance tests remain open in the
[hardware matrix](docs/HARDWARE_TEST_MATRIX.md). Arduino, other ESP32 variants,
other boards, SPI SD cards, hotplug, TLS and public internet exposure are
unsupported.

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
- optional read-only LVGL `S:` drive backed by the same mounted `SdStorage`
- direct, incremental baseline-JPEG and LVGL-BIN rendering from SD without an
  `online_image` full-frame runtime buffer
- a static YAML name-to-path allow-list exposed as an ESPHome Select entity for
  changing the displayed image from Home Assistant
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
  id: cyd_sd_server
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
  display_image:
    image_widget: sd_display_image
    drive_letter: S
    initial_image: Bild 1
    images:
      - name: Bild 1
        path: /Bild1.jpg
      - name: Bild 2
        path: /images/Bild2.jpg
    select:
      id: sd_display_image_select
      name: Display image
```

`format_if_mount_failed` defaults to false. Do not enable it casually: an
unknown or damaged filesystem could otherwise be reformatted. Internal pull-ups
only supplement suitable board-level pull-ups. The example upload limit is a
starting value, not a hardware-tested recommendation.

### Direct SD images and the Home Assistant dropdown

`display_image` is optional. When configured, the component registers a
read-only LVGL drive (default `S:`), connects it to the named LVGL image widget,
and creates the configured ESPHome Select entity. Home Assistant discovers that
entity through the normal ESPHome API and shows `Bild 1`, `Bild 2`, and the
other configured names as dropdown options. Selecting an option changes the
widget source to the mapped SD path, for example `S:/Bild1.jpg`; the path itself
is not supplied by Home Assistant.

The complete display, placeholder widget, and mapping are shown in
[the FNK0104S example](examples/fnk0104s-esp-idf.yaml). The one-pixel compiled
placeholder is necessary because ESPHome requires an initial `src` for the LVGL
image widget. It is replaced during setup and does not allocate a full-screen
runtime image.

For the full build-to-display flow, including the generated Home Assistant
dropdown and live SD reads, see
[Direct SD Image Workflow](docs/DIRECT_SD_IMAGE_WORKFLOW.md).

This first direct-image implementation accepts:

- `.jpg` and `.jpeg`: baseline JPEG decoded incrementally by LVGL Tiny JPEG;
- `.bin`: an LVGL 9.5-compatible binary image asset.

Progressive JPEG and PNG are not supported by this path. Convert photographs to
baseline JPEG at the display's intended dimensions (320 x 480 in the example).
LVGL still uses its display draw buffer, decoder work memory, and small file
buffers, but it does not retain the complete JPEG source or a permanent
307,200-byte RGB565 `online_image` buffer.

The reference FNK0104S build with this feature occupies about 1.20 MiB of its
1.835 MiB OTA application partition (about 66%), leaving roughly 0.63 MiB for
normal ESPHome entities, automations, and display work. The 16 KiB stack
setting adds 8 KiB of internal RAM compared with the default and has no
meaningful flash impact. Much larger features such as a camera, voice stack,
TLS-heavy integrations, or additional media decoders should be compiled and
checked against the same partition budget before being added.

Names and paths must be unique. Paths must be normalized absolute virtual paths
and may not contain traversal, reserved upload/backup names, or unsupported
extensions. The configured initial image must be one of the mapped names. If a
selected file is missing or busy, the component keeps the previous image and
does not publish the failed selection.

All HTTP functions remain available when direct images are enabled. Browser and
command-line downloads still use fixed `Content-Length` and 8 KiB streaming.
LVGL and HTTP share the same synchronized storage service; replacing or deleting
a file that currently has an active reader returns `409 Conflict` instead of
invalidating that reader.

The component registers its second listener, control socket and four client
sockets with ESPHome's socket accounting. With the complete example ESPHome
therefore calculates `CONFIG_LWIP_MAX_SOCKETS` rather than silently relying on
the smaller default.

### Required ESP32 framework settings

When `display_image` uses direct JPEG files, configure the ESPHome main loop
task with 16 KiB of stack:

```yaml
esp32:
  framework:
    type: esp-idf
    advanced:
      loop_task_stack_size: 16384
      include_builtin_idf_components:
        - fatfs
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"
```

The default 8 KiB task stack overflows when the LVGL Tiny JPEG decoder's 4 KiB
temporary buffer is combined with the LVGL, FATFS and SDMMC call chain. The
16 KiB setting reserves an extra 8 KiB of internal RAM once; it is not a
full-screen image buffer and does not grow with the image size.

`CONFIG_FATFS_LFN_STACK: "y"` enables normal FAT32 long file names (LFN), such
as names longer than 8.3 characters or names containing spaces. It allocates
FatFS's temporary LFN buffer on the calling task stack (roughly 512 bytes with
the default 255-character limit). Keep this setting: the server accepts paths
up to 255 UTF-8 bytes and its file UI supports ordinary long names. It was not
the root cause of the JPEG crash; the 16 KiB task stack covers both it and the
JPEG decoder.

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
