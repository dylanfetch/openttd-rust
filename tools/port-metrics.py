#!/usr/bin/env python3
"""Print the roadmap progress metric for the current branch.

Counts, from the merge base with the target branch to HEAD (comments and
blank lines included):

  rust     lines added under rust/
  tooling  lines added under tools/
  glue     lines added under src/ that are compiled when WITH_RUST is defined
  retired  net increase in src/ lines compiled only without WITH_RUST (the
           original C++ kept as the portable fallback), plus deleted src/ files

Both guard forms are understood: `#ifdef WITH_RUST ... #else <original>
#endif` and `#ifndef WITH_RUST <original> #endif` (and the `defined()` forms).
"""

import argparse
import json
import re
import subprocess
from pathlib import Path

ROOT = Path(__file__).resolve().parents[1]
DIRECTIVE = re.compile(r"^\s*#\s*(if|ifdef|ifndef|elif|else|endif)\b(.*)")
POSITIVE = re.compile(r"^\s*(?:defined\s*\(?\s*WITH_RUST\s*\)?|WITH_RUST)\s*$")
NEGATIVE = re.compile(r"^\s*!\s*defined\s*\(?\s*WITH_RUST\s*\)?\s*$")


def git(*args):
    return subprocess.check_output(["git", *args], cwd=ROOT, text=True, errors="replace")


def classify(text):
    """Tag every line 'cpp' (only without WITH_RUST), 'rust' or 'common'."""
    stack, tags = [], []
    for line in text.splitlines():
        match = DIRECTIVE.match(line)
        kind, rest = (match.group(1), re.sub(r"/\*.*?\*/", " ", match.group(2)).split("//")[0].strip()) if match else (None, "")
        if kind in ("if", "ifdef", "ifndef"):
            if (kind == "ifdef" and rest == "WITH_RUST") or (kind == "if" and POSITIVE.match(rest)):
                frame = "rust"
            elif (kind == "ifndef" and rest == "WITH_RUST") or (kind == "if" and NEGATIVE.match(rest)):
                frame = "cpp"
            else:
                frame = None
            stack.append([frame, False])
            tags.append("rust" if frame else state(stack[:-1]))
            continue
        if kind in ("elif", "else") and stack:
            frame, switched = stack[-1]
            tags.append("rust" if frame else state(stack[:-1]))
            if frame and not switched:
                # Every later branch of a WITH_RUST conditional is the other side.
                stack[-1] = ["cpp" if frame == "rust" else "rust", True]
            continue
        if kind == "endif" and stack:
            frame, _ = stack.pop()
            tags.append("rust" if frame else state(stack))
            continue
        tags.append(state(stack))
    return tags


def state(stack):
    frames = [frame for frame, _ in stack]
    if "cpp" in frames:
        return "cpp"
    return "rust" if "rust" in frames else "common"


def show(rev, path):
    result = subprocess.run(["git", "show", f"{rev}:{path}"], cwd=ROOT, capture_output=True, text=True, errors="replace")
    return result.stdout if result.returncode == 0 else ""


def added_lines(base, head, old, new):
    """Return 1-based line numbers of `new` at head not present in `old` at base."""
    numbers = []
    for line in git("diff", "-U0", "-M", base, head, "--", *dict.fromkeys((old, new))).splitlines():
        hunk = re.match(r"^@@ -\S+ \+(\d+)(?:,(\d+))? @@", line)
        if hunk:
            start, count = int(hunk.group(1)), int(hunk.group(2) or "1")
            numbers.extend(range(start, start + count))
    return numbers


def changes(base, head):
    """Yield (status, old path, new path) with renames paired, NUL-safe."""
    fields = git("diff", "--name-status", "-z", "-M", base, head).split("\0")
    i = 0
    while i < len(fields) - 1:
        status = fields[i]
        if status[0] in "RC":
            yield status[0], fields[i + 1], fields[i + 2]
            i += 3
        else:
            yield status[0], fields[i + 1], fields[i + 1]
            i += 2


def metrics(base, head="HEAD"):
    result = {"rust": 0, "tooling": 0, "glue": 0, "retired": 0}
    sources = (".cpp", ".h", ".hpp", ".c", ".cc", ".mm")
    retired_delta = 0
    for status, old, new in changes(base, head):
        before, after = ("" if status == "A" else show(base, old)), ("" if status == "D" else show(head, new))
        # Lines of `new` that do not come from `old` (all of them for an added file).
        numbers = [] if status == "D" else added_lines(base, head, old, new)
        if new.startswith(("rust/", "tools/")):
            result["rust" if new.startswith("rust/") else "tooling"] += len(numbers)
        if not (old.startswith("src/") or new.startswith("src/")) or not new.endswith(sources):
            continue
        if status == "D":
            # Deleting original C++ retires it; deleting earlier glue does not.
            retired_delta += sum(1 for tag in classify(before) if tag != "rust")
            continue
        head_tags, base_tags = classify(after), classify(before)
        retired_delta += head_tags.count("cpp") - base_tags.count("cpp")
        result["glue"] += sum(1 for n in numbers if n <= len(head_tags) and head_tags[n - 1] != "cpp")
    result["retired"] = max(0, retired_delta)
    return result





def main():
    parser = argparse.ArgumentParser(description=__doc__.split("\n\n")[0])
    parser.add_argument("--target", default="origin/rust-migration", help="branch the PR targets (default: origin/rust-migration)")
    parser.add_argument("--head", default="HEAD", help="revision to measure (default: HEAD)")
    parser.add_argument("--json", action="store_true")
    args = parser.parse_args()
    base = git("merge-base", args.target, args.head).strip()
    result = metrics(base, args.head)
    if args.json:
        print(json.dumps(dict(result, base=base)))
    else:
        print(f"Rust added: {result['rust']} | tooling added: {result['tooling']} | "
              f"C++ glue added: {result['glue']} | C++ retired: {result['retired']}")


if __name__ == "__main__":
    main()
