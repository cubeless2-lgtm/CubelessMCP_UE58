import json
import runpy
from pathlib import Path

root = Path(r"C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3")
records = root / "cubeless/records/verification/implementation-2026-10-10"
author = json.loads((records / "blueprint-gated-timer-W3.json").read_text(encoding="utf-8"))
assert author["status"] == "pass" and author["outcome_unknown"] is False
assert author["package"] == "/Game/CubelessMCPTests/GatedTimer_fb4299829e3b4e27a8e11fda54d382ac/BP_GatedTimer"
module = runpy.run_path(str(root / "cubeless/tools/verification/verify_gated_timer_runtime.py"), run_name="cubeless_gated_runtime_W3")
result = module["verify"](
    str(root / "cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject"),
    author["package"], author["asset_file"]["sha256"], str(records / "gated-timer-runtime-W3.json"))
if not result["success"]:
    raise RuntimeError("Gated timer runtime verification failed; inspect the report")
