"""Finite UE58 public SequencerScripting/AssetTools implementation.

No server, private channel pointer or direct Initialize call. Primary resolution uses
the guarded Native loader; internal postchecks remain find-only. Exact
fractional playback and duplicate Cubic keys use two finite public native leaves.
"""

import copy
import json
import math
import uuid

import unreal

from . import blueprint, blueprint_duplicates
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
from cubeless_mcp_ue58.validators.dependencies import parent_chain, reject_tool_reference


BINDINGS = ("create_level_sequence", "get_level_sequence_info",
    "add_level_sequence_actor_binding", "add_level_sequence_transform_keys")
NATIVE_BINDINGS = ("apply_level_sequence_playback_range", "add_level_sequence_transform_keys")
CHANNELS = tuple(group + "." + axis for group in ("Location", "Rotation", "Scale") for axis in ("X", "Y", "Z"))
SEQUENCE_CLASS = "/Script/LevelSequence.LevelSequence"
MOVIE_SCENE_CLASS = "/Script/MovieScene.MovieScene"
TRACK_CLASS = "/Script/MovieSceneTracks.MovieScene3DTransformTrack"
SECTION_CLASS = "/Script/MovieSceneTracks.MovieScene3DTransformSection"


def _require(condition, code, message):
    if not condition:
        raise OperationError(code, message)


def package_path(value):
    try:
        return blueprint_duplicates.package_path(value, "sequence_path")
    except OperationError as error:
        raise OperationError("sequencer_path_invalid", str(error)) from error


def _bindings():
    methods = {
        "MovieSceneSequenceExtensions": ("get_movie_scene", "get_display_rate", "get_tick_resolution",
            "get_playback_start", "get_playback_end", "get_bindings", "get_tracks", "get_possessables",
            "set_display_rate", "add_possessable", "locate_bound_objects"),
        "MovieSceneBindingExtensions": ("get_id", "get_name", "get_tracks", "is_valid"),
        "MovieSceneTrackExtensions": ("get_sections",),
        "MovieSceneSectionExtensions": ("has_start_frame", "has_end_frame", "get_start_frame", "get_end_frame",
            "get_all_channels"),
        "AssetToolsHelpers": ("get_asset_tools",),
        "UnrealEditorSubsystem": ("get_editor_world", "get_game_world"),
        "GameplayStatics": ("get_all_actors_of_class",),
        "SourceControl": ("is_enabled",),
        "MovieSceneScriptingDoubleChannel": ("get_keys", "has_default", "get_default"),
        "MovieSceneScriptingFloatChannel": ("get_keys", "has_default", "get_default"),
    }
    for owner, names in methods.items():
        cls = getattr(unreal, owner, None)
        _require(cls is not None and all(callable(getattr(cls, name, None)) for name in names),
            "sequencer_public_api_unavailable", owner + " required public methods are unavailable; no mutation or fallback ran")
    for name in ("LevelSequence", "LevelSequenceFactoryNew", "MovieScene3DTransformTrack", "MovieScene3DTransformSection",
            "FrameRate", "Actor", "AssetToolsHelpers"):
        _require(callable(getattr(unreal, name, None)), "sequencer_public_api_unavailable", name + " reflected type is unavailable")
    _require(hasattr(unreal.MovieSceneTimeUnit, "TICK_RESOLUTION")
        and hasattr(unreal.RichCurveInterpMode, "RCIM_CUBIC")
        and hasattr(unreal.RichCurveTangentMode, "RCTM_AUTO"),
        "sequencer_public_api_unavailable", "Exact native time/interpolation enums are unavailable")
    for method in NATIVE_BINDINGS:
        _native_binding(method)


def _native_binding(method):
    _require(method in NATIVE_BINDINGS, "sequencer_operation_invalid", "Only the two exact native Sequencer leaves are admitted")
    function = getattr(getattr(unreal, "CubelessSequencerLibrary", None), method, None)
    _require(callable(function), "sequencer_native_api_unavailable",
        "Build/load the exact CubelessSequencerLibrary binding; no dispatch or scripting fallback ran")
    return function


