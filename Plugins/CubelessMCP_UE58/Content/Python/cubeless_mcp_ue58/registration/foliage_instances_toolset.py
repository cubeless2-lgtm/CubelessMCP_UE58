"""Foliage public0/2/4; private JSON2 and exact int64 decimal1 carriers."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from cubeless_mcp_ue58.operations import foliage_instances as operation

@unreal.uclass()
class CubelessFoliageInstanceTools(unreal.ToolsetDefinition):
    'Current Editor world/current-level IFA foliage list/add/remove original public0/2/4. List creates no actor. Add retains per-item parse/write order and actual partial registration/instance prefix. Remove preserves no-op/dedup/range/cap behavior and swapped-index risk. Canonical Named normalization is distinct from raw legacy remove; actual Native Json conversion preserves locale and raw presence through an internal compatibility context. Exact finite coordinate guards, LevelPartition-only scope and safe int32 conversion limits are explicit. Native refresh/remove may start async HISM work; no game-thread render wait, explicit compile/save, uncertain retry or atomic rollback.'
    @toolset_registry.tool_call
    @staticmethod
    def list_foliage_types()->CubelessPythonOperationResult:
        'List current Editor level IFA types/counts without creating an actor; no writer transaction/save.'
        rejected,report,raw=_guard_admission('list_foliage_types')
        return reflected_result(rejected if rejected is not None else operation.list_foliage_types())

    @toolset_registry.tool_call
    @staticmethod
    def add_foliage_instances(foliage_type_path:str,transforms:str)->CubelessPythonOperationResult:
        'Add up to5000 current-level foliage placements. Per-item late parse/write preserves partial first additions; rotation[roll,pitch,yaw], scale default1. Full count/transform/dirty/async receipts; no save or retry.'
        rejected,report,raw=_guard_admission('add_foliage_instances')
        if rejected is not None:return reflected_result(rejected)
        try:transforms=_decode_json(transforms,False)
        except Exception as error:return reflected_result(_carrier_rejected('add_foliage_instances',error,report,raw))
        return reflected_result(operation.add_foliage_instances(foliage_type_path,transforms))

    @toolset_registry.tool_call
    @staticmethod
    def remove_foliage_instances(foliage_type_path:str,instance_indices:str='null',remove_all:bool=False,max_remove_count:str='5000')->CubelessPythonOperationResult:
        'Remove current-level placements with original nullable indices/remove_all/cap5000. Native validates all indices/ranges/dedup/cap before writes. Removal swaps indices; never replay stale indices. No compile/save/render wait.'
        rejected,report,raw=_guard_admission('remove_foliage_instances')
        if rejected is not None:return reflected_result(rejected)
        try:instance_indices=_decode_json(instance_indices,True);max_remove_count=_decode_integer(max_remove_count)
        except Exception as error:return reflected_result(_carrier_rejected('remove_foliage_instances',error,report,raw))
        return reflected_result(operation.remove_foliage_instances(foliage_type_path,instance_indices,remove_all,max_remove_count))

def _guard_admission(method):
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessFoliageInstanceAttachmentLibrary', None), 'verify_foliage_instances_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Foliage instances adapter raw-input admission helper is unavailable')
        raw = helper(CubelessFoliageInstanceTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_foliage_instances_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == method
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Foliage instances adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(method,operation.OperationError('foliage_instances_raw_guard_admission_missing', message))
    result['stage'] = 'foliage_instances_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):raise ValueError('Non-finite JSON not supported: '+value)

def _decode_json(text,nullable):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:raise ValueError('Bounded private JSON carrier required')
    value=json.loads(text,parse_constant=_invalid_constant)
    if value is None and nullable:return None
    if type(value) is not list:raise ValueError('Original array/nullable-array carrier required')
    return value

def _decode_integer(text):
    if type(text) is not str or not text or len(text)>20:raise ValueError('Bounded canonical int64 decimal required')
    value=int(text)
    if str(value)!=text or not -(1<<63)<=value<(1<<63):raise ValueError('Exact int64 decimal required')
    return value

def _carrier_rejected(method,error,report,raw):
    result=operation._rejected(method,operation.OperationError('foliage_instances_registration_carrier_invalid',str(error)))
    result['stage']='foliage_instances_registration_carrier_admission';result['data'].update(registration_guard=report,registration_guard_raw=raw,native_dispatched=False,python_dispatched=True,journal_created=False)
    return result
