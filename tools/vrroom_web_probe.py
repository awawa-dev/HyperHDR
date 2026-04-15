#!/usr/bin/env python3

from __future__ import annotations

import argparse
import hashlib
import json
import mimetypes
import re
import sys
from collections import deque
from dataclasses import dataclass, field
from html.parser import HTMLParser
from pathlib import Path
from typing import Any
from urllib.error import HTTPError, URLError
from urllib.parse import ParseResult, urljoin, urlparse, urlunparse
from urllib.request import Request, urlopen


KEYWORDS = (
	"hdr",
	"hdr10",
	"hlg",
	"dv",
	"dolby",
	"lldv",
	"bt2020",
	"bt709",
	"ictcp",
	"smpte2113rgb",
	"avi",
	"edid",
	"source",
	"sink",
	"tx0",
	"tx1",
	"rx0",
	"rx1",
	"vrr",
	"allm",
	"frl",
	"tmds",
	"444",
	"422",
	"420",
	"rgb",
	"ycc",
	"metadata",
	"status",
	"route",
	"earc",
	"arc",
)

STATUS_LABELS = (
	"SOURCE",
	"IN TX0",
	"IN TX1",
	"VIDEO TX0",
	"VIDEO TX1",
	"AUDIO TX0",
	"AUDIO TX1",
	"AUDIO OUT",
	"eARC RX",
	"ROUTE",
	"EDID TX0",
	"EDID TX1",
	"TX0 SINK",
	"TX1 SINK",
	"OPMODE",
	"MODE",
	"FW",
	"IP Address",
	"Hostname",
)

URL_ATTRS = {
	"a": ("href",),
	"link": ("href",),
	"script": ("src",),
	"img": ("src",),
	"iframe": ("src",),
	"frame": ("src",),
	"source": ("src", "srcset"),
	"form": ("action",),
	"input": ("src",),
	"video": ("src", "poster"),
	"audio": ("src",),
}

TEXT_URL_PATTERNS = (
	re.compile(r"url\((['\"]?)([^)'\"]+)\1\)", flags=re.IGNORECASE),
	re.compile(r"['\"]((?:/|https?://)[^'\"]+)['\"]"),
	re.compile(r"\b(fetch|open|ajax|load|getJSON)\s*\(\s*['\"]([^'\"]+)['\"]", flags=re.IGNORECASE),
)

HTML_MARKERS = (b"<html", b"<!doctype html", b"<head", b"<body")


def normalize_space(value: str) -> str:
	return re.sub(r"\s+", " ", value).strip()


def looks_interesting(value: str) -> bool:
	text = value.lower()
	return any(keyword in text for keyword in KEYWORDS)


def canonicalize_url(url: str) -> str:
	parsed = urlparse(url)
	path = parsed.path or "/"
	normalized = ParseResult(
		scheme=(parsed.scheme or "http").lower(),
		netloc=parsed.netloc.lower(),
		path=path,
		params="",
		query=parsed.query,
		fragment="",
	)
	return urlunparse(normalized)


def url_path_slug(url: str) -> str:
	parsed = urlparse(url)
	path = parsed.path or "/"
	if path.endswith("/"):
		path = path + "index.html"
	if not Path(path).suffix:
		path = path + ".html"
	path = path.lstrip("/") or "index.html"
	if parsed.query:
		query_slug = re.sub(r"[^A-Za-z0-9._-]+", "_", parsed.query).strip("_")
		stem = Path(path).stem
		suffix = Path(path).suffix
		parent = Path(path).parent
		path = str(parent / f"{stem}__q_{query_slug}{suffix}") if query_slug else path
	return path


def content_is_html(content_type: str, data: bytes, url: str) -> bool:
	content_type = (content_type or "").lower()
	if "html" in content_type or "xhtml" in content_type:
		return True
	path = urlparse(url).path.lower()
	if path.endswith((".html", ".htm", ".xhtml", "/")):
		return True
	leading = data[:512].lower()
	return any(marker in leading for marker in HTML_MARKERS)


@dataclass
class ProbeState:
	title: str = ""
	base_url: str = ""
	scripts: list[str] = field(default_factory=list)
	stylesheets: list[str] = field(default_factory=list)
	links: list[str] = field(default_factory=list)
	forms: list[dict[str, Any]] = field(default_factory=list)
	interesting_fields: list[dict[str, Any]] = field(default_factory=list)
	text_chunks: list[str] = field(default_factory=list)
	discovered_urls: set[str] = field(default_factory=set)


