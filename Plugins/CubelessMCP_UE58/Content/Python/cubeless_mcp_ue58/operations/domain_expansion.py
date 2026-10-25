"""The 28 documented UE58 domain contracts on the existing local mutation journal."""
import copy
import json
import re
from . import state
from .state import OperationError
from ..compatibility.ue58 import domain_expansion as api

CONTRACTS = {'configure_pcg_custom_hlsl_elements': {'mutation': True, 'parameters': [{'name': 'graph_path', 'kind': 'str', 'carrier': 'graph_path', 'has_default': False}, {'name': 'node_name', 'kind': 'str', 'carrier': 'node_name', 'has_default': False}, {'name': 'element_counts', 'kind': 'dict_int', 'carrier': 'element_counts_json', 'has_default': False}, {'name': 'expected_current', 'kind': 'optional_dict_int', 'carrier': 'expected_current_json', 'has_default': True, 'default': None}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['pcg_custom_hlsl_elements']}, 'inspect_niagara_emitter_execution_modes': {'mutation': False, 'parameters': [{'name': 'system_path', 'kind': 'str', 'carrier': 'system_path', 'has_default': False}, {'name': 'max_results', 'kind': 'int', 'carrier': 'max_results', 'has_default': True, 'default': 128}], 'native_operations': ['niagara_emitter_execution']}, 'inspect_controlrig_physics_units': {'mutation': False, 'parameters': [{'name': 'control_rig_path', 'kind': 'str', 'carrier': 'control_rig_path', 'has_default': False}, {'name': 'max_results', 'kind': 'int', 'carrier': 'max_results', 'has_default': True, 'default': 128}], 'native_operations': ['controlrig_physics_units']}, 'inspect_anim_blueprint_compiled_layout': {'mutation': False, 'parameters': [{'name': 'blueprint_path', 'kind': 'str', 'carrier': 'blueprint_path', 'has_default': False}, {'name': 'max_results', 'kind': 'int', 'carrier': 'max_results', 'has_default': True, 'default': 128}], 'native_operations': ['animbp_compiled_layout']}, 'inspect_chaos_body_solver_overrides': {'mutation': False, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}], 'native_operations': ['chaos_body']}, 'configure_chaos_body_solver_overrides': {'mutation': True, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'settings', 'kind': 'dict_any', 'carrier': 'settings_json', 'has_default': False}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['chaos_body_configure']}, 'inspect_chaos_cloth_solver_properties': {'mutation': False, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'max_results', 'kind': 'int', 'carrier': 'max_results', 'has_default': True, 'default': 128}], 'native_operations': ['cloth_solver']}, 'inspect_chaos_cloth_snapshot_history': {'mutation': False, 'parameters': [{'name': 'dataflow_path', 'kind': 'str', 'carrier': 'dataflow_path', 'has_default': False}, {'name': 'node_name', 'kind': 'str', 'carrier': 'node_name', 'has_default': False}, {'name': 'max_results', 'kind': 'int', 'carrier': 'max_results', 'has_default': True, 'default': 128}], 'native_operations': ['cloth_snapshots']}, 'inspect_controlrig_physics_unit_catalog': {'mutation': False, 'parameters': [], 'native_operations': ['controlrig.catalog']}, 'build_controlrig_physics_graph_variant': {'mutation': True, 'parameters': [{'name': 'source_rig_path', 'kind': 'str', 'carrier': 'source_rig_path', 'has_default': False}, {'name': 'target_rig_path', 'kind': 'str', 'carrier': 'target_rig_path', 'has_default': False}, {'name': 'nodes', 'kind': 'list_dict_any', 'carrier': 'nodes_json', 'has_default': False}, {'name': 'links', 'kind': 'list_dict_str', 'carrier': 'links_json', 'has_default': False}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['controlrig.recipe']}, 'inspect_anim_blueprint_authoring_nodes': {'mutation': False, 'parameters': [], 'native_operations': ['authoring.animbp.catalog']}, 'build_anim_blueprint_graph_variant': {'mutation': True, 'parameters': [{'name': 'source_blueprint_path', 'kind': 'str', 'carrier': 'source_blueprint_path', 'has_default': False}, {'name': 'target_blueprint_path', 'kind': 'str', 'carrier': 'target_blueprint_path', 'has_default': False}, {'name': 'nodes', 'kind': 'list_dict_any', 'carrier': 'nodes_json', 'has_default': False}, {'name': 'links', 'kind': 'list_dict_str', 'carrier': 'links_json', 'has_default': False}, {'name': 'graph_name', 'kind': 'str', 'carrier': 'graph_name', 'has_default': True, 'default': 'AnimGraph'}, {'name': 'replace_root', 'kind': 'bool', 'carrier': 'replace_root', 'has_default': True, 'default': False}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.animbp.recipe']}, 'inspect_niagara_stateless_authoring': {'mutation': False, 'parameters': [{'name': 'system_path', 'kind': 'str', 'carrier': 'system_path', 'has_default': False}], 'native_operations': ['authoring.niagara.inspect']}, 'create_niagara_stateless_system': {'mutation': True, 'parameters': [{'name': 'target_system_path', 'kind': 'str', 'carrier': 'target_system_path', 'has_default': False}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.niagara.create']}, 'build_niagara_stateless_variant': {'mutation': True, 'parameters': [{'name': 'source_system_path', 'kind': 'str', 'carrier': 'source_system_path', 'has_default': False}, {'name': 'target_system_path', 'kind': 'str', 'carrier': 'target_system_path', 'has_default': False}, {'name': 'emitters', 'kind': 'list_dict_any', 'carrier': 'emitters_json', 'has_default': False}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.niagara.variant']}, 'configure_pcg_manual_inspection': {'mutation': True, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'enabled', 'kind': 'bool', 'carrier': 'enabled', 'has_default': True, 'default': True}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.pcg.inspection']}, 'inspect_pcg_manual_override_targets': {'mutation': False, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}], 'native_operations': ['authoring.pcg.targets']}, 'edit_pcg_manual_point_override': {'mutation': True, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'target_key', 'kind': 'str', 'carrier': 'target_key', 'has_default': False}, {'name': 'action', 'kind': 'str', 'carrier': 'action', 'has_default': False}, {'name': 'data_index', 'kind': 'int', 'carrier': 'data_index', 'has_default': True, 'default': 0}, {'name': 'point_index', 'kind': 'int', 'carrier': 'point_index', 'has_default': True, 'default': -1}, {'name': 'transform', 'kind': 'str', 'carrier': 'transform', 'has_default': True, 'default': ''}, {'name': 'delta_key', 'kind': 'str', 'carrier': 'delta_key', 'has_default': True, 'default': ''}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.pcg.edit']}, 'inspect_chaos_cloth_dataflow_node_catalog': {'mutation': False, 'parameters': [], 'native_operations': ['authoring.cloth.graph_catalog']}, 'build_chaos_cloth_dataflow_graph': {'mutation': True, 'parameters': [{'name': 'target_dataflow_path', 'kind': 'str', 'carrier': 'target_dataflow_path', 'has_default': False}, {'name': 'nodes', 'kind': 'list_dict_any', 'carrier': 'nodes_json', 'has_default': False}, {'name': 'links', 'kind': 'list_dict_str', 'carrier': 'links_json', 'has_default': False}, {'name': 'source_dataflow_path', 'kind': 'str', 'carrier': 'source_dataflow_path', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.cloth.graph_recipe']}, 'create_chaos_cloth_asset_from_template': {'mutation': True, 'parameters': [{'name': 'target_cloth_path', 'kind': 'str', 'carrier': 'target_cloth_path', 'has_default': False}, {'name': 'template_dataflow_path', 'kind': 'str', 'carrier': 'template_dataflow_path', 'has_default': False}, {'name': 'evaluate', 'kind': 'bool', 'carrier': 'evaluate', 'has_default': True, 'default': False}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.cloth.create_asset']}, 'inspect_chaos_cloth_snapshot_state': {'mutation': False, 'parameters': [{'name': 'dataflow_path', 'kind': 'str', 'carrier': 'dataflow_path', 'has_default': False}, {'name': 'node_name', 'kind': 'str', 'carrier': 'node_name', 'has_default': False}], 'native_operations': ['authoring.cloth.snapshots']}, 'edit_chaos_cloth_snapshot_history': {'mutation': True, 'parameters': [{'name': 'dataflow_path', 'kind': 'str', 'carrier': 'dataflow_path', 'has_default': False}, {'name': 'node_name', 'kind': 'str', 'carrier': 'node_name', 'has_default': False}, {'name': 'action', 'kind': 'str', 'carrier': 'action', 'has_default': False}, {'name': 'index', 'kind': 'int', 'carrier': 'index', 'has_default': False}, {'name': 'name', 'kind': 'str', 'carrier': 'name', 'has_default': True, 'default': ''}, {'name': 'locked', 'kind': 'bool', 'carrier': 'locked', 'has_default': True, 'default': False}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.cloth.snapshots']}, 'inspect_chaos_body_collision': {'mutation': False, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}], 'native_operations': ['authoring.physics.body']}, 'configure_chaos_body_collision': {'mutation': True, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'settings', 'kind': 'dict_any', 'carrier': 'settings_json', 'has_default': False}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.physics.body']}, 'inspect_chaos_constraint_profile': {'mutation': False, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': True, 'default': ''}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': True, 'default': ''}, {'name': 'physics_asset_path', 'kind': 'str', 'carrier': 'physics_asset_path', 'has_default': True, 'default': ''}, {'name': 'joint_name', 'kind': 'str', 'carrier': 'joint_name', 'has_default': True, 'default': ''}], 'native_operations': ['authoring.physics.constraint', 'authoring.physics.asset_constraint']}, 'configure_chaos_constraint_profile': {'mutation': True, 'parameters': [{'name': 'actor_path', 'kind': 'str', 'carrier': 'actor_path', 'has_default': False}, {'name': 'component_name', 'kind': 'str', 'carrier': 'component_name', 'has_default': False}, {'name': 'settings', 'kind': 'dict_str', 'carrier': 'settings_json', 'has_default': False}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.physics.constraint']}, 'configure_physics_asset_constraint_profile': {'mutation': True, 'parameters': [{'name': 'physics_asset_path', 'kind': 'str', 'carrier': 'physics_asset_path', 'has_default': False}, {'name': 'joint_name', 'kind': 'str', 'carrier': 'joint_name', 'has_default': False}, {'name': 'settings', 'kind': 'dict_str', 'carrier': 'settings_json', 'has_default': False}, {'name': 'expected_state_hash', 'kind': 'str', 'carrier': 'expected_state_hash', 'has_default': True, 'default': ''}, {'name': 'dry_run', 'kind': 'bool', 'carrier': 'dry_run', 'has_default': True, 'default': True}], 'native_operations': ['authoring.physics.asset_constraint']}}


