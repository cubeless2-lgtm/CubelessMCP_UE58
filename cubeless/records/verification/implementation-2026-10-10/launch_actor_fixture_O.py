"""Create one fresh isolated map for the O component authoring pipeline."""
from pathlib import Path
import sys

root = Path(__file__).resolve().parents[4]
sys.path.insert(0, str(root / "cubeless/tools/verification"))
from create_actor_smoke_map import main

if not main(["--project-file", str(root / "cubeless/workspaces/UE58Verification/CubelessMCPVerification.uproject"),
             "--report", str(Path(__file__).with_name("actor-map-fixture-O.json"))]):
    raise RuntimeError("Fixture failed; inspect the partial report before further mutations")
