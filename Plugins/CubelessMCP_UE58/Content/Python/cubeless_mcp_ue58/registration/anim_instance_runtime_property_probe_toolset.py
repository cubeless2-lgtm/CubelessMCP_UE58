"""Official logical25; reflected string12/bool13 carriers are internal."""
import json
import math
import re
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_instance_runtime_property_probe as probe

@unreal.uclass()
class CubelessAnimInstanceRuntimePropertyProbeTools(unreal.ToolsetDefinition):
    """Apply one current live AnimInstance runtime-property probe operation."""

    @toolset_registry.tool_call
    @staticmethod
    def set_anim_instance_runtime_property_for_probe(actor_label: str = '',
            actor_name: str = '',
            actor_path: str = '',
            component_name: str = '',
            properties: str = 'null',
            property_name: str = '',
            value: str = 'null',
            tick_after_set: bool = False,
            tick_count: str = '1',
            tick_delta_time: str = '0.03333333333333333',
            refresh_bone_transforms: bool = True,
            include_snapshot_after: bool = True,
            include_previous_values: bool = True,
            state_machine_name: str = '',
            include_states: bool = True,
            include_state_weights: bool = True,
            include_relevant_anim_times: bool = False,
            include_transition_progress: bool = True,
            include_inactive_transition_progress: bool = False,
            include_montages: bool = True,
            include_curves: bool = False,
            curve_names: str = 'null',
            max_transitions_per_machine: str = '128',
            prefer_pie_world: bool = True,
            require_pie_world: bool = False) -> CubelessPythonOperationResult:
        'Set properties on the selected existing live main AnimInstance for probing, optionally TickAnimation and RefreshBoneTransforms, then capture runtime metadata/state-machine/transition/montage/curve state. Public25 retains nullable property map, Any value and curve names plus original bool13/int64limits2/double1 defaults. Named always selects main; existing hidden Batch source selection remains internal. Native owns assignment order, previous/echo rows, partial failure, per-call strong/current/parallel guards, tick clamps and actual attempted/returned counts. Tick/refresh may synchronously execute animation/notifies or internally block; returned void SDK calls do not prove an evaluated pose or internal update count. No explicit asset Modify/compiler/save/session start or rollback/retry is added. Full Native raw/Data/actual compile-save/change-unknown prefix reaches the parent journal before errors. Original postprocess selection can still refresh main for snapshot; actual Native scope is authoritative. Python int32/float32 reflection is avoided with private string carriers; upstream official HTTP doubles may already be rounded. This does not prove asset dirty invariance, runtime stability or a full external pipeline.'
        rejected, report, raw = _probe_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            props = _decode_json(properties,"properties")
            item = _decode_json(value,"value")
            curves = _decode_json(curve_names,"curve_names")
            ticks, transitions = _decode_limits(tick_count,max_transitions_per_machine)
            delta = _decode_number(tick_delta_time)
        except Exception as error:
            result=probe._rejected(probe.OperationError("anim_runtime_property_registration_carrier_invalid",str(error)))
            result["stage"]="anim_runtime_property_registration_carrier_admission"
            result["data"].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(probe.set_anim_instance_runtime_property_for_probe(actor_label,actor_name,actor_path,component_name,
            props,property_name,item,tick_after_set,ticks,delta,refresh_bone_transforms,include_snapshot_after,include_previous_values,
            state_machine_name,include_states,include_state_weights,include_relevant_anim_times,include_transition_progress,
            include_inactive_transition_progress,include_montages,include_curves,curves,transitions,prefer_pie_world,require_pie_world))

def _probe_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary", None), "verify_anim_instance_runtime_property_probe_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact AnimInstance runtime raw-input admission helper unavailable")
        raw = helper(CubelessAnimInstanceRuntimePropertyProbeTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_anim_instance_runtime_property_probe_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "set_anim_instance_runtime_property_for_probe"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded runtime property probe delegation required; no Native operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = probe._rejected(probe.OperationError("anim_runtime_property_raw_guard_admission_missing", message))
    result["stage"] = "anim_runtime_property_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError("Non-finite JSON is unsupported: "+value)


def _decode_json(text,field):
    if type(text) is not str or len(text.encode("utf-16-le"))//2>1048576 or "\x00" in text:
        raise ValueError("Bounded Native JSON carrier required")
    depth=0;quoted=False;escaped=False
    for char in text:
        if quoted:
            if escaped: escaped=False
            elif char=="\\": escaped=True
            elif char=='"': quoted=False
        elif char=='"': quoted=True
        elif char in "[{":
            depth+=1
            if depth>64: raise ValueError("Native JSON depth64 exceeded")
        elif char in "]}":
            depth-=1
            if depth<0: raise ValueError("Malformed Native JSON")
    if quoted or depth: raise ValueError("Malformed Native JSON")
    result=json.loads(text,parse_constant=_invalid_constant)
    if field=="properties" and result is not None and type(result) is not dict: raise ValueError("Public properties remains object/null")
    if field=="curve_names" and result is not None and (type(result) is not list or any(type(v) is not str for v in result)): raise ValueError("Public curves remain list[str]/null")
    work=[result];slots=0
    while work:
        value=work.pop();slots+=1
        if slots>65536: raise ValueError("Native JSON slot bound exceeded")
        if type(value) is float and not math.isfinite(value): raise ValueError("Finite JSON number required")
        if type(value) is str and "\x00" in value: raise ValueError("Decoded Native NUL refused")
        if type(value) is dict:
            if any("\x00" in key for key in value): raise ValueError("Decoded Native key NUL refused")
            work.extend(value.values())
        elif type(value) is list: work.extend(value)
    return result


def _decode_limits(*values):
    if len(values)!=2: raise ValueError("Exactly two private integer carriers required")
    result=[]
    for text in values:
        if type(text) is not str or not text or len(text)>20: raise ValueError("Bounded canonical decimal required")
        value=int(text)
        if str(value)!=text or value<-(1<<63) or value>(1<<63)-1: raise ValueError("Exact canonical int64 required")
        result.append(value)
    return result


def _decode_number(text):
    if type(text) is not str or len(text.encode("utf-16-le"))//2>1048576 or re.fullmatch(r"-?(?:0|[1-9][0-9]*)(?:\.[0-9]+)?(?:[eE][+-]?[0-9]+)?",text) is None:
        raise ValueError("Private finite JSON double token required")
    value=float(text)
    if not math.isfinite(value): raise ValueError("NaN/inf/double overflow refused")
    return value
