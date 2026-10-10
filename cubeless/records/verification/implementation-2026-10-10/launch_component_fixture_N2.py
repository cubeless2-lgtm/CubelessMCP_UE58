"""Explicit corrected attempt after N1 failed before all asset actions."""
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools/verification"))
from create_component_read_fixture import author
report = author(str(Path(__file__).with_name("component-fixture-N2.json")), "c0463f159332448fa862e4c19cf5b7f4", os.getpid())
if not report["success"]:
    raise RuntimeError("Fixture failed; preserve evidence and inspect before a new attempt")
