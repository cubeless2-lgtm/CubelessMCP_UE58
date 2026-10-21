"""Specific UE58 linked AnimGraph variant binding; no transport or engine fallback."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessLinkedAnimGraphVariantLibrary"
METHOD = "ensure_linked_anim_graph_sample_variant"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576
GRAPH_FIELDS = ("graph_name", "graph_type")
TARGET_ALIAS_FIELDS = ("target_blueprint_path",)
UNSUPPORTED_LAYER_FIELDS = ("anim_layer_interface_path", "layer_name", "linked_layer_instance_bp_path")
LEGACY_COMMAND_FIELDS = GRAPH_FIELDS + TARGET_ALIAS_FIELDS + UNSUPPORTED_LAYER_FIELDS


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _private_graph_context(legacy):
    if legacy is None:
        return {}
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("linked_anim_graph_variant_input_invalid", "Private context permits graph selectors, one target alias and confirmed Layer-key presence only")
    context = {key: legacy[key] for key in GRAPH_FIELDS + TARGET_ALIAS_FIELDS if key in legacy}
    present = [key for key in UNSUPPORTED_LAYER_FIELDS if key in legacy]
    if present:
        context["unsupported_layer_keys"] = present
    return context


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("linked_anim_graph_variant_carrier_invalid", "Graph context requires finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("linked_anim_graph_variant_carrier_bound", "Private graph carrier exceeds 1 Mi UTF16 units or contains raw NUL")
    return encoded


def ensure_linked_anim_graph_sample_variant(source_anim_bp_path, linked_anim_bp_path,
        target_anim_bp_path="", target_root="/Game/_MCP_Sample/AnimStudy",
        variant_name="LinkedAnimGraphSample", insert_mode="input_pose_to_linked_graph_to_root",
        allow_existing_target_reuse=True, overwrite_existing=False, replace_existing_links=True,
        require_input_pose_pin=False, compile=True, save_asset=False, dry_run=False,
        validate_only=False, *, _legacy_command_params=None):
    try:
        context_json = _carrier(_private_graph_context(_legacy_command_params))
    except Exception as error:
        error.native_call_dispatched = False
        raise
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        error = OperationError("linked_anim_graph_variant_native_api_unavailable", "UE58 linked AnimGraph variant binding is unavailable; no fallback executed")
        error.native_call_dispatched = False
        raise error
    raw = fn(source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root,
        variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing,
        replace_existing_links, require_input_pose_pin, compile, save_asset, dry_run,
        validate_only, context_json)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("linked_anim_graph_variant_native_result_invalid", "Native operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("linked_anim_graph_variant_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native

