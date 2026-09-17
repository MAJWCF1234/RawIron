#!/usr/bin/env python3
"""Local anonymous mill board. Bind loopback only. File-backed."""

from __future__ import annotations

import json
import os
import re
import sys
import threading
import time
import uuid
from http.server import SimpleHTTPRequestHandler, ThreadingHTTPServer
from pathlib import Path
from urllib.parse import urlparse

ROOT = Path(__file__).resolve().parent
STATIC = ROOT / "static"
DATA = ROOT / "data"
BOARD_PATH = DATA / "board.json"
INBOX = DATA / "inbox"
HOST = "127.0.0.1"
PORT = 7420

LOCK = threading.Lock()
MAX_TITLE = 120
MAX_BODY = 8000
MAX_MARK = 32
TAB_IDS = ("floor", "forge", "hearth", "mill", "plans")

ID_RE = re.compile(r"^[A-Za-z0-9_\-]{1,40}$")


def utc_now() -> str:
    return time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())


def new_id(prefix: str) -> str:
    return f"{prefix}_{uuid.uuid4().hex[:10]}"


def clamp_text(value: object, limit: int) -> str:
    text = str(value or "").replace("\r\n", "\n").strip()
    if len(text) > limit:
        return text[:limit]
    return text


def load_board() -> dict:
    DATA.mkdir(parents=True, exist_ok=True)
    INBOX.mkdir(parents=True, exist_ok=True)
    if not BOARD_PATH.exists():
        raise FileNotFoundError(f"missing board at {BOARD_PATH}")
    with BOARD_PATH.open("r", encoding="utf-8") as handle:
        board = json.load(handle)
    board.setdefault("tabs", [])
    board.setdefault("threads", [])
    return board


def save_board(board: dict) -> None:
    tmp = BOARD_PATH.with_suffix(".json.tmp")
    with tmp.open("w", encoding="utf-8", newline="\n") as handle:
        json.dump(board, handle, indent=2, ensure_ascii=False)
        handle.write("\n")
    os.replace(tmp, BOARD_PATH)


def ingest_inbox(board: dict) -> None:
    if not INBOX.exists():
        return
    drops = sorted(
        path for path in INBOX.iterdir() if path.is_file() and path.suffix.lower() == ".json"
    )
    for path in drops:
        try:
            with path.open("r", encoding="utf-8") as handle:
                payload = json.load(handle)
            apply_post(board, payload, create_thread=True)
            path.unlink()
        except Exception as error:  # noqa: BLE001 — leave the drop so it can be fixed
            fail = path.with_suffix(path.suffix + ".error.txt")
            fail.write_text(f"{error}\n", encoding="utf-8")


def find_thread(board: dict, thread_id: str) -> dict | None:
    for thread in board["threads"]:
        if thread.get("id") == thread_id:
            return thread
    return None


def find_thread_by_title(board: dict, tab: str, title: str) -> dict | None:
    needle = title.casefold()
    for thread in board["threads"]:
        if thread.get("tab") == tab and str(thread.get("title", "")).casefold() == needle:
            return thread
    return None


def apply_post(board: dict, payload: dict, create_thread: bool) -> dict:
    tab = str(payload.get("tab") or "").strip().lower()
    if tab not in TAB_IDS:
        raise ValueError("unknown tab")
    body = clamp_text(payload.get("body"), MAX_BODY)
    if not body:
        raise ValueError("empty body")
    mark = clamp_text(payload.get("mark"), MAX_MARK)
    post = {"id": new_id("p"), "at": utc_now(), "mark": mark, "body": body}

    thread_id = str(payload.get("threadId") or payload.get("thread_id") or "").strip()
    title = clamp_text(payload.get("title") or payload.get("thread"), MAX_TITLE)

    thread = None
    if thread_id:
        if not ID_RE.match(thread_id):
            raise ValueError("bad thread id")
        thread = find_thread(board, thread_id)
        if thread is None:
            raise ValueError("missing thread")
        if thread.get("tab") != tab:
            raise ValueError("thread is on another tab")
    elif title:
        thread = find_thread_by_title(board, tab, title)
        if thread is None:
            if not create_thread:
                raise ValueError("missing thread")
            thread = {
                "id": new_id("thr"),
                "tab": tab,
                "title": title,
                "created": utc_now(),
                "posts": [],
            }
            board["threads"].insert(0, thread)
    else:
        raise ValueError("need threadId or title")

    thread.setdefault("posts", []).append(post)
    return {"thread": thread, "post": post}


class Handler(SimpleHTTPRequestHandler):
    def __init__(self, *args, **kwargs):
        super().__init__(*args, directory=str(STATIC), **kwargs)

    def log_message(self, format: str, *args) -> None:  # noqa: A003
        sys.stderr.write("%s - %s\n" % (self.address_string(), format % args))

    def _json(self, code: int, payload: dict) -> None:
        raw = json.dumps(payload, ensure_ascii=False).encode("utf-8")
        self.send_response(code)
        self.send_header("Content-Type", "application/json; charset=utf-8")
        self.send_header("Content-Length", str(len(raw)))
        self.send_header("Cache-Control", "no-store")
        self.end_headers()
        self.wfile.write(raw)

    def _read_json(self) -> dict:
        length = int(self.headers.get("Content-Length") or "0")
        if length <= 0 or length > 64_000:
            raise ValueError("bad body")
        raw = self.rfile.read(length)
        data = json.loads(raw.decode("utf-8"))
        if not isinstance(data, dict):
            raise ValueError("object required")
        return data

    def do_GET(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        if parsed.path == "/api/board":
            with LOCK:
                board = load_board()
                ingest_inbox(board)
                save_board(board)
            self._json(200, board)
            return
        if parsed.path == "/favicon.ico":
            self.send_response(204)
            self.end_headers()
            return
        if parsed.path in ("/", "/index.html"):
            self.path = "/index.html"
        super().do_GET()

    def do_POST(self) -> None:  # noqa: N802
        parsed = urlparse(self.path)
        try:
            payload = self._read_json()
            with LOCK:
                board = load_board()
                ingest_inbox(board)
                if parsed.path == "/api/threads":
                    payload = {
                        "tab": payload.get("tab"),
                        "title": payload.get("title"),
                        "body": payload.get("body"),
                        "mark": payload.get("mark"),
                    }
                    result = apply_post(board, payload, create_thread=True)
                elif parsed.path == "/api/posts":
                    result = apply_post(board, payload, create_thread=False)
                else:
                    self._json(404, {"error": "unknown route"})
                    return
                save_board(board)
            self._json(200, result)
        except (ValueError, json.JSONDecodeError) as error:
            self._json(400, {"error": str(error)})
        except FileNotFoundError as error:
            self._json(500, {"error": str(error)})


def main() -> int:
    STATIC.mkdir(parents=True, exist_ok=True)
    DATA.mkdir(parents=True, exist_ok=True)
    INBOX.mkdir(parents=True, exist_ok=True)
    server = ThreadingHTTPServer((HOST, PORT), Handler)
    print(f"DEVCOMS  http://{HOST}:{PORT}", flush=True)
    print("Anonymous. Loopback only. Drop JSON in data/inbox to post without the UI.", flush=True)
    try:
        server.serve_forever()
    except KeyboardInterrupt:
        print("\nstopped", flush=True)
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
