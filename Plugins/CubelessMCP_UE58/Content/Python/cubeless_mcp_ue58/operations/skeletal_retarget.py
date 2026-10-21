"""Original Retarget5: one journal, opaque native setter/read/save prefix."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import skeletal_retarget as api
from . import state
from .state import OperationError
NAME=api.BINDINGS[0]

def _args(skeleton_path,bone_name,mode,recursive,save):
    values={'skeleton_path':skeleton_path,'bone_name':bone_name,'mode':mode,'recursive':recursive,'save':save}
    if any(type(values[k]) is not str for k in ('skeleton_path','bone_name','mode')) or type(recursive) is not bool or type(save) is not bool:
        raise OperationError('skeletal_retarget_input_invalid','Original three strings and two booleans required')
    for k in ('skeleton_path','bone_name','mode'):
        try:values[k].encode('utf-16-le')
        except UnicodeEncodeError as error:raise OperationError('skeletal_retarget_input_invalid','Invalid Unicode string') from error
    # Literal mode/name/path remain native-owned: no trim, alias, dependency guard, JSON-text surrogate or Python load.
    return values

def _context(result,args):
    result['data'].update(operation=NAME,requested_arguments=copy.deepcopy(args),adapter_explicit_asset_load_calls=0,
        adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,automatic_action_retry=False,
        full_original_skeletal_native_parity=False,retarget_animation_evaluation_verified=False,cold_persistence_verified=False)

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
    result['validation'].update(status='native_skeletal_retarget_postcondition',native_postcondition_passed=native['postcondition_passed'],
        full_asset_reflected_state='not_observed',animation_evaluation='not_run',disk_reload='not_run')
    result['stage']='returned_skeletal_retarget_operation'

def _action(result,args,checkpoint_callback=None):
    callback=checkpoint_callback or state.checkpoint;_context(result,args)
    result['data'].update(native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,mutation_state_unknown=False,
        mutation_performed=False,pending_mutation={'operation':NAME,**args,'dispatch_state_at_checkpoint':'not_yet_dispatched'})
    result['stage']='before_mutation_checkpoint';result['save'].update(requested=args['save'],attempted=False,saved=False,status='not_dispatched');callback(result)
    result['data'].update(native_dispatched=True,mutation_state_unknown=True,mutation_performed=None);result['stage']='native_skeletal_retarget_operation'
    result['save'].update(attempted=None,saved=None,status='unknown_until_native_returns')
    result['changes']=[{'kind':NAME,'attempted':None,'changed':None,'change_state_known':False}]
    try:native=api.set_bone_translation_retargeting(**args)
    except Exception as error:
        if getattr(error,'native_call_returned',False):
            result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None));result['stage']='returned_skeletal_retarget_operation';callback(result)
        if getattr(error,'code','')=='native_skeletal_retarget_api_unavailable':
            result['data'].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result['save'].update(attempted=False,saved=False,status='native_not_dispatched');result['changes']=[]
        raise
    _returned(result,args,native);callback(result)
    if not native['success']:raise OperationError(native.get('error_code') or 'skeletal_retarget_failed',native.get('error') or 'Actual retarget prefix retained; no retry')
    if not native['change_state_known'] or not native['postcondition_passed']:raise OperationError('skeletal_retarget_unverified','Native success lacks complete current proof')
    if native['compile_requested'] or native['compile_attempted'] or native['compiled'] is not None:
        raise OperationError('skeletal_retarget_unexpected_compile','Unexpected actual compiler prefix retained; no second action')
    result.update(success=True,stage='complete');result['retry']['duplicate_risk']='inspect_skeletal_mode_and_journal_no_writer_replay'

def set_bone_translation_retargeting(skeleton_path,bone_name,mode,recursive=False,save=True):
    try:args=_args(skeleton_path,bone_name,mode,recursive,save)
    except Exception as error:
        result=state.fail(state.new_result(),error);result['data'].update(admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
            mutation_state_unknown=False,mutation_performed=False);return result
    return state.mutate(NAME,args,None,lambda result:_action(result,args))

def set_bone_translation_retargeting_action(result,skeleton_path,bone_name,mode,recursive=False,save=True,checkpoint_callback=None):
    _action(result,_args(skeleton_path,bone_name,mode,recursive,save),checkpoint_callback)
