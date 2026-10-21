"""UE58 live AnimInstance case sampler binding; no transport or replay."""
import json
import math
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimStateRuntimeResponseLibrary"
METHOD = "sample_anim_state_machine_runtime_response"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
MAX_JSON_DEPTH = 64
MAX_JSON_SLOTS = 65536


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _pre_dispatch_error(code, message):
    error = OperationError(code, message)
    error.native_call_dispatched = False
    return error


def _carrier(value):
    # Bound traversal before encoding. Keep null, empty containers and row data;
    # only Native interprets property assignment/curve and target semantics.
    pending = [(value, 0, False)]
    active = set()
    slots = 0
    while pending:
        current, depth, exiting = pending.pop()
        if exiting:
            active.remove(id(current))
            continue
        slots += 1
        if slots > MAX_JSON_SLOTS or depth > MAX_JSON_DEPTH or type(current) in (list, dict) and depth >= MAX_JSON_DEPTH:
            raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_bound", "Private JSON exceeds 65536 slots or nesting64")
        if current is None or type(current) in (bool, int):
            continue
        if type(current) is str:
            if len(current) > MAX_CARRIER_UTF16_UNITS or "\x00" in current:
                raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "JSON text exceeds its bound or contains decoded NUL")
            continue
        if type(current) is float:
            if not math.isfinite(current):
                raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "Private JSON numbers must be finite")
            continue
        if type(current) not in (list, dict):
            raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "Private input must contain only JSON values")
        identity = id(current)
        if identity in active:
            raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "Private JSON contains a container cycle")
        if len(current) + slots + len(pending) > MAX_JSON_SLOTS:
            raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_bound", "Private JSON exceeds 65536 value slots")
        active.add(identity)
        pending.append((current, depth, True))
        if type(current) is dict:
            for key in current:
                if type(key) is not str or len(key) > MAX_CARRIER_UTF16_UNITS or "\x00" in key:
                    raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "Private JSON requires bounded string keys without NUL")
            children = list(current.values())
        else:
            children = current
        pending.extend((child, depth + 1, False) for child in reversed(children))
    try:
        units = 0
        chunks = []
        encoder = json.JSONEncoder(ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        for chunk in encoder.iterencode(value):
            units += len(chunk.encode("utf-16-le")) // 2
            if units > MAX_CARRIER_UTF16_UNITS:
                raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_bound", "Each private carrier is limited to 1Mi UE UTF16 units")
            chunks.append(chunk)
        return "".join(chunks)
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise _pre_dispatch_error("anim_state_machine_runtime_response_carrier_invalid", "Private input requires finite valid Unicode JSON") from error


LEGACY_PARAM_KEYS = frozenset(("cases", "properties", "property_values", "property_name", "value", "curve_names", "anim_instance_source"))


def _legacy_projection(cases, properties, curve_names, source, legacy_command_params=None):
    # Specific case/property DTOs preserve original presence/row order, not generic Params.
    if legacy_command_params is None:
        legacy = {}
    elif type(legacy_command_params) is not dict or set(legacy_command_params) - LEGACY_PARAM_KEYS:
        raise _pre_dispatch_error("anim_state_machine_runtime_response_legacy_input_invalid", "Private legacy params accept only the seven confirmed case/property/curve/source keys")
    else:
        legacy = legacy_command_params
    selected_cases = legacy["cases"] if "cases" in legacy else cases
    if selected_cases is not None and type(selected_cases) is not list:
        raise _pre_dispatch_error("anim_state_machine_runtime_response_legacy_input_invalid", "Private cases require null or the original array; ignored rows are Native-owned")
    property_params = {} if properties is None else {"properties": properties}
    for key in ("properties", "property_values", "property_name", "value"):
        if key in legacy:
            property_params[key] = legacy[key]
    curves = legacy["curve_names"] if "curve_names" in legacy else curve_names
    if curves is not None and type(curves) is not list:
        raise _pre_dispatch_error("anim_state_machine_runtime_response_legacy_input_invalid", "Private curve_names require null or the original descriptor array")
    selected_source = legacy["anim_instance_source"] if "anim_instance_source" in legacy else source
    if selected_source is None or type(selected_source) in (list, dict):
        _carrier(selected_source)
        selected_source = "main"
    elif type(selected_source) is not str:
        raise _pre_dispatch_error("anim_state_machine_runtime_response_legacy_source_unsupported", "Private numeric/boolean source coercion is unsupported; use an original source string")
    for value in (selected_cases, property_params, curves, selected_source):
        _carrier(value)
    return selected_cases, property_params, curves, selected_source


def sample_anim_state_machine_runtime_response(actor_label='', actor_name='', actor_path='', component_name='', cases=None, properties=None, name='case_0', tick_count=1, tick_delta_time=1.0 / 30.0, refresh_bone_transforms=True, restore_after_case=True, include_baseline=True, state_machine_name='', include_states=True, include_state_weights=True, include_relevant_anim_times=False, include_transition_progress=True, include_inactive_transition_progress=False, include_montages=True, include_curves=False, curve_names=None, max_state_machines=32, max_state_machine_instances_to_probe=256, max_states_per_machine=64, max_transitions_per_machine=128, max_curves=128, prefer_pie_world=True, require_pie_world=False, *, _anim_instance_source='main', _legacy_command_params=None):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise _pre_dispatch_error("anim_state_machine_runtime_response_native_api_unavailable", "UE58 runtime-response binding is unavailable; no fallback executed")
    selected_cases, property_params, curves, source = _legacy_projection(cases, properties, curve_names, _anim_instance_source, _legacy_command_params)
    cases_json = _carrier(selected_cases)
    properties_json = _carrier(property_params)
    curves_json = _carrier(curves)
    try:
        raw = function(actor_label, actor_name, actor_path, component_name, cases_json, properties_json, name,
            tick_count, tick_delta_time, refresh_bone_transforms, restore_after_case, include_baseline,
            state_machine_name, include_states, include_state_weights, include_relevant_anim_times,
            include_transition_progress, include_inactive_transition_progress, include_montages, include_curves,
            curves_json, max_state_machines, max_state_machine_instances_to_probe, max_states_per_machine,
            max_transitions_per_machine, max_curves, prefer_pie_world, require_pie_world, source)
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = False
        raise
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_state_machine_runtime_response_native_result_invalid", "Native operation/change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_state_machine_runtime_response_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
