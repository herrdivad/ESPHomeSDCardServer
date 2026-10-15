# Work plan: Reliable OTA file server for the CYD SD card

## 1. Purpose of this document

This document is a complete handoff to a new Codex, Claude Code, or human developer. The implementation should be a **clean-room rebuild** and must not depend on the existing, faulty repository, either functionally or structurally.

The new code should reliably transfer data over Wi-Fi to the SD card of an FNK0104S with an ESP32-S3R8. It should list, download, and delete files and create directories. Errors must be visible, verifiable, and reported correctly to the client.

The initial focus is exclusively on the platform actually in use:

- Board family: FNK0104S / CYD
- MCU: ESP32-S3R8
- Framework: ESP-IDF through ESPHome
- PSRAM: 8 MB octal PSRAM, 80 MHz
- SD connection: SDMMC, probably 4-bit
- Network: Wi-Fi through ESPHome
- Standard ESPHome web server: port 80
- New SD file server: separate port 8080

The first release must explicitly **make no Arduino compatibility claim**. Support for other boards or frameworks may be documented only after tests on physical hardware.

---

## 2. Known starting point

### 2.1 Current working ESPHome base

Line breaks were lost when the user pasted the configuration (`80MHz ota:` and `port: 80#`). The corrected relevant base is:

```yaml
esp32:
  board: esp32-s3-devkitc-1
  variant: esp32s3
  framework:
    type: esp-idf
    advanced:
      include_builtin_idf_components:
        - fatfs
        - spiffs
      disable_vfs_support_termios: false
      disable_vfs_support_select: false
      disable_vfs_support_dir: false
    sdkconfig_options:
      CONFIG_FATFS_LFN_STACK: "y"

# The FNK0104S uses an ESP32-S3R8 with octal PSRAM.
psram:
  mode: octal
  speed: 80MHz

ota:
  - platform: esphome

web_server:
  port: 80
```

`web_server_base:` is not a dependency of the rebuild and should not be configured explicitly unless the installed ESPHome version requires it for another reason. The official `web_server:` loads its internal base itself. The new SD server does not use that web server.

### 2.2 Known SDMMC pins

The planned pins are:

| Signal | GPIO |
|---|---:|
| CLK | GPIO38 |
| CMD | GPIO40 |
| DATA0 | GPIO39 |
| DATA1 | GPIO41 |
| DATA2 | GPIO48 |
| DATA3 | GPIO47 |

This assignment suggests 4-bit SDMMC mode. Verify it on the specific FNK0104S hardware before implementing HTTP.

Check the following:

- Are all six lines actually connected to the SD slot?
- Does the board have adequate external pull-ups on CMD and DAT0–DAT3?
- Are any GPIOs also used by the display, touch controller, backlight, or other components?
- Is 4-bit mode stable? Reduce to 1-bit with DATA0 for testing only if hardware problems are demonstrated.
- Does the slot provide stable 3.3 V, especially during write peaks?

Internal pull-ups may supplement the external ones. They do not replace pull-ups suitable for SDMMC.

### 2.3 Versions to record before starting

The new developer must record the actual build environment before coding:

```bash
esphome version
```

Also record from the build log:

- ESPHome version
- ESP-IDF version
- Target chip and board
- Compiler version
- Partition table used

Initially, development is pinned to these exact versions. Specify a version range only after separate build and hardware tests.

---

## 3. Required architecture decision

### 3.1 No ESPHome AsyncWebHandler for file transfers

For upload, download, and delete, the new file server must **not** depend on ESPHome's internal `web_server_base`, `AsyncWebHandler`, `handleUpload()`, multipart support, or `USE_WEBSERVER_OTA`.

That layer has changed across ESPHome versions, supports only some expected methods under ESP-IDF, and couples multipart processing to firmware OTA. These implicit dependencies should be avoided.

### 3.2 Separate native ESP-IDF HTTP server

The external component starts a second HTTP server directly with ESP-IDF's `esp_http_server`:

- ESPHome web server: port 80, unchanged
- SD file server: configurable port 8080
- Separate `ctrl_port`, such as 32769, to avoid a collision with the first HTTPD server
- Own URI handlers for GET, POST, DELETE, and OPTIONS
- Direct `httpd_req_recv()` access for streaming uploads
- Direct `httpd_resp_send_chunk()` access for streaming downloads

