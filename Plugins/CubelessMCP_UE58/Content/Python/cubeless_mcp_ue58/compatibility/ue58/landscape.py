"""UE5.8 Landscape Native0/2/12/3. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessLandscapeLibrary'
BINDINGS=('list_landscapes','get_landscape_info','create_landscape','set_landscape_material')
MAX_CARRIER_UTF16_UNITS=1048576
PUBLIC_FIELDS={'list_landscapes':(),'get_landscape_info':('actor_label',),'create_landscape':('actor_label','material_path','location','scale','component_count_x','component_count_y','sections_per_component','quads_per_section','height_value','allow_non_temp_level','required_root'),'set_landscape_material':('material_path','actor_label')}
INTEGER_FIELDS=PUBLIC_FIELDS['create_landscape'][4:9]


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('landscape_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('landscape_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _private_context(method,legacy):
    if legacy is None:return None
    if type(legacy) is not dict or any(type(k) is not str or k not in PUBLIC_FIELDS[method] for k in legacy):raise OperationError('landscape_raw_context_invalid','Only original public fields with exact case and original presence are allowed')
    return copy.deepcopy(legacy)


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('landscape_native_api_unavailable','Build/load the exact Landscape Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('landscape_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result


def list_landscapes():return _invoke('list_landscapes')


def get_landscape_info(actor_label='',*,_legacy_command_params=None):
    try:
        context=_private_context('get_landscape_info',_legacy_command_params)
        args=(actor_label,'') if context is None else ('',_carrier(context))
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('get_landscape_info',*args)


def create_landscape(actor_label='MCP_Landscape',material_path='',location=None,scale=None,component_count_x=1,component_count_y=1,sections_per_component=1,quads_per_section=7,height_value=32768,allow_non_temp_level=False,required_root='/Game/_MCP_Temp',*,_legacy_command_params=None):
    try:
        context=_private_context('create_landscape',_legacy_command_params)
        if context is None:args=(actor_label,material_path,_carrier(location),_carrier(scale),component_count_x,component_count_y,sections_per_component,quads_per_section,height_value,allow_non_temp_level,required_root,'')
        else:args=('MCP_Landscape','','null','null',1,1,1,7,32768,False,'/Game/_MCP_Temp',_carrier(context))
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('create_landscape',*args)


def set_landscape_material(material_path,actor_label='',*,_legacy_command_params=None):
    try:
        context=_private_context('set_landscape_material',_legacy_command_params)
        args=(material_path,actor_label,'') if context is None else ('','',_carrier(context))
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('set_landscape_material',*args)
