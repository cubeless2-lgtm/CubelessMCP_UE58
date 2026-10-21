"""Original DataTable6 inputs and shared returned-result/journal handling.

Native owns exact finite struct admission, row ordering/import/callbacks and
SavePackage. Reads may load/PostLoad. Matching Add remains an actual writer.
"""
import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import datatable as api
from . import state
from .state import OperationError


SCHEMA = "get_datatable_schema"
LIST = "list_datatable_rows"
GET = "get_datatable_row"
ADD = "add_or_update_datatable_row"
REMOVE = "remove_datatable_row"
CREATE = "create_datatable"
READS = (SCHEMA, LIST, GET)
WRITES = (ADD, REMOVE, CREATE)
_FORBIDDEN = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")
_MAX_JSON_UNITS = 1048576
_MAX_JSON_DEPTH = 32
_MAX_JSON_VALUES = 16384


def _text(value, field):
    if type(value) is not str or "\x00" in value:
        raise OperationError("datatable_input_invalid", field + " requires a string without NUL; no native dispatch ran")
    try:
        value.encode("utf-16-le")
    except UnicodeEncodeError as error:
        raise OperationError("datatable_input_invalid", field + " contains an invalid surrogate") from error
    return value


def _reference(value, field):
    if type(value) is str and any(token in value.strip().strip("\"'").replace("\\", "/").lower() for token in _FORBIDDEN):
        raise OperationError("datatable_reference_unsupported", "MCP-only dependency reference in " + field + "; no native dispatch ran")


def _flag(value, field):
    if type(value) is not bool:
        raise OperationError("datatable_input_invalid", field + " requires an exact boolean")
    return value


def _maximum(value):
    if type(value) is not int or not -(2**31) <= value <= 2**31 - 1:
        raise OperationError("datatable_input_invalid", "max_rows requires a signed-int32 integer, not a boolean or coercible value")
    # Preserve raw request; native owns original clamp1..5000.
    return value


def _row_json(values):
    if type(values) is not dict:
        raise OperationError("datatable_row_values_invalid", "row_values requires a JSON object, not a public JSON string")
    remaining = [_MAX_JSON_VALUES]
    def visit(value, depth, field):
        remaining[0] -= 1
        if remaining[0] < 0 or depth > _MAX_JSON_DEPTH:
            raise OperationError("datatable_row_values_invalid", "row_values exceeds finite depth/value bounds")
        if value is None or type(value) is bool or type(value) is int:
            return
        if type(value) is float:
            if not math.isfinite(value):
                raise OperationError("datatable_row_values_invalid", field + " requires a finite JSON number")
        elif type(value) is str:
            _text(value, field)
            _reference(value, field)
        elif type(value) is list:
            for index, child in enumerate(value):
                visit(child, depth + 1, field + "[" + str(index) + "]")
        elif type(value) is dict:
            for key, child in value.items():
                _text(key, field + " key")
                visit(child, depth + 1, field + "." + key)
        else:
            raise OperationError("datatable_row_values_invalid", field + " has an unsupported non-JSON type")
    visit(values, 0, "row_values")
    try:
        encoded = json.dumps(values, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
    except (TypeError, ValueError, OverflowError, RecursionError) as error:
        raise OperationError("datatable_row_values_invalid", "row_values cannot be encoded as finite JSON") from error
    if len(encoded.encode("utf-16-le")) // 2 > _MAX_JSON_UNITS:
        raise OperationError("datatable_row_values_invalid", "row_values exceeds the adapter's 1048576 UTF-16 unit bound")
    # Preserve unknown keys and nulls. The finite native row parser owns its
    # explicit prewrite admission; importer aliases are never invented here.
    return encoded


def _schema_arguments(datatable_path):
    return {"datatable_path": _text(datatable_path, "datatable_path")}


def _list_arguments(datatable_path, row_name_contains, max_rows, include_values):
    return {**_schema_arguments(datatable_path), "row_name_contains": _text(row_name_contains, "row_name_contains"),
        "max_rows": _maximum(max_rows), "include_values": _flag(include_values, "include_values")}


def _get_arguments(datatable_path, row_name):
    return {**_schema_arguments(datatable_path), "row_name": _text(row_name, "row_name")}


def _add_arguments(datatable_path, row_name, row_values, save):
    return {**_get_arguments(datatable_path, row_name), "row_values_json": _row_json(row_values), "save": _flag(save, "save")}


def _remove_arguments(datatable_path, row_name, save):
    return {**_get_arguments(datatable_path, row_name), "save": _flag(save, "save")}


def _create_arguments(asset_path, row_struct_path, save):
    arguments = {"asset_path": _text(asset_path, "asset_path"), "row_struct_path": _text(row_struct_path, "row_struct_path"), "save": _flag(save, "save")}
    _reference(arguments["asset_path"], "asset_path")
    _reference(arguments["row_struct_path"], "row_struct_path")
    return arguments


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        adapter_explicit_asset_load_calls=0, adapter_explicit_compile_calls=0, adapter_explicit_save_calls=0,
        automatic_action_retry=False, full_legacy_datatable_parity=False,
        cold_persistence_verified=False, gameplay_tag_registration_verified=False,
        library_cdo_before_native_entry_independently_observed=False,
        actual_work_counts_authority="returned_native_counters_and_ordered_load_save_receipts")


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_result_returned=True, native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known, mutation_performed=native.get("changed") if known else None)
    _context(result, identity, arguments)
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="not_requested" if not native["compile_requested"] else "unexpected_native_compile")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_writer = known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"] = [] if no_writer else [{"kind": identity,
        "asset_path": native["data"].get("datatable", arguments.get("datatable_path", arguments.get("asset_path", ""))),
        "attempted": native["mutation_attempted"], "change_state_known": known,
        "changed": native.get("changed") if known else None,
        "outcome": "native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_datatable_postcondition" if native["postcondition_passed"] else "native_datatable_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run",
        initial_load_postload_effects="not_fully_instrumented", gameplay_tag_registration="not_run")
    result["stage"] = "returned_datatable_operation"


