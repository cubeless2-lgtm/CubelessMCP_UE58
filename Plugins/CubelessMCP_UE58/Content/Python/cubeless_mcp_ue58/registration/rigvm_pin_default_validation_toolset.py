"""Original RigVM pin-default validation7 on official Registry; carriers are private."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import rigvm_pin_default_validation as pin_default_validation

@unreal.uclass()
class CubelessRigVMPinDefaultValidationTools(unreal.ToolsetDefinition):
    """Diagnose bounded pin default proposals in the default ControlRig RigVM model."""

    @toolset_registry.tool_call
    @staticmethod
    def validate_controlrig_rigvm_pin_defaults(control_rig_path: str = "", control_rig_asset_path: str = "",
            control_rig_class_path: str = "", pin_defaults: str = "null", defaults: str = "null",
            pins: str = "null", max_pins: str = "64") -> CubelessPythonOperationResult:
        """Validate ControlRig RigVM pin default feasibility without explicitly writing values, resizing arrays, compiling, saving, executing or solving. Public pin_defaults/defaults/pins are optional arrays of Any values with null defaults. Native selects the first array in that order, treats a selected empty array as an error, and checks only the clamped prefix. For a nonempty input max_pins=0 checks zero rows. Native preserves omitted proposed values versus explicit null and dictionary alias/coercion rules; this facade does not reinterpret them. Public paths default to empty strings and max_pins=64. Completed diagnostics can succeed while a pin is missing or a proposed default is invalid. SDK loads/getters/type validation may have unknown total effects, and complete actual state reaches the operation journal before errors. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
        rejected, report, raw = _pin_default_validation_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            pin_arrays = _decode_nullable_arrays(pin_defaults, defaults, pins)
            maximum = _decode_max_pins(max_pins)
        except Exception as carrier_error:
            result = pin_default_validation._rejected(pin_default_validation.OperationError("rigvm_pin_default_validation_carrier_invalid", str(carrier_error)))
            result["stage"] = "rigvm_pin_default_validation_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(pin_default_validation.validate_controlrig_rigvm_pin_defaults(control_rig_path, control_rig_asset_path,
            control_rig_class_path, pin_arrays[0], pin_arrays[1], pin_arrays[2], maximum))


def _pin_default_validation_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessRigVMPinDefaultValidationAttachmentLibrary", None), "verify_rigvm_pin_default_validation_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact RigVM pin-default validation raw-input admission helper unavailable")
        raw = helper(CubelessRigVMPinDefaultValidationTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_rigvm_pin_default_validation_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "validate_controlrig_rigvm_pin_defaults"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded pin-default validation delegation required; no Native validation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = pin_default_validation._rejected(pin_default_validation.OperationError("rigvm_pin_default_validation_raw_guard_admission_missing", message))
    result["stage"] = "rigvm_pin_default_validation_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant is unsupported: " + value)


def _decode_nullable_arrays(*values):
    """Restore None versus [] without inspecting row shape, aliases or the ignored tail."""
    if len(values) != 3:
        raise ValueError("Exactly three nullable pin-default array carriers required")
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
