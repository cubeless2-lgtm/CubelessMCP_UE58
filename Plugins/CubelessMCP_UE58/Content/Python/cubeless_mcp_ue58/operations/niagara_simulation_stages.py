"""Original finite Grid2D stage addition with one durable mutation journal."""

import copy
import json
import math

from cubeless_mcp_ue58.compatibility.ue58 import niagara_simulation_stages as api
from . import state, variables
from .state import OperationError


IDENTITY = "add_niagara_grid_simulation_stage"
SETTINGS_IDENTITY = "set_niagara_simulation_stage_settings"


def _text(value, field, *, empty=False, maximum=256):
    try:
        return variables._text(value, field, empty=empty, maximum=maximum)
    except OperationError as error:
        raise OperationError("niagara_stage_input_invalid", str(error)) from error


def _optional_text(value, field):
    return _text("" if value is None else value, field, empty=True)


def _bool(value, field):
    if type(value) is not bool:
        raise OperationError("niagara_stage_bool_invalid", field + " must be an exact boolean")
    return value


def _int(value, field, *, minimum=0, optional=False):
    if optional and value is None:
        return None
    if type(value) is not int or not minimum <= value <= 2147483647:
        raise OperationError("niagara_stage_count_invalid",
            field + " must be " + ("null or " if optional else "") + "an exact int32 >= " + str(minimum))
    return value


def _duration(value, field):
    if type(value) not in (int, float) or not math.isfinite(value) or value <= 0:
        raise OperationError("niagara_stage_timing_invalid", field + " must be a positive finite number")
    return value


def _arguments(system_path, emitter_index, emitter_name, stage_name,
        data_interface_name, insert_index, skip_if_duplicate, enabled,
        iteration_source, direct_dispatch_type, direct_dispatch_element_type,
        execute_behavior, element_count, element_count_x, element_count_y,
        element_count_z, num_iterations, gpu_dispatch_force_linear,
        override_gpu_dispatch_num_threads, allow_source_edit, save,
        request_compile, wait_for_completion, timeout_seconds, poll_interval_seconds, *, _named_job=False):
    system = _text(system_path, "system_path", maximum=1023)
    if not system.startswith("/") or any(c in system for c in ("\\", ":", "'", '"')):
        raise OperationError("niagara_stage_path_invalid",
            "system_path must be an exact already-loaded package or asset object path")
    wait = _bool(wait_for_completion, "wait_for_completion")
    timing = _named_duration if _named_job else _duration
    timeout = timing(timeout_seconds, "timeout_seconds")
    poll = timing(poll_interval_seconds, "poll_interval_seconds")
    if wait and not _named_job:
        raise OperationError("niagara_stage_blocking_wait_unsupported",
            "wait_for_completion=True cannot block the official game thread; no native call or mutation was dispatched. Observe async readiness then save the exact asset without replaying creation")
    options = {
        "iteration_source": _text(iteration_source, "iteration_source", maximum=64),
        "execute_behavior": _text(execute_behavior, "execute_behavior", maximum=64),
        "num_iterations": _int(num_iterations, "num_iterations", minimum=1),
        "wait_for_completion": wait if _named_job else False,
    }
    # Keep the original wrapper's omission/presence rules. Native validation
    # owns enum aliases and array-versus-axis precedence; no option is dropped.
    for field, value in (("direct_dispatch_type", direct_dispatch_type),
            ("direct_dispatch_element_type", direct_dispatch_element_type)):
        value = _optional_text(value, field)
        if value:
            options[field] = value
    if element_count is not None:
        if type(element_count) is not list or not 1 <= len(element_count) <= 3:
            raise OperationError("niagara_stage_count_invalid",
                "element_count must be null or a list of one through three exact nonnegative int32 values")
        options["element_count"] = [_int(value, "element_count[" + str(index) + "]")
            for index, value in enumerate(element_count)]
    for field, value in (("element_count_x", element_count_x),
            ("element_count_y", element_count_y), ("element_count_z", element_count_z)):
        if value is not None:
            options[field] = _int(value, field)
    for field, value in (("gpu_dispatch_force_linear", gpu_dispatch_force_linear),
            ("override_gpu_dispatch_num_threads", override_gpu_dispatch_num_threads)):
        if value is not None:
            options[field] = _bool(value, field)
    return {"system_path": system,
        "emitter_index_json": json.dumps(_int(emitter_index, "emitter_index", optional=True)),
        "emitter_name": _optional_text(emitter_name, "emitter_name"),
        "stage_name": _text(stage_name, "stage_name"),
        "data_interface_name": _text(data_interface_name, "data_interface_name"),
        "insert_index_json": json.dumps(_int(insert_index, "insert_index", optional=True)),
        "skip_if_duplicate": _bool(skip_if_duplicate, "skip_if_duplicate"),
        "enabled": _bool(enabled, "enabled"),
        "options_json": json.dumps(options, allow_nan=False, sort_keys=True),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"),
        "save": _bool(save, "save"), "request_compile": _bool(request_compile, "request_compile"),
        "timeout_seconds": timeout, "poll_interval_seconds": poll}


