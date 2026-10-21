"""UE5.8.3 spline points: direct Native leaf and original in-editor Python alternative.

No server RPC, external execution, files, sockets or retries are used here.
"""
import copy
import json
import math
import unreal
from .functions import _decode_result
from .blueprint import ensure_editor_idle
from ...operations.state import OperationError

LIBRARY = "CubelessSplineComponentPointsLibrary"
METHOD = "set_spline_component_points"
BINDINGS = (METHOD,)
LEGACY_COMMAND_FIELDS = ("actor_path", "allow_multiple_actor_matches", "coordinate_space", "point_type",
    "input_to_construction_script", "rerun_construction_scripts", "mark_dirty", "strict_tolerance", "component_name", "closed_loop")
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _private_context(legacy):
    if legacy is None:
        return None
    if type(legacy) is not dict or any(key not in LEGACY_COMMAND_FIELDS for key in legacy):
        raise OperationError("spline_component_points_input_invalid", "Only confirmed hidden8 plus component_name/closed_loop raw presence are allowed")
    return copy.deepcopy(legacy)


def _carrier(value):
    try:
        text=json.dumps(value,ensure_ascii=False,allow_nan=False,separators=(",",":"))
        units=len(text.encode("utf-16-le"))//2
    except (TypeError,ValueError,UnicodeError,OverflowError,RecursionError) as error:
        raise OperationError("spline_component_points_carrier_invalid", "Finite valid Unicode JSON is required") from error
    if units>MAX_CARRIER_UTF16_UNITS or "\x00" in text:
        raise OperationError("spline_component_points_carrier_bound", "Each carrier must fit1Mi UTF16 units with no raw NUL")
    return text


def set_spline_component_points(points, actor_label="", actor_name="", actor_tag="", actor_label_prefix="", component_name="Road_SourceSpline", component_tag="", selected_only=False, closed_loop=False, *, _legacy_command_params=None):
    try:
        nodes_json=_carrier(points);context=_private_context(_legacy_command_params)
        context_json="" if context is None else _carrier(context)
    except Exception as error:
        error.native_call_dispatched=False
        raise
    fn=getattr(getattr(unreal,LIBRARY,None),METHOD,None)
    if not callable(fn):
        error=OperationError("spline_component_points_native_api_unavailable", "Build/load the exact Native API; missing binding is not a confirmed Unknown command response")
        error.native_call_dispatched=False
        raise error
    raw=fn(nodes_json,actor_label,actor_name,actor_tag,actor_label_prefix,component_name,component_tag,selected_only,closed_loop,context_json)
    try:
        value=_decode_result(raw)
        if value.get("operation")!=METHOD or "changed" not in value or value["changed"] is not None and type(value["changed"]) is not bool:
            raise OperationError("spline_component_points_native_result_invalid", "Exact operation and change receipt are required")
    except Exception as error:
        error.native_call_returned=True;error.native_raw_result=raw
        raise
    value["_raw_native_json"]=raw
    return value


def _dirty_packages():
    return sorted({p.get_path_name() for p in list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages())+list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())})


def _spline_snapshot(actor,spline):
    points=[];types=[]
    for index in range(int(spline.get_number_of_spline_points())):
        p=spline.get_location_at_spline_point(index,unreal.SplineCoordinateSpace.WORLD)
        points.append([float(p.x),float(p.y),float(p.z)])
        types.append(str(spline.get_spline_point_type(index)))
    return dict(actor=actor.get_actor_label(),actor_path=actor.get_path_name(),component_name=spline.get_name(),component_path=spline.get_path_name(),
        point_count=len(points),points=points,point_types=types,closed_loop=bool(spline.is_closed_loop()),spline_length=float(spline.get_spline_length()),dirty_packages=_dirty_packages())


