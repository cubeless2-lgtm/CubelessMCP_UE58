"""Original logical Chaos learning5/5 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_learning as domain

@unreal.uclass()
class CubelessChaosLearningTools(unreal.ToolsetDefinition):
    'Dedicated Chaos learning fixture5/cache5 with original models/defaults and raw presence. Fixture default dry_run=True is observational; apply creates/reuses fixed-root native assets and optional owned world and reports actual save/partial state. Cache inspect is observational; nine legacy controlled operations and cumulative session state remain distinct from current request save and from FW natural simulation proof. Engine/API owns lifetime/SDKunknown/noReplay; no automatic retry, atomic rollback or packaged completion claim.'
    @toolset_registry.tool_call
    @staticmethod
    def create_chaos_destruction_learning_fixture(dry_run:str='true',force_rebuild:str='false',save_assets:str='true',configure_current_level:str='false',asset_root:str='"/Game/_MCP_Temp/ChaosDestructionLearning"')->CubelessPythonOperationResult:
        'Create or dry-run the fixed-root Chaos learning fixture with native Engine classes. Preserve force_rebuild/reference guards, unique integrity criteria, actual partial save and optional level scope.'
        rejected,report,raw=_guard_admission('create_chaos_destruction_learning_fixture')
        if rejected is not None:return reflected_result(rejected)
        try:values={'dry_run':_decode_value(dry_run),'force_rebuild':_decode_value(force_rebuild),'save_assets':_decode_value(save_assets),'configure_current_level':_decode_value(configure_current_level),'asset_root':_decode_value(asset_root)}
        except Exception as error:return reflected_result(_carrier_rejected('create_chaos_destruction_learning_fixture',error,report,raw))
        return reflected_result(domain._named_mcp('create_chaos_destruction_learning_fixture',values))

    @toolset_registry.tool_call
    @staticmethod
    def run_chaos_cache_smoke(operation:str='"inspect"',cache_collection_path:str='"/Game/_MCP_Temp/ChaosDestructionLearning/CC_ChaosLearning"',cache_manager_actor_name:str='""',component_actor_name:str='""',cache_name:str='"ChaosLearning"')->CubelessPythonOperationResult:
        'Operate the original tagged sixteen-component cache diagnostic: inspect/configure/trigger/record/finalize/playback/sample. cache_name is a compatibility field ignored by the original Native implementation; controlled velocity/kinematic changes do not prove natural playback.'
        rejected,report,raw=_guard_admission('run_chaos_cache_smoke')
        if rejected is not None:return reflected_result(rejected)
        try:values={'operation':_decode_value(operation),'cache_collection_path':_decode_value(cache_collection_path),'cache_manager_actor_name':_decode_value(cache_manager_actor_name),'component_actor_name':_decode_value(component_actor_name),'cache_name':_decode_value(cache_name)}
        except Exception as error:return reflected_result(_carrier_rejected('run_chaos_cache_smoke',error,report,raw))
        return reflected_result(domain._named_mcp('run_chaos_cache_smoke',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosLearningAttachmentLibrary', None), 'verify_chaos_learning_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosLearning adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosLearningTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_learning_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosLearning adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('chaos_learning_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_learning_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('chaos_learning_registration_carrier_invalid',str(error)))
    result['stage']='chaos_learning_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
