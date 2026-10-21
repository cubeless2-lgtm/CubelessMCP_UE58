"""UE5.8 dedicated ChaosGeometryAuthoring Native1/1/1. No transport or fallback."""
import copy
import json
import unreal
from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY='CubelessChaosGeometryAuthoringLibrary'
BINDINGS=('create_geometry_collection_from_mesh', 'configure_building_fracture', 'configure_anchor_field')
MAX_CARRIER_UTF16_UNITS=1048576
PARAM_FIELDS={'create_geometry_collection_from_mesh': ('source_static_mesh_paths', 'output_geometry_collection_path', 'source_transforms', 'root_mesh_proxy_static_mesh_path', 'material_reindex', 'overwrite_existing'), 'configure_building_fracture': ('source_geometry_collection_path', 'output_geometry_collection_path', 'fracture_method', 'site_count', 'random_seed', 'cluster_levels', 'cluster_size', 'damage_thresholds', 'merge_tiny_geometry', 'tiny_relative_threshold', 'collision_object_reduction_percentage', 'maximum_collision_particles', 'collision_implicit_type', 'remove_on_sleep', 'remove_on_break', 'sleep_time_seconds', 'removal_duration_seconds', 'break_timer_seconds', 'root_mesh_proxy_static_mesh_path', 'overwrite_existing', 'operation', 'fractured_geometry_collection_path', 'cluster_all_roots', 'allow_non_deterministic_geometry_order'), 'configure_anchor_field': ('geometry_collection_actor_name', 'field_actor_name', 'box_center', 'box_extent', 'field_magnitude', 'save_level', 'operation')}


def binding_status():return {name:callable(getattr(getattr(unreal,LIBRARY,None),name,None)) for name in BINDINGS}


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(',',':'))
        units=len(text.encode('utf-16-le'))//2
    except (TypeError,ValueError,OverflowError,UnicodeError,RecursionError) as error:
        raise OperationError('chaos_geometry_authoring_carrier_invalid','Finite valid Unicode JSON required') from error
    if units>MAX_CARRIER_UTF16_UNITS or '\x00' in text:raise OperationError('chaos_geometry_authoring_carrier_bound','Private carrier exceeds1Mi UTF16 or contains raw NUL')
    return text


def _invoke(method,*args):
    fn=getattr(getattr(unreal,LIBRARY,None),method,None)
    if not callable(fn):
        error=OperationError('chaos_geometry_authoring_native_api_unavailable','Build/load the exact ChaosGeometryAuthoring Native API; no fallback')
        error.native_call_dispatched=False;raise error
    raw=fn(*args)
    try:
        result=_decode_result(raw)
        if result.get('operation')!=method or 'changed' not in result or result['changed'] is not None and type(result['changed']) is not bool:
            raise OperationError('chaos_geometry_authoring_native_result_invalid','Exact operation and changed receipt required')
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw;raise
    result['_raw_native_json']=raw;return result




def create_geometry_collection_from_mesh(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('create_geometry_collection_from_mesh',text)


def configure_building_fracture(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_building_fracture',text)


def configure_anchor_field(params):
    try:text=_carrier(params)
    except Exception as error:error.native_call_dispatched=False;raise
    return _invoke('configure_anchor_field',text)


