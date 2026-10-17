> **Disclaimer — origin, AI-generated code, and scope**
>
> This is an independent, community-developed open-source project.
> It is not affiliated with, endorsed by, or officially supported by the Home Assistant or ESPHome projects.
> All respective trademarks belong to their owners.
>
> This project was inspired by the idea behind
> [n-serrette/esphome_sd_card](https://github.com/n-serrette/esphome_sd_card),
> but was rewritten from scratch, with approximately 99% of the code generated
> through AI-assisted “vibe coding.” I started this rewrite because the original
> `sd_mmc_card` configurations did not run reliably on my system and sometimes
> produced errors with the latest ESP firmware available at the time. This
> describes my experience with my setup, not a general assessment of the original
> project's quality or compatibility.
>
> This code is developed and tested exclusively for my specific test hardware,
> documented below. I provide no warranty and accept no responsibility for its
> use on other hardware or in different projects. If this project interests or
> inspires you, you are welcome to use it or fork it at your own risk. It includes
> improvements and additional features for my use case, described in the rest of
> this README.

# CYD SD HTTP Server for ESPHome

- **Manage your SD card in the browser** — dedicated web server for uploads, downloads, folders, and deletion, alongside ESPHome and OTA.
- **Bring SD images to your dashboard** — static LVGL widgets, Home Assistant dropdowns, or text status with automation actions.
- **Cache on demand, stream when needed** — dynamically allocated PSRAM image cache with direct SD-streaming fallback.
- **One image library, shared memory** — one YAML allow-list for all widgets; the same cached image uses one pixel buffer across multiple widgets.
- **One source, different widget sizes** — optional per-widget `fit` modes scale a shared cached image without resized SD files.
- **Prepare and transfer with helper tools** — host-side LVGL BIN conversion and streaming Python uploads with optional SHA-256 verification.

Native, streaming SD-card file transfer for the Freenove FNK0104S. The ESPHome
web server remains on port 80 and this component runs an independent ESP-IDF
HTTP server on port 8080. Firmware OTA remains the normal ESPHome OTA service;
file uploads never use multipart handling or firmware-OTA internals.

## Target hardware and test status

This implementation targets only:

- Freenove FNK0104S / ESP32-S3R8
- ESPHome 2026.8.0
- ESP-IDF 5.5.5 selected by that ESPHome release
- 8 MB octal PSRAM at 80 MHz
- four-bit SDMMC: CLK 38, CMD 40, D0 39, D1 41, D2 48, D3 47

Basic functional tests have already been performed by the maintainer on real
FNK0104S hardware. Image display, RAM/PSRAM usage, Home Assistant Selects, and
the web file manager's upload, download, folder, and delete operations work
reliably in those tests.

The maintainer also reported successful physical-device operation of the hidden
image Select, its HA text status, and image switching through the HA action on
2026-10-10. On the same date, the maintainer confirmed that fitted images scale
correctly after the RGB565A8 fix and do not require a second image buffer.
The RGB565 color-format warning has disappeared. Occasional warnings about
roughly 150-850 ms of computation during new uncached operations were already
observed before this fix.
On 2026-10-12, the maintainer confirmed on the physical device that the native
PSRAM/cache diagnostic values reach the Home Assistant UI correctly and that
the cache-release button works. The HA history plot displays the values, and
assigned images remain visible after cache release. The maintainer confirmed
that the remaining diagnosis hardware checks, including hidden-page behavior
and released-byte accounting, are complete.
Reconnect, migration, and endurance scenarios remain open in the hardware
matrix.

Still pending are systematic tests of failures and recovery, simultaneous
access by multiple clients, operator mistakes, power interruptions, and SD-card
failure or corruption, as well as extended stress and endurance testing.
Implemented safeguards are described below; their presence is not proof that
every failure scenario has been validated on hardware.

The [hardware matrix](docs/HARDWARE_TEST_MATRIX.md) and older development notes
contain historical, incomplete test records. Their “Not run” entries do not
override the maintainer's basic functional results summarized here, nor do those
results establish that every detailed matrix scenario has passed.

The versions above identify the reference environment, not a promise of
compatibility with every newer release. Arduino, other ESP32 variants, other
boards, SPI SD cards, hotplug, TLS and public internet exposure are unsupported.

## Features

The original project provided the starting idea of SD-card access and browser
file management. This rewrite focuses on bounded transfer memory, an independent
HTTP service, guarded file replacement, and direct SD-to-LVGL image display with
Home Assistant integration. These are improvements for the target setup; no
comparative performance benchmark against the original project is claimed.

- Native `esp_http_server`, independent of `web_server_base` and `USE_WEBSERVER_OTA`
- required `X-API-Key` with a constant-work comparison
- health, paginated directory listing, streamed download, streamed raw upload,
  file delete, directory create and empty-directory delete
- 8 KiB HTTP transfer buffers; HTTP uploads and downloads never load complete
  files into RAM/PSRAM
- centralized percent decoding and path validation
- one upload at a time, explicit `Content-Length`, configurable size limit and
  preflight free-space check
- same-volume temporary file, checked write/flush/fsync/close/size, then rename
- recoverable overwrite backup; stale uploads are removed and interrupted
  overwrite backups are restored during boot
- embedded, same-origin UI without CDNs; API key lives only in `sessionStorage`
- optional read-only LVGL `S:` drive backed by the same mounted `SdStorage`
- direct baseline-JPEG and LVGL-BIN rendering from SD without the local HTTP
  loop used by `online_image`
- configurable 16 kB LVGL file-read cache plus an optional, dynamically
  allocated PSRAM LRU cache for decoded RGB565 frames (1 MB default, up to
  4 MB)
- one shared YAML name-to-path allow-list for any number of LVGL image widgets;
  each widget can optionally expose its own independent Home Assistant Select,
  or a read-only text status with an image-selection action
- streaming Python uploader and an explicit real-device smoke-test script

## Do's and don'ts

| Do | Don't |
|---|---|
| Start with the documented FNK0104S pins and ESP-IDF configuration. | Assume that another “CYD” board has the same wiring or is supported. |
| Use a prepared FAT32 card, keep backups, and power down before removing it. | Treat the card as the only copy of important data or assume hotplug support. |
| Keep `format_if_mount_failed: false` for normal use. | Enable automatic formatting to troubleshoot an unexplained mount failure; it can erase the card. |
| Keep a strong API key in `secrets.yaml` and restrict access to a trusted LAN. | Commit credentials or expose the plain-HTTP service to the internet. |
| Upload one file at a time, create parent folders first, and opt into overwrites deliberately. | Send multipart or chunked uploads, assume recursive folder creation/deletion, or expect parallel upload support. |
| Use virtual paths such as `/images/photo.jpg` and URL-encode query values. | Pass `/sdcard/...`, `S:/...`, traversal segments, or internal `.cyd-upload-` / `.cyd-backup-` names to the HTTP API. |
| Prepare baseline JPEG or compatible LVGL BIN assets at a resolution suitable for the largest widget that uses them. | Use PNG or progressive JPEG directly in the SD image display path, or assume any `.bin` file is an image. |
| Keep the 16 KiB loop-task stack for JPEG rendering and budget PSRAM for all components. | Assume the cache limit reserves memory or increase it without checking available PSRAM. |
| Handle errors and verify important transfers, for example with the Python uploader's `--verify`. | Treat recovery logic as a guarantee against power loss, card failure, or user mistakes. |

## Installation

Use one of the following external-component sources. ESPHome documents both
source types in its [External Components guide](https://esphome.io/components/external_components/).

### Option A: local checkout

Download or clone this repository onto the machine that runs ESPHome. Keeping
the example in its original directory gives this layout:

```text
DavsESPHomeSDCardServer/
├── components/
│   └── cyd_sd_http_server/
└── examples/
    ├── fnk0104s-esp-idf.yaml
    ├── secrets.yaml
    └── assets/
        └── sd-image-placeholder.svg
```

In `examples/fnk0104s-esp-idf.yaml`, use:

```yaml
external_components:
  - source:
      type: local
      path: ../components
    components: [cyd_sd_http_server]
```

The path is relative to the device YAML file and points to the directory
containing `cyd_sd_http_server`, not to the component directory itself. If you
move the YAML into another ESPHome configuration directory, adjust this path.
For a container or Home Assistant ESPHome installation, the checkout must be
accessible inside the environment running ESPHome.

### Option B: GitHub repository (placeholder)

Replace `YOUR_GITHUB_USER`, `YOUR_REPOSITORY`, and `YOUR_TAG_OR_COMMIT` with the
actual repository and revision before validating this configuration:

```yaml
external_components:
  - source:
      type: git
      url: https://github.com/YOUR_GITHUB_USER/YOUR_REPOSITORY.git
      ref: YOUR_TAG_OR_COMMIT
    components: [cyd_sd_http_server]
```

Pin a known revision for reproducible builds. This source loads the component;
copy the device example and its `assets/sd-image-placeholder.svg` separately if
you want the complete display demo. Keep `assets/` next to the copied YAML, or
adjust the example's `image.file` path.

### Secrets and first installation

Create `secrets.yaml` next to your device YAML using
[secrets.example.yaml](examples/secrets.example.yaml) as the template:

```yaml
wifi_ssid: "YOUR_WIFI_SSID"
wifi_password: "YOUR_WIFI_PASSWORD"
sd_server_api_key: "REPLACE_WITH_A_RANDOM_KEY_OF_AT_LEAST_32_BYTES"
```

Generate a key locally, for example with `openssl rand -hex 32`, and paste the
result into `sd_server_api_key`. Replace all placeholders before use. The
component requires at least 32 UTF-8 bytes; the placeholder is not a secret.
`examples/secrets.yaml` is ignored by this repository; if you move it elsewhere,
ensure your own repository also ignores it.

With ESPHome 2026.8.0 available, run these commands from the repository root
after preparing the example and secrets:

```bash
esphome config examples/fnk0104s-esp-idf.yaml
esphome compile examples/fnk0104s-esp-idf.yaml
esphome upload examples/fnk0104s-esp-idf.yaml --device /dev/ttyACM0
```

Replace `/dev/ttyACM0` with your board's serial port. For subsequent firmware
updates, use normal ESPHome OTA or the ESPHome dashboard. The SD upload endpoint
stores files only; it does not flash firmware.

After boot, open `http://fnk0104s-sd-server.local:8080/sd/` (or use the device's
IP address), enter the API key, and upload your files. The full display example
expects the JPEG and logo files listed under `display_images.images`; create
their folders and upload them, or adapt the mapping and widgets to your assets.
Restart after initially provisioning the assets to load all initial images.
The SD image files do not need to exist at compile time; see
[Image provisioning and rebuild requirements](#image-provisioning-and-rebuild-requirements)
for the distinction between the compiled allow-list and files on the card.

## Configuration

### Minimal file server

This is a complete device configuration for the target board's SD file server,
without the optional display integration. It uses the local source layout above;
replace only `external_components` to use the GitHub source instead. Wi-Fi
credentials and the API key come from the adjacent `secrets.yaml`.

```yaml
esphome:
  name: fnk0104s-sd-server
  friendly_name: FNK0104S SD Server

esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf
    advanced:
      loop_task_stack_size: 16384
      include_builtin_idf_components:
        - fatfs
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"

psram:
  mode: octal
  speed: 80MHz

logger:

wifi:
  ssid: !secret wifi_ssid
  password: !secret wifi_password

api:

ota:
  - platform: esphome

web_server:
  port: 80

external_components:
  - source:
      type: local
      path: ../components
    components: [cyd_sd_http_server]

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
```

`format_if_mount_failed` defaults to false. Internal pull-ups only supplement
suitable board-level pull-ups. The 200 MB upload limit is the configuration
default, not a hardware-tested maximum file size or a reserved RAM allocation.

| Setting | Default / requirement |
|---|---|
| `api_key` | Required; at least 32 UTF-8 bytes. |
| `port` | `8080`; keep distinct from other HTTP listeners. |
| `control_port` | `32769`; internal ESP-IDF HTTP server UDP control socket, distinct from `port` and other server control ports. |
| `mount_point` / `url_prefix` | Only `/sdcard` / `/sd` are accepted. |
| `bus_width` | `4`; accepts `1` or `4`, but only the documented four-bit setup is the test target. |
| SDMMC pins | CLK, CMD, and D0 are required; D1–D3 are required for four-bit mode and must be omitted for one-bit mode. |
| `max_upload_size` | `200MB`; configurable from 1 to 2,147,483,647 bytes. |
| `display_images` | Optional; requires existing LVGL image widgets. |

### Optional image configuration

For the full display setup, start with
[the FNK0104S example](examples/fnk0104s-esp-idf.yaml), which includes the display,
backlight, placeholder asset, and LVGL widgets. The following block belongs
**inside the existing `cyd_sd_http_server:` section**. Both widget IDs must exist
under `lvgl`; they are already defined in the full example. If using this reduced
mapping with that example, remove the unused logo widgets or map them as well.

```yaml
  display_images:
    drive_letter: S
    cache:
      max_size: 1MB
      file_buffer_size: 16kB
      memory: psram
      strategy: lru
    images:
      - name: Image 1
        path: /Image1.jpg
      - name: Image 2
        path: /images/Image2.jpg
      - name: Logo 1
        path: /icons/logo1.bin
    widgets:
      - image_widget: sd_display_image
        initial_image: Image 1
        select:
          id: sd_display_image_select
          name: Display image
      - image_widget: logo_1
        initial_image: Logo 1
        fit: contain
```

Here, `sd_display_image` exposes a Home Assistant dropdown and `logo_1` loads an
initial image without creating a Select entity. All Selects offer the shared
image list. The English image names and paths are illustrative; existing image
names and SD filenames remain valid. When adopting these updated examples on a
running device, place the files at the shown paths and update any Home Assistant
automations that use the previous Select option names.

### Reuse one SD image at different widget sizes

Set `fit: contain` on each widget that should scale its selected image to fit.
The setting keeps the source's aspect ratio and centers it inside the widget;
unused space shows the widget background. Two widgets can select the same image
name and SD path at different sizes. They share one decoded RGB565 buffer in the
PSRAM cache, and LVGL scales that buffer separately while drawing each widget.
The [FNK0104S example](examples/fnk0104s-esp-idf.yaml) includes an
`image_fit_demo_page` with 320 × 300 and 160 × 120 widgets using `Image 1`.
When `fit` is configured, the component enables LVGL's RGB565A8 software
transform support automatically. Rebuild and flash the firmware after adding
`fit`; replacing an SD image alone cannot change this compile-time setting.

```yaml
    images:
      - name: Photo
        path: /images/photo.jpg
    widgets:
      - image_widget: large_photo
        initial_image: Photo
        fit: contain
      - image_widget: small_photo
        initial_image: Photo
        fit: contain
```

`fit` is optional for each widget, including the compatible singular
`display_image` configuration. Its values are:

| Value | Display behavior |
| --- | --- |
| Omitted or `none` | Existing unscaled placement; a smaller widget can crop the source. |
| `contain` | Preserve aspect ratio and show the entire image inside the widget. |
| `cover` | Preserve aspect ratio and fill the widget; edges may be cropped. |
| `stretch` | Fill the widget in both directions; aspect ratio may change. |

Use one source sized for the largest intended display area. A 320 × 480 RGB565
source occupies 307,200 bytes of decoded PSRAM whether it appears once or in
several smaller widgets. Scaling does not reduce that source allocation or the
CPU work needed to draw it. The SD file exists only once; a JPEG typically uses
less SD space than an uncompressed BIN at the same resolution. `fit` requires a
successful PSRAM cache entry: if the source is too large, PSRAM is unavailable,
or the cache budget is exhausted, the widget keeps its previous image and its
Select state does not change. Widgets without `fit` retain the existing
SD-streaming fallback. This restriction avoids scaled drawing from LVGL's
incremental JPEG decoder; [LVGL 9.5 has a reported JPEG reduction
issue](https://github.com/lvgl/lvgl/issues/10466).

### Hidden image Selects: text status and HA automations

Add `hidden: true` inside a widget's existing `select:` block to expose its
current image name as a read-only Home Assistant sensor instead of a dropdown:

```yaml
      - image_widget: sd_display_image
        initial_image: Image 1
        select:
          id: sd_display_image_select
          name: Display image
          hidden: true
```

The three modes can coexist on one device:

| Widget configuration | Home Assistant interface |
| --- | --- |
| No `select:` | No entity; static startup image |
| `select:` with omitted `hidden` or `hidden: false` | Existing image dropdown |
| `select:` with `hidden: true` | Read-only `sensor.*` and an image-selection action |

The hidden mode requires the normal ESPHome `api:` component. No additional
`text_sensor:`, `api.actions:`, `custom_services: true`, or lambda is needed.
The original Select ID remains available to local ESPHome automations, including
`select.set` and `on_value`. Its HA/MQTT Select interface is not exposed. The
generated text sensor inherits the name, icon, entity category,
`disabled_by_default`, sub-device assignment, and web-server sorting settings.
`internal` on the Select does not hide this status sensor: hidden mode always
keeps the Select internal and the status external. For an ID-only Select, the
ID supplies its status name, following ESPHome's normal name default.

For each hidden widget, the component registers
`cyd_sd_set_<image_widget_id>` (lowercase). HA prefixes this with the device name.
For the standard example, an automation's action is:

```yaml
action: esphome.fnk0104s_sd_server_cyd_sd_set_sd_display_image
data:
  option: Image 2
```

In HA's automation editor, select **Perform action**, choose the device's
`cyd_sd_set_...` action, and enter an exact name from `display_images.images`
in the `option` field. Use time, temperature, or light triggers and conditions
as usual. The action field is text; it does not provide a dropdown of images.
Actions remain callable manually and from scripts, so this is a presentation
mode rather than an access restriction.

For example, a time automation can switch at sunset. A temperature automation
can select one image above 25 °C and another below 23 °C, using separate
thresholds to avoid frequent toggling. On restart, the device loads
`initial_image`; add an HA availability/start trigger if the automation should
immediately restore an image based on current conditions.

Only allow-listed names are accepted. Unknown names and detected file-access
failures keep the previous image and status. Failed initial loading leaves the
sensor without a confirmed state. The status confirms a successful source
assignment, not completion of a later streaming decode. The API action has no
result response; check the text status and device logs when diagnosing failures.
Reselecting the same name still reloads a replaced file as before.

Switching an existing widget to hidden mode changes its HA domain from
`select.*` to `sensor.*`. Update its existing `select.select_option` automations
to use the new action; HA may retain an unavailable old entity that needs manual
removal. Switching back restores the Select interface and removes the generated
status/action from firmware. Both transitions require rebuilding and flashing.

See [the mixed-mode example](examples/fnk0104s-hidden-images.yaml) for a visible
image Select, a hidden logo Select, and three static logos on the same device.
The legacy singular `display_image` configuration also accepts `select.hidden`.

### Image provisioning and rebuild requirements

The current architecture fixes the `display_images.images` allow-list (image
names and SD paths), widget mappings, and Select options at compile time. Adding
or changing these YAML entries requires rebuilding and flashing the firmware;
uploading a new file does not automatically add it to the allow-list or create
a widget or Home Assistant option.

The image files themselves only need to be present on the mounted SD card when
the device loads them: during startup for each widget's `initial_image`, or
later when selected. They are not read from the SD card during compilation.
The local `assets/sd-image-placeholder.svg` used by the example is different:
it is a compiled asset and must be available on the build machine.

To provision images through the SD web server with a single firmware build:

1. Define the intended image names, paths, and widgets in YAML, then compile and
   flash the complete configuration.
2. Boot with a working, mounted SD card. Missing initial images leave their
   widgets showing the placeholder; the HTTP file server still starts.
3. Create the required folders and upload the files to the configured paths
   through `/sd/`.
4. Restart the device to load all initial images, including static widgets.
   For a widget with a Select, choosing the uploaded image through Home Assistant
   also triggers loading without a restart. Upload alone does not automatically
   assign a previously missing image to its widget.

If you start with the minimal file-server configuration, upload the images,
and only then add their allow-list entries and widgets to YAML, that workflow
requires a second build and flash. This is necessary because the configuration
changed, not because compilation requires the files to exist on the card.
Replacing file contents at an already configured path needs no rebuild;
reselect the image or restart to load the replacement.

### Direct SD images and the Home Assistant dropdown

`display_images` is optional. When configured, the component registers a
read-only LVGL drive (default `S:`) and connects every entry under `widgets` to
the shared `images` allow-list and PSRAM cache. During setup it loads each
widget's `initial_image`, so direct SD widgets do not require an `on_boot`
lambda.

A widget with `select:` creates its own independent ESPHome Select entity.
With `hidden: true`, it instead exposes the text status and action described above.
For the default visible mode,
Home Assistant discovers each entity through the normal ESPHome API and shows
the shared image names as dropdown options. Selecting an option changes only
that widget and publishes only that Select's state. A widget without `select:`
is initialized and cached in exactly the same way but creates no unnecessary
Home Assistant entity. `image_widget` IDs must be unique.

The source becomes a decoded PSRAM descriptor when caching succeeds, or a
mapped SD path such as `S:/Image1.jpg` in streaming fallback mode. Home Assistant
only sends a selected option; it never supplies an arbitrary path. The former
single-widget `display_image` schema remains accepted for existing YAML, but
new configurations should use `display_images`.

The complete display, placeholder widget, and mapping are shown in
[the FNK0104S example](examples/fnk0104s-esp-idf.yaml). The one-pixel compiled
placeholder is necessary because ESPHome requires an initial `src` for the LVGL
image widget. It is replaced during setup and does not allocate a full-screen
runtime image.

For the full build-to-display flow, including the generated Home Assistant
dropdown and live SD reads, see
[Direct SD Image Workflow](docs/DIRECT_SD_IMAGE_WORKFLOW.md).

The direct-image path accepts:

- `.jpg` and `.jpeg`: baseline JPEG decoded incrementally by LVGL Tiny JPEG;
- `.bin`: an LVGL 9.5-compatible binary image asset.

Progressive JPEG and PNG are not supported by this path. Convert photographs to
baseline JPEG at a resolution suitable for the largest intended widget
(320 x 480 for the full-screen image in the example).

The `cache` block is optional and defaults to the values shown above. It does
not reserve 1 MB at boot. The component allocates decoded RGB565 frames from
PSRAM on demand and evicts the least recently used inactive frame before the
configured limit would be exceeded. `max_size` may be configured from `0` to
`4MB`; 1 MB remains the conservative default. A 480 x 320 or 320 x 480 RGB565
frame uses 307,200 bytes, so the default budget holds three full-screen images.
Cache hits avoid reading and decoding the image body (a small existence probe
still opens the mapped SD file); the display still has to copy the frame
through LVGL and SPI. The first load remains a decode operation.

A larger limit is only a budget, not a guaranteed reservation. Allocation
still happens per decoded image and exclusively in PSRAM. Choose 4 MB only on
devices with enough free PSRAM for LVGL, display buffers, decoders, and all
other components. If an allocation fails, a widget without `fit` falls back to
direct `S:/...` streaming. A widget with `fit` keeps its previous image.

The decoded cache is shared by SD path across all configured widgets. If two
widgets show the same file, both descriptors reference one RGB565 allocation.
Eviction scans every widget and never frees an entry that is still assigned to
any of them. If the sum of simultaneously referenced unique images is larger
than `max_size`, widgets without `fit` use the safe `S:/...` streaming fallback
for entries that cannot fit. Widgets with `fit` keep their previous image.

Four 240 x 160 RGB565 logo widgets together use 307,200 bytes. Define all four
under `widgets` and their paths under the shared `images` list; the component
preloads them during setup, after which returning to their LVGL page draws from
PSRAM without a boot lambda. See the complete portrait-oriented 2x2 example in
[the FNK0104S configuration](examples/fnk0104s-esp-idf.yaml); for a rotated
480 x 320 layout, prepare each file as 240 x 160.

`file_buffer_size` belongs to each LVGL image file while it is open. It turns
the BIN decoder's small line reads into fewer larger FATFS/SDMMC reads and also
helps incremental JPEG input. Set `max_size: 0` to disable decoded-frame
caching, or `file_buffer_size: 0` to restore uncached LVGL file reads. The
decoded cache never falls back to internal RAM: when PSRAM is absent or an
allocation fails, the component logs the reason. Widgets without `fit` use the
original `S:/...` streaming path; widgets with `fit` keep their previous image.

Replacing or deleting a mapped image through the HTTP API invalidates the
decoded generation. Pixels currently referenced by widgets remain valid, but
the next selection reloads the changed file instead of returning stale cached
data.

### Native Home Assistant PSRAM diagnostics

When `display_images` or the compatible `display_image` block is configured,
the component creates six numeric diagnostic sensors and one Button entity
automatically. No `sensor:`, `button:`, lambda, or extra ESPHome YAML is needed.
With the existing `api:` connection, they appear on the device's Home Assistant
page under diagnostic entities:

| Entity name | Meaning |
|---|---|
| SD PSRAM Total | PSRAM bytes available to the ESP-IDF allocator |
| SD PSRAM Free | Currently free PSRAM bytes across the whole device |
| SD PSRAM Largest Free Block | Largest contiguous PSRAM allocation currently possible |
| SD Image Cache Limit | Configured decoded-image cache budget in bytes; not a reservation |
| SD Image Cache Used | Bytes in retained decoded RGB565 image buffers |
| SD Image Cache Reclaimable | Cache bytes assigned to no image widget |
| Release Unused SD Image Cache | Button that frees only reclaimable image buffers |

The sensor values use byte units and the diagnostic entity category. Changing
values have `measurement` state class, so Home Assistant can show their history
after it records them. Open a sensor's history or add a
[History graph card](https://www.home-assistant.io/dashboards/history-graph/)
in the HA dashboard to plot image switches and cache releases. Cache
values update after selections and button presses; all values are checked every
10 seconds. Repeated unchanged values are not republished.

The cache limit is not preallocated. `SD PSRAM Total` and `SD PSRAM Free` cover
all PSRAM users, including LVGL and other components, while `SD Image Cache
Used` counts only this component's decoded RGB565 pixel buffers. Pressing the
button does not detach pictures from widgets, including widgets on hidden
pages. It reports the released entry count and bytes in the device log; a
zero-byte release is normal when every cached picture is still assigned. The
device-wide free PSRAM change can differ from the pixel-byte count because of
allocator overhead and concurrent allocations.

An earlier reference FNK0104S example build with direct-image support occupied
about 1.10 MB of its 1.835 MB OTA application partition (about 60%), leaving roughly 0.74 MB
for normal ESPHome entities, automations, and display work. Treat these figures
as a recorded example, and check the size reported by your own build. The 16 KiB
stack setting adds 8 KiB of internal RAM compared with the default and has no
meaningful flash impact. Much larger features such as a camera, voice stack,
TLS-heavy integrations, or additional media decoders should be compiled and
checked against the same partition budget before being added.
The 2026-10-11 example build with native diagnostics produced a 1,119,907-byte
firmware image, using 61% of the 1,835,008-byte application partition.

Names and paths must be unique. Paths must be normalized absolute virtual paths
and may not contain traversal, reserved upload/backup names, or unsupported
extensions. The configured initial image must be one of the mapped names. If a
selected file is missing or busy, the component keeps the previous image and
does not publish the failed selection.

All HTTP functions remain available when direct images and their PSRAM cache
are enabled. Browser and command-line downloads still use fixed
`Content-Length` and 8 KiB streaming.
LVGL and HTTP share the same synchronized storage service; replacing or deleting
a file that currently has an active reader returns `409 Conflict` instead of
invalidating that reader.

The component registers its second listener, control socket and four client
sockets with ESPHome's socket accounting. With the complete example ESPHome
therefore calculates `CONFIG_LWIP_MAX_SOCKETS` rather than silently relying on
the smaller default.

### Required ESP32 framework settings

When `display_images` or the compatible `display_image` form uses direct JPEG
files, configure the ESPHome main loop task with 16 KiB of stack:

```yaml
esp32:
  framework:
    type: esp-idf
    advanced:
      loop_task_stack_size: 16384
      include_builtin_idf_components:
        - fatfs
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
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

Paths are relative to the SD-card root: `/photos/photo.jpg` is a virtual path,
`/sdcard/photos/photo.jpg` is its internal filesystem location, and
`S:/photos/photo.jpg` is the LVGL path. Use only the first form in API requests
and image mappings.

| Method | Endpoint | Purpose |
|---|---|---|
| `GET` | `/sd/api/health` | mount/card/capacity/version/upload/uptime status |
| `GET` | `/sd/api/list?path=/&offset=0&limit=100` | one directory, max 100 rows per page |
| `GET` | `/sd/api/file?path=/file.bin` | streamed download |
| `POST` | `/sd/api/file?path=/file.bin&overwrite=false` | raw binary upload |
| `DELETE` | `/sd/api/file?path=/file.bin` | delete a regular file |
| `POST` | `/sd/api/directory?path=/media` | create one directory |
| `DELETE` | `/sd/api/directory?path=/media` | delete an empty directory only |

The examples below use Bash. Set the device URL and read the API key without
putting its literal value in shell history:

```bash
SD_BASE_URL='http://fnk0104s-sd-server.local:8080'
read -r -s -p 'SD API key: ' SD_API_KEY
export SD_API_KEY
```

Check health and create the destination folder (skip creation if it exists):

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" "$SD_BASE_URL/sd/api/health"

curl --fail-with-body --show-error -X POST \
  -H "X-API-Key: $SD_API_KEY" \
  "$SD_BASE_URL/sd/api/directory?path=%2Fphotos"
```

Upload a file as a raw body with a known `Content-Length`. Do not use `curl -F`
or pipe an unknown-length body. The Python uploader below is the streaming
client for larger files.

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -H "Content-Type: application/octet-stream" \
  --data-binary @./photo.jpg \
  "$SD_BASE_URL/sd/api/file?path=%2Fphotos%2Fphoto.jpg&overwrite=false"
```

List the directory and download the file to the host:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  "$SD_BASE_URL/sd/api/list?path=%2Fphotos&offset=0&limit=100"

curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  --output ./downloaded-photo.jpg \
  "$SD_BASE_URL/sd/api/file?path=%2Fphotos%2Fphoto.jpg"
```

Use `overwrite=true` only when replacement is intended. Deleting a file uses
`DELETE /sd/api/file`; a directory must be empty before `DELETE /sd/api/directory`
will remove it. Neither upload nor directory creation creates missing parent
directories recursively.

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

The uploader uses Python's standard library, streams the request body, and reads
`SD_API_KEY` or prompts without echoing the key. Create `/photos` first:

```bash
python3 scripts/upload_file.py \
  http://fnk0104s-sd-server.local:8080 ./photo.jpg /photos/photo.jpg --verify
```

`--verify` downloads to an OS temporary file, compares SHA-256 and removes only
that local temporary file. It does not delete the remote upload.
Add `--overwrite` to replace an existing target deliberately.

## Host-side LVGL BIN converter

The independent [image conversion tool](add_tools/README.md) converts PNG and
JPEG files into the uncompressed LVGL 9.5 RGB565 BIN format accepted by the
direct `S:` image path. It supports proportional resizing when only width or
height is supplied, exact resizing when both are supplied, clockwise rotation,
and configurable compositing for transparent PNGs.

```bash
python3 -m venv .venv
.venv/bin/python -m pip install -r add_tools/requirements.txt
.venv/bin/python add_tools/image_to_lvgl_bin.py input.png \
  --rotate 90 --width 240 --height 160 --output logo.bin
```

Conversion happens entirely on the host computer and does not connect to or
modify the microcontroller. If `.venv` already exists, reuse it and skip its
creation.

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
Historical build details and design constraints are recorded in
[BUILD_ENVIRONMENT.md](docs/BUILD_ENVIRONMENT.md) and
[DESIGN_DECISIONS.md](docs/DESIGN_DECISIONS.md). Earlier environment restrictions
recorded there do not describe the maintainer's current hardware test status;
see [Target hardware and test status](#target-hardware-and-test-status).

## Security and known limitations

HTTP exposes the API key and data in clear text. Use only a trusted, segmented
LAN, firewall port 8080 and never expose the server to the internet. There is no
per-user authorization, TLS, brute-force throttling, recursive delete or SD
hotplug support.

FATFS is not a transactional filesystem. Uploads are staged in temporary files
so a failed body transfer does not directly overwrite the existing target.
The component attempts to restore interrupted overwrite backups at the next
boot. These safeguards cannot guarantee data integrity during power loss,
filesystem corruption, or physical card failure. Fault-injection and recovery
tests on the real card and deployed IDF build remain pending. Keep independent
backups. Directory timestamps are best-effort without configured system time.
