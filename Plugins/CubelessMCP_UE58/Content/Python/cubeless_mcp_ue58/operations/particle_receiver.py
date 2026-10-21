"""Specific receiver contracts, sharing mutation accounting without a generic executor."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import particle_receiver as api
from . import events, functions, state
from .state import OperationError

_NO_EVENT = object()


def _interface_arguments(blueprint_name, interface_class):
    interface = functions._text(interface_class, "interface_class")
    return {"blueprint_path": functions._selectors(blueprint_name, None, None, None)["blueprint_path"],
            "interface_class": interface}


def _node_arguments(blueprint_name, node_position_json, graph_name, graph_id, graph_type, event_node_id=_NO_EVENT):
    args = events._arguments(blueprint_name, "receiver_scope", "BasicParticleData", node_position_json,
        graph_name, graph_id, graph_type, False)
    del args["receiver_scope"]
    if event_node_id is not _NO_EVENT:
        args["event_node_id"] = functions._text(event_node_id, "event_node_id")
    return args


def _run(result, identity, arguments, checkpoint_callback=None):
    result["data"].update(full_legacy_particle_receiver_parity=False,
        legacy_public_tool=identity == "add_blueprint_interface",
        explicit_create_and_wire_extension=identity in ("add_particle_data_array_length_and_wire", "add_particle_data_array_get_copy_and_wire"),
        hardware_or_niagara_transport_verified=False)
    functions._run_action(result, identity, arguments, checkpoint_callback, operation_api=api)


def _prepared(identity, prepare, operation_id):
    try:
        arguments = prepare()
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(identity, arguments, operation_id, lambda result: _run(result, identity, arguments))


def add_blueprint_interface(blueprint_name, interface_class, operation_id=None):
    return _prepared("add_blueprint_interface", lambda: _interface_arguments(blueprint_name, interface_class), operation_id)


def add_blueprint_interface_action(result, blueprint_name, interface_class, checkpoint_callback=None):
    _run(result, "add_blueprint_interface", _interface_arguments(blueprint_name, interface_class), checkpoint_callback)


def inspect_particle_receiver_action(result, blueprint_name, graph_name=None, graph_id=None, graph_type=None, checkpoint_callback=None):
    arguments = functions._selectors(blueprint_name, graph_name, graph_id, graph_type)
    result["data"].update(operation="inspect_particle_receiver", resolved_blueprint_path=arguments["blueprint_path"],
        journaled=checkpoint_callback is not None, loading_side_effects_verified=False, adapter_preloaded_asset=False,
        full_legacy_particle_receiver_parity=False, hardware_or_niagara_transport_verified=False)
    result["stage"] = "native_particle_receiver_read"
    result["data"].update(native_dispatched=True, mutation_state_unknown=True)
    result["changes"] = [{"kind": "particle_receiver_read", "asset_path": arguments["blueprint_path"],
        "attempted": None, "changed": None, "change_state_known": False, "outcome": "unknown_until_native_result"}]
    try:
        native = api.inspect_particle_receiver(**arguments)
    except Exception as error:
        if getattr(error, "code", "") == "native_function_api_unavailable":
            result["data"].update(native_dispatched=False, native_mutation_attempted=False,
                mutation_state_unknown=False, mutation_performed=False)
            result["changes"] = []
        raise
    result["data"].update(copy.deepcopy(native["data"]))
    result["data"].update(native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key != "data"},
        native_mutation_attempted=native["mutation_attempted"], mutation_state_unknown=not native["change_state_known"],
        mutation_performed=native.get("changed") if native["change_state_known"] else None)
    result["stage"] = native.get("stage", "native_particle_receiver_read")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
    if native["change_state_known"] and native.get("changed") is False:
        result["changes"] = []
    else:
        result["changes"][0].update(attempted=native["mutation_attempted"], changed=native.get("changed") if native["change_state_known"] else None,
            change_state_known=native["change_state_known"], outcome="native_read_observation")
    if native["mutation_attempted"] or native.get("changed") is True or any(native[key] for key in (
            "compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")) or native["compiled"] is True:
        raise OperationError("particle_receiver_read_changed_state", "Read-only receiver inspection reported changes or explicit compiler/save work; observations retained")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "particle_receiver_read_failed", native.get("error") or "Native receiver read failed")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("particle_receiver_read_unverified", "Native receiver read lacks known preserved postconditions")
    result["validation"].update(status="native_particle_receiver_read_postcondition", native_postcondition_passed=True,
        disk_reload="not_run", packaged_runtime="not_run")
    result.update(success=True, stage="complete")


def inspect_particle_receiver(blueprint_name, graph_name=None, graph_id=None, graph_type=None):
    result = state.new_result()
    try:
        inspect_particle_receiver_action(result, blueprint_name, graph_name, graph_id, graph_type)
    except Exception as error:
        state.fail(result, error)
    return result


def add_particle_data_array_length_and_wire(blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, operation_id=None):
    return _prepared("add_particle_data_array_length_and_wire", lambda: _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type, event_node_id), operation_id)


def add_particle_data_array_length_and_wire_action(result, blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, checkpoint_callback=None):
    _run(result, "add_particle_data_array_length_and_wire", _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type, event_node_id), checkpoint_callback)


def add_particle_data_array_get_copy_and_wire(blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, operation_id=None):
    return _prepared("add_particle_data_array_get_copy_and_wire", lambda: _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type, event_node_id), operation_id)


def add_particle_data_array_get_copy_and_wire_action(result, blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, checkpoint_callback=None):
    _run(result, "add_particle_data_array_get_copy_and_wire", _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type, event_node_id), checkpoint_callback)


def add_basic_particle_data_break_node(blueprint_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, operation_id=None):
    return _prepared("add_basic_particle_data_break_node", lambda: _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type), operation_id)


def add_basic_particle_data_break_node_action(result, blueprint_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, checkpoint_callback=None):
    _run(result, "add_basic_particle_data_break_node", _node_arguments(blueprint_name, node_position_json,
        graph_name, graph_id, graph_type), checkpoint_callback)
