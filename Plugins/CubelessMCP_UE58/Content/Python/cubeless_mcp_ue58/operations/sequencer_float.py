"""Original public Float6, agreed hidden5 insertion and one existing journal.

Native source/producer fields are pending final freeze. No current binding,
array packing, actor-property evaluation, cold or full legacy proof is claimed.
"""
import copy
import json
import math
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import sequencer_float as api
from . import material, sequencer as shared, state
from .state import OperationError


IDENTITY = "add_level_sequence_float_property_keys"
_FLOAT32_MAX = 3.4028234663852886e38


def _optional_text(value, field):
    if type(value) is not str or "\x00" in value:
        raise OperationError("sequencer_float_input_invalid", field + " requires original non-null text")
    return shared._text(value, field, True)


def _rows(value):
    if type(value) is not list or not 1 <= len(value) <= 512:
        raise OperationError("sequencer_invalid_keys", "keys must contain1..512 row objects, not public JSON text")
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        material._json_input(encoded)
        rows = json.loads(encoded)
    except (OperationError, TypeError, ValueError, OverflowError, RecursionError) as error:
        raise OperationError("sequencer_invalid_keys", "keys require bounded finite JSON row objects") from error
    for index, row in enumerate(rows):
        if type(row) is not dict or "frame" not in row or "value" not in row:
            raise OperationError("sequencer_invalid_keys", "Each Float key requires numeric frame/value")
        try:
            row["frame"] = shared._number(row["frame"], "keys[" + str(index) + "].frame")
            row["value"] = shared._number(row["value"], "keys[" + str(index) + "].value")
        except OverflowError as error:
            raise OperationError("sequencer_invalid_keys", "Float key number exceeds finite double bounds") from error
        if not -(2**31) <= math.floor(row["frame"]) <= 2**31 - 1 or abs(row["value"]) > _FLOAT32_MAX:
            raise OperationError("sequencer_invalid_keys", "Display floor must fit int32 and value finite native float32")
        # Unknown JSON fields remain in the raw request. Only native owns the
        # original ignored-field and float32/cubic/floor insertion semantics.
    try:
        material._json_input(json.dumps(rows, ensure_ascii=False, allow_nan=False, separators=(",", ":")))
    except (OperationError, TypeError, ValueError, OverflowError, RecursionError) as error:
        raise OperationError("sequencer_invalid_keys", "Normalized Float key JSON exceeds finite bounds") from error
    return rows


def _arguments(sequence_path, property_path, keys, binding_id, actor_label, save):
    guid = _optional_text(binding_id, "binding_id")
    label = _optional_text(actor_label, "actor_label")
    if guid:
        try:
            guid = str(uuid.UUID(guid))
        except ValueError as error:
            raise OperationError("sequencer_binding_id_invalid", "Invalid preferred GUID does not fall back to actor_label") from error
        if uuid.UUID(guid).int == 0:
            raise OperationError("sequencer_binding_id_invalid", "Zero binding GUID is invalid")
    if not guid and not label:
        raise OperationError("sequencer_binding_required", "Supply binding_id or actor_label")
    return {"sequence_path": shared._path(sequence_path), "property_path": shared._text(property_path, "property_path"),
        "keys": _rows(keys), "binding_id": guid, "actor_label": label, "save": shared._bool(save, "save")}


def _context(result, arguments):
    result["data"].update(operation=IDENTITY, requested_arguments=copy.deepcopy(arguments),
        original_save_requested=arguments.get("save", True), automatic_action_retry=False,
        adapter_explicit_compile_calls=0,
        full_legacy_sequencer_parity=False, full_original_late_invalid_row_prefix_semantics_preserved=False,
        public_input_representation="original JSON array<object> via stock reflected wrappers",
        actual_call_packing_verified=False, actor_property_evaluation_verified=False,
        cold_persistence_verified=False, playback_execution_verified=False, renderer_verified=False,
        library_cdo_before_native_entry_independently_observed=False)
    result["data"].setdefault("adapter_explicit_asset_load_calls", 0)
    result["data"].setdefault("domain_exact_save_attempt_count", 0)
    result["data"].setdefault("domain_exact_save_calls", [])


