"""Four original Sequencer identities with one durable mutation journal.

The typed domain keeps keys as a list of dictionaries. Only its reflected
representation is JSON text. Engine-specific logic lives in compatibility.
"""

import copy
import json
import math
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import sequencer as api
from . import material, state, variables
from .state import OperationError


CREATE = "create_level_sequence"
INFO = "get_level_sequence_info"
BIND = "add_level_sequence_actor_binding"
KEYS = "add_level_sequence_transform_keys"


def _text(value, field, optional=False):
    if optional and value is None:
        return ""
    if optional and value == "":
        return ""
    return variables._text(value, field)


def _path(value):
    value = _text(value, "sequence_path")
    package, dot, name = value.partition(".")
    if dot and name != package.rsplit("/", 1)[-1]:
        raise OperationError("sequencer_path_invalid", "Object suffix must match the package asset name")
    return api.package_path(package)


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("sequencer_input_invalid", field + " requires an exact boolean")
    return value


def _number(value, field):
    if type(value) not in (int, float) or not math.isfinite(value):
        raise OperationError("sequencer_input_invalid", field + " requires a finite number, excluding bool")
    return float(value)


def _create_arguments(sequence_path, frame_rate, playback_start_frame, playback_end_frame, save):
    rate = _number(frame_rate, "frame_rate")
    integral = math.floor(rate + 0.5)
    if not 0 < rate <= 480 or abs(rate - integral) > 1e-8 or integral == 0:
        raise OperationError("sequencer_frame_rate_invalid", "frame_rate must be an integral rate in (0,480]; 29.97 is unsupported before allocation")
    start = _number(playback_start_frame, "playback_start_frame")
    end = _number(playback_end_frame, "playback_end_frame")
    if not -(2**31) <= start < end <= 2**31 - 1:
        raise OperationError("sequencer_playback_range_invalid", "Playback bounds require signed-int32 display frames with end > start")
    return {"sequence_path": _path(sequence_path), "frame_rate": rate,
        "playback_start_frame": start, "playback_end_frame": end, "save": _bool(save, "save")}


def _key_rows(value):
    try:
        raw = value if type(value) is str else json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        material._json_input(raw)
        rows = json.loads(raw)
    except (OperationError, TypeError, ValueError, RecursionError) as error:
        raise OperationError("sequencer_invalid_keys", "keys requires a bounded JSON array: " + str(error)) from error
    if type(rows) is not list or not 1 <= len(rows) <= 512:
        raise OperationError("sequencer_invalid_keys", "keys must contain 1..512 row objects")
    result = []
    for index, row in enumerate(rows):
        if type(row) is not dict or "frame" not in row:
            raise OperationError("sequencer_invalid_keys", "Each key requires a numeric frame")
        frame = _number(row["frame"], "keys[" + str(index) + "].frame")
        if not -(2**31) <= math.floor(frame) <= 2**31 - 1:
            raise OperationError("sequencer_invalid_keys", "Display frame floor must fit signed int32")
        clean = {"frame": frame}
        for field in ("location", "rotation", "scale"):
            if field not in row:
                continue
            if type(row[field]) is not list or len(row[field]) != 3:
                raise OperationError("sequencer_invalid_keys", field + " requires three finite numbers when present")
            clean[field] = [_number(item, field) for item in row[field]]
        result.append(clean)
    return result


def _key_arguments(sequence_path, keys, binding_id, actor_label, save):
    guid = _text(binding_id, "binding_id", True)
    if guid:
        try:
            guid = str(uuid.UUID(guid))
        except ValueError as error:
            raise OperationError("sequencer_binding_id_invalid", "binding_id must be a valid GUID; an invalid supplied GUID does not fall back to actor_label") from error
        if uuid.UUID(guid).int == 0:
            raise OperationError("sequencer_binding_id_invalid", "Zero binding GUID is invalid")
    label = _text(actor_label, "actor_label", True)
    if not guid and not label:
        raise OperationError("sequencer_binding_required", "Supply binding_id or actor_label")
    return {"sequence_path": _path(sequence_path), "keys": _key_rows(keys),
        "binding_id": guid, "actor_label": label, "save": _bool(save, "save")}


