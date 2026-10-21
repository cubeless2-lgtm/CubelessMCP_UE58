"""Logical BlendSpace public17; physical carriers are private registration details."""
import json
import math
import re
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import blendspace_pose_grid as operation

@unreal.uclass()
class CubelessBlendSpacePoseGridTools(unreal.ToolsetDefinition):
    """Sample BlendSpace inputs through a transient SingleNode skeletal actor in an already available PIE/SIE/play world by default. Preserve target/sample ordering and None/empty fallback, weighted animation samples, first valid pose baseline, deltas, partial errors and actual cleanup results. This runtime probe spawns/configures/ticks/refreshes and may initialize animation; it does not start a session or explicitly compile/save assets. Native owns lifetime, parallel readiness, finite work and actual partial/unknown effects. Formal Named array item types differ from legacy Batch raw descriptors. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
    @toolset_registry.tool_call
    @staticmethod
    def sample_blendspace_runtime_pose_grid(skeletal_mesh: str, blendspace_path: str = "", blendspaces: str = "null", samples: str = "null", sample_bones: str = "null", sample_sockets: str = "null", actor_label: str = "MCP_BlendSpace_RuntimePoseGrid", sample_time: str = "0.25", settle_tick_count: str = "2", tick_count: str = "1", tick_delta_time: str = "0.03333333333333333", include_axis_extremes: bool = False, allow_missing_bones: bool = False, refresh_bone_transforms: bool = True, cleanup: bool = True, prefer_pie_world: bool = True, require_pie_world: bool = True) -> CubelessPythonOperationResult:
        """Sample BlendSpace inputs through a transient SingleNode skeletal actor in an already available PIE/SIE/play world by default. Preserve target/sample ordering and None/empty fallback, weighted animation samples, first valid pose baseline, deltas, partial errors and actual cleanup results. This runtime probe spawns/configures/ticks/refreshes and may initialize animation; it does not start a session or explicitly compile/save assets. Native owns lifetime, parallel readiness, finite work and actual partial/unknown effects. Formal Named array item types differ from legacy Batch raw descriptors. Official Named HTTP numbers may already be rounded before this adapter; exact large-limit Batch action verification is separate."""
        rejected,report,raw=_guard_admission()
        if rejected is not None: return reflected_result(rejected)
        try:
            blendspaces = _decode_list(blendspaces, "any")
            samples = _decode_list(samples, "object")
            sample_bones = _decode_list(sample_bones, "string")
            sample_sockets = _decode_list(sample_sockets, "string")
            sample_time = _decode_number(sample_time)
            settle_tick_count = _decode_integer(settle_tick_count)
            tick_count = _decode_integer(tick_count)
            tick_delta_time = _decode_number(tick_delta_time)
        except Exception as error:
            result=operation._rejected(operation.OperationError('blendspace_pose_grid_registration_carrier_invalid',str(error)))
            result['stage']='blendspace_pose_grid_registration_carrier_admission'
            result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.sample_blendspace_runtime_pose_grid(skeletal_mesh, blendspace_path, blendspaces, samples, sample_bones, sample_sockets, actor_label, sample_time, settle_tick_count, tick_count, tick_delta_time, include_axis_extremes, allow_missing_bones, refresh_bone_transforms, cleanup, prefer_pie_world, require_pie_world))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessBlendSpacePoseGridAttachmentLibrary', None), 'verify_blendspace_pose_grid_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact BlendSpace adapter raw-input admission helper is unavailable')
        raw = helper(CubelessBlendSpacePoseGridTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_blendspace_pose_grid_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'sample_blendspace_runtime_pose_grid'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded BlendSpace adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('blendspace_pose_grid_raw_guard_admission_missing', message))
    result['stage'] = 'blendspace_pose_grid_registration_admission'
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
