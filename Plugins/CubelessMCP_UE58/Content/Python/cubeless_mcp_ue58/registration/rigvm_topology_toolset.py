"""Original topology11 on official Registry; decimal4 is a private reflection ABI."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import rigvm_topology as topology

@unreal.uclass()
class CubelessRigVMTopologyTools(unreal.ToolsetDefinition):
    """Inspect the default ControlRig RigVM model through original bounded options."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_controlrig_rigvm_topology(control_rig_path: str = "", control_rig_asset_path: str = "",
            control_rig_class_path: str = "", include_pins: bool = True, include_sub_pins: bool = True,
            include_links: bool = True, include_pin_links: bool = False, max_nodes: str = "256",
            max_pins_per_node: str = "64", max_sub_pins_per_pin: str = "32", max_links: str = "512") -> CubelessPythonOperationResult:
        """Inspect the ControlRig default RigVM topology using three optional string path aliases, four include flags and four bounded integer limits. Public limit fields are integers with defaults256/64/32/512; private decimal FString4 avoids the official Python int32 reflection limit. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate. Public paths default to empty strings; null is rejected. Resolve priority is control_rig_path, control_rig_asset_path, then control_rig_class_path. Native owns loading, exact target/model lifetime, projection order, truncation and dirty/change observations. This does not explicitly modify, compile, save or execute a rig; SDK loads/getters may have unknown effects. Full actual raw/partial state reaches one Named parent journal before errors. No automatic replay, atomic rollback, runtime Cubeless reference or complete external pipeline claim."""
        rejected, report, raw = _topology_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            limits = _decode_topology_limits(max_nodes, max_pins_per_node, max_sub_pins_per_pin, max_links)
        except Exception as carrier_error:
            result = topology._rejected(topology.OperationError("rigvm_topology_carrier_invalid", str(carrier_error)))
            result["stage"] = "rigvm_topology_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(topology.inspect_controlrig_rigvm_topology(control_rig_path, control_rig_asset_path,
            control_rig_class_path, include_pins, include_sub_pins, include_links, include_pin_links,
            limits[0], limits[1], limits[2], limits[3]))


def _topology_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessRigVMTopologyAttachmentLibrary", None), "verify_rigvm_topology_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact topology raw-input admission helper unavailable")
        raw = helper(CubelessRigVMTopologyTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_rigvm_topology_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "inspect_controlrig_rigvm_topology"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded topology delegation required; no Native inspection dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = topology._rejected(topology.OperationError("rigvm_topology_raw_guard_admission_missing", message))
    result["stage"] = "rigvm_topology_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _decode_topology_limits(*values):
    """Decode only canonical private int64 decimals; leave the original clamps in Native."""
    limits = []
    if len(values) != 4:
        raise ValueError("Exactly four private topology limits required")
    for text in values:
        if type(text) is not str or not text or len(text) > 20:
            raise ValueError("Private topology limit requires a bounded canonical decimal")
        value = int(text)
        if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
            raise ValueError("Private topology limit must be an exact canonical int64 decimal")
        limits.append(value)
    return limits
