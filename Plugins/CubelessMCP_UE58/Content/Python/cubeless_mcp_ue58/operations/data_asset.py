"""Original DataAsset three-name family with native save and one writer journal.

Native admission owns the existing specialized/generated and bounded generic
native class/value scopes; this module does not select a class whitelist.
Original matching/empty and partial failed-property work, notification and
requested-save receipts are retained; no Python second save or retry.
"""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import data_asset as api
from . import datatable, state
from .state import OperationError

GET = "get_data_asset"
SET = "set_data_asset_properties"
CREATE = "create_data_asset"
WRITES = (SET, CREATE)


def _shared(validator, *arguments):
    try:
        return validator(*arguments)
    except OperationError as error:
        code = error.code.replace("datatable_row_values", "data_asset_properties").replace("datatable_", "data_asset_")
        raise OperationError(code, str(error).replace("row_values", "properties")) from error


def _text(value, field):
    return _shared(datatable._text, value, field)


def _flag(value, field):
    return _shared(datatable._flag, value, field)


def _get_arguments(asset_path):
    # Native owns original finite path normalization/resolution and load receipts.
    return {"asset_path": _text(asset_path, "asset_path")}


def _properties_json(properties):
    # Reuse finite JSON bounds and original recursive MCP-value rejection.
    # NULs, nulls, unknown keys and other literal strings retain native
    # per-property failed-row semantics, including partial write/save prefixes.
    if type(properties) is not dict:
        raise OperationError("data_asset_properties_invalid", "properties requires a JSON object, not JSON text")
    remaining = [datatable._MAX_JSON_VALUES]
    def visit(value, depth, reference_value=True):
        remaining[0] -= 1
        if remaining[0] < 0 or depth > datatable._MAX_JSON_DEPTH:
            raise OperationError("data_asset_properties_invalid", "properties exceeds finite JSON depth/value bounds")
        if value is None or type(value) in (bool, int):
            return
        if type(value) is float:
            if not math.isfinite(value):
                raise OperationError("data_asset_properties_invalid", "properties requires finite JSON numbers")
        elif type(value) is str:
            if reference_value:
                _shared(datatable._reference, value, "properties")
            try:
                value.encode("utf-16-le")
            except UnicodeEncodeError as error:
                raise OperationError("data_asset_properties_invalid", "properties contains an invalid surrogate") from error
        elif type(value) is list:
            for child in value:
                visit(child, depth + 1)
        elif type(value) is dict:
            for key, child in value.items():
                if type(key) is not str:
                    raise OperationError("data_asset_properties_invalid", "properties requires string JSON keys")
                visit(key, depth + 1, reference_value=False)
                visit(child, depth + 1)
        else:
            raise OperationError("data_asset_properties_invalid", "properties contains an unsupported non-JSON type")
    visit(properties, 0)
    try:
        return json.dumps(properties, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, OverflowError, RecursionError) as error:
        raise OperationError("data_asset_properties_invalid", "properties cannot be encoded as finite JSON") from error


def _set_arguments(asset_path, properties, save):
    encoded = _properties_json(properties)
    if len(encoded.encode("utf-16-le")) // 2 > 65536 or len(properties) > 32:
        raise OperationError("data_asset_properties_json_invalid", "properties exceeds the finite native 65536 UTF16/32-field input bounds; no dispatch")
    return {**_get_arguments(asset_path), "properties_json": encoded, "save": _flag(save, "save")}


def _create_arguments(asset_path, class_path, save):
    args = {**_get_arguments(asset_path), "class_path": _text(class_path, "class_path"), "save": _flag(save, "save")}
    _shared(datatable._reference, args["asset_path"], "asset_path")
    _shared(datatable._reference, args["class_path"], "class_path")
    return args


def _context(result, identity, args):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(args),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_legacy_data_asset_parity=False, cold_persistence_verified=False,
        runtime_deformer_payload_verified=False, library_cdo_before_native_entry_independently_observed=False,
        actual_work_counts_authority="returned_native_counters_and_ordered_load_save_receipts")


