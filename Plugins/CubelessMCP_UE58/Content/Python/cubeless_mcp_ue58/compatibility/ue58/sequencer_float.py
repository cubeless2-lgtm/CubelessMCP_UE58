"""Finite Float-property native insertion and public saved-payload observation.

Native hidden5 is an agreed source pact, not current binding evidence. Exact
duplicate AddCubicKey/floor remains native; no Scripting AddKey. Shared primary
resolution retains the guarded loaded0/cold1 receipt and actual asset wrapper.
"""
import math
import unreal

from . import blueprint, sequencer as shared
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessSequencerLibrary"
NAME = "add_level_sequence_float_property_keys"
BINDINGS = (NAME,)
PREPARATION = "prepare_sequencer_float_toolset"
TRACK_CLASS = "/Script/MovieSceneTracks.MovieSceneFloatTrack"
SECTION_CLASS = "/Script/MovieSceneTracks.MovieSceneFloatSection"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {NAME: callable(getattr(library, NAME, None))}


def observation_binding_status():
    required = {
        "MovieScenePropertyTrackExtensions": ("get_property_name", "get_property_path"),
        "MovieSceneScriptingFloatKey": ("get_time", "get_value", "get_interpolation_mode", "get_tangent_mode",
            "get_arrive_tangent", "get_leave_tangent", "get_tangent_weight_mode", "get_arrive_tangent_weight", "get_leave_tangent_weight"),
        "MovieSceneScriptingFloatChannel": ("get_keys", "has_default", "get_default"),
        "MovieSceneSectionExtensions": ("get_all_channels",),
    }
    methods = {owner + "." + name: callable(getattr(getattr(unreal, owner, None), name, None))
        for owner, names in required.items() for name in names}
    methods.update({name: callable(getattr(unreal, name, None)) for name in ("MovieSceneFloatTrack", "MovieSceneFloatSection")})
    methods["shared_AX_public_routes"] = shared.construction_available()
    return methods


def _require(condition, code, message):
    if not condition:
        raise OperationError(code, message)


def _number(value):
    _require(type(value) in (int, float) and math.isfinite(value),
        "sequencer_float_observation_invalid", "Actual Float channel value/tangent must be finite")
    return value


def _channel(channel):
    keys = list(channel.get_keys())
    _require(len(keys) <= 8192, "sequencer_float_observation_bound_exceeded", "Float channel key inventory exceeds8192")
    rows = []
    for key in keys:
        time = key.get_time(unreal.MovieSceneTimeUnit.TICK_RESOLUTION)
        frame = time.get_editor_property("frame_number").get_editor_property("value")
        _require(type(frame) is int and -(2**31) <= frame <= 2**31 - 1,
            "sequencer_float_observation_invalid", "Actual tick frame must fit signed-int32")
        interpolation, tangent = key.get_interpolation_mode(), key.get_tangent_mode()
        rows.append({"tick_frame": frame, "value": _number(key.get_value()),
            "interpolation": str(interpolation), "tangent_mode": str(tangent),
            "is_cubic": interpolation == unreal.RichCurveInterpMode.RCIM_CUBIC,
            "is_auto_tangent": tangent == unreal.RichCurveTangentMode.RCTM_AUTO,
            "arrive_tangent": _number(key.get_arrive_tangent()), "leave_tangent": _number(key.get_leave_tangent()),
            "tangent_weight_mode": str(key.get_tangent_weight_mode()),
            "arrive_tangent_weight": _number(key.get_arrive_tangent_weight()),
            "leave_tangent_weight": _number(key.get_leave_tangent_weight())})
    has_default = bool(channel.has_default())
    return {"metadata_name": str(channel.get_editor_property("channel_name")),
        "class_path": channel.get_class().get_path_name(), "key_count": len(rows), "keys": rows,
        "has_default": has_default, "default": _number(channel.get_default()) if has_default else None,
        "wrapper_object_path_is_not_channel_identity": True}


