"""Original logical Niagara guarded input paths3 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_module_input_paths as domain

@unreal.uclass()
class CubelessNiagaraModuleInputPathTools(unreal.ToolsetDefinition):
    'Three dedicated Niagara input-path APIs preserve proven unused User/Module/Custom HLSL removal and separate scalar versus Array DI User binding. Guarded dry-run exact GUID/hash/type discovery, apply validation/deletion order, physical selectors, overwrite/default handling, raw hidden scalar requirement and forced Array DI dispatch remain distinct. Actual mutation/compile/saveTrue and SDK unknown prefixes are adopted before parent failure. Named removal and scalar/Array User binding share the existing HN Job, parent lease and final future; Direct/raw/ordinary Batch retain their original synchronous wait limitations; no retry or atomic rollback or Cubeless runtime asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_unused_module_input_path(system_path:str,scratch_pad_name:str,module_name:str,module_node_guid:str,input_name:str,linked_user_parameter_name:str,custom_hlsl_signature_name:str,expected_hlsl_identifier_absent:str,custom_hlsl_input_name:str='""',scratch_pad_script_path:str='""',expected_hlsl_body_hash:str='""',expected_custom_hlsl_node_guid:str='""',expected_override_node_guid:str='""',expected_input_type:str='""',expected_user_source_node_guid:str='""',expected_scratch_map_get_node_guids:str='null',dry_run:str='true',allow_source_edit:str='false',save:str='false',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated remove_niagara_unused_module_input_path preserves original public admission, direct passed-as-received values and raw presence. Dry-run returns exact GUID/hash/type contract; apply requires full absence/compile proof before save. Actual partial/saveTrue/unknown is retained before failure. Named dry-run/apply return the existing HN same-Job typed future; apply requires the original strict baseline/compile/absence gate before optional save. Timeout is a Native gate failure, not successful observation; Direct/raw/ordinary Batch retain prewriter apply refusal. No automatic retry or atomic rollback is claimed.'
        rejected,report,raw=_guard_admission('remove_niagara_unused_module_input_path')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'scratch_pad_name':_decode_value(scratch_pad_name),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'input_name':_decode_value(input_name),'linked_user_parameter_name':_decode_value(linked_user_parameter_name),'custom_hlsl_signature_name':_decode_value(custom_hlsl_signature_name),'expected_hlsl_identifier_absent':_decode_value(expected_hlsl_identifier_absent),'custom_hlsl_input_name':_decode_value(custom_hlsl_input_name),'scratch_pad_script_path':_decode_value(scratch_pad_script_path),'expected_hlsl_body_hash':_decode_value(expected_hlsl_body_hash),'expected_custom_hlsl_node_guid':_decode_value(expected_custom_hlsl_node_guid),'expected_override_node_guid':_decode_value(expected_override_node_guid),'expected_input_type':_decode_value(expected_input_type),'expected_user_source_node_guid':_decode_value(expected_user_source_node_guid),'expected_scratch_map_get_node_guids':_decode_value(expected_scratch_map_get_node_guids),'dry_run':_decode_value(dry_run),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('remove_niagara_unused_module_input_path',error,report,raw))
        return domain._named_mcp('remove_niagara_unused_module_input_path',values)

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_input_user_parameter(system_path:str,input_name:str,user_parameter_name:str,default_value:str='null',emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',overwrite_existing:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated set_niagara_module_input_user_parameter preserves original public admission, direct passed-as-received values and raw presence. Scalar/Array DI types, physical selectors, User default omission, overwrite/source-link checks and actual save-before-wait remain distinct. Actual partial/saveTrue/unknown is retained before failure; Named uses the existing HN same-Job finite wait and returns its exact typed future; timeout diagnostics are not compiled success. Direct/raw/ordinary Batch retain their synchronous admission limits. No automatic retry or atomic rollback is claimed.'
        rejected,report,raw=_guard_admission('set_niagara_module_input_user_parameter')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'user_parameter_name':_decode_value(user_parameter_name),'default_value':_decode_value(default_value),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'overwrite_existing':_decode_value(overwrite_existing),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('set_niagara_module_input_user_parameter',error,report,raw))
        return domain._named_mcp('set_niagara_module_input_user_parameter',values)

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_array_data_interface_module_input_user_parameter(system_path:str,input_name:str,user_parameter_name:str,emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',overwrite_existing:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated set_niagara_array_data_interface_module_input_user_parameter preserves original public admission, direct passed-as-received values and raw presence. Scalar/Array DI types, physical selectors, User default omission, overwrite/source-link checks and actual save-before-wait remain distinct. Actual partial/saveTrue/unknown is retained before failure; Named uses the existing HN same-Job finite wait and returns its exact typed future; timeout diagnostics are not compiled success. Direct/raw/ordinary Batch retain their synchronous admission limits. No automatic retry or atomic rollback is claimed.'
        rejected,report,raw=_guard_admission('set_niagara_array_data_interface_module_input_user_parameter')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'user_parameter_name':_decode_value(user_parameter_name),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'overwrite_existing':_decode_value(overwrite_existing),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('set_niagara_array_data_interface_module_input_user_parameter',error,report,raw))
        return domain._named_mcp('set_niagara_array_data_interface_module_input_user_parameter',values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraModuleInputPathAttachmentLibrary', None), 'verify_niagara_module_input_paths_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraModuleInputPath adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraModuleInputPathTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_module_input_paths_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraModuleInputPath adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_module_input_paths_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_module_input_paths_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_module_input_paths_registration_carrier_invalid',str(error)))
    result['stage']='niagara_module_input_paths_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
