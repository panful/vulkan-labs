#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

MAX_FILE_SIZE_BYTES = 1024 * 1024
SKIPPED_ROOTS = {"assets", "third_party"}


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


def main() -> int:
    failed = False

    for file_path in candidate_files(sys.argv[1:]):
        if not file_path.is_file():
            continue
        if file_path.parts and file_path.parts[0] in SKIPPED_ROOTS:
            continue

        file_size = file_path.stat().st_size
        if file_size <= MAX_FILE_SIZE_BYTES:
            continue

        print(
            f"Error: {file_path} is {file_size} bytes, which exceeds the {MAX_FILE_SIZE_BYTES} byte limit",
            file=sys.stderr,
        )
        failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
