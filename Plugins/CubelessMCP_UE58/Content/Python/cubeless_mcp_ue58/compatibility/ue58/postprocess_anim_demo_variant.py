"""UE58 PostProcess AnimBP/SkeletalMesh variant binding, with no RPC or retry."""
import json
import math
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessPostProcessAnimDemoVariantLibrary"
METHOD = "ensure_postprocess_anim_demo_variant"
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
            raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_bound", "Private JSON exceeds 65536 slots or nesting64")
        if current is None or type(current) in (bool, int):
            continue
        if type(current) is str:
            if len(current) > MAX_CARRIER_UTF16_UNITS or "\x00" in current:
                raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "JSON text exceeds its bound or contains decoded NUL")
            continue
        if type(current) is float:
            if not math.isfinite(current):
                raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "Private JSON numbers must be finite")
            continue
        if type(current) not in (list, dict):
            raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "Private input must contain only JSON values")
        identity = id(current)
        if identity in active:
            raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "Private JSON contains a container cycle")
        if len(current) + slots + len(pending) > MAX_JSON_SLOTS:
            raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_bound", "Private JSON exceeds 65536 value slots")
        active.add(identity)
        pending.append((current, depth, True))
        if type(current) is dict:
            for key in current:
                if type(key) is not str or len(key) > MAX_CARRIER_UTF16_UNITS or "\x00" in key:
                    raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "Private JSON requires bounded string keys without NUL")
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
                raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_bound", "Each private carrier is limited to 1Mi UE UTF16 units")
            chunks.append(chunk)
        return "".join(chunks)
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise _pre_dispatch_error("postprocess_anim_demo_variant_carrier_invalid", "Private input requires finite valid Unicode JSON") from error


LEGACY_PARAM_KEYS = frozenset(("target_blueprint_name", "target_skeletal_mesh", "graph_name", "graph_type", "rotation"))


def _legacy_projection(target_blueprint_name, target_skeletal_mesh, rotation, graph_name, graph_type, legacy_command_params=None):
    # A closed five-key presence projection, not generic Params or an engine executor.
    if legacy_command_params is None:
        legacy = {}
    elif type(legacy_command_params) is not dict or set(legacy_command_params) - LEGACY_PARAM_KEYS:
        raise _pre_dispatch_error("postprocess_anim_demo_variant_legacy_input_invalid", "Private legacy projection accepts only target2/graph2/rotation presence keys")
    else:
        legacy = legacy_command_params
    targets = []
    for key, public in (("target_blueprint_name",target_blueprint_name),("target_skeletal_mesh",target_skeletal_mesh)):
        value = legacy[key] if key in legacy else (None if public == "" else public)
        if value is not None and type(value) is not str:
            raise _pre_dispatch_error("postprocess_anim_demo_variant_legacy_input_unsupported", "Private target selector admits only original string or null; other raw GetString coercion is unsupported")
        targets.append(value)
    graphs = []
    for key, public, fallback in (("graph_name",graph_name,"AnimGraph"),("graph_type",graph_type,"function")):
        value = legacy[key] if key in legacy else (public or fallback)
        if value is None:
            # Actual GetStringParam uses its supplied default when JSON null is present.
            value = fallback
        elif type(value) is not str:
            raise _pre_dispatch_error("postprocess_anim_demo_variant_legacy_input_unsupported", "Private graph selector requires string/null; other raw coercion is unsupported")
        graphs.append(value)
    actual_rotation = legacy["rotation"] if "rotation" in legacy else ([0,0,6] if rotation is None else rotation)
    for value in (*targets, actual_rotation):
        _carrier(value)
    return targets[0], targets[1], actual_rotation, graphs[0], graphs[1]


def ensure_postprocess_anim_demo_variant(source_blueprint_name, source_skeletal_mesh, variant_name='Variant', target_root='/Game/_MCP_Sample/AnimStudy', target_blueprint_name='', target_skeletal_mesh='', bone_name='head', rotation=None, graph_name='AnimGraph', graph_id='', graph_type='function', replace_existing=True, overwrite_existing=False, compile=True, save=True, dry_run=False, allow_non_sample=False, *, _legacy_command_params=None):
    function = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(function):
        raise _pre_dispatch_error("postprocess_anim_demo_variant_native_api_unavailable", "UE58 specific PostProcess variant binding unavailable; no fallback executed")
    target_bp, target_mesh, actual_rotation, selected_graph_name, selected_graph_type = _legacy_projection(
        target_blueprint_name,target_skeletal_mesh,rotation,graph_name,graph_type,_legacy_command_params)
    target_bp_json = _carrier(target_bp)
    target_mesh_json = _carrier(target_mesh)
    rotation_json = _carrier(actual_rotation)
    try:
        raw = function(source_blueprint_name,source_skeletal_mesh,variant_name,target_root,target_bp_json,target_mesh_json,
            bone_name,rotation_json,selected_graph_name,graph_id,selected_graph_type,replace_existing,overwrite_existing,
            compile,save,dry_run,allow_non_sample)
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = False
        raise
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("postprocess_anim_demo_variant_native_result_invalid", "Native operation/change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("postprocess_anim_demo_variant_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