def construction_available():
    try:
        _bindings()
        return True
    except Exception:
        return False


def _dirty():
    return {p.get_path_name() for p in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()}


def _resolve_loaded(package):
    asset = unreal.find_object(None, package + "." + package.rsplit("/", 1)[-1])
    _require(asset is not None and asset.get_class().get_path_name() == SEQUENCE_CLASS,
        "sequencer_exact_loaded_path_required", "Load the exact native /Game LevelSequence through a separate named asset tool first")
    movie = unreal.MovieSceneSequenceExtensions.get_movie_scene(asset)
    _require(movie is not None and movie.get_class().get_path_name() == MOVIE_SCENE_CLASS
        and movie.get_outer() == asset, "sequencer_movie_scene_invalid", "An exact sequence-owned native MovieScene is required")
    return asset


def _resolve(package, receipt=None):
    # Retained wrappers participate in UObject reference collection; paths do not.
    prefix = receipt if receipt is not None else {}
    prefix.update(native_dispatched=False, native_returned=False, receipt_decoded=False,
        context_known=True, asset_load_count=0, asset_load_invoked=False,
        python_identity_verified=False, full_loader_effects_measured=False)
    native = None
    try:
        function = getattr(getattr(unreal, "CubelessSequencerLibrary", None), "resolve_level_sequence_asset", None)
        _require(callable(function), "sequencer_native_resolver_unavailable",
            "Exact private sequence resolver unavailable; no load or fallback ran")
        prefix.update(native_dispatched=True, context_known=False,
            asset_load_count=None, asset_load_invoked=None)
        try:
            returned = function(package + "." + package.rsplit("/", 1)[-1])
        finally:
            function = None
        prefix["native_returned"] = True
        _require(type(returned) is tuple and len(returned) == 2,
            "sequencer_native_resolver_return_invalid", "Actual asset and copied receipt tuple required; no loader retry")
        asset, raw = returned
        prefix["native_raw_result"] = raw
        native = _decode_result(raw)
        prefix.update(receipt_decoded=True, native_result=copy.deepcopy(native),
            context_known=native["change_state_known"])
        count = native["data"].get("asset_load_count")
        prefix.update(asset_load_count=count, asset_load_invoked=None if type(count) is not int else count > 0)
        if not native["success"]:
            raise OperationError(native.get("error_code") or "sequencer_native_resolve_failed",
                native.get("error") or "Native sequence resolution failed; actual load prefix retained")
        _require(native["change_state_known"] and native["postcondition_passed"],
            "sequencer_native_resolve_unknown", "Unknown resolver boundary; no target getter, writer or save")
        _require(type(count) is int and count in (0, 1),
            "sequencer_native_loader_prefix_invalid", "Actual loaded0 or primary-cold1 prefix required")
        _require(not any(native[k] for k in ("compile_attempted", "save_attempted", "saved")),
            "sequencer_native_resolver_policy_mismatch", "Resolver unexpectedly compiled/saved; actual receipt retained")
        # Failure/known decision precedes every target getter.
        _require(asset is not None and unreal.SystemLibrary.is_valid(asset) is True,
            "sequencer_native_asset_unavailable", "Returned actual sequence wrapper unavailable")
        path = package + "." + package.rsplit("/", 1)[-1]
        _require(unreal.find_object(None, path, follow_redirectors=False) == asset
            and asset.get_path_name() == path and asset.get_class().get_path_name() == SEQUENCE_CLASS,
            "sequencer_native_asset_identity_changed", "Fresh identity differs from held Native return; no retry")
        movie = unreal.MovieSceneSequenceExtensions.get_movie_scene(asset)
        _require(movie is not None and unreal.SystemLibrary.is_valid(movie) is True
            and movie.get_class().get_path_name() == MOVIE_SCENE_CLASS and movie.get_outer() == asset,
            "sequencer_movie_scene_invalid", "Exact current sequence-owned native MovieScene required")
        prefix["python_identity_verified"] = True
        return asset
    except Exception as error:
        if prefix["native_dispatched"] and not (native is not None and native["success"] is False):
            prefix["context_known"] = False
        error.sequence_resolution = copy.deepcopy(prefix)
        raise


