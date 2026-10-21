"""Private fixed Compare/Literal helper for the same official expression RAW toolset.

Only trusted C++ literals select a method. Inputs remain data, not user code.
Unexpected post-dispatch/export errors escape so the owner retains uncertainty.
"""
import base64
import binascii
import json
import re

from cubeless_mcp_ue58.operations import blueprint_value, state
from cubeless_mcp_ue58.operations.state import OperationError
from . import blueprint_value_schema as schema
from .results import reflected_result

MARKER = "__CUBELESS_BLUEPRINT_EXPRESSION_RESULT__"
PUBLIC_FIELDS_BY_METHOD = schema.PUBLIC_FIELDS_BY_METHOD
DEFAULTS_BY_METHOD = schema.DEFAULTS_BY_METHOD
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
MAX_ARGUMENT_BYTES = 131072


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("raw_blueprint_value_arguments_invalid", "Duplicate JSON keys are outside the bounded raw adapter")
        result[key] = value
    return result


def _nonfinite(value):
    raise OperationError("raw_blueprint_value_arguments_invalid", "Nonfinite JSON literals are not admitted")


def _arguments(method, encoded):
    if method not in schema.BINDINGS or type(encoded) is not str or len(encoded) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
        raise OperationError("raw_blueprint_value_arguments_invalid", "Exact method and bounded base64 input required")
    try:
        raw = base64.b64decode(encoded, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_blueprint_value_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
    except (binascii.Error, UnicodeError, ValueError, RecursionError) as error:
        raise OperationError("raw_blueprint_value_arguments_invalid", "Strict base64/UTF8/JSON request required") from error
    if type(supplied) is not dict or set(supplied) - set(PUBLIC_FIELDS_BY_METHOD[method]):
        raise OperationError("raw_blueprint_value_arguments_invalid", "Only this original method's public arguments are admitted")
    for field in schema.REQUIRED_BY_METHOD[method]:
        if field not in supplied or type(supplied[field]) is not str:
            raise OperationError("raw_blueprint_value_arguments_invalid", "Original required names/type require strings")
    arguments = {**DEFAULTS_BY_METHOD[method], **supplied}
    for field in PUBLIC_FIELDS_BY_METHOD[method]:
        if field in ("node_position", "value"):
            continue  # Whole Any JSON remains data; native owns position and late literal conversion.
        expected = bool if field == "create_graph_if_missing" else str
        if type(arguments[field]) is not expected:
            raise OperationError("raw_blueprint_value_arguments_invalid", "Original field type differs: " + field)
    return arguments, supplied


def _envelope(record):
    reflected = reflected_result(record)
    fields = {key: getattr(reflected, key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_blueprint_value_result_invalid", "Exact existing operation output11 field types required")
    return {"returnValue": fields}


def emit_result(method, encoded_args_b64, nonce):
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_blueprint_value_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        arguments, supplied = _arguments(method, encoded_args_b64)
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method if method in schema.BINDINGS else "invalid_value_method",
            admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
            raw_named_toolset_argument_validation_failed=True)
    else:
        if method == schema.BINDINGS[0]:
            record = blueprint_value.add_blueprint_compare_node(**arguments)
        else:
            record = blueprint_value.add_blueprint_literal_node(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[key for key in DEFAULTS_BY_METHOD[method] if key not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))