def _native_action(result, identity, arguments, checkpoint_callback=None):
    _context(result, identity, arguments)
    result["stage"] = "native_datatable_operation"
    result["data"].update(native_dispatched=True, native_result_returned=False, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "attempted": None, "change_state_known": False,
        "changed": None, "outcome": "unknown_until_native_response"}]
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            result["stage"] = "returned_datatable_operation"
            if checkpoint_callback:
                checkpoint_callback(result)
        if getattr(error, "code", "") == "native_datatable_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    if checkpoint_callback:
        checkpoint_callback(result)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_datatable_failed", native.get("error") or "DataTable operation failed; returned prefix retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("datatable_result_unverified", "Native success lacks complete known DataTable postconditions; do not replay")
    if native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not None:
        raise OperationError("datatable_unexpected_compile", "DataTable operation unexpectedly reports explicit compilation; actual prefix retained")
    if identity in READS and (native["save_requested"] or native["save_attempted"] or native["saved"]):
        raise OperationError("datatable_read_unexpected_save", "DataTable read unexpectedly reports an explicit save; load effects are separate")
    result.update(success=True, stage="complete")
    if identity in WRITES:
        result["retry"]["duplicate_risk"] = "inspect_rows_and_journal_no_writer_replay"


def _read_action(result, identity, arguments, checkpoint_callback=None):
    callback = None if checkpoint_callback is None else lambda observed: checkpoint_callback(observed, "returned_datatable_read")
    _native_action(result, identity, arguments, callback)


def _write_action(result, identity, arguments, checkpoint_callback=None):
    _context(result, identity, arguments)
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched", "interrupted_outcome": "unknown_inspect_datatable_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    callback = checkpoint_callback or state.checkpoint
    callback(result)
    _native_action(result, identity, arguments, callback)


def _rejected(identity, error):
    result = state.fail(state.new_result(), error)
    _context(result, identity, {})
    result["data"].update(admission_rejected=True, native_dispatched=False,
        native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
    return result


def _read(identity, arguments):
    result = state.new_result()
    try:
        _read_action(result, identity, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def get_datatable_schema(datatable_path):
    try:
        arguments = _schema_arguments(datatable_path)
    except Exception as error:
        return _rejected(SCHEMA, error)
    return _read(SCHEMA, arguments)


def get_datatable_schema_action(result, datatable_path, checkpoint_callback=None):
    _read_action(result, SCHEMA, _schema_arguments(datatable_path), checkpoint_callback)


def list_datatable_rows(datatable_path, row_name_contains="", max_rows=200, include_values=True):
    try:
        arguments = _list_arguments(datatable_path, row_name_contains, max_rows, include_values)
    except Exception as error:
        return _rejected(LIST, error)
    return _read(LIST, arguments)


def list_datatable_rows_action(result, datatable_path, row_name_contains="", max_rows=200,
        include_values=True, checkpoint_callback=None):
    _read_action(result, LIST, _list_arguments(datatable_path, row_name_contains, max_rows, include_values), checkpoint_callback)


def get_datatable_row(datatable_path, row_name):
    try:
        arguments = _get_arguments(datatable_path, row_name)
    except Exception as error:
        return _rejected(GET, error)
    return _read(GET, arguments)


def get_datatable_row_action(result, datatable_path, row_name, checkpoint_callback=None):
    _read_action(result, GET, _get_arguments(datatable_path, row_name), checkpoint_callback)


def add_or_update_datatable_row(datatable_path, row_name, row_values, save=True):
    try:
        arguments = _add_arguments(datatable_path, row_name, row_values, save)
    except Exception as error:
        return _rejected(ADD, error)
    return state.mutate(ADD, arguments, None, lambda result: _write_action(result, ADD, arguments))


def add_or_update_datatable_row_action(result, datatable_path, row_name, row_values, save=True, checkpoint_callback=None):
    _write_action(result, ADD, _add_arguments(datatable_path, row_name, row_values, save), checkpoint_callback)


def remove_datatable_row(datatable_path, row_name, save=True):
    try:
        arguments = _remove_arguments(datatable_path, row_name, save)
    except Exception as error:
        return _rejected(REMOVE, error)
    return state.mutate(REMOVE, arguments, None, lambda result: _write_action(result, REMOVE, arguments))


def remove_datatable_row_action(result, datatable_path, row_name, save=True, checkpoint_callback=None):
    _write_action(result, REMOVE, _remove_arguments(datatable_path, row_name, save), checkpoint_callback)


def create_datatable(asset_path, row_struct_path, save=True):
    try:
        arguments = _create_arguments(asset_path, row_struct_path, save)
    except Exception as error:
        return _rejected(CREATE, error)
    return state.mutate(CREATE, arguments, None, lambda result: _write_action(result, CREATE, arguments))


def create_datatable_action(result, asset_path, row_struct_path, save=True, checkpoint_callback=None):
    _write_action(result, CREATE, _create_arguments(asset_path, row_struct_path, save), checkpoint_callback)
