import runpy
module = runpy.run_path('C:\\Git\\UnrealMCP5_8_3\\UnrealMCP5_8_3\\cubeless\\tools\\verification\\verify_function_timer_runtime.py', run_name="cubeless_timer_verifier")
result = module["verify"]('C:\\Git\\UnrealMCP5_8_3\\UnrealMCP5_8_3\\cubeless\\workspaces\\UE58RuntimeProof\\CubelessRuntimeProof.uproject', '/Game/CubelessMCPTests/FunctionTimer_ac30351e04c74596887ffe9b9146f4dd/BP_FunctionTimer', 'bf97de438589de1155166eba36d99c05a98ea7398d918bd5fb018a71ddb5c3b1', 'C:\\Git\\UnrealMCP5_8_3\\UnrealMCP5_8_3\\cubeless\\records\\verification\\implementation-2026-10-10\\timer-runtime-V.json')
if not result["success"]:
    raise RuntimeError("Timer runtime proof failed; inspect timer-runtime-V.json")
