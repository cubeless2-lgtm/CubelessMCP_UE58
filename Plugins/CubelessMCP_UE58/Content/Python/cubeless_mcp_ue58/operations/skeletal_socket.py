"""Socket add/set/remove: original None/empty/rename semantics and one writer journal."""
import copy
import json
import math
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_socket as api
from . import state
from .state import OperationError

def _string(value,field):
    if type(value) is not str:raise OperationError('skeletal_socket_input_invalid',field+' requires the original string input')
    try:value.encode('utf-16-le')
    except UnicodeEncodeError as error:raise OperationError('skeletal_socket_input_invalid','Invalid Unicode string') from error
    return value

def _reference(value,field):
    candidate=value.strip().strip('"\'').replace('\\','/').lower()
    if any(token in candidate for token in ('unrealmcp','mcpunreal','mcp_unreal','cubelesscontrolrigtools')):
        raise OperationError('skeletal_socket_dependency_forbidden',field+' contains an original forbidden dependency reference')

def _vector(value,field):
    if value is not None and type(value) is not list:raise OperationError('skeletal_socket_input_invalid',field+' requires numeric array or None, not JSON text')
    if value is not None:
        if len(value)>64:raise OperationError('skeletal_socket_input_invalid','Transform exceeds finite transport bound; no dispatch')
        for item in value:
            if type(item) not in (int,float):raise OperationError('skeletal_socket_input_invalid',field+' requires finite JSON numbers')
            try:valid=math.isfinite(item)
            except OverflowError:valid=False
            if not valid:raise OperationError('skeletal_socket_input_invalid',field+' requires finite JSON numbers')
    # None becomes omitted native semantics. Explicit [] stays [] and reaches native invalid-length refusal.
    return json.dumps(value,allow_nan=False,separators=(',',':'))

def _args(name,skeleton_path,socket_name,bone_name='',new_socket_name='',location=None,rotation=None,scale=None,save=True):
    if type(save) is not bool:raise OperationError('skeletal_socket_input_invalid','save requires a boolean')
    out={'skeleton_path':_string(skeleton_path,'skeleton_path'),'socket_name':_string(socket_name,'socket_name'),'save':save}
    if name==api.BINDINGS[0]:_reference(out['socket_name'],'socket_name')
    if name!=api.BINDINGS[2]:
        out['bone_name']=_string(bone_name,'bone_name')
        if name==api.BINDINGS[1]:
            out['new_socket_name']=_string(new_socket_name,'new_socket_name')
            if out['new_socket_name']:_reference(out['new_socket_name'],'new_socket_name')
        out.update(location_json=_vector(location,'location'),rotation_json=_vector(rotation,'rotation'),scale_json=_vector(scale,'scale'))
    return out

def _context(result,name,args):
    result['data'].update(operation=name,requested_arguments=copy.deepcopy(args),adapter_explicit_asset_load_calls=0,
        adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,automatic_action_retry=False,
        full_original_skeletal_native_parity=False,cold_persistence_verified=False,runtime_socket_attachment_verified=False)

def _returned(result,name,args,native):
    known=native['change_state_known'];result['data'].update(copy.deepcopy(native['data']),native_raw_result=native['_raw_native_json'],
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},native_result_returned=True,
        native_mutation_attempted=native['mutation_attempted'],mutation_state_unknown=not known,mutation_performed=native['changed'] if known else None)
    _context(result,name,args)
    result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_returned')
    result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],
        status='native_returned_saved' if native['saved'] else 'native_returned_failed' if native['save_attempted'] else 'native_not_attempted')
    none=known and native['changed'] is False and not native['mutation_attempted'] and not native['save_attempted']
    result['changes']=[] if none else [{'kind':name,'attempted':native['mutation_attempted'],'change_state_known':known,
        'changed':native['changed'] if known else None,'outcome':'native_scoped_observation' if known else 'unknown_after_dispatch'}]
    result['validation'].update(status='native_socket_postcondition',native_postcondition_passed=native['postcondition_passed'],
        full_asset_reflected_state='not_observed',disk_reload='not_run',implicit_loading_save_callbacks='not_exhaustively_measured')
    result['stage']='returned_skeletal_socket_operation'