def _apply_native(result, arguments, native, *, identity=IDENTITY):
    result["data"].update(copy.deepcopy(native["data"]))
    known = native["change_state_known"]
    result["data"].update(operation=identity,
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        particle_export_transport_verified=False, gpu_execution_verified=False)
    if identity == IDENTITY:
        result["data"].update(full_legacy_grid_stage_parity=False, blocking_wait_supported=False,
            timeout_seconds=arguments["timeout_seconds"], poll_interval_seconds=arguments["poll_interval_seconds"],
            timing_scope="Preserved caller/readiness-continuation context only; no game-thread wait or polling")
    else:
        result["data"].update(full_legacy_stage_settings_parity=False,
            compile_notification_scope="Original Stage.RequestRecompile and optional SetEnabled callbacks remain actual compile attempts even when request_compile=False; that caller flag controls only the extra explicit System request. Internal fanout and shader completion remain unmeasured")
    result["stage"] = native.get("stage", "native_niagara_stage_operation")
    compiled = native["compiled"]
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=compiled, status="passed" if compiled is True else "failed" if compiled is False
        else "requested_completion_unobserved" if native["compile_requested"]
        else "native_stage_recompile_without_explicit_system_request" if native["compile_attempted"] else "not_requested")
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    no_change = known and native.get("changed") is False and not native["mutation_attempted"]
    result["changes"] = [] if no_change else [{"kind": identity,
        "asset_path": arguments["system_path"], "attempted": native["mutation_attempted"],
        "changed": native.get("changed") if known else None, "change_state_known": known,
        "outcome": ("native_stage_addition_observation" if identity == IDENTITY
            else "native_stage_settings_observation") if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_stage_postcondition" if native["postcondition_passed"]
        else "native_postcondition_failed", native_postcondition_passed=native["postcondition_passed"],
        compile_data_scope="FillCompilationData metadata only, not compile or GPU execution proof",
        disk_reload="not_run", particle_export_transport="not_run", gpu_execution="not_run", packaged_runtime="not_run")


def _action(result, arguments, checkpoint_callback=None):
    result["data"].update(operation=IDENTITY, requested_system_path=arguments["system_path"],
        adapter_preloaded_asset=False, adapter_load_calls=0, full_legacy_grid_stage_parity=False,
        blocking_wait_supported=False, timeout_seconds=arguments["timeout_seconds"],
        poll_interval_seconds=arguments["poll_interval_seconds"], particle_export_transport_verified=False)
    result["data"]["pending_mutation"] = {"operation": IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_stages_graph_system_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_stage_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": IDENTITY, "asset_path": arguments["system_path"],
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = api.add_niagara_grid_simulation_stage(**{name: arguments[name] for name in api.NATIVE_ARGUMENTS})
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_stage_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native)
    result["retry"]["duplicate_risk"] = "stage_addition_would_repeat" if (
        native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]
        or native.get("changed") is True) else "none_observed"
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_stage_operation_failed",
            native.get("error") or "Stage addition failed; exact partial work is retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_stage_result_unverified",
            "Native success lacks known stage postconditions; do not retry creation")
    result.update(success=True, stage="complete")


