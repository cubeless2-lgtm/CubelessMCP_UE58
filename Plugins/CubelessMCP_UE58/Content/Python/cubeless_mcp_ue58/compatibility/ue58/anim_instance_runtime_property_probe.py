"""UE58 specific AnimInstance runtime-property probe binding; no transport or replay."""
import json
import math
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimInstanceRuntimePropertyProbeLibrary"
METHOD = "set_anim_instance_runtime_property_for_probe"
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
            raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_bound", "Private JSON exceeds 65536 slots or nesting64")
        if current is None or type(current) in (bool, int):
            continue
        if type(current) is str:
            if len(current) > MAX_CARRIER_UTF16_UNITS or "\x00" in current:
                raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "JSON text exceeds its bound or contains decoded NUL")
            continue
        if type(current) is float:
            if not math.isfinite(current):
                raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "Private JSON numbers must be finite")
            continue
        if type(current) not in (list, dict):
            raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "Private input must contain only JSON values")
        identity = id(current)
        if identity in active:
            raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "Private JSON contains a container cycle")
        if len(current) + slots + len(pending) > MAX_JSON_SLOTS:
            raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_bound", "Private JSON exceeds 65536 value slots")
        active.add(identity)
        pending.append((current, depth, True))
        if type(current) is dict:
            for key in current:
                if type(key) is not str or len(key) > MAX_CARRIER_UTF16_UNITS or "\x00" in key:
                    raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "Private JSON requires bounded string keys without NUL")
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
                raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_bound", "Each private carrier is limited to 1Mi UE UTF16 units")
            chunks.append(chunk)
        return "".join(chunks)
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise _pre_dispatch_error("anim_instance_runtime_property_probe_carrier_invalid", "Private input requires finite valid Unicode JSON") from error



LEGACY_PARAM_KEYS = frozenset(("properties", "property_values", "curve_names", "anim_instance_source"))


def _legacy_projection(properties, curve_names, source, legacy_command_params=None):
    # A specific two-key property DTO, not an engine executor or generic Params.
    if legacy_command_params is None:
        legacy = {}
    elif type(legacy_command_params) is not dict or set(legacy_command_params) - LEGACY_PARAM_KEYS:
        raise _pre_dispatch_error("anim_instance_runtime_property_probe_legacy_input_invalid", "Private legacy params accept only properties/property_values/curve_names/anim_instance_source")
    else:
        legacy = legacy_command_params
    property_params = {} if properties is None else {"properties": properties}
    for key in ("properties", "property_values"):
        if key in legacy:
            property_params[key] = legacy[key]
    curves = legacy["curve_names"] if "curve_names" in legacy else curve_names
    if curves is not None and type(curves) is not list:
        raise _pre_dispatch_error("anim_instance_runtime_property_probe_legacy_input_invalid", "Private curve_names requires null or its original array, including descriptor objects")
    selected_source = legacy["anim_instance_source"] if "anim_instance_source" in legacy else source
    if selected_source is None or type(selected_source) in (list, dict):
        # Actual TryGetStringField fails on these JSON kinds, preserving main.
        _carrier(selected_source)
        selected_source = "main"
    elif type(selected_source) is not str:
        raise _pre_dispatch_error("anim_instance_runtime_property_probe_legacy_source_unsupported", "Private numeric/boolean source coercion is unsupported; use an original source string")
    _carrier(selected_source)
    _carrier(property_params)
    _carrier(curves)
    return property_params, curves, selected_source


def set_anim_instance_runtime_property_for_probe(actor_label="", actor_name="", actor_path="", component_name="",
        properties=None, property_name="", value=None, tick_after_set=False, tick_count=1,
        tick_delta_time=1.0 / 30.0, refresh_bone_transforms=True, include_snapshot_after=True,
        include_previous_values=True, state_machine_name="", include_states=True, include_state_weights=True,
        include_relevant_anim_times=False, include_transition_progress=True, include_inactive_transition_progress=False,
        include_montages=True, include_curves=False, curve_names=None, max_transitions_per_machine=128,
        prefer_pie_world=True, require_pie_world=False, *, _anim_instance_source="main", _legacy_command_params=None):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise _pre_dispatch_error("anim_instance_runtime_property_probe_native_api_unavailable", "UE58 runtime-property probe binding is unavailable; no fallback executed")
    property_params, curves, selected_source = _legacy_projection(properties, curve_names, _anim_instance_source, _legacy_command_params)
    properties_json = _carrier(property_params)
    # Original Python did not put value into Params when property_name was empty.
    value_json = _carrier(value if property_name else None)
    curve_names_json = _carrier(curves)
    try:
        raw = function(actor_label, actor_name, actor_path, component_name, properties_json, property_name,
            value_json, tick_after_set, tick_count, tick_delta_time, refresh_bone_transforms,
            include_snapshot_after, include_previous_values, state_machine_name, include_states, include_state_weights,
            include_relevant_anim_times, include_transition_progress, include_inactive_transition_progress,
            include_montages, include_curves, curve_names_json, max_transitions_per_machine, prefer_pie_world, require_pie_world, selected_source)
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = False
        raise
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_instance_runtime_property_probe_native_result_invalid", "Native operation/change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_instance_runtime_property_probe_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