def tool_error(message, error_code="domain_input_invalid"):
    return {"success": False, "error": message, "error_code": error_code}


def _native(operation, **params):
    # Original work-request semantics, now delivered only through the Editor attachment.
    return {"operation": operation, **params}


def _rig_native(operation, **params):
    return _native("controlrig." + operation, **params)


def _typed(value, kind):
    if kind == "str": return type(value) is str
    if kind == "bool": return type(value) is bool
    if kind == "int": return type(value) is int
    if kind == "optional_dict_int" and value is None: return True
    if kind.startswith("list_"):
        return type(value) is list and all(_typed(row, kind[5:]) for row in value)
    if type(value) is not dict or any(type(key) is not str for key in value): return False
    if kind in ("dict_int", "optional_dict_int"): return all(type(item) is int for item in value.values())
    if kind == "dict_str": return all(type(item) is str for item in value.values())
    return kind == "dict_any"


def invoke_tool(name, carriers, operation_id=None):
    try:
        spec = CONTRACTS[name]
        logical = {}
        if set(carriers) != {p["carrier"] for p in spec["parameters"]}:
            raise OperationError("domain_input_fields", "Exact documented Toolset carrier fields required")
        for param in spec["parameters"]:
            value = carriers[param["carrier"]]
            if "dict" in param["kind"]:
                if type(value) is not str or len(value) > 1048576:
                    raise OperationError("domain_json_carrier", param["carrier"] + " requires bounded JSON text")
                value = json.loads(value, parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
            logical[param["name"]] = value
        return _execute(name, logical, operation_id)
    except Exception as error:
        return state.fail(state.new_result(operation_id if type(operation_id) is str else ""), error)


def _adopt(result, native, request, mutation):
    raw = native.pop("_raw_native_json")
    preserved = copy.deepcopy(native)
    data = result["data"]
    data.update(native_result=preserved, native_raw_result=raw, native_result_returned=True)
    for key in ("changed", "dry_run", "saved", "compiled", "rollback", "rolled_back", "candidate_retained", "do_not_retry", "pending", "compilation_pending", "mutation_attempted", "mutation_performed", "change_state_known", "compile_succeeded"):
        if key in native: data[key] = copy.deepcopy(native[key])
    result.update(success=native["success"], error_code=native.get("error_code", ""),
        error=native.get("error", ""), stage=native.get("stage", "native_domain_returned"))
    known = native.get("change_state_known") is True
    if mutation:
        attempted = native.get("mutation_attempted") if type(native.get("mutation_attempted")) is bool else None
        data["mutation_state_unknown"] = not known
        result["changes"] = [dict(kind=request["operation"], attempted=attempted,
            change_state_known=known, changed=native.get("changed") if known and type(native.get("changed")) is bool else None)]
    compiled = native.get("compiled")
    attempted_compile = native.get("compile_attempted")
    compile_status = "native_receipt_fields_only"
    if type(native.get("compile_succeeded")) is bool:
        compiled = native["compile_succeeded"]
        # This explicit terminal compiler receipt reports an actual compile;
        # it does not imply that an inspector requested an existing pending job.
        if type(attempted_compile) is not bool: attempted_compile = True
        compile_status = "succeeded_native_receipt" if compiled else "failed_native_receipt"
    if native.get("compilation_pending") is True:
        compile_status = "pending_native_receipt"
    result["compile"].update(requested=native.get("compile_requested"), attempted=attempted_compile,
        compiled=compiled, status=compile_status)
    result["save"].update(requested=native.get("save_requested"), attempted=native.get("save_attempted"),
        saved=native.get("saved"), status="not_saved_native_receipt" if native.get("saved") is False else
            "saved_native_receipt" if native.get("saved") is True else "native_receipt_fields_only")
    result["validation"].update(status="native_receipt_only", complete_domain_contract_verified=False)
    retained = native.get("candidate_retained") is True or native.get("do_not_retry") is True
    pending = native.get("pending") is True or native.get("compilation_pending") is True
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="retained_candidate_or_pending_native_work" if retained or pending else
            "inspect_native_receipt_and_actual_state_before_new_mutation" if mutation else "none_observed")


