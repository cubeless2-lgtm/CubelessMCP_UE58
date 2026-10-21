"""Named asynchronous admission; one legacy report, no public job protocol."""
import copy
import math
from pathlib import Path
import re
import uuid
import unreal
from . import state
from .ai_live_journal import LiveJournal
from .ai_live_runtime import Workflow, DUMMY, BT, FORBIDDEN
from ..compatibility.ue58 import ai_live as adapter

_LIVE = {}  # Strong domain/result closures through actual terminal delivery.
_QUARANTINED = {}
_LABEL = re.compile(r"^MCP_[A-Za-z0-9][A-Za-z0-9_-]{3,59}$")


def _admit(name, arguments):
    if name not in (DUMMY, BT):
        raise state.OperationError("ai_live_operation_invalid", "Only the two original live smokes are admitted")
    config = copy.deepcopy(arguments)
    for key in ("pawn_asset_path", "skeletal_mesh_path", "anim_blueprint_path", "behavior_tree_path", "blackboard_path"):
        if key not in config:
            continue
        value = config[key]
        if not isinstance(value, str) or len(value) > 512 or any(token in value.lower() for token in FORBIDDEN):
            raise state.OperationError("mcp_dependency_reference_rejected", "Invalid or forbidden fixture reference: " + key)
        if value and (not value.startswith(("/Game/", "/Engine/")) or ".." in value or "\\" in value or ":" in value):
            raise state.OperationError("ai_live_asset_path_invalid", "Fixture paths are bounded Game/Engine object/package paths")
    if not isinstance(config["label_prefix"], str) or not _LABEL.fullmatch(config["label_prefix"]):
        raise state.OperationError("invalid_ai_smoke_label_prefix", "label_prefix requires the original dedicated MCP_ namespace")
    for key in ("setup_wait_seconds", "patrol_wait_seconds", "chase_wait_seconds", "bt_wait_seconds", "sight_radius", "acceptance_radius"):
        if key not in config:
            continue
        value = config[key]
        if type(value) not in (int, float) or not math.isfinite(value):
            raise state.OperationError("ai_live_numeric_input_invalid", "Finite numeric input excluding bool required: " + key)
        if key.endswith("wait_seconds"):
            # Original sleeps clamp negative waits to zero. The initial bounded
            # async cohort admits up to30s each; longer waits are explicit gaps.
            config[key] = max(0.0, float(value))
            if config[key] > 30:
                raise state.OperationError("ai_live_wait_scope_unsupported", "The finite tick cohort admits waits at most30s")
        else:
            config[key] = float(value)
    for key in ("cleanup_existing", "cleanup_after"):
        if key in config and type(config[key]) is not bool:
            raise state.OperationError("ai_live_boolean_input_invalid", "Strict original bool required: " + key)
    if name == BT:
        config.update(cleanup_existing=True, cleanup_after=True)
        for key in ("target_actor_key", "patrol_location_key"):
            value = config[key]
            if not isinstance(value, str) or not re.fullmatch(r"[A-Za-z_][A-Za-z0-9_]{0,127}", value):
                raise state.OperationError("ai_live_key_invalid", "Finite original Blackboard key name required")
    directory = config["report_dir"]
    if not isinstance(directory, str) or len(directory) > 256 or not directory:
        raise state.OperationError("ai_live_report_dir_invalid", "Bounded relative report directory required")
    relative = Path(directory)
    if relative.is_absolute() or ":" in directory or "\\" in directory or any(part in ("..", ".") for part in relative.parts) or len(relative.parts) > 8:
        raise state.OperationError("ai_live_report_dir_unsupported", "Initial migration retains the key/default but stores only under project Saved/AILiveReports")
    root = Path(unreal.Paths.project_saved_dir()).resolve() / "CubelessMCP_UE58/AILiveReports"
    target = (root / relative).resolve()
    if root.resolve() not in target.parents:
        raise state.OperationError("ai_live_report_dir_unsupported", "Resolved report directory must stay within its Saved root")
    config.update(floor_center=[180.0, 0.0, -55.0], floor_scale=[12.0, 7.0, 0.1], dummy_start=[-350.0, 0.0, 100.0],
        player_far=[1500.0, 0.0, 100.0], player_near=[650.0, 0.0, 100.0], patrol_target=[250.0, 0.0, 100.0])
    config.update({"patrol_pass_distance": 90.0, "required_chase_delta": 250.0} if name == DUMMY else
        {"target_pass_distance": 180.0, "required_bt_chase_delta": 500.0})
    return config, target


