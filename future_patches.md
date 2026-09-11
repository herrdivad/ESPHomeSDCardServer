# Future Patches

## Implemented in 0.2.0: Direct SD Images and Home Assistant Selection

The recommended initial image milestone below is now implemented:

- `SdStorage` is shared by HTTP downloads and the read-only LVGL file-system
  driver, including open, read, absolute seek, tell, close, active-reader
  tracking, and conflict handling;
- LVGL reads allow-listed paths such as `S:/Bild1.jpg` directly from the mounted
  SD card without the local HTTP/`online_image` loop;
- baseline JPEG uses LVGL's incremental Tiny JPEG decoder, and LVGL 9.5 BIN
  images are accepted;
- YAML maps human-readable names to validated SD paths and exposes them through
  an ESPHome Select entity in Home Assistant;
- the authenticated browser UI and fixed-`Content-Length` HTTP downloads remain
  available.

This was compiled locally for the pinned ESPHome/ESP-IDF target and reached the
first physical FNK0104S boot test. That test found that LVGL Tiny JPEG decoding
overflows ESPHome's default 8 KiB `loopTask` stack when FATFS/SDMMC are in the
same call chain. The example and README now require
`loop_task_stack_size: 16384`; functional and endurance behavior remains open
in the test matrix.

## Priority 1: Reduce SD Image Page-Switch Latency

Before the cache implementation, the direct SD image path needed more than
approximately 1,000 ms to render a full-screen image during a page switch on
the physical FNK0104S. The initial performance target was to reduce the
duration reported by the page-switch log to **300-500 ms or less**, without
removing the existing HTTP server, browser downloads, uploads, or Home
Assistant image selection.

The RGB565 BIN result shows that JPEG decoding is not the only bottleneck. A
480 x 320 RGB565 image contains 307,200 bytes, and LVGL 9.5 may read a
file-backed BIN image in many small operations when `LV_BIN_DECODER_RAM_LOAD`
is disabled. Increasing `loop_task_stack_size` prevents stack overflow but does
not improve this throughput; the ESP32-S3 already runs at 240 MHz.

### Implemented in 0.3.0: bounded PSRAM and file caches

The first optimization stage is now implemented in the component and example:

- a configurable LVGL per-file read cache, defaulting to 16 kB and capped at
  64 kB, reduces repeated FATFS/SDMMC reads during the first decode;
- a dynamically allocated decoded RGB565 cache uses PSRAM only, defaults to a
  hard 1 MB budget, can be configured up to 4 MB, and evicts inactive entries
  in least-recently-used order;
- a 480 x 320 image uses 307,200 bytes, so the default budget can retain three
  full-screen frames without exceeding the configured limit;
- an image currently assigned to an LVGL widget is never evicted underneath
  LVGL, and systems without PSRAM retain the original low-RAM `S:/` streaming
  fallback;
- successful upload, overwrite, or deletion of a mapped image invalidates the
  decoded generation so a later selection cannot return stale pixels;
- browser uploads and fixed-`Content-Length` downloads are unchanged.

### Hardware result reported for 0.3.0

The PSRAM cache has now been exercised on the physical FNK0104S. The supplied
boot log confirms that the component allocated and used a 307,200-byte RGB565
frame inside the configured 1,000,000-byte PSRAM budget:

```text
Decoded image cached path=/Bild1.jpg bytes=307200 duration_ms=277
Display image selected: Bild 1 (PSRAM cache MISS, 307200/1000000 bytes)
Decoded image cache: 307200/1000000 bytes in PSRAM (1 entries)
```

Observed end-to-end behavior was approximately **600-700 ms** when selecting,
decoding, and placing a previously uncached image into PSRAM, depending on the
image and surrounding API work. The initial boot example logged 277 ms for the
decoder/cache-copy portion alone. Once the frame was cached, leaving and
returning to the image page took approximately **150-160 ms**, and subsequent
page changes were often below ESPHome's current blocking-warning threshold.

This verifies that PSRAM is actively used and that cached page redraws beat
the original 300-500 ms target. The remaining display time is primarily the
LVGL draw and SPI flush rather than SD reading or JPEG decoding. Exact timings
can still vary with image format, other ESPHome activity, and the adaptive
component warning threshold.

Continue with the following measurements and optimizations in this order:

1. Add timing and counters that separate file open/seek/read time, decoder and
   LVGL draw time, and final display flush time. Use one clearly defined
   page-switch duration for the acceptance target.
