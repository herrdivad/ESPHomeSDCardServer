#!/usr/bin/env python3
"""Check the built site, links, public artifact, and project-subpath behavior offline."""

from __future__ import annotations

import argparse
from html.parser import HTMLParser
from pathlib import Path
import re
import struct
from urllib.parse import unquote, urljoin, urlsplit

from build_website import DEFAULT_OUTPUT, IMAGE_ASSETS, PHOTOS, ROOT, SCREENSHOTS, STATIC_ASSETS, public_photo

BASE = "https://herrdivad.github.io/ESPHomeSDCardServer/"
REPOSITORY = "https://github.com/herrdivad/ESPHomeSDCardServer/"


class Page(HTMLParser):
    def __init__(self, path: Path):
        super().__init__(convert_charrefs=True)
        self.path = path
        self.ids: set[str] = set()
        self.references: list[tuple[str, str]] = []
        self.errors: list[str] = []
        self.h1_count = 0
        self.has_language = False
        self.has_title = False
        self.has_viewport = False
        self.feed(path.read_text(encoding="utf-8"))
        self.close()

    def handle_starttag(self, tag: str, attributes: list[tuple[str, str | None]]) -> None:
        attrs = dict(attributes)
        element_id = attrs.get("id")
        if element_id:
            if element_id in self.ids:
                self.errors.append(f"Duplicate ID: {element_id}")
            self.ids.add(element_id)
        self.h1_count += tag == "h1"
        self.has_language |= tag == "html" and attrs.get("lang") == "en"
        self.has_title |= tag == "title"
        self.has_viewport |= tag == "meta" and attrs.get("name") == "viewport"
        if tag == "img" and "alt" not in attrs:
            self.errors.append("Image missing alt attribute")
        if tag == "a" and not attrs.get("href"):
            self.errors.append("Anchor missing href")
        for name in ("href", "src"):
            if attrs.get(name):
                self.references.append((tag, attrs[name]))
        for name in ("aria-controls", "aria-labelledby", "aria-describedby"):
            for element_id in (attrs.get(name) or "").split():
                self.references.append(("aria", "#" + element_id))


def markdown_ids(path: Path) -> set[str]:
    ids: set[str] = set()
    in_code = False
    for line in path.read_text(encoding="utf-8").splitlines():
        if line.startswith("```"):
            in_code = not in_code
        if in_code or not re.match(r"^#{1,6} ", line):
            continue
        heading = re.sub(r"^#+\s+", "", line).lower()
        heading = re.sub(r"[^\w\-\s]", "", heading).replace(" ", "-")
        candidate = heading
        suffix = 0
        while candidate in ids:
            suffix += 1
            candidate = f"{heading}-{suffix}"
        ids.add(candidate)
    return ids


def validate(output: Path) -> list[str]:
    output = output.resolve()
    errors: list[str] = []
    pages = {path.name: Page(path) for path in output.glob("*.html")}
    expected = {"index.html", "guide.html", "404.html", ".nojekyll"}
    expected.update("assets/" + name for name in (*STATIC_ASSETS, *IMAGE_ASSETS))
    actual = {path.relative_to(output).as_posix() for path in output.rglob("*") if path.is_file()}
    if actual != expected:
        errors.append(f"Artifact file mismatch: missing={expected - actual}, unexpected={actual - expected}")
    if any(path.is_symlink() for path in output.rglob("*")):
        errors.append("Artifact must not contain symlinks")
    for name, page in pages.items():
        errors.extend(f"{name}: {error}" for error in page.errors)
        if page.h1_count != 1 or not all((page.has_language, page.has_title, page.has_viewport)):
            errors.append(f"{name}: expected one h1, English lang, title, and viewport")
        if "@@" in page.path.read_text(encoding="utf-8"):
            errors.append(f"{name}: unresolved template marker")
        for tag, reference in page.references:
            url = urljoin(BASE + name, reference)
            parsed = urlsplit(url)
            if url.startswith(REPOSITORY):
                repo_match = re.match(re.escape(REPOSITORY) + r"(?:blob|tree)/master/([^#?]+)", url)
                if repo_match:
                    target = ROOT / unquote(repo_match.group(1))
                    if not target.exists():
                        errors.append(f"{name}: missing repository target: {reference}")
                    elif parsed.fragment and target.suffix == ".md" and unquote(parsed.fragment) not in markdown_ids(target):
                        errors.append(f"{name}: missing repository heading: {reference}")
                continue
            if parsed.netloc != urlsplit(BASE).netloc:
                if parsed.scheme not in ("http", "https"):
                    errors.append(f"{name}: unsupported URL scheme: {reference}")
                if tag != "a":
                    errors.append(f"{name}: external runtime dependency: {reference}")
                continue
            if not parsed.path.startswith(urlsplit(BASE).path):
                errors.append(f"{name}: link escapes /ESPHomeSDCardServer/: {reference}")
                continue
            relative = unquote(parsed.path[len(urlsplit(BASE).path):]) or "index.html"
            target = (output / relative).resolve()
            if not target.is_relative_to(output) or not target.is_file():
                errors.append(f"{name}: missing local target: {reference}")
            elif parsed.fragment:
                target_page = pages.get(target.name)
                if target_page is None or unquote(parsed.fragment) not in target_page.ids:
                    errors.append(f"{name}: missing anchor: {reference}")
    for name in SCREENSHOTS:
        original = (ROOT / "examples" / name).read_bytes()
        copied = (output / "assets" / name).read_bytes()
        if original != copied:
            errors.append(f"Screenshot was changed: {name}")
        width, height = struct.unpack(">II", original[16:24])
        if width <= 0 or height <= 0:
            errors.append(f"Invalid screenshot: {name}")
    for name in PHOTOS:
        original = (ROOT / "examples" / name).read_bytes()
        copied = (output / "assets" / name).read_bytes()
        sanitized = public_photo(original)
        if original != sanitized:
            errors.append(f"Source photo still contains JPEG metadata: {name}")
        if copied != sanitized:
            errors.append(f"Photo copy must preserve encoded pixels and omit all JPEG metadata: {name}")
    css = (output / "assets/styles.css").read_text(encoding="utf-8")
    if re.search(r"@import|@font-face|url\s*\(", css, re.IGNORECASE):
        errors.append("CSS must remain self-contained with system fonts")
    return errors


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--output", type=Path, default=DEFAULT_OUTPUT)
    args = parser.parse_args()
    if not (args.output / "index.html").is_file():
        parser.error("Build the website first: python3 scripts/build_website.py")
    errors = validate(args.output)
    if errors:
        print("Website validation failed:\n" + "\n".join(f"- {error}" for error in errors))
        return 1
    print("Website checks passed: artifact allow-list, HTML metadata, image alt text, IDs, ARIA references,")
    print("local links/anchors, repository links, unchanged screenshots, sanitized photo, system fonts, and Pages subpath.")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
