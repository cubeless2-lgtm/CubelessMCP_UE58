"""Logical BlendSpace public13; physical carriers are private registration details."""
import json
import math
import re
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import blendspace_variant as operation

@unreal.uclass()
class CubelessBlendSpaceVariantTools(unreal.ToolsetDefinition):
    """Create or reuse a guarded BlendSpace variant, apply ordered axis/sample edits and additions, resample when changed or forced, then save only after successful edits. dry_run/validate_only preserve loaded source diagnostics without duplicate/edit/save. Partial failure can leave a created/reused target dirty; save failure, axis changes before failed SDK edits, deletion and callback effects remain observable and are not atomic rollback or automatic retry. Default sample/temp paths, explicit in-place allow_non_sample+force_resample and formal nullable object arrays are preserved. Legacy command aliases remain Batch-only."""
    @toolset_registry.tool_call
    @staticmethod
    def ensure_blendspace_sample_variant(source_blendspace: str, variant_name: str = "Variant", target_root: str = "/Game/_MCP_Sample/AnimStudy", target_blendspace: str = "", axis_edits: str = "null", sample_edits: str = "null", add_samples: str = "null", overwrite_existing: bool = False, save: bool = True, dry_run: bool = False, validate_only: bool = False, allow_non_sample: bool = False, force_resample: bool = False) -> CubelessPythonOperationResult:
        """Create or reuse a guarded BlendSpace variant, apply ordered axis/sample edits and additions, resample when changed or forced, then save only after successful edits. dry_run/validate_only preserve loaded source diagnostics without duplicate/edit/save. Partial failure can leave a created/reused target dirty; save failure, axis changes before failed SDK edits, deletion and callback effects remain observable and are not atomic rollback or automatic retry. Default sample/temp paths, explicit in-place allow_non_sample+force_resample and formal nullable object arrays are preserved. Legacy command aliases remain Batch-only."""
        rejected,report,raw=_guard_admission()
        if rejected is not None: return reflected_result(rejected)
        try:
            axis_edits = _decode_list(axis_edits, "object")
            sample_edits = _decode_list(sample_edits, "object")
            add_samples = _decode_list(add_samples, "object")
        except Exception as error:
            result=operation._rejected(operation.OperationError('blendspace_variant_registration_carrier_invalid',str(error)))
            result['stage']='blendspace_variant_registration_carrier_admission'
            result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.ensure_blendspace_sample_variant(source_blendspace, variant_name, target_root, target_blendspace, axis_edits, sample_edits, add_samples, overwrite_existing, save, dry_run, validate_only, allow_non_sample, force_resample))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessBlendSpaceVariantAttachmentLibrary', None), 'verify_blendspace_variant_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact BlendSpace adapter raw-input admission helper is unavailable')
        raw = helper(CubelessBlendSpaceVariantTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_blendspace_variant_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'ensure_blendspace_sample_variant'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded BlendSpace adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('blendspace_variant_raw_guard_admission_missing', message))
    result['stage'] = 'blendspace_variant_registration_admission'
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

def _decode_number(text):
    if type(text) is not str or len(text)>1048576 or re.fullmatch(r'-?(0|[1-9][0-9]*)(\.[0-9]+)?([eE][+-]?[0-9]+)?',text) is None: raise ValueError('Private finite JSON-number text required')
    value=float(text)
    if not math.isfinite(value): raise ValueError('Finite double required')
    return value
