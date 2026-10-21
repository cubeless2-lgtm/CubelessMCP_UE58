"""Original solver-intent validator on official Registry; the union carrier is private."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import controlrig_solver_intents as solver_intents

@unreal.uclass()
class CubelessControlRigSolverIntentTools(unreal.ToolsetDefinition):
    """Validate solver intent JSON without building, solving or modifying engine assets."""

    @toolset_registry.tool_call
    @staticmethod
    def validate_controlrig_solver_intents(request: str) -> CubelessPythonOperationResult:
        """Validate one required request supplied publicly as a JSON object or a string. This adapter restores only the private union carrier; the Python domain owns parsing JSON inside a request string, dependency-reference checks, validation-only gates, intent/recipe aliases, issue ordering and the unchanged legacy inner result. No Engine validator library, asset loading, graph mutation, solver materialization, compile, save or solve operation is dispatched. Official registration metadata and the shared operation journal may query their own editor/project context and are separate from validation work. Full validation reports and failures reach the sole operation/Batch parent journal; invalid union inputs are rejected before reflected packing or journal creation. Request is required with no default and object members remain Any values. Whole official HTTP object-number precision is subject to its upstream parser; this adapter does not reconstruct lost tokens. Source-only registration or invocation does not establish live solver, runtime or pipeline proof."""
        rejected, report, raw = _solver_intents_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            value = _decode_request(request)
        except Exception as carrier_error:
            result = solver_intents._rejected(solver_intents.OperationError("controlrig_solver_intents_carrier_invalid", str(carrier_error)))
            result["stage"] = "controlrig_solver_intents_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, validator_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(solver_intents.validate_controlrig_solver_intents(value))


def _solver_intents_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessControlRigSolverIntentsAttachmentLibrary", None), "verify_controlrig_solver_intents_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact solver-intent union raw-input admission helper unavailable")
        raw = helper(CubelessControlRigSolverIntentTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_controlrig_solver_intents_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "validate_controlrig_solver_intents"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded solver-intent delegation required; no validator dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = solver_intents._rejected(solver_intents.OperationError("controlrig_solver_intents_raw_guard_admission_missing", message))
    result["stage"] = "controlrig_solver_intents_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, validator_dispatched=False, python_dispatched=True, journal_created=False,
        registration_helper_receipt_is_validation_result=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant is unsupported: " + value)


def _decode_request(text):
    """Restore only object versus string, without parsing inside a request string."""
    if type(text) is not str or len(text.encode("utf-16-le")) // 2 > 1048576:
        raise ValueError("Request union carrier requires at most 1Mi UE UTF16 units")
    value = json.loads(text, parse_constant=_invalid_constant)
    if type(value) not in (dict, str):
        raise ValueError("Request union carrier must decode as an object or a string")
    return value
