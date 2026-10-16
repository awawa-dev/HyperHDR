#!/usr/bin/env python3
"""Update rpi-imager_hyperhdr.json with a new stable release.

Only touches os_list[0] fields that change per release:
  name, url, image_download_size, image_download_sha256,
  extract_size, extract_sha256, release_date.

Everything else (description, icon, website, devices, init_format)
is preserved as-is.

Usage example (called from update-rpi-imager-json.yml):
  python3 rpi-imager/update_imager_json.py \
    --json rpi-imager/rpi-imager_hyperhdr.json \
    --release-name "HyperHDR 22.0.0" \
    --url "https://github.com/awawa-dev/HyperHDR/releases/download/v22.0.0.0/....img.xz" \
    --download-size 760724096 \
    --download-sha 0d94... \
    --extract-size 3594519552 \
    --extract-sha 2274... \
    --release-date 2026-09-01
"""

import argparse
import json
import re
import sys
from pathlib import Path

_SHA256_RE = re.compile(r"^[0-9a-f]{64}$")
_DATE_RE = re.compile(r"^\d{4}-\d{2}-\d{2}$")


def parse_args(argv=None):
    parser = argparse.ArgumentParser(
        description="Update rpi-imager_hyperhdr.json for a stable release."
    )
    parser.add_argument("--json", required=True, help="Path to rpi-imager_hyperhdr.json")
    parser.add_argument(
        "--release-name",
        required=True,
        help='Release name verbatim, e.g. "HyperHDR 22.0.0"',
    )
    parser.add_argument("--url", required=True, help="Release asset download URL")
    parser.add_argument("--download-size", required=True, type=int)
    parser.add_argument("--download-sha", required=True)
    parser.add_argument("--extract-size", required=True, type=int)
    parser.add_argument("--extract-sha", required=True)
    parser.add_argument("--release-date", required=True, help="YYYY-MM-DD")
    return parser.parse_args(argv)


def build_display_name(release_name: str) -> str:
    name = release_name.strip()
    if not name:
        raise ValueError("release name must not be empty")
    if not name.endswith("(64-bit)"):
        name = f"{name} (64-bit)"
    return name


def main(argv=None) -> int:
    args = parse_args(argv)
    json_path = Path(args.json)

    try:
        payload = json.loads(json_path.read_text(encoding="utf-8"))
    except FileNotFoundError:
        print(f"error: JSON file not found: {json_path}", file=sys.stderr)
        return 1
    except json.JSONDecodeError as exc:
        print(f"error: invalid JSON in {json_path}: {exc}", file=sys.stderr)
        return 1

    os_list = payload.get("os_list")
    if not isinstance(os_list, list) or not os_list:
        print("error: JSON must contain a non-empty os_list array", file=sys.stderr)
        return 1
    if not isinstance(os_list[0], dict):
        print("error: os_list[0] must be an object", file=sys.stderr)
        return 1

    for label, value in (
        ("download-sha", args.download_sha),
        ("extract-sha", args.extract_sha),
    ):
        if not _SHA256_RE.match(value.strip().lower()):
            print(f"error: --{label} must be a 64-char hex sha256", file=sys.stderr)
            return 1
    if args.download_size <= 0 or args.extract_size <= 0:
        print("error: sizes must be positive integers", file=sys.stderr)
        return 1
    if not _DATE_RE.match(args.release_date):
        print("error: --release-date must be YYYY-MM-DD", file=sys.stderr)
        return 1
    if not args.url.startswith("https://"):
        print("error: --url must start with https://", file=sys.stderr)
        return 1

    try:
        display_name = build_display_name(args.release_name)
    except ValueError as exc:
        print(f"error: {exc}", file=sys.stderr)
        return 1

    entry = os_list[0]
    entry["name"] = display_name
    entry["url"] = args.url
    entry["image_download_size"] = int(args.download_size)
    entry["image_download_sha256"] = args.download_sha.strip().lower()
    entry["extract_size"] = int(args.extract_size)
    entry["extract_sha256"] = args.extract_sha.strip().lower()
    entry["release_date"] = args.release_date

    # Minimal sanity checks before writing.
    for key in (
        "name",
        "url",
        "image_download_size",
        "image_download_sha256",
        "extract_size",
        "extract_sha256",
        "release_date",
    ):
        if entry.get(key) in (None, "", 0):
            print(f"error: refusing to write empty value for {key}", file=sys.stderr)
            return 1

    json_path.write_text(json.dumps(payload, indent=2) + "\n", encoding="utf-8")
    print(f"updated {json_path}: {display_name}")
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
