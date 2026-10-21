"""Private fixed whole-JSON dispatcher for one original local-variable12 tool.

Only decoded public values reach the named domain. No caller-selected code,
module, function, expression or universal dispatch table is evaluated.
"""
import base64
import binascii
import json
import re

from cubeless_mcp_ue58.operations import local_variable, state
from cubeless_mcp_ue58.operations.state import OperationError
from .results import reflected_result

MARKER = "__CUBELESS_LOCAL_VARIABLE_RESULT__"
MAX_ARGUMENT_BYTES = 131072
PUBLIC_FIELDS = ("blueprint_name", "variable_name", "variable_type", "default_value", "category",
    "tooltip", "type_object", "is_array", "graph_name", "graph_id", "graph_type", "create_graph_if_missing")
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
DEFAULTS = {"default_value": None, "category": "", "tooltip": "", "type_object": "", "is_array": False,
    "graph_name": "", "graph_id": "", "graph_type": "function", "create_graph_if_missing": False}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("raw_local_variable_arguments_invalid", "Duplicate JSON object keys are not admitted")
        result[key] = value
    return result


def _nonfinite(value):
    raise OperationError("raw_local_variable_arguments_invalid", "Nonfinite JSON literals are not admitted")


def _arguments(encoded):
    if type(encoded) is not str or len(encoded) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
        raise OperationError("raw_local_variable_arguments_invalid", "Bounded base64 JSON input required")
    try:
        raw = base64.b64decode(encoded, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_local_variable_arguments_invalid", "Raw arguments exceed the fixed input budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
    except (binascii.Error, UnicodeError, ValueError, RecursionError) as error:
        raise OperationError("raw_local_variable_arguments_invalid", "Strict base64/UTF8/JSON arguments required") from error
    if type(supplied) is not dict or set(supplied) - set(PUBLIC_FIELDS):
        raise OperationError("raw_local_variable_arguments_invalid", "Only the exact twelve public arguments are accepted")
    for key in PUBLIC_FIELDS[:3]:
        if key not in supplied or type(supplied[key]) is not str:
            raise OperationError("raw_local_variable_arguments_invalid", "Required blueprint_name, variable_name and variable_type are strings")
    arguments = {**DEFAULTS, **supplied}
    if any(type(arguments[key]) is not str for key in ("category", "tooltip", "type_object", "graph_name", "graph_id", "graph_type")):
        raise OperationError("raw_local_variable_arguments_invalid", "Metadata/type/graph selectors require literal strings")
    if any(type(arguments[key]) is not bool for key in ("is_array", "create_graph_if_missing")):
        raise OperationError("raw_local_variable_arguments_invalid", "Array/create options require exact booleans")
    # default_value remains any JSON kind including []/{} and null. Original
    # dependency guards and finite native default admission stay in the domain.
    return arguments, supplied


def _envelope(record):
    reflected = reflected_result(record)
    fields = {key: getattr(reflected, key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_local_variable_result_invalid", "Exact existing reflected result field types required")
    return {"returnValue": fields}


def emit_result(encoded_args_b64, nonce):
    """The retained native owner calls this exact private method once."""
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_local_variable_nonce_invalid", "Native owner nonce must be lowercase UUID hex")
    try:
        arguments, supplied = _arguments(encoded_args_b64)
    except Exception as error:
        # Decode/shape only, before domain/native dispatch. Later unexpected
        # domain/export/print failures escape with C++ log/prefix uncertainty.
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=local_variable.IDENTITY, admission_rejected=True,
            native_dispatched=False, native_mutation_attempted=False,
            mutation_state_unknown=False, mutation_performed=False,
            raw_named_toolset_argument_validation_failed=True)
    else:
        record = local_variable.add_blueprint_local_variable(**arguments)
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))
