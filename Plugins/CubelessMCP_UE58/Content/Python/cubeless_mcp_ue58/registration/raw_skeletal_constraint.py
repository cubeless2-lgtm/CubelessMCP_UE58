"""Two fixed skeletal fixture/inspector helpers; no arbitrary dispatch."""
import base64
import binascii
import json
import re
from cubeless_mcp_ue58.operations import skeletal_constraint,state
from cubeless_mcp_ue58.operations.state import OperationError
from .results import reflected_result
MARKER='__CUBELESS_SKELETAL_CONSTRAINT_RESULT__'
MAX_ARGUMENT_BYTES=131072
PUBLIC_FIELDS_BY_METHOD={
    'create_skeletal_bone_constraint_fixture':('skeletal_mesh_path','actor_name','bone_name1','bone_name2','world_context','location','constraint_component_name','swing1_limit_degrees','swing2_limit_degrees','twist_limit_degrees','disable_collision','simulate_all_bodies','dry_run'),
    'inspect_skeletal_bone_constraint_fixture':('actor_name','world_context','constraint_component_name')}
DEFAULTS_BY_METHOD={
    'create_skeletal_bone_constraint_fixture':{'actor_name':'MCP_SkeletalBoneConstraint_Ghost','bone_name1':'hand_l','bone_name2':'hand_r','world_context':'editor','location':None,'constraint_component_name':'MCP_SkeletalBoneConstraint','swing1_limit_degrees':45.0,'swing2_limit_degrees':45.0,'twist_limit_degrees':30.0,'disable_collision':True,'simulate_all_bodies':True,'dry_run':True},
    'inspect_skeletal_bone_constraint_fixture':{'world_context':'auto','constraint_component_name':''}}
RESULT_FIELDS=('success','operation_id','stage','error_code','error','changes_json','compile_json','save_json','validation_json','retry_json','data_json')
METHODS=tuple(PUBLIC_FIELDS_BY_METHOD)

def _unique(pairs):
    result={}
    for key,value in pairs:
        if key in result:raise OperationError('raw_skeletal_constraint_arguments_invalid','Duplicate JSON keys are not admitted')
        result[key]=value
    return result

def _nonfinite(value):raise OperationError('raw_skeletal_constraint_arguments_invalid','Nonfinite JSON literals are not admitted')

def _arguments(method,encoded):
    if method not in METHODS:raise OperationError('raw_skeletal_constraint_method_invalid','Exact fixture/inspector method required')
    if type(encoded) is not str or len(encoded)>4*((MAX_ARGUMENT_BYTES+2)//3):raise OperationError('raw_skeletal_constraint_arguments_invalid','Bounded base64 JSON required')
    try:
        raw=base64.b64decode(encoded,validate=True)
        if len(raw)>MAX_ARGUMENT_BYTES:raise OperationError('raw_skeletal_constraint_arguments_invalid','Raw arguments exceed128KiB')
        supplied=json.loads(raw.decode('utf-8'),object_pairs_hook=_unique,parse_constant=_nonfinite)
    except (binascii.Error,UnicodeError,ValueError,RecursionError) as error:raise OperationError('raw_skeletal_constraint_arguments_invalid','Strict base64/UTF8/JSON required') from error
    fields=PUBLIC_FIELDS_BY_METHOD[method];defaults=DEFAULTS_BY_METHOD[method]
    if type(supplied) is not dict or set(supplied)-set(fields) or any(k not in supplied for k in fields if k not in defaults):
        raise OperationError('raw_skeletal_constraint_arguments_invalid','Exact original fields and required selectors expected')
    arguments={**defaults,**supplied}
    if method==METHODS[0]:skeletal_constraint._create_args(**arguments)
    else:skeletal_constraint._inspect_args(**arguments)
    return arguments,supplied

def _envelope(record):
    reflected=reflected_result(record);fields={key:getattr(reflected,key) for key in RESULT_FIELDS}
    if type(fields['success']) is not bool or any(type(fields[key]) is not str for key in RESULT_FIELDS[1:]):raise OperationError('raw_skeletal_constraint_result_invalid','Exact operation result types required')
    return {'returnValue':fields}

def emit_result(method,encoded_args_b64,nonce):
    if type(method) is not str or method not in METHODS:raise OperationError('raw_skeletal_constraint_method_invalid','Exact fixture/inspector method required')
    if type(nonce) is not str or re.fullmatch('[0-9a-f]{32}',nonce) is None:raise OperationError('raw_skeletal_constraint_nonce_invalid','Native owner nonce must be lowercase UUID hex')
    try:arguments,supplied=_arguments(method,encoded_args_b64)
    except Exception as error:
        record=state.fail(state.new_result(),error)
        record['data'].update(operation=method,admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
            mutation_state_unknown=False,mutation_performed=False,raw_named_toolset_argument_validation_failed=True)
    else:
        if method==METHODS[0]:record=skeletal_constraint.create_skeletal_bone_constraint_fixture(**arguments)
        else:record=skeletal_constraint.inspect_skeletal_bone_constraint_fixture(**arguments)
        record['data'].update(raw_public_arguments=supplied,default_applied_keys=[k for k in DEFAULTS_BY_METHOD[method] if k not in supplied])
    # Unexpected dispatch/export/print failure remains raw-owner uncertainty.
    payload=json.dumps(_envelope(record),ensure_ascii=False,allow_nan=False,separators=(',',':')).encode('utf-8')
    print(MARKER+nonce+':'+base64.b64encode(payload).decode('ascii'))