def add_niagara_grid_simulation_stage(system_path, emitter_index=None, emitter_name="",
        stage_name="MCP_GridSimulationStage", data_interface_name="Emitter.Grid2D Collection",
        insert_index=None, skip_if_duplicate=True, enabled=True, iteration_source="DataInterface",
        direct_dispatch_type="", direct_dispatch_element_type="", execute_behavior="Always",
        element_count=None, element_count_x=None, element_count_y=None, element_count_z=None,
        num_iterations=1, gpu_dispatch_force_linear=None, override_gpu_dispatch_num_threads=None,
        allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, operation_id=None):
    try:
        arguments = _arguments(system_path, emitter_index, emitter_name, stage_name,
            data_interface_name, insert_index, skip_if_duplicate, enabled, iteration_source,
            direct_dispatch_type, direct_dispatch_element_type, execute_behavior, element_count,
            element_count_x, element_count_y, element_count_z, num_iterations, gpu_dispatch_force_linear,
            override_gpu_dispatch_num_threads, allow_source_edit, save, request_compile,
            wait_for_completion, timeout_seconds, poll_interval_seconds)
    except Exception as error:
        result = state.fail(state.new_result(operation_id), error)
        result["data"].update(admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False,
            full_legacy_grid_stage_parity=False, blocking_wait_supported=False)
        return result
    return state.mutate(IDENTITY, arguments, operation_id, lambda result: _action(result, arguments))


def add_niagara_grid_simulation_stage_action(result, system_path, emitter_index=None,
        emitter_name="", stage_name="MCP_GridSimulationStage", data_interface_name="Emitter.Grid2D Collection",
        insert_index=None, skip_if_duplicate=True, enabled=True, iteration_source="DataInterface",
        direct_dispatch_type="", direct_dispatch_element_type="", execute_behavior="Always",
        element_count=None, element_count_x=None, element_count_y=None, element_count_z=None,
        num_iterations=1, gpu_dispatch_force_linear=None, override_gpu_dispatch_num_threads=None,
        allow_source_edit=False, save=True, request_compile=True, wait_for_completion=False,
        timeout_seconds=10.0, poll_interval_seconds=0.1, checkpoint_callback=None):
    arguments = _arguments(system_path, emitter_index, emitter_name, stage_name,
        data_interface_name, insert_index, skip_if_duplicate, enabled, iteration_source,
        direct_dispatch_type, direct_dispatch_element_type, execute_behavior, element_count,
        element_count_x, element_count_y, element_count_z, num_iterations, gpu_dispatch_force_linear,
        override_gpu_dispatch_num_threads, allow_source_edit, save, request_compile,
        wait_for_completion, timeout_seconds, poll_interval_seconds)
    _action(result, arguments, checkpoint_callback)


def _settings_arguments(system_path, emitter_index, emitter_name, stage_index,
        stage_name, enabled, iteration_source, data_interface_name,
        direct_dispatch_type, direct_dispatch_element_type, execute_behavior,
        element_count, element_count_x, element_count_y, element_count_z,
        num_iterations, num_iterations_binding, gpu_dispatch_force_linear,
        override_gpu_dispatch_num_threads, allow_source_edit, save, request_compile):
    system = _text(system_path, "system_path", maximum=1023)
    if not system.startswith("/") or any(c in system for c in ("\\", ":", "'", '"')):
        raise OperationError("niagara_stage_path_invalid",
            "system_path must be an exact already-loaded package or asset object path")
    options = {}
    # The original wrapper sends a field only when present. Native validation
    # owns joint selectors, enum aliases and existing User.int bindings.
    for field, value in (("enabled", enabled), ("gpu_dispatch_force_linear", gpu_dispatch_force_linear),
            ("override_gpu_dispatch_num_threads", override_gpu_dispatch_num_threads)):
        if value is not None:
            options[field] = _bool(value, field)
    for field, value in (("iteration_source", iteration_source), ("data_interface_name", data_interface_name),
            ("direct_dispatch_type", direct_dispatch_type), ("direct_dispatch_element_type", direct_dispatch_element_type),
            ("execute_behavior", execute_behavior), ("num_iterations_binding", num_iterations_binding)):
        value = _optional_text(value, field)
        if value:
            options[field] = value
    if element_count is not None:
        if type(element_count) is not list or not 1 <= len(element_count) <= 3:
            raise OperationError("niagara_stage_count_invalid",
                "element_count must be null or a list of one through three exact nonnegative int32 values")
        options["element_count"] = [_int(value, "element_count[" + str(index) + "]")
            for index, value in enumerate(element_count)]
    for field, value in (("element_count_x", element_count_x), ("element_count_y", element_count_y),
            ("element_count_z", element_count_z)):
        if value is not None:
            options[field] = _int(value, field)
    if num_iterations is not None:
        options["num_iterations"] = _int(num_iterations, "num_iterations", minimum=1)
    return {"system_path": system,
        "emitter_index_json": json.dumps(_int(emitter_index, "emitter_index", optional=True)),
        "emitter_name": _optional_text(emitter_name, "emitter_name"),
        "stage_index_json": json.dumps(_int(stage_index, "stage_index", optional=True)),
        "stage_name": _optional_text(stage_name, "stage_name"),
        "options_json": json.dumps(options, allow_nan=False, sort_keys=True),
        "allow_source_edit": _bool(allow_source_edit, "allow_source_edit"),
        "save": _bool(save, "save"), "request_compile": _bool(request_compile, "request_compile")}


