"""Original logical Chaos trace11 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_trace_strain as domain

@unreal.uclass()
class CubelessChaosTraceStrainTools(unreal.ToolsetDefinition):
    'Dedicated Chaos trace strain11: preserve original prepare/begin/dispatch/sample/finish/abort and raw prepared_trace_id. Named eager normalization, direct Python None-only vectors, raw field presence, exact collision item and ordered physics dispatch remain distinct. Current request and cumulative session snapshots are separate. Actual physics is high risk; Native owns Editor/PIE lifetime/dispatch/async sampling/noReplay/partial state. No automatic retry, atomic rollback, compile/save, rendered/packaged completion claim.'
    @toolset_registry.tool_call
    @staticmethod
    def trigger_trace_external_strain(operation:str='"begin"',actor_name:str='""',start:str='null',end:str='null',expected_hit:str='true',trace_complex:str='true',radius:str='100',strain:str='100000',propagation_depth:str='1',propagation_factor:str='0.5',breaking_velocity:str='800')->CubelessPythonOperationResult:
        'Trace the exact Geometry Collection collision item, prepare or dispatch ordered strain and breaking velocity, then inspect bounded Native ticker observations. Hidden prepared_trace_id is raw-only. Session acceptance and terminal physics verification differ.'
        rejected,report,raw=_guard_admission('trigger_trace_external_strain')
        if rejected is not None:return reflected_result(rejected)
        try:values={'operation':_decode_value(operation),'actor_name':_decode_value(actor_name),'start':_decode_value(start),'end':_decode_value(end),'expected_hit':_decode_value(expected_hit),'trace_complex':_decode_value(trace_complex),'radius':_decode_value(radius),'strain':_decode_value(strain),'propagation_depth':_decode_value(propagation_depth),'propagation_factor':_decode_value(propagation_factor),'breaking_velocity':_decode_value(breaking_velocity)}
        except Exception as error:return reflected_result(_carrier_rejected('trigger_trace_external_strain',error,report,raw))
        return reflected_result(domain._named_mcp('trigger_trace_external_strain',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosTraceStrainAttachmentLibrary', None), 'verify_chaos_trace_strain_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosTraceStrain adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosTraceStrainTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_trace_strain_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosTraceStrain adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('chaos_trace_strain_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_trace_strain_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('chaos_trace_strain_registration_carrier_invalid',str(error)))
    result['stage']='chaos_trace_strain_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
