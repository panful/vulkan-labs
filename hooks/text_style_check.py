#!/usr/bin/env python3
from __future__ import annotations

import subprocess
import sys
from pathlib import Path

# Match the repository line-ending policy:
# - Windows-specific scripts use CRLF
# - Other text files, including documentation, use LF
WINDOWS_CRLF_SUFFIXES = {".bat", ".cmd"}


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


def normalize_lines(data: bytes, file_path: Path) -> bytes:
    if not data:
        return data

    newline = b"\r\n" if file_path.suffix.lower() in WINDOWS_CRLF_SUFFIXES else b"\n"
    normalized_lines: list[bytes] = []

    for raw_line in data.splitlines(keepends=True):
        content = raw_line.rstrip(b"\r\n").rstrip(b" \t")
        if raw_line.endswith(b"\r\n"):
            normalized_lines.append(content + newline)
        elif raw_line.endswith((b"\n", b"\r")):
            normalized_lines.append(content + newline)
        else:
            normalized_lines.append(content)

    normalized_data = b"".join(normalized_lines)
    if normalized_data and not normalized_data.endswith(newline):
        normalized_data += newline

    return normalized_data


def main() -> int:
    modified_files: list[Path] = []

    for file_path in candidate_files(sys.argv[1:]):
        if not file_path.is_file() or not is_text_file(file_path):
            continue

        original_data = file_path.read_bytes()
        normalized_data = normalize_lines(original_data, file_path)
        if normalized_data == original_data:
            continue

        file_path.write_bytes(normalized_data)
        modified_files.append(file_path)

    if not modified_files:
        return 0

    for file_path in modified_files:
        print(f"Fixed text style issues in {file_path}", file=sys.stderr)

    print("Text style hook modified files. Please review and re-run pre-commit.", file=sys.stderr)
    return 1


if __name__ == "__main__":
    raise SystemExit(main())