def _rate(value):
    numerator = value.get_editor_property("numerator")
    denominator = value.get_editor_property("denominator")
    _require(type(numerator) is int and type(denominator) is int and numerator > 0 and denominator > 0,
        "sequencer_rate_observation_invalid", "Observed frame rate must be a positive rational")
    return {"numerator": numerator, "denominator": denominator, "fps": numerator / denominator}


def _guid(binding):
    # Native GuidLibrary ScriptMethod=ToString; do not stringify the struct's
    # export-text representation or invent private GUID field access.
    return str(uuid.UUID(unreal.MovieSceneBindingExtensions.get_id(binding).to_string()))


def _channel(channel):
    name = str(channel.get_editor_property("channel_name"))
    key_objects = list(channel.get_keys())
    _require(len(key_objects) <= 8192, "sequencer_observation_bound_exceeded", "Channel key inventory exceeds the finite observation bound")
    keys = []
    for key in key_objects:
        frame = key.get_time(unreal.MovieSceneTimeUnit.TICK_RESOLUTION)
        value = key.get_value()
        _require(type(value) in (int, float) and math.isfinite(value), "sequencer_key_observation_invalid", "Channel contains a nonfinite value")
        keys.append({"tick_frame": frame.get_editor_property("frame_number").get_editor_property("value"),
            "value": value, "interpolation": str(key.get_interpolation_mode()),
            "tangent_mode": str(key.get_tangent_mode()),
            "is_cubic": key.get_interpolation_mode() == unreal.RichCurveInterpMode.RCIM_CUBIC,
            "is_auto_tangent": key.get_tangent_mode() == unreal.RichCurveTangentMode.RCTM_AUTO})
    has_default = bool(channel.has_default())
    default = channel.get_default() if has_default else None
    _require(not has_default or type(default) in (int, float) and math.isfinite(default),
        "sequencer_key_observation_invalid", "Channel default must be finite when present")
    return {"name": name, "class_path": channel.get_class().get_path_name(),
        "has_default": has_default, "default": default,
        "key_count": len(keys), "keys": keys}


def _section(section):
    result = {"object_path": section.get_path_name(), "class_path": section.get_class().get_path_name(),
        "start_bounded": bool(unreal.MovieSceneSectionExtensions.has_start_frame(section)),
        "end_bounded": bool(unreal.MovieSceneSectionExtensions.has_end_frame(section))}
    if result["start_bounded"]:
        result["start_display_frame"] = unreal.MovieSceneSectionExtensions.get_start_frame(section)
    if result["end_bounded"]:
        result["end_display_frame"] = unreal.MovieSceneSectionExtensions.get_end_frame(section)
    if result["class_path"] == SECTION_CLASS:
        channels = list(unreal.MovieSceneSectionExtensions.get_all_channels(section))
        doubles = [c for c in channels if c.get_class() == unreal.MovieSceneScriptingDoubleChannel.static_class()]
        _require(tuple(str(c.get_editor_property("channel_name")) for c in doubles) == CHANNELS,
            "sequencer_transform_channels_invalid", "Require exactly nine Double channels in native Location/Rotation/Scale X/Y/Z order")
        others = [c for c in channels if c.get_class() != unreal.MovieSceneScriptingDoubleChannel.static_class()]
        _require(all(c.get_class() == unreal.MovieSceneScriptingFloatChannel.static_class()
            and str(c.get_editor_property("channel_name")) == "Weight" for c in others) and len(others) <= 1,
            "sequencer_transform_channels_invalid", "Only the actual separate Float Weight channel is admitted beside the nine Double channels")
        result.update(transform_channels=[_channel(c) for c in doubles], other_channels=[_channel(c) for c in others],
            public_channel_wrapper_paths_are_not_channel_identity=True,
            auto_tangent_numeric_derivatives_observed=False)
    return result


def _track(track):
    sections = list(unreal.MovieSceneTrackExtensions.get_sections(track))
    _require(len(sections) <= 32, "sequencer_observation_bound_exceeded", "Track section inventory exceeds32")
    return {"object_path": track.get_path_name(), "class_path": track.get_class().get_path_name(),
        "sections": [_section(s) for s in sections]}