def snapshot(asset):
    # The common reader preserves rates/bindings/root-track and transform
    # payload. Float additions observe only source-backed public getters.
    result = shared.snapshot(asset)
    float_tracks = []
    for binding in unreal.MovieSceneSequenceExtensions.get_bindings(asset):
        for track in unreal.MovieSceneBindingExtensions.get_tracks(binding):
            if track.get_class().get_path_name() != TRACK_CLASS:
                continue
            sections = []
            for section in unreal.MovieSceneTrackExtensions.get_sections(track):
                row = shared._section(section)
                if section.get_class().get_path_name() == SECTION_CLASS:
                    channels = list(unreal.MovieSceneSectionExtensions.get_all_channels(section))
                    _require(len(channels) == 1 and channels[0].get_class() == unreal.MovieSceneScriptingFloatChannel.static_class(),
                        "sequencer_float_channel_observation_invalid", "Exact FloatSection must expose one public Float channel wrapper")
                    row["float_channel"] = _channel(channels[0])
                    row["native_scalar_channel_type_and_override_absence_independently_observed"] = False
                sections.append(row)
            float_tracks.append({"binding_id": shared._guid(binding), "object_path": track.get_path_name(),
                "class_path": track.get_class().get_path_name(),
                "property_name": str(unreal.MovieScenePropertyTrackExtensions.get_property_name(track)),
                "property_path": unreal.MovieScenePropertyTrackExtensions.get_property_path(track), "sections": sections})
    result["float_property_tracks"] = float_tracks
    result["float_observation_scope"] = "public Float property identity/range/ordered tick-value-mode-tangent-weight/default projection; native channel type/override/full section properties use returned pre-save native proof"
    result["actor_property_evaluation_verified"] = False
    return result


def prepare(sequence_path):
    prefix = {"native_dispatched": False, "editor_idle_guard_dispatched": False,
        "engine_version_getter_dispatched": False, "loaded_sequence_resolver_dispatched": False,
        "public_snapshot_dispatched": False, "public_snapshot_returned": False,
        "adapter_explicit_asset_load_calls": 0, "scripting_wrapper_effects_fully_instrumented": False}
    try:
        _require(all(binding_status().values()), "sequencer_float_native_api_unavailable",
            "Build/load the exact agreed Float hidden5 binding; no scripting insertion fallback ran")
        _require(all(observation_binding_status().values()), "sequencer_float_public_api_unavailable",
            "Required source-backed Float and common AX public observer bindings are unavailable")
        prefix["editor_idle_guard_dispatched"] = True
        blueprint.ensure_editor_idle()
        prefix["engine_version_getter_dispatched"] = True
        _require(unreal.SystemLibrary.get_engine_version().startswith("5.8."),
            "sequencer_float_engine_version_unsupported", "Source-pinned UE5.8 APIs required")
        prefix["loaded_sequence_resolver_dispatched"] = True
        resolution = {}
        prefix["sequence_resolution"] = resolution
        asset = shared._resolve(sequence_path, resolution)
        prefix["adapter_explicit_asset_load_calls"] = resolution["asset_load_count"]
        prefix["public_snapshot_dispatched"] = True
        before = snapshot(asset)
        prefix["public_snapshot_returned"] = True
        return {"asset": asset, "before": before, "preparation_observation": prefix}
    except Exception as error:
        resolution = prefix.get("sequence_resolution", getattr(error, "sequence_resolution", None))
        if type(resolution) is dict:
            prefix["adapter_explicit_asset_load_calls"] = resolution.get("asset_load_count")
        error.observation_prefix = prefix
        raise


def add_level_sequence_float_property_keys(sequence_path, property_path, keys_json, binding_id, actor_label):
    function = getattr(getattr(unreal, LIBRARY, None), NAME, None)
    _require(callable(function), "sequencer_float_native_api_unavailable", "Exact agreed hidden5 unavailable; no native dispatch or fallback ran")
    raw = function(sequence_path, property_path, keys_json, binding_id, actor_label)
    try:
        return _decode_result(raw), raw
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise


def save_exact_asset(asset):
    # Original SaveLoadedAsset(false) public subsystem route, one call only.
    return blueprint.asset_subsystem().save_loaded_asset(asset, False)
