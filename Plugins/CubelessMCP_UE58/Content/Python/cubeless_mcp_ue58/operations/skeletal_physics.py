"""Original PhysicsAsset15: one journal, complete native generation/dry/save prefix."""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_physics as api
from . import state
from .state import OperationError
NAME=api.BINDINGS[0]

def _args(skeletal_mesh_path,target_physics_asset_path,expected_bones=None,kinematic_root_bone='',root_kinematic=True,disable_collision=True,swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=25.0,linear_damping=0.15,angular_damping=0.5,tip_mass_scale=3.0,overwrite_existing=False,save=True,dry_run=True):
    values={'skeletal_mesh_path':skeletal_mesh_path,'target_physics_asset_path':target_physics_asset_path,'expected_bones':expected_bones,'kinematic_root_bone':kinematic_root_bone,'root_kinematic':root_kinematic,'disable_collision':disable_collision,'swing1_limit_degrees':swing1_limit_degrees,'swing2_limit_degrees':swing2_limit_degrees,'twist_limit_degrees':twist_limit_degrees,'linear_damping':linear_damping,'angular_damping':angular_damping,'tip_mass_scale':tip_mass_scale,'overwrite_existing':overwrite_existing,'save':save,'dry_run':dry_run}
    if any(type(values[k]) is not str for k in ('skeletal_mesh_path','target_physics_asset_path','kinematic_root_bone')):
        raise OperationError('skeletal_physics_input_invalid','Original path/root inputs require strings')
    if any(type(values[k]) is not bool for k in ('root_kinematic','disable_collision','overwrite_existing','save','dry_run')):
        raise OperationError('skeletal_physics_input_invalid','Original options require exact booleans')
    if expected_bones is not None and (type(expected_bones) is not list or any(type(v) is not str for v in expected_bones)):
        raise OperationError('skeletal_physics_expected_bones_invalid','Original expected_bones requires a string array or null')
    for text in [skeletal_mesh_path,target_physics_asset_path,kinematic_root_bone]+(expected_bones or []):
        try:text.encode('utf-16-le')
        except UnicodeEncodeError as error:raise OperationError('skeletal_physics_input_invalid','Invalid Unicode input') from error
    for key in ('swing1_limit_degrees','swing2_limit_degrees','twist_limit_degrees','linear_damping','angular_damping','tip_mass_scale'):
        if type(values[key]) not in (int,float):raise OperationError('skeletal_physics_input_invalid','Original numeric options require numbers, not booleans')
        try:value=float(values[key])
        except (OverflowError,ValueError) as error:raise OperationError('skeletal_physics_input_invalid','Numeric option exceeds finite double carrier') from error
        if not math.isfinite(value):raise OperationError('skeletal_physics_input_invalid','Finite numeric options required')
        values[key]=value
    # Preserve only the original public target VALUE guard, before journal/dispatch.
    target=target_physics_asset_path.strip().strip("\"'").replace('\\','/').lower()
    if any(token in target for token in ('/script/unrealmcp','/unrealmcp','unrealmcp','mcpunreal','mcp_unreal','/script/cubelesscontrolrigtools','cubelesscontrolrigtools','cubelesscontrolrigtoolseditor')):
        raise OperationError('skeletal_physics_dependency_reference','Refusing an MCP-only target reference')
    # Native owns path/name/count/root/finite-float32 range and original Clamp/Max policy.
    # None and [] remain distinct requested JSON values; no public text surrogate.
    encoded=json.dumps(expected_bones,ensure_ascii=False,allow_nan=False,separators=(',',':'))
    encoded.encode('utf-8')
    return values,encoded

def _context(result,args):
    result['data'].update(operation=NAME,requested_arguments=copy.deepcopy(args),adapter_explicit_asset_load_calls=0,
        adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,automatic_action_retry=False,
        full_original_skeletal_native_parity=False,runtime_physics_evaluation_verified=False,cold_persistence_verified=False)