def snapshot(asset):
    sequence = unreal.MovieSceneSequenceExtensions
    movie = sequence.get_movie_scene(asset)
    package = asset.get_outermost().get_path_name()
    bindings = list(sequence.get_bindings(asset))
    _require(len(bindings) <= 128, "sequencer_observation_bound_exceeded", "Binding inventory exceeds128")
    rows = []
    for binding in bindings:
        tracks = list(unreal.MovieSceneBindingExtensions.get_tracks(binding))
        _require(len(tracks) <= 32, "sequencer_observation_bound_exceeded", "Binding track inventory exceeds32")
        rows.append({"binding_id": _guid(binding), "name": unreal.MovieSceneBindingExtensions.get_name(binding),
            "tracks": [t.get_class().get_path_name() for t in tracks], "track_observations": [_track(t) for t in tracks]})
    return {"sequence": asset.get_path_name(), "movie_scene_path": movie.get_path_name(),
        "class_path": asset.get_class().get_path_name(), "package": package, "package_dirty": package in _dirty(),
        "display_rate_value": _rate(sequence.get_display_rate(asset)), "tick_resolution_value": _rate(sequence.get_tick_resolution(asset)),
        "playback_start_frame": sequence.get_playback_start(asset), "playback_end_frame": sequence.get_playback_end(asset),
        "precise_playback_tick_bounds_observed": False, "playback_bounds_scope": "integer display-frame projection; fractional native bounds not independently observed",
        "binding_count": len(rows), "bindings": rows,
        "root_tracks": [_track(t) for t in sequence.get_tracks(asset)],
        "has_camera_cut_track": any(t.get_class().get_path_name() == "/Script/MovieSceneTracks.MovieSceneCameraCutTrack" for t in sequence.get_tracks(asset)),
        "file": blueprint_duplicates.file_state(package),
        "observation_scope": "asset identity/rates/integer range/binding-track-section topology and all native transform Double/Weight time-value-mode data; other track payload and tangent derivatives unobserved",
        "implicit_scripting_wrapper_allocation": True, "editor_callback_side_effects_measured": False}


def get_level_sequence_info(sequence_path):
    _bindings()
    resolution = {}
    try:
        asset = _resolve(sequence_path, resolution)
        before = snapshot(asset)
        rate = before["display_rate_value"]
        tick = before["tick_resolution_value"]
        after = snapshot(asset)
    except Exception as error:
        error.sequence_resolution = copy.deepcopy(resolution)
        raise
    return {**before, "display_rate": str(rate["numerator"]) + "/" + str(rate["denominator"]),
        "tick_resolution": str(tick["numerator"]) + "/" + str(tick["denominator"]),
        "rate_pretty_text_parity": False, "before": before, "after": after, "payload_observed_unchanged": before == after,
        "sequence_resolution": copy.deepcopy(resolution),
        "asset_load_invoked": resolution["asset_load_invoked"],
        "adapter_explicit_asset_load_calls": resolution["asset_load_count"],
        "read_only_asset_observation": resolution["asset_load_count"] == 0,
        "explicit_sequence_mutation_requested": False, "loader_callback_effects_fully_measured": False,
        "renderer_verified": False, "playback_execution_verified": False}



