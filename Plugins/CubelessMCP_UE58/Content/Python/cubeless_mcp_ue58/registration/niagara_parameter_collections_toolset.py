"""Original logical Niagara parameter collection6 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_parameter_collections as domain

@unreal.uclass()
class CubelessNiagaraParameterCollectionTools(unreal.ToolsetDefinition):
    'Six dedicated Editor-only Niagara parameter-collection APIs preserve default-store values and repair, first-match names, graph and VM references, script paths, actual partial compile/save prefixes and SDK global unknown. Nested values and original public/direct/raw admission remain distinct. Named script remap shares the existing HN Job/future for required global AssetCompilingManager and loaded NiagaraSystem completion before ordered script saves. Unknown pending-on-demand distinctions fail explicitly; Direct/raw/ordinary Batch retain prewriter wait refusal. No-change and compileFalse/waitFalse paths are retained. No retry, atomic rollback or Cubeless runtime asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_parameter_collection_values(collection_path:str,parameter_names:str='null',namespace:str='""',max_parameters:str='256')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_parameter_collection_values preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_parameter_collection_values')
        if rejected is not None:return reflected_result(rejected)
        try:values={'collection_path':_decode_value(collection_path),'parameter_names':_decode_value(parameter_names),'namespace':_decode_value(namespace),'max_parameters':_decode_value(max_parameters)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_parameter_collection_values',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_parameter_collection_values',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_parameter_collection_values(collection_path:str,values:str,namespace:str='""',allow_source_edit:str='false',save:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_parameter_collection_values preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_parameter_collection_values')
        if rejected is not None:return reflected_result(rejected)
        try:values={'collection_path':_decode_value(collection_path),'values':_decode_value(values),'namespace':_decode_value(namespace),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_parameter_collection_values',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_parameter_collection_values',values))

    @toolset_registry.tool_call
    @staticmethod
    def repair_niagara_parameter_collection(collection_path:str,allow_source_edit:str='false',save:str='true')->CubelessPythonOperationResult:
        'Dedicated repair_niagara_parameter_collection preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('repair_niagara_parameter_collection')
        if rejected is not None:return reflected_result(rejected)
        try:values={'collection_path':_decode_value(collection_path),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('repair_niagara_parameter_collection',error,report,raw))
        return reflected_result(domain._named_mcp('repair_niagara_parameter_collection',values))

    @toolset_registry.tool_call
    @staticmethod
    def remap_niagara_parameter_collection_references(system_path:str,new_collection_path:str,old_collection_path:str='""',old_namespace:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',replace_object_references:str='false')->CubelessPythonOperationResult:
        'Dedicated remap_niagara_parameter_collection_references preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('remap_niagara_parameter_collection_references')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'new_collection_path':_decode_value(new_collection_path),'old_collection_path':_decode_value(old_collection_path),'old_namespace':_decode_value(old_namespace),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'replace_object_references':_decode_value(replace_object_references)}
        except Exception as error:return reflected_result(_carrier_rejected('remap_niagara_parameter_collection_references',error,report,raw))
        return reflected_result(domain._named_mcp('remap_niagara_parameter_collection_references',values))

    @toolset_registry.tool_call
    @staticmethod
    def remap_niagara_script_parameter_collection_references(new_collection_path:str,old_collection_path:str='""',old_namespace:str='""',script_path:str='""',script_paths:str='null',root_path:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true')->CubelessPythonAsyncOperationResult:
        'Dedicated remap_niagara_script_parameter_collection_references preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Named returns its exact same-Job future; global pending/compile/save authority stays Native and no timeout or local-idle success is invented. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('remap_niagara_script_parameter_collection_references')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'new_collection_path':_decode_value(new_collection_path),'old_collection_path':_decode_value(old_collection_path),'old_namespace':_decode_value(old_namespace),'script_path':_decode_value(script_path),'script_paths':_decode_value(script_paths),'root_path':_decode_value(root_path),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion)}
        except Exception as error:return domain.completed_result(_carrier_rejected('remap_niagara_script_parameter_collection_references',error,report,raw))
        return domain._named_mcp('remap_niagara_script_parameter_collection_references',values)

    @toolset_registry.tool_call
    @staticmethod
    def recompile_niagara_scripts_in_root(root_path:str='""',script_paths:str='null',allow_source_compile:str='false',force:str='true',save:str='true')->CubelessPythonOperationResult:
        'Dedicated recompile_niagara_scripts_in_root preserves original public/direct/raw input, ordered partial mutation, actual compile/save and unknown receipts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('recompile_niagara_scripts_in_root')
        if rejected is not None:return reflected_result(rejected)
        try:values={'root_path':_decode_value(root_path),'script_paths':_decode_value(script_paths),'allow_source_compile':_decode_value(allow_source_compile),'force':_decode_value(force),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('recompile_niagara_scripts_in_root',error,report,raw))
        return reflected_result(domain._named_mcp('recompile_niagara_scripts_in_root',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraParameterCollectionAttachmentLibrary', None), 'verify_niagara_parameter_collections_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraParameterCollection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraParameterCollectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_parameter_collections_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraParameterCollection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_parameter_collections_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_parameter_collections_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_parameter_collections_registration_carrier_invalid',str(error)))
    result['stage']='niagara_parameter_collections_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
