"""Original Pawn6 omissions/False/ordered partial outcomes on one shared journal.

Native owns property operations and the original admitted outer transaction.
No explicit Modify/PostEdit/compile/save or Python second writer is introduced.
"""
import copy
import json
from cubeless_mcp_ue58.compatibility.ue58 import pawn as api
from . import state,variables
from .state import OperationError
IDENTITY="set_pawn_properties"
FLAGS=("use_controller_rotation_yaw","use_controller_rotation_pitch","use_controller_rotation_roll","can_be_damaged")


def _arguments(blueprint_name,auto_possess_player,use_controller_rotation_yaw,use_controller_rotation_pitch,use_controller_rotation_roll,can_be_damaged):
    if type(auto_possess_player) is not str:
        raise OperationError("pawn_input_invalid","auto_possess_player requires the original literal string type")
    # Enum validity/bounds are per-property native decisions. Rejecting them
    # here would suppress later valid booleans and lose original partial work.
    auto=auto_possess_player
    values=dict(zip(FLAGS,(use_controller_rotation_yaw,use_controller_rotation_pitch,use_controller_rotation_roll,can_be_damaged)))
    for name,value in values.items():
        if value is not None and type(value) is not bool:
            raise OperationError("pawn_input_invalid",name+" requires boolean/null; False remains meaningful")
    empty=not auto and all(value is None for value in values.values())
    # The original public no-properties result precedes target path resolution.
    # Its old server connection check is not a new Engine lookup on this route.
    if type(blueprint_name) is not str:
        raise OperationError("pawn_input_invalid","blueprint_name requires the original public string type")
    path=blueprint_name if empty else variables._path(blueprint_name,write=True)
    return {"blueprint_path":path,"auto_possess_player":auto,**{name+"_json":json.dumps(value,allow_nan=False) for name,value in values.items()},"empty_request":empty}


def _context(result,arguments):
    result["data"].update(operation=IDENTITY,requested_arguments=copy.deepcopy(arguments),
        ordered_optional_property_names=["auto_possess_player",*FLAGS],
        adapter_explicit_asset_load_calls=0,adapter_explicit_compile_calls=0,adapter_explicit_save_calls=0,
        automatic_action_retry=False,full_original_pawn_contract_parity=False,
        actual_work_counts_authority="native_returned_raw_counts_and_ordered_property_receipts",
        arbitrary_callback_CDO_runtime_behavior_verified=False,cold_persistence_verified=False)


def _empty(result,arguments):
    _context(result,arguments)
    legacy={"success":True,"message":"No properties specified to set","results":{}}
    result["data"].update(legacy_response=legacy,message=legacy["message"],results={},no_properties_requested=True,
        native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False,
        target_path_resolution_performed=False,target_lookup_or_load_performed=False)
    result["changes"]=[]
    result["compile"].update(requested=False,attempted=False,compiled=None,status="no_properties_not_dispatched")
    result["save"].update(requested=False,attempted=False,saved=False,status="no_properties_not_dispatched")
    result["validation"].update(status="original_public_no_properties_shortcut",native_postcondition_verified=False)
    result.update(success=True,stage="complete")


def _apply_native(result,arguments,native):
    result["data"].update(copy.deepcopy(native["data"]))
    known=native["change_state_known"]
    result["data"].update(native_result=copy.deepcopy(native),native_raw_result=native["_raw_native_json"],
        native_outcome={k:copy.deepcopy(v) for k,v in native.items() if k not in ("data","_raw_native_json")},
        native_result_returned=True,native_mutation_attempted=native["mutation_attempted"],
        mutation_state_unknown=not known,mutation_performed=native["changed"] if known else None)
    _context(result,arguments)
    for key,leaf in (("compile","compiled"),("save","saved")):
        result[key].update(requested=native[key+"_requested"],attempted=native[key+"_attempted"],**{leaf:native[leaf]},
            status="native_returned" if native[key+"_attempted"] else "native_not_attempted")
    no_work=known and native["changed"] is False and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"]
    result["changes"]=[] if no_work else [{"kind":IDENTITY,"asset_path":arguments["blueprint_path"],"attempted":native["mutation_attempted"],
        "change_state_known":known,"changed":native["changed"] if known else None,"outcome":"native_scoped_observation" if known else "unknown_after_native_dispatch"}]
    result["validation"].update(status="native_pawn_postcondition" if native["postcondition_passed"] else "native_pawn_postcondition_failed",
        native_postcondition_passed=native["postcondition_passed"],disk_reload="not_run",runtime="not_run")
    result["stage"]="returned_pawn_properties_operation"