def _prepare(identity, arguments, resolution):
    _bindings()
    blueprint.ensure_editor_idle()
    _require(unreal.SystemLibrary.get_engine_version().startswith("5.8."), "sequencer_engine_version_unsupported", "This implementation is source-pinned to UE5.8 public APIs")
    package = arguments["sequence_path"]
    if identity == BINDINGS[0]:
        _require(unreal.SourceControl.is_enabled() is False, "sequencer_source_control_unsupported", "AssetTools may save through source control; creation requires it disabled without changing settings")
        before = blueprint_duplicates.require_target_absent(package)
        return {"asset": None, "public": {"before": before, "sequence": package,
            "factory_initialization_route": "public AssetTools + default LevelSequenceFactoryNew; Initialize is not exposed directly",
            "factory_implicit_callbacks_measured": False, "fractional_playback_supported": True}}
    asset = _resolve(package, resolution)
    before = snapshot(asset)
    context = {"asset": asset, "public": {"before": before, "sequence": asset.get_path_name(),
        "sequence_resolution": copy.deepcopy(resolution), "asset_load_invoked": resolution["asset_load_invoked"],
        "adapter_explicit_asset_load_calls": resolution["asset_load_count"]}}
    bindings = list(unreal.MovieSceneSequenceExtensions.get_bindings(asset))
    if identity == BINDINGS[2]:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        _require(world is not None, "sequencer_editor_world_missing", "Current editor world is required")
        actors = list(unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor))
        actor = next((a for a in actors if a.get_actor_label() == arguments["actor_label"] or a.get_name() == arguments["actor_label"]), None)
        _require(actor is not None and actor.get_world() == world, "sequencer_actor_not_found", "No actor label/object-name match in current editor world")
        reject_tool_reference(actor.get_class().get_path_name())
        parent_chain(actor.get_class())
        existing = next((b for b in bindings if unreal.MovieSceneBindingExtensions.get_name(b) == actor.get_actor_label()), None)
        context.update(actor=actor, world=world, binding=existing)
        context["public"].update(actor_path=actor.get_path_name(), actor_class_path=actor.get_class().get_path_name(),
            actor_label=actor.get_actor_label(), world_path=world.get_path_name(), selected_actor_order="first public world Actor iteration match")
        if existing is None:
            for binding in unreal.MovieSceneSequenceExtensions.get_possessables(asset):
                located = unreal.MovieSceneSequenceExtensions.locate_bound_objects(asset, binding, world)
                _require(all(obj != actor for obj in located), "sequencer_actor_other_binding_incomplete",
                    "Public AddPossessable reuses same-object bindings with another name; original always-new variant requires native fallback")
            context["public"].update(binding_resolver_transient_evaluation_invoked=True, binding_resolver_allocation_measured=False)
        return context
    _require(identity == BINDINGS[3], "sequencer_operation_invalid", "Only the three exact named mutations are admitted")
    binding = next((b for b in bindings if _guid(b) == arguments["binding_id"]), None) if arguments["binding_id"] else next(
        (b for b in bindings if unreal.MovieSceneBindingExtensions.get_name(b) == arguments["actor_label"]), None)
    _require(binding is not None, "sequencer_binding_not_found", "No supplied GUID or first matching binding name found")
    context.update(binding=binding)
    context["public"].update(binding_id=_guid(binding), requested_key_rows=len(arguments["keys"]),
        axis_order=list(CHANNELS), rotation_component_order=["roll", "pitch", "yaw"],
        fractional_key_floor_conversion=True, duplicate_tick_insertion_supported=True,
        whole_request_key_prevalidation=True, original_invalid_late_row_prefix_semantics_preserved=False)

    return context


def prepare(identity, arguments):
    resolution = {}
    try:
        return _prepare(identity, arguments, resolution)
    except Exception as error:
        if resolution:
            error.sequence_resolution = copy.deepcopy(resolution)
        raise


