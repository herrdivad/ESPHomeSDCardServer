# Hardware test matrix

No physical-device tests were run during this implementation. Every row remains
open until it is executed on the target FNK0104S and the result, card model,
ESPHome build and log excerpt are recorded.

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
| File/delete/directory error behavior | Not run |
| Traversal, long names, spaces and umlauts | Not run |
| Invalid API key causes no filesystem operation | Not run |
| One hour of transfers with both ports active | Not run |
| Heap, PSRAM, sockets, resets and watchdog logs inspected | Not run |

