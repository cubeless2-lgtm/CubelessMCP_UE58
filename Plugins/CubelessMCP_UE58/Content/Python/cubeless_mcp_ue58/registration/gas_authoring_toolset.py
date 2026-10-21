"""Original logical GAS6 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import gas_authoring as domain

@unreal.uclass()
class CubelessGASAuthoringTools(unreal.ToolsetDefinition):
    'Six dedicated GAS Editor APIs preserve specialized Ability factory, Engine GameplayAbilities-only base/struct/component references, inherited ASC and local attribute no-op rules, compile/type readback, dependency audit, actual saved history and exact owned-new-output cleanup. Optional GameplayAbilities disabled is explicit unavailable without auto-enable/restart. Public MCP coercion precedes strict Python body; raw gas_* retains exact JSON types/presence. SDK global effects may remain unknown; no automatic retry or atomic rollback. No Cubeless runtime class dependency.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_gas_blueprint(blueprint_path:str)->CubelessPythonOperationResult:
        'Dedicated inspect_gas_blueprint through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('inspect_gas_blueprint')
        if rejected is not None:return reflected_result(rejected)
        try:values={'blueprint_path':_decode_value(blueprint_path)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_gas_blueprint',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_gas_blueprint',values))

    @toolset_registry.tool_call
    @staticmethod
    def create_gameplay_ability_blueprint(asset_name:str,package_path:str='"/Game/GAS/Abilities"',save:str='true')->CubelessPythonOperationResult:
        'Dedicated create_gameplay_ability_blueprint through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('create_gameplay_ability_blueprint')
        if rejected is not None:return reflected_result(rejected)
        try:values={'asset_name':_decode_value(asset_name),'package_path':_decode_value(package_path),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('create_gameplay_ability_blueprint',error,report,raw))
        return reflected_result(domain._named_mcp('create_gameplay_ability_blueprint',values))

    @toolset_registry.tool_call
    @staticmethod
    def create_gameplay_effect_blueprint(asset_name:str,package_path:str='"/Game/GAS/Effects"',save:str='true')->CubelessPythonOperationResult:
        'Dedicated create_gameplay_effect_blueprint through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('create_gameplay_effect_blueprint')
        if rejected is not None:return reflected_result(rejected)
        try:values={'asset_name':_decode_value(asset_name),'package_path':_decode_value(package_path),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('create_gameplay_effect_blueprint',error,report,raw))
        return reflected_result(domain._named_mcp('create_gameplay_effect_blueprint',values))

    @toolset_registry.tool_call
    @staticmethod
    def create_attribute_set_blueprint(asset_name:str,package_path:str='"/Game/GAS/Attributes"',save:str='true')->CubelessPythonOperationResult:
        'Dedicated create_attribute_set_blueprint through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('create_attribute_set_blueprint')
        if rejected is not None:return reflected_result(rejected)
        try:values={'asset_name':_decode_value(asset_name),'package_path':_decode_value(package_path),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('create_attribute_set_blueprint',error,report,raw))
        return reflected_result(domain._named_mcp('create_attribute_set_blueprint',values))

    @toolset_registry.tool_call
    @staticmethod
    def add_ability_system_component_to_blueprint(blueprint_path:str,component_name:str='"AbilitySystem"',save:str='true')->CubelessPythonOperationResult:
        'Dedicated add_ability_system_component_to_blueprint through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('add_ability_system_component_to_blueprint')
        if rejected is not None:return reflected_result(rejected)
        try:values={'blueprint_path':_decode_value(blueprint_path),'component_name':_decode_value(component_name),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('add_ability_system_component_to_blueprint',error,report,raw))
        return reflected_result(domain._named_mcp('add_ability_system_component_to_blueprint',values))

    @toolset_registry.tool_call
    @staticmethod
    def add_gameplay_attribute_to_attribute_set(attribute_set_path:str,attribute_name:str,category:str='"GAS|Attributes"',save:str='true')->CubelessPythonOperationResult:
        'Dedicated add_gameplay_attribute_to_attribute_set through public UE5.8 Editor GAS APIs. Preserve validation, no-op/compile/save/dependency/partial-state contracts. Actual binding and Engine behavior require separate verification.'
        rejected,report,raw=_guard_admission('add_gameplay_attribute_to_attribute_set')
        if rejected is not None:return reflected_result(rejected)
        try:values={'attribute_set_path':_decode_value(attribute_set_path),'attribute_name':_decode_value(attribute_name),'category':_decode_value(category),'save':_decode_value(save)}
        except Exception as error:return reflected_result(_carrier_rejected('add_gameplay_attribute_to_attribute_set',error,report,raw))
        return reflected_result(domain._named_mcp('add_gameplay_attribute_to_attribute_set',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessGASAuthoringAttachmentLibrary', None), 'verify_gas_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact GASAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessGASAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_gas_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded GASAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('gas_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'gas_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('gas_authoring_registration_carrier_invalid',str(error)))
    result['stage']='gas_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