def _action(result,name,args,checkpoint_callback=None):
    callback=checkpoint_callback or state.checkpoint;_context(result,name,args)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False,
        pending_mutation={'operation':name,**args,'dispatch_state_at_checkpoint':'not_yet_dispatched'})
    result['stage']='before_mutation_checkpoint';result['save'].update(requested=args['save'],attempted=False,saved=False,status='not_dispatched');callback(result)
    result['data'].update(native_dispatched=True,mutation_state_unknown=True,mutation_performed=None);result['stage']='native_skeletal_socket_operation'
    result['save'].update(attempted=None,saved=None,status='unknown_until_native_returns');result['changes']=[{'kind':name,'attempted':None,'changed':None,'change_state_known':False}]
    try:native=getattr(api,name)(**args)
    except Exception as error:
        if getattr(error,'native_call_returned',False):
            result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None));result['stage']='returned_skeletal_socket_operation';callback(result)
        if getattr(error,'code','')=='native_skeletal_socket_api_unavailable':
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result['save'].update(attempted=False,saved=False,status='native_not_dispatched');result['changes']=[]
        raise
    _returned(result,name,args,native);callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'skeletal_socket_failed',native.get('error') or 'Actual socket prefix retained; no replay')
    if not native['change_state_known'] or not native['postcondition_passed']:raise OperationError('skeletal_socket_unverified','Native success lacks complete current proof')
    if native['compile_requested'] or native['compile_attempted'] or native['compiled'] is not None:raise OperationError('skeletal_socket_unexpected_compile','Actual compile prefix retained; no second action')
    result.update(success=True,stage='complete');result['retry']['duplicate_risk']='inspect_socket_and_journal_no_writer_replay'

def _named(name,args_builder):
    try:args=args_builder()
    except Exception as error:
        result=state.fail(state.new_result(),error);result['data'].update(admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False);return result
    return state.mutate(name,args,None,lambda result:_action(result,name,args))

def add_skeletal_socket(skeleton_path,socket_name,bone_name,location=None,rotation=None,scale=None,save=True):
    return _named(api.BINDINGS[0],lambda:_args(api.BINDINGS[0],skeleton_path,socket_name,bone_name,location=location,rotation=rotation,scale=scale,save=save))
def set_skeletal_socket(skeleton_path,socket_name,bone_name='',new_socket_name='',location=None,rotation=None,scale=None,save=True):
    return _named(api.BINDINGS[1],lambda:_args(api.BINDINGS[1],skeleton_path,socket_name,bone_name,new_socket_name,location,rotation,scale,save))
def remove_skeletal_socket(skeleton_path,socket_name,save=True):return _named(api.BINDINGS[2],lambda:_args(api.BINDINGS[2],skeleton_path,socket_name,save=save))
def add_skeletal_socket_action(result,skeleton_path,socket_name,bone_name,location=None,rotation=None,scale=None,save=True,checkpoint_callback=None):
    _action(result,api.BINDINGS[0],_args(api.BINDINGS[0],skeleton_path,socket_name,bone_name,location=location,rotation=rotation,scale=scale,save=save),checkpoint_callback)
def set_skeletal_socket_action(result,skeleton_path,socket_name,bone_name='',new_socket_name='',location=None,rotation=None,scale=None,save=True,checkpoint_callback=None):
    _action(result,api.BINDINGS[1],_args(api.BINDINGS[1],skeleton_path,socket_name,bone_name,new_socket_name,location,rotation,scale,save),checkpoint_callback)
def remove_skeletal_socket_action(result,skeleton_path,socket_name,save=True,checkpoint_callback=None):
    _action(result,api.BINDINGS[2],_args(api.BINDINGS[2],skeleton_path,socket_name,save=save),checkpoint_callback)
