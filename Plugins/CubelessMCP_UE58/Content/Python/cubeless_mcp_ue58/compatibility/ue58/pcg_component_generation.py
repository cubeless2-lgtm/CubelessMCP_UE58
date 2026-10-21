"""HD dedicated UE5.8 EngineAPI x2. No RPC, fallback, sleep or job here."""
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessPCGComponentGenerationLibrary'
BINDINGS=["refresh_pcg_components","bind_pcg_generation_settings_to_blueprint","configure_blueprint_pcg_template"]
PARAM_FIELDS={"refresh_pcg_components":["actor_label","actor_name","actor_path","actor_tag","actor_label_prefix","component_name","component_tag","selected_only","cleanup","immediate_cleanup","remove_components","notify_properties_changed","refresh","cancel_existing_refresh","generate","force","wait_until_complete","timeout_seconds","poll_interval_seconds","min_generated_output_data_count","min_managed_resource_count","max_components"],"bind_pcg_generation_settings_to_blueprint":["blueprint_name","component_variable","generation_trigger_variable","regenerate_in_editor_variable","generate_on_drop_variable","generate_on_drop_default","graph_name","graph_id","graph_type","replace_existing","sync_component_template_defaults","function_class","function_class_path","function_name","compile","save"]}
MAX_CARRIER_UTF16_UNITS=1048576


def binding_status():
    library=getattr(unreal,LIBRARY,None)
    return {name:callable(getattr(library,name,None)) for name in BINDINGS if name != 'configure_blueprint_pcg_template'} | {'configure_blueprint_pcg_template': callable(getattr(getattr(unreal,'CubelessComponentLibrary',None),'configure_blueprint_pcg_template',None))}


def _carrier(params):
    if type(params) is not dict:raise OperationError('pcg_component_generation_carrier_invalid','Object parameters required')
    try:
        text=json.dumps(params,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,UnicodeError,OverflowError,RecursionError) as error:
        raise OperationError('pcg_component_generation_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:
        raise OperationError('pcg_component_generation_carrier_bound','Carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    function=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(function):
        error=OperationError('pcg_component_generation_native_api_unavailable','Exact HD Native binding is unavailable; no call dispatched')
        error.native_call_dispatched=False;raise error
    try:raw=function(text)
    finally:function=None
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('pcg_component_generation_native_result_invalid','Exact operation and typed changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw
    return result


def refresh_pcg_components(params):return _invoke('refresh_pcg_components',params)

def bind_pcg_generation_settings_to_blueprint(params):return _invoke('bind_pcg_generation_settings_to_blueprint',params)


def _template_call(method, *arguments):
    """Fixed ComponentLibrary calls; release bound Unreal callables before decoding/errors."""
    function = library = None
    dispatched = False
    raw = None
    try:
        library = getattr(unreal, 'CubelessComponentLibrary', None)
        function = getattr(library, method, None) if library is not None else None
        if not callable(function):
            error = OperationError('pcg_template_native_api_unavailable', 'Exact ComponentLibrary template helper is unavailable; no fallback dispatched')
            error.native_call_dispatched = False
            raise error
        dispatched = True
        raw = function(*arguments)
    except Exception as error:
        error.native_call_dispatched = dispatched
        raise
    finally:
        function = None
        library = None
    try:
        value = json.loads(raw)
        if type(value) is not dict or type(value.get('data')) is not dict:
            raise ValueError('Exact component result object required')
        for field in ('success','mutation_attempted','compile_requested','change_state_known','postcondition_passed','save_requested','saved'):
            if type(value.get(field)) is not bool: raise ValueError('Typed component result field required: '+field)
        if value.get('change_state_known') is True and type(value.get('changed')) is not bool:
            raise ValueError('Known component change must have an actual boolean value')
        if method == 'configure_blueprint_pcg_template':
            if value.get('operation') != method or type(value.get('compile_attempted')) is not bool:
                raise ValueError('Exact configuration operation/compile result required')
            if value.get('compiled') is not None and type(value.get('compiled')) is not bool:
                raise ValueError('Typed actual compile result required')
        elif any(value.get(field) is not False for field in ('mutation_attempted','compile_requested','save_requested','saved')) or value.get('loaded_during_read') is not False:
            raise ValueError('Post-configuration read must not load or report explicit mutation')
    except Exception as error:
        error.native_call_dispatched = True
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    value['_raw_native_json'] = raw
    return value


def configure_blueprint_pcg_template(params):
    try: carrier = _carrier(params)
    except Exception as error:
        error.native_call_dispatched = False
        raise
    return _template_call('configure_blueprint_pcg_template', carrier)


def observe_blueprint_pcg_template(blueprint_path, component_name):
    """Existing fixed native read APIs; this does not reapply the configuration."""
    values = {}
    calls = [('components', 'list_blueprint_components', (blueprint_path, '', 10000))]
    calls += [(name, 'get_component_property', (blueprint_path, component_name, name, 10000, 65536)) for name in ('GraphInstance','GenerationTrigger','bRegenerateInEditor','bGenerateOnDropWhenTriggerOnDemand')]
    for key, method, arguments in calls:
        receipt = _template_call(method, *arguments)
        values[key] = receipt
        if receipt.get('success') is not True or receipt.get('change_state_known') is not True or receipt.get('postcondition_passed') is not True or receipt.get('changed') is not False:
            error = OperationError('pcg_template_readback_unverified', 'Existing exact local component read did not establish unchanged current state')
            error.native_decoded_result = receipt
            error.partial_observation = values
            raise error
    return values


def save_blueprint_template(blueprint_path):
    """Reuse the established Blueprint loaded-asset save, with no compile or new journal."""
    from . import blueprint, blueprint_duplicates
    asset = None
    try:
        blueprint.ensure_editor_idle()
        asset = blueprint.resolve_blueprint(blueprint_path)
        return blueprint_duplicates.save(asset)
    finally:
        asset = None


def template_file_state(package):
    from . import blueprint_duplicates
    return blueprint_duplicates.file_state(package)


def template_saved_dependencies(package, expected_file):
    from . import pcg, blueprint_duplicates
    refreshed = pcg.refresh_saved_file_registry(package, expected_file)
    audit = blueprint_duplicates.dependency_audit(package)
    return {'registry_refresh':refreshed, 'audit':audit}
