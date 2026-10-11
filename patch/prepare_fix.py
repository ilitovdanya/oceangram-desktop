#!/usr/bin/env python3
"""Build-system fixes for the pinned tdesktop commit (library preparation only).

Usage: prepare_fix.py <path to tdesktop checkout>

Backport of upstream b244f1b346 "Update msys2 and use ucrt versions of packages":
MSYS2 dropped the mingw-w64-x86_64-* packages the pinned prepare.py installs.
"""
import pathlib
import sys

path = pathlib.Path(sys.argv[1]).resolve() / "Telegram/build/prepare/prepare.py"
with open(path, encoding="utf-8", newline="") as f:
    text = f.read()

edits = [
    ("'ThirdParty\\\\msys64\\\\mingw64\\\\bin',", "'ThirdParty\\\\msys64\\\\ucrt64\\\\bin',"),
    ("releases/download/2025-08-30/msys2-base-x86_64-20250830.sfx.exe",
     "releases/download/2026-09-27/msys2-base-x86_64-20260927.sfx.exe"),
] + [
    (f"mingw-w64-x86_64-{pkg}", f"mingw-w64-ucrt-x86_64-{pkg}")
    for pkg in ("diffutils", "gperf", "nasm", "perl", "pkgconf")
]
for old, new in edits:
    if text.count(old) != 1:
        sys.exit(f"prepare_fix: expected one {old!r}, found {text.count(old)}")
    text = text.replace(old, new)

with open(path, "w", encoding="utf-8", newline="") as f:
    f.write(text)
print("prepare_fix: OK")
