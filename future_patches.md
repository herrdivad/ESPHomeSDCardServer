# Future Patches

Commit references below are local Git hashes and can be inspected with
`git show <hash>`. The implemented milestones are historical context; the
sections marked planned or remaining are the current work list.

## Completed: PSRAM Diagnostics and Home Assistant Cache Release Button

**Status (2026-10-12): implemented in the working tree after the maintainer's
approval. The maintainer confirmed that the diagnostic values reach the Home
Assistant UI correctly and the cache-release button works on the physical
device. The HA history plot and assigned images after cache release also work.
The maintainer confirmed that all diagnosis hardware checks are complete,
including hidden-page behavior and released-byte accounting.**

### Current behavior and scope

The decoded RGB565 image cache already tracks its configured byte limit,
allocated image bytes, entries, and per-widget references in
`cyd_sd_http_server.h/.cpp`. The new `diagnosis.cpp` publishes these values to
Home Assistant. The cache limit is an on-demand budget, not a PSRAM
reservation. ESPHome's existing `debug` PSRAM sensor can already report free
PSRAM for the whole device independently of this component; it cannot report
the component's decoded-cache usage or configured limit.

Keep three meanings separate in the HA presentation:

- **Usable PSRAM total/free:** ESP-IDF heap values for PSRAM-capable memory;
  total minus free includes all users of that heap, not just SD images. The
  physical board's advertised 8 MB and the allocator's usable total need not
  be identical.
- **Decoded image cache limit/used:** this component's configured maximum and
  actual retained RGB565 pixel allocations. This does not include LVGL draw
  buffers, decoder work memory, or the per-open-file read buffer.
- **Reclaimable cache bytes:** entries retained by the component but not
  assigned to any configured image widget. An image on a hidden LVGL page can
  still be assigned to a widget and must remain protected.

### Home Assistant entities

With either image configuration, the component now creates six numeric
diagnostic sensors without further YAML: PSRAM total, free, and largest free
block; decoded image cache limit, used, and reclaimable bytes. They use byte
units, integer precision, and `measurement` state class so Home Assistant can
graph changing values. A text sensor is unnecessary for the numeric history.

The component publishes a fresh snapshot after image selection and explicit
release and checks all values every 10 seconds. Unchanged values are not
republished. The Home Assistant recorder/history graph provides the plot;
the firmware does not store graph data.

The native `Release Unused SD Image Cache` HA Button evicts only decoded entries
whose descriptors are not assigned to any widget. It reuses the existing
reference check and `evict_cache_entry_` path, which drops the LVGL image-cache
entry before freeing pixel data. The log reports entries and bytes released;
the sensors refresh afterward. A zero-byte release is a successful no-op.
Active widget sources remain protected even when their page is hidden.

A separate `release all` operation is outside the initial scope. It would need
to detach or replace active LVGL sources, handle widgets with `fit` that cannot
use the streaming fallback, and guarantee a valid redraw. Assess that as a
separate feature only if there is a demonstrated need.

### Implementation and verification record

The component creates entities automatically when `display_images` or legacy
`display_image` is present and creates none in file-server-only configurations.
`diagnosis.cpp` owns sampling and release behavior; the existing Python
generator and server header/source contain only registration and event hooks.
README and both example YAML files document the native entities; the example
YAML additions are comments only. Host tests and a firmware build verify
generation and compilation. On 2026-10-12, the maintainer reported successful
physical-device tests of HA diagnostic values, the history plot, the release
button, and assigned-image visibility after release. The maintainer subsequently
confirmed that the remaining diagnosis checks, including exact released-byte
accounting and hidden-page rendering, are complete. No numeric measurements
were supplied for this record.

The accepted behavior is that cache-used bytes decrease by the released pixel
allocations, assigned images continue drawing correctly, and HA history shows
both image-switch changes and subsequent release. The device-wide free
PSRAM value can change by a different amount because other components use the
same allocator and allocator overhead is separate from image pixel bytes.

## Planned Next: Directory and Wildcard Image Sources with a Home Assistant Refresh Button

**Status (2026-09-13): planned for near-term implementation; awaiting the
maintainer's explicit GO. This entry authorizes planning only, not component
implementation.**

