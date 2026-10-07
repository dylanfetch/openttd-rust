#!/usr/bin/env python3
"""Print the main-thread text of Claude Code sessions for this project.

Usage: transcript.py            list sessions, newest first, with first prompt
       transcript.py <id|path>  print user and assistant text of one session
"""

import json
import sys
from pathlib import Path

DIR = Path.home() / ".claude/projects/-home-fetch-projects-openttd-rust"


def texts(path):
    for line in path.open():
        try:
            d = json.loads(line)
        except ValueError:
            continue
        if d.get("isSidechain") or d.get("type") not in ("user", "assistant"):
            continue
        c = d["message"].get("content")
        blocks = [{"type": "text", "text": c}] if isinstance(c, str) else c or []
        for b in blocks:
            if b.get("type") == "text":
                yield d["type"], d.get("timestamp", ""), b["text"]


def prompt(path):
    for role, _, t in texts(path):
        if role == "user" and not t.startswith("<") and "task-notification" not in t:
            return t.replace("\n", " ")[:160]
    return ""


if len(sys.argv) < 2:
    for p in sorted(DIR.glob("*.jsonl"), key=lambda p: p.stat().st_mtime, reverse=True):
        print(p.stem, p.stat().st_size // 1024, "KB |", prompt(p))
else:
    a = Path(sys.argv[1])
    p = a if a.exists() else DIR / f"{sys.argv[1]}.jsonl"
    for role, ts, t in texts(p):
        print(f"[{role} {ts[:16]}] {t}\n")
