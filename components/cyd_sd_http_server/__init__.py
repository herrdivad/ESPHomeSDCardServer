"""ESPHome code generation for the native CYD SD HTTP server."""

import esphome.codegen as cg
from esphome import pins
from esphome.components import button, esp32, select, sensor, socket, text_sensor
from esphome.components.lvgl import defines as lvgl_defines
from esphome.components.lvgl.types import lv_image_t
import esphome.config_validation as cv
import esphome.final_validate as fv
from esphome.const import (
    CONF_DEVICE_ID,
    CONF_DISABLED_BY_DEFAULT,
    CONF_ENTITY_CATEGORY,
    CONF_ICON,
    CONF_ID,
    CONF_INTERNAL,
    CONF_MQTT_ID,
    CONF_NAME,
    CONF_PATH,
    CONF_WEB_SERVER,
    Framework,
)

DEPENDENCIES = ["esp32"]
CODEOWNERS = []

CONF_API_KEY = "api_key"
CONF_BUS_WIDTH = "bus_width"
CONF_CACHE = "cache"
CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_CONTROL_PORT = "control_port"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_FORMAT_IF_MOUNT_FAILED = "format_if_mount_failed"
CONF_DISPLAY_IMAGE = "display_image"
CONF_DISPLAY_IMAGES = "display_images"
CONF_DRIVE_LETTER = "drive_letter"
CONF_IMAGE_WIDGET = "image_widget"
CONF_FIT = "fit"
CONF_IMAGES = "images"
CONF_INITIAL_IMAGE = "initial_image"
CONF_INTERNAL_PULLUPS = "internal_pullups"
CONF_FILE_BUFFER_SIZE = "file_buffer_size"
CONF_MEMORY = "memory"
CONF_MAX_SIZE = "max_size"
CONF_MAX_UPLOAD_SIZE = "max_upload_size"
CONF_MOUNT_POINT = "mount_point"
CONF_PORT = "port"
CONF_SELECT = "select"
CONF_STRATEGY = "strategy"
CONF_URL_PREFIX = "url_prefix"
CONF_WIDGETS = "widgets"
CONF_HIDDEN = "hidden"
CONF_IMAGE_STATUS = "image_status"
CONF_DIAGNOSIS = "diagnosis"

DIAGNOSIS_SENSORS = (
    ("psram_total", "SD PSRAM Total", "set_psram_total_sensor"),
    ("psram_free", "SD PSRAM Free", "set_psram_free_sensor"),
    ("psram_largest_free", "SD PSRAM Largest Free Block", "set_psram_largest_free_sensor"),
    ("image_cache_limit", "SD Image Cache Limit", "set_image_cache_limit_sensor"),
    ("image_cache_used", "SD Image Cache Used", "set_image_cache_used_sensor"),
    ("image_cache_reclaimable", "SD Image Cache Reclaimable", "set_image_cache_reclaimable_sensor"),
)

MAX_DECODED_CACHE_SIZE = 4 * 1024 * 1024
MAX_FILE_BUFFER_SIZE = 64 * 1024

cyd_ns = cg.esphome_ns.namespace("cyd_sd_http_server")
CydSdHttpServer = cyd_ns.class_("CydSdHttpServer", cg.Component)
CydSdImageSelect = cyd_ns.class_("CydSdImageSelect", select.Select)
CydSdCacheReleaseButton = cyd_ns.class_("CydSdCacheReleaseButton", button.Button)


def _display_widgets(config):
    if display := config.get(CONF_DISPLAY_IMAGES):
        if not isinstance(display, dict):
            return []
        widgets = display.get(CONF_WIDGETS, [])
        return widgets if isinstance(widgets, list) else [widgets]
    if legacy := config.get(CONF_DISPLAY_IMAGE):
        return [legacy]
    return []


def AUTO_LOAD(config):
    components = ["network", "select", "socket"]
    if config.get(CONF_DISPLAY_IMAGES) or config.get(CONF_DISPLAY_IMAGE):
        components.extend(("button", "sensor"))
    # AUTO_LOAD runs before schema validation, including ensure_list conversion.
    for widget in _display_widgets(config):
        if not isinstance(widget, dict):
            continue
        selection = widget.get(CONF_SELECT)
        if not isinstance(selection, dict):
            continue
        try:
            hidden = cv.boolean(selection.get(CONF_HIDDEN, False))
        except cv.Invalid:
            # Report malformed values through the normal schema error path.
            continue
        if hidden:
            components.append("text_sensor")
            break
    return components


