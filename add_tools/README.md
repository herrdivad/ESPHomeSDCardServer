# LVGL image conversion tool

`image_to_lvgl_bin.py` converts a PNG or baseline/progressive JPEG on the host
computer into the uncompressed LVGL 9.5 RGB565 BIN format used by this project.
The microcontroller is not involved.

## Installation

Use the existing project virtual environment:

```bash
.venv/bin/python -m pip install -r add_tools/requirements.txt
```

Or create an isolated environment for the tool:

```bash
python3 -m venv add_tools/.venv
add_tools/.venv/bin/python -m pip install -r add_tools/requirements.txt
```

## Usage

Keep the input dimensions:

```bash
.venv/bin/python add_tools/image_to_lvgl_bin.py photo.png
```

Set only the width; the height is calculated proportionally:

```bash
.venv/bin/python add_tools/image_to_lvgl_bin.py logo.png --width 120
```

Set only the height; the width is calculated proportionally:

```bash
.venv/bin/python add_tools/image_to_lvgl_bin.py logo.jpg --height 80
```

Rotate 90 degrees clockwise first and then create an exact 480 x 320 result:

```bash
.venv/bin/python add_tools/image_to_lvgl_bin.py portrait.jpg \
  --rotate 90 --width 480 --height 320 --output landscape.bin
```

When both dimensions are supplied, the rotated image is resized to that exact
size and its aspect ratio may change. Rotation is clockwise and happens before
resizing, so `--width` and `--height` always describe the final BIN dimensions.
Angles other than multiples of 90 degrees expand the canvas. Transparent PNG
areas and newly exposed corners use black by default; choose another color with
`--background '#204060'`.

The default output is next to the input with its suffix replaced by `.bin`.
Existing files are protected unless `--force` is supplied.

## Output format

The result contains:

- a 12-byte little-endian LVGL 9.5 image header;
- color format `LV_COLOR_FORMAT_RGB565` (`0x12`);
- uncompressed, row-major, little-endian RGB565 pixels;
- no alpha channel.

The expected size is `12 + width * height * 2` bytes. For example, a 240 x 160
quarter-screen image is 76,812 bytes and four decoded images require 307,200
bytes of pixel storage.
