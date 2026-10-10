"""Read-only project inventory; write a new planning evidence file only.

Does not import project Python, load Unreal assets, or alter the source project.
"""

from __future__ import annotations

import argparse
import hashlib
import json
import os
from collections import Counter
from datetime import datetime, timedelta, timezone
from pathlib import Path


def capture(root: Path) -> dict:
    excluded = {".git", "Intermediate", "Saved", "DerivedDataCache", "__pycache__", "cubeless"}
    started = datetime.now(timezone(timedelta(hours=9))).isoformat()
    records = []
    exclusions = []
    for parent, directories, files in os.walk(root):
        for name in list(directories):
            candidate = Path(parent) / name
            if name in excluded or candidate == root / "docs" / "index.bleve":
                exclusions.append(candidate.relative_to(root).as_posix())
                directories.remove(name)
        for name in files:
            path = Path(parent) / name
            before = path.stat()
            digest = hashlib.sha256()
            with path.open("rb") as stream:
                for chunk in iter(lambda: stream.read(1024 * 1024), b""):
                    digest.update(chunk)
            after = path.stat()
            records.append({
                "path": path.relative_to(root).as_posix(),
                "bytes": after.st_size,
                "mtime_ns": after.st_mtime_ns,
                "sha256": digest.hexdigest(),
                "stable_during_read": before.st_size == after.st_size and before.st_mtime_ns == after.st_mtime_ns,
            })
    records.sort(key=lambda row: row["path"])
    return {
        "schema_version": 1,
        "purpose": "Planning preservation baseline; hashes are evidence, not backups or Unreal validation.",
        "root": str(root),
        "started_at_kst": started,
        "finished_at_kst": datetime.now(timezone(timedelta(hours=9))).isoformat(),
        "git_repository": False,
        "excluded_directories": sorted(exclusions),
        "file_count": len(records),
        "bytes": sum(row["bytes"] for row in records),
        "extension_counts": dict(sorted(Counter(Path(row["path"]).suffix for row in records).items())),
        "unstable_reads": [row["path"] for row in records if not row["stable_during_read"]],
        "files": records,
    }


if __name__ == "__main__":
    parser = argparse.ArgumentParser()
    parser.add_argument("--root", type=Path, required=True)
    parser.add_argument("--output", type=Path, required=True)
    arguments = parser.parse_args()
    result = capture(arguments.root.resolve())
    with arguments.output.open("x", encoding="utf-8") as stream:
        json.dump(result, stream, ensure_ascii=False, indent=2)
        stream.write("\n")
    print(json.dumps({key: result[key] for key in ("file_count", "bytes", "extension_counts", "unstable_reads")}, ensure_ascii=False))
