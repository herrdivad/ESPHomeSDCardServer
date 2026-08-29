#!/usr/bin/env python3
"""Destructive-only-to-its-own-random-file smoke test for a real device.

This script is provided for later hardware validation. It is never run as part
of the local test suite.
"""

from __future__ import annotations

import argparse
import getpass
import hashlib
import http.client
import json
import os
from pathlib import Path
import secrets
import tempfile
from urllib.parse import quote, urlsplit


class Client:
    def __init__(self, url: str, api_key: str) -> None:
        parsed = urlsplit(url if "://" in url else "http://" + url)
        if parsed.scheme != "http":
            raise ValueError("Only http:// is supported")
        self.host = parsed.hostname
        self.port = parsed.port or 8080
        self.prefix = parsed.path.rstrip("/")
        self.api_key = api_key

    def request(self, method: str, endpoint: str, body=None, headers=None, expect=(200,)):
        conn = http.client.HTTPConnection(self.host, self.port, timeout=45)
        merged = {"X-API-Key": self.api_key, **(headers or {})}
        conn.request(method, self.prefix + endpoint, body=body, headers=merged)
        response = conn.getresponse()
        payload = response.read()
        status = response.status
        conn.close()
        if status not in expect:
            try:
                detail = json.loads(payload)
            except Exception:
                detail = payload[:200]
            raise RuntimeError(f"{method} {endpoint}: HTTP {status}: {detail}")
        return status, payload


def digest(path: Path) -> str:
    value = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            value.update(block)
    return value.hexdigest()


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("host")
    parser.add_argument("--size", type=int, default=1024 * 1024)
    args = parser.parse_args()
    api_key = os.environ.get("SD_API_KEY") or getpass.getpass("SD server API key: ")
    client = Client(args.host, api_key)
    target = f"/.cyd-smoke-{secrets.token_hex(8)}.bin"
    try:
        with tempfile.TemporaryDirectory(prefix="cyd-sd-smoke-") as directory:
            source = Path(directory) / "source.bin"
            downloaded = Path(directory) / "downloaded.bin"
            source.write_bytes(os.urandom(args.size))
            status, payload = client.request("GET", "/sd/api/health")
            health = json.loads(payload)
            if not health.get("mounted"):
                raise RuntimeError("health reports that the SD card is not mounted")
            with source.open("rb") as body:
                client.request(
                    "POST",
                    f"/sd/api/file?path={quote(target, safe='')}&overwrite=false",
                    body,
                    {"Content-Type": "application/octet-stream", "Content-Length": str(source.stat().st_size)},
                )
            _, payload = client.request("GET", "/sd/api/list?path=%2F&limit=100")
            if target not in {item["path"] for item in json.loads(payload)["entries"]}:
                raise RuntimeError("uploaded file is absent from directory listing")
            conn = http.client.HTTPConnection(client.host, client.port, timeout=45)
            conn.request("GET", client.prefix + f"/sd/api/file?path={quote(target, safe='')}", headers={"X-API-Key": api_key})
            response = conn.getresponse()
            if response.status != 200:
                raise RuntimeError(f"download returned HTTP {response.status}")
            with downloaded.open("wb") as output:
                while block := response.read(1024 * 1024):
                    output.write(block)
            conn.close()
            if digest(source) != digest(downloaded):
                raise RuntimeError("SHA-256 mismatch")
            client.request("DELETE", f"/sd/api/file?path={quote(target, safe='')}")
            client.request("GET", f"/sd/api/file?path={quote(target, safe='')}", expect=(404,))
            print(f"PASS: {args.size} bytes, SHA-256 {digest(source)}")
    except (OSError, RuntimeError, ValueError, json.JSONDecodeError) as error:
        print(f"FAIL: {error}", file=os.sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
