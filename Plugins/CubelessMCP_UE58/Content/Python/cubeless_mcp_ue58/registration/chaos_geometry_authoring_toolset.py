"""Original logical geometry authoring15/9 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import chaos_geometry_authoring as domain

@unreal.uclass()
class CubelessChaosGeometryAuthoringTools(unreal.ToolsetDefinition):
    'Dedicated native Geometry Collection conversion, deterministic fracture and serialized anchor authoring. Public6/20/6 required2/2/1, original nested Any/nullable arrays and raw hidden0/4/1 remain distinct. Preserve source bytes/dirty, strict and spatial hashes, reload, actual partial save/cleanup, Named anchor default1 versus raw omitted Kinematic2, original post-mutation rejection prefix. Original legacy vacuous or assertion-gated saved leaves never replace actual SDK save. Hidden inspect_saved remains a raw observer. No automatic retry, atomic rollback, Engine global purity, cold reload, physics, packaging or pipeline completion claim.'
    @toolset_registry.tool_call
    @staticmethod
    def create_geometry_collection_from_mesh(source_static_mesh_paths:str,output_geometry_collection_path:str,source_transforms:str='null',root_mesh_proxy_static_mesh_path:str='""',material_reindex:str='true',overwrite_existing:str='false')->CubelessPythonOperationResult:
        'Convert source StaticMeshes into a distinct owned temporary GeometryCollection; preserve original source/transform/guard contracts and actual append/save/partial prefix.'
        rejected,report,raw=_guard_admission('create_geometry_collection_from_mesh')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_static_mesh_paths':_decode_value(source_static_mesh_paths),'output_geometry_collection_path':_decode_value(output_geometry_collection_path),'source_transforms':_decode_value(source_transforms),'root_mesh_proxy_static_mesh_path':_decode_value(root_mesh_proxy_static_mesh_path),'material_reindex':_decode_value(material_reindex),'overwrite_existing':_decode_value(overwrite_existing)}
        except Exception as error:return reflected_result(_carrier_rejected('create_geometry_collection_from_mesh',error,report,raw))
        return reflected_result(domain._named_mcp('create_geometry_collection_from_mesh',values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_building_fracture(source_geometry_collection_path:str,output_geometry_collection_path:str,fracture_method:str='"uniform_voronoi"',site_count:str='64',random_seed:str='1337',cluster_levels:str='2',cluster_size:str='8',damage_thresholds:str='null',merge_tiny_geometry:str='true',tiny_relative_threshold:str='0.01',collision_object_reduction_percentage:str='20',maximum_collision_particles:str='60',collision_implicit_type:str='"convex"',remove_on_sleep:str='true',remove_on_break:str='true',sleep_time_seconds:str='5',removal_duration_seconds:str='1',break_timer_seconds:str='1',root_mesh_proxy_static_mesh_path:str='""',overwrite_existing:str='false')->CubelessPythonOperationResult:
        'Create distinct deterministic fractured GeometryCollection, preserving hidden inspect_saved/proxy configuration, same-seed/managed/spatial/readback/tiny/reload/cleanup criteria and actual saved history.'
        rejected,report,raw=_guard_admission('configure_building_fracture')
        if rejected is not None:return reflected_result(rejected)
        try:values={'source_geometry_collection_path':_decode_value(source_geometry_collection_path),'output_geometry_collection_path':_decode_value(output_geometry_collection_path),'fracture_method':_decode_value(fracture_method),'site_count':_decode_value(site_count),'random_seed':_decode_value(random_seed),'cluster_levels':_decode_value(cluster_levels),'cluster_size':_decode_value(cluster_size),'damage_thresholds':_decode_value(damage_thresholds),'merge_tiny_geometry':_decode_value(merge_tiny_geometry),'tiny_relative_threshold':_decode_value(tiny_relative_threshold),'collision_object_reduction_percentage':_decode_value(collision_object_reduction_percentage),'maximum_collision_particles':_decode_value(maximum_collision_particles),'collision_implicit_type':_decode_value(collision_implicit_type),'remove_on_sleep':_decode_value(remove_on_sleep),'remove_on_break':_decode_value(remove_on_break),'sleep_time_seconds':_decode_value(sleep_time_seconds),'removal_duration_seconds':_decode_value(removal_duration_seconds),'break_timer_seconds':_decode_value(break_timer_seconds),'root_mesh_proxy_static_mesh_path':_decode_value(root_mesh_proxy_static_mesh_path),'overwrite_existing':_decode_value(overwrite_existing)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_building_fracture',error,report,raw))
        return reflected_result(domain._named_mcp('configure_building_fracture',values))

    @toolset_registry.tool_call
    @staticmethod
    def configure_anchor_field(geometry_collection_actor_name:str,field_actor_name:str='"Chaos_AnchorField"',box_center:str='null',box_extent:str='null',field_magnitude:str='1',save_level:str='true')->CubelessPythonOperationResult:
        'Configure serialized Engine anchor field or raw inspect_saved. Preserve Named defaults and raw omission, explicit actual save versus vacuous level_saved and post-spawn invalid-magnitude prefix.'
        rejected,report,raw=_guard_admission('configure_anchor_field')
        if rejected is not None:return reflected_result(rejected)
        try:values={'geometry_collection_actor_name':_decode_value(geometry_collection_actor_name),'field_actor_name':_decode_value(field_actor_name),'box_center':_decode_value(box_center),'box_extent':_decode_value(box_extent),'field_magnitude':_decode_value(field_magnitude),'save_level':_decode_value(save_level)}
        except Exception as error:return reflected_result(_carrier_rejected('configure_anchor_field',error,report,raw))
        return reflected_result(domain._named_mcp('configure_anchor_field',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessChaosGeometryAuthoringAttachmentLibrary', None), 'verify_chaos_geometry_authoring_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact ChaosGeometryAuthoring adapter raw-input admission helper is unavailable')
        raw = helper(CubelessChaosGeometryAuthoringTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_chaos_geometry_authoring_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded ChaosGeometryAuthoring adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('chaos_geometry_authoring_raw_guard_admission_missing', message))
    result['stage'] = 'chaos_geometry_authoring_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('chaos_geometry_authoring_registration_carrier_invalid',str(error)))
    result['stage']='chaos_geometry_authoring_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
