"""Spline points public contract and bounded two-route execution accounting.

Named10 selects Native first or the actual in-editor Python alternative. An
explicit raw context dict selects the old Native command path and preserves
hidden8 plus component/closed-loop omission. There is no transport implementation.
"""
import copy
import math
from typing import Any, Dict
from cubeless_mcp_ue58.compatibility.ue58 import spline_component_points as api
from . import state
from .state import OperationError

METHOD="set_spline_component_points"
PUBLIC_FIELDS=("points","actor_label","actor_name","actor_tag","actor_label_prefix","component_name","component_tag","selected_only","closed_loop","native_first")
TEXT_FIELDS=PUBLIC_FIELDS[1:7]
BOOL_FIELDS=PUBLIC_FIELDS[7:]
LEGACY_COMMAND_FIELDS=api.LEGACY_COMMAND_FIELDS
_FORBIDDEN_MCP_REFERENCE_TOKENS=("/script/unrealmcp","/unrealmcp","unrealmcp","mcpunreal","mcp_unreal","/script/cubelesscontrolrigtools","cubelesscontrolrigtools","cubelesscontrolrigtoolseditor")


def _reject_legacy_command_dependency_refs(params):
    """Original public Batch recursive string-value guard, never a Named guard."""
    def visit(field,value):
        if isinstance(value,str):
            candidate=value.strip().strip("\"'").replace("\\","/").lower()
            if any(token in candidate for token in _FORBIDDEN_MCP_REFERENCE_TOKENS):
                error=OperationError("spline_component_points_dependency_reference","Refusing MCP-only reference in '"+field+"'")
                error.field_name=field;error.reason="mcp_only_reference";raise error
        elif isinstance(value,dict):
            for key,row in value.items():visit(field+"."+str(key),row)
        elif isinstance(value,(list,tuple,set)):
            for i,row in enumerate(value):visit(field+"["+str(i)+"]",row)
    visit("params",params)


def _validate_points(points,python_path=False):
    if type(points) is not list or len(points)<2:
        raise OperationError("pcg_invalid_spline_points","points must contain at least two [x, y, z] values")
    for i,row in enumerate(points):
        if type(row) is list and len(row)>=3:xyz=row[:3]
        elif not python_path and type(row) is dict:
            xyz=[]
            for lower in ("x","y","z"):
                key=lower if type(row.get(lower)) in (int,float) else lower.upper()
                if key not in row:raise OperationError("spline_component_points_input_invalid","point lacks xyz: "+str(i))
                xyz.append(row[key])
        else:raise OperationError("spline_component_points_input_invalid","point requires a >=3 array"+(" for the Python route" if python_path else " or xyz object")+": "+str(i))
        if any(type(x) not in (int,float) or not math.isfinite(x) for x in xyz):
            raise OperationError("spline_component_points_input_invalid","Point first3 coordinates must be finite exact numbers, not bool: "+str(i))
    api._carrier(points)


def _arguments(points,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,native_first,*,_legacy_command_params=None):
    values=dict(zip(PUBLIC_FIELDS,(points,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,native_first)))
    for field in TEXT_FIELDS:
        if type(values[field]) is not str:raise OperationError("spline_component_points_input_invalid",field+" must be a string")
    for field in BOOL_FIELDS:
        if field=="native_first" and _legacy_command_params is not None:continue
        if type(values[field]) is not bool:raise OperationError("spline_component_points_input_invalid",field+" must be a boolean")
    context=api._private_context(_legacy_command_params)
    if context is not None:api._carrier(context)
    _validate_points(points,python_path=context is None and native_first is False)
    return copy.deepcopy(values),context


def _context(result,args,legacy):
    result["data"].update(operation=METHOD,requested_arguments=copy.deepcopy(args),legacy_command_params=copy.deepcopy(legacy),
        legacy_command_keys=[] if legacy is None else list(legacy),raw_Native_entry=legacy is not None,
        native_private_context=copy.deepcopy(legacy),public_input_count=10,Native_input_count=10,
        Native_forwarded_public_input_count=9,native_first_is_not_Native_argument=True,
        raw_entry_ignores_native_first=legacy is not None,Named_dependency_literal_guard_present=False,
        public_Batch_recursive_value_guard_owned_by_Batch=True,
        None_private_context_is_Named_default9_empty_FString=True,dict_private_context_preserves_raw_presence10_only=True,
        native_and_Python_selection_precision_flags_results_are_distinct=True,
        additional_finite_exact_number_and_array_shape_admission=True,
        direct_Engine_API_dependency=True,server_RPC_dependency=False,external_execute_python=False,result_side_channel=False,
        actual_named_wire_verified=False,disk_reload_verified=False,unique_external_pipeline_complete=False,
        packaged_runtime_verified=False,automatic_action_retry=False,atomic_rollback=False)