def _action(result,arguments,checkpoint_callback=None):
    if arguments["empty_request"]:
        _empty(result,arguments)
        return
    _context(result,arguments)
    callback=checkpoint_callback or state.checkpoint
    result["data"]["pending_mutation"]={"operation":IDENTITY,**arguments,"dispatch_state_at_checkpoint":"not_yet_dispatched"}
    result["compile"].update(requested=False,attempted=False,compiled=None,status="intent_prepared_not_dispatched")
    result["save"].update(requested=False,attempted=False,saved=False,status="intent_prepared_not_dispatched")
    result["stage"]="before_mutation_checkpoint";callback(result)
    result["data"].update(native_dispatched=True,native_result_returned=False,mutation_state_unknown=True,mutation_performed=None)
    result["changes"]=[{"kind":IDENTITY,"asset_path":arguments["blueprint_path"],"attempted":None,"changed":None,"change_state_known":False,"outcome":"unknown_until_native_response"}]
    result["stage"]="native_pawn_properties_operation"
    try:
        native=api.set_pawn_properties(**{k:v for k,v in arguments.items() if k!="empty_request"})
    except Exception as error:
        if getattr(error,"native_call_returned",False):
            result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            result["stage"]="returned_pawn_properties_operation";callback(result)
        if getattr(error,"code","")=="native_pawn_api_unavailable":
            result["data"].update(native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
            result["changes"]=[]
        raise
    _apply_native(result,arguments,native);callback(result)
    # Original any-success semantics are native-authoritative. Do not require
    # every property to succeed or turn an invalid enum into whole-call abort.
    if not native["success"]:
        raise OperationError(native.get("error_code") or "native_pawn_failed",native.get("error") or "Actual ordered property prefix retained; no retry")
    if not native["change_state_known"] or not native["postcondition_passed"]:
        raise OperationError("pawn_result_unverified","Native success lacks bounded known postcondition; full raw result retained")
    result.update(success=True,stage="complete")
    result["retry"]["duplicate_risk"]="inspect_current_Pawn_CDO_and_journal_no_setter_replay"


def set_pawn_properties(blueprint_name,auto_possess_player="",use_controller_rotation_yaw=None,use_controller_rotation_pitch=None,use_controller_rotation_roll=None,can_be_damaged=None):
    try:
        arguments=_arguments(blueprint_name,auto_possess_player,use_controller_rotation_yaw,use_controller_rotation_pitch,use_controller_rotation_roll,can_be_damaged)
    except Exception as error:
        result=state.fail(state.new_result(),error)
        result["data"].update(operation=IDENTITY,admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,mutation_state_unknown=False,mutation_performed=False)
        return result
    if arguments["empty_request"]:
        result=state.new_result();_empty(result,arguments)
        return result
    return state.mutate(IDENTITY,arguments,None,lambda result:_action(result,arguments))


def set_pawn_properties_action(result,blueprint_name,auto_possess_player="",use_controller_rotation_yaw=None,use_controller_rotation_pitch=None,use_controller_rotation_roll=None,can_be_damaged=None,checkpoint_callback=None):
    _action(result,_arguments(blueprint_name,auto_possess_player,use_controller_rotation_yaw,use_controller_rotation_pitch,use_controller_rotation_roll,can_be_damaged),checkpoint_callback)
