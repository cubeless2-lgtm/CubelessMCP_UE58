"""Logical public28; reflected FString16/bool12 carriers remain internal."""
import json
import math
import re
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_state_machine_runtime_response as probe

@unreal.uclass()
class CubelessAnimStateRuntimeResponseTools(unreal.ToolsetDefinition):
    """Sample compound live AnimInstance cases with Native-owned partial restoration."""

    @toolset_registry.tool_call
    @staticmethod
    def sample_anim_state_machine_runtime_response(actor_label: str = '',
            actor_name: str = '',
            actor_path: str = '',
            component_name: str = '',
            cases: str = 'null',
            properties: str = 'null',
            name: str = 'case_0',
            tick_count: str = '1',
            tick_delta_time: str = '0.03333333333333333',
            refresh_bone_transforms: bool = True,
            restore_after_case: bool = True,
            include_baseline: bool = True,
            state_machine_name: str = '',
            include_states: bool = True,
            include_state_weights: bool = True,
            include_relevant_anim_times: bool = False,
            include_transition_progress: bool = True,
            include_inactive_transition_progress: bool = False,
            include_montages: bool = True,
            include_curves: bool = False,
            curve_names: str = 'null',
            max_state_machines: str = '32',
            max_state_machine_instances_to_probe: str = '256',
            max_states_per_machine: str = '64',
            max_transitions_per_machine: str = '128',
            max_curves: str = '128',
            prefer_pie_world: bool = True,
            require_pie_world: bool = False) -> CubelessPythonOperationResult:
        'Sample one existing live AnimInstance baseline and compound property cases in one Native operation. Each case can apply ordered properties, tick/refresh, capture a main-instance snapshot and optionally reapply previous values in original forward order. Duplicate property assignments, failed writes, tick/time/notifies and partial restoration are not atomic rollback or guaranteed baseline restoration. Native preserves each applied/tick/snapshot/restore prefix, actual call counts, errors, unknown total SDK effects and captured dirty observations; Python never retries or adds Engine work. Named always targets main while the original hidden source/projection remains Batch-only. Work is bounded to 256 executable cases, 4096 aggregate ticks and 65536 assignments; synchronous Engine animation/refresh may block and returned calls do not prove completed pose/evaluation. No explicit asset compiler/save or session start is added; this does not prove dirty invariance, runtime stability, playback or a complete external pipeline. Public28 types/defaults are logical; private strings avoid Python int32/float32 reflection while upstream official HTTP numbers may already be rounded.'
        rejected, report, raw = _probe_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            decoded_cases = _decode_json(cases,"cases")
            decoded_properties = _decode_json(properties,"properties")
            decoded_curves = _decode_json(curve_names,"curve_names")
            ticks, machines, probes, states, transitions, curves = _decode_limits(tick_count,max_state_machines,max_state_machine_instances_to_probe,max_states_per_machine,max_transitions_per_machine,max_curves)
            delta = _decode_number(tick_delta_time)
        except Exception as error:
            result=probe._rejected(probe.OperationError("anim_runtime_response_registration_carrier_invalid",str(error)))
            result["stage"]="anim_runtime_response_registration_carrier_admission"
            result["data"].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(probe.sample_anim_state_machine_runtime_response(actor_label,actor_name,actor_path,component_name,
            decoded_cases,decoded_properties,name,ticks,delta,refresh_bone_transforms,restore_after_case,include_baseline,
            state_machine_name,include_states,include_state_weights,include_relevant_anim_times,include_transition_progress,
            include_inactive_transition_progress,include_montages,include_curves,decoded_curves,machines,probes,states,
            transitions,curves,prefer_pie_world,require_pie_world))

def _probe_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessAnimStateRuntimeResponseAttachmentLibrary", None), "verify_anim_state_machine_runtime_response_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact AnimInstance runtime raw-input admission helper unavailable")
        raw = helper(CubelessAnimStateRuntimeResponseTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_anim_state_machine_runtime_response_toolset"
                and report.get("delegated_call_admitted") is True and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "sample_anim_state_machine_runtime_response"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded runtime response sampler delegation required; no Native operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = probe._rejected(probe.OperationError("anim_runtime_response_raw_guard_admission_missing", message))
    result["stage"] = "anim_runtime_response_registration_admission"
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
    if field=="cases" and result is not None and (type(result) is not list or any(type(v) is not dict for v in result)): raise ValueError("Public cases remain list[dict]/null")
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
    if len(values)!=6: raise ValueError("Exactly six private integer carriers required")
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