def _has_unknown_marker(value):
    """Outcome guard only, not transport processing or dispatch."""
    todo=[value];seen=set()
    while todo:
        row=todo.pop()
        if id(row) in seen:continue
        seen.add(id(row))
        if len(seen)>512:return True
        if type(row) is dict:
            if row.get("mutation_state_unknown") is True or row.get("command_may_still_be_in_flight") is True:return True
            if str(row.get("command_outcome","")).strip().lower()=="unknown":return True
            todo.extend(row.values())
        elif type(row) is list:todo.extend(row)
    return False


def _confirmed_unknown_command(native):
    response=native.get("data",{}).get("legacy_result")
    if (native.get("mutation_attempted") is not False or native.get("change_state_known") is not True or native.get("changed") is not False or _has_unknown_marker(native)):
        return False
    if type(response) is not dict:return False
    error=str(response.get("error") or response.get("message") or "")
    return response.get("status")=="error" and "Unknown command" in error and METHOD in error


def _adopt(result,args,legacy,receipt,path):
    known=receipt["change_state_known"]
    result["data"].update(copy.deepcopy(receipt["data"]))
    result["data"].update(execution_path=path,execution_result_returned=True,
        execution_result_data=copy.deepcopy(receipt["data"]),execution_outcome={key:copy.deepcopy(value) for key,value in receipt.items() if key not in ("data","_raw_native_json")},
        native_result_returned=path=="native",native_raw_result=receipt.get("_raw_native_json") if path=="native" else None,
        native_result_data=copy.deepcopy(receipt["data"]) if path=="native" else None,
        native_mutation_attempted=receipt["mutation_attempted"] if path=="native" else False,execution_mutation_attempted=receipt["mutation_attempted"],mutation_attempted=receipt["mutation_attempted"],
        change_state_known=known,mutation_state_unknown=not known,mutation_performed=receipt["changed"] if known else None,
        observed_state_changed=receipt["changed"] if known else None)
    _context(result,args,legacy)
    result["compile"].update(requested=receipt["compile_requested"],attempted=receipt["compile_attempted"],compiled=receipt["compiled"],status="spline_explicit_compiler_receipt")
    result["save"].update(requested=receipt["save_requested"],attempted=receipt["save_attempted"],saved=receipt["saved"],status="spline_explicit_save_receipt")
    no_work=known and receipt["changed"] is False and not receipt["mutation_attempted"]
    result["changes"]=[] if no_work else [dict(kind=METHOD,execution_path=path,requested_actor_selectors={k:args[k] for k in TEXT_FIELDS},
        attempted=receipt["mutation_attempted"],change_state_known=known,changed=receipt["changed"] if known else None,postcondition_passed=receipt["postcondition_passed"])]
    result["validation"].update(status="spline_points_public_postcondition",postcondition_passed=receipt["postcondition_passed"],
        total_change_state_known=known,full_actual_partial_prefix_preserved=True,implicit_SDK_effects_fully_observed=False,
        disk_reload="not_run",unique_external_pipeline="not_run")
    result["retry"].update(automatic_retry=False,duplicate_risk="unknown_spline_or_callback_effects_inspect_prefix_no_replay" if not known else "observed_changes_inspect_before_new_call" if receipt["mutation_attempted"] else "none_observed")
    result["stage"]="returned_spline_component_points_operation"


def _legacy_named_native_marker(receipt):
    response=receipt.get("data",{}).get("legacy_result")
    if type(response) is not dict:return None
    payload=response.get("result",response)
    nested=response.get("result")
    failed=response.get("status")=="error" or response.get("success") is False or type(nested) is dict and nested.get("success") is False
    succeeded=not failed and bool(response.get("status")=="success" or response.get("success") is True or type(nested) is dict and nested.get("success") is True)
    if succeeded:
        out=copy.deepcopy(payload) if type(payload) is dict else {"result":copy.deepcopy(payload)}
        out.setdefault("success",True);out.update(native_command_used=True,native_command=METHOD,native_response=copy.deepcopy(response));return out
    return dict(success=False,error="Native spline point command failed before Python fallback",error_code="native_spline_point_command_failed",
        native_command_used=False,native_command=METHOD,native_response=copy.deepcopy(response))


