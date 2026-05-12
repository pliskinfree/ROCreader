#!/usr/bin/env python3
import json
import re
import sys
from urllib.parse import urlsplit

from curl_cffi import requests


def session():
    return requests.Session(impersonate="chrome120")


def fetch(url, referer=""):
    headers = {"User-Agent": "ROCreader-OPDS"}
    if referer:
        headers["Referer"] = referer
    res = session().get(url, headers=headers, timeout=20)
    res.raise_for_status()
    sys.stdout.buffer.write(res.content)


def download(url, output_path, referer=""):
    headers = {"User-Agent": "ROCreader-OPDS"}
    if referer:
        headers["Referer"] = referer
    res = session().get(url, headers=headers, timeout=(20, 300), stream=True)
    res.raise_for_status()
    with open(output_path, "wb") as f:
        for chunk in res.iter_content(chunk_size=1024 * 256):
            if chunk:
                f.write(chunk)


def emit_error(step, detail=""):
    payload = {"error": step}
    if detail:
        payload["detail"] = detail
    print(json.dumps(payload, ensure_ascii=False))


def origin_from_url(url):
    parts = urlsplit(url)
    if not parts.scheme or not parts.netloc:
        return "https://www.wn04.cfd/"
    return f"{parts.scheme}://{parts.netloc}/"


def resolve(detail_url, title, source_url):
    s = session()
    try:
        detail = s.get(detail_url, headers={"Referer": source_url, "User-Agent": "ROCreader-OPDS"}, timeout=20)
        detail.raise_for_status()
    except Exception as exc:
        emit_error("detail_fetch_failed", str(exc))
        return
    match = re.search(r'<a[^>]+href\s*=\s*[\'"]([^\'"]*/download-index-aid-[^\'"]*)[\'"]', detail.text, re.I)
    if not match:
        emit_error("download_landing_link_not_found")
        return
    landing_url = requests.utils.urljoin(detail_url, match.group(1))
    try:
        landing = s.get(landing_url, headers={"Referer": detail_url, "User-Agent": "ROCreader-OPDS"}, timeout=20)
        landing.raise_for_status()
    except Exception as exc:
        emit_error("landing_fetch_failed", str(exc))
        return
    key_match = re.search(r'[\'"](down/[^\'"]+\.zip)[\'"]', landing.text, re.I)
    if not key_match:
        emit_error("download_key_not_found")
        return
    api = "https://d1.wcdn.date/api/generate-link"
    file_name = re.sub(r'[\\/:*?"<>|\r\n\t]+', "_", title).strip(" _.") or "online_book"
    try:
        api_res = s.post(
            api,
            json={"file_key": key_match.group(1), "file_name": file_name + ".zip"},
            headers={
                "Origin": source_url.rstrip("/"),
                "Referer": landing_url,
                "User-Agent": "ROCreader-OPDS",
            },
            timeout=20,
        )
        api_res.raise_for_status()
        real_url = api_res.json().get("url", "")
    except Exception as exc:
        emit_error("generate_link_failed", str(exc))
        return
    if not real_url:
        emit_error("real_download_url_not_found")
        return
    print(json.dumps({"url": real_url}, ensure_ascii=False))


def main():
    if len(sys.argv) < 3:
        raise SystemExit("usage: manual_web_fetch.py fetch URL [REFERER] | download URL OUTPUT [REFERER] | resolve DETAIL_URL TITLE SOURCE_URL")
    mode = sys.argv[1]
    if mode == "fetch":
        fetch(sys.argv[2], sys.argv[3] if len(sys.argv) > 3 else "")
    elif mode == "download":
        if len(sys.argv) < 4:
            raise SystemExit("download requires URL OUTPUT")
        download(sys.argv[2], sys.argv[3], sys.argv[4] if len(sys.argv) > 4 else "")
    elif mode == "resolve":
        if len(sys.argv) < 3:
            raise SystemExit("resolve requires DETAIL_URL [TITLE...] [SOURCE_URL]")
        detail_url = sys.argv[2]
        args = sys.argv[3:]
        if args and re.match(r"https?://", args[-1], re.I):
            source_url = args[-1]
            title_parts = args[:-1]
        else:
            source_url = origin_from_url(detail_url)
            title_parts = args
        title = " ".join(title_parts).strip() or "online_book"
        resolve(detail_url, title, source_url)
    else:
        raise SystemExit("unknown mode: " + mode)


if __name__ == "__main__":
    main()