def _bind_arguments(sequence_path, actor_label, save):
    return {"sequence_path": _path(sequence_path), "actor_label": _text(actor_label, "actor_label"), "save": _bool(save, "save")}


def _rejected(error, operation_id=None):
    result = state.fail(state.new_result(operation_id), error)
    result["data"].update(mutation_attempted=False, mutation_state_unknown=False,
        mutation_performed=False, engine_calls_dispatched=0, admission_rejected=True,
        full_legacy_sequencer_parity=False, full_public_input_representation_parity=False)
    return result


def _returned_save_status(output, *, outcome_unverified=False):
    # A recorded True save result is retained even if a later observer fails.
    # Default False flags cannot certify no save on an unobserved exception.
    if output.get("saved") is True:
        return "domain_returned_saved"
    if outcome_unverified or type(output.get("save_attempted")) is not bool:
        return "domain_exception_save_outcome_unverified"
    return "domain_returned_attempted_not_saved" if output["save_attempted"] else "domain_returned_not_attempted"


def _adopt_sequence_resolution(result, prefix):
    if type(prefix) is not dict or not prefix:
        return
    result["data"].update(sequence_resolution=copy.deepcopy(prefix),
        adapter_explicit_asset_load_calls=prefix.get("asset_load_count"),
        asset_load_invoked=prefix.get("asset_load_invoked"), loader_callback_effects_fully_measured=False)
    if prefix.get("context_known") is False:
        result["data"].update(mutation_state_unknown=True, mutation_performed=None,
            unsafe_loader_observation=True, automatic_action_retry=False)


def _run_action(result, identity, arguments, checkpoint_callback=None):
    result["data"].update(operation=identity, mutation_attempted=False,
        mutation_state_unknown=False, mutation_performed=False, engine_calls_dispatched=0,
        completed_engine_calls=0, full_legacy_sequencer_parity=False,
        full_public_input_representation_parity=identity != KEYS,
        renderer_verified=False, playback_execution_verified=False, disk_reload_verified=False)
    try:
        context = api.prepare(identity, arguments)
    except Exception as error:
        _adopt_sequence_resolution(result, getattr(error, "sequence_resolution", None))
        raise
    result["data"].update(copy.deepcopy(context["public"]))
    _adopt_sequence_resolution(result, context["public"].get("sequence_resolution"))
    result["save"]["requested"] = arguments["save"]

    def notify(stage, intent, observation=None, native=None):
        if native is not None:
            result["data"].setdefault("native_outcomes", []).append(copy.deepcopy(native))
            if "after" in native["data"]:
                result["data"]["native_after"] = copy.deepcopy(native["data"]["after"])
            result["data"]["completed_engine_calls"] += 1
            result["data"]["mutation_state_unknown"] = not native["change_state_known"]
            result["changes"][-1].update(attempted=native["mutation_attempted"],
                changed=native.get("changed"), change_state_known=native["change_state_known"],
                postcondition_passed=native["postcondition_passed"], native_operation=native.get("operation"))
            result["data"]["mutation_attempted"] = any(row["attempted"] for row in result["changes"])
            result["data"]["mutation_performed"] = any(row.get("changed") is True for row in result["changes"])
            result["data"].update(copy.deepcopy(context.get("output", {})))
            result["stage"] = "returned_" + stage
            (checkpoint_callback or state.checkpoint)(result)
        elif observation is None:
            result["stage"] = "before_" + stage
            result["data"]["pending_mutation"] = {"stage": stage, **intent,
                "dispatch_state_at_checkpoint": "not_yet_dispatched",
                "interrupted_outcome": "unknown_inspect_sequence_and_journal"}
            (checkpoint_callback or state.checkpoint)(result)
            result["stage"] = stage
            result["data"].update(mutation_attempted=True, mutation_state_unknown=True)
            result["data"]["engine_calls_dispatched"] += 1
            if stage == "save_exact_asset":
                result["save"]["attempted"] = True
            result["changes"].append({"kind": identity, "stage": stage,
                "asset_path": arguments["sequence_path"], "attempted": True,
                "changed": None, "change_state_known": False})
        else:
            result["data"]["after"] = copy.deepcopy(observation)
            result["data"]["completed_engine_calls"] += 1
            result["data"]["mutation_state_unknown"] = False
            changed = context["public"]["before"] != observation
            result["data"]["mutation_performed"] = result["data"]["mutation_performed"] or changed
            result["changes"][-1].update(changed=changed, change_state_known=True)

    try:
        output = api.execute(identity, context, arguments, notify)
    except Exception as error:
        # Returned prefixes/counts must remain visible even when a later row,
        # postcondition or exact save fails. No child or native action is retried.
        result["data"].update(copy.deepcopy(context.get("output", {})))
        partial = context.get("output", {})
        result["save"].update(requested=partial.get("save_requested", arguments["save"]),
            attempted=partial.get("save_attempted", result["save"]["attempted"]), saved=partial.get("saved", False))
        if getattr(error, "code", "") in ("sequencer_exact_save_failed", "sequencer_saved_file_unverified"):
            result["data"]["mutation_state_unknown"] = True
        result["save"]["status"] = _returned_save_status(partial,
            outcome_unverified=result["data"].get("mutation_state_unknown") is not False)
        raise
    result["data"].update(output)
    result["save"].update(requested=output["save_requested"], attempted=output["save_attempted"], saved=output["saved"])
    result["save"]["status"] = _returned_save_status(output)
    result["validation"].update(status="public_sequence_payload_observed", postcondition_verified=True,
        disk_reload="not_run", playback_execution="not_run", renderer="not_run")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "sequence_mutation_would_repeat" if result["data"]["mutation_attempted"] else "none_observed"


