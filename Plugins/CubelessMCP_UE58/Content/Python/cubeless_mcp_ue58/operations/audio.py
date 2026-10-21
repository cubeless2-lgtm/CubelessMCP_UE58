"""Four original Audio contracts; explicit native builder scope, no playback proof."""

import copy
import math

from cubeless_mcp_ue58.compatibility.ue58 import audio as api
from . import state
from .state import OperationError


LIST = "list_audio_assets"
INSPECT = "inspect_audio_asset"
CREATE_CUE = "create_sound_cue_from_wave"
CREATE_META = "create_basic_metasound_source"
_FORBIDDEN = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
    "/cubelessmcp_ue58/", "/script/cubelessmcpue58editor")


def _text(value, field):
    if type(value) is not str or "\x00" in value:
        raise OperationError("audio_validation_failed", field + " must be a string without NUL; no Engine Audio action ran")
    try:
        value.encode("utf-16-le")
    except UnicodeEncodeError as error:
        raise OperationError("audio_validation_failed", field + " contains an invalid surrogate; no Engine Audio action ran") from error
    if any(token in value.replace("\\", "/").lower() for token in _FORBIDDEN):
        raise OperationError("audio_validation_failed", "MCP-only dependency reference in " + field + "; no Engine Audio action ran")
    return value


def _int32(value, field):
    if type(value) is not int or not -(2**31) <= value <= 2**31 - 1:
        raise OperationError("audio_validation_failed", field + " requires a signed-int32 integer, not a boolean or coercible value")
    return value


def _list_arguments(directory, recursive, max_results, offset):
    if type(recursive) is not bool:
        raise OperationError("audio_validation_failed", "recursive requires an exact boolean")
    return {"directory": _text(directory, "directory"), "recursive": recursive,
        "max_results": max(1, min(_int32(max_results, "max_results"), 1000)),
        "offset": max(_int32(offset, "offset"), 0)}


def _cue_arguments(name, sound_wave_path, package_path):
    return {"name": _text(name, "name"), "sound_wave_path": _text(sound_wave_path, "sound_wave_path"),
        "package_path": _text(package_path, "package_path")}


def _metasound_arguments(name, package_path, oscillator, frequency, output_format):
    if type(frequency) not in (int, float):
        raise OperationError("audio_invalid_frequency", "frequency requires a finite number, not a boolean or coercible value; no builder dispatch ran")
    try:
        numeric_frequency = float(frequency)
    except (ValueError, OverflowError) as error:
        raise OperationError("audio_invalid_frequency", "frequency cannot be represented as a finite double; no builder dispatch ran") from error
    if not math.isfinite(numeric_frequency):
        raise OperationError("audio_invalid_frequency", "frequency requires a finite number; no builder dispatch ran")
    # Original wrapper converts to float. Range 1..20000 (including noise) and
    # oscillator/output normalization remain the exact native parser's work.
    return {"name": _text(name, "name"), "package_path": _text(package_path, "package_path"),
        "oscillator": _text(oscillator, "oscillator"), "frequency": numeric_frequency,
        "output_format": _text(output_format, "output_format")}


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0,
        adapter_explicit_save_calls=0, automatic_action_retry=False,
        full_legacy_audio_parity=False,
        playback_verified=False, cold_persistence_verified=False, packaged_audio_verified=False,
        library_cdo_before_native_entry_independently_observed=False)


