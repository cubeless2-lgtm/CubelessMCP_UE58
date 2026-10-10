import json, runpy
from pathlib import Path
root = Path('C:\\Git\\UnrealMCP5_8_3\\UnrealMCP5_8_3')
records = root / 'cubeless/records/verification/implementation-2026-10-10'
author = json.loads((records / 'blueprint-component-host-Y.json').read_text(encoding='utf-8'))
assert author['status'] == 'pass' and author['outcome_unknown'] is False
assert author['package'] == '/Game/CubelessMCPTests/ComponentHost_a3b6919bb2e3427f9cd0f4c5a1e36b81/BP_ComponentHost'
module = runpy.run_path(str(root / 'cubeless/tools/verification/verify_component_host_runtime.py'), run_name='cubeless_component_runtime_Y')
result = module['verify'](str(root / 'cubeless/workspaces/UE58RuntimeProof/CubelessRuntimeProof.uproject'), author['package'], author['asset_file']['sha256'], author['component_package'], author['component_file']['sha256'], str(records / 'component-host-runtime-Y.json'))
if not result['success']:
    raise RuntimeError('Component host runtime verification failed; inspect report')
