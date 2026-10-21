"""Specific UE58 AnimGraph pre/post binding; engine work stays in Native."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAnimNodePrePostRuntimePoseLibrary"
METHOD = "sample_anim_node_pre_post_runtime_pose"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
LEGACY_COMMAND_FIELDS = ("sample_bones", "sample_sockets", "anim_blueprint_path",
    "allow_missing_samples", "graph_name", "graph_id", "graph_type", "input_pose_index",
    "properties", "property_name", "curve_values")
GRAPH_FIELDS = ("graph_name", "graph_id", "graph_type")
IGNORED_DRIVER_FIELDS = ("properties", "property_name", "curve_values")


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _legacy_params(legacy):
    if legacy is None:
        return None  # Named semantics differ from a raw command with zero keys.
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("anim_node_pre_post_runtime_pose_input_invalid",
            "Private context permits only the eleven confirmed raw command fields")
    # The original warning only tests these three keys' presence. Their values
    # are not evaluated, serialized, or introduced into the Native DTO.
    return {key: None if key in IGNORED_DRIVER_FIELDS else value for key, value in legacy.items()}


def _private_context(legacy):
    legacy = _legacy_params(legacy)
    if legacy is None:
        return {}
    context = {"graph_selector": {key: legacy[key] for key in GRAPH_FIELDS if key in legacy}}
    for key in ("anim_blueprint_path", "allow_missing_samples", "input_pose_index"):
        if key in legacy:
            context[key] = legacy[key]
    present = [key for key in IGNORED_DRIVER_FIELDS if key in legacy]
    if present:
        context["ignored_driver_keys"] = present
    return context


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("anim_node_pre_post_runtime_pose_carrier_invalid",
            "Limited carriers require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("anim_node_pre_post_runtime_pose_carrier_bound",
            "Each carrier must fit 1 Mi UTF16 units and contain no raw NUL")
    return encoded


def _payloads(sample_bones, sample_sockets, legacy):
    legacy = _legacy_params(legacy)
    bones = legacy["sample_bones"] if legacy is not None and "sample_bones" in legacy else sample_bones
    sockets = legacy["sample_sockets"] if legacy is not None and "sample_sockets" in legacy else sample_sockets
    return _carrier(bones), _carrier(sockets), _carrier(_private_context(legacy))


def sample_anim_node_pre_post_runtime_pose(blueprint_name='', anim_blueprint='', graph_name='AnimGraph', graph_id='', graph_type='function', node_id='', node_type='', title_contains='', sample_bones=None, sample_sockets=None, mode='active_component_tick_delta', skeletal_mesh='', temp_root='/Game/_MCP_Temp/AnimNodePrePost', cleanup=True, overwrite_existing_temp_assets=True, run_id='', actor_label='', actor_name='', actor_path='', component_name='', anim_instance_source='main', prefer_pie_world=True, require_pie_world=False, tick_count=1, tick_delta_time=1.0 / 30.0, settle_tick_count=0, refresh_bone_transforms=True, allow_missing_bones=False, pose_link_max_depth=4, input_pose_mode='first', input_pose_field_path='', input_pose_index=-1, include_pins=True, max_depth=3, dry_run=True, *, _legacy_command_params=None):
    try:
        bones_json, sockets_json, context_json = _payloads(sample_bones, sample_sockets, _legacy_command_params)
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        error = OperationError("anim_node_pre_post_runtime_pose_native_api_unavailable",
            "UE58 AnimGraph pre/post binding is unavailable; no fallback executed")
        error.native_call_dispatched = False
        raise error
    raw = fn(blueprint_name, anim_blueprint, graph_name, graph_id, graph_type,
        node_id, node_type, title_contains, bones_json, sockets_json, mode,
        skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id,
        actor_label, actor_name, actor_path, component_name, anim_instance_source,
        prefer_pie_world, require_pie_world, tick_count, tick_delta_time,
        settle_tick_count, refresh_bone_transforms, allow_missing_bones,
        pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index,
        include_pins, max_depth, dry_run, context_json)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("anim_node_pre_post_runtime_pose_native_result_invalid",
                "Native operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("anim_node_pre_post_runtime_pose_native_result_invalid",
                "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
