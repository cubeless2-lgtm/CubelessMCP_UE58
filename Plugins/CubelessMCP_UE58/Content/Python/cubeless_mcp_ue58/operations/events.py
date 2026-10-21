"""Named event/input contracts and a separately identified parent-call extension."""

from cubeless_mcp_ue58.compatibility.ue58 import events as api
from . import functions, state
from .state import OperationError


def _arguments(blueprint_name, selector_name, selector, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    if functions._bool(create_graph_if_missing, "create_graph_if_missing"):
        raise OperationError("event_graph_creation_unsupported", "Resolve an existing event graph; implicit graph creation is not implemented")
    position = functions._json(node_position_json, "node_position_json")
    if position is None:
        position = [0, 0]
    if type(position) is not list or len(position) != 2 or any(type(v) is not int or abs(v) > 1000000 for v in position):
        raise OperationError("event_position_invalid", "node_position_json requires two integers from -1000000 to 1000000")
    return {**functions._selectors(blueprint_name, graph_name, graph_id, graph_type),
        selector_name: functions._text(selector, selector_name), "x": position[0], "y": position[1]}


def _run(result, identity, arguments, checkpoint_callback=None):
    if identity == "add_blueprint_custom_event_node" and arguments.get("custom_event_signature_route") is True:
        from . import custom_event_signatures
        return custom_event_signatures._run_action(result, arguments, checkpoint_callback)
    result["data"].update(full_legacy_event_parity=False,
        legacy_public_tool=identity in ("add_blueprint_event_node", "add_blueprint_custom_event_node", "add_blueprint_input_key_node"),
        parent_call_is_new_extension=identity == "add_blueprint_parent_function_call")
    if identity == "add_blueprint_event_node" and arguments.get("event_name") == "ReceiveParticleData":
        result["data"].update(finite_particle_callback_route=True, callback_interface_added=False,
            automatic_wiring=False, hardware_or_niagara_transport_verified=False)
    functions._run_action(result, identity, arguments, checkpoint_callback, operation_api=api)


def _prepared(identity, arguments, operation_id):
    try:
        values = arguments()
    except Exception as error:
        return state.fail(state.new_result(operation_id), error)
    return state.mutate(identity, values, operation_id, lambda result: _run(result, identity, values))


def add_blueprint_event_node(blueprint_name, event_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_event_node", lambda: _arguments(blueprint_name, "event_name", event_name,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_event_node_action(result, blueprint_name, event_name, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_event_node", _arguments(blueprint_name, "event_name", event_name,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def add_blueprint_parent_function_call(blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_parent_function_call", lambda: _arguments(blueprint_name, "event_node_id", event_node_id,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_parent_function_call_action(result, blueprint_name, event_node_id, node_position_json="[0,0]", graph_name=None,
        graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_parent_function_call", _arguments(blueprint_name, "event_node_id", event_node_id,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def _custom_arguments(blueprint_name, custom_event_name, node_position_json,
        signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
        native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing):
    if any(value is not None and value != "" for value in (signature_source_dispatcher_name,
            signature_source_class, signature_source_delegate_name, native_delegate_name)):
        from . import custom_event_signatures
        return custom_event_signatures._adapted_arguments(blueprint_name, custom_event_name, node_position_json,
            signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
            native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing)
    for name, value in (("signature_source_dispatcher_name", signature_source_dispatcher_name),
            ("signature_source_class", signature_source_class),
            ("signature_source_delegate_name", signature_source_delegate_name), ("native_delegate_name", native_delegate_name)):
        if functions._text("" if value is None else value, name, empty=True):
            raise OperationError("custom_event_signature_unsupported", "Only no-argument custom events are implemented; dispatcher/delegate signatures are not dispatched")
    args = _arguments(blueprint_name, "custom_event_name", custom_event_name, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)
    args.update(call_in_editor=False if call_in_editor is None else functions._bool(call_in_editor, "call_in_editor"),
                has_call_in_editor=call_in_editor is not None)
    return args


def add_blueprint_custom_event_node(blueprint_name, custom_event_name, node_position_json="[0,0]",
        signature_source_dispatcher_name=None, signature_source_class=None, signature_source_delegate_name=None,
        native_delegate_name=None, call_in_editor=None, graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_custom_event_node", lambda: _custom_arguments(blueprint_name, custom_event_name,
        node_position_json, signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
        native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_custom_event_node_action(result, blueprint_name, custom_event_name, node_position_json="[0,0]",
        signature_source_dispatcher_name=None, signature_source_class=None, signature_source_delegate_name=None,
        native_delegate_name=None, call_in_editor=None, graph_name=None, graph_id=None, graph_type=None,
        create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_custom_event_node", _custom_arguments(blueprint_name, custom_event_name,
        node_position_json, signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
        native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)


def _input_arguments(blueprint_name, key, node_position_json, control, alt, shift, command,
        consume_input, execute_when_paused, override_parent_binding, graph_name, graph_id, graph_type, create_graph_if_missing):
    args = _arguments(blueprint_name, "key", key, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)
    args.update({name: functions._bool(value, name) for name, value in (
        ("control", control), ("alt", alt), ("shift", shift), ("command", command),
        ("consume_input", consume_input), ("execute_when_paused", execute_when_paused),
        ("override_parent_binding", override_parent_binding))})
    return args


def add_blueprint_input_key_node(blueprint_name, key, node_position_json="[0,0]", control=False, alt=False,
        shift=False, command=False, consume_input=True, execute_when_paused=False, override_parent_binding=False,
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, operation_id=None):
    return _prepared("add_blueprint_input_key_node", lambda: _input_arguments(blueprint_name, key, node_position_json,
        control, alt, shift, command, consume_input, execute_when_paused, override_parent_binding,
        graph_name, graph_id, graph_type, create_graph_if_missing), operation_id)


def add_blueprint_input_key_node_action(result, blueprint_name, key, node_position_json="[0,0]", control=False, alt=False,
        shift=False, command=False, consume_input=True, execute_when_paused=False, override_parent_binding=False,
        graph_name=None, graph_id=None, graph_type=None, create_graph_if_missing=False, checkpoint_callback=None):
    _run(result, "add_blueprint_input_key_node", _input_arguments(blueprint_name, key, node_position_json,
        control, alt, shift, command, consume_input, execute_when_paused, override_parent_binding,
        graph_name, graph_id, graph_type, create_graph_if_missing), checkpoint_callback)