def _execute(name, arguments, operation_id=None):
    try:
        spec = CONTRACTS[name]
        if operation_id is not None and type(operation_id) is not str:
            raise OperationError("domain_operation_id", "operation_id must be a string")
        if set(arguments) != {p["name"] for p in spec["parameters"]}:
            raise OperationError("domain_input_fields", "Exact documented logical arguments required")
        for param in spec["parameters"]:
            if not _typed(arguments[param["name"]], param["kind"]):
                raise OperationError("domain_input_type", param["name"] + " has an invalid documented type")
        # Reject nonfinite/unserializable nested Any values before journal/engine dispatch.
        json.dumps(arguments, ensure_ascii=False, allow_nan=False)
        request = globals()["_request_" + name](**arguments)
        if request.get("success") is False:
            raise OperationError(request.get("error_code", "domain_input_invalid"), request.get("error", "Invalid domain request"))
        if request.get("operation") not in spec["native_operations"]:
            raise OperationError("domain_operation_mismatch", "Request builder did not select its documented native operation")
    except Exception as error:
        return state.fail(state.new_result(operation_id if type(operation_id) is str else ""), error)

    def action(result):
        result["data"].update(public_method=name, requested_arguments=copy.deepcopy(arguments),
            native_request=copy.deepcopy(request), native_dispatched=False)
        result["stage"] = "before_domain_dispatch"
        if spec["mutation"]: state.checkpoint(result)
        result["data"].update(native_dispatched=True, native_result_returned=False,
            mutation_state_unknown=spec["mutation"])
        if spec["mutation"]:
            result["changes"] = [dict(kind=request["operation"], attempted=None, changed=None, change_state_known=False)]
        result["stage"] = "native_domain_dispatch"
        if spec["mutation"]: state.checkpoint(result)
        try:
            native = api.invoke(request)
        except Exception as error:
            if getattr(error, "native_call_returned", False):
                result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
            if getattr(error, "native_call_dispatched", None) is False:
                result["data"].update(native_dispatched=False, mutation_state_unknown=False)
                result["changes"] = []
            if spec["mutation"]: state.checkpoint(result)
            raise
        _adopt(result, native, request, spec["mutation"])
        if spec["mutation"]: state.checkpoint(result)
        if not result["success"]:
            raise OperationError(result["error_code"] or "domain_native_failed", result["error"] or "Native domain operation failed; original receipt preserved")
        return result

    if spec["mutation"]:
        return state.mutate(name, arguments, operation_id or None, action)
    result = state.new_result()
    try: return action(result)
    except Exception as error: return state.fail(result, error)


