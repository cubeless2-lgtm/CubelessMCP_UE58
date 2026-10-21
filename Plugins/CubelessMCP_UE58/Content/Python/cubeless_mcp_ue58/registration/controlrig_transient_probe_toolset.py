"""The original two ControlRig transient probes on the official Toolset Registry."""
import json
import math
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import controlrig_direct_gate_probe as direct_ops
from cubeless_mcp_ue58.operations import controlrig_pre_post_runtime_pose as pose_ops

@unreal.uclass()
class CubelessControlRigTransientProbeTools(unreal.ToolsetDefinition):
    """Run two direct transient ControlRig probes; these do not instrument the compiled AnimGraph."""

    @toolset_registry.tool_call
    @staticmethod
    def controlrig_direct_gate_probe(control_rig_path: str = "", control_rig_class: str = "", cases: str = "null", sample_elements: str = "null", control_values: str = "null", execute_events: str = "null", should_trace_property: str = "ShouldDoIKTrace", interaction_location_property: str = "InteractionWorldLocation") -> CubelessPythonOperationResult:
        'Initialize and execute transient ControlRig case probes; sample hierarchy/control values and deltas from the first case. Original optional path/class strings and property-name defaults are preserved. Nullable JSON inputs remain None versus explicit empty arrays/objects; original Native fallback may still choose default cases/events/sample elements. Native owns transient initialization/execution, per-case errors, sampling and full partial/unknown effect receipts. For Blueprint targets Native explicitly calls public RecompileVMIfRequired before creating the TransientPackage/RF_Transient host: compile requested/attempted are true while completion is unobserved (compiled is None). Direct-class targets do not take that Blueprint preparation path. Native dirty/unknown receipts remain authoritative. No extra load, initialize, execute, compile, save or retry is added here. Pre/post input/curve default dictionaries are applied only in domain when None; logical defaults remain null. Asset save absence is not proof of zero custom callbacks or a live runtime pose.'
        rejected, report, raw = _guard_admission('controlrig_direct_gate_probe', direct_ops)
        if rejected is not None: return reflected_result(rejected)
        try:
            cases = _decode_nullable('cases', cases)
            sample_elements = _decode_nullable('sample_elements', sample_elements)
            control_values = _decode_nullable('control_values', control_values)
            execute_events = _decode_nullable('execute_events', execute_events)
        except Exception as error:
            result = direct_ops._rejected(direct_ops.OperationError("controlrig_transient_probe_carrier_invalid", str(error)))
            result["stage"] = "controlrig_transient_probe_registration_carrier_admission"
            result["data"].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(direct_ops.controlrig_direct_gate_probe(control_rig_path, control_rig_class, cases, sample_elements, control_values, execute_events, should_trace_property, interaction_location_property))

    @toolset_registry.tool_call
    @staticmethod
    def sample_controlrig_pre_post_runtime_pose(control_rig_path: str = "", control_rig_class: str = "", input_defaults: str = "null", curve_values: str = "null", cases: str = "null", sample_elements: str = "null", control_values: str = "null", execute_events: str = "null", should_trace_property: str = "ShouldDoIKTrace", interaction_location_property: str = "InteractionWorldLocation") -> CubelessPythonOperationResult:
        'Sample hierarchy transforms before and after transient ControlRig execution; this is direct_transient_controlrig, not compiled runtime AnimGraph node-stack instrumentation. Original optional path/class strings and property-name defaults are preserved. Nullable JSON inputs remain None versus explicit empty arrays/objects; original Native fallback may still choose default cases/events/sample elements. Native owns transient initialization/execution, per-case errors, sampling and full partial/unknown effect receipts. For Blueprint targets Native explicitly calls public RecompileVMIfRequired before creating the TransientPackage/RF_Transient host: compile requested/attempted are true while completion is unobserved (compiled is None). Direct-class targets do not take that Blueprint preparation path. Native dirty/unknown receipts remain authoritative. No extra load, initialize, execute, compile, save or retry is added here. Pre/post input/curve default dictionaries are applied only in domain when None; logical defaults remain null. Asset save absence is not proof of zero custom callbacks or a live runtime pose.'
        rejected, report, raw = _guard_admission('sample_controlrig_pre_post_runtime_pose', pose_ops)
        if rejected is not None: return reflected_result(rejected)
        try:
            input_defaults = _decode_nullable('input_defaults', input_defaults)
            curve_values = _decode_nullable('curve_values', curve_values)
            cases = _decode_nullable('cases', cases)
            sample_elements = _decode_nullable('sample_elements', sample_elements)
            control_values = _decode_nullable('control_values', control_values)
            execute_events = _decode_nullable('execute_events', execute_events)
        except Exception as error:
            result = pose_ops._rejected(pose_ops.OperationError("controlrig_transient_probe_carrier_invalid", str(error)))
            result["stage"] = "controlrig_transient_probe_registration_carrier_admission"
            result["data"].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(pose_ops.sample_controlrig_pre_post_runtime_pose(control_rig_path, control_rig_class, input_defaults, curve_values, cases, sample_elements, control_values, execute_events, should_trace_property, interaction_location_property))

