"""Editor startup for CubelessMCP_UE58; never modifies project startup scripts."""

import unreal

try:
    from cubeless_mcp_ue58.registration import register
    register()
except Exception as error:
    unreal.log_error(f"CubelessMCP_UE58 Python registration failed: {error}")
    raise
