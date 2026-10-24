"""Original skeleton-backed reads, preserving native loading and full read prefixes."""
import copy
from cubeless_mcp_ue58.compatibility.ue58 import skeletal as api
from . import state
from .state import OperationError

def _read_action(result,name,path,contains='',max_bones=500,checkpoint_callback=None):
    result['data'].update(operation=name,read_only=True,native_dispatched=False,native_result_returned=False,
        native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False,adapter_explicit_load_calls=0)
    try:
        if type(path) is not str or type(contains) is not str or type(max_bones) is not int or not -(2**31)<=max_bones<2**31:
            raise OperationError('skeletal_read_input_invalid','Original path/filter strings and reflected int32 limit required')
        result['data'].update(native_dispatched=True,mutation_state_unknown=True,mutation_performed=None)
        result['stage']='native_skeletal_read'
        native=api.list_skeleton_bones(path,contains,max_bones) if name==api.BINDINGS[0] else api.list_skeletal_sockets(path)
        result['data'].update(copy.deepcopy(native['data']),native_result_returned=True,native_raw_result=native['_raw_native_json'],
            native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ('data','_raw_native_json')},
            native_mutation_attempted=native['mutation_attempted'],mutation_state_unknown=not native['change_state_known'],
            mutation_performed=native['changed'] if native['change_state_known'] else None)
        result['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled'],status='native_read_returned')
        result['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'],status='native_read_returned')
        result['changes']=[] if native['change_state_known'] and native['changed'] is False else [{'kind':'skeletal_read_scope_drift','attempted':native['mutation_attempted'],
            'change_state_known':native['change_state_known'],'changed':native['changed'] if native['change_state_known'] else None}]
        result['stage']='returned_skeletal_read';result['validation'].update(status='native_skeletal_read_postcondition',native_postcondition_passed=native['postcondition_passed'])
        if checkpoint_callback is not None:checkpoint_callback(result)
        if native['mutation_attempted'] or native['compile_attempted'] or native['save_attempted']:
            raise OperationError('unexpected_skeletal_read_work','Unexpected actual work retained; no second action')
        if not native['success']:raise OperationError(native.get('error_code') or 'skeletal_read_failed',native.get('error') or 'Native read prefix retained')
        if not native['change_state_known'] or not native['postcondition_passed']:raise OperationError('skeletal_read_unverified','Incomplete current scoped read proof')
        result.update(success=True,stage='complete');return result
    except Exception as error:
        if getattr(error,'native_call_returned',False):result['data'].update(native_result_returned=True,native_raw_result=getattr(error,'native_raw_result',None))
        if getattr(error,'code','')=='native_skeletal_read_unavailable':result['data'].update(native_dispatched=False,mutation_state_unknown=False,mutation_performed=False)
        raise

def _read(name,path,contains='',max_bones=500):
    result=state.new_result()
    try:return _read_action(result,name,path,contains,max_bones)
    except Exception as error:return state.fail(result,error)


def list_skeleton_bones_action(result,skeleton_path,bone_name_contains='',max_bones=500,checkpoint_callback=None):
    return _read_action(result,api.BINDINGS[0],skeleton_path,bone_name_contains,max_bones,checkpoint_callback)


def list_skeletal_sockets_action(result,skeleton_path,checkpoint_callback=None):
    return _read_action(result,api.BINDINGS[1],skeleton_path,checkpoint_callback=checkpoint_callback)


def list_skeleton_bones(skeleton_path,bone_name_contains='',max_bones=500):return _read(api.BINDINGS[0],skeleton_path,bone_name_contains,max_bones)
def list_skeletal_sockets(skeleton_path):return _read(api.BINDINGS[1],skeleton_path)
