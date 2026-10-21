"""Original logical content reconstruction4 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import content_reconstruction as domain

@unreal.uclass()
class CubelessContentReconstructionTools(unreal.ToolsetDefinition):
    'Four dedicated content reconstruction Editor APIs preserve fresh recreation and accepted/unresolved fallback, reference remap, Blueprint/struct compile passes, world repair, log health and report output. Named original model coercion then unchanged Python values; raw hidden fields, omission and locale conversion remain Native-owned. Actual asset save, compile, report writes and partial child receipts are separate; dry_run may write reports. No automatic retry or atomic rollback, no Cubeless runtime dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def recreate_content_folder_mcp(source_root:str='"/Game/UltraDynamicSky"',target_root:str='"/Game/_MCP_Temp/UltraDynamicSky_MCP"',suffix:str='"_MCP"',allow_fallback_duplicate:str='true',dry_run:str='false',overwrite_existing:str='false',delete_target_root_first:str='false',remap_references:str='true',compile_blueprints:str='true',log_archive_remap_objects:str='false',refresh_blueprint_nodes:str='false',refresh_failed_blueprint_nodes:str='true',save_assets:str='true',repair_level_actors:str='true',force_repair_level_actors:str='true',compile_passes:str='2',check_editor_log_health:str='true',fail_on_editor_log_issues:str='true',suppress_editor_prompts:str='true')->CubelessPythonOperationResult:
        'Dedicated recreate_content_folder_mcp with original composite, validation, compile/save/report and partial-state contracts. Actual Engine behavior requires separate verification.'
        rejected,report,raw=_guard_admission('recreate_content_folder_mcp')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_root':_decode_value(source_root),'target_root':_decode_value(target_root),'suffix':_decode_value(suffix),'allow_fallback_duplicate':_decode_value(allow_fallback_duplicate),'dry_run':_decode_value(dry_run),'overwrite_existing':_decode_value(overwrite_existing),'delete_target_root_first':_decode_value(delete_target_root_first),'remap_references':_decode_value(remap_references),'compile_blueprints':_decode_value(compile_blueprints),'log_archive_remap_objects':_decode_value(log_archive_remap_objects),'refresh_blueprint_nodes':_decode_value(refresh_blueprint_nodes),'refresh_failed_blueprint_nodes':_decode_value(refresh_failed_blueprint_nodes),'save_assets':_decode_value(save_assets),'repair_level_actors':_decode_value(repair_level_actors),'force_repair_level_actors':_decode_value(force_repair_level_actors),'compile_passes':_decode_value(compile_passes),'check_editor_log_health':_decode_value(check_editor_log_health),'fail_on_editor_log_issues':_decode_value(fail_on_editor_log_issues),'suppress_editor_prompts':_decode_value(suppress_editor_prompts)}
        except Exception as error:return reflected_result(_carrier_rejected('recreate_content_folder_mcp',error,report,raw))
        return reflected_result(domain._named_mcp('recreate_content_folder_mcp',values))

    @toolset_registry.tool_call
    @staticmethod
    def postprocess_content_folder_mcp(source_root:str='"/Game/UltraDynamicSky"',target_root:str='"/Game/_MCP_Temp/UltraDynamicSky_MCP"',suffix:str='"_MCP"',dry_run:str='false',remap_references:str='true',compile_blueprints:str='true',refresh_blueprint_nodes:str='false',refresh_failed_blueprint_nodes:str='true',save_assets:str='true',repair_level_actors:str='false',force_repair_level_actors:str='false',compile_passes:str='2',check_editor_log_health:str='true',fail_on_editor_log_issues:str='true',suppress_editor_prompts:str='true')->CubelessPythonOperationResult:
        'Dedicated postprocess_content_folder_mcp with original composite, validation, compile/save/report and partial-state contracts. Actual Engine behavior requires separate verification.'
        rejected,report,raw=_guard_admission('postprocess_content_folder_mcp')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_root':_decode_value(source_root),'target_root':_decode_value(target_root),'suffix':_decode_value(suffix),'dry_run':_decode_value(dry_run),'remap_references':_decode_value(remap_references),'compile_blueprints':_decode_value(compile_blueprints),'refresh_blueprint_nodes':_decode_value(refresh_blueprint_nodes),'refresh_failed_blueprint_nodes':_decode_value(refresh_failed_blueprint_nodes),'save_assets':_decode_value(save_assets),'repair_level_actors':_decode_value(repair_level_actors),'force_repair_level_actors':_decode_value(force_repair_level_actors),'compile_passes':_decode_value(compile_passes),'check_editor_log_health':_decode_value(check_editor_log_health),'fail_on_editor_log_issues':_decode_value(fail_on_editor_log_issues),'suppress_editor_prompts':_decode_value(suppress_editor_prompts)}
        except Exception as error:return reflected_result(_carrier_rejected('postprocess_content_folder_mcp',error,report,raw))
        return reflected_result(domain._named_mcp('postprocess_content_folder_mcp',values))

    @toolset_registry.tool_call
    @staticmethod
    def repair_world_actor_instances_mcp(map_path:str,source_root:str='"/Game/UltraDynamicSky"',target_root:str='"/Game/_MCP_Temp/UltraDynamicSky_MCP"',suffix:str='"_MCP"',dry_run:str='false',save_map:str='true',remap_level_blueprint:str='false',repair_actor_classes:str='true',repair_component_classes:str='true',remap_actor_references:str='true',remove_source_object_map_keys:str='true',check_editor_log_health:str='true',fail_on_editor_log_issues:str='true',suppress_editor_prompts:str='true')->CubelessPythonOperationResult:
        'Dedicated repair_world_actor_instances_mcp with original composite, validation, compile/save/report and partial-state contracts. Actual Engine behavior requires separate verification.'
        rejected,report,raw=_guard_admission('repair_world_actor_instances_mcp')
        if rejected is not None:return reflected_result(rejected)
        try:values={'map_path':_decode_value(map_path),'source_root':_decode_value(source_root),'target_root':_decode_value(target_root),'suffix':_decode_value(suffix),'dry_run':_decode_value(dry_run),'save_map':_decode_value(save_map),'remap_level_blueprint':_decode_value(remap_level_blueprint),'repair_actor_classes':_decode_value(repair_actor_classes),'repair_component_classes':_decode_value(repair_component_classes),'remap_actor_references':_decode_value(remap_actor_references),'remove_source_object_map_keys':_decode_value(remove_source_object_map_keys),'check_editor_log_health':_decode_value(check_editor_log_health),'fail_on_editor_log_issues':_decode_value(fail_on_editor_log_issues),'suppress_editor_prompts':_decode_value(suppress_editor_prompts)}
        except Exception as error:return reflected_result(_carrier_rejected('repair_world_actor_instances_mcp',error,report,raw))
        return reflected_result(domain._named_mcp('repair_world_actor_instances_mcp',values))

    @toolset_registry.tool_call
    @staticmethod
    def run_content_validation_pipeline_mcp(source_root:str,target_root:str,suffix:str='"_MCP"',map_path:str='""',continue_on_failure:str='false',run_world_repair:str='false',allow_fallback_duplicate:str='true',dry_run:str='false',overwrite_existing:str='true',delete_target_root_first:str='true',remap_references:str='true',compile_blueprints:str='true',refresh_blueprint_nodes:str='false',refresh_failed_blueprint_nodes:str='true',save_assets:str='true',repair_level_actors:str='true',force_repair_level_actors:str='true',compile_passes:str='3',check_editor_log_health:str='true',fail_on_editor_log_issues:str='true',suppress_editor_prompts:str='true',save_map:str='true',repair_actor_classes:str='true',repair_component_classes:str='true',remap_actor_references:str='true',remove_source_object_map_keys:str='true')->CubelessPythonOperationResult:
        'Dedicated run_content_validation_pipeline_mcp with original composite, validation, compile/save/report and partial-state contracts. Actual Engine behavior requires separate verification.'
        rejected,report,raw=_guard_admission('run_content_validation_pipeline_mcp')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_root':_decode_value(source_root),'target_root':_decode_value(target_root),'suffix':_decode_value(suffix),'map_path':_decode_value(map_path),'continue_on_failure':_decode_value(continue_on_failure),'run_world_repair':_decode_value(run_world_repair),'allow_fallback_duplicate':_decode_value(allow_fallback_duplicate),'dry_run':_decode_value(dry_run),'overwrite_existing':_decode_value(overwrite_existing),'delete_target_root_first':_decode_value(delete_target_root_first),'remap_references':_decode_value(remap_references),'compile_blueprints':_decode_value(compile_blueprints),'refresh_blueprint_nodes':_decode_value(refresh_blueprint_nodes),'refresh_failed_blueprint_nodes':_decode_value(refresh_failed_blueprint_nodes),'save_assets':_decode_value(save_assets),'repair_level_actors':_decode_value(repair_level_actors),'force_repair_level_actors':_decode_value(force_repair_level_actors),'compile_passes':_decode_value(compile_passes),'check_editor_log_health':_decode_value(check_editor_log_health),'fail_on_editor_log_issues':_decode_value(fail_on_editor_log_issues),'suppress_editor_prompts':_decode_value(suppress_editor_prompts),'save_map':_decode_value(save_map),'repair_actor_classes':_decode_value(repair_actor_classes),'repair_component_classes':_decode_value(repair_component_classes),'remap_actor_references':_decode_value(remap_actor_references),'remove_source_object_map_keys':_decode_value(remove_source_object_map_keys)}
        except Exception as error:return reflected_result(_carrier_rejected('run_content_validation_pipeline_mcp',error,report,raw))
        return reflected_result(domain._named_mcp('run_content_validation_pipeline_mcp',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessContentReconstructionAttachmentLibrary', None), 'verify_content_reconstruction_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ContentReconstruction adapter raw-input admission helper is unavailable')
        raw = helper(CubelessContentReconstructionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_content_reconstruction_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ContentReconstruction adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('content_reconstruction_raw_guard_admission_missing', message))
    result['stage'] = 'content_reconstruction_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('content_reconstruction_registration_carrier_invalid',str(error)))
    result['stage']='content_reconstruction_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
