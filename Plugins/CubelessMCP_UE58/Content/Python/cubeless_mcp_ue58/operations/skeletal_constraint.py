"""Two original skeletal constraint contracts; no automatic world lifecycle or save."""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_constraint as api
from . import state
from .state import OperationError
CREATE,INSPECT=api.BINDINGS
FORBIDDEN_REFERENCE_TOKENS=('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')

def _strings(values,keys):
    for key in keys:
        value=values[key]
        if type(value) is not str:raise OperationError('skeletal_constraint_input_invalid','Original selector inputs require strings')
        try:value.encode('utf-16-le')
        except UnicodeEncodeError as error:raise OperationError('skeletal_constraint_input_invalid','Invalid Unicode selector') from error

def _reject_references(values,keys):
    for key in keys:
        value=values[key].strip().strip("\"'").replace('\\','/').lower()
        if any(token in value for token in FORBIDDEN_REFERENCE_TOKENS):
            raise OperationError('skeletal_constraint_dependency_reference','Refusing an original MCP-only selector reference')

def _number(value):
    if type(value) not in (int,float):raise OperationError('skeletal_constraint_input_invalid','Original numeric fields require numbers, not booleans')
    try:result=float(value)
    except (OverflowError,ValueError) as error:raise OperationError('skeletal_constraint_input_invalid','Numeric input exceeds finite double carrier') from error
    if not math.isfinite(result):raise OperationError('skeletal_constraint_input_invalid','Finite numeric input required')
    return result

def _create_args(skeletal_mesh_path,actor_name='MCP_SkeletalBoneConstraint_Ghost',bone_name1='hand_l',bone_name2='hand_r',world_context='editor',location=None,constraint_component_name='MCP_SkeletalBoneConstraint',swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=30.0,disable_collision=True,simulate_all_bodies=True,dry_run=True):
    values=dict(skeletal_mesh_path=skeletal_mesh_path,actor_name=actor_name,bone_name1=bone_name1,bone_name2=bone_name2,world_context=world_context,location=location,constraint_component_name=constraint_component_name,swing1_limit_degrees=swing1_limit_degrees,swing2_limit_degrees=swing2_limit_degrees,twist_limit_degrees=twist_limit_degrees,disable_collision=disable_collision,simulate_all_bodies=simulate_all_bodies,dry_run=dry_run)
    keys=('skeletal_mesh_path','actor_name','bone_name1','bone_name2','world_context','constraint_component_name');_strings(values,keys)
    _reject_references(values,('skeletal_mesh_path','actor_name','bone_name1','bone_name2','constraint_component_name'))
    if world_context not in ('editor','pie','game','game_preview'):raise OperationError('skeletal_constraint_invalid_world_context','world_context must be editor, pie, game, or game_preview')
    if any(type(values[k]) is not bool for k in ('disable_collision','simulate_all_bodies','dry_run')):raise OperationError('skeletal_constraint_input_invalid','Original options require booleans')
    if location is not None:
        if type(location) is not list or len(location)!=3:raise OperationError('skeletal_constraint_invalid_location','location must be a list of three numbers')
        values['location']=[_number(v) for v in location]
    for key in ('swing1_limit_degrees','swing2_limit_degrees','twist_limit_degrees'):values[key]=_number(values[key])
    # Null remains omitted native location; explicit [] remains an invalid request.
    encoded=json.dumps(values['location'],ensure_ascii=False,allow_nan=False,separators=(',',':'));encoded.encode('utf-8')
    return values,encoded

def _inspect_args(actor_name,world_context='auto',constraint_component_name=''):
    values=dict(actor_name=actor_name,world_context=world_context,constraint_component_name=constraint_component_name)
    _strings(values,tuple(values));_reject_references(values,('actor_name','constraint_component_name'))
    if world_context not in ('auto','editor','pie','game','game_preview'):raise OperationError('skeletal_constraint_invalid_world_context','world_context must be auto, editor, pie, game, or game_preview')
    return values

def _context(result,name,args):
    result['data'].update(operation=name,requested_arguments=copy.deepcopy(args),read_only=name==INSPECT,
        adapter_explicit_asset_load_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        adapter_play_start_stop_calls=0,automatic_action_retry=False,full_original_skeletal_native_parity=False,
        runtime_persistence_verified=False,simulation_force_or_pose_stability_verified=False)

def _returned(result,name,args,native):
    known=native['change_state_known']
    result['data'].update(copy.deepcopy(native['data']),native_raw_result=native['_raw_native_json'],
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},
        native_result_returned=True,native_mutation_attempted=native['mutation_attempted'],
        mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,name,args)
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_returned')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_returned_saved' if native['saved'] else 'native_returned_failed' if native['save_attempted'] else 'native_not_attempted')
    none=known and native['changed'] is False and not native['mutation_attempted'] and not native['save_attempted']
    result['changes']=[] if none else [{'kind':name,'attempted':native['mutation_attempted'],'change_state_known':known,
        'changed':native['changed'] if known else None,'outcome':'native_scoped_observation' if known else 'unknown_after_dispatch'}]
    result['validation'].update(status='native_skeletal_constraint_postcondition',native_postcondition_passed=native['postcondition_passed'],
        full_world_state='not_observed',simulation_stability='not_observed',cold_persistence='not_applicable_transient_fixture')
    result['stage']='returned_skeletal_constraint_operation'