Firmware OTA through `platform: esphome` therefore remains independent of SD file transfers.

### 3.3 Programming languages

If ESPHome remains in use, the microcontroller runtime must be C++:

- Python in the external component: YAML schema, validation, and C++ code generation
- C++ on the ESP32: SDMMC, FATFS, HTTP server, and file operations
- HTML/CSS/JavaScript: browser interface
- Optional Python or C# on the PC: CLI uploader and tests

Python or C# can implement the PC client but cannot replace the ESPHome runtime without changing the entire firmware ecosystem. MicroPython or .NET nanoFramework would mean leaving ESPHome altogether and are outside this project.

---

## 4. New repository and file structure

Create a new repository or a completely new source directory. Do not copy C++ files from the old repository.

Proposed structure:

```text
cyd-sd-http-server/
├── README.md
├── LICENSE
├── components/
│   └── cyd_sd_http_server/
│       ├── __init__.py
│       ├── cyd_sd_http_server.h
│       ├── cyd_sd_http_server.cpp
│       ├── sd_storage.h
│       ├── sd_storage.cpp
│       ├── path_utils.h
│       ├── path_utils.cpp
│       └── web_ui.h
├── examples/
│   └── fnk0104s-esp-idf.yaml
├── scripts/
│   ├── upload_file.py
│   └── hardware_smoke_test.py
└── tests/
    ├── test_path_rules.py
    └── README.md
```

Keep the code small and understandable. Introduce extra abstraction layers only when they can be tested concretely.

---

## 5. Proposed YAML interface

Target configuration:

```yaml
external_components:
  - source:
      type: local
      path: components

cyd_sd_http_server:
  id: cyd_sd_server

  # Separate server, independent of web_server: on port 80.
  port: 8080
  control_port: 32769

  mount_point: /sdcard
  format_if_mount_failed: false
  bus_width: 4

  clk_pin: GPIO38
  cmd_pin: GPIO40
  data0_pin: GPIO39
  data1_pin: GPIO41
  data2_pin: GPIO48
  data3_pin: GPIO47

  # Never accept arbitrarily large files without notice.
  # 0 could optionally mean "no software limit" later; set an explicit limit initially.
  max_upload_size: 200MB

  # Must come from secrets.yaml. No insecure default.
  api_key: !secret sd_server_api_key
```

Proposed validation rules in `__init__.py`:

- ESP32 only
- ESP-IDF only
- Initially ESP32-S3 only
- Validate the port range
- `port` and `control_port` must differ
- 1-bit mode requires CLK, CMD, and DATA0
- 4-bit mode also requires DATA1, DATA2, and DATA3
- `format_if_mount_failed` must default to `false`
- Require an `api_key` with a minimum length, such as 32 characters
- Use a fixed `/sd` URL prefix or a configurable but normalized one
- Return a clear `cv.Invalid` message for unsupported configurations
- Set a concrete minimum version only after identifying and testing the actual ESPHome version

The Python module must not pretend to implement runtime logic. Its only jobs are validation, code generation, and adding ESP-IDF components and defines that are actually needed.

---
## 6. SDMMC and FATFS storage layer

### 6.1 Initialization

The storage layer uses native ESP-IDF APIs:

- `SDMMC_HOST_DEFAULT()`
- `SDMMC_SLOT_CONFIG_DEFAULT()`
- Assign pins explicitly
- Set `slot_config.width = 4` or 1
- Add `SDMMC_SLOT_FLAG_INTERNAL_PULLUP` if needed
- Use `esp_vfs_fat_sdmmc_mount()` on `/sdcard`
- Set `format_if_mount_failed = false` as the safe default
- Choose a sensible `max_files` value, such as 8

A mount failure must:

- Be logged with the specific `esp_err_t`
- Mark the ESPHome component status as failed
- Either prevent the HTTP server from starting or make it return an unambiguous 503
- Never trigger automatic formatting unless the user explicitly enabled it

### 6.2 Result and error contract

No public file operation that can fail may return `void`. Provide an internal result object, for example:

```cpp
enum class StorageError {
  NONE,
  NOT_MOUNTED,
  INVALID_PATH,
  NOT_FOUND,
  ALREADY_EXISTS,
  NOT_A_FILE,
  NOT_A_DIRECTORY,
  OPEN_FAILED,
  READ_FAILED,
  WRITE_FAILED,
  SHORT_WRITE,
  CLOSE_FAILED,
  RENAME_FAILED,
  DELETE_FAILED,
  NO_SPACE,
  BUSY,
};

struct StorageResult {
  StorageError error;
  int system_errno;
  size_t bytes_processed;
};
```

Every error must be machine-readable over HTTP and appear in the ESPHome log with its path, operation, and `errno`.

### 6.3 Write rules

- Check `fwrite()` for the exact number of bytes written, not only zero versus nonzero.
- Check `fclose()`.
- Do not visibly replace the target file before successful completion.
- Initially write uploads to a temporary file on the same FAT partition, such as `/.upload-<id>.part`.
- After receiving the complete upload: flush, close, check size, optionally check a checksum, then rename.
- Replace an existing target only when `overwrite=true` was explicitly requested.
- If an upload fails, leave any existing target unchanged.
- Remove temporary files on error.
- Detect old `.part` files at boot and either remove them with a clear log message or show them in a diagnostic list.

Note: Test how FATFS `rename()` behaves when replacing an existing file on the ESP-IDF version in use. If `rename()` does not replace an existing target, a safe, documented two-step strategy is needed. Explicitly test power loss between the steps.

### 6.4 Concurrency

- Protect FATFS access with a dedicated mutex.
- Initially allow no more than one concurrent upload.
- Reject a second upload with HTTP 409 `BUSY`.
- Do not use global file handles without a clear owner.
- Server shutdown and component teardown must close open files.
- Interrupted connections must not leave a `FILE *` open.
- An upload context needs a start time, expected size, received size, temporary path, target path, and error state.

### 6.5 Path safety

A central function must validate and normalize every external path:

- Require an absolute path inside the virtual SD root, such as `/images/a.jpg`.
- Reject `.` and `..` completely.
- Reject backslashes, NUL, and control characters.
- Normalize or reject repeated slashes.
- Never concatenate the mount point with an unchecked request.
- Check maximum total path length and segment length.
- Allow an empty path only where it means `/`.
- Forbid deletion of the mount root.
- Prevent symlink traversal; FATFS normally has no symlinks, but document that assumption.
- Never emit file names as unescaped HTML.

Implement path normalization as a separately testable function.

---

## 7. HTTP API contract

Base URL:

```text
http://<device>:8080/sd
```

Serve the web interface from the same port to avoid a CORS dependency.

### 7.1 Authentication

All API endpoints require:

```http
X-API-Key: <long random key>
```

Requirements:

- Read the key from `secrets.yaml`.
- Compare keys in approximately constant time.
- Never log the key.
- Have the browser interface request it and store it at most in `sessionStorage`, not persistently.
- HTTP transmits the key without encryption. Document that the server must run only on a trusted, segmented LAN.
- Do not enable permissive CORS by default.
- Respond to OPTIONS only as needed by specifically allowed clients.

### 7.2 Consistent JSON format

Success:

```json
{
  "ok": true,
  "operation": "upload",
  "path": "/images/photo.jpg",
  "bytes": 123456
}
```

Error:

```json
{
  "ok": false,
  "operation": "upload",
  "path": "/images/photo.jpg",
  "error": "SHORT_WRITE",
  "message": "SD card accepted fewer bytes than requested",
  "errno": 28,
  "expected_bytes": 123456,
  "processed_bytes": 98304
}
```

No handler may report success until the underlying operation has demonstrably completed successfully.

### 7.3 Endpoints

#### Health

```http
GET /sd/api/health
```

The response includes at least:

- Mounted: yes or no
- Card type, if available
- Total, used, and free storage
- Server version
- Whether an upload is active
- Uptime

#### List a directory

```http
GET /sd/api/list?path=/
```

Entries in the response include:

- Name
- Full virtual path
- File or directory type
- Size
- Optional modification time, if FATFS provides a meaningful one

Do not recurse without limits. For very large directories, add pagination or a clearly documented hard limit.

#### Download a file

```http
GET /sd/api/file?path=/images/photo.jpg
```

