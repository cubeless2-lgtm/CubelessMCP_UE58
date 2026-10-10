"""One-shot component fixture launcher, root owns and verifies the returned PID."""
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools/verification"))
from create_component_read_fixture import author
report = author(r"C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3\cubeless\records\verification\implementation-2026-10-10\component-fixture-N1.json", "c0463f159332448fa862e4c19cf5b7f4", os.getpid())
if not report["success"]:
    raise RuntimeError("Fixture failed; preserve evidence and inspect before a new attempt")