def _failure(name, run_id, config, error):
    return {"operation": name, "run_id": run_id, "success": False, "status": "BLOCKED", "workflow_finished": True,
        "config": copy.deepcopy(config), "steps": [], "summary": {}, "evidence_standard": {"world_mode": "not_started"},
        "report_path": "", "reason": str(error)[:1000], "error_code": getattr(error, "code", "ai_live_admission_failed"),
        "mutation_attempted": False, "mutation_performed": False, "outcome_unknown": False,
        "mutation_state_unknown": False, "read_command_outcome_unknown": False,
        "workflow_recovery_required": False, "automatic_retry": False}


def begin(name, arguments):
    # The synchronous registry adapter creates and returns only the typed
    # promise. Actual world phases run on future public Engine ticker callbacks.
    result_object = adapter.create_result()
    run_id = uuid.uuid4().hex
    journal = None
    workflow = None
    try:
        config, directory = _admit(name, arguments)
        if _QUARANTINED:
            raise state.OperationError("ai_live_workflow_quarantined", "Prior session/journal state requires root reconciliation; no new workflow")
        journal = LiveJournal(name, arguments, run_id)
        directory.mkdir(parents=True, exist_ok=True)
        report_path = directory / (name + "_" + run_id + ".json")

        def terminal(report):
            try:
                journal.finish(report, report_path)
            except Exception as error:
                report.update(success=False, status="BLOCKED", workflow_finished=True, workflow_recovery_required=True,
                    reason="Durable live finalization failed; do not replay: " + str(error)[:700],
                    error_code="journal_finalize_failed", report_path=str(report_path))
            if report.get("workflow_recovery_required") or report.get("outcome_unknown"):
                _QUARANTINED[run_id] = {"journal": journal, "domain": workflow, "result": result_object, "report": copy.deepcopy(report)}
            # Journal/public success cannot disagree. Retain the result closure
            # until this normal native completion call returns.
            try:
                adapter.deliver(result_object, report)
            except Exception as error:
                _QUARANTINED[run_id] = {"journal": journal, "domain": workflow, "result": result_object,
                    "report": copy.deepcopy(report), "delivery_error": str(error)}
                adapter.adapter_error(result_object, "Native completion failed after domain termination; inspect journal " + run_id + ": " + str(error))
            finally:
                _LIVE.pop(run_id, None)

        def shutdown(report):
            journal.shutdown(report)
            _QUARANTINED[run_id] = {"journal": journal, "domain": workflow, "result": result_object, "report": copy.deepcopy(report)}
            # No late result/actor/world methods; absent delivery remains unknown.

        workflow = Workflow(name, config, run_id, journal.observe, terminal, shutdown)
        _LIVE[run_id] = {"domain": workflow, "result": result_object, "journal": journal}
        workflow.start()
    except Exception as error:
        report = _failure(name, run_id, arguments, error)
        if workflow is not None:
            try:
                workflow.abandon_start()
            except Exception as close_error:
                report["callback_cleanup_error"] = str(close_error)[:700]
        if journal is not None:
            report.update(workflow_recovery_required=True, outcome_unknown=True,
                reason="Async admission registered an incomplete journal: " + str(error)[:700])
            journal.shutdown(report)
            _QUARANTINED[run_id] = {"journal": journal, "domain": workflow, "result": result_object, "report": copy.deepcopy(report)}
        _LIVE.pop(run_id, None)
        try:
            adapter.deliver(result_object, report)
        except Exception as delivery_error:
            adapter.adapter_error(result_object, "Live admission/completion failed for journal " + run_id + "; no replay: " + str(delivery_error))
    return result_object


def run_ai_dummy_patrol_chase_smoke(pawn_asset_path="/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype",
        skeletal_mesh_path="/Game/Cubeless/Character/Piper/SK_Piper",
        anim_blueprint_path="/Game/Cubeless/Character/Animation/ABP_RPGChracter", label_prefix="MCP_AIDummySmoke",
        report_dir="Docs/Analysis/AIDummySmoke", cleanup_existing=True, cleanup_after=True,
        setup_wait_seconds=1.5, patrol_wait_seconds=2.0, chase_wait_seconds=2.0, sight_radius=900.0, acceptance_radius=55.0):
    return begin(DUMMY, locals())


def run_ai_behavior_tree_execution_smoke(pawn_asset_path="/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype",
        skeletal_mesh_path="/Game/Cubeless/Character/Piper/SK_Piper",
        anim_blueprint_path="/Game/Cubeless/Character/Animation/ABP_RPGChracter", label_prefix="MCP_AIBTExecutionSmoke",
        report_dir="Docs/Analysis/AIDummySmoke", behavior_tree_path="/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
        blackboard_path="/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase", target_actor_key="TargetActor",
        patrol_location_key="PatrolLocation", setup_wait_seconds=1.5, bt_wait_seconds=5.0):
    return begin(BT, locals())
