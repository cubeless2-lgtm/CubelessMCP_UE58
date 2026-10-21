"""Find-only UE58 emitter attachment binding; no loading or transport fallback."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessNiagaraEmitterLibrary"
BINDINGS = ("duplicate_or_attach_emitter_from_source",)


def duplicate_or_attach_emitter_from_source(target_system_path, source_emitter_path,
        source_system_path, source_emitter_index_json, source_emitter_name,
        new_emitter_name, enabled_json, copy_source_user_parameters,
        allow_source_edit, save, request_compile):
    function = getattr(getattr(unreal, LIBRARY, None), BINDINGS[0], None)
    if not callable(function):
        raise OperationError("native_niagara_emitter_api_unavailable",
            "Build/load the exact CubelessNiagaraEmitterLibrary binding; no fallback or asset load executed")
    return _decode_result(function(target_system_path, source_emitter_path,
        source_system_path, source_emitter_index_json, source_emitter_name,
        new_emitter_name, enabled_json, copy_source_user_parameters,
        allow_source_edit, save, request_compile))