2. Measure first-load misses, repeated Select hits, and leave/return page
   switches with the implemented 16 kB file cache and 1 MB PSRAM LRU cache.
   Also compare 0, 16 kB, 32 kB, and 64 kB file-cache settings.
3. If the target is not reached, inspect whether the remaining duration is the
   LVGL-to-display copy and SPI flush rather than SD access or decoding.
4. Consider larger strip-based reads, a draw-ready cache format, or
   asynchronous prefetching into a bounded PSRAM buffer if the generic LVGL
   BIN decoder remains the limiting factor. LVGL object updates must still
   occur in LVGL's own execution context, and SD access must remain
   synchronized with HTTP operations.
5. Evaluate display SPI and SDMMC clock changes only after the software timing
   breakdown is available. Clock increases require physical-device stability
   testing and must not be used to hide inefficient small reads.

The low-RAM streaming mode must remain available. A faster PSRAM-backed mode
may be optional and configurable because fast repeated page switches and
strictly minimal RAM usage are competing goals. Performance and cache lifetime
should still be measured across more formats and longer runs on the physical
device; cache lifetime and fragmentation endurance remain open. Repository-side
work remains limited to implementation, compilation, and host-side tests.

## Implemented in 0.4.0: Multiple Dynamic Image Widgets

One component instance can now control an arbitrary number of LVGL image
widgets by their YAML IDs. The plural `display_images` configuration uses one
compiled image name-to-SD-path allow-list and one decoded PSRAM cache across
all configured widgets while preserving the existing path validation and
storage synchronization.

Each widget may receive its own ESPHome Select entity and Home Assistant
dropdown. Selecting an option updates only that widget and its published
Select state. Omitting `select:` creates a static SD-backed widget without an
unnecessary Home Assistant entity. The implemented configuration is:

```yaml
cyd_sd_http_server:
  display_images:
    images:
      - name: Weather sunny
        path: /icons/weather-sunny.jpg
      - name: Weather rain
        path: /icons/weather-rain.jpg

    widgets:
      - image_widget: weather_icon
        initial_image: Weather sunny
        select:
          id: weather_icon_select
          name: Weather icon

      - image_widget: secondary_image
        initial_image: Weather rain
        select:
          id: secondary_image_select
          name: Secondary image
```

Every configured widget is initialized during component setup, so neither
dynamic nor static widgets need an `on_boot` lambda. Decoded PSRAM entries are
shared by normalized SD path: two widgets displaying the same image reference
one allocation. Before LRU eviction, the component scans all widget bindings
and never frees an entry still referenced by any widget. If all fitting entries
are referenced, the new image uses the bounded `S:/` streaming fallback instead
of exceeding the cache limit.

The former singular `display_image` configuration remains accepted as a
backward-compatible one-widget form. Directory scanning, per-widget option
subsets, arbitrary user paths, automatic widget creation, and an on-screen LVGL
dropdown remain separate future enhancements.

## Priority 3: Remaining Media Work

The goal is still not zero RAM usage. LVGL needs its display draw buffer, widget
state, decoder work memory, and small file buffers. The implemented image path
does not keep the compressed source file in RAM. Its faster default mode does
retain recently decoded RGB565 frames in PSRAM, but only within the configured
hard budget; `max_size: 0` preserves the low-RAM streaming mode. For comparison,
one 320 x 480 RGB565 frame requires 307,200 bytes.

### Completed Phase 1: Shared SD Storage Service

Refactor the existing `SdStorage` instance into the single shared storage
service used by the HTTP server, LVGL, and later audio playback. The SD card
must remain mounted only once.

Add a read-only file-handle API with:

- open, read, seek, tell, size, and close operations;
- the existing virtual-path validation and reserved-name protection;
- active-reader tracking or read leases;
- defined conflict handling for uploads, replacements, deletions, directory
  changes, and unmounting;
- one synchronization policy shared by HTTP, LVGL, and future media readers.

Uploads may continue writing to their temporary files while unrelated files are
read. Committing a replacement, deleting a file, or changing a directory must
not invalidate a file currently used by LVGL or another reader. Operations that
cannot proceed safely should return a clear busy result instead of corrupting a
read or blocking the LVGL rendering task indefinitely.

### Completed Phase 2: Read-Only LVGL File-System Driver

Register an LVGL file-system driver for the `S:` drive and map paths such as
`S:/images/status.jpg` to `/sdcard/images/status.jpg` through the shared storage
service. The driver must implement LVGL's open, read, seek, tell, and close
callbacks and must not bypass `SdStorage` with independent `fopen` calls.

