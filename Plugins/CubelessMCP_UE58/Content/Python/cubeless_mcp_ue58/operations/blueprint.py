"""Legacy Blueprint lifecycle operations with observable partial outcomes."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import blueprint as api
from .state import OperationError, fail, mutate, new_result


def parent_comparison_state(observation):
    """Ignore only the dependency audit's known timing field, retaining raw input.

    Accept the native inspector or its Python file-observation envelope. No
    recursive key filtering: user metadata, status, identities, dependency edges
    and source file hashes must remain part of equality.
    """
    value = copy.deepcopy(observation)
    inspection = value.get("inspection", value)
    audit = inspection.get("dependency_audit") if isinstance(inspection, dict) else None
    if isinstance(audit, dict):
        audit.pop("elapsed_ms", None)
    return value


def create_blueprint(name, parent_class, package_path="", operation_id=""):
    arguments = {"name": name, "parent_class": parent_class, "package_path": package_path}

    return mutate('create_blueprint', arguments, operation_id, lambda result: create_blueprint_action(result, name, parent_class, package_path))


def create_blueprint_action(result, name, parent_class, package_path, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    api.ensure_editor_idle()
    path = api.new_asset_path(name, package_path)
    subsystem = api.asset_subsystem()
    if subsystem.does_asset_exist(path) or subsystem.does_directory_exist(path):
        raise OperationError("asset_already_exists", f"An asset or directory already exists at {path}; nothing was overwritten")
    policy = api.creation_policy()
    result["data"]["creation_policy"] = policy
    function_library = parent_class == "/Script/Engine.BlueprintFunctionLibrary"
    if function_library:
        result["data"]["function_library_creation"] = {
            "requested": True, "factory_class_path": "/Script/UnrealEd.BlueprintFunctionLibraryFactory",
            "expected_parent_class": parent_class, "factory_returned": False,
            "returned_identity_observed": False, "postcondition_passed": False,
            "function_definition_supported_by_creation": False, "external_call_supported_by_creation": False,
            "runtime_execution_verified": False}
    generated_parent = parent_class.strip().startswith("/Game/")
    parent_load_before = api.parent_reference_state(parent_class) if generated_parent else None
    if parent_load_before is not None:
        result["data"]["parent_before_load"] = parent_load_before
        result["data"]["parent_load_side_effects_fully_observed"] = False
    parent, parents = api.resolve_parent(parent_class)
    parent_baseline = api.generated_parent_observation(parent) if generated_parent else None
    if parent_baseline is not None:
        result["data"]["parent_before_factory"] = parent_baseline
        loaded = parent_baseline["parents"][0]
        if (parent_load_before["file"] != loaded["file"] or parent_load_before["dirty"] != loaded["dirty"]
                or parent_load_before["loaded"] and parent_load_before["status"] != loaded["status"]):
            result["data"]["mutation_state_unknown"] = True
            result["changes"].append({"kind": "parent_load_observed_change", "asset_path": loaded["package_name"], "attempted": True})
            raise OperationError("generated_parent_changed_during_load", "Parent source bytes/loaded status changed during resolution; factory was not called")
    result["stage"] = "create"
    result["compile"]["scope"] = "explicit action; factory initial compile is reported separately"
    result["compile"]["implicit_factory_compile"] = {
        "attempted": False, "factory_call_attempted": False, "factory_returned": False,
        "status": "factory_not_called", "engine_status": None, "diagnostics_captured": False}
    change = {"kind": "create_blueprint", "asset_path": path, "attempted": False, "created": False}

    def observe_parent(phase):
        if parent_baseline is None:
            return
        try:
            current = api.generated_parent_observation(parent)
            result["data"]["parent_" + phase] = current
            if parent_comparison_state(current) != parent_comparison_state(parent_baseline):
                raise OperationError("generated_parent_changed", "Parent class chain, files or loaded state changed; inspect partial results, no automatic rollback/retry")
        except Exception:
            result["data"]["mutation_state_unknown"] = True
            result["validation"]["generated_parent_preserved"] = False
            raise
        result["validation"]["generated_parent_preserved"] = True

    def before_factory():
        # The compatibility layer has passed its second policy check. Persist
        # intent before dispatch; a rejected policy must leave changes empty.
        observe_parent("at_dispatch")
        result["changes"].append(change)
        if checkpoint_callback is not None:
            checkpoint_callback(result)
        change.update(attempted=True, created=None)
        result["compile"]["implicit_factory_compile"].update(
            attempted=None, factory_call_attempted=True, status="unknown_until_factory_returns")

    def after_factory(blueprint):
        # Record the known outcome before any reflected readback can fail.
        change["created"] = True if blueprint is not None else None
        result["compile"]["implicit_factory_compile"].update(
            attempted=True if blueprint is not None else None, factory_returned=True,
            status="factory_returned" if blueprint is not None else "factory_returned_without_asset")
        if function_library:
            api.function_library_return_identity(blueprint, result["data"]["function_library_creation"])
        observe_parent("after_factory")

    try:
        if function_library:
            blueprint = api.create_function_library(name, path, parent, before_factory, after_factory)
        else:
            blueprint = api.create(name, path, parent, before_factory, after_factory)
    except Exception as error:
        if function_library and hasattr(error, "native_function_library_result"):
            result["data"]["function_library_creation"]["native_observation_failure"] = copy.deepcopy(error.native_function_library_result)
        if function_library and change["attempted"]:
            result["data"]["mutation_state_unknown"] = True
        observe_parent("after_factory_failure")
        raise
    if blueprint is None:
        if function_library:
            result["data"]["mutation_state_unknown"] = True
        raise OperationError("create_failed", f"Blueprint factory failed at {path}; inspect package state before retry")
    result["stage"] = "readback"
    if function_library:
        change["asset_path"] = result["data"]["function_library_creation"]["returned_object_path"]
    else:
        change["asset_path"] = blueprint.get_path_name()
    if function_library:
        try:
            result["data"].update(api.function_library_creation_snapshot(
                blueprint, path, parent, result["data"]["function_library_creation"]))
        except Exception as error:
            if hasattr(error, "native_function_library_result"):
                result["data"]["function_library_creation"]["native_observation_failure"] = copy.deepcopy(error.native_function_library_result)
            result["data"]["mutation_state_unknown"] = True
            result["validation"]["function_library_creation_verified"] = False
            raise
        result["validation"]["function_library_creation_verified"] = True
    else:
        result["data"].update(api.snapshot(blueprint))
    result["compile"]["implicit_factory_compile"].update(
        engine_status=result["data"]["status"], status="status_observed")
    result["data"]["requested_parent_class"] = parent_class
    result["data"]["legacy_contract"] = "create_blueprint: unsaved creation, no overwrite"
    result["validation"].update({"status": "loaded_readback_passed", "parent_chain_guard": "passed",
                                 "parent_chain": parents, "asset_identity": "passed"})
    result["success"] = True
    result["stage"] = "complete"
    result["retry"]["duplicate_risk"] = "asset_already_created"
    result["retry"]["policy"] = "Replay the same operation_id to retrieve its outcome; use inspect_blueprint before any new operation."


def inspect_blueprint(blueprint_name):
    result = new_result()
    try:
        inspect_blueprint_action(result, blueprint_name)
    except Exception as error:
        fail(result, error)
    return result



def inspect_blueprint_action(result, blueprint_name, checkpoint_callback=None):
    """Internal loaded-state read; no compile/save or asset mutation."""
    result["stage"] = "load"
    blueprint = api.resolve_blueprint(blueprint_name)
    result["stage"] = "readback"
    result["data"] = api.snapshot(blueprint)
    result["validation"].update({"status": "loaded_readback_passed", "parent_chain_guard": "passed"})
    result["retry"].update({"policy": "Read-only inspection can be repeated; load_asset may return an already-loaded object."})
    result["success"] = True
    result["stage"] = "complete"


def compile_blueprint(blueprint_name, save=False, refresh_nodes=False, operation_id=""):
    arguments = {"blueprint_name": blueprint_name, "save": save, "refresh_nodes": refresh_nodes}

    return mutate('compile_blueprint', arguments, operation_id, lambda result: compile_blueprint_action(result, blueprint_name, save, refresh_nodes))


def compile_blueprint_action(result, blueprint_name, save, refresh_nodes, checkpoint_callback=None):
    """Internal guarded action; standalone/batch callers own journaling."""
    result["compile"]["requested"] = True
    result["save"]["requested"] = save
    api.ensure_editor_idle()
    blueprint = api.resolve_blueprint(blueprint_name)
    before = api.snapshot(blueprint)
    result["data"]["before"] = before
    result["stage"] = "compile"
    result["changes"].append({"kind": "refresh_and_compile" if refresh_nodes else "compile",
                              "asset_path": blueprint.get_path_name(), "attempted": True,
                              "mutation_observed": None})
    # The native call can change engine state before a Python exception or
    # malformed response. Never report "not attempted" or "not saved" then.
    result["compile"].update({"attempted": True, "compiled": None, "status": "unknown_until_native_response"})
    if save:
        result["save"].update({"attempted": None, "saved": None, "status": "unknown_until_native_response"})
    if checkpoint_callback is not None:
        checkpoint_callback(result)
    native_result = api.compile_validation(blueprint, save, refresh_nodes)
    result["changes"][-1]["mutation_observed"] = bool(native_result.get("mutation_performed"))
    result["data"].update(native_result)
    result["compile"].update({"attempted": bool(native_result.get("mutation_performed")), "compiled": bool(native_result.get("compiled")),
                              "status": native_result.get("status", "unknown"),
                              "refreshed_nodes": bool(native_result.get("refreshed_nodes")),
                              "error_count": native_result.get("compile_error_count"),
                              "warning_count": native_result.get("compile_warning_count"),
                              "diagnostics": native_result.get("diagnostics", [])})
    result["save"].update({"attempted": bool(save and native_result.get("compiled")),
                           "saved": bool(native_result.get("saved")),
                           "status": "saved" if native_result.get("saved") else "not_saved"})
    result["validation"].update({"status": "compile_passed" if native_result.get("validation_pass") else "compile_failed",
                                 "parent_chain_guard": "passed"})
    if not native_result.get("compiled"):
        result["stage"] = native_result.get("stage", "compile")
        raise OperationError(native_result.get("error_code", "blueprint_compile_failed"), native_result.get("error", "Blueprint did not compile; inspect compile diagnostics"))
    result["stage"] = "save" if save else "readback"
    if save and not native_result.get("saved"):
        raise OperationError("blueprint_save_failed", "Compilation succeeded but the requested asset save failed")
    result["stage"] = "readback"
    result["data"]["after"] = api.snapshot(blueprint)
    result["success"] = True
    result["stage"] = "complete"
    result["retry"].update({"duplicate_risk": "compile_or_refresh_would_repeat",
                            "policy": "Replay the same operation_id for the recorded outcome. Readback is not a disk reload."})
