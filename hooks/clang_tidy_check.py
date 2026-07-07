#!/usr/bin/env python3
from __future__ import annotations

import json
import shutil
import subprocess
import sys
from pathlib import Path

SOURCE_SUFFIXES = {".c", ".cc", ".cpp", ".cxx"}
HEADER_SUFFIXES = {".h", ".hpp"}
CPP_SUFFIXES = SOURCE_SUFFIXES | HEADER_SUFFIXES
WARNINGS_AS_ERRORS = ",".join(
    [
        "readability-identifier-naming",
        "clang-analyzer-*",
        "bugprone-*",
        "cppcoreguidelines-init-variables",
        "cppcoreguidelines-narrowing-conversions",
        "cppcoreguidelines-noexcept-destructor",
        "cppcoreguidelines-explicit-virtual-functions",
        "cppcoreguidelines-pro-bounds-array-to-pointer-decay",
        "cppcoreguidelines-pro-type-const-cast",
        "cppcoreguidelines-pro-type-cstyle-cast",
        "cppcoreguidelines-pro-type-reinterpret-cast",
        "cppcoreguidelines-pro-type-static-cast-downcast",
        "cppcoreguidelines-pro-type-vararg",
        "cppcoreguidelines-rvalue-reference-param-not-moved",
        "cppcoreguidelines-slicing",
        "misc-throw-by-value-catch-by-reference",
        "modernize-use-nullptr",
        "modernize-use-override",
        "readability-braces-around-statements",
    ]
)


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


def find_compile_commands(repo_root: Path) -> Path | None:
    preferred_paths = [
        repo_root / "build" / "msvc-debug-tidy-local" / "compile_commands.json",
        repo_root / "build" / "msvc-debug-local" / "compile_commands.json",
        repo_root / "build" / "msvc-release-local" / "compile_commands.json",
        repo_root / "build" / "msvc-debug-tidy" / "compile_commands.json",
        repo_root / "build" / "msvc-debug" / "compile_commands.json",
        repo_root / "build" / "msvc-release" / "compile_commands.json",
    ]

    for candidate in preferred_paths:
        if candidate.is_file():
            return candidate

    matches = sorted(repo_root.glob("build/**/compile_commands.json"))
    return matches[0] if matches else None


def load_compilation_database_entries(compile_commands_path: Path) -> set[Path]:
    with compile_commands_path.open("r", encoding="utf-8") as file_handle:
        database = json.load(file_handle)

    return {
        Path(entry["file"]).resolve()
        for entry in database
        if isinstance(entry, dict) and "file" in entry
    }


def run_command(command: list[str]) -> int:
    completed = subprocess.run(command)
    return completed.returncode


def main() -> int:
    clang_tidy = shutil.which("clang-tidy")
    if clang_tidy is None:
        print("Error: clang-tidy was not found in PATH", file=sys.stderr)
        return 1

    repo_root = Path.cwd()
    compile_commands_path = find_compile_commands(repo_root)
    if compile_commands_path is None:
        print(
            "Warning: compile_commands.json was not found. "
            "Skipping clang-tidy because full compilation context is unavailable. "
            "Run CMake configure first if you want reliable clang-tidy results.",
            file=sys.stderr,
        )
        return 0

    database_entries = load_compilation_database_entries(compile_commands_path)

    failed = False
    skipped_files: list[Path] = []

    for relative_path in candidate_files(sys.argv[1:]):
        file_path = (repo_root / relative_path).resolve()
        if not file_path.is_file() or file_path.suffix.lower() not in CPP_SUFFIXES:
            continue

        if file_path not in database_entries and file_path.suffix.lower() in HEADER_SUFFIXES:
            continue

        if file_path not in database_entries:
            skipped_files.append(relative_path)
            continue

        command = [
            clang_tidy,
            "--quiet",
            f"--warnings-as-errors={WARNINGS_AS_ERRORS}",
            "-p",
            str(compile_commands_path.parent),
            str(file_path),
        ]

        if run_command(command) != 0:
            failed = True

    for skipped_file in skipped_files:
        print(
            f"Warning: skipped {skipped_file} because it is not present in compile_commands.json",
            file=sys.stderr,
        )

    return 1 if failed else 0


if __name__ == "__main__":
    raise SystemExit(main())
