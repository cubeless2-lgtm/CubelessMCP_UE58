"""Original logical Niagara emitter7 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_emitter_configuration as domain

@unreal.uclass()
class CubelessNiagaraEmitterConfigurationTools(unreal.ToolsetDefinition):
    'Seven dedicated Editor-only Niagara emitter APIs preserve original physical handle selectors, local-space, CPU/GPU and automatic fixed bounds, exact bounds, Persistent IDs no-op and save gate, handle enabled state and GUID removal. Native owns current compile/save/partial and SDK unknown receipts. Changing Persistent IDs with actual request_compile and wait_for_completion is explicitly unsupported before writers pending nonblocking wait/save continuation; compileFalse wait has no effect and unchanged may return no-op. No fake timeout, retry or atomic rollback. No Cubeless runtime dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_emitter_properties(system_path:str,emitter_index:str='null',emitter_name:str='""')->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_emitter_properties preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('inspect_niagara_emitter_properties')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_emitter_properties',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_emitter_properties',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_emitter_enabled(system_path:str,enabled:str,emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_emitter_enabled preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_emitter_enabled')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'enabled':_decode_value(enabled),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_emitter_enabled',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_emitter_enabled',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_emitter_local_space(system_path:str,value:str,emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_emitter_local_space preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_emitter_local_space')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'value':_decode_value(value),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_emitter_local_space',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_emitter_local_space',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_emitter_sim_target(system_path:str,sim_target:str,auto_fixed_bounds:str='true',emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_emitter_sim_target preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_emitter_sim_target')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'sim_target':_decode_value(sim_target),'auto_fixed_bounds':_decode_value(auto_fixed_bounds),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_emitter_sim_target',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_emitter_sim_target',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_emitter_fixed_bounds(system_path:str,min:str='null',max:str='null',mode:str='""',emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_emitter_fixed_bounds preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_emitter_fixed_bounds')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'min':_decode_value(min),'max':_decode_value(max),'mode':_decode_value(mode),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_emitter_fixed_bounds',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_emitter_fixed_bounds',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_emitter_persistent_ids(system_path:str,enabled:str,emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='60',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated set_niagara_emitter_persistent_ids preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('set_niagara_emitter_persistent_ids')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'enabled':_decode_value(enabled),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('set_niagara_emitter_persistent_ids',error,report,raw))
        return domain._named_mcp('set_niagara_emitter_persistent_ids',values)

    @toolset_registry.tool_call
    @staticmethod
    def remove_niagara_emitter(system_path:str,emitter_index:str='null',emitter_name:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated remove_niagara_emitter preserves original public/direct/raw input, selector, Engine mutation/readback and partial compile/save result contracts. Actual Engine verification is separate.'
        rejected,report,raw=_guard_admission('remove_niagara_emitter')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('remove_niagara_emitter',error,report,raw))
        return reflected_result(domain._named_mcp('remove_niagara_emitter',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraEmitterConfigurationAttachmentLibrary', None), 'verify_niagara_emitter_configuration_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraEmitterConfiguration adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraEmitterConfigurationTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_emitter_configuration_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraEmitterConfiguration adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_emitter_configuration_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_emitter_configuration_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_emitter_configuration_registration_carrier_invalid',str(error)))
    result['stage']='niagara_emitter_configuration_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
