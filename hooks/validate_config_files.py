#!/usr/bin/env python3
from __future__ import annotations

import json
import subprocess
import sys
from pathlib import Path

import yaml

CONFIG_SUFFIXES = {".json", ".yml", ".yaml"}


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
        if not file_path.is_file() or file_path.suffix.lower() not in CONFIG_SUFFIXES:
            continue

        try:
            with file_path.open("r", encoding="utf-8") as file_handle:
                if file_path.suffix.lower() == ".json":
                    json.load(file_handle)
                else:
                    yaml.safe_load(file_handle)
        except (json.JSONDecodeError, yaml.YAMLError, UnicodeDecodeError) as error:
            print(f"Error: {file_path} is invalid: {error}", file=sys.stderr)
            failed = True

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