def _configure_pcg_counts(graph_path, node_name, element_counts, expected_current, dry_run):
    keys = {'num_elements', 'num_elements_x', 'num_elements_y', 'num_elements_z'}
    if not isinstance(graph_path, str) or not re.fullmatch('/Game/_MCP_Temp/(?:[A-Za-z0-9_]+/)*[A-Za-z0-9_]+', graph_path):
        return tool_error('A temp graph package path is required', error_code='ue58_pcg_scope')
    if not isinstance(node_name, str) or not node_name or len(node_name) > 256:
        return tool_error('Exact node_name is required', error_code='ue58_pcg_node')
    if not isinstance(dry_run, bool) or not isinstance(element_counts, dict) or (not element_counts) or (not set(element_counts) <= keys):
        return tool_error('Nonempty allowlisted element_counts and boolean dry_run required', error_code='ue58_pcg_counts')
    if any((type(v) is not int or not 1 <= v <= 16777216 for v in element_counts.values())):
        return tool_error('Counts must be integers in 1..16777216', error_code='ue58_pcg_counts')
    if not dry_run and (not isinstance(expected_current, dict) or set(expected_current) != keys or any((type(v) is not int for v in expected_current.values()))):
        return tool_error('Apply requires all four expected_current counts from dry-run', error_code='ue58_pcg_expected')
    return _native('pcg_custom_hlsl_elements', asset_path=graph_path, node_name=node_name, counts=element_counts, expected=expected_current, dry_run=dry_run)