def _apply_native(result, identity, args, native):
    data = result["data"]
    data.update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    data.update(native_result=copy.deepcopy(native), native_raw_result=native["_raw_native_json"],
        native_outcome={k: copy.deepcopy(v) for k,v in native.items() if k not in ("data", "_raw_native_json")},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None)
    # Native legacy_mutation_contract/legacy_mutation_performed/verification
    # remain opaque source-derived fields; semantic changed is journal scope.
    _context(result, identity, args)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"],
        status="not_requested" if not native["compile_requested"] else "unexpected_native_compile")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"],
        status="native_returned_saved" if native["saved"] else "native_returned_failed" if native["save_attempted"] else "native_not_attempted")
    no_writer = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_writer else [{"kind":identity, "asset_path":native["data"].get("data_asset", args["asset_path"]),
        "attempted":native["mutation_attempted"], "change_state_known":known, "changed":native.get("changed") if known else None,
        "outcome":"native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_data_asset_postcondition" if native["postcondition_passed"] else "native_data_asset_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", initial_load_effects="not_fully_instrumented")
    result["stage"] = "returned_data_asset_operation"


def _native_action(result, identity, args, callback=None):
    _context(result, identity, args)
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True, mutation_performed=None)
    result["stage"] = "native_data_asset_operation"
    result["compile"].update(requested=False, attempted=None, compiled=None, status="unknown_until_native_returns")
    result["save"].update(requested=args.get("save", False), attempted=None, saved=None, status="unknown_until_native_returns")
    result["changes"] = [{"kind":identity, "attempted":None, "changed":None, "change_state_known":False, "outcome":"unknown_until_native_response"}]
    try:
        native = getattr(api, identity)(**args)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_data_asset_operation"
            if callback: callback(result)
        if getattr(error, "code", "") == "native_data_asset_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
            result["compile"].update(attempted=False, status="native_not_dispatched")
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        raise
    _apply_native(result, identity, args, native)
    if callback: callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_data_asset_failed", native.get("error") or "DataAsset failed; actual prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("data_asset_result_unverified", "Native success lacks known complete postconditions; no replay")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None:
        raise OperationError("data_asset_unexpected_compile", "Unexpected actual compile prefix retained")
    if identity == GET and (native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("data_asset_read_unexpected_save", "Actual read save prefix retained; load effects are separate")
    result.update(success=True, stage="complete")
    if identity in WRITES: result["retry"]["duplicate_risk"] = "inspect_data_asset_and_journal_no_writer_replay"


def _read_action(result, args, checkpoint_callback=None):
    callback = None if checkpoint_callback is None else lambda observed: checkpoint_callback(observed, "returned_data_asset_read")
    _native_action(result, GET, args, callback)


def _write_action(result, identity, args, checkpoint_callback=None):
    _context(result, identity, args)
    result["save"].update(requested=args["save"], attempted=False, saved=False, status="not_dispatched")
    result["data"]["pending_mutation"] = {"operation":identity, **args, "dispatch_state_at_checkpoint":"not_yet_dispatched"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    _native_action(result, identity, args, callback)


def _rejected(identity, error):
    result = state.fail(state.new_result(), error)
    _context(result, identity, {})
    result["data"].update(admission_rejected=True, native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
    return result


def get_data_asset(asset_path):
    try: args = _get_arguments(asset_path)
    except Exception as error: return _rejected(GET, error)
    result = state.new_result()
    try: _read_action(result, args)
    except Exception as error: state.fail(result, error)
    return result


def get_data_asset_action(result, asset_path, checkpoint_callback=None):
    _read_action(result, _get_arguments(asset_path), checkpoint_callback)


def set_data_asset_properties(asset_path, properties, save=True):
    try: args = _set_arguments(asset_path, properties, save)
    except Exception as error: return _rejected(SET, error)
    return state.mutate(SET, args, None, lambda result: _write_action(result, SET, args))


def set_data_asset_properties_action(result, asset_path, properties, save=True, checkpoint_callback=None):
    _write_action(result, SET, _set_arguments(asset_path, properties, save), checkpoint_callback)


def create_data_asset(asset_path, class_path, save=True):
    try: args = _create_arguments(asset_path, class_path, save)
    except Exception as error: return _rejected(CREATE, error)
    return state.mutate(CREATE, args, None, lambda result: _write_action(result, CREATE, args))


def create_data_asset_action(result, asset_path, class_path, save=True, checkpoint_callback=None):
    _write_action(result, CREATE, _create_arguments(asset_path, class_path, save), checkpoint_callback)