- Stream the file in fixed blocks, for example 8 KiB.
- Never load the complete file into RAM or PSRAM.
- Send a correct `Content-Length` when known.
- Default safely to `Content-Type: application/octet-stream`.
- Use a safely encoded file name in `Content-Disposition`.
- Log read errors and client disconnects.

#### Upload a file

```http
POST /sd/api/file?path=/images/photo.jpg&overwrite=false
Content-Type: application/octet-stream
Content-Length: 123456
X-API-Key: ...

<binary data>
```

Implementation:

1. Authenticate.
2. Decode and validate the query path.
3. Require `Content-Length`.
4. Check the size limit and free storage.
5. Open a temporary file.
6. Receive the body in a loop with `httpd_req_recv()`.
7. Write each block completely to SD and update counters.
8. Retry or abort appropriately on socket timeout; never loop forever.
9. On client disconnect, close cleanly and remove the temporary file.
10. Finalize and rename only after an exact size check.
11. Send JSON success.

Multipart is intentionally unnecessary.

#### Delete a file

Primary endpoint:

```http
DELETE /sd/api/file?path=/images/photo.jpg
```

An additional POST fallback may optionally be offered:

```http
POST /sd/api/delete?path=/images/photo.jpg
```

The new native HTTPD server registers DELETE itself, so it does not depend on ESPHome's web server methods. This endpoint does not delete directories.

#### Create a directory

```http
POST /sd/api/directory?path=/images
```

The body may be empty if `Content-Length: 0` is handled correctly. Alternatively, use a small JSON body.

#### Delete a directory

Add only after the MVP:

```http
DELETE /sd/api/directory?path=/images
```

Default to empty directories only. Do not allow recursive deletion without a separate, explicitly confirmed API.

### 7.4 HTTP status codes

Because the new server uses ESP-IDF directly, set appropriate status lines explicitly:

- 200: successful operation
- 204: successful deletion without a JSON body
- 400: invalid request or path
- 401: missing or incorrect API key
- 404: file or directory not found
- 409: target exists, upload is active, or resource is busy
- 411: missing Content-Length
- 413: upload exceeds configured limit
- 422: semantically invalid operation
- 500: internal error
- 503: SD card unavailable
- 507: insufficient storage

If an error has a JSON body, set `Content-Type: application/json`.

---

## 8. Browser interface

Serve the interface as its own static HTML page on port 8080:

```text
GET /sd/
```

Minimum features:

- API key input
- Mount status and free storage display
- Directory navigation
- File selection and upload
- Upload progress using `XMLHttpRequest.upload.onprogress`
- Clear success and error messages, including the server response
- Automatic list refresh after a successful operation
- Download link
- Confirmation dialog before deletion
- Directory creation
- Disabled buttons while an operation is running

Security rules:

- Insert file names only through `textContent`, never unescaped `innerHTML`.
- No external JavaScript or CSS CDNs.
- Do not store credentials persistently.
- Check API responses for `response.ok` and the expected JSON.
- Display network and HTTP errors separately.

The interface is a convenience, not the only access path. All functions must also work through the documented HTTP API.

---

## 9. PC clients

### 9.1 curl reference

Upload:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -H "Content-Type: application/octet-stream" \
  --data-binary "@./photo.jpg" \
  "http://device.local:8080/sd/api/file?path=/photo.jpg&overwrite=false"
```

Download:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -o photo.downloaded.jpg \
  "http://device.local:8080/sd/api/file?path=/photo.jpg"
```

Delete:

```bash
curl --fail-with-body --show-error \
  -H "X-API-Key: $SD_API_KEY" \
  -X DELETE \
  "http://device.local:8080/sd/api/file?path=/photo.jpg"
```

Real clients must URL-encode paths.

### 9.2 Python uploader

A small `scripts/upload_file.py` should:

- Accept host, local path, and target path as arguments
- Read the API key from an environment variable or secure input
- Stream the file rather than reading it in full
- Set a timeout
- Display the server's JSON
- Exit with a nonzero code on failure
- Optionally download the file again after upload and compare SHA-256

A C# client can later use the same API contract through `HttpClient`. No separate firmware support is required.

---

## 10. Logging and diagnostics

Give every operation a short request ID. Never log API keys or file contents.

Example of a successful upload:

```text
[I][cyd_sd_http]: [req=42] upload start path=/images/photo.jpg bytes=123456 overwrite=false
[D][cyd_sd_storage]: [req=42] temporary file=/.upload-42.part
[I][cyd_sd_http]: [req=42] upload complete path=/images/photo.jpg bytes=123456 duration_ms=1840
```

Example of an error:

```text
[E][cyd_sd_storage]: [req=43] write failed path=/.upload-43.part requested=8192 written=4096 errno=28
[W][cyd_sd_http]: [req=43] upload aborted target=/video.bin received=10485760 committed=10481664
```

At startup, log:

- Server address and port
- Control port
- Mount point
- SDMMC mode and pins
- Whether a card was detected
- Total and free storage
- Upload limit
- Whether authentication is enabled, never the key
- `format_if_mount_failed`

If SD is not mounted, the log must not say “server ready” without qualification.

---

## 11. Implementation phases

### Phase 0: Clean-room project and version freeze

- Create a new repository.
- Choose a license.
- Do not copy files from the old repository.
- Document ESPHome, ESP-IDF, and compiler versions.
- Create a minimal build configuration for FNK0104S.
- Ensure serial logs are available.

Acceptance criterion: An empty external component compiles and loads on the device.

### Phase 1: SDMMC and storage layer only

- Initialize SDMMC with the six pins.
- Mount without automatic formatting.
- Log card information and free storage.
- Run internal test operations: create a file, read it exactly, rename it, and delete it.
- Check all return values.
- Implement path normalization with tests.

Acceptance criterion: A deterministic boot self-test or temporarily enabled test proves read, write, and delete operations on physical hardware. Remove test files afterwards.

### Phase 2: Separate HTTPD server and health/list

- Start a second HTTPD server on port 8080.
- Prevent a control-port collision.
- Implement API key checking.
- Implement `/health` and `/list`.
- Support clean server shutdown.

Acceptance criterion: Ports 80 and 8080 run concurrently and remain stable for at least one hour.

### Phase 3: Streaming upload

- Implement a raw binary POST.
- Check Content-Length, upload limit, and free storage.
- Implement the temporary file and final commit.
- Handle client disconnects and timeouts.
- Reject concurrent uploads.

Acceptance criterion: Transfers of 1 KiB, 1 MiB, 20 MiB, and a larger realistic file complete without a reboot and match byte for byte.

### Phase 4: Streaming download and delete

- Download in fixed blocks.
- Add the DELETE file endpoint.
- Support directory creation.
- Set correct MIME and download headers.

Acceptance criterion: An automated upload → download → SHA-256 comparison → delete sequence works.

### Phase 5: Browser interface

- Build the UI without external assets.
- Show progress.
- Show error dialogs.
- Refresh automatically.
- Display arbitrary file names safely.

Acceptance criterion: All MVP operations work in a browser without developer tools, and every error is visible.

### Phase 6: Hardening, documentation, and release

- Test cancellation, power loss, full cards, and concurrency.
- Monitor memory and socket use.
- State exact platform support in the README.
- Show known limitations prominently in the main README.
- Do not advertise untested frameworks.
- Compile the complete example configuration.

Acceptance criterion: Document the test matrix and put known limitations at the top level of the README.

---
## 12. Hardware and integration test matrix

Run at least the following tests on physical hardware and document the results:

| Test | Expected result |
|---|---|
| Boot without SD card | Clear error, no crash, health reports unavailable |
| Boot with a valid FAT32 card | Mount succeeds |
| Unknown or damaged file system | No automatic formatting |
| Empty file | Defined support or clear 422 error |
| 1 KiB upload | Identical SHA-256 |
| 1 MiB upload | Identical SHA-256 |
| 20 MiB upload | Identical SHA-256, no large RAM increase |
| File exceeds the limit | 413 before writing |
| Insufficient free storage | 507, no target file |
| Existing file, overwrite=false | 409, old file unchanged |
| Existing file, overwrite=true | New file correct after complete commit |
| Client closes during upload | Target unchanged, handle closed, `.part` cleaned up |
| Wi-Fi disconnect | No crash, later upload works |
| Power loss during upload | Existing target preferably unchanged, leftover file detected |
| Two simultaneous uploads | One succeeds, second gets 409 BUSY |
| Large file download | Streamed, identical SHA-256 |
| Delete an existing file | File removed, correct response |
| Delete a nonexistent file | 404 |
| Delete root or a directory as a file | Safely rejected |
| Path with `../` | 400, no operation outside mount root |
| Very long file name | Accepted or rejected predictably according to FATFS limits |
| Spaces or non-ASCII characters in a name | Correctly URL-encoded and displayed safely |
| Incorrect API key | 401, no filesystem operation |
| One hour of repeated transfers | No leaks or watchdog resets; both web servers reachable |

