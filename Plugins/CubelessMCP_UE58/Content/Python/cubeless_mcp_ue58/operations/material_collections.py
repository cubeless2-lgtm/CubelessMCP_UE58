"""Two legacy MPC mutations with one journal and native row/count authority.

The domain accepts original dictionaries/lists. Their original public keys use
JSON strings at the reflected Registry boundary. No implicit load or RPC.
"""

import copy
import json

from cubeless_mcp_ue58.compatibility.ue58 import material_collections as api
from . import material, state, variables
from .state import OperationError


SET_VALUES = "set_material_parameter_collection_values"
MIRROR = "mirror_material_parameter_collection"


def _path(value, field, *, source=False):
    try:
        path = variables._text(value, field)
    except OperationError as error:
        raise OperationError("material_collection_path_invalid", str(error)) from error
    roots = ("/Game/", "/Engine/") if source else ("/Game/",)
    if not path.startswith(roots) or any(c in path for c in ("\\", ":", "'", '"')):
        raise OperationError("material_collection_path_invalid",
            field + " requires an exact already-loaded " + ("/Game or /Engine source" if source else "/Game target") + " package/object path")
    return path


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("material_collection_input_invalid", field + " must be an exact boolean")
    return value


def _json_envelope(value, field, shape):
    """Normalize only the original optional container and syntax boundary.

    Per-row invalid scalar/color values and filter entries reach the native
    legacy parser. They must not discard other valid rows at this boundary.
    """
    try:
        raw = value if type(value) is str else json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        # Validate with the existing bounded Material JSON parser. Ignore its
        # sorted output, retaining request order in the actual native payload.
        material._json_input(raw)
        decoded = json.loads(raw)
        if decoded is None:
            decoded = {} if shape is dict else []
        if type(decoded) is not shape:
            raise ValueError(field + " must be " + ("an object" if shape is dict else "an array") + " or null")
        normalized = json.dumps(decoded, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        material._json_input(normalized)
        return normalized
    except (OperationError, TypeError, ValueError, RecursionError) as error:
        raise OperationError("material_collection_json_invalid", field + ": " + str(error)) from error


def _set_arguments(collection_path, scalars, vectors, set_asset_defaults, set_runtime, save):
    return {"collection_path": _path(collection_path, "collection_path"),
        "scalars_json": _json_envelope(scalars, "scalars", dict),
        "vectors_json": _json_envelope(vectors, "vectors", dict),
        "set_asset_defaults": _bool(set_asset_defaults, "set_asset_defaults"),
        "set_runtime": _bool(set_runtime, "set_runtime"), "save": _bool(save, "save")}


def _mirror_arguments(source_collection_path, target_collection_path,
        parameter_names, copy_defaults, preserve_ids, save):
    return {"source_collection_path": _path(source_collection_path, "source_collection_path", source=True),
        "target_collection_path": _path(target_collection_path, "target_collection_path"),
        "parameter_names_json": _json_envelope(parameter_names, "parameter_names", list),
        "copy_defaults": _bool(copy_defaults, "copy_defaults"),
        "preserve_ids": _bool(preserve_ids, "preserve_ids"), "save": _bool(save, "save")}


def _rejected(error, operation_id=None):
    result = state.fail(state.new_result(operation_id), error)
    result["data"].update(admission_rejected=True, native_dispatched=False,
        native_mutation_attempted=False, mutation_state_unknown=False,
        mutation_performed=False, full_legacy_material_collection_parity=False,
        full_public_input_representation_parity=False)
    return result


def _apply_native(result, identity, arguments, native):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=identity,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        full_legacy_material_collection_parity=False, full_public_input_representation_parity=False,
        registry_json_representation="Original scalars/vectors/parameter_names keys use optional JSON strings; typed domain dict/list remains supported",
        runtime_rendering_verified=False, ticker_synchronization_verified=False)
    result["stage"] = native.get("stage", "native_material_collection_operation")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status="not_requested" if not native["compile_requested"] else "requested_completion_unobserved")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    target = arguments.get("target_collection_path", arguments.get("collection_path"))
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": identity, "asset_path": target,
        "attempted": native["mutation_attempted"], "changed": native.get("changed") if known else None,
        "change_state_known": known, "outcome": "native_asset_and_world_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_material_collection_postcondition" if native["postcondition_passed"] else "native_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"], disk_reload="not_run", runtime_rendering="not_run",
        ticker_synchronization="not_run", packaged_runtime="not_run")


def _run_action(result, identity, arguments, checkpoint_callback=None):
    if identity not in (SET_VALUES, MIRROR):
        raise OperationError("material_collection_operation_invalid", "Only the two named legacy collection mutations are supported")
    target = arguments.get("target_collection_path", arguments.get("collection_path"))
    result["data"].update(operation=identity, requested_collection_path=target,
        adapter_preloaded_asset=False, adapter_load_calls=0,
        full_legacy_material_collection_parity=False, full_public_input_representation_parity=False)
    result["data"]["pending_mutation"] = {"operation": identity, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_collection_world_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_material_collection_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": identity, "asset_path": target, "attempted": None,
        "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    try:
        native = getattr(api, identity)(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_material_collection_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, identity, arguments, native)
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_material_collection_operation_failed",
            native.get("error") or "Native collection mutation failed; actual prefix and counts are retained")
    if native["change_state_known"] is not True or native["postcondition_passed"] is not True:
        raise OperationError("material_collection_result_unverified", "Native success lacks known asset/world postconditions; do not replay")
    if native["compile_requested"] or native["compile_attempted"]:
        result["data"]["mutation_state_unknown"] = True
        raise OperationError("material_collection_unexpected_compile", "Collection mutation unexpectedly requested explicit compilation; inspect before another action")
    result.update(success=True, stage="complete")
    repeated = native["mutation_attempted"] or native["save_attempted"] or native.get("changed") is True
    result["retry"]["duplicate_risk"] = "collection_action_would_repeat" if repeated else "none_observed"


def set_material_parameter_collection_values(collection_path, scalars=None, vectors=None,
        set_asset_defaults=True, set_runtime=True, save=True, operation_id=None):
    try:
        arguments = _set_arguments(collection_path, scalars, vectors, set_asset_defaults, set_runtime, save)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(SET_VALUES, arguments, operation_id, lambda result: _run_action(result, SET_VALUES, arguments))


def set_material_parameter_collection_values_action(result, collection_path, scalars=None, vectors=None,
        set_asset_defaults=True, set_runtime=True, save=True, checkpoint_callback=None):
    _run_action(result, SET_VALUES, _set_arguments(collection_path, scalars, vectors, set_asset_defaults, set_runtime, save), checkpoint_callback)


def mirror_material_parameter_collection(source_collection_path, target_collection_path, parameter_names=None,
        copy_defaults=True, preserve_ids=True, save=True, operation_id=None):
    try:
        arguments = _mirror_arguments(source_collection_path, target_collection_path, parameter_names, copy_defaults, preserve_ids, save)
    except Exception as error:
        return _rejected(error, operation_id)
    return state.mutate(MIRROR, arguments, operation_id, lambda result: _run_action(result, MIRROR, arguments))


def mirror_material_parameter_collection_action(result, source_collection_path, target_collection_path, parameter_names=None,
        copy_defaults=True, preserve_ids=True, save=True, checkpoint_callback=None):
    _run_action(result, MIRROR, _mirror_arguments(source_collection_path, target_collection_path,
        parameter_names, copy_defaults, preserve_ids, save), checkpoint_callback)