def execute(identity, context, arguments, notify):
    asset = context["asset"]
    before = context["public"]["before"]
    output = {"save_requested": arguments["save"], "save_attempted": False, "saved": False,
        "explicit_compile_requests": 0, "exact_save_requests": 0, "keys_added": 0,
        "channel_writes_returned": 0, "channel_writes_verified": 0, "postcondition_verified": False}
    context["output"] = output

    def call(stage, intent, function):
        notify(stage, intent)
        if stage == "save_exact_asset":
            output.update(save_attempted=True, exact_save_requests=1)
        returned = function()
        if asset is not None:
            notify(stage, intent, snapshot(asset))
        return returned

    def native_call(method, *native_arguments):
        # Resolve before the checkpoint, so a missing binding is known no-dispatch.
        function = _native_binding(method)
        intent = {"object_path": asset.get_path_name(), "native_method": method}
        notify(method, intent)
        raw = function(*native_arguments)
        output.setdefault("native_raw_results", []).append(raw)
        native = _decode_result(raw)
        output.setdefault("native_results", []).append(copy.deepcopy(native))
        data = native["data"]
        output.update(last_native_operation=method, last_native_change_state_known=native["change_state_known"],
            last_native_postcondition_passed=native["postcondition_passed"])
        if method == "add_level_sequence_transform_keys":
            for key in ("keys_added", "scalar_keys_added", "insertions", "created_track", "created_section",
                    "section_allocation_returned", "binding_id", "track_path", "section_path", "requested_key_rows",
                    "channels_before_insertion", "channels_after_insertion", "native_cubic_channels_verified",
                    "weight_channel_preserved", "nonselected_state_preserved", "duplicate_tick_insertion_supported",
                    "cubic_interpolation_value", "auto_tangent_mode_value", "rotation_axis_order"):
                if key in data:
                    output[key] = copy.deepcopy(data[key])
            output["channel_writes_returned"] = data.get("scalar_keys_added", 0)
        else:
            for key in ("requested_start_frame", "requested_end_frame", "start_tick", "end_tick",
                    "playback_floor_verified", "playback_editor_ranges_verified", "nonselected_state_preserved"):
                if key in data:
                    output[key] = copy.deepcopy(data[key])
        # Native state is authoritative and is retained before a public observer,
        # postcondition error, or requested save can fail. These readers differ.
        notify(method, intent, native=native)
        _require(not any(native[key] for key in ("compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")),
            "sequencer_native_policy_mismatch", "Native leaf unexpectedly requested compile/save; raw result is retained")
        if not native["success"]:
            raise OperationError(native.get("error_code") or "sequencer_native_failed", native.get("error") or "Native mutation failed; retained prefix must not be replayed")
        _require(native["change_state_known"] and native["postcondition_passed"],
            "sequencer_native_postcondition_failed", "Native returned without a verified known post-state; no save or retry")
        return data

    if identity == BINDINGS[0]:
        directory, _, name = arguments["sequence_path"].rpartition("/")
        # Construct only the transient reflected factory; no private editor
        # header or nonreflected ULevelSequence.Initialize call is used.
        factory = unreal.LevelSequenceFactoryNew()
        _require(factory.get_class().get_path_name() == "/Script/LevelSequenceEditor.LevelSequenceFactoryNew",
            "sequencer_factory_type_invalid", "Only the exact reflected native default factory is admitted")
        notify("create_sequence_asset", {"package": arguments["sequence_path"], "factory_class": factory.get_class().get_path_name()})
        asset = unreal.AssetToolsHelpers.get_asset_tools().create_asset(name, directory, unreal.LevelSequence, factory)
        context["asset"] = asset
        _require(asset is not None, "sequencer_factory_failed", "Factory did not return an asset; inspect registry/disk before any new request")
        _resolve_loaded(arguments["sequence_path"])
        output.update(created=True, sequence=asset.get_path_name())
        notify("create_sequence_asset", {"package": arguments["sequence_path"]}, snapshot(asset))
        call("set_display_rate", {"fps": arguments["frame_rate"]}, lambda: unreal.MovieSceneSequenceExtensions.set_display_rate(
            asset, unreal.FrameRate(numerator=int(arguments["frame_rate"] + 0.5), denominator=1)))
        native_range = native_call("apply_level_sequence_playback_range", asset.get_path_name(),
            arguments["playback_start_frame"], arguments["playback_end_frame"])
        observed = snapshot(asset)
        _require(observed["display_rate_value"] == {"numerator": int(arguments["frame_rate"] + 0.5), "denominator": 1, "fps": int(arguments["frame_rate"] + 0.5)}
            and native_range.get("playback_floor_verified") is True
            and native_range.get("playback_editor_ranges_verified") is True
            and native_range.get("nonselected_state_preserved") is True,
            "sequencer_creation_postcondition_failed", "Observed rate/native floor range differs; do not replay factory")
        output.update(sequence=asset.get_path_name(), requested_frame_rate=arguments["frame_rate"],
            frame_rate=int(arguments["frame_rate"] + 0.5), frame_rate_numerator=int(arguments["frame_rate"] + 0.5), frame_rate_denominator=1,
            playback_start_frame=arguments["playback_start_frame"], playback_end_frame=arguments["playback_end_frame"], created=True,
            precise_playback_tick_bounds_observed=True,
            precise_playback_observation_scope="returned native mutation snapshot before exact save; public Info remains integer projection")
    elif identity == BINDINGS[2]:
        existing = context["binding"]
        if existing is not None:
            after = snapshot(asset)
            _require(before == after, "sequencer_binding_reuse_postcondition_failed", "Existing binding observation changed")
            return {**output, "sequence": asset.get_path_name(), "binding_id": _guid(existing),
                "name": unreal.MovieSceneBindingExtensions.get_name(existing), "created": False,
                "save_requested": False, "original_save_requested": arguments["save"], "skipped_existing": True,
                "after": after, "postcondition_verified": True, "affected_count": 0, "created_count": 0, "skipped_count": 1}
        binding = call("add_actor_possessable", {"actor_path": context["actor"].get_path_name()},
            lambda: unreal.MovieSceneSequenceExtensions.add_possessable(asset, context["actor"]))
        _require(unreal.MovieSceneBindingExtensions.is_valid(binding), "sequencer_binding_postcondition_failed", "Returned binding is invalid")
        after = snapshot(asset)
        guid = _guid(binding)
        _require(after["binding_count"] == before["binding_count"] + 1
            and any(b["binding_id"] == guid and b["name"] == context["actor"].get_actor_label() for b in after["bindings"])
            and [b for b in after["bindings"] if b["binding_id"] != guid] == before["bindings"],
            "sequencer_binding_postcondition_failed", "New GUID/count/name or prior bindings differ")
        output.update(sequence=asset.get_path_name(), binding_id=guid, name=context["actor"].get_actor_label(), created=True)
    else:
        native_keys = native_call("add_level_sequence_transform_keys", asset.get_path_name(),
            json.dumps(arguments["keys"], ensure_ascii=False, allow_nan=False, separators=(",", ":")),
            arguments["binding_id"], arguments["actor_label"])
        _require(native_keys.get("keys_added") == len(arguments["keys"])
            and native_keys.get("native_cubic_channels_verified") is True
            and native_keys.get("weight_channel_preserved") is True
            and native_keys.get("nonselected_state_preserved") is True,
            "sequencer_nine_channel_postcondition_failed", "Native full channel/Weight/nonselected postcondition differs; prefix retained")
        output.update(sequence=asset.get_path_name(), channel_writes_verified=native_keys["scalar_keys_added"],
            nine_channel_payload_verified=True, separate_weight_preserved=True,
            nonselected_asset_projection_preserved=True, scripting_channel_changed_notification_invoked=False,
            implicit_editor_callbacks_measured=False, native_cubic_insertion_used=True)
    before_save = snapshot(asset)
    output["after"] = before_save
    if arguments["save"]:
        saved = call("save_exact_asset", {"object_path": asset.get_path_name(), "only_if_is_dirty": False},
            lambda: blueprint.asset_subsystem().save_loaded_asset(asset, False))
        _require(saved is True, "sequencer_exact_save_failed", "Exact public forced save returned False; observe disk/journal and do not automatically retry")
        output.update(saved=True, after_save=snapshot(asset))
        _require(output["after_save"]["file"]["exists"] is True and output["after_save"]["package_dirty"] is False,
            "sequencer_saved_file_unverified", "Saved file/clean package proof failed")
        payload_before = {k: v for k, v in before_save.items() if k not in ("file", "package_dirty")}
        payload_after = {k: v for k, v in output["after_save"].items() if k not in ("file", "package_dirty")}
        _require(payload_before == payload_after, "sequencer_saved_payload_changed", "Observed sequence payload changed across exact save")
    output.update(postcondition_verified=True, affected_count=1, created_count=1 if identity == BINDINGS[0] or identity == BINDINGS[2] else 0,
        before=before, engine_implicit_save_count_measured=False, full_legacy_sequencer_parity=False,
        playback_execution_verified=False, renderer_verified=False, disk_reload_verified=False)
    return output