The existing authenticated browser UI and HTTP API must remain available for
uploads, downloads, directory listing, deletion, and directory creation. The
HTTP download path must keep its fixed `Content-Length` response behavior.

### Partly Completed Phase 3: Memory-Efficient Image Formats and Decoders

Start with formats that can be drawn without retaining a complete decoded image
in RAM:

1. JPEG for photographs, using incremental decoding;
2. LVGL BIN assets for predictable, line-oriented access and icons;
3. optionally BMP where its larger SD-card file size is acceptable.

PNG support should be optional and added only after measuring the decoder used
by the ESPHome/LVGL version pinned by the project. A PNG decoder may require a
full decoded frame and can therefore consume more RAM than the current RGB565
buffer, especially when alpha is required. Transparent icons should preferably
be converted to a suitable LVGL BIN color format when low RAM usage is the main
goal.

### Completed Phase 4: Runtime Asset Controller

Add a component action or equivalent API that safely changes an LVGL image
widget to a validated SD path, conceptually:

```text
set_lvgl_image_path("S:/Bild1.jpg")
```

The controller must:

- validate and normalize the requested virtual path;
- restrict sources to supported file extensions;
- confirm that the file exists before replacing the current image;
- retain the source string for as long as LVGL may reference it;
- perform the widget update in the correct LVGL execution context;
- keep the previous entry only while it fits the bounded LRU policy;
- leave the previous image visible and report an error if loading fails.

### Completed Initial Phase 5: Home Assistant Selection

The first version should expose a static, allow-listed ESPHome Select entity.
Human-readable options are mapped to validated SD paths instead of treating the
display name as an arbitrary file path. The selection may also be linked to an
LVGL dropdown so that Home Assistant and the touchscreen remain synchronized.

A later version may add:

- directory scanning and a dynamically refreshed list of compatible assets;
- a `Refresh media list` action;
- an optional text-based path entity for advanced use.

Any freely supplied path must reject traversal, reserved temporary or backup
files, unsupported extensions, excessive path lengths, and nonexistent files.

### Phase 6: Current-Page Memory Policy

Configure and manage LVGL with a strict, user-selected cache budget:

- retain recently used decoded images only up to the configured hard limit;
- evict inactive entries in least-recently-used order before exceeding it;
- never free a descriptor currently assigned to any image widget;
- invalidate entries after mapped files are replaced or deleted;
- close SD file handles after drawing or decoding completes;
- release page-specific media when leaving a page;
- consider lazy creation and destruction for pages with many widgets or assets.

This policy cannot remove the global LVGL display buffer or all decoder work
memory, but it should prevent multiple full images from accumulating in RAM.

### Phase 7: Audio Streaming

Audio is a separate consumer of the same shared storage service and is not part
of the LVGL file-system driver. Add a streaming pipeline that reads small SD
chunks into a decoder and PCM/I2S ring buffer instead of loading a complete
track into RAM. WAV/PCM can be streamed directly; compressed formats such as
MP3 require their own decoder and bounded working buffers.

The Home Assistant selection model can be reused for audio files, but playback,
pause, end-of-file handling, decoder errors, and I2S output require a separate
implementation phase.

### Verification and Compatibility Requirements

Add host-side tests for path mapping, seek boundaries, short reads, file close,
invalid extensions, reader tracking, and conflicts with upload, replace, delete,
and unmount operations. Add selection tests for allowed, missing, and malicious
paths and verify that switching assets releases the previous cache entry.

The existing HTTP behavior must receive regression coverage so browser uploads
and downloads continue to work after the storage refactor. Firmware compilation
and local tests can be performed in this repository; real display, SD-card,
audio, and concurrency behavior must be verified separately on the physical
device.

### Completed Initial Scope

The first milestone now contains:

1. one shared and synchronized SD storage service;
2. a read-only LVGL `S:` drive;
3. JPEG and LVGL BIN image sources;
4. multiple independently controlled image widgets plus one optional, bounded,
   path-shared PSRAM LRU cache;
5. optional independent Home Assistant dropdowns mapped to approved paths;
6. unchanged browser upload and download support.

PNG, automatic directory-backed dropdown options, an optional touchscreen
dropdown, arbitrary path strings, page lifecycle management, and audio
streaming remain separate milestones after the first version is stable. The
initial PSRAM allocation and page-switch timing have now been measured on
hardware; long-duration cache behavior remains open.