def _hidden_action_name(widget):
    return f"cyd_sd_set_{widget[CONF_IMAGE_WIDGET]}".lower()


def _api_key(value):
    value = cv.string_strict(value)
    if len(value.encode("utf-8")) < 32:
        raise cv.Invalid("api_key must contain at least 32 UTF-8 bytes")
    return value


def _mount_point(value):
    value = cv.string_strict(value)
    if value != "/sdcard":
        raise cv.Invalid("The first release supports only mount_point: /sdcard")
    return value


def _url_prefix(value):
    value = cv.string_strict(value).rstrip("/")
    if value != "/sd":
        raise cv.Invalid("The first release supports only url_prefix: /sd")
    return value


def _drive_letter(value):
    value = cv.string_strict(value).upper()
    if len(value) != 1 or value < "A" or value > "Z":
        raise cv.Invalid("drive_letter must be one ASCII letter from A to Z")
    return value


def _image_path(value):
    value = cv.string_strict(value)
    if not value.startswith("/"):
        raise cv.Invalid("SD image paths must start with /")
    if "\\" in value or "\x00" in value:
        raise cv.Invalid("SD image path contains an invalid character")
    segments = value.split("/")[1:]
    if not segments or any(segment in ("", ".", "..") for segment in segments):
        raise cv.Invalid("SD image path must be normalized and may not contain empty, . or .. segments")
    if any(".cyd-upload-" in segment or ".cyd-backup-" in segment for segment in segments):
        raise cv.Invalid("SD image path may not reference internal temporary files")
    if len(value.encode("utf-8")) > 255:
        raise cv.Invalid("SD image path is longer than 255 UTF-8 bytes")
    if not value.lower().endswith((".jpg", ".jpeg", ".bin")):
        raise cv.Invalid("SD images must use .jpg, .jpeg or LVGL .bin")
    return value


IMAGE_CACHE_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_MAX_SIZE, default="1MB"): cv.All(
            cv.validate_bytes,
            cv.int_range(min=0, max=MAX_DECODED_CACHE_SIZE),
        ),
        cv.Optional(CONF_FILE_BUFFER_SIZE, default="16kB"): cv.All(
            cv.validate_bytes,
            cv.int_range(min=0, max=MAX_FILE_BUFFER_SIZE),
        ),
        # The first implementation deliberately has one safe policy. Keeping
        # these values explicit in YAML documents that decoded frames never
        # consume internal RAM and that eviction order is deterministic.
        cv.Optional(CONF_MEMORY, default="psram"): cv.one_of(
            "psram", lower=True
        ),
        cv.Optional(CONF_STRATEGY, default="lru"): cv.one_of("lru", lower=True),
    }
)


IMAGE_MAPPING_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_NAME): cv.string_strict,
        cv.Required(CONF_PATH): _image_path,
    }
)


IMAGE_SELECT_SCHEMA = select.select_schema(CydSdImageSelect).extend(
    {cv.Optional(CONF_HIDDEN, default=False): cv.boolean}
)


DISPLAY_WIDGET_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_IMAGE_WIDGET): cv.All(
            cv.use_id(lv_image_t), cv.requires_component("lvgl")
        ),
        cv.Optional(CONF_INITIAL_IMAGE): cv.string_strict,
        cv.Optional(CONF_FIT): cv.one_of("none", "contain", "cover", "stretch", lower=True),
        cv.Optional(CONF_SELECT): IMAGE_SELECT_SCHEMA,
    }
)


# Backward-compatible single-widget schema used by version 0.2/0.3 YAML.
DISPLAY_IMAGE_SCHEMA = cv.Schema(
    {
        cv.Required(CONF_IMAGE_WIDGET): cv.All(
            cv.use_id(lv_image_t), cv.requires_component("lvgl")
        ),
        cv.Optional(CONF_DRIVE_LETTER, default="S"): _drive_letter,
        cv.Required(CONF_IMAGES): cv.All(
            cv.ensure_list(IMAGE_MAPPING_SCHEMA),
            cv.Length(min=1),
        ),
        cv.Optional(CONF_INITIAL_IMAGE): cv.string_strict,
        cv.Optional(CONF_FIT): cv.one_of("none", "contain", "cover", "stretch", lower=True),
        cv.Optional(CONF_CACHE, default={}): IMAGE_CACHE_SCHEMA,
        cv.Required(CONF_SELECT): IMAGE_SELECT_SCHEMA,
    }
)


