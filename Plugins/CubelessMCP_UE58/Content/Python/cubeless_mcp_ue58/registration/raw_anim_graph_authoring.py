"""Private fixed AnimGraph authoring helper for the dedicated official AnimGraphAuthoring RAW toolset.

Only trusted C++ literals select a method. Inputs remain data, not user code.
Unexpected post-dispatch/export errors escape so the owner retains uncertainty.
"""
import base64
import binascii
import json
import re

from cubeless_mcp_ue58.operations import anim_graph_authoring, state
from cubeless_mcp_ue58.operations.state import OperationError
from . import anim_graph_authoring_schema as schema
from .results import reflected_result

MARKER = "__CUBELESS_ANIM_GRAPH_AUTHORING_RESULT__"
PUBLIC_FIELDS_BY_METHOD = schema.PUBLIC_FIELDS_BY_METHOD
DEFAULTS_BY_METHOD = schema.DEFAULTS_BY_METHOD
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
MAX_ARGUMENT_BYTES = 131072


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Duplicate JSON keys are outside the bounded raw adapter")
        result[key] = value
    return result


def _nonfinite(value):
    raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Nonfinite JSON literals are not admitted")


def _arguments(method, encoded):
    if method not in schema.BINDINGS or type(encoded) is not str or len(encoded) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
        raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Exact method and bounded base64 input required")
    try:
        raw = base64.b64decode(encoded, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
    except (binascii.Error, UnicodeError, ValueError, RecursionError) as error:
        raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Strict base64/UTF8/JSON request required") from error
    if type(supplied) is not dict or set(supplied) - set(PUBLIC_FIELDS_BY_METHOD[method]):
        raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only this original method's public arguments are admitted")
    for field in schema.REQUIRED_BY_METHOD[method]:
        if field not in supplied:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
    arguments = {**DEFAULTS_BY_METHOD[method], **supplied}
    for field in PUBLIC_FIELDS_BY_METHOD[method]:
        value = arguments[field]
        kind = schema.FIELD_KINDS_BY_METHOD[method][field]
        expected = bool if kind == "boolean" else int if kind == "integer" else str
        if kind != "any" and type(value) is not expected:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
        if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Safe int64 Native carrier required: " + field)
    return arguments, supplied


def _envelope(record):
    reflected = reflected_result(record)
    fields = {key: getattr(reflected, key) for key in RESULT_FIELDS}
    if type(fields["success"]) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):
        raise OperationError("raw_anim_graph_authoring_result_invalid", "Exact existing operation output11 field types required")
    return {"returnValue": fields}


