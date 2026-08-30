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

## Priority 1: Remaining Media Work

The goal is still not zero RAM usage. LVGL needs its display draw buffer, widget
state, decoder work memory, and small file buffers. The implemented image path
avoids keeping the complete source file or a permanently decoded full-screen
image in RAM. For comparison, a 320 x 480 RGB565 `online_image` requires a
307,200-byte runtime image buffer.

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
- invalidate or release the previous image cache entry;
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

Configure and manage LVGL so that only the currently needed media remains
cached:

- keep at most the current image in the image cache;
- release the previous cache entry when the source changes;
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
4. one active image and a bounded image-cache policy;
5. a static Home Assistant dropdown mapped to approved paths;
6. unchanged browser upload and download support.

PNG, automatic directory-backed dropdown options, an optional touchscreen
dropdown, arbitrary path strings, page lifecycle management, and audio
streaming remain separate milestones after the first version is stable and its
memory use has been measured on hardware.
