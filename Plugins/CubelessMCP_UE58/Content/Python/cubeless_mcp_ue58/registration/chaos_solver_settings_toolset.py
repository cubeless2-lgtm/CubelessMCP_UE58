"""Original logical Chaos Solver settings5/7 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_solver_settings as operation

@unreal.uclass()
class CubelessChaosSolverSettingsTools(unreal.ToolsetDefinition):
    'Inspect and configure allowlisted Chaos Solver settings with exact original world/selector/raw presence, optimistic expected_current, dry-run/no-op, complete settings readback, queue/save/semantic-restore prefix and Undo sync receipts. Enqueued physics commands are not applied-completion proof. No uncertain retry or atomic rollback.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_chaos_solver_settings(actor_name:str='""',actor_label:str='""',world_context:str='"auto"',include_runtime_state:str='true',max_results:str='16')->CubelessPythonOperationResult:
        'Inspect original typed Solver actors and configured/runtime diagnostic snapshots; no explicit mutation, compile or save. Preserve global CVar precedence and SDK unknown without claiming physics-frame atomicity.'
        rejected,report,raw=_guard_admission('inspect_chaos_solver_settings')
        if rejected is not None:return reflected_result(rejected)
        try:values={'actor_name':_decode_value(actor_name),'actor_label':_decode_value(actor_label),'world_context':_decode_value(world_context),'include_runtime_state':_decode_value(include_runtime_state),'max_results':_decode_value(max_results)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_chaos_solver_settings',error,report,raw))
        return reflected_result(operation._named_mcp('inspect_chaos_solver_settings',values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_chaos_solver_settings(settings:str,actor_name:str='""',actor_label:str='""',world_context:str='"editor"',expected_current:str='null',dry_run:str='true',save_level:str='false')->CubelessPythonOperationResult:
        'Plan or apply the original allowlisted solver patch with expected-current, full preflight, no-op, save and semantic-restore receipts. Preserve partial queue/property/dirty/save and Undo sync; enqueued physics work is not proven applied. No uncertain replay or atomic rollback.'
        rejected,report,raw=_guard_admission('configure_chaos_solver_settings')
        if rejected is not None:return reflected_result(rejected)
        try:values={'settings':_decode_value(settings),'actor_name':_decode_value(actor_name),'actor_label':_decode_value(actor_label),'world_context':_decode_value(world_context),'expected_current':_decode_value(expected_current),'dry_run':_decode_value(dry_run),'save_level':_decode_value(save_level)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_chaos_solver_settings',error,report,raw))
        return reflected_result(operation._named_mcp('configure_chaos_solver_settings',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosSolverSettingsAttachmentLibrary', None), 'verify_chaos_solver_settings_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosSolverSettings adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosSolverSettingsTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_solver_settings_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosSolverSettings adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('chaos_solver_settings_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_solver_settings_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('chaos_solver_settings_registration_carrier_invalid',str(error)))
    result['stage']='chaos_solver_settings_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