def _settings_action(result, arguments, checkpoint_callback=None):
    result["data"].update(operation=SETTINGS_IDENTITY, requested_system_path=arguments["system_path"],
        adapter_preloaded_asset=False, adapter_load_calls=0, full_legacy_stage_settings_parity=False,
        particle_export_transport_verified=False)
    result["data"]["pending_mutation"] = {"operation": SETTINGS_IDENTITY, **arguments,
        "dispatch_state_at_checkpoint": "not_yet_dispatched",
        "interrupted_outcome": "unknown_inspect_stage_settings_graph_system_and_journal"}
    result["stage"] = "before_mutation_checkpoint"
    (checkpoint_callback or state.checkpoint)(result)
    result["stage"] = "native_niagara_stage_settings_operation"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": SETTINGS_IDENTITY, "asset_path": arguments["system_path"],
        "attempted": None, "changed": None, "change_state_known": False,
        "outcome": "unknown_until_native_result"}]
    try:
        native = api.set_niagara_simulation_stage_settings(
            **{name: arguments[name] for name in api.SETTINGS_NATIVE_ARGUMENTS})
    except Exception as error:
        if getattr(error, "code", "") == "native_niagara_stage_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    _apply_native(result, arguments, native, identity=SETTINGS_IDENTITY)
    result["retry"]["duplicate_risk"] = "stage_settings_write_would_repeat" if (
        native["mutation_attempted"] or native["compile_attempted"] or native["save_attempted"]
        or native.get("changed") is True) else "none_observed"
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_niagara_stage_settings_operation_failed",
            native.get("error") or "Stage settings failed; exact partial work is retained")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("niagara_stage_settings_result_unverified",
            "Native success lacks known settings postconditions; do not retry mutation")
    result.update(success=True, stage="complete")


def set_niagara_simulation_stage_settings(system_path, emitter_index=None,
        emitter_name="", stage_index=None, stage_name="", enabled=None,
        iteration_source="", data_interface_name="", direct_dispatch_type="",
        direct_dispatch_element_type="", execute_behavior="", element_count=None,
        element_count_x=None, element_count_y=None, element_count_z=None,
        num_iterations=None, num_iterations_binding="", gpu_dispatch_force_linear=None,
        override_gpu_dispatch_num_threads=None, allow_source_edit=False,
        save=True, request_compile=True, operation_id=None):
    try:
        arguments = _settings_arguments(system_path, emitter_index, emitter_name,
            stage_index, stage_name, enabled, iteration_source, data_interface_name,
            direct_dispatch_type, direct_dispatch_element_type, execute_behavior,
            element_count, element_count_x, element_count_y, element_count_z,
            num_iterations, num_iterations_binding, gpu_dispatch_force_linear,
            override_gpu_dispatch_num_threads, allow_source_edit, save, request_compile)
    except Exception as error:
        result = state.fail(state.new_result(operation_id), error)
        result["data"].update(admission_rejected=True, native_dispatched=False,
            native_mutation_attempted=False, mutation_state_unknown=False,
            mutation_performed=False, full_legacy_stage_settings_parity=False)
        return result
    return state.mutate(SETTINGS_IDENTITY, arguments, operation_id,
        lambda result: _settings_action(result, arguments))


