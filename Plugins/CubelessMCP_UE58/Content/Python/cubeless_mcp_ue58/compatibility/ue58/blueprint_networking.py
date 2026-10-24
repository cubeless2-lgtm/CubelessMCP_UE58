"""Original Inspect1/Set11 through exact native Blueprint networking methods.

Optional private JSON slots use empty strings solely for public None/omission.
Immutable Variable encoder preserves actual composite compiler/save prefixes.
"""
import unreal
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError

LIBRARY = 'CubelessBlueprintNetworkingLibrary'
BINDINGS = ('inspect_blueprint_networking_settings', 'set_blueprint_networking_settings')
HIDDEN_ARGUMENTS_BY_METHOD = {'inspect_blueprint_networking_settings': ('blueprint_name',), 'set_blueprint_networking_settings': ('blueprint_name', 'replicates_json', 'replicate_movement_json', 'net_dormancy_json', 'always_relevant_json', 'only_relevant_to_owner_json', 'net_update_frequency_json', 'min_net_update_frequency_json', 'net_priority_json', 'net_cull_distance_squared_json', 'dry_run')}


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(method, arguments, raw_input=False):
    if method not in BINDINGS:
        raise OperationError("blueprint_networking_method_invalid", "Only original networking inspect/set methods admitted")
    script_name = method + "_raw_input" if raw_input else method
    function = getattr(getattr(unreal, LIBRARY, None), script_name, None)
    if not callable(function):
        raise OperationError("blueprint_networking_native_api_unavailable", "Exact networking native binding unavailable; no dispatch")
    raw = function(*arguments)
    try:
        value = _decode_result(raw)
        if value.get("operation") != method:
            raise OperationError("invalid_native_blueprint_networking_result", "Returned operation differs; raw prefix retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value["_raw_native_json"] = raw
    return value


def inspect_blueprint_networking_settings_raw_input(params_json):
    return _invoke('inspect_blueprint_networking_settings', (params_json,), raw_input=True)


def set_blueprint_networking_settings_raw_input(params_json):
    return _invoke('set_blueprint_networking_settings', (params_json,), raw_input=True)


def inspect_blueprint_networking_settings(blueprint_name):
    return _invoke('inspect_blueprint_networking_settings', (blueprint_name,))

def set_blueprint_networking_settings(blueprint_name, replicates_json, replicate_movement_json, net_dormancy_json, always_relevant_json, only_relevant_to_owner_json, net_update_frequency_json, min_net_update_frequency_json, net_priority_json, net_cull_distance_squared_json, dry_run):
    return _invoke('set_blueprint_networking_settings', (blueprint_name, replicates_json, replicate_movement_json, net_dormancy_json, always_relevant_json, only_relevant_to_owner_json, net_update_frequency_json, min_net_update_frequency_json, net_priority_json, net_cull_distance_squared_json, dry_run))
