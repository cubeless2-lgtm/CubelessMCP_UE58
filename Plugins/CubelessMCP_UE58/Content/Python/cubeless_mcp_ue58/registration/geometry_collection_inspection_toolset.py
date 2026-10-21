"""Original logical geometry authoring15/9 through private per-field JSON FString carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import geometry_collection_inspection as domain

@unreal.uclass()
class CubelessGeometryCollectionInspectionTools(unreal.ToolsetDefinition):
    'Dedicated read-only GeometryCollection structural and PIE runtime inspection, exact public3/6 required1/1 default2/5. Native owns collection/hierarchy/TinyGeo/removal/proxy/physics locks. Preserve Named strip/selector/bool/int clamp and negative PIE omission versus raw presence/RoundToInt/BLOCKED; operationPASS/assertionFAIL/learningBLOCKED are distinct. No explicit writer/compile/save or Named mutation journal; Engine getter/cache/readlock global effects may remain unknown. No five-run performance/physics/pipeline completion, automatic retry or atomic rollback claim.'
    @toolset_registry.tool_call
    @staticmethod
    def inspect_geometry_collection_performance_guarded(geometry_collection_path:str,actor_name:str='""',tiny_relative_threshold:str='0.04')->CubelessPythonOperationResult:
        'Inspect bounded hierarchy, closed mesh/TinyGeo/contact, removal and RootMeshProxy metadata with optional component state. Restricted learning root and original assertion/learning verdict remain Native-owned.'
        rejected,report,raw=_guard_admission('inspect_geometry_collection_performance_guarded')
        if rejected is not None:return reflected_result(rejected)
        try:values={'geometry_collection_path':_decode_value(geometry_collection_path),'actor_name':_decode_value(actor_name),'tiny_relative_threshold':_decode_value(tiny_relative_threshold)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_geometry_collection_performance_guarded',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_geometry_collection_performance_guarded',values))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_geometry_collection_runtime_state(component_name:str,actor_name:str='""',actor_label:str='""',include_pieces:str='true',max_pieces:str='512',pie_instance:str='-1')->CubelessPythonOperationResult:
        'Observe one exact component/actor in one begun-play PIE instance. Preserve all-piece aggregate and bounded rigid output, indexed physics handles, render/physics transforms, removal timers and actual BLOCKED reasons.'
        rejected,report,raw=_guard_admission('inspect_geometry_collection_runtime_state')
        if rejected is not None:return reflected_result(rejected)
        try:values={'component_name':_decode_value(component_name),'actor_name':_decode_value(actor_name),'actor_label':_decode_value(actor_label),'include_pieces':_decode_value(include_pieces),'max_pieces':_decode_value(max_pieces),'pie_instance':_decode_value(pie_instance)}
        except Exception as error:return reflected_result(_carrier_rejected('inspect_geometry_collection_runtime_state',error,report,raw))
        return reflected_result(domain._named_mcp('inspect_geometry_collection_runtime_state',values))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessGeometryCollectionInspectionAttachmentLibrary', None), 'verify_geometry_collection_inspection_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact GeometryCollectionInspection adapter raw-input admission helper is unavailable')
        raw = helper(CubelessGeometryCollectionInspectionTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_geometry_collection_inspection_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded GeometryCollectionInspection adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = domain._rejected(method,domain.OperationError('geometry_collection_inspection_raw_guard_admission_missing', message))
    result['stage'] = 'geometry_collection_inspection_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_value(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON value carrier required')
    return json.loads(text,parse_constant=_invalid_constant)

def _carrier_rejected(method,error,report,raw):
    result=domain._rejected(method,domain.OperationError('geometry_collection_inspection_registration_carrier_invalid',str(error)))
    result['stage']='geometry_collection_inspection_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
