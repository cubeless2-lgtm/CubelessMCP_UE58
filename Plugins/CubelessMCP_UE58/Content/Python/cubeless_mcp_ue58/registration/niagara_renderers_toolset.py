"""Original logical Niagara renderer7 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_renderers as domain

@unreal.uclass()
class CubelessNiagaraRendererTools(unreal.ToolsetDefinition):
    'Seven dedicated Niagara renderer Editor APIs preserve original UE5.6 enabled predicates and physical indices, type-specific material writes, Sprite fields, Sprite/Mesh add and no-op, enabled unique selection and expected-topology cleanup with descending removal. Original public model/nullable arrays/body omission differs from raw presence/mesh alias. Native owns actual compile/save/partial/SDK unknown receipts. Named cleanup uses the existing HN Job and genuine compile wait before optional save; timeout or pending compilation is a failure with partial changes and no save. Direct/raw/ordinary Batch genuine wait remains unsupported before mutation. No retry/atomic rollback or Cubeless runtime dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_renderers(system_path:str)->CubelessPythonOperationResult:
        'Dedicated inspect_niagara_renderers preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('inspect_niagara_renderers')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_niagara_renderers',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_niagara_renderers',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_renderer_material(system_path:str,material_path:str,emitter_index:str='null',emitter_name:str='""',renderer_index:str='null',material_slot_index:str='0',allow_source_edit:str='false',save:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_renderer_material preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('set_niagara_renderer_material')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'material_path':_decode_value(material_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'renderer_index':_decode_value(renderer_index),'material_slot_index':_decode_value(material_slot_index),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_renderer_material',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_renderer_material',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_sprite_renderer_settings(system_path:str,alignment:str='""',facing_mode:str='""',sub_image_size:str='null',emitter_index:str='null',emitter_name:str='""',renderer_index:str='null',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_sprite_renderer_settings preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('set_niagara_sprite_renderer_settings')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'alignment':_decode_value(alignment),'facing_mode':_decode_value(facing_mode),'sub_image_size':_decode_value(sub_image_size),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'renderer_index':_decode_value(renderer_index),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_sprite_renderer_settings',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_sprite_renderer_settings',values))

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_sprite_renderer(system_path:str,material_path:str,emitter_index:str='null',emitter_name:str='""',alignment:str='"Automatic"',facing_mode:str='"FaceCamera"',sub_image_size:str='null',skip_if_existing:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated add_niagara_sprite_renderer preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('add_niagara_sprite_renderer')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'material_path':_decode_value(material_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'alignment':_decode_value(alignment),'facing_mode':_decode_value(facing_mode),'sub_image_size':_decode_value(sub_image_size),'skip_if_existing':_decode_value(skip_if_existing),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('add_niagara_sprite_renderer',error,report,raw))
        return reflected_result(domain._named_mcp('add_niagara_sprite_renderer',values))

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_mesh_renderer(system_path:str,emitter_index:str='null',emitter_name:str='""',meshes_binding_parameter:str='"User.MeshList"',mesh_index_binding:str='"Particles.MeshIndex"',mesh_paths:str='null',skip_if_existing:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated add_niagara_mesh_renderer preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('add_niagara_mesh_renderer')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'meshes_binding_parameter':_decode_value(meshes_binding_parameter),'mesh_index_binding':_decode_value(mesh_index_binding),'mesh_paths':_decode_value(mesh_paths),'skip_if_existing':_decode_value(skip_if_existing),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('add_niagara_mesh_renderer',error,report,raw))
        return reflected_result(domain._named_mcp('add_niagara_mesh_renderer',values))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_renderer_enabled(system_path:str,enabled:str,emitter_index:str='null',emitter_name:str='""',renderer_index:str='null',allow_source_edit:str='false',save:str='true',request_compile:str='true')->CubelessPythonOperationResult:
        'Dedicated set_niagara_renderer_enabled preserves original selector, input/omission, typed Engine operation, compile/save and failure-state contracts. Engine behavior requires separate actual verification.'
        rejected,report,raw=_guard_admission('set_niagara_renderer_enabled')
        if rejected is not None:return reflected_result(rejected)
        try:values={'system_path':_decode_value(system_path),'enabled':_decode_value(enabled),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'renderer_index':_decode_value(renderer_index),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile)}
        except Exception as error:return reflected_result(_carrier_rejected('set_niagara_renderer_enabled',error,report,raw))
        return reflected_result(domain._named_mcp('set_niagara_renderer_enabled',values))

    @toolset_registry.tool_call
    @staticmethod
    def cleanup_niagara_renderers(system_path:str,remove_renderer_indices:str='null',clear_meshes_binding_renderer_indices:str='null',expected_renderers:str='null',expected_renderer_count:str='null',emitter_index:str='null',emitter_name:str='""',dry_run:str='true',allow_source_edit:str='false',save:str='true',request_compile:str='true',wait_for_completion:str='false',timeout_seconds:str='10',poll_interval_seconds:str='0.1')->CubelessPythonAsyncOperationResult:
        'Named cleanup preserves the original14 inputs and returns the same HN Job future. Native owns conditional compile/wait/save, strict timeout failure and actual partial state.'
        rejected,report,raw=_guard_admission('cleanup_niagara_renderers')
        if rejected is not None:return domain.completed_result(rejected)
        try:values={'system_path':_decode_value(system_path),'remove_renderer_indices':_decode_value(remove_renderer_indices),'clear_meshes_binding_renderer_indices':_decode_value(clear_meshes_binding_renderer_indices),'expected_renderers':_decode_value(expected_renderers),'expected_renderer_count':_decode_value(expected_renderer_count),'emitter_index':_decode_value(emitter_index),'emitter_name':_decode_value(emitter_name),'dry_run':_decode_value(dry_run),'allow_source_edit':_decode_value(allow_source_edit),'save':_decode_value(save),'request_compile':_decode_value(request_compile),'wait_for_completion':_decode_value(wait_for_completion),'timeout_seconds':_decode_value(timeout_seconds),'poll_interval_seconds':_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected('cleanup_niagara_renderers',error,report,raw))
        return domain._named_mcp('cleanup_niagara_renderers',values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraRendererAttachmentLibrary', None), 'verify_niagara_renderers_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraRenderer adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraRendererTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_renderers_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraRenderer adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_renderers_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_renderers_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_renderers_registration_carrier_invalid',str(error)))
    result['stage']='niagara_renderers_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