class VrroomHtmlProbe(HTMLParser):
	def __init__(self, base_url: str):
		super().__init__(convert_charrefs=True)
		self.state = ProbeState(base_url=base_url)
		self._skip_depth = 0
		self._capture_title = False
		self._current_form: dict[str, Any] | None = None

	def _record_url(self, candidate: str) -> None:
		candidate = normalize_space(candidate)
		if not candidate or candidate.startswith(("javascript:", "mailto:", "tel:", "#", "data:")):
			return
		resolved = canonicalize_url(urljoin(self.state.base_url, candidate))
		self.state.discovered_urls.add(resolved)

	def _record_urls_from_attr(self, attr_value: str) -> None:
		for token in re.split(r"\s*,\s*|\s+", attr_value.strip()):
			if not token:
				continue
			url_part = token.split()[0]
			if url_part:
				self._record_url(url_part)

	def handle_starttag(self, tag: str, attrs: list[tuple[str, str | None]]) -> None:
		attr_map = {key: (value or "") for key, value in attrs}
		if tag in {"script", "style", "noscript"}:
			self._skip_depth += 1
		if tag == "title":
			self._capture_title = True

		for attr_name in URL_ATTRS.get(tag, ()):
			attr_value = attr_map.get(attr_name, "")
			if attr_value:
				self._record_urls_from_attr(attr_value)

		if tag == "script" and attr_map.get("src"):
			self.state.scripts.append(canonicalize_url(urljoin(self.state.base_url, attr_map["src"])))
		elif tag == "link":
			href = attr_map.get("href", "")
			rel = attr_map.get("rel", "").lower()
			if href:
				resolved = canonicalize_url(urljoin(self.state.base_url, href))
				self.state.links.append(resolved)
				if "stylesheet" in rel:
					self.state.stylesheets.append(resolved)

		if tag == "form":
			self._current_form = {
				"id": attr_map.get("id", ""),
				"name": attr_map.get("name", ""),
				"method": attr_map.get("method", "get").lower(),
				"action": canonicalize_url(urljoin(self.state.base_url, attr_map.get("action", ""))),
				"inputs": [],
			}
			self.state.forms.append(self._current_form)

		if tag in {"input", "select", "textarea", "button"}:
			field_info = {
				"tag": tag,
				"id": attr_map.get("id", ""),
				"name": attr_map.get("name", ""),
				"type": attr_map.get("type", ""),
				"value": normalize_space(attr_map.get("value", "")),
			}
			if self._current_form is not None:
				self._current_form["inputs"].append(field_info)
			joined = " ".join(part for part in field_info.values() if isinstance(part, str))
			if looks_interesting(joined):
				self.state.interesting_fields.append(field_info)

	def handle_endtag(self, tag: str) -> None:
		if tag in {"script", "style", "noscript"} and self._skip_depth > 0:
			self._skip_depth -= 1
		if tag == "title":
			self._capture_title = False
		if tag == "form":
			self._current_form = None

	def handle_data(self, data: str) -> None:
		if self._skip_depth > 0:
			return
		text = normalize_space(data)
		if not text:
			return
		if self._capture_title:
			self.state.title = text
		self.state.text_chunks.append(text)
		for pattern in TEXT_URL_PATTERNS:
			for match in pattern.finditer(text):
				candidate = match.group(match.lastindex or 1)
				self._record_url(candidate)


@dataclass
class FetchResult:
	url: str
	status: int
	content_type: str
	data: bytes
	headers: dict[str, str]
	error: str = ""


def fetch_url(url: str, timeout: float) -> FetchResult:
	request = Request(
		url,
		headers={
			"User-Agent": "vrroom-web-probe/0.2",
			"Accept": "*/*",
		},
	)
	try:
		with urlopen(request, timeout=timeout) as response:
			return FetchResult(
				url=url,
				status=getattr(response, "status", 200),
				content_type=response.headers.get("Content-Type", ""),
				data=response.read(),
				headers={key: value for key, value in response.headers.items()},
			)
	except HTTPError as exc:
		payload = exc.read()
		return FetchResult(
			url=url,
			status=exc.code,
			content_type=exc.headers.get("Content-Type", "") if exc.headers else "",
			data=payload,
			headers={key: value for key, value in (exc.headers.items() if exc.headers else [])},
			error=f"http error {exc.code}",
		)
	except URLError as exc:
		return FetchResult(
			url=url,
			status=0,
			content_type="",
			data=b"",
			headers={},
			error=f"url error: {exc.reason}",
		)


def extract_keyword_lines(chunks: list[str]) -> list[str]:
	seen: set[str] = set()
	results: list[str] = []
	for chunk in chunks:
		if len(chunk) < 4:
			continue
		if not looks_interesting(chunk):
			continue
		if chunk in seen:
			continue
		seen.add(chunk)
		results.append(chunk)
	return results


def extract_status_fields(page_text: str) -> dict[str, str]:
	collapsed = normalize_space(page_text)
	if not collapsed:
		return {}

	positions: list[tuple[int, str]] = []
	for label in STATUS_LABELS:
		for match in re.finditer(re.escape(label) + r"\s*:", collapsed, flags=re.IGNORECASE):
			positions.append((match.start(), label))
			break

	positions.sort()
	results: dict[str, str] = {}
	for index, (start, label) in enumerate(positions):
		match = re.search(re.escape(label) + r"\s*:", collapsed[start:], flags=re.IGNORECASE)
		if match is None:
			continue
		value_start = start + match.end()
		value_end = positions[index + 1][0] if index + 1 < len(positions) else len(collapsed)
		value = normalize_space(collapsed[value_start:value_end])
		if value:
			results[label] = value
	return results


