#!/usr/bin/env python3
"""Build the public Pages artifact using only Python's standard library."""

from __future__ import annotations

import argparse
import html
from pathlib import Path
import re
import shutil

ROOT = Path(__file__).resolve().parents[1]
SOURCE = ROOT / "website"
DEFAULT_OUTPUT = ROOT / "build" / "pages"
SCREENSHOTS = ("en-dark-1040.png", "de-dark-375.png", "HA-Example1.png")
PHOTOS = ("ESPHome-SDWebAndImageServer.jpg",)
IMAGE_ASSETS = (*SCREENSHOTS, *PHOTOS)
STATIC_ASSETS = ("styles.css", "site.js", "favicon.svg")


def public_photo(data: bytes) -> bytes:
    """Strip all JPEG APP/comment segments and trailers, preserving encoded pixels."""
    if not data.startswith(b"\xff\xd8"):
        raise ValueError("Expected a JPEG hardware photo")
    result = bytearray(data[:2])
    offset = 2
    while offset < len(data):
        start = offset
        if data[offset] != 0xFF:
            raise ValueError("Invalid JPEG marker before scan data")
        while offset < len(data) and data[offset] == 0xFF:
            offset += 1
        if offset >= len(data):
            break
        marker = data[offset]
        offset += 1
        if marker == 0xD9:
            result.extend(data[start:offset])
            return bytes(result)
        if marker == 0x01 or 0xD0 <= marker <= 0xD7:
            result.extend(data[start:offset])
            continue
        if offset + 2 > len(data):
            raise ValueError("Truncated JPEG segment")
        length = int.from_bytes(data[offset:offset + 2], "big")
        end = offset + length
        if length < 2 or end > len(data):
            raise ValueError("Invalid JPEG segment length")
        if not (0xE0 <= marker <= 0xEF or marker == 0xFE):
            result.extend(data[start:end])
        offset = end
        if marker == 0xDA:
            # Stuffed bytes and restart markers belong to the compressed scan.
            # Parse subsequent markers too, so metadata between scans is removed.
            scan_start = offset
            search = offset
            while True:
                scan_end = data.find(b"\xff", search)
                if scan_end < 0:
                    raise ValueError("JPEG photo has no end marker")
                following = scan_end + 1
                while following < len(data) and data[following] == 0xFF:
                    following += 1
                if following >= len(data):
                    raise ValueError("Truncated JPEG scan marker")
                next_marker = data[following]
                if next_marker == 0x00 or 0xD0 <= next_marker <= 0xD7:
                    search = following + 1
                    continue
                result.extend(data[scan_start:scan_end])
                offset = scan_end
                break
    raise ValueError("JPEG photo has no end marker")


def readme_excerpt(readme: str, heading: str, language: str) -> str:
    section = readme.split(heading, 1)[1]
    match = re.search(rf"```{language}\n(.*?)\n```", section, re.DOTALL)
    if not match:
        raise ValueError(f"Missing README code block: {heading}")
    return html.escape(match.group(1))


def api_rows(readme: str) -> str:
    section = readme.split("## HTTP API\n", 1)[1]
    rows = re.findall(r"^\| `(GET|POST|DELETE)` \| `(.*?)` \| (.*?) \|$", section, re.MULTILINE)
    if len(rows) != 7:
        raise ValueError("Expected seven documented API endpoints; review the website")
    return "\n".join(
        f"<tr><td><code>{method}</code></td><td><code>{html.escape(endpoint)}</code></td>"
        f"<td>{html.escape(purpose)}</td></tr>"
        for method, endpoint, purpose in rows
    )


def build(output: Path) -> None:
    # Only named public inputs are copied; never package the checkout or secrets.
    output = output.resolve()
    if not output.is_relative_to(ROOT / "build") or output == ROOT / "build":
        raise ValueError("The output must be a subdirectory of this repository's build/ directory")
    readme = (ROOT / "README.md").read_text(encoding="utf-8")
    replacements = {
        "@@HEADER@@": (SOURCE / "partials/header.html").read_text(encoding="utf-8"),
        "@@FOOTER@@": (SOURCE / "partials/footer.html").read_text(encoding="utf-8"),
        "@@MINIMAL_CONFIG@@": readme_excerpt(readme, "### Minimal file server\n", "yaml"),
        "@@API_ROWS@@": api_rows(readme),
    }
    (output / "assets").mkdir(parents=True, exist_ok=True)
    for name in ("index.html", "guide.html", "404.html"):
        content = (SOURCE / name).read_text(encoding="utf-8")
        for marker, replacement in replacements.items():
            content = content.replace(marker, replacement)
        if re.search(r"@@[A-Z_]+@@", content):
            raise ValueError(f"Unresolved template marker in {name}")
        (output / name).write_text(content, encoding="utf-8")
    for name in STATIC_ASSETS:
        shutil.copyfile(SOURCE / "assets" / name, output / "assets" / name)
    for name in SCREENSHOTS:
        shutil.copyfile(ROOT / "examples" / name, output / "assets" / name)
    for name in PHOTOS:
        original = (ROOT / "examples" / name).read_bytes()
        (output / "assets" / name).write_bytes(public_photo(original))
    (output / ".nojekyll").write_text("", encoding="utf-8")
    print(f"Built static website: {output.relative_to(ROOT)}")


def main() -> None:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    build(args.output)


if __name__ == "__main__":
    main()
