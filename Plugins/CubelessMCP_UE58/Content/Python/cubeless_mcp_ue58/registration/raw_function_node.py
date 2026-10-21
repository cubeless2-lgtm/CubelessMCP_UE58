"""Private exact helper for the named official Registry function-node tool.

The native owner calls this fixed function with base64 JSON and a nonce. No user
expression/module/function is executed; one named journal/domain path is used.
Unexpected failures after dispatch deliberately escape without a fabricated
known-zero result. The C++ owner retains command/log output and uncertain state.
"""
import base64
import binascii
import json
import re

from cubeless_mcp_ue58.operations import function_node, state
from cubeless_mcp_ue58.operations.state import OperationError
from .results import reflected_result

MARKER = "__CUBELESS_FUNCTION_NODE_RESULT__"
MAX_ARGUMENT_BYTES = 131072
PUBLIC_FIELDS = ("blueprint_name", "target", "function_name", "params", "node_position",
    "graph_name", "graph_id", "graph_type", "create_graph_if_missing")
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
DEFAULTS = {"params": None, "node_position": None, "graph_name": "", "graph_id": "",
    "graph_type": "", "create_graph_if_missing": False}


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("raw_function_node_arguments_invalid", "Duplicate JSON object keys are not admitted")
        result[key] = value
    return result


def _nonfinite(value):
    raise OperationError("raw_function_node_arguments_invalid", "Nonfinite JSON literals are not admitted")


def _arguments(encoded):
    if type(encoded) is not str or len(encoded) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
        raise OperationError("raw_function_node_arguments_invalid", "Bounded base64 JSON input required")
    try:
        raw = base64.b64decode(encoded, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_function_node_arguments_invalid", "Raw arguments exceed the fixed input budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
    except (binascii.Error, UnicodeError, ValueError, RecursionError) as error:
        raise OperationError("raw_function_node_arguments_invalid", "Strict base64/UTF8/JSON arguments required") from error
    if type(supplied) is not dict or set(supplied) - set(PUBLIC_FIELDS):
        raise OperationError("raw_function_node_arguments_invalid", "Only the exact nine public arguments are accepted")
    for key in PUBLIC_FIELDS[:3]:
        if key not in supplied or type(supplied[key]) is not str:
            raise OperationError("raw_function_node_arguments_invalid", "Required blueprint_name, target and function_name are strings")
    arguments = {**DEFAULTS, **supplied}
    if any(type(arguments[key]) is not str for key in ("graph_name", "graph_id", "graph_type")):
        raise OperationError("raw_function_node_arguments_invalid", "Graph selectors are literal strings")
    if type(arguments["create_graph_if_missing"]) is not bool:
        raise OperationError("raw_function_node_arguments_invalid", "create_graph_if_missing requires an exact boolean")
    # params remains any JSON kind. Domain checks bounded finite JSON, coordinate
    # admission and original None defaults before it starts a named journal.
    return arguments, supplied


def _envelope(record):
    reflected = reflected_result(record)
    fields = {key: getattr(reflected, key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_function_node_result_invalid", "Exact existing reflected result field types required")
    return {"returnValue": fields}


def emit_result(encoded_args_b64, nonce):
    """One exact fixed private call; prints one framed base64 JSON result line."""
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_function_node_nonce_invalid", "Native owner nonce must be lowercase UUID hex")
    try:
        arguments, supplied = _arguments(encoded_args_b64)
    except Exception as error:
        # This catch covers input decode/shape checks only: named domain/native
        # dispatch has not started, so a known no-dispatch admission is truthful.
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=function_node.IDENTITY, admission_rejected=True,
            native_dispatched=False, native_mutation_attempted=False,
            mutation_state_unknown=False, mutation_performed=False,
            raw_named_toolset_argument_validation_failed=True)
    else:
        # Do not catch an unexpected domain/struct/export/print exception as
        # known zero; the active journal/actual prefix and C++ logs are authority.
        record = function_node.add_blueprint_function_node(**arguments)
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))
