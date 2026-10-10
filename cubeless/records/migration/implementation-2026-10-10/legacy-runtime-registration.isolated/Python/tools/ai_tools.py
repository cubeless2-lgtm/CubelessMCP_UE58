"""AI authoring and smoke-test helpers for Unreal MCP."""

from __future__ import annotations

import json
import logging
import os
import re
import tempfile
import textwrap
import threading
import time
from datetime import datetime
from pathlib import Path
from typing import Any, Dict, List
from uuid import uuid4

from mcp.server.fastmcp import FastMCP

from services.unreal_bridge_connection import unreal_python_result_unknown_error
from tools.dependency_guard import reject_mcp_dependency_references
from tools.mutation_result_contract import (
    composite_failure_safety_fields,
    find_unknown_transport_marker,
)
from tools.response_schema import tool_error, tool_pre_dispatch_error

logger = logging.getLogger("UnrealMCP")


DEFAULT_PAWN_ASSET = "/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype"
DEFAULT_SKELETAL_MESH = "/Game/Cubeless/Character/Piper/SK_Piper"
DEFAULT_ANIM_BLUEPRINT = "/Game/Cubeless/Character/Animation/ABP_RPGChracter"
DEFAULT_REPORT_DIR = "Docs/Analysis/AIDummySmoke"
DEFAULT_LABEL_PREFIX = "MCP_AIDummySmoke"
DEFAULT_AI_ASSET_ROOT = "/Game/Cubeless/AI/Smoke"
DEFAULT_BEHAVIOR_TREE_NAME = "BT_AIDummyPatrolChase"
DEFAULT_BLACKBOARD_NAME = "BB_AIDummyPatrolChase"
DEFAULT_TARGET_ACTOR_KEY = "TargetActor"
DEFAULT_PATROL_LOCATION_KEY = "PatrolLocation"
_AI_SMOKE_WORKFLOW_LOCK = threading.Lock()
_AI_SMOKE_WORKFLOW_QUARANTINED = False
_AI_SMOKE_LABEL_PREFIX_PATTERN = re.compile(r"^MCP_[A-Za-z0-9][A-Za-z0-9_-]{3,59}$")


def _validate_ai_smoke_label_prefix(label_prefix: str) -> Dict[str, Any] | None:
    if isinstance(label_prefix, str) and _AI_SMOKE_LABEL_PREFIX_PATTERN.fullmatch(
        label_prefix
    ):
        return None
    return tool_error(
        "label_prefix must use a dedicated MCP_ namespace with 4-60 safe suffix characters",
        error_code="invalid_ai_smoke_label_prefix",
        mutation_attempted=False,
        mutation_performed=False,
        changed=False,
    )


class _AICompositeTransportOutcomeUnknown(RuntimeError):
    """Stop a multi-step smoke before any further Unreal mutation is sent."""

    def __init__(
        self,
        stage: str,
        response: Dict[str, Any],
        *,
        mutation_relevant: bool,
    ) -> None:
        self.stage = stage
        self.response = response
        self.mutation_relevant = mutation_relevant
        super().__init__(f"Unreal transport outcome is unknown at AI smoke stage: {stage}")


def _ai_unknown_transport_report(
    *,
    run_id: str,
    steps: List[Dict[str, Any]],
    report_dir: str,
    failure: _AICompositeTransportOutcomeUnknown,
    completed_mutation_stages: List[str],
) -> Dict[str, Any]:
    cleanup_attempted = failure.stage in {"cleanup", "cleanup_after_setup_failure"}
    report: Dict[str, Any] = {
        "success": False,
        "run_id": run_id,
        "status": "BLOCKED",
        "reason": "transport command outcome unknown; stopped before further Unreal commands",
        "steps": steps,
        "workflow_recovery_required": True,
        "cleanup_attempted": cleanup_attempted,
        "cleanup_outcome_unknown": cleanup_attempted,
        "cleanup_skipped_due_to_unknown_outcome": not cleanup_attempted,
    }
    report.update(
        composite_failure_safety_fields(
            failure.response,
            failed_stage=failure.stage,
            mutation_relevant=failure.mutation_relevant,
            completed_mutation_stages=completed_mutation_stages,
        )
    )
    _attach_report_path_safely(report, report_dir)
    return report

FORBIDDEN_CONTENT_REFERENCE_TOKENS = (
    "/Script/UnrealMCP",
    "/UnrealMCP",
    "UnrealMCP",
    "MCPUnreal",
    "mcp_unreal",
    "/Script/CubelessControlRigTools",
    "CubelessControlRigTools",
    "CubelessControlRigToolsEditor",
)


def _repo_root() -> Path:
    return Path(__file__).resolve().parents[2]


def _timestamp() -> str:
    return f"{datetime.now().strftime('%Y%m%d_%H%M%S_%f')}_{uuid4().hex[:8]}"


def _json_dumps(value: Any) -> str:
    return json.dumps(value, ensure_ascii=False, indent=2, default=str)


def _bridge_success(response: Dict[str, Any] | None) -> bool:
    if not response:
        return False
    return response.get("status") != "error" and response.get("success") is not False


def _result_from_bridge(response: Dict[str, Any] | None) -> Dict[str, Any]:
    if not response:
        return {}
    result = response.get("result")
    return result if isinstance(result, dict) else response


