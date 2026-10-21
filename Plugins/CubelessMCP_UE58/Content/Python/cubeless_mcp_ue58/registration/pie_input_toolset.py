"""HA original digital key injection into one exact begun local PIE controller. No OS input or automatic release."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import pie_input as domain

@unreal.uclass()
class CubelessPIEInputTools(unreal.ToolsetDefinition):
    "One simulated digital PIE key event with immediate readback; press and release remain separate requests."
    @toolset_registry.tool_call
    @staticmethod
    def inject_pie_input_key(key:str,event:str,pie_instance:str="-1")->CubelessPythonOperationResult:
        "Inject pressed or released into exactly one local begun PIE controller; reject SIE/noPIE/ambiguity."
        rejected,report,raw=_guard_admission("inject_pie_input_key")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"key":_decode_value(key),"event":_decode_value(event),"pie_instance":_decode_value(pie_instance)}
        except Exception as error:return reflected_result(_carrier_rejected("inject_pie_input_key",error,report,raw))
        return reflected_result(domain._named_mcp("inject_pie_input_key",arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPIEInputAttachmentLibrary', None), 'verify_pie_input_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact PIEInput adapter raw-input admission helper is unavailable')
        raw = helper(CubelessPIEInputTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_pie_input_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded PIEInput adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('pie_input_raw_guard_admission_missing', message))
    result['stage'] = 'pie_input_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('pie_input_registration_carrier_invalid',str(error)))
    result['stage']='pie_input_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
