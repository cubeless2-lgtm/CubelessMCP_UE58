"""Original trace5 on the official Toolset Registry; vector JSON is a private ABI."""
import json
import math
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import physical_material_trace as trace

@unreal.uclass()
class CubelessEditorTraceTools(unreal.ToolsetDefinition):
    """Physical-material line trace in the current Editor world."""

    @toolset_registry.tool_call
    @staticmethod
    def line_trace_physical_material(start: str, end: str, trace_channel: str = "visibility",
            trace_complex: bool = False, return_physical_material: bool = True) -> CubelessPythonOperationResult:
        """Trace between required numeric arrays start/end of length3 in the Editor world. The owning public Toolset adapter exposes the original arrays and defaults; these two reflected strings are private JSON carriers. Exact current trace delegation is required before decoding or domain dispatch. Native owns channel aliases, collision query, hit/actor/component and physical material with BodyInstance fallback. Actor resolution/default label may materialize Engine state; successful query completion does not prove total SDK purity. The domain preserves actual unknown/change/label prefixes and uses one Named journal. No extra Python asset load, Modify, compile/save, trace reissue, rollback, shader wait or external pipeline completion claim."""
        rejected, report, raw = _trace_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            vectors = _decode_trace_carriers(start, end)
        except Exception as carrier_error:
            result = trace._rejected(trace.OperationError("editor_trace_carrier_invalid", str(carrier_error)))
            result["stage"] = "editor_trace_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(trace.line_trace_physical_material(vectors["start"], vectors["end"],
            trace_channel, trace_complex, return_physical_material))


def _trace_guard_admission():
    """Only the current exact registered wrapper's trace delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessEditorTraceAttachmentLibrary", None), "verify_editor_trace_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact Editor trace raw-input admission helper unavailable")
        raw = helper(CubelessEditorTraceTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_editor_trace_toolset"
                and report.get("delegated_call_admitted") is True
                and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "line_trace_physical_material"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded trace delegation required; no Native trace dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = trace._rejected(trace.OperationError("editor_trace_raw_guard_admission_missing", message))
    result["stage"] = "editor_trace_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _decode_trace_carriers(start, end):
    """Decode only exact numeric arrays3; the typed domain keeps its legacy float conversion."""
    decoded = {}
    for field, text in (("start", start), ("end", end)):
        if type(text) is not str or len(text) > 4096:
            raise ValueError(field + " private carrier requires at most4096 JSON characters")
        value = json.loads(text)
        if (type(value) is not list or len(value) != 3
                or any(type(item) not in (int, float) for item in value)
                or not all(math.isfinite(float(item)) for item in value)):
            raise ValueError(field + " must decode as exactly3 finite numbers; bool/string are not logical vector components")
        decoded[field] = value
    return decoded
