"""Corrected fixture on a new path after inspecting the unsaved N2 failure."""
import os
from pathlib import Path
import sys
sys.path.insert(0, str(Path(__file__).resolve().parents[3] / "tools/verification"))
from create_component_read_fixture import author
report = author(str(Path(__file__).with_name("component-fixture-N3.json")), "da3d1cd6681a4b72bf5a0e9603398fde", os.getpid())
if not report["success"]:
    raise RuntimeError("Fixture failed; preserve evidence and inspect before a new attempt")