def _guard_admission(method,ops):
    report={};raw=None
    try:
        helper=getattr(getattr(unreal,"CubelessControlRigTransientProbeAttachmentLibrary",None),"verify_controlrig_transient_probe_toolset",None)
        if not callable(helper): raise RuntimeError("Exact transient probe attachment helper unavailable")
        raw=helper(CubelessControlRigTransientProbeTools);report=json.loads(raw)
        if type(report) is dict and report.get("operation")=="verify_controlrig_transient_probe_toolset" and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True and report.get("delegated_method")==method and report.get("asset_mutation") is False and report.get("metadata_mutation") is False:
            return None,report,raw
        message="Only the exact current guarded transient probe method delegation is admitted"
    except Exception as error: message=str(error)
    result=ops._rejected(ops.OperationError("controlrig_transient_probe_raw_guard_admission_missing",message))
    result["stage"]="controlrig_transient_probe_registration_admission"
    result["data"].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False,helper_receipt_is_whole_SDK_change_proof=False)
    return result,report,raw

def _invalid_constant(value):
    raise ValueError("Non-finite JSON constant unsupported: "+value)

def _decode_nullable(field,text):
    if type(text) is not str or len(text.encode("utf-16-le"))//2>1048576 or "\x00" in text:
        raise ValueError("Nullable probe carrier requires at most1Mi UTF16 units without raw NUL")
    depth=0;quoted=False;escaped=False
    for char in text:
        if quoted:
            if escaped: escaped=False
            elif char=="\\": escaped=True
            elif char=='"': quoted=False
        elif char=='"': quoted=True
        elif char in "[{":
            depth+=1
            if depth>64: raise ValueError("Nullable probe carrier exceeds depth64")
        elif char in "]}":
            depth-=1
            if depth<0: raise ValueError("Malformed nullable probe carrier")
    if quoted or depth: raise ValueError("Malformed nullable probe carrier")
    value=json.loads(text,parse_constant=_invalid_constant)
    if value is None: return None
    if field in ("cases","sample_elements","execute_events"):
        if type(value) is not list: raise ValueError("Nullable probe array required: "+field)
        if field=="cases" and any(type(v) is not dict for v in value): raise ValueError("Public cases require object elements")
        if field=="execute_events" and any(type(v) is not str for v in value): raise ValueError("Public execute_events require string elements")
    else:
        if type(value) is not dict: raise ValueError("Nullable probe object required: "+field)
        if field=="curve_values" and any(type(v) not in (int,float) or not math.isfinite(v) for v in value.values()):
            raise ValueError("Public curve_values require finite JSON numbers")
    return value