Git audit on 2026-10-10: the current component still has only compiled image
name/path mappings. Its browser UI has a file-list refresh control, but no
Home Assistant button that rescans image sources or updates Select options.

### Goal and compatibility

Extend the shared image library with optional SD directory and wildcard sources.
New matching files must become selectable without editing YAML, rebuilding, or
flashing firmware after the feature has been configured and deployed.

All code requests are additions by default, as recorded in `AGENTS.md`. Preserve
all existing features, supported YAML configurations, interfaces, and behavior
unless the maintainer explicitly requests replacement or removal. In particular,
retain explicit `images` name/path mappings, `initial_image`, both `display_images`
and legacy `display_image`, independent per-widget Selects, static widgets without
Selects, shared PSRAM caching, streaming fallback, and all existing HTTP/browser
file operations. Existing configurations must not require migration.

### Required Home Assistant interaction

- Provide a configurable ESPHome Button entity exposed in the Home Assistant GUI,
  with a user-facing name such as `Refresh image list`.
- Pressing this button rescans the configured directories and wildcard sources,
  adds newly discovered compatible images to the shared library, and updates the
  options of all configured image Selects in Home Assistant.
- Keep the existing widget, Select, and Button entity identities stable. Refresh
  must not create a separate entity or LVGL widget for each discovered file.
- Uploading a file followed by pressing the button must be sufficient to make it
  selectable; no device restart or firmware rebuild is required.
- The button refreshes the library; it must not automatically switch a widget to
  a newly discovered image. Preserve current selections by stable identity rather
  than by an index that can change when the list is updated.

### Proposed implementation design

1. Add optional YAML source definitions alongside the existing explicit mappings:
   a directory, simple filename patterns such as `*.jpg` or `logo*.bin`, and an
   optional recursive scan setting, disabled by default. Specify exact wildcard
   semantics and configuration keys during implementation; this is not yet valid
   component syntax. Retain support for `.jpg`, `.jpeg`, and LVGL `.bin`.
2. Discover files on the device through the synchronized `SdStorage` service.
   Propose an initial scan after mounting the SD card, with subsequent refreshes
   triggered by the required HA button. Keep scan work and result storage bounded
   so large directories do not monopolize the display loop or exhaust memory.
3. Build concrete name/path entries from matching files. Use the normalized full
   SD path as a stable internal identity, deduplicate overlapping sources, and
   give existing explicit mappings precedence for their configured paths and
   names. Resolve duplicate filenames and display-name collisions deterministically.
4. Keep YAML IDs compile-time declarations: `image_widget`, the Select entity,
   and the compiled placeholder retain their existing IDs. Discovered SD images
   are runtime library entries, not dynamically generated ESPHome `id(...)`
   objects. Runtime lookup uses a stable path/name mapping. Existing explicit
   `initial_image` assignments must keep working; define an additive way to refer
   to a discovered startup image if needed.
5. Publish refreshed Select options and update internal bindings safely in the
   appropriate execution context. Handle repeated button presses without
   overlapping scans, and preserve the last usable library on scan failure.
   Define behavior for empty sources, missing directories, removed files, and
   an active image disappearing without silently selecting a different image.
6. Apply centralized path validation and reserved temporary/backup-name protection
   to every discovered file. Preserve upload/read conflict handling, cache
   invalidation on replacement/deletion, PSRAM-only decoded caches, cache budgets,
   and the lifetime of any image still referenced by LVGL.

### Acceptance and verification after GO

- Existing explicit-only YAML and the legacy form continue to validate and behave
  as before, including startup images and widgets with or without Selects.
- A matching image uploaded into an allowed directory appears in all relevant HA
  Selects after pressing the refresh button; unrelated and unsupported files do not.
- Overlapping patterns, duplicate names, repeated refreshes, list reordering,
  empty/missing directories, removed active files, and scan failures have defined,
  tested behavior without corrupting widget assignments or losing explicit mappings.
- Host checks cover matching, validation, merging, and selection stability. Run
  YAML validation, code generation, and a full firmware build for the pinned target.
- Verify the HA button, updated dropdowns, display/cache behavior, and concurrent
  HTTP file operations on an explicitly intended physical device; record hardware
  results separately from host/build checks.

## Implemented in 0.2.0: Direct SD Images and Home Assistant Selection