def create_level_sequence(sequence_path, frame_rate=30.0, playback_start_frame=0,
        playback_end_frame=150, save=True, operation_id=None):
    try:
        arguments = _create_arguments(sequence_path, frame_rate, playback_start_frame, playback_end_frame, save)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(CREATE, arguments, operation_id, lambda result: _run_action(result, CREATE, arguments))


def create_level_sequence_action(result, sequence_path, frame_rate=30.0,
        playback_start_frame=0, playback_end_frame=150, save=True, checkpoint_callback=None):
    _run_action(result, CREATE, _create_arguments(sequence_path, frame_rate,
        playback_start_frame, playback_end_frame, save), checkpoint_callback)


def get_level_sequence_info_action(result, sequence_path, checkpoint_callback=None):
    # No explicit sequence writer; cold load may invoke Engine callbacks.
    result["data"].update(read_only_asset_observation=None, mutation_state_unknown=False,
        mutation_attempted=False, mutation_performed=False,
        full_legacy_sequencer_parity=False, playback_execution_verified=False)
    try:
        observed = api.get_level_sequence_info(_path(sequence_path))
    except Exception as error:
        _adopt_sequence_resolution(result, getattr(error, "sequence_resolution", None))
        raise
    result["data"].update(observed)
    _adopt_sequence_resolution(result, observed.get("sequence_resolution"))
    result.update(success=True, stage="complete")
    result["validation"]["status"] = "public_loaded_sequence_observed"


def get_level_sequence_info(sequence_path):
    result = state.new_result()
    try:
        get_level_sequence_info_action(result, sequence_path)
    except Exception as error:
        state.fail(result, error)
    return result


def add_level_sequence_actor_binding(sequence_path, actor_label, save=True, operation_id=None):
    try:
        arguments = _bind_arguments(sequence_path, actor_label, save)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(BIND, arguments, operation_id, lambda result: _run_action(result, BIND, arguments))


def add_level_sequence_actor_binding_action(result, sequence_path, actor_label, save=True, checkpoint_callback=None):
    _run_action(result, BIND, _bind_arguments(sequence_path, actor_label, save), checkpoint_callback)


def add_level_sequence_transform_keys(sequence_path, keys, binding_id="", actor_label="", save=True, operation_id=None):
    try:
        arguments = _key_arguments(sequence_path, keys, binding_id, actor_label, save)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(KEYS, arguments, operation_id, lambda result: _run_action(result, KEYS, arguments))


def add_level_sequence_transform_keys_action(result, sequence_path, keys,
        binding_id="", actor_label="", save=True, checkpoint_callback=None):
    _run_action(result, KEYS, _key_arguments(sequence_path, keys, binding_id, actor_label, save), checkpoint_callback)