def _run_action(result,args,legacy,checkpoint_callback=None):
    callback=checkpoint_callback or state.checkpoint
    _context(result,args,legacy)
    raw_entry=legacy is not None
    use_native=raw_entry or args["native_first"] is True
    result["data"].update(native_dispatched=False,native_result_returned=False,execution_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,
        mutation_performed=False,change_state_known=True,mutation_state_unknown=False)
    result["compile"].update(requested=False,attempted=False,compiled=None,status="not_dispatched")
    result["save"].update(requested=False,attempted=False,saved=False,status="not_dispatched")
    result["stage"]="before_spline_component_points_checkpoint";callback(result)
    def dispatch(path):
        result["data"].update(execution_path=path,native_dispatched=path=="native",native_mutation_attempted=None if path=="native" else False,execution_mutation_attempted=None,
            mutation_performed=None,change_state_known=False,mutation_state_unknown=True)
        result["changes"]=[dict(kind=METHOD,execution_path=path,attempted=None,changed=None,change_state_known=False)]
        result["stage"]="spline_component_points_"+path+"_dispatch"
        if path=="editor_python" and fallback is not None:
            # Persist a new unknown intent before the second route can modify the spline.
            result["stage"]="before_spline_component_points_checkpoint";callback(result)
            result["stage"]="spline_component_points_editor_python_dispatch"
        try:
            if path=="native":return api.set_spline_component_points(*(args[f] for f in PUBLIC_FIELDS[:-1]),_legacy_command_params=legacy)
            return api.set_spline_component_points_python(*(args[f] for f in PUBLIC_FIELDS[:-1]))
        except Exception as error:
            if getattr(error,"native_call_returned",False):result["data"].update(native_result_returned=True,native_raw_result=getattr(error,"native_raw_result",None))
            if getattr(error,"native_call_dispatched",None) is False:
                result["data"].update(native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,mutation_performed=False,change_state_known=True,mutation_state_unknown=False);result["changes"]=[]
            result["stage"]="returned_spline_component_points_operation";callback(result);raise
    path="native" if use_native else "editor_python"
    fallback=None
    receipt=dispatch(path)
    if use_native:
        named_marker=_legacy_named_native_marker(receipt) if not raw_entry else None
        # The old raw command never routes through Named fallback, even native_first=False.
        if not raw_entry and not receipt["success"] and _confirmed_unknown_command(receipt):
            _adopt(result,args,legacy,receipt,"native")
            result["data"]["confirmed_unknown_command_fallback_pending"]=True;callback(result)
            fallback=copy.deepcopy(receipt)
            result["data"].update(prior_native_receipt=copy.deepcopy(fallback),native_attempt_preceded_fallback=True)
            # Preserve original Python row semantics; malformed Python rows cannot start a writer.
            _validate_points(args["points"],python_path=True)
            path="editor_python";receipt=dispatch(path)
        elif named_marker is not None:receipt["data"]["legacy_Named_result"]=named_marker
    _adopt(result,args,legacy,receipt,path)
    if path=="editor_python":
        result["data"]["fallback_trigger"]= "confirmed_unknown_command" if fallback is not None else "explicit_native_first_false"
        result["data"]["prior_native_receipt"]=fallback
        legacy_result=result["data"].get("legacy_result")
        if type(legacy_result) is dict:
            legacy_result["native_response"]=copy.deepcopy(fallback.get("data",{}).get("legacy_result")) if fallback is not None else None
            result["data"]["legacy_Named_result"]=copy.deepcopy(legacy_result)
    result["data"]["fallback_performed"]=fallback is not None
    callback(result)
    if not receipt["success"]:raise OperationError(receipt.get("error_code") or "spline_component_points_failed",receipt.get("error") or "Spline execution failed; actual prefix retained")
    if not receipt["postcondition_passed"]:raise OperationError("spline_component_points_postcondition_unverified","Spline postcondition did not pass; no replay")
    result.update(success=True,stage="complete");return result


def _adopt_rejection(result,error):
    state.fail(result,error);result["stage"]="spline_component_points_input_validation";result["changes"]=[]
    result["compile"].update(requested=False,attempted=False,compiled=None,status="not_dispatched")
    result["save"].update(requested=False,attempted=False,saved=False,status="not_dispatched")
    result["data"].update(operation=METHOD,admission_rejected=True,native_dispatched=False,native_result_returned=False,native_mutation_attempted=False,execution_mutation_attempted=False,
        mutation_performed=False,change_state_known=True,mutation_state_unknown=False,journal_created=False)
    return result


def _rejected(error):return _adopt_rejection(state.new_result(),error)


def set_spline_component_points(points:list,actor_label:str="",actor_name:str="",actor_tag:str="",actor_label_prefix:str="",component_name:str="Road_SourceSpline",component_tag:str="",selected_only:bool=False,closed_loop:bool=False,native_first:bool=True)->Dict[str,Any]:
    try:args,legacy=_arguments(points,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,native_first)
    except Exception as error:return _rejected(error)
    return state.mutate(METHOD,args,None,lambda result:_run_action(result,args,legacy))


def set_spline_component_points_action(result,points:list,actor_label:str="",actor_name:str="",actor_tag:str="",actor_label_prefix:str="",component_name:str="Road_SourceSpline",component_tag:str="",selected_only:bool=False,closed_loop:bool=False,native_first:bool=True,checkpoint_callback=None,*,_legacy_command_params=None):
    try:args,legacy=_arguments(points,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,native_first,_legacy_command_params=_legacy_command_params)
    except Exception as error:
        _adopt_rejection(result,error);result["data"]["parent_journal_only"]=True;(checkpoint_callback or state.checkpoint)(result);raise
    return _run_action(result,args,legacy,checkpoint_callback)
