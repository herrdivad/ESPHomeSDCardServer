#!/usr/bin/env python3
"""Stream one file to the CYD SD server and optionally verify SHA-256."""

from __future__ import annotations

import argparse
import getpass
import hashlib
import http.client
import json
import os
from pathlib import Path
import tempfile
from urllib.parse import quote, urlsplit


def connection(base_url: str) -> tuple[http.client.HTTPConnection, str]:
    parsed = urlsplit(base_url if "://" in base_url else "http://" + base_url)
    if parsed.scheme != "http":
        raise ValueError("Only http:// is supported by the firmware")
    return http.client.HTTPConnection(parsed.hostname, parsed.port or 8080, timeout=30), parsed.path.rstrip("/")


def sha256(path: Path) -> str:
    digest = hashlib.sha256()
    with path.open("rb") as source:
        for block in iter(lambda: source.read(1024 * 1024), b""):
            digest.update(block)
    return digest.hexdigest()


def response_json(response: http.client.HTTPResponse) -> dict:
    payload = response.read()
    try:
        data = json.loads(payload)
    except (UnicodeDecodeError, json.JSONDecodeError) as error:
        raise RuntimeError(f"HTTP {response.status}: invalid JSON response") from error
    print(json.dumps(data, indent=2, ensure_ascii=False))
    if not 200 <= response.status < 300 or not data.get("ok"):
        raise RuntimeError(f"server rejected request with HTTP {response.status}")
    return data


def upload(base_url: str, api_key: str, source: Path, target: str, overwrite: bool) -> None:
    conn, prefix = connection(base_url)
    endpoint = f"{prefix}/sd/api/file?path={quote(target, safe='')}&overwrite={'true' if overwrite else 'false'}"
    size = source.stat().st_size
    with source.open("rb") as body:
        conn.request(
            "POST",
            endpoint,
            body=body,
            headers={
                "X-API-Key": api_key,
                "Content-Type": "application/octet-stream",
                "Content-Length": str(size),
            },
        )
        response_json(conn.getresponse())
    conn.close()


def verify_download(base_url: str, api_key: str, source: Path, target: str) -> None:
    conn, prefix = connection(base_url)
    conn.request("GET", f"{prefix}/sd/api/file?path={quote(target, safe='')}", headers={"X-API-Key": api_key})
    response = conn.getresponse()
    if response.status != 200:
        response_json(response)
    with tempfile.NamedTemporaryFile(prefix="cyd-sd-verify-", delete=False) as destination:
        temporary = Path(destination.name)
        while block := response.read(1024 * 1024):
            destination.write(block)
    conn.close()
    try:
        local_hash = sha256(source)
        remote_hash = sha256(temporary)
        if local_hash != remote_hash:
            raise RuntimeError(f"SHA-256 mismatch: local={local_hash} downloaded={remote_hash}")
        print(f"SHA-256 verified: {local_hash}")
    finally:
        temporary.unlink(missing_ok=True)


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument("host", help="Device URL, for example http://fnk0104s.local:8080")
    parser.add_argument("source", type=Path)
    parser.add_argument("target", help="Absolute virtual path, for example /media/file.bin")
    parser.add_argument("--overwrite", action="store_true")
    parser.add_argument("--verify", action="store_true", help="Download again and compare SHA-256")
    args = parser.parse_args()
    if not args.source.is_file():
        parser.error(f"not a regular file: {args.source}")
    if not args.target.startswith("/"):
        parser.error("target must start with /")
    api_key = os.environ.get("SD_API_KEY") or getpass.getpass("SD server API key: ")
    try:
        upload(args.host, api_key, args.source, args.target, args.overwrite)
        if args.verify:
            verify_download(args.host, api_key, args.source, args.target)
    except (OSError, RuntimeError, ValueError) as error:
        print(f"ERROR: {error}", file=os.sys.stderr)
        return 1
    return 0


if __name__ == "__main__":
    raise SystemExit(main())

