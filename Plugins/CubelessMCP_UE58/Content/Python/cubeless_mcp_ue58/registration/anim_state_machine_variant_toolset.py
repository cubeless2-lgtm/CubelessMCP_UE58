"""Logical state-machine public20; JSON2/decimal2 are private reflection carriers."""
import json
import math
import re
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_state_machine_variant as operation

@unreal.uclass()
class CubelessAnimStateMachineVariantTools(unreal.ToolsetDefinition):
    "Create or reuse a guarded sample AnimBP state-machine variant from the source. Preserve ordered state/transition specifications, Sequence or BlendSpace players and inputs, variable rules, entry/root connections, optional compiler/save and topology readback within the one Native operation. None omits public states/transitions; empty lists and raw Batch presence retain their original parser meaning. Native owns count clamps, shorthand, graph selection, source-target guards, partial edits, dirty/compiler/save and unknown SDK effects. Explicit Batch entry empty is passed to the original sanitizer, not declared an unconditional error. No automatic retry or atomic rollback is provided. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."
    @toolset_registry.tool_call
    @staticmethod
    def ensure_anim_state_machine_sample_variant(source_anim_bp_path: str, states: str = "null", transitions: str = "null", target_anim_bp_path: str = "", target_root: str = "/Game/_MCP_Sample/AnimStudy", variant_name: str = "StateMachineSample", state_machine_name: str = "MCP_StateMachine", entry_state: str = "", animation_asset_path: str = "", connect_demo_machine_to_output: bool = True, replace_existing_root_link: bool = True, replace_existing_entry_link: bool = True, allow_existing_target_reuse: bool = True, overwrite_existing: bool = False, compile: bool = True, save_asset: bool = True, dry_run: bool = False, validate_only: bool = False, max_states: str = "16", max_transitions: str = "32") -> CubelessPythonOperationResult:
        "Create or reuse a guarded sample AnimBP state-machine variant from the source. Preserve ordered state/transition specifications, Sequence or BlendSpace players and inputs, variable rules, entry/root connections, optional compiler/save and topology readback within the one Native operation. None omits public states/transitions; empty lists and raw Batch presence retain their original parser meaning. Native owns count clamps, shorthand, graph selection, source-target guards, partial edits, dirty/compiler/save and unknown SDK effects. Explicit Batch entry empty is passed to the original sanitizer, not declared an unconditional error. No automatic retry or atomic rollback is provided. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."
        rejected, report, raw = _guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            states = _decode_list(states, "object")
            transitions = _decode_list(transitions, "object")
            max_states = _decode_integer(max_states)
            max_transitions = _decode_integer(max_transitions)
        except Exception as error:
            result = operation._rejected(operation.OperationError("anim_state_machine_variant_registration_carrier_invalid", str(error)))
            result["stage"] = "anim_state_machine_variant_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw, native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.ensure_anim_state_machine_sample_variant(source_anim_bp_path, states, transitions, target_anim_bp_path, target_root, variant_name, state_machine_name, entry_state, animation_asset_path, connect_demo_machine_to_output, replace_existing_root_link, replace_existing_entry_link, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only, max_states, max_transitions))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessAnimStateMachineVariantAttachmentLibrary', None), 'verify_anim_state_machine_variant_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Anim state-machine adapter raw-input admission helper is unavailable')
        raw = helper(CubelessAnimStateMachineVariantTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_anim_state_machine_variant_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'ensure_anim_state_machine_sample_variant'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Anim state-machine adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('anim_state_machine_variant_raw_guard_admission_missing', message))
    result['stage'] = 'anim_state_machine_variant_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError('Non-finite JSON is unsupported: '+value)

def _decode_list(text,kind):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text: raise ValueError('Bounded private nullable JSON text is required')
    depth=0;quoted=False;escaped=False
    for char in text:
        if quoted:
            if escaped: escaped=False
            elif char=='\\': escaped=True
            elif char=='"': quoted=False
        elif char=='"': quoted=True
        elif char in '[{':
            depth+=1
            if depth>64: raise ValueError('JSON depth64 exceeded')
        elif char in ']}':
            depth-=1
            if depth<0: raise ValueError('Malformed JSON')
    if quoted or depth: raise ValueError('Malformed JSON')
    value=json.loads(text,parse_constant=_invalid_constant)
    if value is not None and type(value) is not list: raise ValueError('Public nullable list required')
    if type(value) is list and kind!='any' and any(type(row) is not (dict if kind=='object' else str) for row in value): raise ValueError('Formal Named list row type differs')
    work=[value];slots=0
    while work:
        item=work.pop();slots+=1
        if slots>65536: raise ValueError('Carrier JSON value-slot bound exceeded')
        if type(item) is float and not math.isfinite(item): raise ValueError('Non-finite number refused')
        if type(item) is str and '\x00' in item: raise ValueError('Decoded NUL refused')
        if type(item) is list: work.extend(item)
        elif type(item) is dict:
            if any('\x00' in key for key in item): raise ValueError('Decoded key NUL refused')
            work.extend(item.values())
    return value

def _decode_integer(text):
    if type(text) is not str or len(text)>20 or re.fullmatch(r'-?(0|[1-9][0-9]*)',text) is None: raise ValueError('Private exact int64 decimal text required')
    value=int(text)
    if not -(1<<63)<=value<(1<<63): raise ValueError('int64 bound exceeded')
    return value

