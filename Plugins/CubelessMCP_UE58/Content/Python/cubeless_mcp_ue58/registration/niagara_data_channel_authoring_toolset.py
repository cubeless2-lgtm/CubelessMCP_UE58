"""Original logical Niagara guarded input paths3 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_data_channel_authoring as domain

@unreal.uclass()
class CubelessNiagaraDataChannelAuthoringTools(unreal.ToolsetDefinition):
    'Three original public Data Channel APIs preserve layout, reader graph authoring and guarded int variadic extension. A fourth Native-only DI-call remains a finite raw Batch action, without an invented public signature. Named variables omission differs from raw empty-layout clear. Current partial, compile and save receipts precede parent failure. Required wait, historical Success capture and direct-index reader variants remain incomplete where unsupported; no retry, atomic rollback or Cubeless runtime asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def configure_niagara_data_channel_asset(asset_path:str,variables:str='null',create_if_missing:str='true',keep_previous_frame_data:str='true',save:str='true')->CubelessPythonOperationResult:
        'Dedicated configure_niagara_data_channel_asset preserves original public model/defaults and direct Python/raw presence distinctions. Actual layout/graph/typed signature, compile/saveTrue and partial unknown receipts precede parent failure. Required wait, historical Success capture and direct-index reader remain incomplete where unsupported; no automatic retry or atomic rollback.'
        rejected,report,raw=_guard_admission('configure_niagara_data_channel_asset')
        if rejected is not None:return reflected_result(rejected)
        try:values={'asset_path':_decode_value(asset_path),'variables':_decode_value(variables),'create_if_missing':_decode_value(create_if_missing),'keep_previous_frame_data':_decode_value(keep_previous_frame_data),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_niagara_data_channel_asset',error,report,raw))
        return reflected_result(domain._named_mcp('configure_niagara_data_channel_asset',values))

    @toolset_registry.tool_call
    @staticmethod
    def create_niagara_ndc_reader_scratch_pad_module(system_path:str,scratch_pad_name:str,data_channel_asset_path:str,stage:str='"particle_spawn_read"',position_variable:str='"Position"',velocity_variable:str='"Velocity"',spawn_count_variable:str='"SpawnCount"',event_type_variable:str='"EventType"',position_target:str='"Particles.Position"',velocity_target:str='"Particles.Velocity"',spawn_count_target:str='"Particles.NDCSpawnCount"',event_type_target:str='"Particles.NDCEventType"',read_success_target:str='"Particles.NDCReadSuccess"',read_current_frame:str='false',dry_run:str='false',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated create_niagara_ndc_reader_scratch_pad_module preserves original public model/defaults and direct Python/raw presence distinctions. Actual layout/graph/typed signature, compile/saveTrue and partial unknown receipts precede parent failure. Required wait, historical Success capture and direct-index reader remain incomplete where unsupported; no automatic retry or atomic rollback.'
        rejected,report,raw=_guard_admission('create_niagara_ndc_reader_scratch_pad_module')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'scratch_pad_name':_decode_value(scratch_pad_name),'data_channel_asset_path':_decode_value(data_channel_asset_path),'stage':_decode_value(stage),'position_variable':_decode_value(position_variable),'velocity_variable':_decode_value(velocity_variable),'spawn_count_variable':_decode_value(spawn_count_variable),'event_type_variable':_decode_value(event_type_variable),'position_target':_decode_value(position_target),'velocity_target':_decode_value(velocity_target),'spawn_count_target':_decode_value(spawn_count_target),'event_type_target':_decode_value(event_type_target),'read_success_target':_decode_value(read_success_target),'read_current_frame':_decode_value(read_current_frame),'dry_run':_decode_value(dry_run),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('create_niagara_ndc_reader_scratch_pad_module',error,report,raw))
        return reflected_result(domain._named_mcp('create_niagara_ndc_reader_scratch_pad_module',values))

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_scratch_pad_di_variadic_input(system_path:str,scratch_pad_name:str,function_node_guid:str,expected_function_name:str,expected_di_class:str,expected_existing_inputs:str,pin_name:str,module_input_name:str,expected_referencing_module_node_guid:str,default_value:str='0',input_type:str='"int"',scratch_pad_script_path:str='""',expected_input_map_node_guid:str='""',expected_output_node_guid:str='""',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='true',timeout_seconds:str='60',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Dedicated variadic input preserves original arguments/defaults and final standard11. Named compile/wait/save uses the owned Native job and official async result; actual partial, saved and unknown prefixes remain journaled. Ordinary Batch required async continuation, semantic recovery/reload, historical Success capture and direct-index reader remain incomplete; no automatic retry or atomic rollback.'
        rejected,report,raw=_guard_admission('add_niagara_scratch_pad_di_variadic_input')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'scratch_pad_name':_decode_value(scratch_pad_name),'function_node_guid':_decode_value(function_node_guid),'expected_function_name':_decode_value(expected_function_name),'expected_di_class':_decode_value(expected_di_class),'expected_existing_inputs':_decode_value(expected_existing_inputs),'pin_name':_decode_value(pin_name),'module_input_name':_decode_value(module_input_name),'expected_referencing_module_node_guid':_decode_value(expected_referencing_module_node_guid),'default_value':_decode_value(default_value),'input_type':_decode_value(input_type),'scratch_pad_script_path':_decode_value(scratch_pad_script_path),'expected_input_map_node_guid':_decode_value(expected_input_map_node_guid),'expected_output_node_guid':_decode_value(expected_output_node_guid),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('add_niagara_scratch_pad_di_variadic_input',error,report,raw))
        return domain._named_mcp('add_niagara_scratch_pad_di_variadic_input',values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraDataChannelAuthoringAttachmentLibrary', None), 'verify_niagara_data_channel_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraDataChannelAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraDataChannelAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_data_channel_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraDataChannelAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_data_channel_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_data_channel_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_data_channel_authoring_registration_carrier_invalid',str(error)))
    result['stage']='niagara_data_channel_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
