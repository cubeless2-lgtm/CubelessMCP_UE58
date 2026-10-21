"""UE58 finite public Niagara parameter-store bindings; no load or transport fallback."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = "CubelessNiagaraParameterLibrary"
BINDINGS = (
    "inspect_niagara_user_parameters",
    "set_niagara_user_parameter",
    "upsert_niagara_user_parameters",
)


def _invoke(method, *arguments):
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("native_niagara_parameter_api_unavailable",
            "Build/load the exact CubelessNiagaraParameterLibrary binding; no fallback or asset load executed")
    return _decode_result(function(*arguments))


def inspect_niagara_user_parameters(system_path):
    return _invoke("inspect_niagara_user_parameters", system_path)


def set_niagara_user_parameter(system_path, parameter_name, value_json, allow_source_edit, save):
    return _invoke("set_niagara_user_parameter", system_path, parameter_name, value_json,
        allow_source_edit, save)


def upsert_niagara_user_parameters(system_path, parameters_json, continue_on_error,
        allow_source_edit, save, request_compile):
    return _invoke("upsert_niagara_user_parameters", system_path, parameters_json,
        continue_on_error, allow_source_edit, save, request_compile)
