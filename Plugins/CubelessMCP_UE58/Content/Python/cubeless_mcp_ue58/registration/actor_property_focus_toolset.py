"""Original logical actor property/focus3/4 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import actor_property_focus as operation

@unreal.uclass()
class CubelessActorPropertyFocusTools(unreal.ToolsetDefinition):
    'Actor property writes and viewport focus preserve original supported property branches, load/write/readback/camera prefix, exact field presence, original Named Any value profile despite string schema, and focus truthiness. Explicit Modify/notify/dirty/compile/save0 does not prove global SDK purity. No uncertain retry or atomic rollback.'
    @toolset_registry.tool_call
    @staticmethod
    def set_actor_property(name:str,property_name:str,property_value:str)->CubelessPythonOperationResult:
        'Set a live actor property by original UObject name and supported type coercion. Public schema property_value remains string but original runtime accepts JSON Any after preparse. Preserve Class/Object reference guard only in those Native branches, actual write/readback and partial load failure. Explicit notify/compile/save0.'
        rejected,report,raw=_guard_admission('set_actor_property')
        if rejected is not None:return reflected_result(rejected)
        try:values={'name':_decode_value(name),'property_name':_decode_value(property_name),'property_value':_decode_value(property_value)}
        except Exception as error:return reflected_result(_carrier_rejected('set_actor_property',error,report,raw))
        return reflected_result(operation._named_mcp('set_actor_property',values))

    @toolset_registry.tool_call
    @staticmethod
    def focus_viewport(target:str='null',location:str='null',distance:str='1000.0',orientation:str='null')->CubelessPythonOperationResult:
        'Focus viewport by original truthy target or location, distance and optional orientation. Raw presence0/[]/null differs from Named omission. Preserve active/fallback selection, -X distance and partial camera/Invalidate receipts without render-completion or rollback claim.'
        rejected,report,raw=_guard_admission('focus_viewport')
        if rejected is not None:return reflected_result(rejected)
        try:values={'target':_decode_value(target),'location':_decode_value(location),'distance':_decode_value(distance),'orientation':_decode_value(orientation)}
        except Exception as error:return reflected_result(_carrier_rejected('focus_viewport',error,report,raw))
        return reflected_result(operation._named_mcp('focus_viewport',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessActorPropertyFocusAttachmentLibrary', None), 'verify_actor_property_focus_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ActorPropertyFocus adapter raw-input admission helper is unavailable')
        raw = helper(CubelessActorPropertyFocusTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_actor_property_focus_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ActorPropertyFocus adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('actor_property_focus_raw_guard_admission_missing', message))
    result['stage'] = 'actor_property_focus_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('actor_property_focus_registration_carrier_invalid',str(error)))
    result['stage']='actor_property_focus_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