def _request_configure_pcg_custom_hlsl_elements(graph_path, node_name, element_counts, expected_current=None, dry_run=True):
    """Set UE5.8 Custom HLSL element counts on one temp node. Dry-run first; apply needs all before values. No compile, generate or save."""
    return _configure_pcg_counts(graph_path, node_name, element_counts, expected_current, dry_run)

def _request_inspect_niagara_emitter_execution_modes(system_path, max_results=128):
    """Read standard/stateless emitter modes and lightweight emitter module classes. Includes disabled handles; no compile or save."""
    return _native('niagara_emitter_execution', asset_path=system_path, max_results=max_results)

def _request_inspect_controlrig_physics_units(control_rig_path, max_results=128):
    """Inspect Physics/Dynamics-named local graph nodes in legacy/runtime/editor ControlRig assets; external function bodies are not expanded."""
    return _native('controlrig_physics_units', asset_path=control_rig_path, max_results=max_results)

def _request_inspect_anim_blueprint_compiled_layout(blueprint_path, max_results=128):
    """Read generated anim-node property/runtime indices and baked state-machine indices without compiling the Blueprint."""
    return _native('animbp_compiled_layout', asset_path=blueprint_path, max_results=max_results)

def _request_inspect_chaos_body_solver_overrides(actor_path, component_name):
    """Read raw/effective body iterations, partial island sleep and concurrency hash. Editor world, single unwelded non-skinned/non-instanced body only."""
    return _native('chaos_body', actor_path=actor_path, component_name=component_name)

def _request_configure_chaos_body_solver_overrides(actor_path, component_name, settings, expected_state_hash='', dry_run=True):
    """Guarded native body solver setters. Apply only in editor temp maps with matching hash; never saves. Partial island sleep affects the body's island; runtime delivery is not measured."""
    return _native('chaos_body_configure', actor_path=actor_path, component_name=component_name, settings=settings, expected_state_hash=expected_state_hash, dry_run=dry_run)

def _request_inspect_chaos_cloth_solver_properties(actor_path, component_name, max_results=128):
    """Read UE5.8 solver-level cloth property collections, separately from per-asset/model properties. Empty collections are reported as empty."""
    return _native('cloth_solver', actor_path=actor_path, component_name=component_name, max_results=max_results)

def _request_inspect_chaos_cloth_snapshot_history(dataflow_path, node_name, max_results=128):
    """Read native editable-node snapshot history: active selection, lock, dates, weight/topology counts. Does not select, delete, regenerate or save."""
    return _native('cloth_snapshots', asset_path=dataflow_path, node_name=node_name, max_results=max_results)

def _request_inspect_controlrig_physics_unit_catalog():
    """Read installed nondeprecated Physics/Dynamics RigVM units and pin defaults. The relevant engine plugins must be enabled independently of MCP."""
    return _rig_native('catalog')

def _request_build_controlrig_physics_graph_variant(source_rig_path, target_rig_path, nodes, links, dry_run=True):
    """Clone a saved independent runtime/legacy rig into an unused temp path; add current Physics/Dynamics units, defaults and reviewed links through the existing authoring module's controller. Compile/hierarchy/node checks; no source mutation/save or unsafe factory creation."""
    return _rig_native('recipe', source_path=source_rig_path, target_path=target_rig_path, nodes=nodes, links=links, dry_run=dry_run)

def _request_inspect_anim_blueprint_authoring_nodes():
    """Read the installed allowlisted AnimGraph node classes and typed runtime property defaults for native recipe authoring."""
    return _native('authoring.animbp.catalog')

def _request_build_anim_blueprint_graph_variant(source_blueprint_path, target_blueprint_path, nodes, links, graph_name='AnimGraph', replace_root=False, dry_run=True):
    """Build a native AnimGraph recipe on an unused temp copy. Guard callback identity/ownership and verify authored settings/assets/logical links after compilation/publication. source_unchanged is null; scoped source identity and clean-package observations are reported separately. No save."""
    return _native('authoring.animbp.recipe', source_path=source_blueprint_path, target_path=target_blueprint_path, nodes=nodes, links=links, graph_name=graph_name, replace_root=replace_root, dry_run=dry_run)

