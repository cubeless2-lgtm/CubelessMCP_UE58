"""Loaded LevelInstance packing contract on the existing durable mutation journal."""
import copy
from . import state
from .state import OperationError
from ..compatibility.ue58 import loaded_level_instance_packing as api


def _arguments(source_actor_path, target_actor_path, dry_run, allow_replace, save):
    if any(type(p) is not str or not p.startswith("/") or len(p) > 4096 or "\0" in p
           for p in (source_actor_path, target_actor_path)):
        raise OperationError("packing_input", "Exact bounded actor object paths are required")
    if any(type(flag) is not bool for flag in (dry_run, allow_replace, save)):
        raise OperationError("packing_input", "dry_run, allow_replace and save must be booleans")
    return dict(source_actor_path=source_actor_path, target_actor_path=target_actor_path,
                dry_run=dry_run, allow_replace=allow_replace, save=save)


def _adopt(result, native):
    attempted = native.get("mutation_attempted", False)
    known = native.get("change_state_known", False)
    result.update(success=native["success"], stage=native.get("stage", "native_returned"),
                  error_code=native.get("error_code", ""), error=native.get("error", ""))
    result["data"].update(copy.deepcopy(native.get("data", {})),
        native_raw_result=native["_raw_native_json"], native_result_returned=True,
        native_outcome={k: copy.deepcopy(v) for k, v in native.items() if k not in ("data", "_raw_native_json")},
        mutation_state_unknown=not known, original_deletion_authorized=False)
    result["changes"] = [] if not attempted else [dict(kind="pack_loaded_level_instance",
        attempted=True, change_state_known=known, changed=native.get("changed") if known else None)]
    result["compile"].update(requested=False, attempted=False, compiled=None,
        status="not_required_native_PLA_no_Blueprint_publication")
    result["save"].update(requested=native.get("save_requested", False),
        attempted=native.get("save_attempted", False), saved=native.get("saved", False),
        scope="exclusive_target_external_actor_package_only_no_map_or_source_save")
    result["validation"].update(status="native_packing_readback",
        native_postcondition_passed=native.get("postcondition_verified", False),
        source_safe_to_delete=False)
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="inspect_actual_target_and_source_before_any_new_mutation"
        if attempted or not known else "none_observed")


def inspect_loaded_level_instance_packing(source_actor_path, target_actor_path):
    result = state.new_result()
    try:
        arguments = _arguments(source_actor_path, target_actor_path, True, False, False)
        _adopt(result, api.invoke(arguments))
    except Exception as error:
        state.fail(result, error)
    return result


def pack_loaded_level_instance(source_actor_path, target_actor_path, operation_id,
                               dry_run=True, allow_replace=False, save=False):
    try:
        arguments = _arguments(source_actor_path, target_actor_path, dry_run, allow_replace, save)
        if type(operation_id) is not str or not state._ID_PATTERN.fullmatch(operation_id):
            raise OperationError("invalid_operation_id", "A fresh 1-96 character operation_id is required")
    except Exception as error:
        return state.fail(state.new_result(), error)

    def action(result):
        result["data"].update(requested_arguments=copy.deepcopy(arguments), native_dispatched=False)
        result["stage"] = "before_packing_dispatch"
        state.checkpoint(result)
        result["stage"] = "native_packing_dispatch"
        result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
        result["changes"] = [dict(kind="pack_loaded_level_instance", attempted=None,
                                  changed=None, change_state_known=False)]
        state.checkpoint(result)
        try:
            native = api.invoke(arguments)
        except Exception as error:
            if getattr(error, "native_call_dispatched", None) is False:
                result["changes"] = []
                result["data"].update(native_dispatched=False, mutation_state_unknown=False)
            if getattr(error, "native_call_returned", False):
                result["data"].update(native_result_returned=True,
                    native_raw_result=getattr(error, "native_raw_result", None))
            state.checkpoint(result)
            raise
        _adopt(result, native)
        state.checkpoint(result)
        return result
    return state.mutate("pack_loaded_level_instance", arguments, operation_id, action)