def _returned(result,args,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']),native_raw_result=native['_raw_native_json'],
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},
        native_result_returned=True,native_mutation_attempted=native['mutation_attempted'],
        mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,args)
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_returned')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],
        status='native_returned_saved' if native['saved'] else 'native_returned_failed' if native['save_attempted'] else 'native_not_attempted')
    none=known and native['changed'] is False and not native['mutation_attempted'] and not native['save_attempted']
    result['changes']=[] if none else [{'kind':NAME,'attempted':native['mutation_attempted'],'change_state_known':known,
        'changed':native['changed'] if known else None,'outcome':'native_scoped_observation' if known else 'unknown_after_dispatch'}]
    result['validation'].update(status='native_skeletal_physics_asset_postcondition',native_postcondition_passed=native['postcondition_passed'],
        full_asset_reflected_state='not_observed',physics_evaluation='not_run',disk_reload='not_run')
    result['stage']='returned_skeletal_physics_asset_operation'

def _action(result,args,expected_json,checkpoint_callback=None):
    callback=checkpoint_callback or state.checkpoint;_context(result,args)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,mutation_state_unknown=False,
        mutation_performed=False,pending_mutation={'operation':NAME,**args,'dispatch_state_at_checkpoint':'not_yet_dispatched'})
    result['stage']='before_mutation_checkpoint';result['save'].update(requested=args['save'],attempted=False,saved=False,status='not_dispatched');callback(result)
    result['data'].update(native_dispatched=True,mutation_state_unknown=True,mutation_performed=None);result['stage']='native_skeletal_physics_asset_operation'
    result['save'].update(attempted=None,saved=None,status='unknown_until_native_returns')
    result['changes']=[{'kind':NAME,'attempted':None,'changed':None,'change_state_known':False}]
    try:
        internal=dict(args);internal['expected_bones_json']=expected_json;del internal['expected_bones']
        native=api.create_skeletal_physics_asset(**internal)
    except Exception as error:
        if getattr(error,'native_call_returned',False):
            result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None));result['stage']='returned_skeletal_physics_asset_operation';callback(result)
        if getattr(error,'code','')=='native_skeletal_physics_api_unavailable':
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result['save'].update(attempted=False,saved=False,status='native_not_dispatched');result['changes']=[]
        raise
    _returned(result,args,native);callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'skeletal_physics_asset_failed',native.get('error') or 'Actual PhysicsAsset prefix retained; no retry')
    if not native['change_state_known'] or not native['postcondition_passed']:raise OperationError('skeletal_physics_unverified','Native success lacks complete current proof')
    if native['compile_requested'] or native['compile_attempted'] or native['compiled'] is not None:
        raise OperationError('skeletal_physics_unexpected_compile','Unexpected actual compiler prefix retained; no second action')
    result.update(success=True,stage='complete');result['retry']['duplicate_risk']='inspect_physics_asset_source_and_journal_no_writer_replay'

def create_skeletal_physics_asset(skeletal_mesh_path,target_physics_asset_path,expected_bones=None,kinematic_root_bone='',root_kinematic=True,disable_collision=True,swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=25.0,linear_damping=0.15,angular_damping=0.5,tip_mass_scale=3.0,overwrite_existing=False,save=True,dry_run=True):
    try:args,encoded=_args(skeletal_mesh_path,target_physics_asset_path,expected_bones,kinematic_root_bone,root_kinematic,disable_collision,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,linear_damping,angular_damping,tip_mass_scale,overwrite_existing,save,dry_run)
    except Exception as error:
        result=state.fail(state.new_result(),error);result['data'].update(admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
            mutation_state_unknown=False,mutation_performed=False);return result
    return state.mutate(NAME,args,None,lambda result:_action(result,args,encoded))

def create_skeletal_physics_asset_action(result,skeletal_mesh_path,target_physics_asset_path,expected_bones=None,kinematic_root_bone='',root_kinematic=True,disable_collision=True,swing1_limit_degrees=45.0,swing2_limit_degrees=45.0,twist_limit_degrees=25.0,linear_damping=0.15,angular_damping=0.5,tip_mass_scale=3.0,overwrite_existing=False,save=True,dry_run=True,checkpoint_callback=None):
    args,encoded=_args(skeletal_mesh_path,target_physics_asset_path,expected_bones,kinematic_root_bone,root_kinematic,disable_collision,swing1_limit_degrees,swing2_limit_degrees,twist_limit_degrees,linear_damping,angular_damping,tip_mass_scale,overwrite_existing,save,dry_run)
    _action(result,args,encoded,checkpoint_callback)