def set_spline_component_points_python(points, actor_label="", actor_name="", actor_tag="", actor_label_prefix="", component_name="Road_SourceSpline", component_tag="", selected_only=False, closed_loop=False):
    """Execute the old Python alternative directly; preserve its distinct semantics.

    This path uses World/Linear, rejects multiple actors, excludes TRASH_ splines,
    compares tag strings case-sensitively and does not set the edited override.
    Legacy swallowed setting exceptions remain visible, and an independent
    count/type/closed/finite/delta postcondition cannot be replaced by legacy success.
    """
    report=dict(operation=METHOD,success=False,stage="python_spline_preconditions",error_code="",error="",
        mutation_attempted=False,change_state_known=True,changed=False,postcondition_passed=False,
        compile_requested=False,compile_attempted=False,compiled=None,save_requested=False,save_attempted=False,saved=False,
        data=dict(execution_path="editor_python",legacy_result=None,completed_stages=[],python_call_counts={},warnings=[],
            explicit_compile_calls=0,explicit_save_calls=0,automatic_retry=False,atomic_rollback=False,
            source_path="Python/tools/pcg_tools.py:742-852",SDK_callback_effects_fully_observed=False))
    data=report["data"]
    def stage(name,fn,*args,mutation=False):
        report["stage"]=name
        if mutation:
            report.update(mutation_attempted=True,change_state_known=False,changed=None)
        data["python_call_counts"][name]=data["python_call_counts"].get(name,0)+1
        value=fn(*args);data["completed_stages"].append(name);return value
    try:
        ensure_editor_idle()
        editor=unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem)
        world=editor.get_editor_world()
        if world is None:raise OperationError("spline_python_editor_world_unavailable","No editor world is available")
        # Actor enumeration and snapshots can trigger engine callbacks; no total SDK purity claim.
        report.update(change_state_known=False,changed=None)
        subsystem=unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
        actors=stage("python_actor_selection",subsystem.get_selected_level_actors if selected_only else subsystem.get_all_level_actors)
        def tags(obj,field):
            try:return [str(x) for x in obj.get_editor_property(field)]
            except Exception as error:
                data["warnings"].append(dict(stage="read_"+field,error=str(error),legacy_behavior="empty_tags"));return []
        def actor_matches(actor):
            label=actor.get_actor_label();name=actor.get_name()
            return (not actor_label or label.lower()==actor_label.lower()) and (not actor_name or name.lower()==actor_name.lower()) and (not actor_tag or actor_tag in tags(actor,"tags")) and (not actor_label_prefix or label.lower().startswith(actor_label_prefix.lower()))
        matches=[actor for actor in actors if actor_matches(actor)]
        data["actor_matches"]=[dict(label=a.get_actor_label(),path=a.get_path_name()) for a in matches]
        if not matches:raise OperationError("spline_python_actor_not_found","No matching actor found for spline point update")
        if len(matches)>1:raise OperationError("spline_python_actor_ambiguous","Multiple matching actors found: "+str([a.get_actor_label() for a in matches]))
        actor=matches[0]
        if actor.get_world()!=world:raise OperationError("spline_python_actor_world_mismatch","Selected actor must belong to the current editor world")
        splines=[s for s in actor.get_components_by_class(unreal.SplineComponent) if not s.get_name().startswith("TRASH_")]
        if component_name:splines=[s for s in splines if s.get_name().lower()==component_name.lower()]
        if component_tag:splines=[s for s in splines if component_tag in tags(s,"component_tags")]
        if not splines:raise OperationError("spline_python_component_not_found","No matching SplineComponent found on actor: "+actor.get_actor_label())
        spline=splines[0];data["before"]=stage("python_snapshot_before",_spline_snapshot,actor,spline)
        # Check again immediately before the first writer; retain the actual selected identities.
        ensure_editor_idle()
        if actor.get_world()!=world or spline not in actor.get_components_by_class(unreal.SplineComponent):
            raise OperationError("spline_python_selection_changed","Actor/component identity changed before modification")
        stage("python_actor_modify",actor.modify,mutation=True)
        stage("python_component_modify",spline.modify,mutation=True)
        try:stage("python_input_to_construction_script",spline.set_editor_property,"input_spline_points_to_construction_script",True,mutation=True)
        except Exception as error:data["warnings"].append(dict(stage="input_to_construction_script",error=str(error),legacy_behavior="swallowed"))
        stage("python_clear_points",spline.clear_spline_points,False,mutation=True)
        for point in points:stage("python_add_point",spline.add_spline_point,unreal.Vector(float(point[0]),float(point[1]),float(point[2])),unreal.SplineCoordinateSpace.WORLD,False,mutation=True)
        for index in range(len(points)):stage("python_linear_type",spline.set_spline_point_type,index,unreal.SplinePointType.LINEAR,False,mutation=True)
        try:stage("python_closed_loop",spline.set_closed_loop,closed_loop,False,mutation=True)
        except Exception as error:data["warnings"].append(dict(stage="closed_loop",error=str(error),legacy_behavior="swallowed"))
        stage("python_update_spline",spline.update_spline,mutation=True)
        after=stage("python_snapshot_after",_spline_snapshot,actor,spline);data["after"]=after
        final=after["points"];deltas=[math.dist(row,[float(x) for x in points[i][:3]]) for i,row in enumerate(final) if i<len(points)]
        maximum=max(deltas) if deltas else 0.0
        legacy=dict(success=True,native_command_used=False,native_command=METHOD,native_response=None,fallback_reason="native unavailable",
            actor=after["actor"],actor_path=after["actor_path"],component_name=after["component_name"],component_path=after["component_path"],
            requested_point_count=len(points),final_point_count=after["point_count"],spline_length=after["spline_length"],max_point_delta_cm=maximum,points=final)
        data["legacy_result"]=legacy
        passed=(len(final)==len(points) and len(deltas)==len(points) and math.isfinite(maximum) and maximum<=5.0 and
            all(math.isfinite(x) for row in final for x in row) and all(v==str(unreal.SplinePointType.LINEAR) for v in after["point_types"]) and after["closed_loop"] is closed_loop)
        data["postcondition"]=dict(passed=passed,point_count_equal=len(final)==len(points),max_point_delta_cm=maximum,tolerance_cm=5.0,
            closed_loop_equal=after["closed_loop"] is closed_loop,linear_types=all(v==str(unreal.SplinePointType.LINEAR) for v in after["point_types"]),
            scope="Public World points/count/types/closed state only; callbacks, transactions and total SDK effects remain unknown")
        report.update(success=passed,stage="complete" if passed else "python_spline_postcondition",postcondition_passed=passed)
        if not passed:report.update(error_code="spline_python_postcondition_failed",error="Original Python success retained, but public spline postcondition did not pass")
    except Exception as error:
        failed=report["stage"];data["failed_stage"]=failed
        report.update(success=False,error_code=getattr(error,"code",None) or "spline_python_execution_failed",error=str(error))
        data["legacy_exception"]=dict(type=type(error).__name__,message=str(error))
        # Only observe already selected identities; do not select or re-run a writer after failure.
        if report["mutation_attempted"] and "actor" in locals() and "spline" in locals():
            try:data["after_failure"]=_spline_snapshot(actor,spline)
            except Exception as observe_error:data["after_failure_error"]=str(observe_error)
        report["stage"]=failed
    return report
