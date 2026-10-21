"""UE58 default RigVM topology11 bindings; no loader or graph work in Python."""
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessRigVMTopologyLibrary"
METHOD = "inspect_controlrig_rigvm_topology"
BINDINGS = (METHOD,)


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def inspect_controlrig_rigvm_topology(control_rig_path="", control_rig_asset_path="", control_rig_class_path="",
        include_pins=True, include_sub_pins=True, include_links=True, include_pin_links=False,
        max_nodes=256, max_pins_per_node=64, max_sub_pins_per_pin=32, max_links=512):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("rigvm_topology_native_api_unavailable", "UE58 RigVM topology binding is unavailable; no fallback executed")
    raw = fn(control_rig_path, control_rig_asset_path, control_rig_class_path,
        include_pins, include_sub_pins, include_links, include_pin_links,
        max_nodes, max_pins_per_node, max_sub_pins_per_pin, max_links)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("rigvm_topology_native_result_invalid", "Native RigVM topology operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("rigvm_topology_native_result_invalid", "Native RigVM topology changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
