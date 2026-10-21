"""UE58 exported Niagara stock-stack binding; the Python adapter does not load or use RPC/reflection fallback. Native Add alone may acquire primary target then module once each (loaded-fast0); Inspect/internal observers remain find-only. Actual load prefixes and scoped unknown failures come from the Native receipt; global loader effects and wider original behavior remain unproved."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessNiagaraModuleLibrary"
BINDINGS = ("add_niagara_stock_module_to_stack",)


def add_niagara_stock_module_to_stack(target_system_path, module_script_path,
        target_usage, target_usage_id, target_emitter_index_json, target_emitter_name,
        target_index_json, suggested_name, skip_if_duplicate, allow_source_edit,
        save, request_compile):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_niagara_module_api_unavailable",
            "Build/load the exact CubelessNiagaraModuleLibrary binding; no fallback or asset load executed")
    return _decode_result(function(target_system_path, module_script_path,
        target_usage, target_usage_id, target_emitter_index_json, target_emitter_name,
        target_index_json, suggested_name, skip_if_duplicate, allow_source_edit,
        save, request_compile))