DISPLAY_IMAGES_SCHEMA = cv.Schema(
    {
        cv.Optional(CONF_DRIVE_LETTER, default="S"): _drive_letter,
        cv.Required(CONF_IMAGES): cv.All(
            cv.ensure_list(IMAGE_MAPPING_SCHEMA),
            cv.Length(min=1),
        ),
        cv.Optional(CONF_CACHE, default={}): IMAGE_CACHE_SCHEMA,
        cv.Required(CONF_WIDGETS): cv.All(
            cv.ensure_list(DISPLAY_WIDGET_SCHEMA),
            cv.Length(min=1),
        ),
    }
)


def _validate_display_images(config):
    legacy = config.get(CONF_DISPLAY_IMAGE)
    display = config.get(CONF_DISPLAY_IMAGES)
    if legacy is not None and display is not None:
        raise cv.Invalid("Use either display_image or display_images, not both")
    if legacy is not None:
        display = legacy
        widgets = [legacy]
        prefix = CONF_DISPLAY_IMAGE
    elif display is not None:
        widgets = display[CONF_WIDGETS]
        prefix = CONF_DISPLAY_IMAGES
    else:
        return config

    names = [entry[CONF_NAME] for entry in display[CONF_IMAGES]]
    paths = [entry[CONF_PATH] for entry in display[CONF_IMAGES]]
    if len(names) != len(set(names)):
        raise cv.Invalid(f"{prefix} image names must be unique")
    if len(paths) != len(set(paths)):
        raise cv.Invalid(f"{prefix} image paths must be unique")
    if any(not name or len(name.encode("utf-8")) > 64 for name in names):
        raise cv.Invalid(f"{prefix} names must contain 1 to 64 UTF-8 bytes")

    widget_ids = [str(widget[CONF_IMAGE_WIDGET]) for widget in widgets]
    if len(widget_ids) != len(set(widget_ids)):
        raise cv.Invalid(f"{prefix} image_widget IDs must be unique")
    for widget in widgets:
        if CONF_INITIAL_IMAGE not in widget:
            widget[CONF_INITIAL_IMAGE] = names[0]
        elif widget[CONF_INITIAL_IMAGE] not in names:
            raise cv.Invalid(
                f"{prefix} widget initial_image must match one of the configured image names"
            )
        selection = widget.get(CONF_SELECT, {})
        if selection.get(CONF_HIDDEN):
            cv.requires_component("api")(True)
            # Preserve presentation metadata, but not Select callbacks or MQTT commands.
            status = {
                key: selection[key]
                for key in (
                    CONF_NAME, CONF_ICON, CONF_ENTITY_CATEGORY,
                    CONF_DISABLED_BY_DEFAULT, CONF_DEVICE_ID, CONF_WEB_SERVER,
                )
                if key in selection
            }
            status[CONF_ID] = f"cyd_sd_{widget[CONF_IMAGE_WIDGET]}_image_status"
            status[CONF_INTERNAL] = False
            widget[CONF_IMAGE_STATUS] = text_sensor.text_sensor_schema()(status)
            selection[CONF_INTERNAL] = True
            selection.pop(CONF_MQTT_ID, None)

    # LVGL gates RGB565 software transforms on RGB565A8 support as well.
    if any(widget.get(CONF_FIT) not in (None, "none") for widget in widgets):
        lvgl_defines.add_define("LV_DRAW_SW_SUPPORT_RGB565A8")

    diagnosis = {}
    for key, name, _ in DIAGNOSIS_SENSORS:
        diagnosis[key] = sensor.sensor_schema(
            unit_of_measurement="B",
            accuracy_decimals=0,
            device_class="data_size",
            state_class="measurement",
            entity_category="diagnostic",
        )({CONF_ID: f"{config[CONF_ID]}_{key}", CONF_NAME: name})
    diagnosis["release_button"] = button.button_schema(
        CydSdCacheReleaseButton,
        entity_category="diagnostic",
    )({
        CONF_ID: f"{config[CONF_ID]}_release_unused_image_cache",
        CONF_NAME: "Release Unused SD Image Cache",
    })
    config[CONF_DIAGNOSIS] = diagnosis

    # ESPHome 2026.8 uses LVGL 9.5. Its built-in Tiny JPEG decoder performs
    # incremental 8x8-tile decoding and registers itself when this define is on.
    lvgl_defines.add_lv_use("tjpgd")
    lvgl_defines.add_lv_use("image")
    return config


