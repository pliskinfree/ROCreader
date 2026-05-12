#!/usr/bin/env python3
import html
import mimetypes
import os
import re
import sys
from http.server import ThreadingHTTPServer, BaseHTTPRequestHandler
from pathlib import Path
from urllib.parse import quote, unquote


ROOT = Path(__file__).resolve().parents[1]
BOOKS = ROOT / "books"
COVERS = ROOT / "book_covers"
PORT = int(os.environ.get("ROCREADER_LOCAL_OPDS_PORT", "8765"))
BOOK_EXTS = {".cbz", ".zip", ".pdf", ".epub"}
COVER_EXTS = [".png", ".jpg", ".jpeg", ".webp", ".bmp"]


def guess_mime(path: Path) -> str:
    ext = path.suffix.lower()
    if ext == ".cbz":
        return "application/vnd.comicbook+zip"
    if ext == ".zip":
        return "application/zip"
    if ext == ".pdf":
        return "application/pdf"
    if ext == ".epub":
        return "application/epub+zip"
    return mimetypes.guess_type(path.name)[0] or "application/octet-stream"


def ascii_id(path: Path) -> str:
    safe = re.sub(r"[^A-Za-z0-9_.-]+", "_", path.name)
    return "local:" + (safe[:160] or "book")


def iter_books():
    if not BOOKS.exists():
        return []
    out = []
    for path in sorted(BOOKS.iterdir(), key=lambda p: p.name.lower()):
        if path.is_file() and path.suffix.lower() in BOOK_EXTS:
            out.append(path)
    return out[:80]


def find_cover(book: Path):
    stem = book.stem
    for ext in COVER_EXTS:
        candidate = COVERS / f"{stem}{ext}"
        if candidate.exists():
            return candidate
    for path in sorted(COVERS.glob("*")):
        if path.is_file() and path.suffix.lower() in COVER_EXTS:
            return path
    return None


def absolute_url(handler, path: str) -> str:
    return f"http://{handler.headers.get('Host', f'127.0.0.1:{PORT}')}{path}"


class Handler(BaseHTTPRequestHandler):
    def log_message(self, fmt, *args):
        sys.stderr.write("[local-opds] " + fmt % args + "\n")

    def send_bytes(self, data: bytes, content_type: str):
        self.send_response(200)
        self.send_header("Content-Type", content_type)
        self.send_header("Content-Length", str(len(data)))
        self.end_headers()
        self.wfile.write(data)

    def do_GET(self):
        if self.path in ("/", "/opds"):
            self.serve_opds()
            return
        if self.path.startswith("/books/"):
            self.serve_file(BOOKS, self.path[len("/books/"):])
            return
        if self.path.startswith("/covers/"):
            self.serve_file(COVERS, self.path[len("/covers/"):])
            return
        self.send_error(404)

    def serve_opds(self):
        entries = []
        for book in iter_books():
            title = book.stem
            book_url = absolute_url(self, "/books/" + quote(book.name))
            cover = find_cover(book)
            cover_link = ""
            if cover:
                cover_url = absolute_url(self, "/covers/" + quote(cover.name))
                cover_link = f'<link rel="http://opds-spec.org/image" href="{html.escape(cover_url)}" type="{html.escape(guess_mime(cover))}"/>'
            entries.append(
                "<entry>"
                f"<title>{html.escape(title)}</title>"
                f"<id>{html.escape(ascii_id(book))}</id>"
                f"{cover_link}"
                f'<link rel="http://opds-spec.org/acquisition" href="{html.escape(book_url)}" type="{html.escape(guess_mime(book))}"/>'
                "</entry>"
            )
        body = (
            '<?xml version="1.0" encoding="utf-8"?>'
            '<feed xmlns="http://www.w3.org/2005/Atom">'
            "<title>ROCreader Local OPDS</title>"
            + "".join(entries) +
            "</feed>"
        )
        self.send_bytes(body.encode("utf-8"), "application/atom+xml;profile=opds-catalog;charset=utf-8")

    def serve_file(self, root: Path, encoded_name: str):
        name = unquote(encoded_name.split("?", 1)[0])
        path = (root / name).resolve()
        try:
            path.relative_to(root.resolve())
        except ValueError:
            self.send_error(403)
            return
        if not path.exists() or not path.is_file():
            self.send_error(404)
            return
        data = path.read_bytes()
        self.send_bytes(data, guess_mime(path))


if __name__ == "__main__":
    server = ThreadingHTTPServer(("127.0.0.1", PORT), Handler)
    print(f"[local-opds] serving {ROOT} at http://127.0.0.1:{PORT}/opds", flush=True)
    server.serve_forever()
