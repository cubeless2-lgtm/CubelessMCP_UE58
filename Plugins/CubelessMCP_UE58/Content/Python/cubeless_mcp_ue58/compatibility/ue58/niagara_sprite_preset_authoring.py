"""UE5.8 dedicated NiagaraSpritePreset Native1 x1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessNiagaraSpritePresetLibrary'
BINDINGS=["create_sprite_niagara_preset_system"]
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={"create_sprite_niagara_preset_system": ["target_system_path", "allow_non_temp_path", "overwrite_existing", "emitter_name", "material_path", "use_project_weather_materials", "save", "wait_for_completion", "timeout_seconds", "poll_interval_seconds", "preset", "spawn_rate", "velocity", "alignment", "facing_mode", "sub_image_size", "sprite_size", "initialize_sprite_size", "lifetime", "particle_lifetime", "fixed_bounds_min", "bounds_min", "fixed_bounds_max", "bounds_max", "local_space", "add_spawn_box_location", "spawn_box_size", "box_location_size", "spawn_box_offset", "box_location_offset", "add_weather_detail_modules", "add_detail_modules"]}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('niagara_sprite_preset_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('niagara_sprite_preset_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('niagara_sprite_preset_authoring_native_api_unavailable','Build/load the exact NiagaraSpritePreset Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('niagara_sprite_preset_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def create_sprite_niagara_preset_system(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke("create_sprite_niagara_preset_system",text)
