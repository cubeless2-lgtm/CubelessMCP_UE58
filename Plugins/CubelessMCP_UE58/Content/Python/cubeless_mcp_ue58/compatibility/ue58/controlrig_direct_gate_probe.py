"""UE58 bindings for the two transient ControlRig probes; no transport or fallback."""
import json
import math
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessControlRigTransientProbeLibrary"
METHOD = "controlrig_direct_gate_probe"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
MAX_JSON_DEPTH = 64


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    # Finite JSON only. Preserve None/[]/{} and all row/proposal semantics for Native.
    active = set()
    visited_slots = 0
    stack = [(value, 0, False)]
    while stack:
        current, depth, exiting = stack.pop()
        if exiting:
            active.remove(id(current))
            continue
        if depth > MAX_JSON_DEPTH or type(current) in (list, dict) and depth >= MAX_JSON_DEPTH:
            raise OperationError("controlrig_transient_probe_carrier_bound", "Private JSON nesting exceeds64")
        visited_slots += 1
        if visited_slots > MAX_CARRIER_UTF16_UNITS:
            raise OperationError("controlrig_transient_probe_carrier_bound", "Probe JSON value inventory exceeds its encoded size budget")
        if type(current) is str and len(current) > MAX_CARRIER_UTF16_UNITS:
            raise OperationError("controlrig_transient_probe_carrier_bound", "Probe JSON string exceeds its encoded size budget")
        if current is None or type(current) in (str, bool, int):
            continue
        if type(current) is float:
            if not math.isfinite(current):
                raise OperationError("controlrig_transient_probe_carrier_invalid", "Probe JSON numbers must be finite")
            continue
        if type(current) not in (list, dict):
            raise OperationError("controlrig_transient_probe_carrier_invalid", "Probe input must contain only JSON values")
        identity = id(current)
        if identity in active:
            raise OperationError("controlrig_transient_probe_carrier_invalid", "Probe input contains a JSON cycle")
        # Even the shortest JSON value requires at least one encoded unit.
        # This cannot exclude a carrier that fits the size limit, and bounds the
        # pending traversal before allocating a stack for a very large input.
        if len(current) + visited_slots + len(stack) > MAX_CARRIER_UTF16_UNITS:
            raise OperationError("controlrig_transient_probe_carrier_bound", "Probe JSON value inventory exceeds its encoded size budget")
        active.add(identity)
        stack.append((current, depth, True))
        if type(current) is dict:
            if any(type(key) is not str for key in current):
                raise OperationError("controlrig_transient_probe_carrier_invalid", "Probe JSON object keys must be strings")
            if any(len(key) > MAX_CARRIER_UTF16_UNITS for key in current):
                raise OperationError("controlrig_transient_probe_carrier_bound", "Probe JSON object key exceeds its encoded size budget")
            children = list(current.values())
        else:
            children = current
        stack.extend((child, depth + 1, False) for child in reversed(children))
    try:
        chunks = []
        units = 0
        encoder = json.JSONEncoder(ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        for chunk in encoder.iterencode(value):
            units += len(chunk.encode("utf-16-le")) // 2
            if units > MAX_CARRIER_UTF16_UNITS:
                raise OperationError("controlrig_transient_probe_carrier_bound", "Each private probe carrier is limited to1 Mi UE UTF16 units")
            chunks.append(chunk)
        encoded = "".join(chunks)
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("controlrig_transient_probe_carrier_invalid", "Probe input requires finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("controlrig_transient_probe_carrier_bound", "Each private probe carrier is limited to1 Mi UE UTF16 units without raw NUL")
    return encoded


def _invoke(method, *arguments):
    # Internal reuse is limited to these two public Native bindings, not an execute tool.
    if method not in (METHOD, "sample_controlrig_pre_post_runtime_pose"):
        raise OperationError("controlrig_transient_probe_binding_invalid", "Unknown transient probe binding")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("controlrig_transient_probe_native_api_unavailable", "UE58 transient ControlRig probe binding is unavailable; no fallback executed")
    raw = function(*arguments)
    try:
        native = _decode_result(raw)
        if native.get("operation") != method or "changed" not in native:
            raise OperationError("controlrig_transient_probe_native_result_invalid", "Native probe operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("controlrig_transient_probe_native_result_invalid", "Native probe changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def controlrig_direct_gate_probe(control_rig_path="", control_rig_class="", cases=None,
        sample_elements=None, control_values=None, execute_events=None,
        should_trace_property="ShouldDoIKTrace", interaction_location_property="InteractionWorldLocation"):
    return _invoke(METHOD, control_rig_path, control_rig_class, _carrier(cases),
        _carrier(sample_elements), _carrier(control_values), _carrier(execute_events),
        should_trace_property, interaction_location_property)