def _apply_native(result, arguments, native, raw):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native), native_raw_result=raw,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_performed=native.get("changed") if known else None, mutation_state_unknown=not known,
        native_postcondition_scope="returned native insertion prefix before domain exact save",
        final_public_save_postcondition_verified=False)
    _context(result, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
    # Public6 requests save; hidden5 explicitly does not. Keep both intents,
    # and retain unexpected actual native save flags before policy rejection.
    result["save"].update(requested=arguments["save"] or native["save_requested"],
        attempted=native["save_attempted"], saved=native["saved"])
    result["data"].update(save_requested=arguments["save"], save_attempted=native["save_attempted"], saved=native["saved"])
    no_writer = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_writer else [{"kind": IDENTITY, "asset_path": arguments["sequence_path"],
        "stage": "native_float_insertion", "attempted": native["mutation_attempted"],
        "change_state_known": known, "changed": native.get("changed") if known else None}]
    result["stage"] = "returned_sequencer_float_operation"


def _checkpoint_returned(result, callback):
    # This remains a returned observation even when it also records a pending
    # exact save. Future batch glue must retain the already returned native
    # row, rather than rewriting previous changes into dispatch intent.
    result["stage"] = "returned_sequencer_float_operation"
    callback(result)


def _run_action(result, arguments, checkpoint_callback=None):
    _context(result, arguments)
    result["data"].update(native_dispatched=False, native_result_returned=False,
        native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
    result["save"]["requested"] = arguments["save"]
    callback = checkpoint_callback or state.checkpoint
    try:
        context = api.prepare(arguments["sequence_path"])
    except Exception as error:
        prefix = getattr(error, "observation_prefix", None)
        if type(prefix) is dict:
            result["data"]["preparation_observation"] = copy.deepcopy(prefix)
            shared._adopt_sequence_resolution(result, prefix.get("sequence_resolution"))
        raise
    result["data"].update(before=copy.deepcopy(context["before"]),
        preparation_observation=copy.deepcopy(context["preparation_observation"]))
    shared._adopt_sequence_resolution(result, context["preparation_observation"].get("sequence_resolution"))
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_sequence_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback(result)
    result["data"].update(native_dispatched=True, native_mutation_attempted=None,
        mutation_performed=None, mutation_state_unknown=True)
    result["stage"] = "native_float_insertion"
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["sequence_path"],
        "attempted": None, "change_state_known": False, "changed": None}]
    try:
        native, raw = api.add_level_sequence_float_property_keys(context["asset"].get_path_name(),
            arguments["property_path"], json.dumps(arguments["keys"], ensure_ascii=False, allow_nan=False, separators=(",", ":")),
            arguments["binding_id"], arguments["actor_label"])
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            _checkpoint_returned(result, callback)
        if getattr(error, "code", "") == "sequencer_float_native_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_performed=False, mutation_state_unknown=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native, raw)
    _checkpoint_returned(result, callback)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "sequencer_float_native_failed", native.get("error") or "Native Float insertion failed; returned prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("sequencer_float_native_unverified", "Native Float insertion lacks known complete postconditions; no save or retry")
    if any(native[key] for key in ("compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")) or native["compiled"] is not None:
        raise OperationError("sequencer_float_native_policy_mismatch", "Hidden5 unexpectedly compiled/saved; raw actual receipt retained")
    if type(native["data"].get("keys_added")) not in (int, float) or native["data"]["keys_added"] != len(arguments["keys"]):
        raise OperationError("sequencer_float_keys_unverified", "Original keys_added row count differs; no save or retry")
    result["stage"] = "observe_float_after_native"
    result["data"].update(public_post_native_snapshot_dispatched=True, public_post_native_snapshot_returned=False)
    result["data"]["after"] = api.snapshot(context["asset"])
    result["data"]["public_post_native_snapshot_returned"] = True
    if arguments["save"]:
        before_save = copy.deepcopy(result["data"]["after"])
        # Preserve every native aggregate in native_result before clearing its
        # flattened prefix flags for a later save observation phase.
        for key in ("observations_complete", "source_preserved", "nonselected_state_preserved", "unselected_state_preserved", "independent_readback_passed", "postcondition_verified"):
            if key in result["data"]:
                result["data"][key] = False
        result["data"].update(pending_exact_save={"asset_path": context["asset"].get_path_name(),
            "only_if_is_dirty": False, "dispatch_state_at_checkpoint": "not_yet_dispatched"},
            final_public_save_postcondition_verified=False)
        _checkpoint_returned(result, callback)
        result["save"].update(attempted=True, saved=False)
        result["data"].update(save_attempted=True, saved=False)
        receipt = {"asset_path": context["asset"].get_path_name(), "attempted": True,
            "only_if_is_dirty": False, "returned": False, "success": None}
        result["data"].update(domain_exact_save_attempt_count=1, domain_exact_save_calls=[receipt],
            mutation_state_unknown=True)
        result["stage"] = "save_exact_asset"
        result["changes"].append({"kind": IDENTITY, "stage": "save_exact_asset", "asset_path": arguments["sequence_path"],
            "attempted": True, "change_state_known": False, "changed": None})
        saved = api.save_exact_asset(context["asset"])
        receipt.update(returned=True, success=saved)
        result["save"]["saved"] = saved is True
        result["data"]["saved"] = saved is True
        result["data"]["exact_save_returned"] = True
        _checkpoint_returned(result, callback)
        if saved is not True:
            raise OperationError("sequencer_float_exact_save_failed", "One forced save returned False; insertion/save prefix retained, no automatic retry")
        result["stage"] = "observe_float_after_save"
        result["data"].update(public_saved_snapshot_dispatched=True, public_saved_snapshot_returned=False)
        after_save = api.snapshot(context["asset"])
        result["data"]["after_save"] = after_save
        result["data"]["public_saved_snapshot_returned"] = True
        payload_before = {k: v for k, v in before_save.items() if k not in ("file", "package_dirty")}
        payload_after = {k: v for k, v in after_save.items() if k not in ("file", "package_dirty")}
        if after_save["file"]["exists"] is not True or after_save["package_dirty"] is not False or payload_before != payload_after:
            raise OperationError("sequencer_float_saved_payload_unverified", "Saved file/clean/public Float payload differs; actual successful save retained, no replay")
        result["data"].update(mutation_state_unknown=False, mutation_performed=True,
            final_public_save_postcondition_verified=True,
            whole_native_source_save_preserved_independently_observed=False)
        result["changes"][-1].update(change_state_known=True, changed=True)
        _checkpoint_returned(result, callback)
    result["validation"].update(status="public_float_payload_and_returned_native_insertion",
        native_insertion_postcondition_verified=True, postcondition_verified=True,
        exact_saved_public_payload_verified=arguments["save"], disk_reload="not_run", actor_property_evaluation="not_run")
    result.update(success=True, stage="complete")
    result["retry"]["duplicate_risk"] = "cubic_insertion_would_repeat_inspect_journal_no_replay"


def add_level_sequence_float_property_keys(sequence_path, property_path, keys, binding_id="", actor_label="", save=True):
    try:
        arguments = _arguments(sequence_path, property_path, keys, binding_id, actor_label, save)
    except Exception as error:
        result = state.fail(state.new_result(), error)
        _context(result, {})
        result["data"].update(admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
        return result
    return state.mutate(IDENTITY, arguments, None, lambda result: _run_action(result, arguments))


def add_level_sequence_float_property_keys_action(result, sequence_path, property_path, keys,
        binding_id="", actor_label="", save=True, checkpoint_callback=None):
    _run_action(result, _arguments(sequence_path, property_path, keys, binding_id, actor_label, save), checkpoint_callback)