Implemented in commit `70f00cd` (direct SD image workflow and HA Select):

- `SdStorage` is shared by HTTP downloads and the read-only LVGL file-system
  driver, including open, read, absolute seek, tell, close, active-reader
  tracking, and conflict handling;
- LVGL reads allow-listed paths such as `S:/Image1.jpg` directly from the mounted
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

## Remaining: Measure and Improve SD Image Page-Switch Latency

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

The first optimization stage was implemented in commit `894a46d` (PSRAM and
file-read caches):

- a configurable LVGL per-file read cache, defaulting to 16 kB and capped at
  64 kB, reduces repeated FATFS/SDMMC reads during the first decode;
- a dynamically allocated decoded RGB565 cache uses PSRAM only, defaults to a
  hard 1 MB budget, can be configured up to 4 MB, and evicts inactive entries
  in least-recently-used order;
- a 480 x 320 image uses 307,200 bytes, so the default budget can retain three
  full-screen frames without exceeding the configured limit;
- an image currently assigned to an LVGL widget is never evicted underneath
  LVGL, and widgets without `fit` retain the original low-RAM `S:/` streaming
  fallback when PSRAM caching is unavailable;
- successful upload, overwrite, or deletion of a mapped image invalidates the
  decoded generation so a later selection cannot return stale pixels;
- browser uploads and fixed-`Content-Length` downloads are unchanged.

### Hardware result reported for 0.3.0

The PSRAM cache has now been exercised on the physical FNK0104S. The supplied
boot log confirms that the component allocated and used a 307,200-byte RGB565
frame inside the configured 1,000,000-byte PSRAM budget:

