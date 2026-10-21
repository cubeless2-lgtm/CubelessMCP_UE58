"""Original RigVM link validation7 on official Registry; carriers are private."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import rigvm_link_validation as link_validation

@unreal.uclass()
class CubelessRigVMLinkValidationTools(unreal.ToolsetDefinition):
    """Diagnose bounded proposed links in the default ControlRig RigVM model."""

    @toolset_registry.tool_call
    @staticmethod
    def validate_controlrig_rigvm_links(control_rig_path: str = "", control_rig_asset_path: str = "",
            control_rig_class_path: str = "", links: str = "null", unit_links: str = "null",
            allow_type_casting: bool = True, max_links: str = "64") -> CubelessPythonOperationResult:
        """Validate proposed ControlRig default-model links without explicitly adding links, compiling, saving or executing a rig. Public links/unit_links are optional arrays of Any values with null defaults; Native selects links before unit_links and validates only the clamped prefix as link strings or alias objects. A selected empty links array is an error, while max_links=0 accepts a nonempty array and validates zero rows. Dictionary numeric/boolean values keep the original Native coercion rules. Public paths default to empty strings, allow_type_casting=True and max_links=64. Completed diagnostics can succeed while can_link is false or pins are missing. SDK load/type/diagnostic caches may have unknown effects, and full actual state reaches the operation journal before errors. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
        rejected, report, raw = _link_validation_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            link_arrays = _decode_nullable_arrays(links, unit_links)
            maximum = _decode_max_links(max_links)
        except Exception as carrier_error:
            result = link_validation._rejected(link_validation.OperationError("rigvm_link_validation_carrier_invalid", str(carrier_error)))
            result["stage"] = "rigvm_link_validation_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(link_validation.validate_controlrig_rigvm_links(control_rig_path, control_rig_asset_path,
            control_rig_class_path, link_arrays[0], link_arrays[1], allow_type_casting, maximum))


def _link_validation_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessRigVMLinkValidationAttachmentLibrary", None), "verify_rigvm_link_validation_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact RigVM link validation raw-input admission helper unavailable")
        raw = helper(CubelessRigVMLinkValidationTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_rigvm_link_validation_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "validate_controlrig_rigvm_links"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded link validation delegation required; no Native validation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = link_validation._rejected(link_validation.OperationError("rigvm_link_validation_raw_guard_admission_missing", message))
    result["stage"] = "rigvm_link_validation_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant is unsupported: " + value)


def _decode_nullable_arrays(*values):
    """Restore None versus [] without inspecting row shape, aliases or the ignored tail."""
    if len(values) != 2:
        raise ValueError("Exactly two nullable link-array carriers required")
    arrays = []
    for text in values:
        if type(text) is not str or len(text.encode("utf-16-le")) // 2 > 1048576:
            raise ValueError("Link array carrier requires at most1Mi UE UTF16 units")
        value = json.loads(text, parse_constant=_invalid_constant)
        if value is not None and type(value) is not list:
            raise ValueError("Link array carrier must decode as null or an array of Any values")
        arrays.append(value)
    return arrays


def _decode_max_links(text):
    """Decode only the private canonical int64 decimal; Native retains the original clamp."""
    if type(text) is not str or not text or len(text) > 20:
        raise ValueError("Private max_links requires a bounded canonical decimal")
    value = int(text)
    if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
        raise ValueError("Private max_links must be an exact canonical int64 decimal")
    return value