def _final_validate_hidden_actions(config):
    full_config = fv.full_config.get()
    api_config = full_config.get("api", {})
    action_names = {
        action["action"] for action in api_config.get("actions", [])
    }
    for widget in _display_widgets(config):
        if widget.get(CONF_SELECT, {}).get(CONF_HIDDEN):
            name = _hidden_action_name(widget)
            if name in action_names:
                raise cv.Invalid(f"Hidden image action name conflicts with another action: {name}")
            action_names.add(name)
            # API source filtering also reads this flag; defines alone omit user_services.cpp.
            api_config["custom_services"] = True
    return config


FINAL_VALIDATE_SCHEMA = _final_validate_hidden_actions


def _validate_config(config):
    if config[CONF_PORT] == config[CONF_CONTROL_PORT]:
        raise cv.Invalid("port and control_port must be different")
    if config[CONF_BUS_WIDTH] == 4:
        missing = [
            key
            for key in (CONF_DATA1_PIN, CONF_DATA2_PIN, CONF_DATA3_PIN)
            if key not in config
        ]
        if missing:
            raise cv.Invalid(
                "bus_width: 4 requires data1_pin, data2_pin and data3_pin"
            )
    elif any(
        key in config for key in (CONF_DATA1_PIN, CONF_DATA2_PIN, CONF_DATA3_PIN)
    ):
        raise cv.Invalid("data1_pin, data2_pin and data3_pin require bus_width: 4")
    return config