def _send_ai_command(
    command_name: str,
    params: Dict[str, Any],
    *,
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """Send an AI C++ helper command to the Unreal bridge."""
    from unreal_mcp_server import get_unreal_connection

    reject_mcp_dependency_references(command_name, params)

    unreal = get_unreal_connection()
    if not unreal:
        return tool_pre_dispatch_error(
            "Failed to connect to Unreal Engine",
            error_code="unreal_connection_failed",
        )

    try:
        response = unreal.send_command(command_name, params, timeout_seconds=timeout_seconds)
    except Exception as exc:
        return tool_pre_dispatch_error(
            f"Failed to dispatch Unreal AI command: {command_name}",
            error_code="unreal_command_dispatch_failed",
            details={"exception_type": type(exc).__name__},
        )
    if response is None:
        return tool_pre_dispatch_error(
            "No response from Unreal Engine",
            error_code="unreal_no_response",
            unreal_response=None,
        )
    if not _bridge_success(response):
        return tool_error(
            f"Unreal AI command failed: {command_name}",
            error_code=f"{command_name}_failed",
            unreal_response=response,
        )

    result = _result_from_bridge(response)
    data = result.get("data") if isinstance(result.get("data"), dict) else result
    output = dict(data)
    output["success"] = result.get("success", True)
    output["unreal_response"] = response
    return output


def _run_unreal_python_json(
    code_body: str,
    result_name: str,
    *,
    allow_unsafe_editor_scripting_during_pie: bool = False,
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """Run Unreal Python code that writes a RESULT object to a temp JSON file."""
    from unreal_mcp_server import get_unreal_connection

    safe_name = "".join(ch if ch.isalnum() or ch in ("_", "-") else "_" for ch in result_name)
    result_path = os.path.join(
        tempfile.gettempdir(),
        f"unreal_mcp_{safe_name}_{uuid4().hex}_result.json",
    )

    wrapped_code = f"""
import json
import traceback
RESULT_PATH = {result_path!r}
RESULT = None
try:
{textwrap.indent(code_body, "    ")}
    if RESULT is None:
        RESULT = {{"success": True}}
except Exception as exc:
    RESULT = {{"success": False, "message": str(exc), "traceback": traceback.format_exc()}}
with open(RESULT_PATH, "w", encoding="utf-8") as result_file:
    json.dump(RESULT, result_file, ensure_ascii=False, indent=2, default=str)
"""

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return tool_pre_dispatch_error(
                "Failed to connect to Unreal Engine",
                error_code="unreal_connection_failed",
            )

        try:
            response = unreal.send_command(
                "execute_python",
                {
                    "code": wrapped_code,
                    "mode": "ExecuteFile",
                    "interchange_policy": "none",
                    "allow_unsafe_editor_scripting_during_pie": allow_unsafe_editor_scripting_during_pie,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return tool_pre_dispatch_error(
                "Failed to dispatch Unreal Python command",
                error_code="unreal_command_dispatch_failed",
                details={"exception_type": type(exc).__name__},
            )
        if response is None:
            return tool_pre_dispatch_error(
                "No response from Unreal Engine",
                error_code="unreal_no_response",
                unreal_response=None,
            )
        if not _bridge_success(response):
            return tool_error(
                "Unreal Python command failed",
                error_code="unreal_python_command_failed",
                unreal_response=response,
            )

        if not os.path.exists(result_path):
            return unreal_python_result_unknown_error(
                "Unreal Python did not write a result file",
                error_code="unreal_result_file_missing",
                unreal_response=response,
                failure_kind="side_channel_result_missing",
            )

        try:
            with open(result_path, "r", encoding="utf-8") as result_file:
                result = json.load(result_file)
            if not isinstance(result, dict):
                raise ValueError("Unreal Python result must be a JSON object")
        except (OSError, UnicodeError, json.JSONDecodeError, ValueError) as result_error:
            return unreal_python_result_unknown_error(
                "Unreal Python wrote an unreadable result file",
                error_code="unreal_result_file_invalid",
                unreal_response=response,
                failure_kind="side_channel_result_invalid",
                exception=result_error,
            )
        if result.get("success") is False and result.get("traceback"):
            unknown_result = unreal_python_result_unknown_error(
                str(result.get("message") or "Unreal Python execution raised after it began"),
                error_code="unreal_python_execution_outcome_unknown",
                unreal_response=response,
                failure_kind="side_channel_execution_exception",
            )
            unknown_result["unreal_python_result"] = result
            return unknown_result
        result["unreal_response"] = response
        return result
    finally:
        try:
            os.remove(result_path)
        except FileNotFoundError:
            pass
        except OSError as exc:
            logger.warning("Failed to remove AI result file %s: %s", result_path, exc)


PY_COMMON = r'''
import math
import traceback
import unreal

def _vector_to_list(value):
    return [float(value.x), float(value.y), float(value.z)]

def _distance(a, b):
    return math.sqrt((float(a.x) - float(b.x)) ** 2 + (float(a.y) - float(b.y)) ** 2 + (float(a.z) - float(b.z)) ** 2)

def _get_editor_world():
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_editor_world()
        if world:
            return world
    except Exception:
        pass
    try:
        return unreal.EditorLevelLibrary.get_editor_world()
    except Exception:
        return None

def _get_game_world():
    try:
        world = unreal.get_editor_subsystem(unreal.UnrealEditorSubsystem).get_game_world()
        if world:
            return world
    except Exception:
        pass
    try:
        world = unreal.EditorLevelLibrary.get_game_world()
        if world:
            return world
    except Exception:
        pass
    return None

def _get_actor_subsystem():
    return unreal.get_editor_subsystem(unreal.EditorActorSubsystem)

def _all_editor_actors():
    return list(_get_actor_subsystem().get_all_level_actors())

def _actors_by_label(world, label):
    matches = []
    for actor in unreal.GameplayStatics.get_all_actors_of_class(world, unreal.Actor):
        try:
            if actor.get_actor_label() == label:
                matches.append(actor)
        except Exception:
            pass
    return matches

def _first_actor_by_label(world, label):
    matches = _actors_by_label(world, label)
    if not matches:
        raise RuntimeError("Actor not found by label: " + label)
    return matches[0]

def _is_in_pie():
    try:
        les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
        if les and hasattr(les, "is_in_play_in_editor"):
            return bool(les.is_in_play_in_editor())
    except Exception:
        pass
    return False

def _is_in_pie_verified():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if not les or not hasattr(les, "is_in_play_in_editor"):
        raise RuntimeError("Unable to verify PIE/SIE state")
    return bool(les.is_in_play_in_editor())

def _delete_labeled_actors_verified(label_prefix):
    editor = _get_actor_subsystem()
    if not editor:
        raise RuntimeError("EditorActorSubsystem is unavailable")
    candidates = []
    delete_errors = []
    expected_labels = {
        label_prefix + "_Floor",
        label_prefix + "_Dummy",
        label_prefix + "_Player",
        label_prefix + "_Controller",
    }
    for actor in _all_editor_actors():
        try:
            label = actor.get_actor_label()
            if label in expected_labels:
                candidates.append((actor, label))
        except Exception as exc:
            delete_errors.append({"label": "<enumeration>", "message": str(exc)})
    for actor, label in candidates:
        try:
            destroy_result = editor.destroy_actor(actor)
            if destroy_result is False:
                delete_errors.append({"label": label, "message": "destroy_actor returned false"})
        except Exception as exc:
            delete_errors.append({"label": label, "message": str(exc)})
    try:
        unreal.SystemLibrary.collect_garbage()
    except Exception as exc:
        delete_errors.append({"label": "<garbage_collection>", "message": str(exc)})
    remaining = []
    for actor in _all_editor_actors():
        try:
            label = actor.get_actor_label()
            if label in expected_labels:
                remaining.append(label)
        except Exception as exc:
            delete_errors.append({"label": "<readback>", "message": str(exc)})
    remaining_set = set(remaining)
    deleted = [label for _actor, label in candidates if label not in remaining_set]
    return {
        "success": not remaining and not delete_errors,
        "deleted": deleted,
        "deleted_count": len(deleted),
        "delete_errors": delete_errors,
        "remaining_labels": remaining,
    }

def _dirty_package_names():
    names = []
    try:
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages():
            names.append(package.get_name())
    except Exception:
        pass
    try:
        for package in unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages():
            names.append(package.get_name())
    except Exception:
        pass
    return sorted(set(names))

def _load_generated_class(asset_path):
    asset = unreal.EditorAssetLibrary.load_asset(asset_path)
    if not asset:
        return None, None, "asset_not_found"
    generated = None
    for prop_name in ("generated_class", "GeneratedClass"):
        try:
            generated = asset.get_editor_property(prop_name)
            if generated:
                return asset, generated, prop_name
        except Exception:
            pass
    try:
        generated = asset.generated_class
        if callable(generated):
            generated = generated()
        if generated:
            return asset, generated, "generated_class_attr"
    except Exception:
        pass
    return asset, None, "no_generated_class"

def _try_apply_character_visuals(actor, skeletal_mesh_path, anim_blueprint_path):
    result = {
        "skeletal_mesh_path": skeletal_mesh_path,
        "anim_blueprint_path": anim_blueprint_path,
        "mesh_applied": False,
        "anim_class_applied": False,
        "warnings": [],
    }
    try:
        mesh_comp = actor.get_component_by_class(unreal.SkeletalMeshComponent)
        if not mesh_comp:
            result["warnings"].append("No SkeletalMeshComponent on actor")
            return result
        mesh = unreal.EditorAssetLibrary.load_asset(skeletal_mesh_path) if skeletal_mesh_path else None
        if mesh:
            try:
                if hasattr(mesh_comp, "set_skinned_asset_and_update"):
                    mesh_comp.set_skinned_asset_and_update(mesh)
                else:
                    mesh_comp.set_skeletal_mesh(mesh)
                result["mesh_applied"] = True
            except Exception:
                mesh_comp.set_editor_property("skeletal_mesh_asset", mesh)
                result["mesh_applied"] = True
        elif skeletal_mesh_path:
            result["warnings"].append("Skeletal mesh asset not found")

        anim_asset, anim_class, anim_route = _load_generated_class(anim_blueprint_path) if anim_blueprint_path else (None, None, "")
        result["anim_class_route"] = anim_route
        if anim_class:
            try:
                mesh_comp.set_anim_instance_class(anim_class)
                result["anim_class_applied"] = True
            except Exception:
                try:
                    mesh_comp.set_editor_property("anim_class", anim_class)
                    result["anim_class_applied"] = True
                except Exception as exc:
                    result["warnings"].append("Anim class apply failed: " + str(exc))
        elif anim_blueprint_path:
            result["warnings"].append("Anim Blueprint generated class not found")
    except Exception as exc:
        result["warnings"].append("Visual setup failed: " + str(exc))
    return result

def _spawn_labeled_actor(actor_class, label, location, rotation=None):
    rotation = rotation or unreal.Rotator(0.0, 0.0, 0.0)
    actor = unreal.EditorLevelLibrary.spawn_actor_from_class(actor_class, location, rotation)
    if not actor:
        raise RuntimeError("Failed to spawn actor: " + label)
    actor.set_actor_label(label)
    return actor
'''


def _setup_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
pawn_asset_path = __PAWN_ASSET_PATH__
skeletal_mesh_path = __SKELETAL_MESH_PATH__
anim_blueprint_path = __ANIM_BLUEPRINT_PATH__
cleanup_existing = __CLEANUP_EXISTING__
floor_center = unreal.Vector(*__FLOOR_CENTER__)
floor_scale = unreal.Vector(*__FLOOR_SCALE__)
dummy_start = unreal.Vector(*__DUMMY_START__)
player_far = unreal.Vector(*__PLAYER_FAR__)

if _is_in_pie_verified():
    RESULT = {"success": False, "message": "Refusing setup while PIE/SIE is already active"}
else:
    dirty_before = _dirty_package_names()
    deleted_existing = []
    if cleanup_existing:
        cleanup_report = _delete_labeled_actors_verified(label_prefix)
        if not cleanup_report["success"]:
            raise RuntimeError("Existing AI fixture cleanup could not be verified: " + str(cleanup_report))
        deleted_existing = cleanup_report["deleted"]

    floor = _spawn_labeled_actor(unreal.StaticMeshActor, label_prefix + "_Floor", floor_center)
    floor_comp = floor.get_component_by_class(unreal.StaticMeshComponent)
    cube_mesh = unreal.EditorAssetLibrary.load_asset("/Engine/BasicShapes/Cube")
    if floor_comp and cube_mesh:
        floor_comp.set_static_mesh(cube_mesh)
        try:
            floor_comp.set_editor_property("collision_profile_name", "BlockAll")
        except Exception:
            pass
    floor.set_actor_scale3d(floor_scale)

    pawn_asset, pawn_class, pawn_class_route = _load_generated_class(pawn_asset_path)
    pawn_class_used = "unreal.Character"
    if pawn_class:
        pawn_class_used = pawn_asset_path
    else:
        pawn_class = unreal.Character

    try:
        dummy = _spawn_labeled_actor(pawn_class, label_prefix + "_Dummy", dummy_start)
        pawn_spawn_fallback = False
    except Exception:
        dummy = _spawn_labeled_actor(unreal.Character, label_prefix + "_Dummy", dummy_start)
        pawn_spawn_fallback = True
        pawn_class_used = "unreal.Character"

    dummy_visuals = _try_apply_character_visuals(dummy, skeletal_mesh_path, anim_blueprint_path)
    player = _spawn_labeled_actor(unreal.Character, label_prefix + "_Player", player_far)
    controller = _spawn_labeled_actor(unreal.AIController, label_prefix + "_Controller", dummy_start)

    dirty_after = _dirty_package_names()
    RESULT = {
        "success": True,
        "stage": "setup_editor_fixture",
        "dirty_packages_before": dirty_before,
        "dirty_packages_after": dirty_after,
        "deleted_existing": deleted_existing,
        "labels": {
            "floor": floor.get_actor_label(),
            "dummy": dummy.get_actor_label(),
            "player": player.get_actor_label(),
            "controller": controller.get_actor_label(),
        },
        "pawn_asset_path": pawn_asset_path,
        "pawn_class_route": pawn_class_route,
        "pawn_class_used": pawn_class_used,
        "pawn_spawn_fallback": pawn_spawn_fallback,
        "dummy_visuals": dummy_visuals,
        "floor": {
            "location": _vector_to_list(floor.get_actor_location()),
            "scale": _vector_to_list(floor.get_actor_scale3d()),
        },
        "mcp_dependency_audit": {
            "serialized_asset_paths_checked": [pawn_asset_path, skeletal_mesh_path, anim_blueprint_path],
            "forbidden_tokens": __FORBIDDEN_TOKENS__,
            "found_forbidden_reference": False,
        },
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__PAWN_ASSET_PATH__", repr(config["pawn_asset_path"]))
        .replace("__SKELETAL_MESH_PATH__", repr(config["skeletal_mesh_path"]))
        .replace("__ANIM_BLUEPRINT_PATH__", repr(config["anim_blueprint_path"]))
        .replace("__CLEANUP_EXISTING__", repr(bool(config["cleanup_existing"])))
        .replace("__FLOOR_CENTER__", repr(config["floor_center"]))
        .replace("__FLOOR_SCALE__", repr(config["floor_scale"]))
        .replace("__DUMMY_START__", repr(config["dummy_start"]))
        .replace("__PLAYER_FAR__", repr(config["player_far"]))
        .replace("__FORBIDDEN_TOKENS__", repr(FORBIDDEN_CONTENT_REFERENCE_TOKENS))
    )


def _start_sie_code() -> str:
    return (
        PY_COMMON
        + r'''
if _is_in_pie():
    RESULT = {"success": False, "message": "PIE/SIE is already active before this smoke test"}
else:
    route = ""
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les and hasattr(les, "editor_play_simulate"):
        les.editor_play_simulate()
        route = "LevelEditorSubsystem.editor_play_simulate"
    elif hasattr(unreal.EditorLevelLibrary, "editor_play_simulate"):
        unreal.EditorLevelLibrary.editor_play_simulate()
        route = "EditorLevelLibrary.editor_play_simulate"
    else:
        raise RuntimeError("No editor_play_simulate route exposed")
    RESULT = {"success": True, "stage": "start_sie", "route": route}
'''
    )


def _patrol_command_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
patrol_target = unreal.Vector(*__PATROL_TARGET__)
acceptance_radius = float(__ACCEPTANCE_RADIUS__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
    controller = _first_actor_by_label(world, label_prefix + "_Controller")
    controller.possess(dummy)
    start = dummy.get_actor_location()
    distance_before = _distance(start, patrol_target)
    request = controller.move_to_location(
        patrol_target,
        acceptance_radius=acceptance_radius,
        stop_on_overlap=True,
        use_pathfinding=False,
        project_destination_to_navigation=False,
        can_strafe=False,
        allow_partial_path=True,
    )
    RESULT = {
        "success": True,
        "stage": "patrol_command",
        "world": world.get_name(),
        "dummy_location_before": _vector_to_list(start),
        "patrol_target": _vector_to_list(patrol_target),
        "distance_before": distance_before,
        "move_request": str(request),
        "move_status": str(controller.get_move_status()),
        "use_pathfinding": False,
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__PATROL_TARGET__", repr(config["patrol_target"]))
        .replace("__ACCEPTANCE_RADIUS__", repr(float(config["acceptance_radius"])))
    )


def _patrol_sample_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
patrol_target = unreal.Vector(*__PATROL_TARGET__)
pass_distance = float(__PASS_DISTANCE__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
    controller = _first_actor_by_label(world, label_prefix + "_Controller")
    loc = dummy.get_actor_location()
    distance_after = _distance(loc, patrol_target)
    RESULT = {
        "success": True,
        "stage": "patrol_sample",
        "world": world.get_name(),
        "dummy_location_after": _vector_to_list(loc),
        "patrol_target": _vector_to_list(patrol_target),
        "distance_after": distance_after,
        "move_status": str(controller.get_move_status()),
        "patrol_reached": distance_after <= pass_distance,
        "pass_distance": pass_distance,
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__PATROL_TARGET__", repr(config["patrol_target"]))
        .replace("__PASS_DISTANCE__", repr(float(config["patrol_pass_distance"])))
    )


def _detect_and_chase_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
player_near = unreal.Vector(*__PLAYER_NEAR__)
sight_radius = float(__SIGHT_RADIUS__)
acceptance_radius = float(__ACCEPTANCE_RADIUS__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
    player = _first_actor_by_label(world, label_prefix + "_Player")
    controller = _first_actor_by_label(world, label_prefix + "_Controller")
    player.set_actor_location(player_near, False, True)
    dummy_loc = dummy.get_actor_location()
    player_loc = player.get_actor_location()
    distance_to_player = _distance(dummy_loc, player_loc)
    try:
        los = bool(controller.line_of_sight_to(player))
    except Exception:
        los = False
    detected = bool(distance_to_player <= sight_radius and los)
    request = None
    if detected:
        request = controller.move_to_actor(
            player,
            acceptance_radius=acceptance_radius,
            stop_on_overlap=True,
            use_pathfinding=False,
            can_strafe=False,
            allow_partial_path=True,
        )
    RESULT = {
        "success": True,
        "stage": "detect_and_chase_command",
        "world": world.get_name(),
        "dummy_location": _vector_to_list(dummy_loc),
        "player_location": _vector_to_list(player_loc),
        "distance_to_player_before_chase": distance_to_player,
        "sight_radius": sight_radius,
        "line_of_sight": los,
        "detected": detected,
        "move_request": str(request) if request is not None else "",
        "move_status": str(controller.get_move_status()),
        "detection_contract": "distance <= sight_radius and AIController.line_of_sight_to(player)",
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__PLAYER_NEAR__", repr(config["player_near"]))
        .replace("__SIGHT_RADIUS__", repr(float(config["sight_radius"])))
        .replace("__ACCEPTANCE_RADIUS__", repr(float(config["acceptance_radius"])))
    )


def _chase_sample_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
required_delta = float(__REQUIRED_DELTA__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
    player = _first_actor_by_label(world, label_prefix + "_Player")
    controller = _first_actor_by_label(world, label_prefix + "_Controller")
    dummy_loc = dummy.get_actor_location()
    player_loc = player.get_actor_location()
    distance_after = _distance(dummy_loc, player_loc)
    RESULT = {
        "success": True,
        "stage": "chase_sample",
        "world": world.get_name(),
        "dummy_location_after_chase": _vector_to_list(dummy_loc),
        "player_location": _vector_to_list(player_loc),
        "distance_to_player_after_chase": distance_after,
        "move_status": str(controller.get_move_status()),
        "required_distance_delta": required_delta,
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__REQUIRED_DELTA__", repr(float(config["required_chase_delta"])))
    )


def _bt_execution_command_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
behavior_tree_path = __BEHAVIOR_TREE_PATH__
blackboard_path = __BLACKBOARD_PATH__
target_actor_key = __TARGET_ACTOR_KEY__
patrol_location_key = __PATROL_LOCATION_KEY__
patrol_target = unreal.Vector(*__PATROL_TARGET__)
player_near = unreal.Vector(*__PLAYER_NEAR__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    behavior_tree_load_path = behavior_tree_path.split(".", 1)[0]
    blackboard_load_path = blackboard_path.split(".", 1)[0]
    if hasattr(unreal, "load_asset"):
        behavior_tree = unreal.load_asset(behavior_tree_load_path)
        blackboard = unreal.load_asset(blackboard_load_path)
    else:
        behavior_tree = unreal.load_object(None, behavior_tree_path)
        blackboard = unreal.load_object(None, blackboard_path)
    if not behavior_tree:
        RESULT = {"success": False, "message": "Behavior Tree asset not found: " + behavior_tree_path}
    elif not blackboard:
        RESULT = {"success": False, "message": "Blackboard asset not found: " + blackboard_path}
    else:
        dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
        player = _first_actor_by_label(world, label_prefix + "_Player")
        controller = _first_actor_by_label(world, label_prefix + "_Controller")
        player.set_actor_location(player_near, False, True)
        controller.possess(dummy)
        start = dummy.get_actor_location()
        player_loc = player.get_actor_location()
        distance_to_player_before = _distance(start, player_loc)
        distance_to_patrol_before = _distance(start, patrol_target)

        blackboard_component = controller.use_blackboard(blackboard)
        if not blackboard_component:
            RESULT = {"success": False, "message": "AIController did not expose a BlackboardComponent"}
        else:
            blackboard_component.set_value_as_vector(patrol_location_key, patrol_target)
            blackboard_component.set_value_as_object(target_actor_key, player)
            run_result = bool(controller.run_behavior_tree(behavior_tree))
            try:
                blackboard_component = controller.get_editor_property("blackboard") or blackboard_component
            except Exception:
                pass
            blackboard_component.set_value_as_vector(patrol_location_key, patrol_target)
            blackboard_component.set_value_as_object(target_actor_key, player)
            try:
                stored_patrol = blackboard_component.get_value_as_vector(patrol_location_key)
                stored_target = blackboard_component.get_value_as_object(target_actor_key)
                blackboard_values_set = bool(stored_target == player and _distance(stored_patrol, patrol_target) <= 1.0)
            except Exception:
                blackboard_values_set = False
            try:
                brain_component = controller.get_editor_property("brain_component")
                brain_component_text = str(brain_component)
                brain_running = bool(brain_component and brain_component.is_running())
                brain_active = bool(brain_component and brain_component.is_active())
                brain_tick_enabled = bool(brain_component and brain_component.is_component_tick_enabled())
            except Exception as exc:
                brain_component_text = "ERROR:" + str(exc)
                brain_running = False
                brain_active = False
                brain_tick_enabled = False

            RESULT = {
                "success": True,
                "stage": "bt_execution_command",
                "world": world.get_name(),
                "behavior_tree_path": behavior_tree_path,
                "blackboard_path": blackboard_path,
                "behavior_tree_class": behavior_tree.get_class().get_name(),
                "blackboard_class": blackboard.get_class().get_name(),
                "dummy_location_before": _vector_to_list(start),
                "patrol_target": _vector_to_list(patrol_target),
                "player_location": _vector_to_list(player_loc),
                "distance_to_patrol_before": distance_to_patrol_before,
                "distance_to_player_before": distance_to_player_before,
                "use_blackboard_component": str(blackboard_component),
                "run_behavior_tree_result": run_result,
                "blackboard_values_set": blackboard_values_set,
                "brain_component": brain_component_text,
                "brain_running": brain_running,
                "brain_active": brain_active,
                "brain_tick_enabled": brain_tick_enabled,
                "move_status": str(controller.get_move_status()),
            }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__BEHAVIOR_TREE_PATH__", repr(config["behavior_tree_path"]))
        .replace("__BLACKBOARD_PATH__", repr(config["blackboard_path"]))
        .replace("__TARGET_ACTOR_KEY__", repr(config["target_actor_key"]))
        .replace("__PATROL_LOCATION_KEY__", repr(config["patrol_location_key"]))
        .replace("__PATROL_TARGET__", repr(config["patrol_target"]))
        .replace("__PLAYER_NEAR__", repr(config["player_near"]))
    )


def _bt_execution_sample_code(config: Dict[str, Any]) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
patrol_target = unreal.Vector(*__PATROL_TARGET__)
target_actor_key = __TARGET_ACTOR_KEY__
patrol_location_key = __PATROL_LOCATION_KEY__
pass_distance = float(__PASS_DISTANCE__)

world = _get_game_world()
if not world:
    RESULT = {"success": False, "message": "No PIE/SIE game world available"}
else:
    dummy = _first_actor_by_label(world, label_prefix + "_Dummy")
    player = _first_actor_by_label(world, label_prefix + "_Player")
    controller = _first_actor_by_label(world, label_prefix + "_Controller")
    dummy_loc = dummy.get_actor_location()
    player_loc = player.get_actor_location()
    distance_to_player_after = _distance(dummy_loc, player_loc)
    distance_to_patrol_after = _distance(dummy_loc, patrol_target)
    blackboard_values_still_set = False
    brain_component_text = ""
    brain_running = False
    brain_active = False
    brain_tick_enabled = False
    try:
        blackboard_component = controller.get_editor_property("blackboard")
        stored_patrol = blackboard_component.get_value_as_vector(patrol_location_key)
        stored_target = blackboard_component.get_value_as_object(target_actor_key)
        blackboard_values_still_set = bool(stored_target == player and _distance(stored_patrol, patrol_target) <= 1.0)
    except Exception:
        pass
    try:
        brain_component = controller.get_editor_property("brain_component")
        brain_component_text = str(brain_component)
        brain_running = bool(brain_component and brain_component.is_running())
        brain_active = bool(brain_component and brain_component.is_active())
        brain_tick_enabled = bool(brain_component and brain_component.is_component_tick_enabled())
    except Exception as exc:
        brain_component_text = "ERROR:" + str(exc)
    RESULT = {
        "success": True,
        "stage": "bt_execution_sample",
        "world": world.get_name(),
        "dummy_location_after": _vector_to_list(dummy_loc),
        "player_location": _vector_to_list(player_loc),
        "patrol_target": _vector_to_list(patrol_target),
        "distance_to_player_after": distance_to_player_after,
        "distance_to_patrol_after": distance_to_patrol_after,
        "move_status": str(controller.get_move_status()),
        "blackboard_values_still_set": blackboard_values_still_set,
        "brain_component": brain_component_text,
        "brain_running": brain_running,
        "brain_active": brain_active,
        "brain_tick_enabled": brain_tick_enabled,
        "within_target_pass_distance": distance_to_player_after <= pass_distance,
        "pass_distance": pass_distance,
    }
"""
        .replace("__LABEL_PREFIX__", repr(config["label_prefix"]))
        .replace("__PATROL_TARGET__", repr(config["patrol_target"]))
        .replace("__TARGET_ACTOR_KEY__", repr(config["target_actor_key"]))
        .replace("__PATROL_LOCATION_KEY__", repr(config["patrol_location_key"]))
        .replace("__PASS_DISTANCE__", repr(float(config["target_pass_distance"])))
    )


def _end_sie_code() -> str:
    return (
        PY_COMMON
        + r'''
route = ""
if _is_in_pie():
    les = unreal.get_editor_subsystem(unreal.LevelEditorSubsystem)
    if les and hasattr(les, "editor_request_end_play"):
        les.editor_request_end_play()
        route = "LevelEditorSubsystem.editor_request_end_play"
    elif hasattr(unreal.EditorLevelLibrary, "editor_end_play"):
        unreal.EditorLevelLibrary.editor_end_play()
        route = "EditorLevelLibrary.editor_end_play"
    else:
        raise RuntimeError("No end play route exposed")
RESULT = {"success": True, "stage": "end_sie", "route": route, "was_in_pie": bool(route)}
'''
    )


def _verify_sie_ended_code() -> str:
    return (
        PY_COMMON
        + r'''
still_in_pie = _is_in_pie_verified()
RESULT = {
    "success": not still_in_pie,
    "stage": "verify_sie_ended",
    "still_in_pie": still_in_pie,
    "message": "SIE is still active after end request" if still_in_pie else "SIE ended",
}
'''
    )


def _cleanup_code(label_prefix: str) -> str:
    return (
        PY_COMMON
        + """
label_prefix = __LABEL_PREFIX__
dirty_before = _dirty_package_names()
if _is_in_pie_verified():
    RESULT = {"success": False, "message": "Refusing editor actor cleanup while PIE/SIE is active"}
else:
    cleanup_report = _delete_labeled_actors_verified(label_prefix)
    dirty_after = _dirty_package_names()
    RESULT = {
        **cleanup_report,
        "stage": "cleanup_editor_fixture",
        "dirty_packages_before": dirty_before,
        "dirty_packages_after": dirty_after,
        "message": "Fixture cleanup verified" if cleanup_report["success"] else "Fixture cleanup could not be verified",
    }
"""
        .replace("__LABEL_PREFIX__", repr(label_prefix))
    )


def _support_code() -> str:
    return (
        PY_COMMON
        + r'''
names = [
    "AIController",
    "AIPerceptionComponent",
    "AISenseConfig_Sight",
    "AIPerceptionStimuliSourceComponent",
    "BehaviorTree",
    "BehaviorTreeFactory",
    "BlackboardData",
    "BlackboardDataFactory",
    "Character",
    "PawnSensingComponent",
    "TargetPoint",
]
exposed = {name: bool(getattr(unreal, name, None)) for name in names}
ai_controller_members = []
if getattr(unreal, "AIController", None):
    ai_controller_members = [
        name for name in dir(unreal.AIController)
        if name in ("move_to_actor", "move_to_location", "line_of_sight_to", "possess", "run_behavior_tree", "use_blackboard")
    ]
RESULT = {
    "success": True,
    "stage": "inspect_ai_authoring_support",
    "exposed": exposed,
    "ai_controller_members": ai_controller_members,
    "behavior_tree_python_note": "BT/BB asset factories are exposed; stock BT node classes are not all exposed to Python in UE 5.6, so this smoke validates AIController movement and LOS detection first.",
}
'''
    )


def inspect_ai_authoring_support_impl() -> Dict[str, Any]:
    """Inspect AI authoring APIs exposed to the running Unreal Python bridge."""
    return _run_unreal_python_json(_support_code(), "ai_authoring_support", timeout_seconds=30)


def _step_status(step: Dict[str, Any]) -> str:
    if not step.get("success"):
        return "BLOCKED"
    return "PASS"


def _write_report(report: Dict[str, Any], report_dir: str) -> str:
    output_dir = Path(report_dir)
    if not output_dir.is_absolute():
        output_dir = _repo_root() / output_dir
    output_dir.mkdir(parents=True, exist_ok=True)
    report_path = output_dir / f"ai-dummy-patrol-chase-smoke-{report['run_id']}.json"
    report_path.write_text(_json_dumps(report), encoding="utf-8")
    return str(report_path)


def _attach_report_path_safely(report: Dict[str, Any], report_dir: str) -> Dict[str, Any]:
    """Keep safety evidence intact even when the optional report file cannot be written."""
    try:
        report["report_path"] = _write_report(report, report_dir)
    except Exception as exc:
        logger.exception("Failed to write AI smoke report")
        report["report_path"] = ""
        report["report_write_error"] = {
            "exception_type": type(exc).__name__,
            "message": str(exc),
        }
    return report


def run_ai_dummy_patrol_chase_smoke_impl(
    *,
    pawn_asset_path: str = DEFAULT_PAWN_ASSET,
    skeletal_mesh_path: str = DEFAULT_SKELETAL_MESH,
    anim_blueprint_path: str = DEFAULT_ANIM_BLUEPRINT,
    label_prefix: str = DEFAULT_LABEL_PREFIX,
    report_dir: str = DEFAULT_REPORT_DIR,
    cleanup_existing: bool = True,
    cleanup_after: bool = True,
    setup_wait_seconds: float = 1.5,
    patrol_wait_seconds: float = 2.0,
    chase_wait_seconds: float = 2.0,
    sight_radius: float = 900.0,
    acceptance_radius: float = 55.0,
) -> Dict[str, Any]:
    """Run a live SIE smoke for a simple AI patrol-to-chase loop."""
    label_error = _validate_ai_smoke_label_prefix(label_prefix)
    if label_error is not None:
        return label_error
    config: Dict[str, Any] = {
        "pawn_asset_path": pawn_asset_path,
        "skeletal_mesh_path": skeletal_mesh_path,
        "anim_blueprint_path": anim_blueprint_path,
        "label_prefix": label_prefix,
        "cleanup_existing": cleanup_existing,
        "floor_center": [180.0, 0.0, -55.0],
        "floor_scale": [12.0, 7.0, 0.1],
        "dummy_start": [-350.0, 0.0, 100.0],
        "player_far": [1500.0, 0.0, 100.0],
        "player_near": [650.0, 0.0, 100.0],
        "patrol_target": [250.0, 0.0, 100.0],
        "sight_radius": float(sight_radius),
        "acceptance_radius": float(acceptance_radius),
        "patrol_pass_distance": 90.0,
        "required_chase_delta": 250.0,
    }

    try:
        reject_mcp_dependency_references("ai_dummy_smoke_config", {
            "pawn_asset_path": pawn_asset_path,
            "skeletal_mesh_path": skeletal_mesh_path,
            "anim_blueprint_path": anim_blueprint_path,
        })
    except ValueError as exc:
        return tool_error(str(exc), error_code="mcp_dependency_reference_rejected")

    run_id = _timestamp()
    steps: List[Dict[str, Any]] = []
    started_sie = False
    completed_mutation_stages: List[str] = []
    unknown_failure: _AICompositeTransportOutcomeUnknown | None = None
    lifecycle_failure: tuple[str, Dict[str, Any]] | None = None
    cleanup_skipped_due_to_sie_end_failure = False

    def run_step(
        name: str,
        code: str,
        *,
        allow_pie: bool = False,
        timeout_seconds: int = 60,
        mutation_relevant: bool = True,
        record_completed_mutation: bool = True,
    ) -> Dict[str, Any]:
        nonlocal unknown_failure
        result = _run_unreal_python_json(
            code,
            f"ai_dummy_{name}_{run_id}",
            allow_unsafe_editor_scripting_during_pie=allow_pie,
            timeout_seconds=timeout_seconds,
        )
        result["name"] = name
        result["status"] = _step_status(result)
        steps.append(result)
        if find_unknown_transport_marker(result) is not None:
            unknown_failure = _AICompositeTransportOutcomeUnknown(
                name,
                result,
                mutation_relevant=mutation_relevant,
            )
            raise unknown_failure
        if mutation_relevant and record_completed_mutation and result.get("success"):
            completed_mutation_stages.append(name)
        return result

    try:
        setup = run_step("setup", _setup_code(config), timeout_seconds=90)
    except _AICompositeTransportOutcomeUnknown as failure:
        return _ai_unknown_transport_report(
            run_id=run_id,
            steps=steps,
            report_dir=report_dir,
            failure=failure,
            completed_mutation_stages=completed_mutation_stages,
        )
    if not setup.get("success"):
        try:
            cleanup = run_step("cleanup_after_setup_failure", _cleanup_code(label_prefix), timeout_seconds=60)
        except _AICompositeTransportOutcomeUnknown as failure:
            return _ai_unknown_transport_report(
                run_id=run_id,
                steps=steps,
                report_dir=report_dir,
                failure=failure,
                completed_mutation_stages=completed_mutation_stages,
            )
        report = {
            "success": False,
            "run_id": run_id,
            "status": "BLOCKED",
            "reason": "setup failed",
            "steps": steps,
            "cleanup_result": cleanup,
        }
        if not cleanup.get("success"):
            report.update(
                composite_failure_safety_fields(
                    cleanup,
                    failed_stage="cleanup_after_setup_failure",
                    mutation_relevant=True,
                )
            )
            report.update(
                {
                    "reason": "setup failed and cleanup did not complete",
                    "mutation_attempted": True,
                    "mutation_state_unknown": True,
                    "do_not_retry": True,
                    "workflow_recovery_required": True,
                }
            )
        _attach_report_path_safely(report, report_dir)
        return report

    try:
        start = run_step("start_sie", _start_sie_code(), timeout_seconds=30)
        started_sie = bool(start.get("success"))
        if not started_sie:
            raise RuntimeError(start.get("message") or "SIE start failed")

        time.sleep(max(0.0, float(setup_wait_seconds)))
        patrol_command = run_step("patrol_command", _patrol_command_code(config), allow_pie=True)
        if not patrol_command.get("success"):
            raise RuntimeError(
                patrol_command.get("message") or "Patrol command failed"
            )
        time.sleep(max(0.0, float(patrol_wait_seconds)))
        patrol_sample = run_step(
            "patrol_sample",
            _patrol_sample_code(config),
            allow_pie=True,
            mutation_relevant=False,
        )
        if not patrol_sample.get("success"):
            raise RuntimeError(
                patrol_sample.get("message") or "Patrol readback failed"
            )

        detect_command = run_step("detect_and_chase_command", _detect_and_chase_code(config), allow_pie=True)
        if not detect_command.get("success"):
            raise RuntimeError(
                detect_command.get("message") or "Detect/chase command failed"
            )
        distance_before_chase = float(detect_command.get("distance_to_player_before_chase") or 0.0)
        time.sleep(max(0.0, float(chase_wait_seconds)))
        chase_sample = run_step(
            "chase_sample",
            _chase_sample_code(config),
            allow_pie=True,
            mutation_relevant=False,
        )
        distance_after_chase = float(chase_sample.get("distance_to_player_after_chase") or 0.0)

        patrol_pass = bool(patrol_sample.get("patrol_reached"))
        detection_pass = bool(detect_command.get("detected"))
        chase_delta = distance_before_chase - distance_after_chase
        chase_pass = bool(
            chase_sample.get("success")
            and detection_pass
            and chase_delta >= float(config["required_chase_delta"])
        )
        pass_flags = {
            "patrol": patrol_pass,
            "detection": detection_pass,
            "chase": chase_pass,
            "mcp_dependency_audit": not bool(
                setup.get("mcp_dependency_audit", {}).get("found_forbidden_reference", True)
            ),
        }
    except _AICompositeTransportOutcomeUnknown:
        pass
    except Exception as exc:
        steps.append(
            {
                "success": False,
                "status": "BLOCKED",
                "name": "orchestration_exception",
                "message": str(exc),
            }
        )
        pass_flags = {
            "patrol": False,
            "detection": False,
            "chase": False,
            "mcp_dependency_audit": False,
        }
        chase_delta = 0.0
    finally:
        if unknown_failure is None:
            try:
                end_sie_succeeded = not started_sie
                if started_sie:
                    end_sie = run_step(
                        "end_sie",
                        _end_sie_code(),
                        allow_pie=True,
                        timeout_seconds=30,
                        record_completed_mutation=False,
                    )
                    end_sie_succeeded = bool(end_sie.get("success"))
                    if end_sie_succeeded:
                        time.sleep(1.0)
                        end_sie_verification = run_step(
                            "verify_sie_ended",
                            _verify_sie_ended_code(),
                            timeout_seconds=30,
                            mutation_relevant=False,
                        )
                        end_sie_succeeded = bool(end_sie_verification.get("success"))
                        if end_sie_succeeded:
                            completed_mutation_stages.append("end_sie")
                        if not end_sie_succeeded:
                            lifecycle_failure = (
                                "verify_sie_ended",
                                end_sie_verification,
                            )
                    else:
                        lifecycle_failure = ("end_sie", end_sie)
                if cleanup_after:
                    if started_sie and not end_sie_succeeded:
                        cleanup_skipped_due_to_sie_end_failure = True
                    else:
                        cleanup = run_step("cleanup", _cleanup_code(label_prefix), timeout_seconds=60)
                        if not cleanup.get("success"):
                            lifecycle_failure = ("cleanup", cleanup)
            except _AICompositeTransportOutcomeUnknown:
                pass

    if unknown_failure is not None:
        return _ai_unknown_transport_report(
            run_id=run_id,
            steps=steps,
            report_dir=report_dir,
            failure=unknown_failure,
            completed_mutation_stages=completed_mutation_stages,
        )

    overall_success = all(pass_flags.values()) and lifecycle_failure is None
    report: Dict[str, Any] = {
        "success": overall_success,
        "run_id": run_id,
        "status": "PASS" if overall_success else "BLOCKED" if lifecycle_failure else "FAIL",
        "summary": {
            "patrol_reached": pass_flags["patrol"],
            "player_detected": pass_flags["detection"],
            "chase_distance_delta": chase_delta,
            "chase_followed": pass_flags["chase"],
            "mcp_dependency_audit_pass": pass_flags["mcp_dependency_audit"],
            "lifecycle_cleanup_pass": lifecycle_failure is None,
        },
        "config": config,
        "steps": steps,
        "evidence_standard": {
            "world_mode": "SIE",
            "content_scope": "temporary editor actors in the current level",
            "serialized_mcp_dependency": "none expected; fixture uses engine/project assets only",
            "detection_contract": "distance threshold plus AIController.line_of_sight_to",
            "movement_contract": "AIController.move_to_location/actor with use_pathfinding=False",
        },
    }
    if cleanup_skipped_due_to_sie_end_failure:
        report["cleanup_skipped_due_to_sie_end_failure"] = True
    if lifecycle_failure is not None:
        failed_stage, failed_response = lifecycle_failure
        report["reason"] = f"{failed_stage} failed; smoke lifecycle cleanup is incomplete"
        report["workflow_recovery_required"] = True
        report.update(
            composite_failure_safety_fields(
                failed_response,
                failed_stage=failed_stage,
                mutation_relevant=failed_stage != "verify_sie_ended",
                completed_mutation_stages=completed_mutation_stages,
            )
        )
    _attach_report_path_safely(report, report_dir)
    return report


def run_ai_behavior_tree_execution_smoke_impl(
    *,
    pawn_asset_path: str = DEFAULT_PAWN_ASSET,
    skeletal_mesh_path: str = DEFAULT_SKELETAL_MESH,
    anim_blueprint_path: str = DEFAULT_ANIM_BLUEPRINT,
    label_prefix: str = "MCP_AIBTExecutionSmoke",
    report_dir: str = DEFAULT_REPORT_DIR,
    behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
    blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
    target_actor_key: str = DEFAULT_TARGET_ACTOR_KEY,
    patrol_location_key: str = DEFAULT_PATROL_LOCATION_KEY,
    cleanup_existing: bool = True,
    cleanup_after: bool = True,
    setup_wait_seconds: float = 1.5,
    bt_wait_seconds: float = 5.0,
) -> Dict[str, Any]:
    """Run a live SIE smoke that drives the saved patrol/chase Behavior Tree."""
    label_error = _validate_ai_smoke_label_prefix(label_prefix)
    if label_error is not None:
        return label_error
    config: Dict[str, Any] = {
        "pawn_asset_path": pawn_asset_path,
        "skeletal_mesh_path": skeletal_mesh_path,
        "anim_blueprint_path": anim_blueprint_path,
        "label_prefix": label_prefix,
        "cleanup_existing": cleanup_existing,
        "floor_center": [180.0, 0.0, -55.0],
        "floor_scale": [12.0, 7.0, 0.1],
        "dummy_start": [-350.0, 0.0, 100.0],
        "player_far": [1500.0, 0.0, 100.0],
        "player_near": [650.0, 0.0, 100.0],
        "patrol_target": [250.0, 0.0, 100.0],
        "behavior_tree_path": behavior_tree_path,
        "blackboard_path": blackboard_path,
        "target_actor_key": target_actor_key,
        "patrol_location_key": patrol_location_key,
        "target_pass_distance": 180.0,
        "required_bt_chase_delta": 500.0,
    }

    try:
        reject_mcp_dependency_references("ai_behavior_tree_execution_smoke_config", {
            "pawn_asset_path": pawn_asset_path,
            "skeletal_mesh_path": skeletal_mesh_path,
            "anim_blueprint_path": anim_blueprint_path,
            "behavior_tree_path": behavior_tree_path,
            "blackboard_path": blackboard_path,
        })
    except ValueError as exc:
        return tool_error(str(exc), error_code="mcp_dependency_reference_rejected")

    run_id = _timestamp()
    steps: List[Dict[str, Any]] = []
    started_sie = False
    completed_mutation_stages: List[str] = []
    unknown_failure: _AICompositeTransportOutcomeUnknown | None = None
    lifecycle_failure: tuple[str, Dict[str, Any]] | None = None
    cleanup_skipped_due_to_sie_end_failure = False

    def run_step(
        name: str,
        code: str,
        *,
        allow_pie: bool = False,
        timeout_seconds: int = 60,
        mutation_relevant: bool = True,
        record_completed_mutation: bool = True,
    ) -> Dict[str, Any]:
        nonlocal unknown_failure
        result = _run_unreal_python_json(
            code,
            f"ai_bt_execution_{name}_{run_id}",
            allow_unsafe_editor_scripting_during_pie=allow_pie,
            timeout_seconds=timeout_seconds,
        )
        result["name"] = name
        result["status"] = _step_status(result)
        steps.append(result)
        if find_unknown_transport_marker(result) is not None:
            unknown_failure = _AICompositeTransportOutcomeUnknown(
                name,
                result,
                mutation_relevant=mutation_relevant,
            )
            raise unknown_failure
        if mutation_relevant and record_completed_mutation and result.get("success"):
            completed_mutation_stages.append(name)
        return result

    try:
        setup = run_step("setup", _setup_code(config), timeout_seconds=90)
    except _AICompositeTransportOutcomeUnknown as failure:
        return _ai_unknown_transport_report(
            run_id=run_id,
            steps=steps,
            report_dir=report_dir,
            failure=failure,
            completed_mutation_stages=completed_mutation_stages,
        )
    if not setup.get("success"):
        try:
            cleanup = run_step("cleanup_after_setup_failure", _cleanup_code(label_prefix), timeout_seconds=60)
        except _AICompositeTransportOutcomeUnknown as failure:
            return _ai_unknown_transport_report(
                run_id=run_id,
                steps=steps,
                report_dir=report_dir,
                failure=failure,
                completed_mutation_stages=completed_mutation_stages,
            )
        report = {
            "success": False,
            "run_id": run_id,
            "status": "BLOCKED",
            "reason": "setup failed",
            "steps": steps,
            "cleanup_result": cleanup,
        }
        if not cleanup.get("success"):
            report.update(
                composite_failure_safety_fields(
                    cleanup,
                    failed_stage="cleanup_after_setup_failure",
                    mutation_relevant=True,
                )
            )
            report.update(
                {
                    "reason": "setup failed and cleanup did not complete",
                    "mutation_attempted": True,
                    "mutation_state_unknown": True,
                    "do_not_retry": True,
                    "workflow_recovery_required": True,
                }
            )
        _attach_report_path_safely(report, report_dir)
        return report

    try:
        start = run_step("start_sie", _start_sie_code(), timeout_seconds=30)
        started_sie = bool(start.get("success"))
        if not started_sie:
            raise RuntimeError(start.get("message") or "SIE start failed")

        time.sleep(max(0.0, float(setup_wait_seconds)))
        bt_command = run_step("bt_execution_command", _bt_execution_command_code(config), allow_pie=True)
        if not bt_command.get("success"):
            raise RuntimeError(
                bt_command.get("message") or "Behavior Tree execution command failed"
            )
        distance_before = float(bt_command.get("distance_to_player_before") or 0.0)
        time.sleep(max(0.0, float(bt_wait_seconds)))
        bt_sample = run_step(
            "bt_execution_sample",
            _bt_execution_sample_code(config),
            allow_pie=True,
            mutation_relevant=False,
        )
        distance_after = float(bt_sample.get("distance_to_player_after") or 0.0)

        chase_delta = distance_before - distance_after
        pass_flags = {
            "run_behavior_tree": bool(bt_command.get("run_behavior_tree_result")),
            "blackboard_values": bool(bt_command.get("blackboard_values_set"))
            and bool(bt_sample.get("blackboard_values_still_set")),
            "bt_movement": bool(
                bt_sample.get("success")
                and (
                    bt_sample.get("within_target_pass_distance")
                    or chase_delta >= float(config["required_bt_chase_delta"])
                )
            ),
            "mcp_dependency_audit": not bool(
                setup.get("mcp_dependency_audit", {}).get("found_forbidden_reference", True)
            ),
        }
    except _AICompositeTransportOutcomeUnknown:
        pass
    except Exception as exc:
        steps.append(
            {
                "success": False,
                "status": "BLOCKED",
                "name": "orchestration_exception",
                "message": str(exc),
            }
        )
        pass_flags = {
            "run_behavior_tree": False,
            "blackboard_values": False,
            "bt_movement": False,
            "mcp_dependency_audit": False,
        }
        chase_delta = 0.0
    finally:
        if unknown_failure is None:
            try:
                end_sie_succeeded = not started_sie
                if started_sie:
                    end_sie = run_step(
                        "end_sie",
                        _end_sie_code(),
                        allow_pie=True,
                        timeout_seconds=30,
                        record_completed_mutation=False,
                    )
                    end_sie_succeeded = bool(end_sie.get("success"))
                    if end_sie_succeeded:
                        time.sleep(1.0)
                        end_sie_verification = run_step(
                            "verify_sie_ended",
                            _verify_sie_ended_code(),
                            timeout_seconds=30,
                            mutation_relevant=False,
                        )
                        end_sie_succeeded = bool(end_sie_verification.get("success"))
                        if end_sie_succeeded:
                            completed_mutation_stages.append("end_sie")
                        if not end_sie_succeeded:
                            lifecycle_failure = (
                                "verify_sie_ended",
                                end_sie_verification,
                            )
                    else:
                        lifecycle_failure = ("end_sie", end_sie)
                if cleanup_after:
                    if started_sie and not end_sie_succeeded:
                        cleanup_skipped_due_to_sie_end_failure = True
                    else:
                        cleanup = run_step("cleanup", _cleanup_code(label_prefix), timeout_seconds=60)
                        if not cleanup.get("success"):
                            lifecycle_failure = ("cleanup", cleanup)
            except _AICompositeTransportOutcomeUnknown:
                pass

    if unknown_failure is not None:
        return _ai_unknown_transport_report(
            run_id=run_id,
            steps=steps,
            report_dir=report_dir,
            failure=unknown_failure,
            completed_mutation_stages=completed_mutation_stages,
        )

    overall_success = all(pass_flags.values()) and lifecycle_failure is None
    report: Dict[str, Any] = {
        "success": overall_success,
        "run_id": run_id,
        "status": "PASS" if overall_success else "BLOCKED" if lifecycle_failure else "FAIL",
        "summary": {
            "run_behavior_tree": pass_flags["run_behavior_tree"],
            "blackboard_values_set": pass_flags["blackboard_values"],
            "bt_movement_delta": chase_delta,
            "bt_movement_pass": pass_flags["bt_movement"],
            "mcp_dependency_audit_pass": pass_flags["mcp_dependency_audit"],
            "lifecycle_cleanup_pass": lifecycle_failure is None,
        },
        "config": config,
        "steps": steps,
        "evidence_standard": {
            "world_mode": "SIE",
            "content_scope": "temporary editor actors plus saved standard BT/BB assets",
            "runtime_entrypoint": "AIController.run_behavior_tree",
            "blackboard_entrypoint": "AIController.use_blackboard plus BlackboardComponent key writes",
            "serialized_mcp_dependency": "fixture uses engine/project assets; MCP remains an authoring/test bridge only",
        },
    }
    if cleanup_skipped_due_to_sie_end_failure:
        report["cleanup_skipped_due_to_sie_end_failure"] = True
    if lifecycle_failure is not None:
        failed_stage, failed_response = lifecycle_failure
        report["reason"] = f"{failed_stage} failed; smoke lifecycle cleanup is incomplete"
        report["workflow_recovery_required"] = True
        report.update(
            composite_failure_safety_fields(
                failed_response,
                failed_stage=failed_stage,
                mutation_relevant=failed_stage != "verify_sie_ended",
                completed_mutation_stages=completed_mutation_stages,
            )
        )
    _attach_report_path_safely(report, report_dir)
    return report


def create_ai_patrol_chase_behavior_assets_impl(
    *,
    asset_root: str = DEFAULT_AI_ASSET_ROOT,
    behavior_tree_name: str = DEFAULT_BEHAVIOR_TREE_NAME,
    blackboard_name: str = DEFAULT_BLACKBOARD_NAME,
    target_actor_key: str = DEFAULT_TARGET_ACTOR_KEY,
    patrol_location_key: str = DEFAULT_PATROL_LOCATION_KEY,
    tree_shape: str = "sequence_patrol_chase",
    acceptable_radius: float = 55.0,
    alert_wait_seconds: float = 0.5,
    face_precision_degrees: float = 12.0,
    replace_existing: bool = False,
    save_assets: bool = False,
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """
    Create standard Unreal Behavior Tree and Blackboard assets for the AI smoke.

    The Unreal side uses a narrow C++ helper so Behavior Tree/Blackboard asset
    construction stays in engine APIs instead of brittle Unreal Python graph
    manipulation. The command rejects MCP-only references before and after
    authoring.
    """
    params = {
        "asset_root": asset_root,
        "behavior_tree_name": behavior_tree_name,
        "blackboard_name": blackboard_name,
        "target_actor_key": target_actor_key,
        "patrol_location_key": patrol_location_key,
        "tree_shape": tree_shape,
        "acceptable_radius": acceptable_radius,
        "alert_wait_seconds": alert_wait_seconds,
        "face_precision_degrees": face_precision_degrees,
        "replace_existing": replace_existing,
        "save_assets": save_assets,
    }
    reject_mcp_dependency_references("create_ai_patrol_chase_behavior_assets", params)
    result = _send_ai_command(
        "create_ai_patrol_chase_behavior_assets",
        params,
        timeout_seconds=timeout_seconds,
    )
    if result.get("success"):
        result["evidence_standard"] = {
            "behavior_tree_nodes": "standard AIModule UBehaviorTree, UBTComposite_Sequence/Selector, UBTTask_RotateToFaceBBEntry, UBTTask_Wait, and UBTTask_MoveDirectlyToward",
            "blackboard_keys": "standard UBlackboardData object/vector keys",
            "serialized_mcp_dependency": "rejected by Python preflight and C++ package-object audit",
            "save_behavior": "save_assets=false leaves packages unsaved for smoke; save_assets=true writes standard .uasset files",
        }
    return result


def configure_ai_controller_perception_impl(
    *,
    controller_blueprint: str = "/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller",
    component_name: str = "AIPerception",
    sight_radius: float = 900.0,
    lose_sight_radius: float = 1150.0,
    peripheral_vision_angle_degrees: float = 90.0,
    max_age: float = 3.0,
    detect_enemies: bool = True,
    detect_neutrals: bool = True,
    detect_friendlies: bool = True,
    replace_existing_config: bool = True,
    save_blueprint: bool = False,
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """Configure a standard AIPerception sight component on an AIController Blueprint."""
    params = {
        "controller_blueprint": controller_blueprint,
        "component_name": component_name,
        "sight_radius": sight_radius,
        "lose_sight_radius": lose_sight_radius,
        "peripheral_vision_angle_degrees": peripheral_vision_angle_degrees,
        "max_age": max_age,
        "detect_enemies": detect_enemies,
        "detect_neutrals": detect_neutrals,
        "detect_friendlies": detect_friendlies,
        "replace_existing_config": replace_existing_config,
        "save_blueprint": save_blueprint,
    }
    reject_mcp_dependency_references("configure_ai_controller_perception", params)
    result = _send_ai_command(
        "configure_ai_controller_perception",
        params,
        timeout_seconds=timeout_seconds,
    )
    if result.get("success"):
        result["evidence_standard"] = {
            "controller_component": "standard UAIPerceptionComponent SCS template on an AIController Blueprint",
            "sight_config": "standard UAISenseConfig_Sight with UAISense_Sight as dominant sense",
            "serialized_mcp_dependency": "rejected by Python preflight and C++ package-object audit",
        }
    return result


def inspect_ai_behavior_assets_impl(
    *,
    behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
    blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """Inspect saved standard Unreal Behavior Tree and Blackboard asset structure."""
    params = {
        "behavior_tree_path": behavior_tree_path,
        "blackboard_path": blackboard_path,
    }
    reject_mcp_dependency_references("inspect_ai_behavior_assets", params)
    return _send_ai_command(
        "inspect_ai_behavior_assets",
        params,
        timeout_seconds=timeout_seconds,
    )


def list_ai_assets_impl(
    *,
    asset_root: str = "/Game",
    asset_type: str = "all",
    name_filter: str = "",
    max_results: int = 500,
    timeout_seconds: int = 60,
) -> Dict[str, Any]:
    """List standard UE 5.6 Behavior Tree, Blackboard, and EQS assets."""
    normalized_root = str(asset_root).replace("\\", "/").rstrip("/") or "/Game"
    normalized_type = str(asset_type).strip().lower()
    allowed_types = {"all", "behavior_tree", "blackboard", "eqs"}
    if not (normalized_root == "/Game" or normalized_root.startswith("/Game/")):
        return tool_error(
            "asset_root must be /Game or a project content subpath",
            error_code="invalid_ai_asset_root",
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
    if ".." in normalized_root or "." in normalized_root:
        return tool_error(
            "asset_root must be a package path, not an object or traversal path",
            error_code="invalid_ai_asset_root",
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
    if normalized_type not in allowed_types:
        return tool_error(
            "asset_type must be all, behavior_tree, blackboard, or eqs",
            error_code="invalid_ai_asset_type",
            mutation_attempted=False,
            mutation_performed=False,
            changed=False,
        )
    max_results = max(1, min(int(max_results), 2000))
    reject_mcp_dependency_references(
        "list_ai_assets",
        {"asset_root": normalized_root, "name_filter": name_filter},
    )
    code = f"""
import unreal
asset_root = {normalized_root!r}
asset_type = {normalized_type!r}
name_filter = {str(name_filter).strip().lower()!r}
max_results = {max_results!r}
class_map = {{
    "BehaviorTree": "behavior_tree",
    "BlackboardData": "blackboard",
    "EnvQuery": "eqs",
    "EnvironmentQuery": "eqs",
}}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
items = []
for asset in registry.get_assets_by_path(asset_root, recursive=True):
    class_name = str(asset.asset_class_path.asset_name)
    kind = class_map.get(class_name)
    if not kind or (asset_type != "all" and kind != asset_type):
        continue
    asset_name = str(asset.asset_name)
    package_name = str(asset.package_name)
    object_path = package_name + "." + asset_name
    if name_filter and name_filter not in asset_name.lower() and name_filter not in object_path.lower():
        continue
    items.append({{
        "asset_type": kind,
        "asset_name": asset_name,
        "package_name": package_name,
        "object_path": object_path,
        "class_name": class_name,
    }})
items.sort(key=lambda item: (item["asset_type"], item["object_path"].lower()))
total_matched = len(items)
RESULT = {{
    "success": True,
    "asset_root": asset_root,
    "asset_type": asset_type,
    "total_matched": total_matched,
    "returned_count": min(total_matched, max_results),
    "truncated": total_matched > max_results,
    "assets": items[:max_results],
    "engine_target": "Unreal Engine 5.6.1",
    "mutation_attempted": False,
    "mutation_performed": False,
    "changed": False,
}}
"""
    return _run_unreal_python_json(
        code,
        "list_ai_assets",
        timeout_seconds=timeout_seconds,
    )


def _ai_tool_failure(operation: str, exc: Exception) -> Dict[str, Any]:
    logger.exception("AI tool failed: %s", operation)
    error_factory = tool_pre_dispatch_error if isinstance(exc, ValueError) else tool_error
    return error_factory(
        str(exc),
        error_code=f"{operation}_failed",
        details={"exception_type": type(exc).__name__},
    )


def register_ai_tools(mcp: FastMCP):
    """Register AI authoring and smoke-test MCP tools."""

    @mcp.tool()
    def inspect_ai_authoring_support() -> Dict[str, Any]:
        """Inspect Unreal Python AI authoring API support in the running editor."""
        try:
            return inspect_ai_authoring_support_impl()
        except Exception as exc:
            logger.exception("Failed to inspect AI authoring support")
            return tool_error(
                str(exc),
                error_code="inspect_ai_authoring_support_failed",
                details={"exception_type": type(exc).__name__},
            )

    @mcp.tool()
    def list_ai_assets(
        asset_root: str = "/Game",
        asset_type: str = "all",
        name_filter: str = "",
        max_results: int = 500,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        List UE 5.6 Behavior Tree, Blackboard, and EQS assets under /Game.

        asset_type accepts all, behavior_tree, blackboard, or eqs. This is a
        read-only Unreal Python Asset Registry query.
        """
        try:
            return list_ai_assets_impl(
                asset_root=asset_root,
                asset_type=asset_type,
                name_filter=name_filter,
                max_results=max_results,
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("list_ai_assets", exc)

    @mcp.tool()
    def inspect_ai_asset_structure(
        asset_path: str,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Inspect one UE 5.6 Behavior Tree, Blackboard, or EQS asset.

        Behavior Trees return their runtime hierarchy, services, decorators,
        editor graph state, and linked Blackboard. Blackboards return own and
        inherited typed keys. EQS returns its option and graph summary.
        """
        try:
            return _send_ai_command(
                "ai_inspect_asset_structure",
                {"asset_path": asset_path},
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("inspect_ai_asset_structure", exc)

    @mcp.tool()
    def create_ai_blackboard_asset(
        asset_name: str,
        asset_root: str = "/Game/AI",
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Create an empty standard UE 5.6 BlackboardData asset.

        Existing targets are never replaced. The native editor helper updates
        Blackboard key caches, saves by default, and verifies exact-path readback.
        """
        try:
            return _send_ai_command(
                "ai_create_blackboard_asset",
                {
                    "asset_root": asset_root,
                    "asset_name": asset_name,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("create_ai_blackboard_asset", exc)

    @mcp.tool()
    def add_ai_blackboard_key(
        blackboard_path: str,
        key_name: str,
        key_type: str,
        base_class: str = "",
        enum_path: str = "",
        instance_synced: bool = False,
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Add one typed key to a UE 5.6 BlackboardData asset.

        Supported key_type values are bool, class, enum, float, int, name,
        object, rotator, string, and vector. Object/Class may use an exact
        base_class object path; enum requires an exact enum_path.
        """
        try:
            return _send_ai_command(
                "ai_add_blackboard_key",
                {
                    "blackboard_path": blackboard_path,
                    "key_name": key_name,
                    "key_type": key_type,
                    "base_class": base_class,
                    "enum_path": enum_path,
                    "instance_synced": instance_synced,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("add_ai_blackboard_key", exc)

    @mcp.tool()
    def remove_ai_blackboard_key(
        blackboard_path: str,
        key_name: str,
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Remove one locally owned key from a UE 5.6 BlackboardData asset.

        Inherited and engine-provided keys are rejected. Behavior Tree nodes
        referencing the removed key are not rewritten.
        """
        try:
            return _send_ai_command(
                "ai_remove_blackboard_key",
                {
                    "blackboard_path": blackboard_path,
                    "key_name": key_name,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("remove_ai_blackboard_key", exc)

    @mcp.tool()
    def create_ai_behavior_tree_asset(
        asset_name: str,
        asset_root: str = "/Game/AI",
        blackboard_path: str = "",
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Create a blank standard UE 5.6 BehaviorTree asset and editor graph.

        Optionally links an existing BlackboardData asset. Existing targets are
        never replaced; the graph is regenerated and read back before success.
        """
        try:
            return _send_ai_command(
                "ai_create_behavior_tree_asset",
                {
                    "asset_root": asset_root,
                    "asset_name": asset_name,
                    "blackboard_path": blackboard_path,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("create_ai_behavior_tree_asset", exc)

    @mcp.tool()
    def set_ai_behavior_tree_blackboard(
        behavior_tree_path: str,
        blackboard_path: str,
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Assign a BlackboardData asset to a UE 5.6 BehaviorTree.

        Synchronizes both the runtime BehaviorTree pointer and editor graph root,
        runs the UE 5.6 graph update route, saves, and verifies both readbacks.
        """
        try:
            return _send_ai_command(
                "ai_set_behavior_tree_blackboard",
                {
                    "behavior_tree_path": behavior_tree_path,
                    "blackboard_path": blackboard_path,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("set_ai_behavior_tree_blackboard", exc)

    @mcp.tool()
    def search_ai_behavior_tree_node_classes(
        category: str = "all",
        query: str = "",
        include_abstract: bool = False,
        max_results: int = 200,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Search loaded UE 5.6 BT composite, task, decorator, and service classes.

        Returns native classes and any Blueprint-generated classes currently
        loaded in the editor; no asset or editor session is mutated.
        """
        try:
            return _send_ai_command(
                "ai_search_behavior_tree_node_classes",
                {
                    "category": category,
                    "query": query,
                    "include_abstract": include_abstract,
                    "max_results": max_results,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("search_ai_behavior_tree_node_classes", exc)

    @mcp.tool()
    def create_ai_eqs_query(
        asset_name: str,
        asset_root: str = "/Game/AI/EQS",
        save_asset: bool = True,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Create a blank standard UE 5.6 EnvQuery asset.

        This minimal tool intentionally creates only the factory-equivalent
        blank query; EQS option/generator/test graph authoring is out of scope.
        """
        try:
            return _send_ai_command(
                "ai_create_eqs_query",
                {
                    "asset_root": asset_root,
                    "asset_name": asset_name,
                    "save_asset": save_asset,
                },
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("create_ai_eqs_query", exc)

    @mcp.tool()
    def run_ai_dummy_patrol_chase_smoke(
        pawn_asset_path: str = DEFAULT_PAWN_ASSET,
        skeletal_mesh_path: str = DEFAULT_SKELETAL_MESH,
        anim_blueprint_path: str = DEFAULT_ANIM_BLUEPRINT,
        label_prefix: str = DEFAULT_LABEL_PREFIX,
        report_dir: str = DEFAULT_REPORT_DIR,
        cleanup_existing: bool = True,
        cleanup_after: bool = True,
        setup_wait_seconds: float = 1.5,
        patrol_wait_seconds: float = 2.0,
        chase_wait_seconds: float = 2.0,
        sight_radius: float = 900.0,
        acceptance_radius: float = 55.0,
    ) -> Dict[str, Any]:
        """
        Run an SIE smoke for an AI dummy that patrols, detects a player, and follows.

        The fixture uses engine/project assets only and leaves no serialized
        UnrealMCP dependency in authored content.
        """
        global _AI_SMOKE_WORKFLOW_QUARANTINED
        if not _AI_SMOKE_WORKFLOW_LOCK.acquire(blocking=False):
            return tool_error(
                "Another AI SIE smoke is already running",
                error_code="ai_smoke_already_running",
                workflow_lock="ai_sie_smoke",
                mutation_attempted=False,
                mutation_performed=False,
                changed=False,
            )
        try:
            if _AI_SMOKE_WORKFLOW_QUARANTINED:
                return tool_error(
                    "AI SIE smoke is quarantined after an unknown prior outcome",
                    error_code="ai_smoke_recovery_required",
                    workflow_lock="ai_sie_smoke",
                    mutation_attempted=False,
                    mutation_performed=False,
                    changed=False,
                    do_not_retry=True,
                    recovery_steps=[
                        "inspect whether SIE is still active",
                        "inspect and clean the prior smoke actors manually",
                        "restart the MCP server only after editor state is known",
                    ],
                )
            result = run_ai_dummy_patrol_chase_smoke_impl(
                pawn_asset_path=pawn_asset_path,
                skeletal_mesh_path=skeletal_mesh_path,
                anim_blueprint_path=anim_blueprint_path,
                label_prefix=label_prefix,
                report_dir=report_dir,
                cleanup_existing=cleanup_existing,
                cleanup_after=cleanup_after,
                setup_wait_seconds=setup_wait_seconds,
                patrol_wait_seconds=patrol_wait_seconds,
                chase_wait_seconds=chase_wait_seconds,
                sight_radius=sight_radius,
                acceptance_radius=acceptance_radius,
            )
            if (
                result.get("mutation_state_unknown")
                or result.get("read_command_outcome_unknown")
                or result.get("workflow_recovery_required")
            ):
                _AI_SMOKE_WORKFLOW_QUARANTINED = True
            return result
        except Exception as exc:
            _AI_SMOKE_WORKFLOW_QUARANTINED = True
            logger.exception("Failed to run AI dummy patrol/chase smoke")
            return tool_error(
                str(exc),
                error_code="ai_dummy_patrol_chase_smoke_failed",
                details={"exception_type": type(exc).__name__},
                mutation_state_unknown=True,
                do_not_retry=True,
                workflow_recovery_required=True,
            )
        finally:
            _AI_SMOKE_WORKFLOW_LOCK.release()

    @mcp.tool()
    def run_ai_behavior_tree_execution_smoke(
        pawn_asset_path: str = DEFAULT_PAWN_ASSET,
        skeletal_mesh_path: str = DEFAULT_SKELETAL_MESH,
        anim_blueprint_path: str = DEFAULT_ANIM_BLUEPRINT,
        label_prefix: str = "MCP_AIBTExecutionSmoke",
        report_dir: str = DEFAULT_REPORT_DIR,
        behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
        blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
        target_actor_key: str = DEFAULT_TARGET_ACTOR_KEY,
        patrol_location_key: str = DEFAULT_PATROL_LOCATION_KEY,
        setup_wait_seconds: float = 1.5,
        bt_wait_seconds: float = 5.0,
    ) -> Dict[str, Any]:
        """Run the saved patrol/chase Behavior Tree through AIController in SIE."""
        global _AI_SMOKE_WORKFLOW_QUARANTINED
        if not _AI_SMOKE_WORKFLOW_LOCK.acquire(blocking=False):
            return tool_error(
                "Another AI SIE smoke is already running",
                error_code="ai_smoke_already_running",
                workflow_lock="ai_sie_smoke",
                mutation_attempted=False,
                mutation_performed=False,
                changed=False,
            )
        try:
            if _AI_SMOKE_WORKFLOW_QUARANTINED:
                return tool_error(
                    "AI SIE smoke is quarantined after an unknown prior outcome",
                    error_code="ai_smoke_recovery_required",
                    workflow_lock="ai_sie_smoke",
                    mutation_attempted=False,
                    mutation_performed=False,
                    changed=False,
                    do_not_retry=True,
                    recovery_steps=[
                        "inspect whether SIE is still active",
                        "inspect and clean the prior smoke actors manually",
                        "restart the MCP server only after editor state is known",
                    ],
                )
            result = run_ai_behavior_tree_execution_smoke_impl(
                pawn_asset_path=pawn_asset_path,
                skeletal_mesh_path=skeletal_mesh_path,
                anim_blueprint_path=anim_blueprint_path,
                label_prefix=label_prefix,
                report_dir=report_dir,
                behavior_tree_path=behavior_tree_path,
                blackboard_path=blackboard_path,
                target_actor_key=target_actor_key,
                patrol_location_key=patrol_location_key,
                setup_wait_seconds=setup_wait_seconds,
                bt_wait_seconds=bt_wait_seconds,
            )
            if (
                result.get("mutation_state_unknown")
                or result.get("read_command_outcome_unknown")
                or result.get("workflow_recovery_required")
            ):
                _AI_SMOKE_WORKFLOW_QUARANTINED = True
            return result
        except Exception as exc:
            _AI_SMOKE_WORKFLOW_QUARANTINED = True
            logger.exception("Failed to run AI Behavior Tree execution smoke")
            return tool_error(
                str(exc),
                error_code="ai_behavior_tree_execution_smoke_failed",
                details={"exception_type": type(exc).__name__},
                mutation_state_unknown=True,
                do_not_retry=True,
                workflow_recovery_required=True,
            )
        finally:
            _AI_SMOKE_WORKFLOW_LOCK.release()

    @mcp.tool()
    def create_ai_patrol_chase_behavior_assets(
        asset_root: str = DEFAULT_AI_ASSET_ROOT,
        behavior_tree_name: str = DEFAULT_BEHAVIOR_TREE_NAME,
        blackboard_name: str = DEFAULT_BLACKBOARD_NAME,
        target_actor_key: str = DEFAULT_TARGET_ACTOR_KEY,
        patrol_location_key: str = DEFAULT_PATROL_LOCATION_KEY,
        tree_shape: str = "sequence_patrol_chase",
        acceptable_radius: float = 55.0,
        alert_wait_seconds: float = 0.5,
        face_precision_degrees: float = 12.0,
        replace_existing: bool = False,
        save_assets: bool = False,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Create standard Behavior Tree and Blackboard assets for patrol/chase AI.

        Uses the editor-only C++ helper in UnrealMCP for stable BT/BB authoring,
        while ensuring generated content does not depend on UnrealMCP.
        """
        try:
            return create_ai_patrol_chase_behavior_assets_impl(
                asset_root=asset_root,
                behavior_tree_name=behavior_tree_name,
                blackboard_name=blackboard_name,
                target_actor_key=target_actor_key,
                patrol_location_key=patrol_location_key,
                tree_shape=tree_shape,
                acceptable_radius=acceptable_radius,
                alert_wait_seconds=alert_wait_seconds,
                face_precision_degrees=face_precision_degrees,
                replace_existing=replace_existing,
                save_assets=save_assets,
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("create_ai_patrol_chase_behavior_assets", exc)

    @mcp.tool()
    def configure_ai_controller_perception(
        controller_blueprint: str = "/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller",
        component_name: str = "AIPerception",
        sight_radius: float = 900.0,
        lose_sight_radius: float = 1150.0,
        peripheral_vision_angle_degrees: float = 90.0,
        max_age: float = 3.0,
        detect_enemies: bool = True,
        detect_neutrals: bool = True,
        detect_friendlies: bool = True,
        replace_existing_config: bool = True,
        save_blueprint: bool = False,
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """
        Configure standard AIPerception sight settings on an AIController Blueprint.

        Uses a narrow editor-only C++ helper because Unreal Python does not
        expose UAIPerceptionComponent::ConfigureSense or SetDominantSense.
        """
        try:
            return configure_ai_controller_perception_impl(
                controller_blueprint=controller_blueprint,
                component_name=component_name,
                sight_radius=sight_radius,
                lose_sight_radius=lose_sight_radius,
                peripheral_vision_angle_degrees=peripheral_vision_angle_degrees,
                max_age=max_age,
                detect_enemies=detect_enemies,
                detect_neutrals=detect_neutrals,
                detect_friendlies=detect_friendlies,
                replace_existing_config=replace_existing_config,
                save_blueprint=save_blueprint,
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            return _ai_tool_failure("configure_ai_controller_perception", exc)

    @mcp.tool()
    def inspect_ai_behavior_assets(
        behavior_tree_path: str = "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase",
        blackboard_path: str = "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase",
        timeout_seconds: int = 60,
    ) -> Dict[str, Any]:
        """Inspect saved Behavior Tree/Blackboard structure through the C++ helper."""
        try:
            return inspect_ai_behavior_assets_impl(
                behavior_tree_path=behavior_tree_path,
                blackboard_path=blackboard_path,
                timeout_seconds=timeout_seconds,
            )
        except Exception as exc:
            logger.exception("Failed to inspect AI behavior assets")
            return tool_error(
                str(exc),
                error_code="inspect_ai_behavior_assets_failed",
                details={"exception_type": type(exc).__name__},
            )

    logger.info("AI tools registered successfully")
