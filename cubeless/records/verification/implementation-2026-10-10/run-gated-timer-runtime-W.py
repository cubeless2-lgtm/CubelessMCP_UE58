import runpy
from pathlib import Path

root = Path(r"C:\Git\UnrealMCP5_8_3\UnrealMCP5_8_3")
module = runpy.run_path(str(root / "cubeless/tools/verification/verify_gated_timer_runtime.py"), run_name="cubeless_gated_runtime_W")
result = module["verify"](
    str(root / "cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject"),
    "/Game/CubelessMCPTests/GatedTimer_fcbd1012d9a543ecac1ac83315c2206f/BP_GatedTimer",
    "c72230b29eacc59f055984a5b46d0b05a09275f3522b78d643b0988d068c2389",
    str(root / "cubeless/records/verification/implementation-2026-10-10/gated-timer-runtime-W.json"))
if not result["success"]:
    raise RuntimeError("Gated timer runtime verification failed; inspect the report")
