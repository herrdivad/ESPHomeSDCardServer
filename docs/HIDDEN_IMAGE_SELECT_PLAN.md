# Plan: Image control with a hidden Select

Status: 2026-10-09. **Historical design plan; implementation now available.**

The implemented configuration and migration instructions are documented in the
[README](../README.md#hidden-image-selects-text-status-and-ha-automations).
The design below records the original proposal. The status pointer is held by
`CydSdImageSelect` rather than the widget binding, and its implementation lives
in `image_select.cpp`. The maintainer reported a successful physical-device test
of the hidden image Select and HA image action on 2026-10-10; remaining checks
are tracked in [the hardware test matrix](HARDWARE_TEST_MATRIX.md).

The goal was a third operating mode selectable for each LVGL image widget. The
user would still configure `select:`, with an additional Boolean `hidden` option.
Home Assistant would show the current image name as read-only text status; an
action in the HA automation editor would change the image. The user confirmed
this meaning of “Text Entry.”

| Configuration | Home Assistant representation | Image changes |
| --- | --- | --- |
| No `select:` | No image entity | Existing static initial image |
| `select:` without `hidden` or with `hidden: false` | Existing dropdown | Dropdown and existing automations |
| `select:` with `hidden: true` | Text status as `sensor.*`, no dropdown | New ESPHome API action |

`hidden` is an option of this component, not a general ESPHome or Home Assistant
switch. All three modes may coexist on one device and continue to share the image
list and cache.

Simply hiding an HA Select would leave a controllable Select. The proposal
therefore uses an internal ESPHome Select, a separate text sensor, and an API
action. Internal ESPHome entities are not published to HA; see the
[ESPHome entity options](https://esphome.io/components/text_sensor/).

The action can also be called manually or from scripts. “Automations only”
describes the intended control path without a Select control; it is not a
technical restriction on manual action calls. HA uses the same actions for
different call paths; see [HA actions](https://www.home-assistant.io/docs/scripts/perform-actions/).

## Proposed YAML interface

This fragment described the proposed extension and was not valid with the
component version at the time. The LVGL widget and remaining device
configuration were created separately, as before.

```yaml
api:
  # Preserve the existing API configuration, including encryption.

cyd_sd_http_server:
  display_images:
    images:
      - name: Day
        path: /images/day.bin
      - name: Night
        path: /images/night.bin
    widgets:
      - image_widget: sd_display_image
        initial_image: Day
        select:
          id: sd_display_image_select
          name: Display image
          hidden: true
```

- `hidden` belongs inside `select:` and defaults to `false`.
- The extension also applies to the existing singular `display_image` form.
- `id` remains the ID of a real ESPHome Select, so local references and Select
  automations remain usable.
- With `hidden: true`, this Select remains internal. The component automatically
  creates an external text sensor with the configured display name. No additional
  `text_sensor:` block is required.
- The text sensor gets its own ID, deterministically derived from the widget ID.
  Its display name and icon are copied; Select callbacks stay on the Select.
  Other entity options are handled and tested explicitly instead of copying the
  entire Select configuration to the sensor.
- The new mode requires `api:`. Existing configurations without a hidden Select
  gain no new API requirement.
- `hidden` is firmware configuration, not a mode that can be changed at runtime.

## Using it in Home Assistant

Each hidden widget automatically gets an action with one text parameter,
`option`. The proposed ESPHome-side action name is
`cyd_sd_set_<image_widget_id>`. The explicit widget ID provides a stable
association even when Select IDs are generated automatically.

For an example device named `display_panel`:

```yaml
# Proposed HA action call inside an automation:
action: esphome.display_panel_cyd_sd_set_sd_display_image
data:
  option: Night
```

The actual HA action name must be checked after device discovery. ESPHome
publishes user-defined API actions to HA and supports text parameters; see
[Native API: User-defined Actions](https://esphome.io/components/api/#user-defined-actions).

In the HA automation editor, a user could create a time, temperature, or light
trigger, select the relevant ESPHome action under “Perform action,” and enter an
image name. `choose` or templates could handle different states. This creates
no new image selection entity. The first implementation did not promise an
automatically generated dropdown of image names in the action form; `option`
is a text field.

Temperature and brightness thresholds should use separate on and off values
when needed. To restore the correct image promptly after a restart or
reconnection, the HA automation can also evaluate the current state when the
device becomes available. The device itself still loads `initial_image`.

## Implementation steps

1. **Extend the schema and code generation**
   - Use a shared Select schema with `hidden: false` for both image configuration
     forms in `components/cyd_sd_http_server/__init__.py`.
   - Keep the existing `CydSdImageSelect` in every Select mode. In the new mode,
     force internal registration and add the text sensor.
   - Enable text sensor and API action support only for the new mode. Keep the
     existing standalone file server buildable without LVGL or the API.
   - Reject missing `api:`, invalid Boolean values, and generated sensor ID or
     action name collisions with understandable validation errors.
   - Handle conflicting or non-transferable entity options explicitly; continue
     accepting previously valid YAML without `hidden: true` unchanged.

2. **Connect status to the existing image change path**
   - Add an optional text sensor reference to `DisplayImageWidget` in
     `cyd_sd_http_server.h`.
   - In `select_image()`, publish the assigned image name to this sensor after
     the existing selection process succeeds.
   - Use the same path for the initial image, local Select control, and API
     action. Do not change status optimistically before successful assignment.
   - Unknown options, missing files, and detected storage access conflicts leave
     the image and status unchanged. If the initial image fails, the sensor has
     no confirmed value; do not claim that an image loaded successfully.
   - Status confirms the source assignment made by the controller. The current
     streaming implementation cannot guarantee complete subsequent decoding:
     an existing corrupt file may fail only during rendering. Do not describe
     this status as confirmation that the image rendered successfully.

3. **Register the API action automatically**
   - Add a small adapter for each hidden widget. It checks `option` against the
     existing Select options and uses the existing Select control path. Accept
     only allowed image names, never arbitrary file paths.
   - Stay in the existing ESPHome/LVGL execution context. Do not introduce
     another SD access layer or HTTP requests for image changes.
   - The locally installed target version, ESPHome 2026.8.0, provides
     `api::CustomAPIDevice::register_service()`. This path requires
     `USE_API_USER_DEFINED_ACTIONS` and `USE_API_CUSTOM_SERVICES`. The component
     should enable this support conditionally, so `hidden: true` needs no
     manually written API actions or lambdas. Check the build alongside existing
     `api.actions`.
   - Action calls through this adapter have no result response. Errors are
     logged, and the text sensor confirms an accepted selection. A successful
     HA call alone does not confirm a successful image change.
   - Register once before API discovery. Reconnection must not create more
     actions or entities.

4. **Extend documentation and examples**
   - Document the three modes, HA action call, and error and restart behavior in
     the README and `docs/DIRECT_SD_IMAGE_WORKFLOW.md`.
   - Add an example with a static, visible, and hidden widget while retaining
     existing examples and controls.
   - Document HA examples for time of day and temperature-based selection.

## Compatibility and migration

Existing configurations retain their behavior with the default `hidden: false`.
The shared allowlist, centralized path validation, SD synchronization, PSRAM
cache, streaming fallback, uploads, and downloads remain available. Selecting
the same image name again must still reload a replaced file.

When an existing widget is deliberately changed to `hidden: true`, its external
`select.*` is replaced by a `sensor.*`. This changes the HA entity domain.
Existing `select.select_option` automations for that widget must migrate to the
new action. Orphaned HA registry entries may need manual cleanup. Stable
firmware IDs do not guarantee the same HA entity ID across this domain change.
Changing back to `hidden: false` also needs testing and documentation.

## Acceptance checks for the later implementation

- Schema and code generation: cover absent and explicitly false `hidden`, the
  new mode, mixed mode, singular form, missing API, and name collisions.
- Behavior: test correct widget association, allowed and unknown names, missing
  and busy files, initial image failures, repeated selection, and matching status.
- Check existing local Select references and callbacks in internal mode too.
- Host syntax tests do not yet fully cover the LVGL/API branch; add suitable
  test support and do not treat it as a replacement for a firmware build.
- Run host tests, `esphome config`, `compile --only-generate`, and a full build
  with ESPHome 2026.8.0 / ESP-IDF 5.5.5. Check both previous configurations and
  a new mixed configuration.
- On an explicitly intended device with HA, check text status without a
  dropdown, the action form in the UI, actual image changes, independent
  widgets, reconnection, mode switching, and unchanged web and cache behavior.
  Record hardware results separately from host and build results.

This plan covered no implementation or hardware change. Its technical basis was
the repository code and locally installed ESPHome 2026.8.0 sources at the time;
the linked online documentation may be newer.
