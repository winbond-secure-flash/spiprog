#!/usr/bin/env python3
"""Pack the repository into a zip file, excluding .git and tests/ directories."""

import os
import zipfile
from pathlib import Path
from datetime import datetime

EXCLUDE_DIRS = {".git", "tests", ".vs"}
EXCLUDE_FILES = {".gitignore", ".gitmodules", "export_repo.py"}
EXCLUDE_EXTENSIONS = {".zip"}

def should_exclude(path: Path) -> bool:
    parts = path.parts
    return any(part in EXCLUDE_DIRS for part in parts)

def main():
    repo_root = Path(__file__).parent.resolve()
    timestamp = datetime.now().strftime("%Y%m%d_%H%M%S")
    zip_name = f"spiprog_{timestamp}.zip"
    zip_path = repo_root / zip_name

    with zipfile.ZipFile(zip_path, "w", zipfile.ZIP_DEFLATED) as zf:
        for file in sorted(repo_root.rglob("*")):
            if not file.is_file():
                continue
            rel = file.relative_to(repo_root)
            if should_exclude(rel):
                continue
            if rel.name in EXCLUDE_FILES:
                continue
            if rel.suffix in EXCLUDE_EXTENSIONS:
                continue
            zf.write(file, arcname=str(rel))
            print(f"  + {rel}")

    print(f"\nExported to: {zip_path}")
    print(f"Size: {zip_path.stat().st_size / 1024:.1f} KB")

    input("\nPress any key to exit...")

if __name__ == "__main__":
    main()