CONFIG_SCHEMA = cv.All(
    cv.only_on_esp32,
    cv.only_with_framework(Framework.ESP_IDF),
    esp32.only_on_variant(
        supported=[esp32.VARIANT_ESP32S3],
        msg_prefix="The CYD SD HTTP server",
    ),
    cv.Schema(
        {
            cv.GenerateID(): cv.declare_id(CydSdHttpServer),
            cv.Optional(CONF_PORT, default=8080): cv.port,
            cv.Optional(CONF_CONTROL_PORT, default=32769): cv.port,
            cv.Optional(CONF_MOUNT_POINT, default="/sdcard"): _mount_point,
            cv.Optional(CONF_URL_PREFIX, default="/sd"): _url_prefix,
            cv.Optional(CONF_FORMAT_IF_MOUNT_FAILED, default=False): cv.boolean,
            cv.Optional(CONF_INTERNAL_PULLUPS, default=True): cv.boolean,
            cv.Optional(CONF_BUS_WIDTH, default=4): cv.one_of(1, 4, int=True),
            cv.Required(CONF_CLK_PIN): pins.internal_gpio_pin_number,
            cv.Required(CONF_CMD_PIN): pins.internal_gpio_pin_number,
            cv.Required(CONF_DATA0_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA1_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA2_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_DATA3_PIN): pins.internal_gpio_pin_number,
            cv.Optional(CONF_MAX_UPLOAD_SIZE, default="200MB"): cv.All(
                cv.validate_bytes, cv.int_range(min=1, max=0x7FFFFFFF)
            ),
            cv.Required(CONF_API_KEY): _api_key,
            cv.Optional(CONF_DISPLAY_IMAGE): DISPLAY_IMAGE_SCHEMA,
            cv.Optional(CONF_DISPLAY_IMAGES): DISPLAY_IMAGES_SCHEMA,
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_config,
    _validate_display_images,
    socket.consume_sockets(4, "cyd_sd_http_server"),
    socket.consume_sockets(
        1, "cyd_sd_http_server.control", socket.SocketType.UDP
    ),
    socket.consume_sockets(
        1, "cyd_sd_http_server", socket.SocketType.TCP_LISTEN
    ),
)


async def to_code(config):
    var = cg.new_Pvariable(config[CONF_ID])
    await cg.register_component(var, config)

    cg.add(var.set_port(config[CONF_PORT]))
    cg.add(var.set_control_port(config[CONF_CONTROL_PORT]))
    cg.add(var.set_mount_point(config[CONF_MOUNT_POINT]))
    cg.add(var.set_url_prefix(config[CONF_URL_PREFIX]))
    cg.add(var.set_format_if_mount_failed(config[CONF_FORMAT_IF_MOUNT_FAILED]))
    cg.add(var.set_internal_pullups(config[CONF_INTERNAL_PULLUPS]))
    cg.add(var.set_bus_width(config[CONF_BUS_WIDTH]))
    cg.add(var.set_clk_pin(config[CONF_CLK_PIN]))
    cg.add(var.set_cmd_pin(config[CONF_CMD_PIN]))
    cg.add(var.set_data0_pin(config[CONF_DATA0_PIN]))
    if CONF_DATA1_PIN in config:
        cg.add(var.set_data1_pin(config[CONF_DATA1_PIN]))
        cg.add(var.set_data2_pin(config[CONF_DATA2_PIN]))
        cg.add(var.set_data3_pin(config[CONF_DATA3_PIN]))
    cg.add(var.set_max_upload_size(config[CONF_MAX_UPLOAD_SIZE]))
    cg.add(var.set_api_key(config[CONF_API_KEY]))

    legacy_display = config.get(CONF_DISPLAY_IMAGE)
    display = config.get(CONF_DISPLAY_IMAGES) or legacy_display
    if display is not None:
        cg.add_define("USE_CYD_SD_LVGL")
        cg.add_define("USE_CYD_SD_DIAGNOSIS")
        cg.add(var.set_lvgl_drive_letter(display[CONF_DRIVE_LETTER]))
        cache = display[CONF_CACHE]
        cg.add(var.set_decoded_image_cache_size(cache[CONF_MAX_SIZE]))
        cg.add(var.set_lvgl_file_cache_size(cache[CONF_FILE_BUFFER_SIZE]))
        for image in display[CONF_IMAGES]:
            cg.add(var.add_display_image(image[CONF_NAME], image[CONF_PATH]))

        names = [image[CONF_NAME] for image in display[CONF_IMAGES]]
        widgets = [legacy_display] if legacy_display is not None else display[CONF_WIDGETS]
        for widget_index, widget in enumerate(widgets):
            image_widget = await cg.get_variable(widget[CONF_IMAGE_WIDGET])
            initial_index = names.index(widget[CONF_INITIAL_IMAGE])
            fit_mode = {
                None: "LV_IMAGE_ALIGN_DEFAULT",
                "none": "LV_IMAGE_ALIGN_DEFAULT",
                "contain": "LV_IMAGE_ALIGN_CONTAIN",
                "cover": "LV_IMAGE_ALIGN_COVER",
                "stretch": "LV_IMAGE_ALIGN_STRETCH",
            }[widget.get(CONF_FIT)]
            cg.add(
                var.add_display_image_widget(
                    image_widget,
                    initial_index,
                    str(widget[CONF_IMAGE_WIDGET]),
                    cg.RawExpression(fit_mode),
                )
            )

            if select_config := widget.get(CONF_SELECT):
                image_select = cg.new_Pvariable(
                    select_config[CONF_ID], var, widget_index
                )
                await select.register_select(
                    image_select, select_config, options=names
                )
                cg.add(var.set_image_select(widget_index, image_select))
                if select_config[CONF_HIDDEN]:
                    cg.add_define("USE_CYD_SD_HIDDEN_SELECT")
                    cg.add_define("USE_API_USER_DEFINED_ACTIONS")
                    cg.add_define("USE_API_CUSTOM_SERVICES")
                    status = await text_sensor.new_text_sensor(widget[CONF_IMAGE_STATUS])
                    cg.add(image_select.set_hidden_status(status, _hidden_action_name(widget)))

        diagnosis = config[CONF_DIAGNOSIS]
        for key, _, setter in DIAGNOSIS_SENSORS:
            value = await sensor.new_sensor(diagnosis[key])
            cg.add(getattr(var, setter)(value))
        await button.new_button(diagnosis["release_button"], var)

    # ESPHome excludes these native ESP-IDF components unless requested.
    esp32.include_builtin_idf_component("fatfs")
    esp32.include_builtin_idf_component("esp_http_server")
    esp32.add_idf_sdkconfig_option("CONFIG_FATFS_LFN_STACK", True)
    esp32.add_idf_sdkconfig_option("CONFIG_FATFS_MAX_LFN", 255)
