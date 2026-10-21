"""Original RigVM pin-default writer11 on official Registry; carriers are private."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import rigvm_pin_default_writer as pin_default_writer

@unreal.uclass()
class CubelessRigVMPinDefaultWriterTools(unreal.ToolsetDefinition):
    """Apply reviewed pin defaults with Native target and compound-write guards."""

    @toolset_registry.tool_call
    @staticmethod
    def set_controlrig_rigvm_pin_defaults(control_rig_path: str = "", control_rig_asset_path: str = "",
            control_rig_class_path: str = "", pin_defaults: str = "null", defaults: str = "null",
            resize_arrays: bool = False, set_value_on_linked_pins: bool = False, dry_run: bool = False,
            compile: bool = True, save_asset: bool = False, max_pins: str = "64") -> CubelessPythonOperationResult:
        """Set reviewed ControlRig RigVM pin defaults through the Native guarded writer. Public pin_defaults/defaults are optional arrays of Any values with null defaults. Native selects and validates proposals, requires a proposed value for every requested pin, and owns all row aliases, explicit-null/omitted-value, array resize, linked-pin propagation, idempotence and partial-failure decisions. Original sample/temp restrictions and the dedicated current /Game/CubelessMCPTests verification root are enforced in Native and are not broadened by this adapter. Public defaults are empty paths, resize_arrays=False, set_value_on_linked_pins=False, dry_run=False, compile=True, save_asset=False and max_pins=64. All five flags require exact JSON booleans. Full Native change/unknown, compile/save and failure-prefix state reaches the parent journal before errors; uncertain mutations are not retried and atomic rollback is not promised. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
        rejected, report, raw = _pin_default_writer_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            pin_arrays = _decode_nullable_arrays(pin_defaults, defaults)
            maximum = _decode_max_pins(max_pins)
        except Exception as carrier_error:
            result = pin_default_writer._rejected(pin_default_writer.OperationError("rigvm_pin_default_writer_carrier_invalid", str(carrier_error)))
            result["stage"] = "rigvm_pin_default_writer_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(pin_default_writer.set_controlrig_rigvm_pin_defaults(control_rig_path, control_rig_asset_path,
            control_rig_class_path, pin_arrays[0], pin_arrays[1], resize_arrays, set_value_on_linked_pins, dry_run, compile, save_asset, maximum))


def _pin_default_writer_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessRigVMPinDefaultWriterAttachmentLibrary", None), "verify_rigvm_pin_default_writer_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact RigVM pin-default writer raw-input admission helper unavailable")
        raw = helper(CubelessRigVMPinDefaultWriterTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_rigvm_pin_default_writer_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "set_controlrig_rigvm_pin_defaults"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded pin-default writer delegation required; no Native writer dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = pin_default_writer._rejected(pin_default_writer.OperationError("rigvm_pin_default_writer_raw_guard_admission_missing", message))
    result["stage"] = "rigvm_pin_default_writer_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant is unsupported: " + value)


def _decode_nullable_arrays(*values):
    """Restore None versus [] without inspecting row shape, aliases or the ignored tail."""
    if len(values) != 2:
        raise ValueError("Exactly two nullable pin-default array carriers required")
    arrays = []
    for text in values:
        if type(text) is not str or len(text.encode("utf-16-le")) // 2 > 1048576:
            raise ValueError("Pin-default array carrier requires at most1Mi UE UTF16 units")
        value = json.loads(text, parse_constant=_invalid_constant)
        if value is not None and type(value) is not list:
            raise ValueError("Pin-default array carrier must decode as null or an array of Any values")
        arrays.append(value)
    return arrays


def _decode_max_pins(text):
    """Decode only the private canonical int64 decimal; Native retains the original clamp."""
    if type(text) is not str or not text or len(text) > 20:
        raise ValueError("Private max_pins requires a bounded canonical decimal")
    value = int(text)
    if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
        raise ValueError("Private max_pins must be an exact canonical int64 decimal")
    return value
