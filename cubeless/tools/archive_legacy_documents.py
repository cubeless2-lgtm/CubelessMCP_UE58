"""Byte-preserve inventoried UE5.6 documents without activating old instructions.

Reads source working tree/Git only. Appends .source to every archived filename.
Git index/HEAD variants are recorded separately; no source restore is performed.
"""
from __future__ import annotations

import argparse
import hashlib
import json
import subprocess
from datetime import datetime, timezone
from pathlib import Path, PurePosixPath


def sha(data):
    return hashlib.sha256(data).hexdigest()


def main():
    parser = argparse.ArgumentParser(description=__doc__)
    parser.add_argument("--source", type=Path, required=True)
    parser.add_argument("--inventory", type=Path, required=True)
    parser.add_argument("--archive", type=Path, required=True)
    parser.add_argument("--evidence", type=Path, required=True)
    args = parser.parse_args()
    args.source = args.source.resolve()
    args.archive = args.archive.resolve()
    if args.source == args.archive or args.source in args.archive.parents:
        parser.error("Archive must be outside the read-only source repository")
    if args.evidence.exists() or (args.archive / "manifest.json").exists():
        parser.error("Refusing to overwrite an existing archive manifest/evidence")
    inventory = json.loads(args.inventory.read_text(encoding="utf-8"))
    git = lambda *parts: subprocess.check_output(["git", "-C", str(args.source), *parts])
    head = git("rev-parse", "HEAD").decode().strip()
    index = {}
    for entry in git("ls-files", "--stage", "-z").split(b"\0"):
        if entry:
            metadata, path = entry.split(b"\t", 1)
            mode, blob, stage = metadata.decode().split()
            if stage == "0":
                index[path.decode("utf-8", errors="surrogateescape")] = blob
    tree = {}
    for entry in git("ls-tree", "-r", "-z", head).split(b"\0"):
        if entry:
            metadata, path = entry.split(b"\t", 1)
            mode, kind, blob = metadata.decode().split()
            if kind == "blob":
                tree[path.decode("utf-8", errors="surrogateescape")] = blob
    selected = [x for x in inventory["files"] if Path(x["source_relative_path"]).suffix.lower() in {".md", ".toml", ".yaml", ".yml"}]
    report = {"schema": "cubeless_ue56_document_archive_v1", "created_utc": datetime.now(timezone.utc).isoformat(), "source_root": str(args.source), "source_head": head, "inventory_sha256": sha(args.inventory.read_bytes()), "scope": "Inventoried .md/.toml/.yaml/.yml documents only; scripts, binary evidence and ignored/uninventoried files are not claimed archived", "source_modified": False, "archive_is_migration_completion": False, "files": [], "errors": []}
    cache = {}
    def read_blob(blob):
        if blob not in cache:
            cache[blob] = git("cat-file", "blob", blob)
        return cache[blob]
    def save_variant(folder, rel, data, **extra):
        dest = args.archive / folder / (rel + ".source")
        dest.parent.mkdir(parents=True, exist_ok=True)
        with dest.open("xb") as handle:
            handle.write(data)
        assert sha(dest.read_bytes()) == sha(data)
        return {"archive_path": dest.relative_to(args.archive).as_posix(), "bytes": len(data), "sha256": sha(data), "all_nul": bool(data) and data.count(b"\0") == len(data), **extra}
    for record in selected:
        rel = record["source_relative_path"].replace("\\", "/")
        if PurePosixPath(rel).is_absolute() or ".." in PurePosixPath(rel).parts:
            raise ValueError("Unsafe source-relative path")
        row = {"source_relative_path": rel, "variants": {}}
        try:
            src = args.source / rel
            current = src.read_bytes()
            row["variants"]["working_tree"] = save_variant("working-tree", rel, current)
            row["matches_planning_hash"] = sha(current) == record["sha256"]
            row["source_recheck_unchanged"] = sha(src.read_bytes()) == sha(current)
            for label, mapping in (("index", index), ("head", tree)):
                if rel not in mapping:
                    row["variants"][label] = {"status": "not_present"}
                    continue
                blob = mapping[rel]
                data = read_blob(blob)
                if data == current:
                    row["variants"][label] = {"same_bytes_as": "working_tree", "git_blob": blob, "sha256": sha(data), "commit": head if label == "head" else None}
                else:
                    folder = f"git/{head}" if label == "head" else "index"
                    row["variants"][label] = save_variant(folder, rel, data, git_blob=blob, commit=head if label == "head" else None)
            if row["variants"]["working_tree"]["all_nul"]:
                row["recovery_note"] = "Preserved corrupt bytes; readable Git variants are historical recovery, not proof of latest unstaged intent"
            report["files"].append(row)
        except Exception as exc:
            report["errors"].append({"source_relative_path": rel, "error": str(exc)})
    report["counts"] = {"selected_documents": len(selected), "archived_documents": len(report["files"]), "variant_files": sum("archive_path" in v for r in report["files"] for v in r["variants"].values()), "all_nul_working_tree": sum(r["variants"]["working_tree"]["all_nul"] for r in report["files"]), "changed_since_planning": sum(not r["matches_planning_hash"] for r in report["files"]), "errors": len(report["errors"])}
    text = json.dumps(report, ensure_ascii=False, indent=2)
    args.archive.mkdir(parents=True, exist_ok=True)
    (args.archive / "manifest.json").write_text(text, encoding="utf-8")
    args.evidence.parent.mkdir(parents=True, exist_ok=True)
    args.evidence.write_text(text, encoding="utf-8")
    print(json.dumps(report["counts"]))
    return bool(report["errors"])


if __name__ == "__main__":
    raise SystemExit(main())
