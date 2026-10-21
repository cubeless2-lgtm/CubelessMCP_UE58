"""HD PCG public13/15: Named Refresh async final and Bind synchronous standard11; raw/ordinary Batch remain distinct."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import pcg_component_generation as domain
from cubeless_mcp_ue58.operations import pcg_component_generation_async as async_domain

@unreal.uclass()
class CubelessPCGComponentGenerationTools(unreal.ToolsetDefinition):
    "PCG component refresh/readiness and exact Blueprint runtime-helper binding; preserve separate wait, partial and persistence states."
    @toolset_registry.tool_call
    @staticmethod
    def refresh_pcg_components(actor_name:str="\"\"",selected_only:str="false",cleanup:str="true",generate:str="true",refresh:str="false",force:str="true",wait_until_complete:str="false",timeout_seconds:str="10.0",poll_interval_seconds:str="0.05",min_generated_output_data_count:str="-1",min_managed_resource_count:str="-1",max_components:str="1000",native_first:str="true")->CubelessPythonAsyncOperationResult:
        "Refresh PCG components through the original Native-first route. A bounded Native first-generation profile admits one uniquely matched live instance of a saved owned direct Actor Blueprint and its exact native PCGComponent, using a saved owned CreatePoints CPU source graph in /Game/_MCP_Temp/.../CubelessTests/.... The first-generation/query path also admits an exact native StaticMeshActor with its own single instance PCGComponent and registered native StaticMeshComponent root, saved-clean runtime StaticMesh with finite positive-volume bounds, and the same CPU source graph. Actual mesh/graph files and host roles are observed; no source Blueprint is synthesized. Request cleanup=False, refresh=False and generate=True for either first-generation profile. Native-host cleanup/compound refresh and wider native host shapes remain incomplete; the cleanup profiles described below still require the original direct Actor Blueprint host. After completed generation, the same owned CPU profile also supports cleanup=True, generate=False and refresh=False through synchronous immediate cleanup; the Named route retains the Native immediate/remove defaults. Cleanup completion is separate from generation completion and from requested output/resource minima. The same already-generated owned CPU profile additionally admits cleanup=True, generate=True, refresh=False and explicit force=False with immediate cleanup/remove enabled and notifications disabled. This scoped sequence preserves actual synchronous cleanup completion before separately reporting generation task acceptance, pending work and observed completion; successful enqueue is not completed generation. The existing Named query-only continuation captures the composite source identity and replays neither writer. Cleanup completion remains recorded if later generation, polling or requested minima fail. actor_name remains an actor-label prefix, not an exact object path; max_components alone does not establish unique ownership. Named wait reuses the existing query-only continuation with the captured source identity and requested minima; no cleanup or generation writer is replayed. An enqueue return alone is not completed generation. The direct Actor Blueprint host additionally admits a saved parameterless native CreatePoints-to-WeightedStaticMeshSpawner-to-Output CPU graph. The stock selector owns1..32 positive entries with already-loaded saved-clean native StaticMesh and Material/MaterialInstanceConstant references; no target Actor override, postprocess functions, property overrides, packer instance, attribute material overrides, runtime virtual textures or nonnull physical-material override. Descriptor component classes and actual generated components are exact native ISM/HISM. Bounded point/resource counts come from actual topology and resources, not a fixed fixture count. Generate-only uses the same enqueue and query-only completion path. Immediate remove=true cleanup captures selected native ManagedISM resources and owned generated components before the call, retires them before dispatch, and checks fresh resource/owner/output inventories after return without reading retired objects. Native source identities/observed flags, mesh material slots, graph settings and saved source files are preserved, together with the original nonselected PCG/Scene/StaticMesh/Box owner components; exact native Box survivors retain authored extent, collision settings, attachment and transform. The standard Editor visualization Billboard survivor is handled as its own bounded native role with authored sprite/settings and unchanged owner/attachment; render cache identity is not an authored invariant. The same hard cleanup then nonforce generation continuation is reused; task acceptance is not completion. Soft remove=false for these constructed resources, non-immediate cleanup, custom/other resource classes, wider hosts/graphs, arbitrary material runtime state, regeneration outside this exact composite, notification/refresh, runtime/partitioned/GPU effects and global callback purity remain valid original scope not yet implemented. Actual Native writer prefixes and partial/unknown state are preserved without automatic retry, rollback, explicit compile or save."
        rejected,report,raw=_guard_admission("refresh_pcg_components")
        if rejected is not None:return async_domain.completed_result(rejected)
        try:arguments={"actor_name":_decode_value(actor_name),"selected_only":_decode_value(selected_only),"cleanup":_decode_value(cleanup),"generate":_decode_value(generate),"refresh":_decode_value(refresh),"force":_decode_value(force),"wait_until_complete":_decode_value(wait_until_complete),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds),"min_generated_output_data_count":_decode_value(min_generated_output_data_count),"min_managed_resource_count":_decode_value(min_managed_resource_count),"max_components":_decode_value(max_components),"native_first":_decode_value(native_first)}
        except Exception as error:return async_domain.completed_result(_carrier_rejected("refresh_pcg_components",error,report,raw))
        return async_domain.begin(arguments)

    @toolset_registry.tool_call
    @staticmethod
    def bind_pcg_generation_settings_to_blueprint(blueprint_name:str,component_variable:str="\"ForestPCG\"",generation_trigger_variable:str="\"PCGGenerationTrigger\"",regenerate_in_editor_variable:str="\"PCGRegenerateInEditor\"",generate_on_drop_variable:str="\"\"",generate_on_drop_default:str="false",graph_name:str="\"UserConstructionScript\"",graph_id:str="\"\"",graph_type:str="\"function\"",replace_existing:str="true",sync_component_template_defaults:str="true",function_class:str="\"\"",function_name:str="\"ApplyPCGGenerationSettings\"",compile:str="false",save:str="false")->CubelessPythonOperationResult:
        "Bind a caller-owned runtime PCG settings function to an exact Blueprint graph; preserve actual partial compile/save receipts."
        rejected,report,raw=_guard_admission("bind_pcg_generation_settings_to_blueprint")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_name":_decode_value(blueprint_name),"component_variable":_decode_value(component_variable),"generation_trigger_variable":_decode_value(generation_trigger_variable),"regenerate_in_editor_variable":_decode_value(regenerate_in_editor_variable),"generate_on_drop_variable":_decode_value(generate_on_drop_variable),"generate_on_drop_default":_decode_value(generate_on_drop_default),"graph_name":_decode_value(graph_name),"graph_id":_decode_value(graph_id),"graph_type":_decode_value(graph_type),"replace_existing":_decode_value(replace_existing),"sync_component_template_defaults":_decode_value(sync_component_template_defaults),"function_class":_decode_value(function_class),"function_name":_decode_value(function_name),"compile":_decode_value(compile),"save":_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected("bind_pcg_generation_settings_to_blueprint",error,report,raw))
        return reflected_result(domain._named_mcp("bind_pcg_generation_settings_to_blueprint",arguments))


    @toolset_registry.tool_call
    @staticmethod
    def configure_blueprint_pcg_template(blueprint_path:str,component_name:str,graph_path:str,regenerate_in_editor:str="false",generate_on_drop_when_on_demand:str="false",compile:str="true",save:str="true",operation_id:str="\"\"")->CubelessPythonOperationResult:
        "Configure one exact local native PCGComponent template with a graph and GenerateOnDemand policy; retain actual callback/compile/save/dependency outcomes in one durable journal. No generation, rollback or automatic retry. Policy defaults and exact-component selection are explicit extensions of the original embedded helper."
        rejected,report,raw=_guard_admission("configure_blueprint_pcg_template")
        if rejected is not None:return reflected_result(rejected)
        try:arguments={"blueprint_path":_decode_value(blueprint_path),"component_name":_decode_value(component_name),"graph_path":_decode_value(graph_path),"regenerate_in_editor":_decode_value(regenerate_in_editor),"generate_on_drop_when_on_demand":_decode_value(generate_on_drop_when_on_demand),"compile":_decode_value(compile),"save":_decode_value(save),"operation_id":_decode_value(operation_id)}
        except Exception as error:return reflected_result(_carrier_rejected("configure_blueprint_pcg_template",error,report,raw))
        return reflected_result(domain.configure_blueprint_pcg_template(**arguments))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPCGComponentGenerationAttachmentLibrary', None), 'verify_pcg_component_generation_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact PCGComponentGeneration adapter raw-input admission helper is unavailable')
        raw = helper(CubelessPCGComponentGenerationTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_pcg_component_generation_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded PCGComponentGeneration adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('pcg_component_generation_raw_guard_admission_missing', message))
    result['stage'] = 'pcg_component_generation_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('pcg_component_generation_registration_carrier_invalid',str(error)))
    result['stage']='pcg_component_generation_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