def set_niagara_simulation_stage_settings_action(result, system_path,
        emitter_index=None, emitter_name="", stage_index=None, stage_name="",
        enabled=None, iteration_source="", data_interface_name="",
        direct_dispatch_type="", direct_dispatch_element_type="",
        execute_behavior="", element_count=None, element_count_x=None,
        element_count_y=None, element_count_z=None, num_iterations=None,
        num_iterations_binding="", gpu_dispatch_force_linear=None,
        override_gpu_dispatch_num_threads=None, allow_source_edit=False,
        save=True, request_compile=True, checkpoint_callback=None):
    arguments = _settings_arguments(system_path, emitter_index, emitter_name,
        stage_index, stage_name, enabled, iteration_source, data_interface_name,
        direct_dispatch_type, direct_dispatch_element_type, execute_behavior,
        element_count, element_count_x, element_count_y, element_count_z,
        num_iterations, num_iterations_binding, gpu_dispatch_force_linear,
        override_gpu_dispatch_num_threads, allow_source_edit, save, request_compile)
    _settings_action(result, arguments, checkpoint_callback)

# Named2 reuses the existing HV Event Job lifecycle. Direct/actions above remain synchronous.
def _named_duration(value,field):
    if type(value) not in (int,float) or not math.isfinite(value):
        raise OperationError("niagara_stage_timing_invalid",field+" must be a finite number")
    return value  # Native retains the original clamp and bounded wait policy.

def _named_context(result,method,public):
    result["data"].update(operation=method,requested_arguments=copy.deepcopy(public["original"]),native_params=copy.deepcopy(public["args"]["params"]),entry_route="Named_official_MCP_only",async_native_job=True,shared_HV_lifecycle=True,parent_journal_only=True,no_Python_asset_mutation_compile_or_save=True,global_compiler_cancel_attempted=False,automatic_action_retry=False,atomic_rollback=False,full_legacy_grid_stage_parity=False,actual_official_async_wire_verified=False)

def _completed_replay(method,arguments,run_id):
    path=state._journal_path(run_id)
    if not path.is_file():return None
    import hashlib
    fingerprint=hashlib.sha256(json.dumps({"name":method,"arguments":arguments},sort_keys=True).encode("utf-8")).hexdigest()
    existing=state.get_operation(run_id)
    if existing.get("fingerprint")!=fingerprint:raise OperationError("operation_id_conflict","This operation_id already belongs to different arguments")
    if existing.get("state")!="complete":raise OperationError("operation_incomplete","A previous request may have changed assets; inspect its journal and assets. No automatic retry was performed")
    replay=copy.deepcopy(existing["result"]);replay["retry"]["replayed"]=True
    return replay

def _named_mcp(method,public):
    from . import niagara_event_stage_authoring as shared
    try:
        original=copy.deepcopy(public);values=copy.deepcopy(public);operation_id=values.pop("operation_id",None)
        if method==IDENTITY:arguments=_arguments(**values,_named_job=True)
        elif method==SETTINGS_IDENTITY:arguments=_settings_arguments(**values)
        else:raise OperationError("niagara_stage_method_invalid","Only the two original stage writers use this continuation")
        return shared._begin(method,dict(arguments=original,args=dict(params=arguments),original=original,operation_id=operation_id))
    except Exception as error:
        result=state.fail(state.new_result(public.get("operation_id")),error)
        result["data"].update(operation=method,admission_rejected=True,native_start_dispatched=False,native_dispatched=False,native_mutation_attempted=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
        result["compile"].update(requested=False,attempted=False,compiled=None);result["save"].update(requested=False,attempted=False,saved=False)
        return shared.completed_result(result)