Also observe through serial logs and, if applicable, ESPHome debug sensors:

- Free heap before and after large transfers
- Free PSRAM before and after large transfers
- Open sockets
- Reboots and reset cause
- Watchdog messages
- FATFS errors

---

## 13. Automated smoke test

`scripts/hardware_smoke_test.py` should run against a physical device and at least:

1. Fetch health.
2. Create a random local test file.
3. Compute its local SHA-256.
4. Upload the file.
5. Check the directory listing.
6. Download the file again.
7. Compare SHA-256.
8. Delete the file.
9. Check that downloading it then returns 404.
10. Return an unambiguous exit code.

Create local test data in a temporary directory and remove it after success. Do not overwrite or delete user files.

---

## 14. Definition of done for the first release

The MVP is complete only when all of these conditions are met:

- The example YAML compiles with the documented ESPHome version.
- The FNK0104S mounts the SD card reproducibly using the documented pins.
- The ESPHome web server on port 80 and SD server on port 8080 run concurrently and stably.
- Upload uses neither multipart nor web server OTA.
- Large files are streamed and never held in full in RAM.
- Downloads are streamed too.
- Every file I/O return value is checked.
- Failed uploads never report success.
- Failed deletes never report success.
- Aborted uploads do not damage an existing target file.
- Tests rule out path traversal.
- The API key is required and never logged.
- The browser, curl, and Python smoke test work.
- Upload and download checksums match.
- The main README prominently identifies supported and unsupported platforms.
- Known limitations are not hidden in subdirectories.
- The documented hardware test matrix has actually been run.

---

## 15. Deliberate exclusions from the first release

- Arduino framework
- Other ESP32 variants
- SPI-based SD slots
- SD card hotplug during operation
- Recursive deletion of whole directory trees
- Multiple concurrent uploads
- WebDAV, FTP, SMB, or SFTP
- TLS directly on the ESP
- Public access from the internet
- Integration into Home Assistant's media browser
- Automatic formatting of unknown cards

These may be added later, but each requires its own design and tests.

---

## 16. Risks and open decisions

Resolve before or during Phase 1:

1. **Pin assignment:** Confirm the stated SDMMC pins on the specific FNK0104S.
2. **Pull-ups:** Check the schematic or measurements; internal pull-ups alone may be insufficient.
3. **FAT format:** Which card size and file system are actually used? Initially target FAT32; support exFAT only after demonstrating toolchain support.
4. **Rename semantics:** Test the atomic replacement process on the specific FATFS version.
5. **HTTPD resources:** A second server needs a task, stack, and sockets. Monitor both ports and `CONFIG_LWIP_MAX_SOCKETS`.
6. **Upload limit:** Agree with the user on the maximum file size actually needed; 200 MB is only a starting example.
7. **Empty files:** Decide whether an upload with `Content-Length: 0` counts as a valid empty file.
8. **Timestamps:** Without RTC/NTP, file times may be unusable; do not promise them as a feature.
9. **Power-loss safety:** FATFS does not provide a complete transaction. Phrase guarantees precisely according to tests.
10. **API key in browser:** Weigh convenience against security; for now use only `sessionStorage`.

Do not silently assume answers to open questions. Document them in the README or ADR files.

---

## 17. Starting instructions for the next developer

The next developer should start in this order:

1. Read this document in full.
2. Create a new repository or directory; do not copy the old source code.
3. Determine and document the actual ESPHome version.
4. Compile the corrected base YAML without the old `sd_mmc_card` and `sd_file_server`.
5. Verify the pin assignment and SDMMC mount independently on hardware.
6. Implement the second native HTTPD server only after a successful mount.
7. Implement health and list first, then raw upload, then download and delete, and finally the UI.
8. Run the stated hardware acceptance checks after each phase.
9. Never send a success response before the actual operation has been fully verified.
10. Never claim support that has not been tested on physical hardware.
