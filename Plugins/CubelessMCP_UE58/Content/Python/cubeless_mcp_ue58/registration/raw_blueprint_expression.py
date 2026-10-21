"""Private fixed two-method helper for the official expression RAW toolset.

Only trusted C++ literals select a method. Inputs remain data, not user code.
Unexpected post-dispatch/export errors escape so the owner retains uncertainty.
"""
import base64
import binascii
import json
import re

from cubeless_mcp_ue58.operations import blueprint_expression, state
from cubeless_mcp_ue58.operations.state import OperationError
from . import blueprint_expression_schema as schema
from .results import reflected_result

MARKER = "__CUBELESS_BLUEPRINT_EXPRESSION_RESULT__"
PUBLIC_FIELDS_BY_METHOD = schema.PUBLIC_FIELDS_BY_METHOD
DEFAULTS_BY_METHOD = schema.DEFAULTS_BY_METHOD
RESULT_FIELDS = schema.RESULT_FIELDS
MAX_ARGUMENT_BYTES = schema.MAX_ARGUMENT_BYTES


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("raw_blueprint_expression_arguments_invalid", "Duplicate JSON keys are outside the bounded raw adapter")
        result[key] = value
    return result


def _nonfinite(value):
    raise OperationError("raw_blueprint_expression_arguments_invalid", "Nonfinite JSON literals are not admitted")


def _arguments(method, encoded):
    if method not in schema.BINDINGS or type(encoded) is not str or len(encoded) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
        raise OperationError("raw_blueprint_expression_arguments_invalid", "Exact method and bounded base64 input required")
    try:
        raw = base64.b64decode(encoded, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_blueprint_expression_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
    except (binascii.Error, UnicodeError, ValueError, RecursionError) as error:
        raise OperationError("raw_blueprint_expression_arguments_invalid", "Strict base64/UTF8/JSON request required") from error
    if type(supplied) is not dict or set(supplied) - set(PUBLIC_FIELDS_BY_METHOD[method]):
        raise OperationError("raw_blueprint_expression_arguments_invalid", "Only this original method's public arguments are admitted")
    for field in schema.REQUIRED_BY_METHOD[method]:
        if field not in supplied or type(supplied[field]) is not str:
            raise OperationError("raw_blueprint_expression_arguments_invalid", "Original required names/type require strings")
    arguments = {**DEFAULTS_BY_METHOD[method], **supplied}
    for field in PUBLIC_FIELDS_BY_METHOD[method]:
        if field == "node_position":
            continue  # Any JSON shape remains data; Native owns its finite position interpretation.
        expected = bool if field == "create_graph_if_missing" else str
        if type(arguments[field]) is not expected:
            raise OperationError("raw_blueprint_expression_arguments_invalid", "Original field type differs: " + field)
    return arguments, supplied


def _envelope(record):
    reflected = reflected_result(record)
    fields = {key: getattr(reflected, key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_blueprint_expression_result_invalid", "Exact existing operation output11 field types required")
    return {"returnValue": fields}


def emit_result(method, encoded_args_b64, nonce):
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_blueprint_expression_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    if method in ('add_blueprint_input_axis_event_node', 'add_blueprint_enhanced_input_action_node'):
        from .raw_blueprint_input_events import emit_result as emit_input_events_result
        return emit_input_events_result(method, encoded_args_b64, nonce)
    if method in ("add_blueprint_compare_node", "add_blueprint_literal_node"):
        from .raw_blueprint_value import emit_result as emit_value_result
        return emit_value_result(method, encoded_args_b64, nonce)
    if method in ("add_blueprint_get_subsystem_from_pc_node", "add_blueprint_construct_object_node"):
        from .raw_blueprint_construction import emit_result as emit_construction_result
        return emit_construction_result(method, encoded_args_b64, nonce)
    if method in ("add_blueprint_input_action_node", "add_blueprint_self_reference"):
        from .raw_blueprint_input_self import emit_result as emit_input_self_result
        return emit_input_self_result(method, encoded_args_b64, nonce)
    if method == "add_blueprint_get_editor_subsystem_node":
        from .raw_blueprint_editor_subsystem import emit_result as emit_editor_subsystem_result
        return emit_editor_subsystem_result(method, encoded_args_b64, nonce)
    try:
        arguments, supplied = _arguments(method, encoded_args_b64)
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method if method in schema.BINDINGS else "invalid_expression_method",
            admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
            raw_named_toolset_argument_validation_failed=True)
    else:
        if method == schema.BINDINGS[0]:
            record = blueprint_expression.add_blueprint_math_node(**arguments)
        else:
            record = blueprint_expression.add_blueprint_boolean_node(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[key for key in DEFAULTS_BY_METHOD[method] if key not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))
