# Build environment freeze

Recorded on 2026-08-22 before implementation:

| Item | Value |
|---|---|
| ESPHome | 2026.8.0 |
| Python | 3.14.7 |
| Target | ESP32-S3 (`esp32-s3-devkitc-1`) |
| Framework | ESP-IDF 5.5.5 selected by ESPHome |
| Board memory | ESP32-S3R8, 8 MB octal PSRAM at 80 MHz |
| Compiler | Not recorded: the ESP-IDF archive is not present locally and network download is blocked |
| Socket pool | 19 generated: TCP 12, UDP 3, TCP listeners 4 |

Generated partition table:

| Name | Type | Subtype | Size |
|---|---|---|---:|
| `otadata` | data | ota | `0x2000` |
| `phy_init` | data | phy | `0x1000` |
| `app0` | app | ota_0 | `0x1C0000` |
| `app1` | app | ota_1 | `0x1C0000` |
| `nvs` | data | nvs | `0x70000` |

`esphome compile --only-generate` completed and produced the C++ source,
sdkconfig and partition table. A full compile was attempted twice. It stopped
before compilation because ESP-IDF 5.5.5 was not cached: the default cache was
outside the permitted workspace, and the workspace-local retry could not resolve
GitHub because network access is disabled. No hardware result is inferred from
configuration validation or source generation.