def _dispatch(result,name,args,location_json=None,checkpoint_callback=None):
    callback=checkpoint_callback or state.checkpoint;_context(result,name,args)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,mutation_state_unknown=False,
        mutation_performed=False,pending_operation={'operation':name,**args,'dispatch_state_at_checkpoint':'not_yet_dispatched'})
    result['compile'].update(requested=False,attempted=False,compiled=None,status='not_dispatched')
    result['save'].update(requested=False,attempted=False,saved=False,status='not_dispatched')
    result['stage']='before_mutation_checkpoint';callback(result)
    result['data'].update(native_dispatched=True,mutation_state_unknown=True,mutation_performed=None)
    result['stage']='native_skeletal_constraint_operation';result['changes']=[{'kind':name,'attempted':None,'changed':None,'change_state_known':False}]
    result['compile'].update(attempted=None,compiled=None,status='unknown_until_native_returns')
    result['save'].update(attempted=None,saved=None,status='unknown_until_native_returns')
    try:
        if name==CREATE:
            internal=dict(args);del internal['location'];internal['location_json']=location_json
            native=api.create_skeletal_bone_constraint_fixture(**internal)
        else:native=api.inspect_skeletal_bone_constraint_fixture(**args)
    except Exception as error:
        if getattr(error,'native_call_returned',False):
            result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None));result['stage']='returned_skeletal_constraint_operation';callback(result)
        if getattr(error,'code','')=='native_skeletal_constraint_api_unavailable':
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result['compile'].update(attempted=False,compiled=None,status='native_not_dispatched');result['save'].update(attempted=False,saved=False,status='native_not_dispatched');result['changes']=[]
        raise
    _returned(result,name,args,native);callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'skeletal_constraint_operation_failed',native.get('error') or 'Actual native prefix retained; no retry')
    if not native['change_state_known'] or not native['postcondition_passed']:raise OperationError('skeletal_constraint_unverified','Native success lacks complete current proof')
    if native['compile_requested'] or native['compile_attempted'] or native['compiled'] is not None or native['save_requested'] or native['save_attempted'] or native['saved']:
        raise OperationError('skeletal_constraint_unexpected_compile_save','Unexpected actual work retained; no second action')
    if name==INSPECT and native['mutation_attempted']:raise OperationError('skeletal_constraint_unexpected_read_work','Unexpected actual inspector writer prefix retained')
    result.update(success=True,stage='complete');result['retry']['duplicate_risk']='inspect_owned_fixture_world_and_journal_no_writer_replay'

def _admission_error(error):
    result=state.fail(state.new_result(),error);result['data'].update(admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
        mutation_state_unknown=False,mutation_performed=False);return result

def create_skeletal_bone_constraint_fixture(skeletal_mesh_path,actor_name='MCP_SkeletalBoneConstraint_Ghost',bone_name1='hand_l',bone_name2='hand_r',world_context='editor',location=None,constraint_component_name='MCP_SkeletalBoneConstraint',swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=30.0,disable_collision=True,simulate_all_bodies=True,dry_run=True):
    try:args,encoded=_create_args(skeletal_mesh_path,actor_name,bone_name1,bone_name2,world_context,location,constraint_component_name,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,disable_collision,simulate_all_bodies,dry_run)
    except Exception as error:return _admission_error(error)
    return state.mutate(CREATE,args,None,lambda result:_dispatch(result,CREATE,args,encoded))

def create_skeletal_bone_constraint_fixture_action(result,skeletal_mesh_path,actor_name='MCP_SkeletalBoneConstraint_Ghost',bone_name1='hand_l',bone_name2='hand_r',world_context='editor',location=None,constraint_component_name='MCP_SkeletalBoneConstraint',swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=30.0,disable_collision=True,simulate_all_bodies=True,dry_run=True,checkpoint_callback=None):
    args,encoded=_create_args(skeletal_mesh_path,actor_name,bone_name1,bone_name2,world_context,location,constraint_component_name,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,disable_collision,simulate_all_bodies,dry_run)
    _dispatch(result,CREATE,args,encoded,checkpoint_callback)

def inspect_skeletal_bone_constraint_fixture(actor_name,world_context='auto',constraint_component_name=''):
    try:args=_inspect_args(actor_name,world_context,constraint_component_name)
    except Exception as error:return _admission_error(error)
    result=state.new_result()
    try:_dispatch(result,INSPECT,args,checkpoint_callback=lambda observed:None);return result
    except Exception as error:return state.fail(result,error)

def inspect_skeletal_bone_constraint_fixture_action(result,actor_name,world_context='auto',constraint_component_name='',checkpoint_callback=None):
    args=_inspect_args(actor_name,world_context,constraint_component_name)
    _dispatch(result,INSPECT,args,checkpoint_callback=checkpoint_callback)
