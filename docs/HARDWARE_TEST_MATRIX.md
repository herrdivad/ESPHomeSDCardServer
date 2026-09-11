# Hardware test matrix

No physical-device tests were run from this repository environment. Rows marked
as passed are based on logs supplied from the user's FNK0104S; all other rows
remain open until they are executed on the target hardware and recorded.

| Test | Status |
|---|---|
| Boot without SD card reports unavailable without crashing | Not run |
| FAT32 mount over documented 4-bit pins | Not run |
| Invalid filesystem is not formatted | Not run |
| Empty, 1 KiB, 1 MiB, 20 MiB and large realistic uploads | Not run |
| Size limit and full-card handling | Not run |
| Overwrite false/true and interrupted overwrite | Not run |
| Client/WLAN interruption cleans temporary file | Not run |
| Two uploads cause one 409 response | Not run |
| Large streamed download and SHA-256 match | Not run |
| Initial baseline JPEG renders using `loop_task_stack_size: 16384` | Passed in user-supplied 0.3.0 log |
| Home Assistant Select switches between mapped JPEG files | Not run |
| Missing mapped image keeps the previous image and selection | Not run |
| First cache miss decodes one 480 x 320 image into PSRAM | Passed: 307,200 bytes, 277 ms decoder/cache copy |
| Re-selecting a cached image logs `HIT` without another SD decode | Not run |
| Leaving and returning to the image page redraws in 300-500 ms or less | Passed: approximately 156 ms reported |
| Multiple widget initial images preload without an `on_boot` lambda | Not run |
| Two widgets sharing one path use one decoded PSRAM allocation | Not run |
| Independent widget Select entities update only their own widget/state | Not run |
| Static widget without `select` creates no Home Assistant entity | Not run |
| Cache eviction never frees an entry referenced by any widget | Not run |
| Fourth full-screen image evicts the least recently used inactive entry | Not run |
| Decoded cache remains at or below the configured 1 MB limit | Not run |
| A configured 4 MB cache stays within its budget and leaves enough PSRAM for LVGL | Not run |
| `max_size: 0` and unavailable PSRAM use the `S:/` streaming fallback | Not run |
| Upload/overwrite/delete invalidates mapped cached image data | Not run |
| Browser download still works while direct-image support is enabled | Not run |
| Upload/delete of an actively read image returns 409 | Not run |
| Repeated image switching does not grow heap/PSRAM beyond configured caches | Not run |
| File/delete/directory error behavior | Not run |
| Traversal, long names, spaces and umlauts | Not run |
| Invalid API key causes no filesystem operation | Not run |
| One hour of transfers with both ports active | Not run |
| Heap, PSRAM, sockets, resets and watchdog logs inspected | Not run |