```text
Decoded image cached path=/Image1.jpg bytes=307200 duration_ms=277
Display image selected: Image 1 (PSRAM cache MISS, 307200/1000000 bytes)
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

The cached page-redraw target has been met in the reported hardware test.
First loads, fragmentation, and longer runs remain open. Continue with these
measurements and optimizations in this order:

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

The low-RAM streaming mode remains available with `max_size: 0` for widgets
without `fit`; fitted widgets require a decoded PSRAM entry. Performance and
cache lifetime should still be measured across more formats and longer runs on
the physical device; cache lifetime and fragmentation endurance remain open.
Repository-side work remains limited to implementation, compilation, and
host-side tests.

## Implemented in 0.4.0: Multiple Dynamic Image Widgets

Implemented in commit `3655bcc` (multiple widgets and shared image library).

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
and never frees an entry still referenced by any widget. If the budget cannot
fit another image because all eviction candidates remain referenced, a widget
without `fit` uses the bounded `S:/` streaming fallback. A widget with `fit`
keeps its previous image and selection because scaling requires a decoded
PSRAM entry.

The former singular `display_image` configuration remains accepted as a
backward-compatible one-widget form. Directory scanning, per-widget option
subsets, arbitrary user paths, automatic widget creation, and an on-screen LVGL
dropdown remain separate future enhancements.

## Implemented Later: Hidden Image Controls and Widget Scaling

- Commit `77c3088` added optional hidden image Selects that publish a read-only
  HA text status and remain controllable through a named HA API action. The
  visible dropdown and static-widget modes remain available.
- Commit `7e9c61e` added per-widget `fit` modes and the RGB565A8 software
  transform needed to scale shared cached RGB565 images. The same decoded
  source can serve widgets of different sizes without an extra resized image
  allocation.

Those commits predate the PSRAM diagnostics and cache-release button implemented
locally above. Directory/wildcard refresh and audio playback remain open.

## Remaining Media Work

The goal is still not zero RAM usage. LVGL needs its display draw buffer, widget
state, decoder work memory, and small file buffers. The implemented image path
does not keep the compressed source file in RAM. Its faster default mode does
retain recently decoded RGB565 frames in PSRAM, but only within the configured
hard budget; `max_size: 0` preserves the low-RAM streaming mode. For comparison,
one 320 x 480 RGB565 frame requires 307,200 bytes.

### Completed Phase 1: Shared SD Storage Service

Commit `1b36591` introduced `SdStorage`; commit `70f00cd` extended its
synchronized read-only open/read/seek/tell/close interface for LVGL. HTTP and
LVGL share the same mounted storage service, centralized path validation, reader
tracking, and busy/conflict handling for operations that could invalidate an
active read. Future consumers such as audio must reuse this service and preserve
its bounded, synchronized access.

### Completed Phase 2: Read-Only LVGL File-System Driver

Commit `70f00cd` registered the read-only LVGL `S:` driver with open, read,
seek, tell, and close callbacks backed by `SdStorage`. The authenticated browser
UI and HTTP API, including fixed-`Content-Length` downloads, remain available.

### Partly Completed Phase 3: Memory-Efficient Image Formats and Decoders

Commit `70f00cd` added incremental baseline JPEG and LVGL BIN display sources.
Commit `6d24b95` added a host-side LVGL BIN converter. Optional direct BMP and
PNG support has not been added. Evaluate decoder working memory before adding
either format; transparent icons can be converted to an appropriate LVGL BIN
format on the host.

### Completed Phase 4: Allow-Listed Runtime Asset Controller

Commit `70f00cd` introduced runtime selection of compiled name/path mappings;
commit `3655bcc` extended it to independent widgets. The controller checks that
the mapped SD file can be opened, retains the LVGL source for its required
lifetime, and leaves the previous image and selection intact on failure. Cache
retention and eviction were added in commit `894a46d`.

The old conceptual `set_lvgl_image_path("S:/Image1.jpg")` example was not a
public arbitrary-path API. Runtime selection remains restricted to the compiled
allow-list. Arbitrary user-supplied paths are a separate, unimplemented feature
that would require the same path and reserved-name checks.

### Completed Initial Phase 5: Home Assistant Selection

Commit `70f00cd` introduced the allow-listed HA Select; commit `3655bcc` added
independent per-widget Selects. Commit `77c3088` added hidden Select mode with
read-only text status and an HA image-selection action. Dynamic directory
scanning and its refresh button remain planned above. An on-screen LVGL
dropdown and an optional freely supplied path entity remain unimplemented.
Any future free-path interface must reject traversal, reserved temporary or
backup names, unsupported extensions, excessive lengths, and missing files.

### Partly Completed Phase 6: Cache Budget and Page Lifecycle

Commit `894a46d` introduced the bounded PSRAM-only LRU decoded-image cache and
invalidation after mapped files change. Commit `3655bcc` made reference checks
cover every configured widget. Entries assigned to any widget are protected,
including widgets on a hidden page. SD file handles close after decoder reads.

Automatic release on leaving a page and lazy creation/destruction of many
widgets are still open. The implemented HA button releases only entries
that no widget currently references. Neither policy can eliminate LVGL's draw
buffer or all decoder working memory.

### Phase 7: Audio Streaming

The detailed working plan is [Optional SD MP3 Playback](docs/PlayMP3AudioPlan.md)
(reviewed 2026-09-17). It records the confirmed use of the FNK0104S's existing
audio connector, hardware and ESPHome findings, streaming memory estimates,
the proposed HA Select/media-player integration, implementation steps, effort,
and acceptance criteria for human contributors and AI agents. This remains
planning only; audio implementation and hardware validation are still pending.

Audio is a separate consumer of the same shared storage service and is not part
of the LVGL file-system driver. Add a streaming pipeline that reads small SD
chunks into a decoder and PCM/I2S ring buffer instead of loading a complete
track into RAM. WAV/PCM can be streamed directly; compressed formats such as
MP3 require their own decoder and bounded working buffers.

The Home Assistant selection model can be reused for audio files, but playback,
pause, end-of-file handling, decoder errors, and I2S output require a separate
implementation phase.

### Verification and Compatibility Requirements

The existing host tests and firmware builds cover parts of the implemented SD
image path; they do not prove all runtime behavior. For each future feature, add
focused tests for the paths it changes, including validation, short reads,
reader conflicts, cache references, and failed selections where applicable.
Keep HTTP upload/download regression checks and the legacy image YAML forms.
Run ESPHome validation and a full firmware build for code changes. The
[hardware test matrix](docs/HARDWARE_TEST_MATRIX.md) remains the source for
physical-device gaps, including endurance, concurrency, and card failures.

### Completed Initial Scope

The first milestones, established by commits `1b36591`, `70f00cd`, `894a46d`,
and `3655bcc`, now contain:

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
