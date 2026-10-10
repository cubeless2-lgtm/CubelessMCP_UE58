"""Named pin-default native bridge; caller owns graph selection/observations."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


def set_blueprint_pin_default(blueprint_path, graph_path, node_path, pin_name, value_json, direction):
    function = getattr(getattr(unreal, "CubelessGraphLibrary", None), "set_blueprint_pin_default", None)
    if not callable(function):
        raise OperationError("native_pin_api_unavailable", "Build/load the named pin-default helper; no generic property fallback executed")
    return _decode_result(function(graph_path, node_path, pin_name, value_json, direction))