def _request_inspect_niagara_stateless_authoring(system_path):
    """Read stateless emitter spawn-info and engine module names/export-text properties for variant authoring; no compile/save."""
    return _native('authoring.niagara.inspect', asset_path=system_path)

def _request_create_niagara_stateless_system(target_system_path, dry_run=True):
    """Create an unused temp system through the stock editor's combined standard/stateless setup. Manages engine-requested compile, retains pending candidates with do_not_retry, never saves."""
    return _native('authoring.niagara.create', target_path=target_system_path, dry_run=dry_run)

def _request_build_niagara_stateless_variant(source_system_path, target_system_path, emitters, dry_run=True):
    """Duplicate a saved independent system into an unused temp path and edit stateless spawn_infos/module fields. Exact emitter/module names, engine compilation, source unchanged and no save. Dry-run is path/source preflight only."""
    return _native('authoring.niagara.variant', source_path=source_system_path, target_path=target_system_path, emitters=emitters, dry_run=dry_run)

def _request_configure_pcg_manual_inspection(actor_path, component_name, enabled=True, dry_run=True):
    """Acquire/release this subsystem's transient PCG inspection reference in a temp map. Does not remove another editor's reference or generate/save content."""
    return _native('authoring.pcg.inspection', actor_path=actor_path, component_name=component_name, enabled=enabled, dry_run=dry_run)

def _request_inspect_pcg_manual_override_targets(actor_path, component_name):
    """Read exact executed stack/pin storage keys, point samples, existing engine deltas and concurrency hashes. Enable inspection and generate first."""
    return _native('authoring.pcg.targets', actor_path=actor_path, component_name=component_name)

def _request_edit_pcg_manual_point_override(actor_path, component_name, target_key, action, data_index=0, point_index=-1, transform='', delta_key='', expected_state_hash='', dry_run=True):
    """Author engine transform/offset/insert deltas or hide a point with the stock deletion delta (zero scale, count preserved); remove/clear deltas. Hash-bound idle temp component, explicit regeneration, no save."""
    return _native('authoring.pcg.edit', actor_path=actor_path, component_name=component_name, target_key=target_key, action=action, data_index=data_index, point_index=point_index, transform=transform, delta_key=delta_key, expected_state_hash=expected_state_hash, dry_run=dry_run)

def _request_inspect_chaos_cloth_dataflow_node_catalog():
    """Read installed current Cloth Dataflow node types, exact pin names/types and property export text."""
    return _native('authoring.cloth.graph_catalog')

def _request_build_chaos_cloth_dataflow_graph(target_dataflow_path, nodes, links, source_dataflow_path='', dry_run=True):
    """Build a bounded native Cloth Dataflow graph at an unused temp path. Current node types, export-text settings, saved mesh asset_bindings and exact links. Optional clean source clone; no source mutation, evaluation or save. Dry-run is path/source/type preflight."""
    return _native('authoring.cloth.graph_recipe', target_path=target_dataflow_path, source_path=source_dataflow_path, nodes=nodes, links=links, dry_run=dry_run)

def _request_create_chaos_cloth_asset_from_template(target_cloth_path, template_dataflow_path, evaluate=False, dry_run=True):
    """Use stock UE5.8 non-modal Cloth factory for an unused temp ClothAsset with owned embedded Dataflow. Optional native evaluation returns LOD mesh counts; no save or simulation success claim."""
    return _native('authoring.cloth.create_asset', target_path=target_cloth_path, template_path=template_dataflow_path, evaluate=evaluate, dry_run=dry_run)

def _request_inspect_chaos_cloth_snapshot_state(dataflow_path, node_name):
    """Read bounded Cloth snapshot metadata and a full serialized-data state hash for guarded history edits."""
    return _native('authoring.cloth.snapshots', asset_path=dataflow_path, node_name=node_name)

def _request_edit_chaos_cloth_snapshot_history(dataflow_path, node_name, action, index, name='', locked=False, expected_state_hash='', dry_run=True):
    """Select/delete/rename/lock/duplicate Cloth snapshots on a temp Dataflow node. Dry-run exposes eviction; locked deletion/history growth refused; native rollback and no save."""
    return _native('authoring.cloth.snapshots', asset_path=dataflow_path, node_name=node_name, action=action, index=index, name=name, locked=locked, expected_state_hash=expected_state_hash, dry_run=dry_run)

def _request_inspect_chaos_body_collision(actor_path, component_name):
    """Read one unwelded editor body's collision, CCD, gravity, damping, mass scale and state hash."""
    return _native('authoring.physics.body', actor_path=actor_path, component_name=component_name)

