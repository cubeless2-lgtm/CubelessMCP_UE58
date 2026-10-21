"""Public spline10: one points JSON carrier, six strings and three Booleans."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import spline_component_points as operation

@unreal.uclass()
class CubelessSplineComponentPointsTools(unreal.ToolsetDefinition):
    'Set Editor spline points with original public10: points array plus actor/component selectors and selected_only/closed_loop/native_first. Native-first sends9 public values to the native10 ABI and falls back only after a confirmed no-mutation Unknown command response. native_first=False directly runs the in-editor Python World/Linear clear/add implementation; it rejects multiple actors, excludes TRASH_ splines and preserves its distinct case-sensitive tag, precision, flags and legacy result. Raw legacy Batch goes directly to Native regardless of native_first, preserving hidden8 and component_name/closed_loop raw omission. Exact Boolean, finite first3 coordinates and bounded carriers are extra admission safeguards. Explicit compile/save0; callback or construction effects can remain unknown. Original partial/error prefixes and public point/count/type/closed postconditions are preserved. No uncertain replay, atomic rollback, external execute_python or result-file side channel.'
    @toolset_registry.tool_call
    @staticmethod
    def set_spline_component_points(points:str,actor_label:str="",actor_name:str="",actor_tag:str="",actor_label_prefix:str="",component_name:str="Road_SourceSpline",component_tag:str="",selected_only:bool=False,closed_loop:bool=False,native_first:bool=True)->CubelessPythonOperationResult:
        'Set Editor spline points with original public10: points array plus actor/component selectors and selected_only/closed_loop/native_first. Native-first sends9 public values to the native10 ABI and falls back only after a confirmed no-mutation Unknown command response. native_first=False directly runs the in-editor Python World/Linear clear/add implementation; it rejects multiple actors, excludes TRASH_ splines and preserves its distinct case-sensitive tag, precision, flags and legacy result. Raw legacy Batch goes directly to Native regardless of native_first, preserving hidden8 and component_name/closed_loop raw omission. Exact Boolean, finite first3 coordinates and bounded carriers are extra admission safeguards. Explicit compile/save0; callback or construction effects can remain unknown. Original partial/error prefixes and public point/count/type/closed postconditions are preserved. No uncertain replay, atomic rollback, external execute_python or result-file side channel.'
        rejected,report,raw=_guard_admission()
        if rejected is not None:return reflected_result(rejected)
        try:points=_decode_points(points)
        except Exception as error:
            result=operation._rejected(operation.OperationError('spline_component_points_registration_carrier_invalid',str(error)))
            result['stage']='spline_component_points_registration_carrier_admission'
            result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.set_spline_component_points(points,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,native_first))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessSplineComponentPointsAttachmentLibrary', None), 'verify_spline_component_points_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Spline component points adapter raw-input admission helper is unavailable')
        raw = helper(CubelessSplineComponentPointsTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_spline_component_points_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'set_spline_component_points'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Spline component points adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('spline_component_points_raw_guard_admission_missing', message))
    result['stage'] = 'spline_component_points_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON is unsupported: '+value)

def _decode_points(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private points JSON required')
    value=json.loads(text,parse_constant=_invalid_constant)
    if type(value) is not list or len(value)<2:raise ValueError('points requires at least2 entries; route-specific shape validation belongs to the operation')
    return value
