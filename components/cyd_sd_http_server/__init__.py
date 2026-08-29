"""ESPHome code generation for the native CYD SD HTTP server."""

import esphome.codegen as cg
from esphome import pins
from esphome.components import esp32, socket
import esphome.config_validation as cv
from esphome.const import CONF_ID, Framework

DEPENDENCIES = ["esp32"]
AUTO_LOAD = ["network", "socket"]
CODEOWNERS = []

CONF_API_KEY = "api_key"
CONF_BUS_WIDTH = "bus_width"
CONF_CLK_PIN = "clk_pin"
CONF_CMD_PIN = "cmd_pin"
CONF_CONTROL_PORT = "control_port"
CONF_DATA0_PIN = "data0_pin"
CONF_DATA1_PIN = "data1_pin"
CONF_DATA2_PIN = "data2_pin"
CONF_DATA3_PIN = "data3_pin"
CONF_FORMAT_IF_MOUNT_FAILED = "format_if_mount_failed"
CONF_INTERNAL_PULLUPS = "internal_pullups"
CONF_MAX_UPLOAD_SIZE = "max_upload_size"
CONF_MOUNT_POINT = "mount_point"
CONF_PORT = "port"
CONF_URL_PREFIX = "url_prefix"

cyd_ns = cg.esphome_ns.namespace("cyd_sd_http_server")
CydSdHttpServer = cyd_ns.class_("CydSdHttpServer", cg.Component)


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
        }
    ).extend(cv.COMPONENT_SCHEMA),
    _validate_config,
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

    # ESPHome excludes these native ESP-IDF components unless requested.
    esp32.include_builtin_idf_component("fatfs")
    esp32.include_builtin_idf_component("esp_http_server")
    esp32.add_idf_sdkconfig_option("CONFIG_FATFS_LFN_STACK", True)
    esp32.add_idf_sdkconfig_option("CONFIG_FATFS_MAX_LFN", 255)
