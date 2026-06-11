#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path


def staged_files() -> list[Path]:
    result = subprocess.run(
        ["git", "diff", "--cached", "--name-only", "--diff-filter=ACM"],
        capture_output=True,
        text=True,
        check=True,
    )
    return [Path(file_name) for file_name in result.stdout.splitlines() if file_name]


def candidate_files(argv: list[str]) -> list[Path]:
    if argv:
        return [Path(file_name) for file_name in argv]
    return staged_files()


def is_text_file(path: Path) -> bool:
    try:
        with path.open("rb") as file_handle:
            chunk = file_handle.read(1024)
    except OSError:
        return False
    return b"\0" not in chunk


def has_utf8_bom(path: Path) -> bool:
    with path.open("rb") as file_handle:
        return file_handle.read(3) == b"\xef\xbb\xbf"


def is_utf8_or_ascii(path: Path) -> bool:
    try:
        path.read_text(encoding="utf-8")
        return True
    except UnicodeDecodeError:
        return False


def main() -> int:
    failed = False

    for file_path in candidate_files(sys.argv[1:]):
        if not file_path.is_file():
            continue

        if not is_text_file(file_path):
            continue

        if has_utf8_bom(file_path):
            print(f"Error: {file_path} contains UTF-8 BOM", file=sys.stderr)
            failed = True
            continue

        if not is_utf8_or_ascii(file_path):
            print(f"Error: {file_path} is not UTF-8 / ASCII encoded", file=sys.stderr)
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
