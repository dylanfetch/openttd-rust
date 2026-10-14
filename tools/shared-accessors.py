#!/usr/bin/env python3
"""Reject new single-read shared-world wrappers; existing fingerprints only shrink."""

import argparse
import hashlib
import json
import re
import subprocess
from collections import Counter
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
BASELINE = "tools/shared-accessor-baseline.txt"
SHARED = re.compile(
    r"^src/rust/(?:services[^/]*|vehicle_services[^/]*|environment_services\.h|"
    r"settings_services\.h|company_services\.h)$"
)
HEADER = re.compile(
    r"(?P<header>\b\w+(?:::\w+)*\s*\([^{};]*\)\s*(?:const\s*)?(?:noexcept\s*)?"
    r"|\[[^\]\n]*\]\s*\([^{};]*\)\s*(?:(?:noexcept|mutable)\s*)?(?:->[^{};]+)?)\{"
)
OWNER = r"(?:Vehicle|BaseStation|Station|Company)"
WORLD = re.compile(
    r"\b(?:_settings_game|_settings_client|_game_mode|_current_company)\b|\bTimerGame\w*::\w+|\bMap::\w+"
)


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True)


def definitions(source):
    # Mask comments/strings without changing brace positions or line boundaries.
    source = re.sub(
        r'/\*.*?\*/|//[^\n]*|"(?:\\.|[^"\\])*"|\'(?:\\.|[^\'\\])*\'',
        lambda m: re.sub(r"[^\n]", " ", m[0]),
        source,
        flags=re.S,
    )
    for match in HEADER.finditer(source):
        header = " ".join(match["header"].split())
        if header.split("(")[0].strip() in {"if", "while", "switch", "for", "catch"}:
            continue
        # A selected single-read body has no nested braces; nested bodies are rejected.
        body = source[match.end() : source.find("}", match.end())]
        yield header, body, source[: match.start()].count("\n") + 1


def single_read(header, body, helpers):
    statements = [s.strip() for s in body.split(";") if s.strip()]
    if not statements or any("{" in s or "}" in s for s in statements):
        return False
    aliases = []
    for statement in statements[:-1]:
        alias = re.fullmatch(r"(?:const )?(\w+)\s*[*&]?\s*(\w+)\s*=\s*(.+)", statement)
        if not alias or not re.search(rf"\b{OWNER}\b", alias[1] + " " + alias[3]):
            return False
        aliases.append(alias[2])
    expression = re.sub(r"^return\s+", "", statements[-1])
    if re.search(r"(?<![=!<>])=(?!=)|\+\+|--|\b(?:new|delete|throw)\b", expression):
        return False
    reads = len(WORLD.findall(expression))
    owners = aliases + re.findall(rf"\b{OWNER}\s*[*&]\s*(\w+)", header)
    reads += sum(
        len(re.findall(rf"\b{owner}\s*(?:->|\.)\s*\w+(?!\w|\s*\()", expression))
        for owner in owners
    )
    reads += len(
        re.findall(rf"\b{OWNER}::(?:Get|GetIfValid)\([^;]*?->\s*\w+", expression)
    )
    reads += len(
        re.findall(
            rf"(?:static|reinterpret)_cast<[^>]*\b{OWNER}\s*\*>\([^)]*\)->\s*\w+",
            expression,
        )
    )
    if reads == 1:
        return True
    # Pass-through includes boundary casts/base() but no extra service calls.
    calls = set(re.findall(r"\b(\w+)\s*\(", expression))
    return bool(calls & helpers) and calls <= helpers | {"base", "to_underlying"}


def collect(files, originals):
    helpers = {
        h.split("(")[0].strip()
        for p, s in files.items()
        if SHARED.fullmatch(p)
        for h, _, _ in definitions(s)
    }
    hits = {}
    for path, source in files.items():
        if SHARED.fullmatch(path):
            continue

        def identity(h, b):
            return h, "".join(b.split()) if h.startswith("[") else ""

        inherited = Counter(
            identity(h, b) for h, b, _ in definitions(originals.get(path, ""))
        )
        occurrences = Counter()
        for header, body, line in definitions(source):
            key = identity(header, body)
            if inherited[key]:
                inherited[key] -= 1
            elif single_read(header, body, helpers):
                digest = hashlib.sha256("".join(body.split()).encode()).hexdigest()[:16]
                key = f"{path} | {header} | {digest}"
                occurrences[key] += 1
                hits[f"{key} | {occurrences[key]}"] = line
    return hits


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--base-ref", help="ratchet against this Git revision")
    args = parser.parse_args()
    pinned = json.loads((ROOT / "migration/baseline.json").read_text())["commit"]
    paths = git("diff", "--name-only", pinned, "--", "src").splitlines()
    files = {
        p: (ROOT / p).read_text()
        for p in paths
        if (ROOT / p).is_file() and Path(p).suffix in {".h", ".hpp", ".cpp"}
    }
    tracked = set(git("ls-tree", "-r", "--name-only", pinned, "src").splitlines())
    originals = {p: git("show", f"{pinned}:{p}") for p in files if p in tracked}
    hits = collect(files, originals)
    baseline = set((ROOT / BASELINE).read_text().splitlines())
    previous = baseline
    if (
        args.base_ref
        and BASELINE in git("ls-tree", "-r", "--name-only", args.base_ref).splitlines()
    ):
        previous = set(git("show", f"{args.base_ref}:{BASELINE}").splitlines())
    new, growth = set(hits) - baseline, baseline - previous
    for key in new:
        print(f"{key.split(' | ')[0]}:{hits[key]}: new shared accessor: {key}")
    stale = baseline - set(hits)
    for label, keys in [("addition forbidden", growth), ("remove retired", stale)]:
        for key in sorted(keys):
            print(f"{BASELINE}: {label}: {key}")
    print("Shared accessors:", len(hits), "hits;", len(new | growth | stale), "errors")
    return bool(new or growth or stale)


if __name__ == "__main__":
    raise SystemExit(main())
