"""In-place enum edits with the existing durable operation journal."""
import copy
import json
from . import state
from .state import OperationError
from ..compatibility.ue58 import user_defined_enum_editing as api


def _adopt(result, native):
    data = result["data"]
    data.update(copy.deepcopy(native.get("data", {})), native_raw_result=native["_raw_native_json"],
                native_result_returned=True, native_outcome={k: copy.deepcopy(v) for k, v in native.items()
                if k not in ("data", "_raw_native_json")}, automatic_retry=False,
                mutation_state_unknown=not native.get("change_state_known", False))
    result.update(success=native["success"], stage=native.get("stage", "returned_enum_edit"),
                  error_code=native.get("error_code", ""), error=native.get("error", ""))
    attempted = native.get("mutation_attempted", False)
    known = native.get("change_state_known", False)
    result["changes"] = [] if not attempted else [dict(kind="edit_user_defined_enum", attempted=attempted,
                       change_state_known=known, changed=native.get("changed") if known else None)]
    result["compile"].update(requested=native.get("compile_requested", False),
        attempted=native.get("compile_attempted", False), compiled=native.get("compiled"),
        status="native_explicit_receipt_only", dependent_Blueprint_compile_verified=False)
    result["save"].update(requested=native.get("save_requested", False),
        attempted=native.get("save_attempted", False), saved=native.get("saved", False),
        scope="target_enum_only; dependent packages are never saved by this tool")
    result["validation"].update(native_postcondition_passed=native.get("postcondition_passed", False),
        status="native_enum_readback", dependent_Blueprint_compile_verified=False)
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="inspect_actual_enum_and_dependent_packages_before_any_new_mutation" if attempted else "none_observed")


def inspect_user_defined_enum(asset_path):
    result = state.new_result()
    try:
        if type(asset_path) is not str:
            raise OperationError("enum_edit_input_invalid", "asset_path must be a string")
        native = api.invoke("inspect_user_defined_enum", asset_path)
        _adopt(result, native)
    except Exception as error:
        state.fail(result, error)
    return result


def edit_user_defined_enum(asset_path, edits_json, dry_run, save, allow_remove, operation_id=""):
    try:
        if type(asset_path) is not str or type(edits_json) is not str or len(edits_json) > 1048576:
            raise OperationError("enum_edit_input_invalid", "Asset path and bounded edits_json strings required")
        if any(type(flag) is not bool for flag in (dry_run, save, allow_remove)):
            raise OperationError("enum_edit_input_invalid", "dry_run/save/allow_remove must be explicit booleans")
        edits = json.loads(edits_json, parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
        if type(edits) is not list or len(edits) > 64 or any(type(row) is not dict for row in edits):
            raise OperationError("enum_edit_input_invalid", "edits_json must encode at most64 operation objects")
        if type(operation_id) is not str:
            raise OperationError("enum_edit_input_invalid", "operation_id must be a string")
        arguments = dict(asset_path=asset_path, edits=edits, dry_run=dry_run, save=save, allow_remove=allow_remove)
    except Exception as error:
        return state.fail(state.new_result(), error)

    def action(result):
        result["data"].update(requested_arguments=copy.deepcopy(arguments), native_dispatched=False)
        result["stage"] = "before_enum_edit_dispatch"
        state.checkpoint(result)
        result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
        result["changes"] = [dict(kind="edit_user_defined_enum", attempted=None, changed=None, change_state_known=False)]
        result["stage"] = "native_enum_edit_dispatch"
        state.checkpoint(result)
        try:
            native = api.invoke("edit_user_defined_enum", asset_path,
                json.dumps(edits, ensure_ascii=False, allow_nan=False), dry_run, save, allow_remove)
        except Exception as error:
            if getattr(error, "native_call_returned", False):
                result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            state.checkpoint(result)
            raise
        _adopt(result, native)
        state.checkpoint(result)
        if not native["success"]:
            raise OperationError(native.get("error_code") or "enum_edit_failed", native.get("error") or "Native enum edit failed")
        return result
    return state.mutate("edit_user_defined_enum", arguments, operation_id or None, action)
