"""Validate real ESPHome configuration and generated hidden image entities."""

import importlib.util
from pathlib import Path
import re
import subprocess
import sys
import tempfile
import unittest


ROOT = Path(__file__).resolve().parents[1]


@unittest.skipUnless(importlib.util.find_spec("esphome"), "ESPHome is not installed")
class HiddenImageConfigTest(unittest.TestCase):
    def setUp(self):
        import yaml

        source = (ROOT / "examples/fnk0104s-esp-idf.yaml").read_text()
        # Use test credentials, never the maintainer's secrets.yaml.
        source = re.sub(r"!secret \w+", '"test-credentials-with-at-least-32-bytes"', source)
        self.config = yaml.safe_load(source)
        self.config["wifi"]["ssid"] = "test-network"
        self.config["external_components"][0]["source"]["path"] = str(ROOT / "components")
        self.config["image"][0]["file"] = str(ROOT / "examples/assets/sd-image-placeholder.svg")
        self.temp = tempfile.TemporaryDirectory(prefix="cyd-hidden-test-")
        self.addCleanup(self.temp.cleanup)
        self.directory = Path(self.temp.name)
        self.config["esphome"]["build_path"] = str(self.directory / "build")

    def hidden_widget(self):
        widget = self.config["cyd_sd_http_server"]["display_images"]["widgets"][0]
        widget["select"]["hidden"] = True
        return widget

    def run_config(self, *, generate=False, error=None):
        import yaml

        path = self.directory / "device.yaml"
        path.write_text(yaml.safe_dump(self.config, sort_keys=False))
        command = [sys.executable, "-m", "esphome"]
        command += ["compile", "--only-generate"] if generate else ["config"]
        result = subprocess.run(command + [str(path)], cwd=ROOT, text=True,
                                capture_output=True, timeout=90)
        output = result.stdout + result.stderr
        if error is not None:
            self.assertNotEqual(result.returncode, 0, output)
            self.assertIn(error, output)
        else:
            self.assertEqual(result.returncode, 0, output)
        return output

    def generated_sources(self):
        self.run_config(generate=True)
        build = self.directory / "build/src"
        return ((build / "main.cpp").read_text(),
                (build / "esphome/core/defines.h").read_text())

    def test_existing_visible_select_without_api(self):
        self.config.pop("api")
        main, defines = self.generated_sources()
        self.assertNotIn("USE_CYD_SD_HIDDEN_SELECT", defines)
        self.assertNotIn("set_hidden_status(", main)
        self.assertIn('App.register_select(sd_display_image_select, "Display image"', main)

    def test_native_diagnostics_without_extra_yaml(self):
        main, defines = self.generated_sources()
        self.assertIn("#define USE_CYD_SD_DIAGNOSIS", defines)
        for name in (
            "SD PSRAM Total", "SD PSRAM Free", "SD PSRAM Largest Free Block",
            "SD Image Cache Limit", "SD Image Cache Used",
            "SD Image Cache Reclaimable",
        ):
            self.assertRegex(main, rf'App\.register_sensor\([^\n]*"{name}"')
        self.assertEqual(main.count("App.register_sensor("), 6)
        self.assertIn('App.register_button(cyd_sd_server_release_unused_image_cache,', main)
        self.assertIn('"Release Unused SD Image Cache"', main)
        self.assertEqual(main.count("App.register_button("), 1)
        self.assertEqual(main.count("set_state_class(sensor::STATE_CLASS_MEASUREMENT)"), 6)

    def test_explicit_false_does_not_require_api(self):
        self.hidden_widget()["select"]["hidden"] = False
        self.config.pop("api")
        self.run_config()

    def test_single_widget_mapping_without_list(self):
        widget = self.hidden_widget()
        self.config["cyd_sd_http_server"]["display_images"]["widgets"] = widget
        self.run_config()

    def test_id_only_hidden_select(self):
        self.hidden_widget()["select"].pop("name")
        self.run_config()

    def test_hidden_entities_and_existing_api_action(self):
        widget = self.hidden_widget()
        widget["select"]["icon"] = "mdi:image"
        widget["select"]["on_value"] = [{"then": [{"logger.log": "Image changed"}]}]
        widgets = self.config["cyd_sd_http_server"]["display_images"]["widgets"]
        widgets[1]["select"] = {"id": "logo_select", "name": "Visible logo"}
        widgets[2]["select"] = {"name": "Hidden logo", "hidden": True}
        self.config["api"] = {"actions": [{"action": "existing_action", "then": [
            {"logger.log": "Existing action"}]}]}
        main, defines = self.generated_sources()
        self.assertIn("#define USE_CYD_SD_HIDDEN_SELECT", defines)
        self.assertIn("#define USE_API_CUSTOM_SERVICES", defines)
        self.assertIn("#define USE_API_USER_DEFINED_ACTIONS", defines)
        self.assertIn("#define USE_TEXT_SENSOR", defines)
        select_line = next(line for line in main.splitlines()
                           if "App.register_select(sd_display_image_select," in line)
        self.assertIn("// internal", select_line)
        status_line = next(line for line in main.splitlines()
                           if "App.register_text_sensor(cyd_sd_sd_display_image_image_status," in line)
        self.assertNotIn("// internal", status_line)
        self.assertIn('"Display image"', status_line)
        self.assertIn('"cyd_sd_set_sd_display_image"', main)
        self.assertIn('"cyd_sd_set_logo_2"', main)
        self.assertIn('"existing_action"', main)
        self.assertIn("SelectStateTrigger(sd_display_image_select)", main)
        self.assertIn("App.register_select(logo_select,", main)
        self.assertEqual(main.count("App.register_text_sensor("), 2)

    def test_legacy_hidden_select(self):
        self.hidden_widget()
        server = self.config["cyd_sd_http_server"]
        display = server.pop("display_images")
        widget = display.pop("widgets")[0]
        server["display_image"] = {**display, **widget}
        self.run_config(generate=True)
        self.assertTrue((self.directory / "build/src/esphome/components/api/user_services.cpp").is_file())
        main = (self.directory / "build/src/main.cpp").read_text()
        self.assertIn('App.register_button(cyd_sd_server_release_unused_image_cache,', main)

    def test_widget_fit_modes_are_generated_independently(self):
        widgets = self.config["cyd_sd_http_server"]["display_images"]["widgets"]
        widgets[0]["fit"] = "contain"
        widgets[1]["fit"] = "cover"
        widgets[2]["fit"] = "stretch"
        main, _ = self.generated_sources()
        self.assertIn('add_display_image_widget(sd_display_image, 0, "sd_display_image", LV_IMAGE_ALIGN_CONTAIN)', main)
        self.assertIn('add_display_image_widget(logo_1, 2, "logo_1", LV_IMAGE_ALIGN_COVER)', main)
        self.assertIn('add_display_image_widget(logo_2, 3, "logo_2", LV_IMAGE_ALIGN_STRETCH)', main)
        self.assertIn('add_display_image_widget(logo_3, 4, "logo_3", LV_IMAGE_ALIGN_DEFAULT)', main)
        lv_conf = (self.directory / "build/src/lv_conf.h").read_text()
        self.assertIn("#define LV_DRAW_SW_SUPPORT_RGB565A8 1", lv_conf)

    def test_no_fit_keeps_rgb565a8_disabled(self):
        widgets = self.config["cyd_sd_http_server"]["display_images"]["widgets"]
        for widget in widgets:
            widget.pop("fit", None)
        self.generated_sources()
        lv_conf = (self.directory / "build/src/lv_conf.h").read_text()
        self.assertIn("#define LV_DRAW_SW_SUPPORT_RGB565A8 0", lv_conf)

    def test_legacy_widget_accepts_fit(self):
        server = self.config["cyd_sd_http_server"]
        display = server.pop("display_images")
        widget = display.pop("widgets")[0]
        server["display_image"] = {**display, **widget, "fit": "contain"}
        main, _ = self.generated_sources()
        self.assertIn("LV_IMAGE_ALIGN_CONTAIN", main)

    def test_invalid_widget_fit_is_rejected(self):
        self.config["cyd_sd_http_server"]["display_images"]["widgets"][0]["fit"] = "zoom"
        self.run_config(error="fit")

    def test_missing_api(self):
        self.hidden_widget()
        self.config.pop("api")
        self.run_config(error="requires component api")

    def test_invalid_hidden_value(self):
        self.hidden_widget()["select"]["hidden"] = "sometimes"
        self.run_config(error="boolean")

    def test_generated_sensor_id_collision(self):
        self.hidden_widget()
        self.config["text_sensor"] = [{"platform": "template", "name": "Conflicting sensor",
                                       "id": "cyd_sd_sd_display_image_image_status"}]
        self.run_config(error="redefined")

    def test_generated_sensor_name_collision(self):
        self.hidden_widget()
        self.config["text_sensor"] = [{"platform": "template", "name": "Display image"}]
        self.run_config(error="Duplicate")

    def test_api_action_collision(self):
        self.hidden_widget()
        self.config["api"] = {"actions": [{"action": "cyd_sd_set_sd_display_image",
                                            "then": [{"logger.log": "Conflicting action"}]}]}
        self.run_config(error="Hidden image action name conflicts")

    def test_legacy_api_service_collision(self):
        self.hidden_widget()
        self.config["api"] = {"services": [{"service": "cyd_sd_set_sd_display_image",
                                             "then": [{"logger.log": "Conflicting service"}]}]}
        self.run_config(error="Hidden image action name conflicts")

    def test_file_server_without_display_or_api(self):
        self.config["cyd_sd_http_server"].pop("display_images")
        for key in ("api", "lvgl", "display", "image", "spi"):
            self.config.pop(key, None)
        main, defines = self.generated_sources()
        self.assertNotIn("USE_CYD_SD_LVGL", defines)
        self.assertNotIn("USE_CYD_SD_HIDDEN_SELECT", defines)
        self.assertNotIn("USE_CYD_SD_DIAGNOSIS", defines)
        self.assertNotIn("USE_TEXT_SENSOR", defines)
        self.assertNotIn("set_hidden_status(", main)
        self.assertNotIn("Release Unused SD Image Cache", main)