def same_origin(url: str, root: str) -> bool:
	left = urlparse(url)
	right = urlparse(root)
	return (left.scheme.lower(), left.netloc.lower()) == (right.scheme.lower(), right.netloc.lower())


def write_download(download_dir: Path, url: str, data: bytes, content_type: str) -> str:
	relative = Path(url_path_slug(url))
	target = download_dir / relative
	target.parent.mkdir(parents=True, exist_ok=True)
	if not target.suffix and content_type:
		ext = mimetypes.guess_extension(content_type.split(";", 1)[0].strip()) or ""
		if ext:
			target = target.with_suffix(ext)
	target.write_bytes(data)
	return target.as_posix()


def summarize_html(url: str, html_text: str) -> dict[str, Any]:
	probe = VrroomHtmlProbe(url)
	probe.feed(html_text)
	page_text = "\n".join(probe.state.text_chunks)
	return {
		"title": probe.state.title,
		"status_fields": extract_status_fields(page_text),
		"keyword_lines": extract_keyword_lines(probe.state.text_chunks),
		"interesting_fields": probe.state.interesting_fields,
		"forms": probe.state.forms,
		"scripts": sorted(set(probe.state.scripts)),
		"stylesheets": sorted(set(probe.state.stylesheets)),
		"discovered_urls": sorted(probe.state.discovered_urls),
	}


def crawl_site(root_url: str, timeout: float, max_files: int, download_dir: Path | None) -> dict[str, Any]:
	root_url = canonicalize_url(root_url)
	queue: deque[str] = deque([root_url])
	seen: set[str] = set()
	manifest: list[dict[str, Any]] = []
	landing_summary: dict[str, Any] | None = None

	while queue and len(seen) < max_files:
		url = queue.popleft()
		if url in seen:
			continue
		if not same_origin(url, root_url):
			continue
		seen.add(url)

		fetched = fetch_url(url, timeout)
		entry: dict[str, Any] = {
			"url": url,
			"status": fetched.status,
			"content_type": fetched.content_type,
			"size": len(fetched.data),
			"error": fetched.error,
			"sha256": hashlib.sha256(fetched.data).hexdigest() if fetched.data else "",
		}

		if download_dir is not None and fetched.data:
			entry["download_path"] = write_download(download_dir, url, fetched.data, fetched.content_type)

		if fetched.status and 200 <= fetched.status < 400 and fetched.data:
			if content_is_html(fetched.content_type, fetched.data, url):
				html_text = fetched.data.decode("utf-8", errors="replace")
				html_summary = summarize_html(url, html_text)
				entry["html_summary"] = html_summary
				for discovered in html_summary["discovered_urls"]:
					if discovered not in seen and same_origin(discovered, root_url):
						queue.append(discovered)
				if landing_summary is None and url == root_url:
					landing_summary = html_summary
			else:
				text_preview = ""
				try:
					decoded = fetched.data.decode("utf-8", errors="replace")
					text_preview = decoded[:2000]
					for pattern in TEXT_URL_PATTERNS:
						for match in pattern.finditer(decoded):
							candidate = match.group(match.lastindex or 1)
							resolved = canonicalize_url(urljoin(url, candidate))
							if resolved not in seen and same_origin(resolved, root_url):
								queue.append(resolved)
				except UnicodeDecodeError:
					text_preview = ""
				if text_preview:
					entry["text_preview"] = text_preview

		manifest.append(entry)

	return {
		"root_url": root_url,
		"fetched_count": len(manifest),
		"max_files": max_files,
		"landing_page": landing_summary or {},
		"files": manifest,
	}


def build_argument_parser() -> argparse.ArgumentParser:
	parser = argparse.ArgumentParser(description="Crawl the VRRoom local web UI and optionally download every reachable same-host file.")
	parser.add_argument("--url", default="http://VRROOM-09/", help="VRRoom base URL")
	parser.add_argument("--timeout", type=float, default=10.0, help="HTTP timeout in seconds")
	parser.add_argument("--max-files", type=int, default=256, help="Maximum same-host URLs to fetch")
	parser.add_argument("--out", help="Write JSON manifest to this file")
	parser.add_argument("--download-dir", help="Directory to save fetched files")
	parser.add_argument("--pretty", action="store_true", help="Pretty-print JSON output")
	return parser


def main(argv: list[str] | None = None) -> int:
	parser = build_argument_parser()
	args = parser.parse_args(argv)
	download_dir = Path(args.download_dir) if args.download_dir else None
	if download_dir is not None:
		download_dir.mkdir(parents=True, exist_ok=True)

	result = crawl_site(args.url, args.timeout, max(1, args.max_files), download_dir)
	json_text = json.dumps(result, indent=2 if args.pretty else None, sort_keys=True)

	if args.out:
		Path(args.out).write_text(json_text + ("\n" if not json_text.endswith("\n") else ""), encoding="utf-8")
	else:
		print(json_text)

	return 0


if __name__ == "__main__":
	raise SystemExit(main())