def _emit_bone_demo_result(encoded_args_b64, nonce):
    # One literal third route; the original two-method helper below is retained.
    from . import anim_graph_bone_demo_schema as bone_schema
    from cubeless_mcp_ue58.operations import anim_graph_bone_demo
    method = "ensure_anim_graph_modify_bone_demo"
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = bone_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original ModifyBone8 fields admitted")
        if any(f not in supplied for f in bone_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
        arguments = {**bone_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in bone_schema.FIELD_KINDS_BY_METHOD[method].items():
            if kind != "any" and type(arguments[field]) is not (bool if kind == "boolean" else str):
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        record = anim_graph_bone_demo.ensure_anim_graph_modify_bone_demo(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in bone_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_modify_curve_demo_result(encoded_args_b64, nonce):
    # One literal fourth route; the existing three-method helper is retained.
    from . import anim_graph_modify_curve_demo_schema as curve_schema
    from cubeless_mcp_ue58.operations import anim_graph_modify_curve_demo
    method = "ensure_anim_graph_modify_curve_demo"
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = curve_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original ModifyCurve9 fields admitted")
        if any(f not in supplied for f in curve_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
        arguments = {**curve_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in curve_schema.FIELD_KINDS_BY_METHOD[method].items():
            value = arguments[field]
            if kind == "nullable_number_map":
                valid = value is None or type(value) is dict and all(type(k) is str and type(v) in (int, float) for k, v in value.items())
            elif kind == "number":
                valid = type(value) in (int, float)
            else:
                valid = type(value) is (bool if kind == "boolean" else str)
            if not valid:
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
        json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        record = anim_graph_modify_curve_demo.ensure_anim_graph_modify_curve_demo(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in curve_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_trail_demo_result(encoded_args_b64, nonce):
    # One literal fifth route; the existing four-method helper is retained.
    from . import anim_graph_trail_demo_schema as trail_schema
    from cubeless_mcp_ue58.operations import anim_graph_trail_demo
    method = "ensure_anim_graph_trail_demo"
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = trail_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original Trail19 fields admitted")
        if any(f not in supplied for f in trail_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
        arguments = {**trail_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in trail_schema.FIELD_KINDS_BY_METHOD[method].items():
            value = arguments[field]
            if kind == "any":
                continue
            valid = type(value) in (int, float) if kind == "number" else type(value) is (bool if kind == "boolean" else int if kind == "integer" else str)
            if not valid:
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
            if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Safe int64 Native carrier required: " + field)
        json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        record = anim_graph_trail_demo.ensure_anim_graph_trail_demo(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in trail_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_secondary_motion_result(encoded_args_b64, nonce):
    # One literal sixth route; the existing five-method helper is retained.
    from . import anim_graph_secondary_motion_schema as motion_schema
    from cubeless_mcp_ue58.operations import anim_graph_secondary_motion
    method = "ensure_anim_graph_secondary_motion_chain"
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = motion_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original SecondaryMotion19 fields admitted")
        if any(f not in supplied for f in motion_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
        arguments = {**motion_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in motion_schema.FIELD_KINDS_BY_METHOD[method].items():
            value = arguments[field]
            if kind == "nullable_number_list":
                valid = value is None or type(value) is list and all(type(member) in (int, float) for member in value)
            elif kind == "number":
                valid = type(value) in (int, float)
            else:
                valid = type(value) is (bool if kind == "boolean" else str)
            if not valid:
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
        json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        record = anim_graph_secondary_motion.ensure_anim_graph_secondary_motion_chain(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in motion_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_controlrig_result(method, encoded_args_b64, nonce):
    # Two fixed seventh/eighth routes; the existing six-method helper is retained.
    from . import anim_graph_controlrig_schema as rig_schema
    from cubeless_mcp_ue58.operations import anim_graph_controlrig
    if method not in ("set_anim_graph_controlrig_input_defaults", "ensure_controlrig_forced_driver_animbp"):
        raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only the exact two ControlRig methods admitted")
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = rig_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original ControlRig method fields admitted")
        if any(f not in supplied for f in rig_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required fields must be supplied")
        arguments = {**rig_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in rig_schema.FIELD_KINDS_BY_METHOD[method].items():
            value = arguments[field]
            if kind == "nullable_any_map":
                valid = value is None or type(value) is dict and all(type(key) is str for key in value)
            elif kind == "nullable_number_map":
                valid = value is None or type(value) is dict and all(type(key) is str and type(member) in (int, float) for key, member in value.items())
            elif kind == "nullable_string_list":
                valid = value is None or type(value) is list and all(type(member) is str for member in value)
            elif kind == "number":
                valid = type(value) in (int, float)
            else:
                valid = type(value) is (bool if kind == "boolean" else str)
            if not valid:
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original field type differs: " + field)
        json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        if method == "set_anim_graph_controlrig_input_defaults":
            record = anim_graph_controlrig.set_anim_graph_controlrig_input_defaults(**arguments)
        else:
            record = anim_graph_controlrig.ensure_controlrig_forced_driver_animbp(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in rig_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_controlrig_builder_result(method, encoded_args_b64, nonce):
    # Fixed ninth/tenth builders; existing eight helper branches remain intact.
    from . import controlrig_builders_schema as builder_schema
    from cubeless_mcp_ue58.operations import controlrig_builders
    if method not in ("ensure_anim_graph_controlrig_node_sample", "ensure_controlrig_sample_asset"):
        raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only exact original two ControlRig builders admitted")
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = builder_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original builder method fields admitted")
        if any(f not in supplied for f in builder_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required field must be supplied")
        arguments = {**builder_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in builder_schema.FIELD_KINDS_BY_METHOD[method].items():
            value = arguments[field]
            valid = value is None or type(value) is list if kind == "nullable_any_list" else type(value) is (bool if kind == "boolean" else int if kind == "integer" else str)
            if not valid:
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original public field type differs: " + field)
            if kind == "integer" and not -(1 << 63) <= value < (1 << 63):
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Safe private int64 carrier required: " + field)
        json.dumps(arguments, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        if method == "ensure_anim_graph_controlrig_node_sample":
            record = controlrig_builders.ensure_anim_graph_controlrig_node_sample(**arguments)
        else:
            record = controlrig_builders.ensure_controlrig_sample_asset(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in builder_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def _emit_anim_blueprint_sample_result(encoded_args_b64, nonce):
    # One fixed eleventh creator route; existing ten helpers remain unchanged.
    from . import anim_blueprint_sample_schema as sample_schema
    from cubeless_mcp_ue58.operations import anim_blueprint_sample
    method = "create_anim_blueprint_sample"
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        if type(encoded_args_b64) is not str or len(encoded_args_b64) > 4 * ((MAX_ARGUMENT_BYTES + 2) // 3):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Bounded base64 input required")
        raw = base64.b64decode(encoded_args_b64, validate=True)
        if len(raw) > MAX_ARGUMENT_BYTES:
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Request exceeds fixed 128KiB budget")
        supplied = json.loads(raw.decode("utf-8"), object_pairs_hook=_unique_object, parse_constant=_nonfinite)
        fields = sample_schema.PUBLIC_FIELDS_BY_METHOD[method]
        if type(supplied) is not dict or set(supplied) - set(fields):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Only original creator10 public fields admitted")
        if any(f not in supplied for f in sample_schema.REQUIRED_BY_METHOD[method]):
            raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original required target field must be supplied")
        arguments = {**sample_schema.DEFAULTS_BY_METHOD[method], **supplied}
        for field, kind in sample_schema.FIELD_KINDS_BY_METHOD[method].items():
            if type(arguments[field]) is not (bool if kind == "boolean" else str):
                raise OperationError("raw_anim_graph_authoring_arguments_invalid", "Original creator public field type differs: " + field)
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method, admission_rejected=True, native_dispatched=False,
            native_result_returned=False, native_mutation_attempted=False, mutation_performed=False,
            mutation_state_unknown=False, raw_named_toolset_argument_validation_failed=True)
    else:
        record = anim_blueprint_sample.create_anim_blueprint_sample(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[k for k in sample_schema.DEFAULTS_BY_METHOD[method] if k not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))


def emit_result(method, encoded_args_b64, nonce):
    if method == 'create_anim_blueprint_sample':
        _emit_anim_blueprint_sample_result(encoded_args_b64, nonce)
        return
    if method == 'ensure_anim_graph_controlrig_node_sample':
        _emit_controlrig_builder_result('ensure_anim_graph_controlrig_node_sample', encoded_args_b64, nonce)
        return
    if method == 'ensure_controlrig_sample_asset':
        _emit_controlrig_builder_result('ensure_controlrig_sample_asset', encoded_args_b64, nonce)
        return
    if method == 'set_anim_graph_controlrig_input_defaults':
        _emit_controlrig_result('set_anim_graph_controlrig_input_defaults', encoded_args_b64, nonce)
        return
    if method == 'ensure_controlrig_forced_driver_animbp':
        _emit_controlrig_result('ensure_controlrig_forced_driver_animbp', encoded_args_b64, nonce)
        return
    if method == "ensure_anim_graph_secondary_motion_chain":
        _emit_secondary_motion_result(encoded_args_b64, nonce)
        return
    if method == "ensure_anim_graph_trail_demo":
        _emit_trail_demo_result(encoded_args_b64, nonce)
        return
    if method == "ensure_anim_graph_modify_curve_demo":
        _emit_modify_curve_demo_result(encoded_args_b64, nonce)
        return
    if method == "ensure_anim_graph_modify_bone_demo":
        _emit_bone_demo_result(encoded_args_b64, nonce)
        return
    if type(nonce) is not str or re.fullmatch(r"[0-9a-f]{32}", nonce) is None:
        raise OperationError("raw_anim_graph_authoring_nonce_invalid", "Trusted owner lowercase UUID nonce required")
    try:
        arguments, supplied = _arguments(method, encoded_args_b64)
    except Exception as error:
        record = state.fail(state.new_result(), error)
        record["data"].update(operation=method if method in schema.BINDINGS else "invalid_anim_graph_authoring_method",
            admission_rejected=True, native_dispatched=False, native_result_returned=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False,
            raw_named_toolset_argument_validation_failed=True)
    else:
        record = getattr(anim_graph_authoring, method)(**arguments)
        record["data"].update(raw_supplied_public_arguments=supplied,
            raw_defaulted_argument_keys=[key for key in DEFAULTS_BY_METHOD[method] if key not in supplied])
    payload = json.dumps(_envelope(record), ensure_ascii=False, allow_nan=False, separators=(",", ":")).encode("utf-8")
    print(MARKER + nonce + ":" + base64.b64encode(payload).decode("ascii"))