def _apply_native(result, identity, arguments, native):
    # Legacy data.saved, actual save receipts, raw phase snapshots and partial
    # counts are kept verbatim before success/postcondition checks or errors.
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_call_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, native_change_state_known=known,
        actual_work_counts_authority="returned_native_counters_and_ordered_save_calls")
    _context(result, identity, arguments)
    result["stage"] = native.get("stage", "native_audio_action")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="not_requested" if not native["compile_requested"] else "unexpected_native_compile")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_observed_work = known and native["changed"] is False and not native["mutation_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_observed_work else [{"kind": identity,
        "asset_path": native["data"].get("asset_path", arguments.get("asset_path", "")),
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None,
        "outcome": "native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_audio_postcondition" if native["postcondition_passed"] else "native_audio_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        full_initial_load_cache_effects="not_instrumented", audio_playback="not_run", packaged_runtime="not_run")


def _native_action(result, identity, arguments, checkpoint_callback=None):
    _context(result, identity, arguments)
    result["stage"] = "native_audio_action"
    result["data"].update(native_dispatched=True, native_call_returned=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "attempted": None, "change_state_known": False,
        "changed": None, "outcome": "unknown_until_native_response"}]
    try:
        if identity == INSPECT:
            native = api.inspect_audio_asset(arguments["asset_path"])
        elif identity == CREATE_CUE:
            native = api.create_sound_cue_from_wave(arguments["name"], arguments["sound_wave_path"], arguments["package_path"])
        elif identity == CREATE_META:
            native = api.create_basic_metasound_source(arguments["name"], arguments["package_path"],
                arguments["oscillator"], arguments["frequency"], arguments["output_format"])
        else:
            raise OperationError("audio_operation_invalid", "Only the three exact native Audio actions are admitted")
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_call_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            if checkpoint_callback:
                checkpoint_callback(result)
        if getattr(error, "code", "") == "native_audio_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    if checkpoint_callback:
        checkpoint_callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_audio_action_failed", native.get("error") or "Native Audio action failed; actual prefix is retained")
    if not known_success(native):
        raise OperationError("audio_result_unverified", "Native success lacks known Audio postconditions; inspect retained evidence before another call")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None:
        raise OperationError("audio_unexpected_compile", "Audio action unexpectedly reports explicit compilation; raw actual result is retained")
    if identity == INSPECT and (native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("audio_inspection_unexpected_save", "Inspection unexpectedly reports an explicit save; retained cache scope is separate")
    # Inspection may resolve/load or refresh aggregate caches. Do not require
    # mutation_attempted=False, or treat an empty scoped changes list as proof
    # that no initial getter/wrapping/cache effects occurred.
    result.update(success=True, stage="complete")
    if identity in (CREATE_CUE, CREATE_META):
        result["retry"]["duplicate_risk"] = "inspect_target_and_journal_before_new_request"


def known_success(native):
    return native["change_state_known"] is True and native["postcondition_passed"] is True


def _rejected(identity, error):
    result = state.fail(state.new_result(), error)
    _context(result, identity, {})
    result["data"].update(admission_rejected=True, native_dispatched=False,
        engine_api_dispatched=False, mutation_state_unknown=False)
    return result


def list_audio_assets(directory="/Game", recursive=True, max_results=200, offset=0):
    try:
        arguments = _list_arguments(directory, recursive, max_results, offset)
    except Exception as error:
        return _rejected(LIST, error)
    result = state.new_result()
    try:
        list_audio_assets_action(result, **arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def list_audio_assets_action(result, directory="/Game", recursive=True, max_results=200,
        offset=0, checkpoint_callback=None):
    arguments = _list_arguments(directory, recursive, max_results, offset)
    _context(result, LIST, arguments)
    result["stage"] = "asset_registry_metadata_read"
    result["data"].update(engine_api_dispatched=True, registry_read_returned=False, read_outcome_unknown=True)
    try:
        payload = api.list_audio_assets(**arguments)
    except Exception as error:
        prefix = getattr(error, "observation_prefix", None)
        if type(prefix) is dict:
            result["data"].update(copy.deepcopy(prefix))
            result["data"].update(engine_api_dispatched=prefix["engine_api_dispatch_attempted"],
                registry_read_returned=prefix["registry_asset_query_returned"],
                read_outcome_unknown=prefix["registry_asset_query_dispatched"] and not prefix["inventory_complete"])
        # Missing query or invalid syntax can follow actual normalization and
        # Registry getter calls. Preserve their returned prefix, never reset
        # all Engine dispatch to False merely from the exception's code.
        if checkpoint_callback:
            checkpoint_callback(result, "returned_audio_metadata_failure")
        raise
    result["data"].update(copy.deepcopy(payload))
    _context(result, LIST, arguments)
    result["data"].update(registry_read_returned=True, read_outcome_unknown=False)
    if checkpoint_callback:
        checkpoint_callback(result, "returned_audio_metadata_read")
    result["validation"]["status"] = "current_registry_audio_metadata"
    result.update(success=True, stage="complete")


def inspect_audio_asset(asset_path):
    try:
        arguments = {"asset_path": _text(asset_path, "asset_path")}
    except Exception as error:
        return _rejected(INSPECT, error)
    result = state.new_result()
    try:
        _native_action(result, INSPECT, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def inspect_audio_asset_action(result, asset_path, checkpoint_callback=None):
    # Read actions use the existing two-argument returned-observation pact;
    # adapt it explicitly to the native action's mutation-style checkpoint.
    callback = None if checkpoint_callback is None else lambda observed: checkpoint_callback(observed, "returned_audio_inspection")
    _native_action(result, INSPECT, {"asset_path": _text(asset_path, "asset_path")}, callback)


def create_sound_cue_from_wave(name, sound_wave_path, package_path="/Game/Audio/SoundCues"):
    try:
        arguments = _cue_arguments(name, sound_wave_path, package_path)
    except Exception as error:
        return _rejected(CREATE_CUE, error)
    # Original public three inputs stay exact. One generated internal operation
    # ID owns the durable writer journal; no public operation_id was added.
    return state.mutate(CREATE_CUE, arguments, None,
        lambda result: create_sound_cue_from_wave_action(result, **arguments))


def create_sound_cue_from_wave_action(result, name, sound_wave_path,
        package_path="/Game/Audio/SoundCues", checkpoint_callback=None):
    arguments = _cue_arguments(name, sound_wave_path, package_path)
    _context(result, CREATE_CUE, arguments)
    result["data"]["pending_mutation"] = {"operation": CREATE_CUE, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_target_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    _native_action(result, CREATE_CUE, arguments, callback)


def create_basic_metasound_source(name, package_path="/Game/Audio/MetaSounds",
        oscillator="sine", frequency=440.0, output_format="mono"):
    try:
        arguments = _metasound_arguments(name, package_path, oscillator, frequency, output_format)
    except Exception as error:
        return _rejected(CREATE_META, error)
    return state.mutate(CREATE_META, arguments, None,
        lambda result: create_basic_metasound_source_action(result, **arguments))


def create_basic_metasound_source_action(result, name, package_path="/Game/Audio/MetaSounds",
        oscillator="sine", frequency=440.0, output_format="mono", checkpoint_callback=None):
    arguments = _metasound_arguments(name, package_path, oscillator, frequency, output_format)
    _context(result, CREATE_META, arguments)
    result["data"]["pending_mutation"] = {"operation": CREATE_META, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_builder_target_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    _native_action(result, CREATE_META, arguments, callback)
