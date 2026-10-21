"""Original logical Niagara cleanup6 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_input_parameters as domain

@unreal.uclass()
class CubelessNiagaraInputParameterTools(unreal.ToolsetDefinition):
    'Seven dedicated Niagara RI/static/module-input APIs preserve original selectors, Any JSON values, raw static scope aliases and existing-versus-create store distinction. Read-only queries retain actual total/matched/reported counts and SDK cache unknown. Writers retain actual partial compile/save receipt before failure. Static enum/hidden/current/no-op and affected compile-ID effect proof differ from RI save-before-wait diagnostics; required wait is not bypassed or scheduling-only success. Real async terminal lifetime remains separately tracked. No retry, atomic rollback, private Engine headers or Cubeless runtime asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_rapid_iteration_parameters(system_path:str,contains:str='null',max_parameters_per_script:str='256')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_rapid_iteration_parameters preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('inspect_niagara_rapid_iteration_parameters')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'contains':_decode_value(contains),'max_parameters_per_script':_decode_value(max_parameters_per_script)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_rapid_iteration_parameters',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_rapid_iteration_parameters',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_rapid_iteration_parameter(system_path:str,parameter_name:str,value:str,owner_kind:str='""',owner_name:str='""',script_usage:str='""',emitter_index:str='null',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_rapid_iteration_parameter preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('set_niagara_rapid_iteration_parameter')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'parameter_name':_decode_value(parameter_name),'value':_decode_value(value),'owner_kind':_decode_value(owner_kind),'owner_name':_decode_value(owner_name),'script_usage':_decode_value(script_usage),'emitter_index':_decode_value(emitter_index),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_rapid_iteration_parameter',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_rapid_iteration_parameter',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_static_switch(system_path:str,input_name:str='""',value:str='null',emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',expected_current_value:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='60',poll_interval_seconds:str='0.1',require_compile_gate_for_save:str='true')->CubelessPythonAsyncOperationResult:
        'Dedicated set_niagara_module_static_switch preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('set_niagara_module_static_switch')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'value':_decode_value(value),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'expected_current_value':_decode_value(expected_current_value),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds),'require_compile_gate_for_save':_decode_value(require_compile_gate_for_save)}
        except Exception as error:return domain.completed_result(_carrier_rejected('set_niagara_module_static_switch',error,report,raw))
        return domain._named_mcp('set_niagara_module_static_switch',values)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_module_inputs(system_path:str,include_linked_sources:str='true',include_resolved_stack_inputs:str='false',max_modules:str='200',max_candidates_per_module:str='24',max_resolved_inputs_per_module:str='8',max_top_candidates:str='80')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_module_inputs preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('inspect_niagara_module_inputs')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'include_linked_sources':_decode_value(include_linked_sources),'include_resolved_stack_inputs':_decode_value(include_resolved_stack_inputs),'max_modules':_decode_value(max_modules),'max_candidates_per_module':_decode_value(max_candidates_per_module),'max_resolved_inputs_per_module':_decode_value(max_resolved_inputs_per_module),'max_top_candidates':_decode_value(max_top_candidates)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_module_inputs',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_module_inputs',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_data_interface_overrides(system_path:str,input_name:str='""',emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',max_modules:str='200',max_inputs_per_module:str='64',include_data_interface_properties:str='false',max_data_interface_properties:str='120',max_data_interface_property_value_length:str='512')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_data_interface_overrides preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('inspect_niagara_data_interface_overrides')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'max_modules':_decode_value(max_modules),'max_inputs_per_module':_decode_value(max_inputs_per_module),'include_data_interface_properties':_decode_value(include_data_interface_properties),'max_data_interface_properties':_decode_value(max_data_interface_properties),'max_data_interface_property_value_length':_decode_value(max_data_interface_property_value_length)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_data_interface_overrides',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_data_interface_overrides',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_input_value(system_path:str,input_name:str,value:str,emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated set_niagara_module_input_value preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('set_niagara_module_input_value')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'value':_decode_value(value),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('set_niagara_module_input_value',error,report,raw))
        return domain._named_mcp('set_niagara_module_input_value',values)

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_module_input_override(system_path:str,input_name:str,value:str,emitter_index:str='null',emitter_name:str='""',module_index:str='null',module_name:str='""',module_node_guid:str='""',overwrite_existing:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated create_niagara_module_input_override preserves original Named/direct/raw selectors, original type/omission and compound verification/failure/compile/save ordering. Actual savedTrue/SDKunknown survives later failure; scheduling is not terminal completion and no automatic retry/rollback.'
        rejected,report,raw=_guard_admission('create_niagara_module_input_override')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'input_name':_decode_value(input_name),'value':_decode_value(value),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'module_index':_decode_value(module_index),'module_name':_decode_value(module_name),'module_node_guid':_decode_value(module_node_guid),'overwrite_existing':_decode_value(overwrite_existing),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('create_niagara_module_input_override',error,report,raw))
        return domain._named_mcp('create_niagara_module_input_override',values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraInputParameterAttachmentLibrary', None), 'verify_niagara_input_parameters_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraInputParameter adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraInputParameterTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_input_parameters_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraInputParameter adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_input_parameters_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_input_parameters_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_input_parameters_registration_carrier_invalid',str(error)))
    result['stage']='niagara_input_parameters_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
