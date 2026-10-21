"""UE58 dedicated Blueprint Niagara template inspect/dryrun/canary/durable Native1x4. No fallback or synthetic verification."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessBlueprintNiagaraTemplateSwapLibrary'
BINDINGS=["inspect_blueprint_niagara_component_template", "dry_run_blueprint_niagara_component_template_asset_swap", "execute_blueprint_niagara_component_template_asset_swap_canary", "execute_blueprint_niagara_component_template_asset_swap_durable"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"inspect_blueprint_niagara_component_template": ["blueprint_path", "component_name", "expected_system_path", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_expected_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "dry_run_blueprint_niagara_component_template_asset_swap": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "execute_blueprint_niagara_component_template_asset_swap_canary": ["source_blueprint_path", "target_blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "replace_existing_target", "allow_dirty_source_blueprint", "compile_blueprint", "save_blueprint", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"], "execute_blueprint_niagara_component_template_asset_swap_durable": ["blueprint_path", "component_name", "source_system_path", "replacement_system_path", "confirmation_token", "operation", "dry_run", "preserve_override_parameters", "allow_unrealmcp_dependency", "allow_dirty_blueprint", "compile_blueprint", "save_blueprint", "parameter_name_contains", "include_inherited_scs_templates", "require_exactly_one", "require_source_system_match", "max_components", "max_parameters", "include_parameter_values", "include_raw_bytes", "max_raw_bytes", "include_parameter_object_properties", "include_component_object_properties", "max_object_properties", "max_array_elements", "max_property_value_chars"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('blueprint_niagara_template_swap_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('blueprint_niagara_template_swap_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('blueprint_niagara_template_swap_native_api_unavailable','Build/load the exact BlueprintNiagaraTemplateSwap Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('blueprint_niagara_template_swap_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def inspect_blueprint_niagara_component_template(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("inspect_blueprint_niagara_component_template",text)

def dry_run_blueprint_niagara_component_template_asset_swap(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("dry_run_blueprint_niagara_component_template_asset_swap",text)

def execute_blueprint_niagara_component_template_asset_swap_canary(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("execute_blueprint_niagara_component_template_asset_swap_canary",text)

def execute_blueprint_niagara_component_template_asset_swap_durable(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("execute_blueprint_niagara_component_template_asset_swap_durable",text)

