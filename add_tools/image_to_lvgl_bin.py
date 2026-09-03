#!/usr/bin/env python3
"""Convert a PNG or JPEG into an LVGL 9.5 RGB565 binary image."""

from __future__ import annotations

import argparse
import math
from pathlib import Path
import struct
import sys
import tempfile

from PIL import Image, ImageColor, ImageOps, UnidentifiedImageError


LV_IMAGE_HEADER_MAGIC = 0x19
LV_COLOR_FORMAT_RGB565 = 0x12
HEADER = struct.Struct("<BBHHHHH")
SUPPORTED_FORMATS = {"JPEG", "PNG"}


def positive_dimension(value: str) -> int:
    try:
        parsed = int(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a whole number") from error
    if parsed <= 0:
        raise argparse.ArgumentTypeError("must be greater than zero")
    return parsed


def finite_rotation(value: str) -> float:
    try:
        parsed = float(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError("must be a number of degrees") from error
    if not math.isfinite(parsed):
        raise argparse.ArgumentTypeError("must be finite")
    return parsed


def parse_background(value: str) -> tuple[int, int, int, int]:
    try:
        red, green, blue = ImageColor.getrgb(value)
    except ValueError as error:
        raise argparse.ArgumentTypeError(f"invalid color: {value}") from error
    return red, green, blue, 255


def rotate_clockwise(image: Image.Image, degrees: float) -> Image.Image:
    normalized = degrees % 360.0
    if math.isclose(normalized, 0.0, abs_tol=1e-9):
        return image
    if math.isclose(normalized, 90.0, abs_tol=1e-9):
        return image.transpose(Image.Transpose.ROTATE_270)
    if math.isclose(normalized, 180.0, abs_tol=1e-9):
        return image.transpose(Image.Transpose.ROTATE_180)
    if math.isclose(normalized, 270.0, abs_tol=1e-9):
        return image.transpose(Image.Transpose.ROTATE_90)
    return image.rotate(
        -normalized,
        resample=Image.Resampling.BICUBIC,
        expand=True,
        fillcolor=(0, 0, 0, 0),
    )


def output_dimensions(image: Image.Image, width: int | None, height: int | None) -> tuple[int, int]:
    source_width, source_height = image.size
    if width is None and height is None:
        return source_width, source_height
    if width is None:
        width = max(1, round(source_width * height / source_height))
    elif height is None:
        height = max(1, round(source_height * width / source_width))
    return width, height


def prepare_image(
    source: Path,
    width: int | None,
    height: int | None,
    rotation: float,
    background: tuple[int, int, int, int],
) -> Image.Image:
    with Image.open(source) as opened:
        if opened.format not in SUPPORTED_FORMATS:
            raise ValueError(f"unsupported input format {opened.format!r}; expected PNG or JPEG")
        image = ImageOps.exif_transpose(opened).convert("RGBA")

    image = rotate_clockwise(image, rotation)
    target_size = output_dimensions(image, width, height)
    if target_size != image.size:
        image = image.resize(target_size, Image.Resampling.LANCZOS)

    opaque = Image.new("RGBA", image.size, background)
    opaque.alpha_composite(image)
    return opaque.convert("RGB")


def rgb565_bytes(image: Image.Image) -> bytes:
    rgb = image.tobytes()
    converted = bytearray(image.width * image.height * 2)
    destination = 0
    for source in range(0, len(rgb), 3):
        red, green, blue = rgb[source : source + 3]
        value = ((red & 0xF8) << 8) | ((green & 0xFC) << 3) | (blue >> 3)
        converted[destination] = value & 0xFF
        converted[destination + 1] = value >> 8
        destination += 2
    return bytes(converted)


def lvgl_header(width: int, height: int) -> bytes:
    stride = width * 2
    if width > 0xFFFF or height > 0xFFFF:
        raise ValueError("LVGL BIN width and height must fit into 16 bits")
    if stride > 0xFFFF:
        raise ValueError("RGB565 row stride must fit into 16 bits; width must be at most 32767")
    return HEADER.pack(
        LV_IMAGE_HEADER_MAGIC,
        LV_COLOR_FORMAT_RGB565,
        0,  # flags
        width,
        height,
        stride,
        0,  # reserved_2
    )


def write_bin(image: Image.Image, destination: Path, overwrite: bool) -> int:
    if destination.exists() and not overwrite:
        raise FileExistsError(f"output already exists: {destination} (use --force to replace it)")
    destination.parent.mkdir(parents=True, exist_ok=True)
    header = lvgl_header(image.width, image.height)
    pixels = rgb565_bytes(image)

    temporary_name: str | None = None
    try:
        with tempfile.NamedTemporaryFile(
            mode="wb",
            prefix=f".{destination.name}.",
            suffix=".tmp",
            dir=destination.parent,
            delete=False,
        ) as temporary:
            temporary_name = temporary.name
            temporary.write(header)
            temporary.write(pixels)
            temporary.flush()
        Path(temporary_name).replace(destination)
    except Exception:
        if temporary_name is not None:
            Path(temporary_name).unlink(missing_ok=True)
        raise
    return len(header) + len(pixels)


def parser() -> argparse.ArgumentParser:
    result = argparse.ArgumentParser(
        description="Convert PNG/JPEG to an uncompressed LVGL 9.5 RGB565 .bin file.",
    )
    result.add_argument("input", type=Path, help="source PNG, JPG, or JPEG")
    result.add_argument("-o", "--output", type=Path, help="output path; defaults to INPUT with a .bin suffix")
    result.add_argument("-x", "--width", type=positive_dimension, help="final width in pixels")
    result.add_argument("-y", "--height", type=positive_dimension, help="final height in pixels")
    result.add_argument(
        "-r",
        "--rotate",
        type=finite_rotation,
        default=0.0,
        metavar="DEGREES",
        help="clockwise rotation before resizing; any angle is accepted (default: 0)",
    )
    result.add_argument(
        "--background",
        type=parse_background,
        default=parse_background("#000000"),
        metavar="COLOR",
        help="background for PNG transparency and rotated corners (default: #000000)",
    )
    result.add_argument("--force", action="store_true", help="replace an existing output file")
    return result


def main() -> int:
    arguments = parser().parse_args()
    source = arguments.input
    destination = arguments.output or source.with_suffix(".bin")
    if not source.is_file():
        print(f"ERROR: input is not a regular file: {source}", file=sys.stderr)
        return 2

    try:
        image = prepare_image(
            source,
            arguments.width,
            arguments.height,
            arguments.rotate,
            arguments.background,
        )
        written = write_bin(image, destination, arguments.force)
    except (FileExistsError, OSError, UnidentifiedImageError, ValueError) as error:
        print(f"ERROR: {error}", file=sys.stderr)
        return 1

    print(
        f"Created {destination}: {image.width}x{image.height}, RGB565, "
        f"{written} bytes ({written - HEADER.size} pixel bytes + {HEADER.size}-byte header)"
    )
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
