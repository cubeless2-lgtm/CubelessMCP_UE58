import json
import os
from pathlib import Path
import sys

os.environ["KIMODO_TEXT_ENCODER_DEVICE"] = "cpu"
import torch
torch.set_num_threads(min(6, torch.get_num_threads()))
# This workstation also runs the editor. Keep the CPU encoder in BF16 when available.
try:
    probe=torch.ones((16,16),dtype=torch.bfloat16)
    torch.mm(probe,probe)
except RuntimeError:
    pass
else:
    os.environ.setdefault("KIMODO_CPU_BF16","1")
import kimodo_generate

job = Path(sys.argv[1])
recipe = json.loads((job/"recipe.json").read_text())
result = kimodo_generate.infer_only(dict(description=recipe["prompt"], num_frames=round(30*recipe["duration"]),
    random_seed=recipe["seed"], frame_rate=30, use_trajectory=False, post_processing=False,
    text_encoder_precision="full", diffusion_steps=30, bvh_output=str(job/"motion.bvh")))
if result["result_code"] != kimodo_generate.KIMODO_GENERATE_SUCCESS:
    raise RuntimeError(result.get("error_message",result))


