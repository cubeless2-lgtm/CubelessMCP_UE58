"""One fixed raw Registry PhysicsAsset15 helper; no arbitrary dispatch."""
import base64
import binascii
import json
import re
from cubeless_mcp_ue58.operations import skeletal_physics,state
from cubeless_mcp_ue58.operations.state import OperationError
from .results import reflected_result
MARKER='__CUBELESS_SKELETAL_PHYSICS_RESULT__'
MAX_ARGUMENT_BYTES=131072
METHOD='create_skeletal_physics_asset'
PUBLIC_FIELDS=('skeletal_mesh_path', 'target_physics_asset_path', 'expected_bones', 'kinematic_root_bone', 'root_kinematic', 'disable_collision', 'swing1_limit_degrees', 'swing2_limit_degrees', 'twist_limit_degrees', 'linear_damping', 'angular_damping', 'tip_mass_scale', 'overwrite_existing', 'save', 'dry_run')
DEFAULTS={'expected_bones': None, 'kinematic_root_bone': '', 'root_kinematic': True, 'disable_collision': True, 'swing1_limit_degrees': 45.0, 'swing2_limit_degrees': 45.0, 'twist_limit_degrees': 25.0, 'linear_damping': 0.15, 'angular_damping': 0.5, 'tip_mass_scale': 3.0, 'overwrite_existing': False, 'save': True, 'dry_run': True}
RESULT_FIELDS=('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')

def _unique(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise OperationError('raw_skeletal_physics_arguments_invalid','Duplicate JSON keys are not admitted')
        result[key]=value
    return result

def _nonfinite(value):
    raise OperationError('raw_skeletal_physics_arguments_invalid','Nonfinite JSON literals are not admitted')

def _arguments(encoded):
    if type(encoded) is not str or len(encoded)>4*((MAX_ARGUMENT_BYTES+2)//3):raise OperationError('raw_skeletal_physics_arguments_invalid','Bounded base64 JSON required')
    try:
        raw=base64.b64decode(encoded,validate=True)
        if len(raw)>MAX_ARGUMENT_BYTES:raise OperationError('raw_skeletal_physics_arguments_invalid','Raw arguments exceed128KiB')
        supplied=json.loads(raw.decode('utf-8'),object_pairs_hook=_unique,parse_constant=_nonfinite)
    except (binascii.Error,UnicodeError,ValueError,RecursionError) as error:raise OperationError('raw_skeletal_physics_arguments_invalid','Strict base64/UTF8/JSON required') from error
    if type(supplied) is not dict or set(supplied)-set(PUBLIC_FIELDS) or any(k not in supplied for k in PUBLIC_FIELDS[:2]):
        raise OperationError('raw_skeletal_physics_arguments_invalid','Exact original fifteen fields and required paths expected')
    arguments={**DEFAULTS,**supplied}
    # Complete original type/target/value admission precedes the named journal.
    skeletal_physics._args(**arguments)
    return arguments,supplied

def _envelope(record):
    reflected=reflected_result(record);fields={key:getattr(reflected,key) for key in RESULT_FIELDS}
    if type(fields['success']) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):raise OperationError('raw_skeletal_physics_result_invalid','Exact operation result types required')
    return {'returnValue':fields}

def emit_result(encoded_args_b64,nonce):
    if type(nonce) is not str or re.fullmatch('[0-9a-f]{32}',nonce) is None:raise OperationError('raw_skeletal_physics_nonce_invalid','Native owner nonce must be lowercase UUID hex')
    try:arguments,supplied=_arguments(encoded_args_b64)
    except Exception as error:
        record=state.fail(state.new_result(),error)
        record['data'].update(operation=METHOD,admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
            mutation_state_unknown=False,mutation_performed=False,raw_named_toolset_argument_validation_failed=True)
    else:
        record=skeletal_physics.create_skeletal_physics_asset(**arguments)
        record['data'].update(raw_public_arguments=supplied,default_applied_keys=[k for k in DEFAULTS if k not in supplied])
    # Unexpected dispatch/export/print failure is not replaced with a known-zero result.
    payload=json.dumps(_envelope(record),ensure_ascii=False,allow_nan=False,separators=(',',':')).encode('utf-8')
    print(MARKER+nonce+':'+base64.b64encode(payload).decode('ascii'))
