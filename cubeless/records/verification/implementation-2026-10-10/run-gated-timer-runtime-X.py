import json, runpy
from pathlib import Path
root = Path('C:\\Git\\UnrealMCP5_8_3\\UnrealMCP5_8_3')
records = root / 'cubeless/records/verification/implementation-2026-10-10'
author = json.loads((records / 'blueprint-guarded-links-X.json').read_text(encoding='utf-8'))
assert author['status'] == 'pass' and author['outcome_unknown'] is False
assert author['package'] == '/Game/CubelessMCPTests/GatedTimer_f0ae3ba58ebf484d91f252c55100fe3e/BP_GatedTimer'
module = runpy.run_path(str(root / 'cubeless/tools/verification/verify_gated_timer_runtime.py'), run_name='cubeless_gated_runtime_X')
result = module['verify'](str(root / 'cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject'), author['package'], author['asset_file']['sha256'], str(records / 'gated-timer-runtime-X.json'))
if not result['success']:
    raise RuntimeError('Guarded timer runtime verification failed; inspect report')
