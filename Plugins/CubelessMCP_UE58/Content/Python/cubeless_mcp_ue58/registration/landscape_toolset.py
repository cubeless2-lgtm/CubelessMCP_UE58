"""Landscape public0/1/11/2; internal vectorJSON2 and decimal int645 carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import landscape as operation

@unreal.uclass()
class CubelessLandscapeTools(unreal.ToolsetDefinition):
    'Landscape original public0/1/11/2: loaded Editor-world proxies and first IgnoreCase label/name match. Preserve duplicate/iterator/layer order. Create flat Landscape with original root guard and synchronous public Import, dimensions/height/material/transform/component/dirty postconditions and actual partial prefix. Set material preserves load-before-lookup and PostEditChangeProperty effects. Explicit compile/save0; Import may run internal dialogs/cache work. No full async/no-wait guarantee, global WP cell traversal, Foliage LevelPartition guard, uncertain replay or atomic rollback.'
    @toolset_registry.tool_call
    @staticmethod
    def list_landscapes()->CubelessPythonOperationResult:
        "List loaded Editor-world LandscapeProxy actors with original labels/names/classes, component counts, materials, location and GUID. Preserve iterator order. Direct read only; no mutation journal, transaction or world actor creation."
        rejected,report,raw=_guard_admission('list_landscapes')
        if rejected is not None:return reflected_result(rejected)
        return reflected_result(operation.list_landscapes())

    @toolset_registry.tool_call
    @staticmethod
    def get_landscape_info(actor_label:str='')->CubelessPythonOperationResult:
        "Inspect first loaded LandscapeProxy when actor_label is empty, otherwise first IgnoreCase label/name match. Preserve duplicate-match/iterator and target-layer order. Return original scale string and layer_info_objects; read only, no journal/save."
        rejected,report,raw=_guard_admission('get_landscape_info')
        if rejected is not None:return reflected_result(rejected)
        return reflected_result(operation.get_landscape_info(actor_label))

    @toolset_registry.tool_call
    @staticmethod
    def create_landscape(actor_label:str='MCP_Landscape',material_path:str='',location:str='null',scale:str='null',component_count_x:str='1',component_count_y:str='1',sections_per_component:str='1',quads_per_section:str='7',height_value:str='32768',allow_non_temp_level:bool=False,required_root:str='/Game/_MCP_Temp')->CubelessPythonOperationResult:
        "Create a flat Landscape using original required_root/override guard, nullable vector truthiness, clamped dimensions and uint16 height. Verify actual actor/GUID/components/transform/material after synchronous public Import. Import can run internal slow-task/cache work; failures retain actual spawn/texture/component prefix and unknown whole SDK effects. Explicit compile/save0; never replay uncertain creation."
        rejected,report,raw=_guard_admission('create_landscape')
        if rejected is not None:return reflected_result(rejected)
        try:
            location=_decode_json(location);scale=_decode_json(scale)
            component_count_x=_decode_integer(component_count_x)
            component_count_y=_decode_integer(component_count_y)
            sections_per_component=_decode_integer(sections_per_component)
            quads_per_section=_decode_integer(quads_per_section)
            height_value=_decode_integer(height_value)
        except Exception as error:return reflected_result(_carrier_rejected('create_landscape',error,report,raw))
        return reflected_result(operation.create_landscape(actor_label,material_path,location,scale,component_count_x,component_count_y,sections_per_component,quads_per_section,height_value,allow_non_temp_level,required_root))

    @toolset_registry.tool_call
    @staticmethod
    def set_landscape_material(material_path:str,actor_label:str='')->CubelessPythonOperationResult:
        "Load the requested material before first matching loaded proxy lookup, assign LandscapeMaterial and call public PostEditChangeProperty. Return actual material/update/dirty prefix and postconditions, including unknown indirect LoadAsset/callback effects on failure. Empty label selects first loaded proxy. Explicit compile/save0; no automatic retry/rollback."
        rejected,report,raw=_guard_admission('set_landscape_material')
        if rejected is not None:return reflected_result(rejected)
        return reflected_result(operation.set_landscape_material(material_path,actor_label))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessLandscapeAttachmentLibrary', None), 'verify_landscape_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Landscape adapter raw-input admission helper is unavailable')
        raw = helper(CubelessLandscapeTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_landscape_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Landscape adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('landscape_raw_guard_admission_missing', message))
    result['stage'] = 'landscape_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_json(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON carrier required')
    value=json.loads(text,parse_constant=_invalid_constant)
    if value is None:return None
    if type(value) is not list:raise ValueError('Original array/nullable-array carrier required')
    return value

def _decode_integer(text):
    if type(text) is not str or not text or len(text)>20:raise ValueError('Bounded canonical int64 decimal required')
    value=int(text)
    if str(value)!=text or not -(1<<63)<=value<(1<<63):raise ValueError('Exact int64 decimal required')
    return value

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('landscape_registration_carrier_invalid',str(error)))
    result['stage']='landscape_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