def _request_configure_chaos_body_collision(actor_path, component_name, settings, expected_state_hash='', dry_run=True):
    """Native collision/CCD/gravity/damping/mass setters. Exact hash and temp map required for apply; transaction/readback/rollback, no save or simulate-physics toggles."""
    return _native('authoring.physics.body', actor_path=actor_path, component_name=component_name, settings=settings, expected_state_hash=expected_state_hash, dry_run=dry_run)

def _request_inspect_chaos_constraint_profile(actor_path='', component_name='', physics_asset_path='', joint_name=''):
    """Read constraint profile export-text fields and full profile state hash. Select exactly one editor component or PhysicsAsset/joint."""
    if physics_asset_path:
        if actor_path or component_name or (not joint_name):
            return tool_error('Select either component or PhysicsAsset/joint', error_code='ue58_constraint_selector')
        return _native('authoring.physics.asset_constraint', asset_path=physics_asset_path, joint_name=joint_name)
    if joint_name or not actor_path or (not component_name):
        return tool_error('Exact actor_path/component_name required', error_code='ue58_constraint_selector')
    return _native('authoring.physics.constraint', actor_path=actor_path, component_name=component_name)

def _request_configure_chaos_constraint_profile(actor_path, component_name, settings, expected_state_hash='', dry_run=True):
    """Patch constraint limits, drives, break/plasticity/projection fields using native CopyProfilePropertiesFrom. Export-text values, hash-bound temp map only; no save."""
    return _native('authoring.physics.constraint', actor_path=actor_path, component_name=component_name, settings=settings, expected_state_hash=expected_state_hash, dry_run=dry_run)

def _request_configure_physics_asset_constraint_profile(physics_asset_path, joint_name, settings, expected_state_hash='', dry_run=True):
    """Patch one exactly named, asset-owned PhysicsAsset constraint template. Temp asset only; never edits an external shared template or saves."""
    return _native('authoring.physics.asset_constraint', asset_path=physics_asset_path, joint_name=joint_name, settings=settings, expected_state_hash=expected_state_hash, dry_run=dry_run)

def configure_pcg_custom_hlsl_elements(graph_path, node_name, element_counts, expected_current=None, dry_run=True, operation_id=""):
    return _execute('configure_pcg_custom_hlsl_elements', {'graph_path': graph_path, 'node_name': node_name, 'element_counts': element_counts, 'expected_current': expected_current, 'dry_run': dry_run}, operation_id)


def inspect_niagara_emitter_execution_modes(system_path, max_results=128):
    return _execute('inspect_niagara_emitter_execution_modes', {'system_path': system_path, 'max_results': max_results}, None)


def inspect_controlrig_physics_units(control_rig_path, max_results=128):
    return _execute('inspect_controlrig_physics_units', {'control_rig_path': control_rig_path, 'max_results': max_results}, None)


def inspect_anim_blueprint_compiled_layout(blueprint_path, max_results=128):
    return _execute('inspect_anim_blueprint_compiled_layout', {'blueprint_path': blueprint_path, 'max_results': max_results}, None)


def inspect_chaos_body_solver_overrides(actor_path, component_name):
    return _execute('inspect_chaos_body_solver_overrides', {'actor_path': actor_path, 'component_name': component_name}, None)


