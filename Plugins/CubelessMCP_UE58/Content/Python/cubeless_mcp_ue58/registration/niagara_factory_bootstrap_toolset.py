"""Original logical Niagara runtime inspection4 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import niagara_factory_bootstrap as domain

@unreal.uclass()
class CubelessNiagaraFactoryBootstrapTools(unreal.ToolsetDefinition):
    'Four original factory bootstrap contracts: owned transient defaults and fresh System/Emitter/Sprite asset creation. Actual transient work, compile enqueue, exact save history, partial prefix and SDK unknown remain authoritative. Named System2 keep one Native Job until bounded wait and optional save/audit finish; timeout remains a diagnostic and direct/raw genuine wait remains refused before writing. No overwrite, fake completion, automatic replay, atomic rollback or runtime plugin asset dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_factory_bootstrap_defaults(kind:str="\"all\"",create_default_system_nodes:str="true",add_default_modules_and_renderers:str="false",include_pins:str="false",max_function_calls:str="80")->CubelessPythonOperationResult:
        "\n        Inspect UE Niagara factory bootstrap defaults using transient objects only.\n\n        Args:\n            kind: One of all, system, emitter, empty_emitter, or basic_sprite_emitter\n            create_default_system_nodes: Whether system factory initialization creates default system graph nodes\n            add_default_modules_and_renderers: Used only with kind=\"emitter\"; creates the factory default sprite stack\n            include_pins: Include graph pin metadata in returned module/function-call summaries\n            max_function_calls: Maximum function-call nodes to include per graph summary\n         Native actual transient/persistent, compile/save/partial receipt and branch-specific wait policy remain authoritative."
        rejected,report,raw=_guard_admission("inspect_niagara_factory_bootstrap_defaults")
        if rejected is not None:return reflected_result(rejected)
        try:values={"kind":_decode_value(kind),"create_default_system_nodes":_decode_value(create_default_system_nodes),"add_default_modules_and_renderers":_decode_value(add_default_modules_and_renderers),"include_pins":_decode_value(include_pins),"max_function_calls":_decode_value(max_function_calls)}
        except Exception as error:return reflected_result(_carrier_rejected("inspect_niagara_factory_bootstrap_defaults",error,report,raw))
        return reflected_result(domain._named_mcp("inspect_niagara_factory_bootstrap_defaults",values))

    @toolset_registry.tool_call
    @staticmethod
    def create_empty_niagara_system(target_system_path:str,allow_non_temp_path:str="false",overwrite_existing:str="false",save:str="true",wait_for_completion:str="true",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Create a saved empty Niagara System using UE's Niagara System factory.\n\n        Args:\n            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated\n            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root\n            overwrite_existing: Reserved; currently refused by the C++ command\n            save: Save the created package\n            wait_for_completion: Wait for factory-requested Niagara compilation\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual transient/persistent, compile/save/partial receipt and branch-specific wait policy remain authoritative."
        rejected,report,raw=_guard_admission("create_empty_niagara_system")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"target_system_path":_decode_value(target_system_path),"allow_non_temp_path":_decode_value(allow_non_temp_path),"overwrite_existing":_decode_value(overwrite_existing),"save":_decode_value(save),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_empty_niagara_system",error,report,raw))
        return domain._named_mcp("create_empty_niagara_system",values)

    @toolset_registry.tool_call
    @staticmethod
    def create_empty_niagara_emitter(target_emitter_path:str,add_default_modules_and_renderers:str="false",allow_non_temp_path:str="false",overwrite_existing:str="false",save:str="true")->CubelessPythonOperationResult:
        "\n        Create a saved Niagara Emitter asset using UE's Niagara Emitter factory.\n\n        Args:\n            target_emitter_path: Target Niagara Emitter package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated\n            add_default_modules_and_renderers: Add the editor default sprite renderer/modules instead of a truly empty emitter\n            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root\n            overwrite_existing: Reserved; currently refused by the C++ command\n            save: Save the created package\n         Native actual transient/persistent, compile/save/partial receipt and branch-specific wait policy remain authoritative."
        rejected,report,raw=_guard_admission("create_empty_niagara_emitter")
        if rejected is not None:return reflected_result(rejected)
        try:values={"target_emitter_path":_decode_value(target_emitter_path),"add_default_modules_and_renderers":_decode_value(add_default_modules_and_renderers),"allow_non_temp_path":_decode_value(allow_non_temp_path),"overwrite_existing":_decode_value(overwrite_existing),"save":_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected("create_empty_niagara_emitter",error,report,raw))
        return reflected_result(domain._named_mcp("create_empty_niagara_emitter",values))

    @toolset_registry.tool_call
    @staticmethod
    def create_basic_sprite_niagara_system(target_system_path:str,emitter_name:str="\"Emitter\"",material_path:str="\"\"",allow_non_temp_path:str="false",overwrite_existing:str="false",save:str="true",wait_for_completion:str="true",timeout_seconds:str="10.0",poll_interval_seconds:str="0.1")->CubelessPythonAsyncOperationResult:
        "\n        Create a saved Niagara System with one default sprite emitter.\n\n        Args:\n            target_system_path: Target Niagara System package/object path, default-safe under /Game/_MCP_Temp/NiagaraGenerated\n            emitter_name: Name for the generated emitter handle/source copy\n            material_path: Optional material to apply to the generated sprite renderer\n            allow_non_temp_path: Explicitly allow creation outside the generated temp Niagara root\n            overwrite_existing: Reserved; currently refused by the C++ command\n            save: Save the created package\n            wait_for_completion: Wait for factory-requested Niagara compilation\n            timeout_seconds: Compile wait timeout\n            poll_interval_seconds: Compile wait poll interval\n         Native actual transient/persistent, compile/save/partial receipt and branch-specific wait policy remain authoritative."
        rejected,report,raw=_guard_admission("create_basic_sprite_niagara_system")
        if rejected is not None:return domain.completed_result(rejected)
        try:values={"target_system_path":_decode_value(target_system_path),"emitter_name":_decode_value(emitter_name),"material_path":_decode_value(material_path),"allow_non_temp_path":_decode_value(allow_non_temp_path),"overwrite_existing":_decode_value(overwrite_existing),"save":_decode_value(save),"wait_for_completion":_decode_value(wait_for_completion),"timeout_seconds":_decode_value(timeout_seconds),"poll_interval_seconds":_decode_value(poll_interval_seconds)}
        except Exception as error:return domain.completed_result(_carrier_rejected("create_basic_sprite_niagara_system",error,report,raw))
        return domain._named_mcp("create_basic_sprite_niagara_system",values)

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessNiagaraFactoryBootstrapAttachmentLibrary', None), 'verify_niagara_factory_bootstrap_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact NiagaraFactoryBootstrap adapter raw-input admission helper is unavailable')
        raw = helper(CubelessNiagaraFactoryBootstrapTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_niagara_factory_bootstrap_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded NiagaraFactoryBootstrap adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('niagara_factory_bootstrap_raw_guard_admission_missing', message))
    result['stage'] = 'niagara_factory_bootstrap_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('niagara_factory_bootstrap_registration_carrier_invalid',str(error)))
    result['stage']='niagara_factory_bootstrap_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