def configure_chaos_body_solver_overrides(actor_path, component_name, settings, expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('configure_chaos_body_solver_overrides', {'actor_path': actor_path, 'component_name': component_name, 'settings': settings, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)


def inspect_chaos_cloth_solver_properties(actor_path, component_name, max_results=128):
    return _execute('inspect_chaos_cloth_solver_properties', {'actor_path': actor_path, 'component_name': component_name, 'max_results': max_results}, None)


def inspect_chaos_cloth_snapshot_history(dataflow_path, node_name, max_results=128):
    return _execute('inspect_chaos_cloth_snapshot_history', {'dataflow_path': dataflow_path, 'node_name': node_name, 'max_results': max_results}, None)


def inspect_controlrig_physics_unit_catalog():
    return _execute('inspect_controlrig_physics_unit_catalog', {}, None)


def build_controlrig_physics_graph_variant(source_rig_path, target_rig_path, nodes, links, dry_run=True, operation_id=""):
    return _execute('build_controlrig_physics_graph_variant', {'source_rig_path': source_rig_path, 'target_rig_path': target_rig_path, 'nodes': nodes, 'links': links, 'dry_run': dry_run}, operation_id)


def inspect_anim_blueprint_authoring_nodes():
    return _execute('inspect_anim_blueprint_authoring_nodes', {}, None)


def build_anim_blueprint_graph_variant(source_blueprint_path, target_blueprint_path, nodes, links, graph_name='AnimGraph', replace_root=False, dry_run=True, operation_id=""):
    return _execute('build_anim_blueprint_graph_variant', {'source_blueprint_path': source_blueprint_path, 'target_blueprint_path': target_blueprint_path, 'nodes': nodes, 'links': links, 'graph_name': graph_name, 'replace_root': replace_root, 'dry_run': dry_run}, operation_id)


def inspect_niagara_stateless_authoring(system_path):
    """Observe authoring_status without advancing/replaying writes; inspect nested terminal outcome."""
    return _execute('inspect_niagara_stateless_authoring', {'system_path': system_path}, None)


def create_niagara_stateless_system(target_system_path, dry_run=True, operation_id=""):
    """Initiate frame-based native authoring; accepted does not mean complete. Inspect, never replay."""
    return _execute('create_niagara_stateless_system', {'target_system_path': target_system_path, 'dry_run': dry_run}, operation_id)


def build_niagara_stateless_variant(source_system_path, target_system_path, emitters, dry_run=True, operation_id=""):
    """Initiate frame-based variant; phase deadline is 10s. Inspect completion, never replay."""
    return _execute('build_niagara_stateless_variant', {'source_system_path': source_system_path, 'target_system_path': target_system_path, 'emitters': emitters, 'dry_run': dry_run}, operation_id)


def configure_pcg_manual_inspection(actor_path, component_name, enabled=True, dry_run=True, operation_id=""):
    return _execute('configure_pcg_manual_inspection', {'actor_path': actor_path, 'component_name': component_name, 'enabled': enabled, 'dry_run': dry_run}, operation_id)


def inspect_pcg_manual_override_targets(actor_path, component_name):
    return _execute('inspect_pcg_manual_override_targets', {'actor_path': actor_path, 'component_name': component_name}, None)


def edit_pcg_manual_point_override(actor_path, component_name, target_key, action, data_index=0, point_index=-1, transform='', delta_key='', expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('edit_pcg_manual_point_override', {'actor_path': actor_path, 'component_name': component_name, 'target_key': target_key, 'action': action, 'data_index': data_index, 'point_index': point_index, 'transform': transform, 'delta_key': delta_key, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)


def inspect_chaos_cloth_dataflow_node_catalog():
    return _execute('inspect_chaos_cloth_dataflow_node_catalog', {}, None)


def build_chaos_cloth_dataflow_graph(target_dataflow_path, nodes, links, source_dataflow_path='', dry_run=True, operation_id=""):
    return _execute('build_chaos_cloth_dataflow_graph', {'target_dataflow_path': target_dataflow_path, 'nodes': nodes, 'links': links, 'source_dataflow_path': source_dataflow_path, 'dry_run': dry_run}, operation_id)


def create_chaos_cloth_asset_from_template(target_cloth_path, template_dataflow_path, evaluate=False, dry_run=True, operation_id=""):
    return _execute('create_chaos_cloth_asset_from_template', {'target_cloth_path': target_cloth_path, 'template_dataflow_path': template_dataflow_path, 'evaluate': evaluate, 'dry_run': dry_run}, operation_id)


def inspect_chaos_cloth_snapshot_state(dataflow_path, node_name):
    return _execute('inspect_chaos_cloth_snapshot_state', {'dataflow_path': dataflow_path, 'node_name': node_name}, None)


def edit_chaos_cloth_snapshot_history(dataflow_path, node_name, action, index, name='', locked=False, expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('edit_chaos_cloth_snapshot_history', {'dataflow_path': dataflow_path, 'node_name': node_name, 'action': action, 'index': index, 'name': name, 'locked': locked, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)


def inspect_chaos_body_collision(actor_path, component_name):
    return _execute('inspect_chaos_body_collision', {'actor_path': actor_path, 'component_name': component_name}, None)


def configure_chaos_body_collision(actor_path, component_name, settings, expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('configure_chaos_body_collision', {'actor_path': actor_path, 'component_name': component_name, 'settings': settings, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)


def inspect_chaos_constraint_profile(actor_path='', component_name='', physics_asset_path='', joint_name=''):
    return _execute('inspect_chaos_constraint_profile', {'actor_path': actor_path, 'component_name': component_name, 'physics_asset_path': physics_asset_path, 'joint_name': joint_name}, None)


def configure_chaos_constraint_profile(actor_path, component_name, settings, expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('configure_chaos_constraint_profile', {'actor_path': actor_path, 'component_name': component_name, 'settings': settings, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)


def configure_physics_asset_constraint_profile(physics_asset_path, joint_name, settings, expected_state_hash='', dry_run=True, operation_id=""):
    return _execute('configure_physics_asset_constraint_profile', {'physics_asset_path': physics_asset_path, 'joint_name': joint_name, 'settings': settings, 'expected_state_hash': expected_state_hash, 'dry_run': dry_run}, operation_id)
