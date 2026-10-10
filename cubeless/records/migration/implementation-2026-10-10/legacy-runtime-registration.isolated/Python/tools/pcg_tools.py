"""
PCG automation tools for Unreal MCP.

Fast editor operations prefer native UnrealMCP bridge commands when available.
Unreal Python remains as a compatibility fallback for older running editor
sessions or commands that still need TA script flexibility.
"""

import json
import logging
import os
import tempfile
import textwrap
import time
from typing import Any, Dict, List
from uuid import uuid4

from mcp.server.fastmcp import Context, FastMCP

from services.unreal_bridge_connection import unreal_python_result_unknown_error
from tools.dependency_guard import (
    reject_mcp_dependency_reference,
    reject_mcp_dependency_references,
)
from tools.mutation_result_contract import (
    composite_failure_safety_fields,
    find_unknown_transport_marker,
)
from tools.response_schema import tool_error

logger = logging.getLogger("UnrealMCP")


def _unreal_tchar_length(value: str) -> int:
    """Return the Windows Unreal FString length in UTF-16 code units."""

    return len(value.encode("utf-16-le", errors="surrogatepass")) // 2


def _unreal_connection_error() -> Dict[str, Any]:
    return tool_error(
        "Failed to connect to Unreal Engine",
        error_code="unreal_connection_failed",
    )


def _unreal_no_response_error() -> Dict[str, Any]:
    return tool_error(
        "No response from Unreal Engine",
        error_code="unreal_no_response",
    )


def _pcg_exception_error(message: str, exc: Exception) -> Dict[str, Any]:
    return tool_error(
        message,
        error_code="pcg_tool_failed",
        details={"exception_type": type(exc).__name__},
    )


def _run_unreal_python_json(code_body: str) -> Dict[str, Any]:
    """Run Unreal Python code that writes RESULT to a temp JSON file."""
    from unreal_mcp_server import get_unreal_connection

    invocation_id = uuid4().hex
    result_path = os.path.join(
        tempfile.gettempdir(),
        f"unreal_mcp_pcg_{invocation_id}_result.json",
    )
    scope_name = f"_unreal_mcp_pcg_scope_{invocation_id}"

    wrapped_code = f"""
def {scope_name}():
    import json
    import traceback
    class MCPResultReady(Exception):
        pass
    RESULT_PATH = {result_path!r}
    RESULT = None
    try:
{textwrap.indent(code_body, "        ")}
        if RESULT is None:
            RESULT = {{"success": True}}
    except MCPResultReady:
        pass
    except Exception as exc:
        RESULT = {{"success": False, "error": str(exc), "traceback": traceback.format_exc()}}
    with open(RESULT_PATH, "w", encoding="utf-8") as result_file:
        json.dump(RESULT, result_file, ensure_ascii=False, indent=2, default=str)

try:
    {scope_name}()
finally:
    del {scope_name}
"""

    preserve_result_file = False

    def preserve_unknown_evidence(result: Dict[str, Any]) -> Dict[str, Any]:
        nonlocal preserve_result_file
        preserve_result_file = True
        result = dict(result)
        result["side_channel_evidence"] = {
            "path": result_path,
            "preserved": True,
            "exists_at_return": os.path.exists(result_path),
            "may_appear_after_return": True,
            "reason": "mutation_outcome_unknown",
        }
        return result

    try:
        unreal = get_unreal_connection()
        if not unreal:
            return _unreal_connection_error()

        response = unreal.send_command(
            "execute_python",
            {
                "code": wrapped_code,
                "mode": "ExecuteFile",
                "interchange_policy": "none",
            },
        )
        if (
            isinstance(response, dict)
            and find_unknown_transport_marker(response) is not None
        ):
            return preserve_unknown_evidence(response)
        if not response or response.get("status") == "error":
            return response or _unreal_no_response_error()

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
        except (
            OSError,
            UnicodeError,
            json.JSONDecodeError,
            ValueError,
        ) as result_error:
            unknown_result = unreal_python_result_unknown_error(
                "Unreal Python wrote an unreadable result file",
                error_code="unreal_result_file_invalid",
                unreal_response=response,
                failure_kind="side_channel_result_invalid",
                exception=result_error,
            )
            return preserve_unknown_evidence(unknown_result)
        if result.get("success") is False and result.get("traceback"):
            unknown_result = unreal_python_result_unknown_error(
                str(
                    result.get("error")
                    or result.get("message")
                    or "Unreal Python execution raised after it began"
                ),
                error_code="unreal_python_execution_outcome_unknown",
                unreal_response=response,
                failure_kind="side_channel_execution_exception",
            )
            unknown_result["unreal_python_result"] = result
            return preserve_unknown_evidence(unknown_result)
        result["unreal_response"] = response
        return result
    finally:
        if not preserve_result_file:
            try:
                os.remove(result_path)
            except FileNotFoundError:
                pass
            except OSError as exc:
                logger.warning(
                    "Failed to remove PCG result file %s: %s", result_path, exc
                )


def _bridge_response_succeeded(response: Dict[str, Any] | None) -> bool:
    if not response:
        return False
    if response.get("status") == "error" or response.get("success") is False:
        return False
    nested = response.get("result")
    if isinstance(nested, dict) and nested.get("success") is False:
        return False
    return bool(
        response.get("status") == "success"
        or response.get("success") is True
        or (isinstance(nested, dict) and nested.get("success") is True)
    )


def _bridge_response_unknown_command(
    response: Dict[str, Any] | None, command_name: str
) -> bool:
    if not response:
        return False
    if find_unknown_transport_marker(response) is not None:
        return False
    error_text = str(response.get("error") or response.get("message") or "")
    return (
        response.get("status") == "error"
        and "Unknown command" in error_text
        and command_name in error_text
    )


def _native_result_with_marker(
    response: Dict[str, Any], command_name: str
) -> Dict[str, Any]:
    result = response.get("result", response)
    if isinstance(result, dict):
        result = dict(result)
    else:
        result = {"result": result}
    result.setdefault("success", True)
    result["native_command_used"] = True
    result["native_command"] = command_name
    result["native_response"] = response
    return result


def _int_or_default(value: Any, default: int) -> int:
    try:
        return int(value)
    except (TypeError, ValueError):
        return default


def _native_refresh_result_ready(result: Dict[str, Any]) -> bool:
    components = result.get("components", [])
    if not isinstance(components, list):
        return bool(result.get("wait_completed", False))
    if not components:
        return _int_or_default(result.get("component_count"), 0) == 0

    min_generated_output_data_count = _int_or_default(
        result.get("min_generated_output_data_count"),
        -1,
    )
    min_managed_resource_count = _int_or_default(
        result.get("min_managed_resource_count"),
        -1,
    )

    for component in components:
        if not isinstance(component, dict):
            return False
        if (
            component.get("generating")
            or component.get("cleaning_up")
            or component.get("refresh_in_progress")
        ):
            return False
        if (
            min_generated_output_data_count >= 0
            and _int_or_default(component.get("generated_output_data_count"), 0)
            < min_generated_output_data_count
        ):
            return False
        if (
            min_managed_resource_count >= 0
            and _int_or_default(component.get("managed_resource_count"), 0)
            < min_managed_resource_count
        ):
            return False

    return True


def register_pcg_tools(mcp: FastMCP):
    """Register PCG-focused automation tools."""

    def send_pcg_command(command_name: str, params: Dict[str, Any]) -> Dict[str, Any]:
        from unreal_mcp_server import get_unreal_connection

        unreal = get_unreal_connection()
        if not unreal:
            logger.error("Failed to connect to Unreal Engine")
            return _unreal_connection_error()

        response = unreal.send_command(command_name, params)
        if not response:
            logger.error("No response from Unreal Engine")
            return _unreal_no_response_error()

        return response

    def poll_native_refresh_completion(
        command_name: str,
        initial_params: Dict[str, Any],
        initial_result: Dict[str, Any],
        timeout_seconds: float,
        poll_interval_seconds: float,
    ) -> Dict[str, Any]:
        timeout_seconds = max(0.0, min(float(timeout_seconds), 300.0))
        poll_interval_seconds = max(0.005, min(float(poll_interval_seconds), 1.0))

        poll_params = dict(initial_params)
        poll_params.update(
            {
                "cleanup": False,
                "generate": False,
                "refresh": False,
                "force": False,
                "wait_until_complete": True,
            }
        )

        result = dict(initial_result)
        initial_mutation_count = sum(
            max(0, _int_or_default(initial_result.get(field), 0))
            for field in ("cleanup_count", "refresh_count", "generate_count")
        )
        initial_mutation_performed = initial_mutation_count > 0
        completed_mutation_stages = (
            ["refresh_pcg_components_initial"] if initial_mutation_performed else []
        )
        start_time = time.monotonic()
        iterations = 0
        ready = _native_refresh_result_ready(result)

        while not ready:
            elapsed = time.monotonic() - start_time
            if elapsed >= timeout_seconds:
                break

            time.sleep(min(poll_interval_seconds, timeout_seconds - elapsed))
            iterations += 1

            poll_response = send_pcg_command(command_name, poll_params)
            if not _bridge_response_succeeded(poll_response):
                return tool_error(
                    "Native PCG completion poll failed",
                    error_code="native_pcg_completion_poll_failed",
                    native_command_used=True,
                    native_command=command_name,
                    initial_native_response=initial_result.get("native_response"),
                    **composite_failure_safety_fields(
                        poll_response,
                        failed_stage="poll_pcg_completion",
                        mutation_relevant=False,
                        completed_mutation_stages=completed_mutation_stages,
                    ),
                )

            result = _native_result_with_marker(poll_response, command_name)
            ready = _native_refresh_result_ready(result)

        elapsed = time.monotonic() - start_time
        result["external_wait_used"] = True
        result["wait_requested"] = True
        result["wait_supported"] = True
        result["wait_mode"] = "external_mcp_poll"
        result["wait_completed"] = ready
        result["wait_timed_out"] = not ready
        result["wait_elapsed_seconds"] = elapsed
        result["wait_iterations"] = iterations
        result["timeout_seconds"] = timeout_seconds
        result["poll_interval_seconds"] = poll_interval_seconds
        result["initial_native_response"] = initial_result.get("native_response")
        result["initial_component_count"] = initial_result.get("component_count")
        result["initial_cleanup_count"] = initial_result.get("cleanup_count")
        result["initial_refresh_count"] = initial_result.get("refresh_count")
        result["initial_generate_count"] = initial_result.get("generate_count")
        if not ready:
            result.update(
                {
                    "success": False,
                    "status": "error",
                    "error_code": "pcg_generation_completion_timeout",
                    "message": "PCG generation did not reach a terminal ready state before the external wait deadline",
                    "error": "PCG generation did not reach a terminal ready state before the external wait deadline",
                    "failed_stage": "wait_for_pcg_completion",
                    "failed_stage_effect": "read",
                    "pcg_generation_may_still_be_in_flight": True,
                    "recovery_policy": "poll_or_inspect_current_pcg_component_state_before_retry",
                }
            )
            if initial_mutation_performed:
                result.update(
                    {
                        "partial_mutation": True,
                        "mutation_performed": True,
                        "completed_mutation_count": len(completed_mutation_stages),
                        "completed_mutation_stages": completed_mutation_stages,
                        "do_not_retry": True,
                    }
                )
            else:
                result.update(
                    {
                        "mutation_attempted": False,
                        "mutation_performed": False,
                        "changed": False,
                    }
                )
        return result

    @mcp.tool()
    def list_pcg_assets(ctx: Context, root_path: str = "/Game") -> Dict[str, Any]:
        """
        List PCG-related assets under a content path.

        Args:
            root_path: Unreal content path to scan, for example /Game.
        """
        code = f"""
import unreal
root_path = {root_path!r}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path(root_path, recursive=True)
items = []
for asset in assets:
    class_name = str(asset.asset_class_path.asset_name)
    package_name = str(asset.package_name)
    asset_name = str(asset.asset_name)
    if "PCG" in class_name or "PCG" in asset_name:
        items.append({{
            "asset_name": asset_name,
            "package_name": package_name,
            "class_name": class_name,
            "object_path": f"{{package_name}}.{{asset_name}}",
        }})
RESULT = {{"success": True, "count": len(items), "assets": items}}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def list_pcg_components(ctx: Context) -> Dict[str, Any]:
        """List PCG-like components in the current editor level."""
        code = """
import unreal
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_all_level_actors()
items = []
for actor in actors:
    for component in actor.get_components_by_class(unreal.ActorComponent):
        class_name = component.get_class().get_name()
        if "PCG" in class_name:
            items.append({
                "actor": actor.get_actor_label(),
                "actor_path": actor.get_path_name(),
                "component": component.get_name(),
                "component_class": class_name,
            })
RESULT = {"success": True, "count": len(items), "components": items}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def refresh_pcg_components(
        ctx: Context,
        actor_name: str = "",
        selected_only: bool = False,
        cleanup: bool = True,
        generate: bool = True,
        refresh: bool = False,
        force: bool = True,
        wait_until_complete: bool = False,
        timeout_seconds: float = 10.0,
        poll_interval_seconds: float = 0.05,
        min_generated_output_data_count: int = -1,
        min_managed_resource_count: int = -1,
        max_components: int = 1000,
        native_first: bool = True,
    ) -> Dict[str, Any]:
        """
        Try to refresh/generate PCG components in the current editor level.

        Args:
            actor_name: Optional actor label/path substring filter.
            selected_only: If true, process only selected actors.
            cleanup: Clean existing generated output before generation.
            generate: Request PCG generation.
            refresh: Dirty and refresh components before generation.
            force: Force PCG generation when the native command is available.
            wait_until_complete: Poll native component state externally until PCG busy state clears.
            timeout_seconds: External native polling timeout.
            poll_interval_seconds: External native polling interval.
            min_generated_output_data_count: Optional native wait minimum generated output data count.
            min_managed_resource_count: Optional native wait minimum managed resource count.
            max_components: Maximum native PCG components to process.
            native_first: Prefer the native C++ bridge command when available.
        """
        native_command = "refresh_pcg_components"
        native_response = None
        if native_first:
            native_params: Dict[str, Any] = {
                "selected_only": selected_only,
                "cleanup": cleanup,
                "generate": generate,
                "refresh": refresh,
                "force": force,
                "wait_until_complete": False,
                "timeout_seconds": timeout_seconds,
                "poll_interval_seconds": poll_interval_seconds,
                "min_generated_output_data_count": min_generated_output_data_count,
                "min_managed_resource_count": min_managed_resource_count,
                "max_components": max_components,
            }
            if actor_name:
                native_params["actor_label_prefix"] = actor_name
            native_response = send_pcg_command(native_command, native_params)
            if _bridge_response_succeeded(native_response):
                native_result = _native_result_with_marker(
                    native_response, native_command
                )
                if not actor_name or int(native_result.get("component_count", 0)) > 0:
                    if wait_until_complete:
                        return poll_native_refresh_completion(
                            native_command,
                            native_params,
                            native_result,
                            timeout_seconds,
                            poll_interval_seconds,
                        )
                    return native_result
            elif native_response and not _bridge_response_unknown_command(
                native_response, native_command
            ):
                return tool_error(
                    "Native PCG refresh command failed before Python fallback",
                    error_code="native_pcg_refresh_failed",
                    native_command_used=False,
                    native_command=native_command,
                    native_response=native_response,
                )

        method_candidates = []
        if cleanup:
            method_candidates.append("cleanup")
        if refresh:
            method_candidates.extend(["refresh", "dirty_generated"])
        if generate:
            method_candidates.append("generate")
        if not method_candidates:
            method_candidates = ["refresh"]

        code = f"""
import unreal
actor_filter = {actor_name!r}
selected_only = {bool(selected_only)!r}
method_candidates = {method_candidates!r}
max_components = {int(max_components)!r}
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_selected_level_actors() if selected_only else editor_actor_subsystem.get_all_level_actors()
processed = []
errors = []
limit_hit = False
for actor in actors:
    if len(processed) >= max_components:
        limit_hit = True
        break
    label = actor.get_actor_label()
    path = actor.get_path_name()
    if actor_filter and actor_filter not in label and actor_filter not in path:
        continue
    for component in actor.get_components_by_class(unreal.ActorComponent):
        class_name = component.get_class().get_name()
        if "PCG" not in class_name:
            continue
        if len(processed) >= max_components:
            limit_hit = True
            break
        called = []
        for method_name in method_candidates:
            method = getattr(component, method_name, None)
            if not method:
                continue
            try:
                method()
                called.append(method_name)
            except Exception as exc:
                errors.append({{
                    "actor": label,
                    "component": component.get_name(),
                    "method": method_name,
                    "error": str(exc),
                }})
        processed.append({{
            "actor": label,
            "component": component.get_name(),
            "component_class": class_name,
            "called_methods": called,
        }})
RESULT = {{
    "success": True,
    "native_command_used": False,
    "native_command": {native_command!r},
    "native_response": {native_response!r},
    "fallback_reason": "native unavailable or no matching native components",
    "wait_requested": {bool(wait_until_complete)!r},
    "wait_supported": False,
    "wait_note": "Python fallback does not block for PCG completion because that can stall editor ticking; use native refresh_pcg_components for wait_until_complete.",
    "count": len(processed),
    "component_limit_hit": limit_hit,
    "processed": processed,
    "errors": errors,
}}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def inspect_pcg_component_outputs(
        ctx: Context,
        actor_label: str = "",
        actor_name: str = "",
        actor_path: str = "",
        actor_tag: str = "",
        actor_label_prefix: str = "",
        component_name: str = "",
        component_tag: str = "",
        selected_only: bool = False,
        max_components: int = 16,
        max_tagged_data: int = 64,
        max_points: int = 8,
        include_point_samples: bool = True,
    ) -> Dict[str, Any]:
        """
        Inspect generated PCG component output data and sample point payloads.

        Args:
            actor_label: Exact actor label filter.
            actor_name: Exact actor object name filter.
            actor_path: Exact actor path filter.
            actor_tag: Actor tag filter.
            actor_label_prefix: Actor label prefix filter.
            component_name: Exact PCG component name filter.
            component_tag: PCG component tag filter.
            selected_only: If true, inspect selected actors only.
            max_components: Maximum PCG components to inspect.
            max_tagged_data: Maximum tagged output data entries per component.
            max_points: Maximum point samples per point data entry.
            include_point_samples: Include point transform/density/bounds/color samples.
        """
        try:
            command_name = "inspect_pcg_component_outputs"
            params = {
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_path": actor_path,
                "actor_tag": actor_tag,
                "actor_label_prefix": actor_label_prefix,
                "component_name": component_name,
                "component_tag": component_tag,
                "selected_only": selected_only,
                "max_components": max_components,
                "max_tagged_data": max_tagged_data,
                "max_points": max_points,
                "include_point_samples": include_point_samples,
            }
            response = send_pcg_command(command_name, params)
            if _bridge_response_unknown_command(response, command_name):
                return tool_error(
                    (
                        "The running UnrealMCP editor bridge does not expose "
                        "inspect_pcg_component_outputs yet. Rebuild the UnrealMCP "
                        "plugin and restart the editor."
                    ),
                    error_code="native_pcg_component_output_inspect_unavailable",
                    native_response=response,
                )
            return response
        except Exception as e:
            error_msg = f"Error inspecting PCG component outputs: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def set_spline_component_points(
        ctx: Context,
        points: List[List[float]],
        actor_label: str = "",
        actor_name: str = "",
        actor_tag: str = "",
        actor_label_prefix: str = "",
        component_name: str = "Road_SourceSpline",
        component_tag: str = "",
        selected_only: bool = False,
        closed_loop: bool = False,
        native_first: bool = True,
    ) -> Dict[str, Any]:
        """
        Set points on a SplineComponent, preferring the native C++ bridge helper.

        Args:
            points: World-space points as [[x, y, z], ...].
            actor_label: Exact actor label filter.
            actor_name: Exact actor object name filter.
            actor_tag: Actor tag filter.
            actor_label_prefix: Actor label prefix filter.
            component_name: Spline component name, default Road_SourceSpline.
            component_tag: Spline component tag filter.
            selected_only: If true, process the selected actor.
            closed_loop: Set the spline closed-loop state.
            native_first: Prefer the native C++ bridge command when available.
        """
        native_command = "set_spline_component_points"
        if not points or len(points) < 2:
            return tool_error(
                "points must contain at least two [x, y, z] values",
                error_code="pcg_invalid_spline_points",
            )

        native_response = None
        if native_first:
            native_params = {
                "points": points,
                "actor_label": actor_label,
                "actor_name": actor_name,
                "actor_tag": actor_tag,
                "actor_label_prefix": actor_label_prefix,
                "component_name": component_name,
                "component_tag": component_tag,
                "selected_only": selected_only,
                "closed_loop": closed_loop,
            }
            native_response = send_pcg_command(native_command, native_params)
            if _bridge_response_succeeded(native_response):
                return _native_result_with_marker(native_response, native_command)
            if native_response and not _bridge_response_unknown_command(
                native_response, native_command
            ):
                return tool_error(
                    "Native spline point command failed before Python fallback",
                    error_code="native_spline_point_command_failed",
                    native_command_used=False,
                    native_command=native_command,
                    native_response=native_response,
                )

        code = f"""
import math
import unreal

points = {points!r}
actor_label = {actor_label!r}
actor_name = {actor_name!r}
actor_tag = {actor_tag!r}
actor_label_prefix = {actor_label_prefix!r}
component_name = {component_name!r}
component_tag = {component_tag!r}
selected_only = {bool(selected_only)!r}
closed_loop = {bool(closed_loop)!r}

def actor_tags(actor):
    try:
        return [str(item) for item in actor.get_editor_property("tags")]
    except Exception:
        return []

def component_tags(component):
    try:
        return [str(item) for item in component.get_editor_property("component_tags")]
    except Exception:
        return []

def actor_matches(actor):
    label = actor.get_actor_label()
    name = actor.get_name()
    if actor_label and label.lower() != actor_label.lower():
        return False
    if actor_name and name.lower() != actor_name.lower():
        return False
    if actor_tag and actor_tag not in actor_tags(actor):
        return False
    if actor_label_prefix and not label.lower().startswith(actor_label_prefix.lower()):
        return False
    return True

editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_selected_level_actors() if selected_only else editor_actor_subsystem.get_all_level_actors()
matches = [actor for actor in actors if actor_matches(actor)]
if not matches:
    raise RuntimeError("No matching actor found for spline point update")
if len(matches) > 1:
    raise RuntimeError("Multiple matching actors found: " + str([actor.get_actor_label() for actor in matches]))

actor = matches[0]
splines = [
    component
    for component in actor.get_components_by_class(unreal.SplineComponent)
    if not component.get_name().startswith("TRASH_")
]
if component_name:
    splines = [component for component in splines if component.get_name().lower() == component_name.lower()]
if component_tag:
    splines = [component for component in splines if component_tag in component_tags(component)]
if not splines:
    raise RuntimeError("No matching SplineComponent found on actor: " + actor.get_actor_label())

spline = splines[0]
actor.modify()
spline.modify()
try:
    spline.set_editor_property("input_spline_points_to_construction_script", True)
except Exception:
    pass
spline.clear_spline_points(False)
for point in points:
    spline.add_spline_point(unreal.Vector(float(point[0]), float(point[1]), float(point[2])), unreal.SplineCoordinateSpace.WORLD, False)
for index in range(len(points)):
    spline.set_spline_point_type(index, unreal.SplinePointType.LINEAR, False)
try:
    spline.set_closed_loop(closed_loop, False)
except Exception:
    pass
spline.update_spline()

final_points = []
deltas = []
for index in range(int(spline.get_number_of_spline_points())):
    location = spline.get_location_at_spline_point(index, unreal.SplineCoordinateSpace.WORLD)
    final = [float(location.x), float(location.y), float(location.z)]
    final_points.append(final)
    if index < len(points):
        expected = points[index]
        deltas.append(math.sqrt(
            (float(final[0]) - float(expected[0])) ** 2
            + (float(final[1]) - float(expected[1])) ** 2
            + (float(final[2]) - float(expected[2])) ** 2
        ))

RESULT = {{
    "success": True,
    "native_command_used": False,
    "native_command": {native_command!r},
    "native_response": {native_response!r},
    "fallback_reason": "native unavailable",
    "actor": actor.get_actor_label(),
    "actor_path": actor.get_path_name(),
    "component_name": spline.get_name(),
    "component_path": spline.get_path_name(),
    "requested_point_count": len(points),
    "final_point_count": int(spline.get_number_of_spline_points()),
    "spline_length": float(spline.get_spline_length()),
    "max_point_delta_cm": max(deltas) if deltas else 0.0,
    "points": final_points,
}}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def set_pcg_debug_enabled(
        ctx: Context,
        enabled: bool = False,
        actor_name: str = "",
        selected_only: bool = False,
    ) -> Dict[str, Any]:
        """
        Best-effort toggle for common PCG component debug/editor flags.

        Args:
            enabled: Desired debug state.
            actor_name: Optional actor label/path substring filter.
            selected_only: If true, process only selected actors.
        """
        code = f"""
import unreal
enabled = {bool(enabled)!r}
actor_filter = {actor_name!r}
selected_only = {bool(selected_only)!r}
property_names = [
    "debug",
    "bDebug",
    "b_debug",
    "show_debug",
    "bShowDebug",
    "b_show_debug",
    "debug_enabled",
    "bDebugEnabled",
    "b_debug_enabled",
]
editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_selected_level_actors() if selected_only else editor_actor_subsystem.get_all_level_actors()
changed = []
errors = []
for actor in actors:
    label = actor.get_actor_label()
    path = actor.get_path_name()
    if actor_filter and actor_filter not in label and actor_filter not in path:
        continue
    for component in actor.get_components_by_class(unreal.ActorComponent):
        class_name = component.get_class().get_name()
        if "PCG" not in class_name:
            continue
        component_changes = []
        for property_name in property_names:
            try:
                component.set_editor_property(property_name, enabled)
                component_changes.append(property_name)
            except Exception:
                pass
        if component_changes:
            changed.append({{
                "actor": label,
                "component": component.get_name(),
                "component_class": class_name,
                "properties": component_changes,
            }})
RESULT = {{"success": True, "count": len(changed), "changed": changed, "errors": errors}}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def resave_pcg_assets(ctx: Context, root_path: str = "/Game") -> Dict[str, Any]:
        """
        Resave PCG-related assets under a content path.

        Args:
            root_path: Unreal content path to scan, for example /Game.
        """
        code = f"""
import unreal
root_path = {root_path!r}
registry = unreal.AssetRegistryHelpers.get_asset_registry()
assets = registry.get_assets_by_path(root_path, recursive=True)
saved = []
errors = []
for asset_data in assets:
    class_name = str(asset_data.asset_class_path.asset_name)
    asset_name = str(asset_data.asset_name)
    package_name = str(asset_data.package_name)
    if "PCG" not in class_name and "PCG" not in asset_name:
        continue
    try:
        asset = unreal.EditorAssetLibrary.load_asset(package_name)
        if asset and unreal.EditorAssetLibrary.save_loaded_asset(asset):
            saved.append(package_name)
    except Exception as exc:
        errors.append({{"asset": package_name, "error": str(exc)}})
RESULT = {{"success": True, "count": len(saved), "saved": saved, "errors": errors}}
"""
        return _run_unreal_python_json(code)

    @mcp.tool()
    def create_pcg_graph_from_spec(
        ctx: Context,
        graph_spec: Dict[str, Any],
        overwrite_existing: bool = False,
        allow_overwrite_non_temp: bool = False,
        save: bool = True,
        dry_run: bool = False,
    ) -> Dict[str, Any]:
        """
        Create a PCG graph from a compact JSON-style spec.

        The spec supports:
        - asset_path: /Game package path for the graph
        - nodes: [{id, settings_class, title, position, settings, mesh_entries}]
        - edges: [{from, from_pin, to, to_pin}]
        - forbidden_dependency_prefixes: optional dependency guard after save

        Use /Game/_MCP_Temp for disposable validation graphs. Existing targets
        are never overwritten automatically: build to an unused candidate path,
        verify it, then perform any replacement manually. The overwrite flags
        remain only for wire compatibility and return a fail-closed response.
        """
        if not isinstance(graph_spec, dict):
            return tool_error(
                "graph_spec must be an object",
                error_code="pcg_invalid_graph_spec",
            )

        requested_asset_path = graph_spec.get("asset_path") or graph_spec.get(
            "graph_path"
        )
        if not isinstance(requested_asset_path, str) or not requested_asset_path:
            return tool_error(
                "graph_spec.asset_path must be a non-empty string",
                error_code="pcg_invalid_asset_path",
                mutation_attempted=False,
                mutation_performed=False,
            )
        asset_path_tchar_length = _unreal_tchar_length(requested_asset_path)
        if asset_path_tchar_length >= 1024:
            return tool_error(
                "graph_spec.asset_path exceeds the Unreal FName UTF-16 code-unit limit",
                error_code="pcg_asset_path_too_long",
                mutation_attempted=False,
                mutation_performed=False,
                max_length=1023,
                actual_tchar_length=asset_path_tchar_length,
            )

        try:
            reject_mcp_dependency_references("graph_spec", graph_spec)
            spec_json = json.dumps(graph_spec, ensure_ascii=False)
            code = """
import json
import math
import unreal

spec = json.loads(__SPEC_JSON__)
overwrite_existing = __OVERWRITE_EXISTING__
allow_overwrite_non_temp = __ALLOW_OVERWRITE_NON_TEMP__
save_graph = __SAVE_GRAPH__
dry_run = __DRY_RUN__

def normalize_package_path(path):
    if not path:
        return ""
    text = str(path).strip()
    if "." in text and text.startswith("/Game/"):
        text = text.split(".", 1)[0]
    return text.rstrip("/")

def unreal_tchar_length(value):
    return len(str(value).encode("utf-16-le", errors="surrogatepass")) // 2

def safe_unreal_string(value, field_name):
    text = str(value)
    if unreal_tchar_length(text) >= 1024:
        raise RuntimeError(str(field_name) + " exceeds Unreal FName UTF-16 code-unit limit")
    return text

def split_asset_path(path):
    package_path = safe_unreal_string(normalize_package_path(path), "asset_path")
    if not package_path.startswith("/Game/"):
        raise RuntimeError("asset_path must be a /Game package path")
    parent, _, asset_name = package_path.rpartition("/")
    if not parent or not asset_name:
        raise RuntimeError("asset_path must include a parent folder and asset name")
    safe_unreal_string(asset_name, "asset_path name")
    return parent, asset_name, package_path

def safe_unreal_name(value, field_name):
    return unreal.Name(safe_unreal_string(value, field_name))

def resolve_unreal_class(class_name):
    if not class_name:
        raise RuntimeError("settings_class is required")
    name = safe_unreal_string(class_name, "settings_class")
    if hasattr(unreal, name):
        return getattr(unreal, name)
    loaded = unreal.load_class(None, name)
    if loaded:
        return loaded
    if not name.startswith("PCG") and hasattr(unreal, "PCG" + name):
        return getattr(unreal, "PCG" + name)
    raise RuntimeError("Could not resolve Unreal class: " + name)

def pin_label(pin):
    props = pin.get_editor_property("properties")
    return str(props.get_editor_property("label"))

def edge_count_for_pin(node, pin_name, pin_key):
    for pin in node.get_editor_property(pin_key):
        if pin_label(pin) == pin_name:
            return len(pin.get_editor_property("edges"))
    return -1

def enum_value_from_string(current, value):
    enum_class = getattr(unreal, type(current).__name__, None)
    if not enum_class:
        return value
    token = str(value).split(".")[-1]
    candidates = [token, token.upper()]
    for candidate in candidates:
        if hasattr(enum_class, candidate):
            return getattr(enum_class, candidate)
    return value

def coerce_value(current, value):
    type_name = type(current).__name__
    if value is None:
        return None
    if isinstance(value, str) and type_name.startswith("PCG"):
        return enum_value_from_string(current, value)
    if type_name == "Vector" and isinstance(value, (list, tuple)) and len(value) >= 3:
        components = [float(value[0]), float(value[1]), float(value[2])]
        if not all(math.isfinite(component) for component in components):
            raise RuntimeError("Vector setting must contain only finite numbers")
        return unreal.Vector(components[0], components[1], components[2])
    if type_name == "Rotator" and isinstance(value, (list, tuple)) and len(value) >= 3:
        components = [float(value[0]), float(value[1]), float(value[2])]
        if not all(math.isfinite(component) for component in components):
            raise RuntimeError("Rotator setting must contain only finite numbers")
        return unreal.Rotator(components[0], components[1], components[2])
    if type_name == "Name":
        return safe_unreal_name(value, "Name setting")
    return value

def validate_setting_value_keys(value, field_name, sequence_item=False):
    if isinstance(value, float) and not math.isfinite(value):
        raise RuntimeError(str(field_name) + " must contain only finite numbers")
    if isinstance(value, str):
        if sequence_item:
            safe_unreal_string(value, field_name)
    elif isinstance(value, dict):
        for key, nested_value in value.items():
            safe_unreal_string(key, str(field_name) + " key")
            validate_setting_value_keys(nested_value, str(field_name) + "." + str(key))
    elif isinstance(value, (list, tuple)):
        for index, nested_value in enumerate(value):
            validate_setting_value_keys(
                nested_value,
                str(field_name) + "[" + str(index) + "]",
                True,
            )

def set_property_recursive(obj, property_name, value):
    property_name = safe_unreal_string(property_name, "settings property name")
    if isinstance(value, dict):
        try:
            nested = obj.get_editor_property(property_name)
        except Exception:
            nested = None
        if nested is None:
            obj.set_editor_property(property_name, value)
            return
        for key, nested_value in value.items():
            set_property_recursive(nested, key, nested_value)
        obj.set_editor_property(property_name, nested)
        return

    property_found = True
    try:
        current = obj.get_editor_property(property_name)
    except Exception:
        property_found = False
        current = None
    if property_found:
        value = coerce_value(current, value)
    obj.set_editor_property(property_name, value)

def apply_settings(settings_obj, settings_spec):
    applied = []
    errors = []
    for property_name, value in (settings_spec or {}).items():
        try:
            safe_property_name = safe_unreal_string(property_name, "settings property name")
            validate_setting_value_keys(value, "settings." + safe_property_name)
            set_property_recursive(settings_obj, safe_property_name, value)
            applied.append(safe_property_name)
        except Exception as exc:
            errors.append({"property": property_name, "error": str(exc)})
    return applied, errors

def static_mesh_from_path(path):
    path = safe_unreal_string(path, "static mesh path")
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError("Could not load static mesh: " + str(path))
    if mesh.get_class().get_name() != "StaticMesh":
        raise RuntimeError("Asset is not a StaticMesh: " + str(path))
    return mesh

def weighted_entry_snapshot(entry):
    descriptor = entry.get_editor_property("descriptor")
    mesh = descriptor.get_editor_property("static_mesh")
    return {
        "mesh": normalize_package_path(mesh.get_path_name()) if mesh else None,
        "weight": int(entry.get_editor_property("weight")),
        "cast_shadow": bool(descriptor.get_editor_property("cast_shadow")),
        "receives_decals": bool(descriptor.get_editor_property("receives_decals")),
        "override_materials": [
            normalize_package_path(item.get_path_name())
            for item in descriptor.get_editor_property("override_materials")
        ],
    }

def configure_static_mesh_spawner(settings_obj, node_spec):
    configured = {}
    errors = []
    selector_spec = node_spec.get("mesh_selector") or {}
    selector_type = str(selector_spec.get("type") or "").lower()

    if selector_type in ("by_attribute", "attribute"):
        try:
            settings_obj.set_mesh_selector_type(unreal.PCGMeshSelectorByAttribute)
        except Exception as exc:
            errors.append({"mesh_selector_type": "by_attribute", "error": str(exc)})
        selector = settings_obj.get_editor_property("mesh_selector_parameters")
        try:
            if selector.get_class().get_name() != "PCGMeshSelectorByAttribute":
                raise RuntimeError("PCG selector type did not change to PCGMeshSelectorByAttribute")
            expected_attribute_name = str(selector_spec.get("attribute_name", "Meshes"))
            selector.set_editor_property("attribute_name", safe_unreal_name(expected_attribute_name, "mesh_selector.attribute_name"))
            material_attrs = [
                safe_unreal_name(item, "mesh_selector.material_override_attributes")
                for item in selector_spec.get("material_override_attributes", [])
            ]
            selector.set_editor_property("material_override_attributes", material_attrs)
            selector.set_editor_property("use_attribute_material_overrides", bool(material_attrs))
            expected_attribute_snapshot = {
                "attribute_name": expected_attribute_name,
                "material_override_attributes": [str(item) for item in material_attrs],
                "use_attribute_material_overrides": bool(material_attrs),
            }
            actual_attribute_snapshot = {
                "attribute_name": str(selector.get_editor_property("attribute_name")),
                "material_override_attributes": [
                    str(item) for item in selector.get_editor_property("material_override_attributes")
                ],
                "use_attribute_material_overrides": bool(
                    selector.get_editor_property("use_attribute_material_overrides")
                ),
            }
            if actual_attribute_snapshot != expected_attribute_snapshot:
                raise RuntimeError("PCG attribute selector readback did not match requested fields")
            configured["mesh_selector"] = "by_attribute"
        except Exception as exc:
            errors.append({"mesh_selector": "by_attribute", "error": str(exc)})

    entries_spec = (
        node_spec.get("mesh_entries")
        or node_spec.get("static_mesh_entries")
        or node_spec.get("spawner_mesh_entries")
        or selector_spec.get("entries")
        or []
    )
    if entries_spec:
        try:
            settings_obj.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
            selector = settings_obj.get_editor_property("mesh_selector_parameters")
            if selector.get_class().get_name() != "PCGMeshSelectorWeighted":
                raise RuntimeError("PCG selector type did not change to PCGMeshSelectorWeighted")
            entries = []
            expected_entry_snapshots = []
            for entry_spec in entries_spec:
                mesh_path = entry_spec.get("mesh") or entry_spec.get("static_mesh") or entry_spec.get("path")
                mesh = static_mesh_from_path(mesh_path)
                descriptor = unreal.PCGSoftISMComponentDescriptor()
                descriptor.set_editor_property("static_mesh", mesh)
                expected_fields = {
                    "mesh": normalize_package_path(mesh.get_path_name()),
                    "weight": int(entry_spec.get("weight", 1)),
                }
                if "cast_shadow" in entry_spec:
                    expected_fields["cast_shadow"] = bool(entry_spec["cast_shadow"])
                    descriptor.set_editor_property("cast_shadow", expected_fields["cast_shadow"])
                if "receives_decals" in entry_spec:
                    expected_fields["receives_decals"] = bool(entry_spec["receives_decals"])
                    descriptor.set_editor_property("receives_decals", expected_fields["receives_decals"])
                if "override_materials" in entry_spec:
                    materials = []
                    for material_path in entry_spec.get("override_materials") or []:
                        material = unreal.load_asset(
                            safe_unreal_string(material_path, "override material path")
                        )
                        if not material:
                            raise RuntimeError("Could not load override material: " + str(material_path))
                        materials.append(material)
                    expected_fields["override_materials"] = [
                        normalize_package_path(material.get_path_name()) for material in materials
                    ]
                    descriptor.set_editor_property("override_materials", materials)
                entry = unreal.PCGMeshSelectorWeightedEntry()
                entry.set_editor_property("descriptor", descriptor)
                entry.set_editor_property("weight", expected_fields["weight"])
                entry_snapshot = weighted_entry_snapshot(entry)
                for field_name, expected_value in expected_fields.items():
                    if entry_snapshot.get(field_name) != expected_value:
                        raise RuntimeError(
                            "PCG weighted entry readback did not match " + str(field_name)
                        )
                entries.append(entry)
                expected_entry_snapshots.append(entry_snapshot)
            selector.set_editor_property("mesh_entries", entries)
            actual_entry_snapshots = [
                weighted_entry_snapshot(item)
                for item in selector.get_editor_property("mesh_entries")
            ]
            if actual_entry_snapshots != expected_entry_snapshots:
                raise RuntimeError("PCG weighted selector entry readback mismatch")
            configured["weighted_mesh_entries"] = len(entries)
        except Exception as exc:
            errors.append({"mesh_entries": "weighted", "error": str(exc)})
    return configured, errors

def graph_dependencies(package_path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )
    initial_query_error = None
    refresh_error = None
    try:
        dependencies = registry.get_dependencies(package_path, options)
    except Exception as exc:
        dependencies = None
        initial_query_error = str(exc)[:512]
    if dependencies is None:
        try:
            registry.scan_modified_asset_files([str(package_path)])
        except Exception as exc:
            refresh_error = str(exc)[:512]
        if refresh_error is None:
            try:
                dependencies = registry.get_dependencies(package_path, options)
            except Exception as exc:
                dependencies = None
                refresh_error = str(exc)[:512]
    if dependencies is None:
        return [], {
            "reason": "Asset Registry dependency query unavailable after one exact-package refresh",
            "package_path": str(package_path),
            "refresh_attempted": True,
            "initial_query_error": initial_query_error,
            "refresh_error": refresh_error,
        }
    return [str(item) for item in dependencies], None

MAX_ASSET_REGISTRY_DESCRIPTION_CHARS = 800

def unsafe_asset_registry_descriptions(graph):
    offenders = []

    def read_description(obj):
        if not obj:
            return ""
        try:
            value = obj.get_editor_property("description")
        except Exception:
            try:
                value = obj.description
            except Exception:
                return ""
        return str(value or "")

    def add_if_long(scope, obj):
        text = read_description(obj)
        text_tchar_length = unreal_tchar_length(text)
        if text_tchar_length > MAX_ASSET_REGISTRY_DESCRIPTION_CHARS:
            offenders.append({
                "scope": scope,
                "length": text_tchar_length,
                "tchar_length": text_tchar_length,
                "codepoint_length": len(text),
                "preview": text[:160],
            })

    add_if_long("graph", graph)
    try:
        graph_nodes = list(graph.get_editor_property("nodes"))
    except Exception:
        graph_nodes = []
    for node in graph_nodes:
        try:
            title = str(node.get_editor_property("node_title")) or node.get_name()
        except Exception:
            title = node.get_name()
        try:
            settings_obj = node.get_settings()
        except Exception:
            settings_obj = None
        add_if_long("settings:" + title, settings_obj)
    return offenders

def dependency_scan_skip_reason(graph):
    offenders = unsafe_asset_registry_descriptions(graph)
    if not offenders:
        return None
    return {
        "reason": "PCG asset description text exceeds the safe dependency scan limit",
        "max_safe_chars": MAX_ASSET_REGISTRY_DESCRIPTION_CHARS,
        "max_safe_tchar_length": MAX_ASSET_REGISTRY_DESCRIPTION_CHARS,
        "offenders": offenders,
    }

def read_asset_description(obj):
    if not obj:
        return ""
    try:
        value = obj.get_editor_property("description")
    except Exception:
        try:
            value = obj.description
        except Exception:
            return ""
    return str(value or "")

def write_asset_description(obj, text):
    safe_text = str(text or "")[:240].rstrip()
    try:
        obj.set_editor_property("description", safe_text)
        return True
    except Exception:
        try:
            obj.description = safe_text
            return True
        except Exception:
            return False

def sanitize_asset_registry_descriptions(graph):
    changes = []

    def sanitize(scope, obj):
        text = read_asset_description(obj)
        text_tchar_length = unreal_tchar_length(text)
        if text_tchar_length <= MAX_ASSET_REGISTRY_DESCRIPTION_CHARS:
            return
        if not write_asset_description(obj, text):
            changes.append({
                "scope": scope,
                "length": text_tchar_length,
                "tchar_length": text_tchar_length,
                "codepoint_length": len(text),
                "sanitized": False,
                "preview": text[:160],
            })
            return
        changes.append({
            "scope": scope,
            "length": text_tchar_length,
            "tchar_length": text_tchar_length,
            "codepoint_length": len(text),
            "new_length": unreal_tchar_length(read_asset_description(obj)),
            "sanitized": True,
            "preview": text[:160],
        })

    sanitize("graph", graph)
    try:
        graph_nodes = list(graph.get_editor_property("nodes"))
    except Exception:
        graph_nodes = []
    for node in graph_nodes:
        try:
            title = str(node.get_editor_property("node_title")) or node.get_name()
        except Exception:
            title = node.get_name()
        try:
            settings_obj = node.get_settings()
        except Exception:
            settings_obj = None
        sanitize("settings:" + title, settings_obj)
    return changes

asset_path = spec.get("asset_path") or spec.get("graph_path")
parent_path, asset_name, package_path = split_asset_path(asset_path)
nodes_spec = spec.get("nodes") or []
edges_spec = spec.get("edges") or []
forbidden_prefixes = list(spec.get("forbidden_dependency_prefixes") or [])

target_exists = bool(unreal.EditorAssetLibrary.does_asset_exist(package_path))
existing_asset = None
existing_asset_class = ""
can_execute = True
blocked_reason = ""
would_rebuild_existing = False

if target_exists:
    existing_asset = unreal.EditorAssetLibrary.load_asset(package_path)
    if existing_asset:
        existing_asset_class = existing_asset.get_class().get_name()
    can_execute = False
    if overwrite_existing:
        blocked_reason = (
            "Automatic PCG overwrite is disabled because transient validation cannot "
            "provide atomic rollback for the existing UObject and package. Build to an "
            "unused candidate path, verify it, then replace the target manually: " + package_path
        )
    else:
        blocked_reason = "Asset already exists: " + package_path

if dry_run:
    RESULT = {
        "success": True,
        "dry_run": True,
        "asset_path": package_path,
        "node_count": len(nodes_spec),
        "edge_count": len(edges_spec),
        "target_exists": target_exists,
        "existing_asset_class": existing_asset_class,
        "can_execute": can_execute,
        "blocked_reason": blocked_reason,
        "would_create_new": bool(can_execute and not target_exists),
        "would_rebuild_existing": bool(would_rebuild_existing),
        "would_overwrite": bool(would_rebuild_existing),
        "mutation_attempted": False,
        "mutation_performed": False,
        "changed": False,
    }
else:
    if not can_execute:
        RESULT = {
            "success": False,
            "error_code": (
                "pcg_existing_target_overwrite_unsupported"
                if target_exists and overwrite_existing
                else "pcg_asset_exists"
            ),
            "message": blocked_reason,
            "asset_path": package_path,
            "target_exists": target_exists,
            "existing_asset_class": existing_asset_class,
            "overwrite_existing_requested": bool(overwrite_existing),
            "mutation_attempted": False,
            "mutation_performed": False,
            "changed": False,
        }
        raise MCPResultReady()

    def collect_string_values(value):
        if isinstance(value, str):
            return [value]
        if isinstance(value, dict):
            return [
                item
                for nested_value in value.values()
                for item in collect_string_values(nested_value)
            ]
        if isinstance(value, (list, tuple)):
            return [
                item
                for nested_value in value
                for item in collect_string_values(nested_value)
            ]
        return []

    def build_graph(graph):
        node_lookup = {
            "__input__": graph.get_input_node(),
            "__output__": graph.get_output_node(),
            "input": graph.get_input_node(),
            "output": graph.get_output_node(),
        }
        created_nodes = []
        setting_errors = []
        spawner_errors = []
        reserved_identifiers = {"input", "output", "__input__", "__output__"}
        authored_identifiers = set()
        for index, node_spec in enumerate(nodes_spec):
            if not isinstance(node_spec, dict):
                setting_errors.append({
                    "node_index": index,
                    "error": "Each nodes entry must be an object",
                })
                continue
            node_id = str(node_spec.get("id") or node_spec.get("title") or "Node_" + str(index))
            title = str(node_spec.get("title") or node_id)
            requested_identifiers = {node_id, title}
            reserved_hits = sorted(requested_identifiers.intersection(reserved_identifiers))
            duplicate_hits = sorted(requested_identifiers.intersection(authored_identifiers))
            if reserved_hits or duplicate_hits:
                setting_errors.append({
                    "node_id": node_id,
                    "error": (
                        "Node id/title uses a reserved graph identifier: " + ", ".join(reserved_hits)
                        if reserved_hits
                        else "Duplicate node id/title: " + ", ".join(duplicate_hits)
                    ),
                })
                continue
            authored_identifiers.update(requested_identifiers)
            try:
                safe_unreal_string(title, "node title")
                settings_class = resolve_unreal_class(node_spec.get("settings_class") or node_spec.get("class"))
                result = graph.add_node_of_type(settings_class)
                if isinstance(result, tuple):
                    node, settings_obj = result[0], result[1]
                else:
                    node, settings_obj = result, result.get_settings()
                if title:
                    node.set_editor_property("node_title", safe_unreal_name(title, "node title"))
                position = node_spec.get("position") or node_spec.get("node_position") or [index * 320, 0]
                if not isinstance(position, (list, tuple)) or len(position) < 2:
                    raise RuntimeError("position must contain at least two values")
                node.set_node_position(int(position[0]), int(position[1]))
            except Exception as exc:
                setting_errors.append({"node_id": node_id, "error": str(exc)})
                continue

            applied, errors = apply_settings(settings_obj, node_spec.get("settings") or {})
            for error in errors:
                error["node_id"] = node_id
            setting_errors.extend(errors)

            spawner_config = {}
            if settings_obj.get_class().get_name() == "PCGStaticMeshSpawnerSettings":
                spawner_config, errors = configure_static_mesh_spawner(settings_obj, node_spec)
                for error in errors:
                    error["node_id"] = node_id
                spawner_errors.extend(errors)

            node_lookup[node_id] = node
            node_lookup[title] = node
            engine_node_name = node.get_name()
            if engine_node_name not in node_lookup:
                node_lookup[engine_node_name] = node
            created_nodes.append({
                "id": node_id,
                "title": title,
                "node_name": node.get_name(),
                "settings_class": settings_obj.get_class().get_name(),
                "position": list(node.get_node_position()),
                "settings_applied": applied,
                "spawner_config": spawner_config,
            })

        connected_edges = []
        edge_errors = []
        for edge_spec in edges_spec:
            if not isinstance(edge_spec, dict):
                edge_errors.append({"edge": edge_spec, "error": "Each edges entry must be an object"})
                continue
            from_id = str(edge_spec.get("from") or edge_spec.get("from_node") or "")
            to_id = str(edge_spec.get("to") or edge_spec.get("to_node") or "")
            from_node = node_lookup.get(from_id)
            to_node = node_lookup.get(to_id)
            if not from_node or not to_node:
                edge_errors.append({"edge": edge_spec, "error": "Unknown from/to node id"})
                continue
            try:
                from_pin = safe_unreal_string(
                    edge_spec.get("from_pin") or "Out",
                    "edge.from_pin",
                )
                to_pin = safe_unreal_string(
                    edge_spec.get("to_pin") or ("Out" if to_id in ("__output__", "output") else "In"),
                    "edge.to_pin",
                )
                before = edge_count_for_pin(from_node, from_pin, "output_pins")
                graph.add_edge(from_node, from_pin, to_node, to_pin)
                after = edge_count_for_pin(from_node, from_pin, "output_pins")
                if after <= before:
                    edge_errors.append({
                        "edge": edge_spec,
                        "error": "Graph add_edge did not create a new source-pin edge",
                        "from_pin_edge_count_before": before,
                        "from_pin_edge_count_after": after,
                    })
                else:
                    connected_edges.append({"from": from_id, "from_pin": from_pin, "to": to_id, "to_pin": to_pin})
            except Exception as exc:
                edge_errors.append({"edge": edge_spec, "error": str(exc)})

        return {
            "created_nodes": created_nodes,
            "connected_edges": connected_edges,
            "setting_errors": setting_errors,
            "spawner_errors": spawner_errors,
            "edge_errors": edge_errors,
        }

    # Only node configuration can introduce asset dependencies. Exclude the
    # target path and the guard's own prefix list from this preflight scan.
    spec_dependency_values = collect_string_values(nodes_spec)
    forbidden_spec_hits = sorted({
        value
        for value in spec_dependency_values
        for prefix in forbidden_prefixes
        if value.startswith(str(prefix))
    })
    validation_graph = unreal.new_object(unreal.PCGGraph)
    validation = build_graph(validation_graph)
    validation_errors = (
        validation["setting_errors"]
        + validation["spawner_errors"]
        + validation["edge_errors"]
    )
    preflight_policy_errors = []
    if forbidden_prefixes and not save_graph:
        preflight_policy_errors.append({
            "error": "save=True is required when forbidden_dependency_prefixes are requested",
        })
    if validation_errors or forbidden_spec_hits or preflight_policy_errors:
        RESULT = {
            "success": False,
            "error_code": "pcg_graph_spec_preflight_failed",
            "message": "PCG graph spec failed transient preflight; target was not modified",
            "asset_path": package_path,
            "preflight_graph_transient": True,
            "mutation_attempted": False,
            "mutation_performed": False,
            "changed": False,
            "created_nodes": validation["created_nodes"],
            "connected_edges": validation["connected_edges"],
            "setting_errors": validation["setting_errors"],
            "spawner_errors": validation["spawner_errors"],
            "edge_errors": validation["edge_errors"],
            "preflight_policy_errors": preflight_policy_errors,
            "forbidden_dependency_prefixes": forbidden_prefixes,
            "forbidden_dependency_hits": forbidden_spec_hits,
        }
    else:
        unreal.EditorAssetLibrary.make_directory(parent_path)
        graph = None
        target_mutation_started = False
        if graph:
            target_mutation_started = True
            for existing_node in list(graph.get_editor_property("nodes")):
                graph.remove_node(existing_node)

        asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
        if graph is None:
            graph = asset_tools.create_asset(asset_name, parent_path, unreal.PCGGraph, unreal.PCGGraphFactory())
            target_mutation_started = bool(graph)
        if not graph:
            raise RuntimeError("Failed to create PCG graph: " + package_path)

        built = build_graph(graph)
        sanitized_descriptions = sanitize_asset_registry_descriptions(graph)
        build_failed = bool(
            built["setting_errors"]
            or built["spawner_errors"]
            or built["edge_errors"]
        )

        saved = False
        save_failed = False
        save_error = ""
        if save_graph and not build_failed:
            try:
                saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(graph))
            except Exception as exc:
                save_error = str(exc)
                saved = False
            save_failed = not saved

        dependency_scan_warning = None
        dependencies = []
        if not build_failed and not save_failed and save_graph:
            dependency_scan_warning = dependency_scan_skip_reason(graph)
            if not dependency_scan_warning:
                dependencies, dependency_scan_warning = graph_dependencies(package_path)
        elif not save_graph:
            dependency_scan_warning = {
                "reason": "Asset Registry dependency verification requires save=True",
                "package_path": package_path,
            }

        forbidden_hits = sorted({
            str(dependency)
            for dependency in dependencies
            for prefix in forbidden_prefixes
            if str(dependency).startswith(str(prefix))
        })
        dependency_scan_required_but_skipped = bool(
            forbidden_prefixes and dependency_scan_warning
        )
        success = bool(
            not build_failed
            and not save_failed
            and not forbidden_hits
            and not dependency_scan_required_but_skipped
        )
        RESULT = {
            "success": success,
            "asset_path": package_path,
            "object_path": graph.get_path_name(),
            "created_new_asset": not target_exists,
            "rebuilt_existing": bool(would_rebuild_existing),
            "saved": saved,
            "save_requested": bool(save_graph),
            "created_nodes": built["created_nodes"],
            "connected_edges": built["connected_edges"],
            "setting_errors": built["setting_errors"],
            "spawner_errors": built["spawner_errors"],
            "edge_errors": built["edge_errors"],
            "sanitized_descriptions": sanitized_descriptions,
            "dependencies": dependencies,
            "dependency_scan_skipped": bool(dependency_scan_warning),
            "dependency_scan_warning": dependency_scan_warning,
            "forbidden_dependency_prefixes": forbidden_prefixes,
            "forbidden_dependency_hits": forbidden_hits,
            "mutation_attempted": True,
            "mutation_performed": bool(target_mutation_started),
        }
        if save_error:
            RESULT["save_error"] = save_error
        if success:
            RESULT["changed"] = bool(target_mutation_started)
            RESULT["verification"] = {
                "status": "unverified",
                "method": "transient_preflight_save_and_dependency_checks_without_full_property_readback",
                "checks": [
                    "transient_graph_build_completed",
                    "save_result_checked" if save_graph else "save_not_requested",
                    "dependency_scan_checked" if save_graph else "dependency_scan_not_applicable",
                ],
            }
        elif target_mutation_started:
            failed_stage = (
                "build_pcg_target"
                if build_failed
                else "save_pcg_target"
                if save_failed
                else "verify_pcg_dependencies"
            )
            RESULT.update({
                "error_code": (
                    "pcg_graph_save_failed"
                    if save_failed
                    else "pcg_graph_commit_verification_failed"
                ),
                "message": (
                    "PCG graph was modified in memory but could not be saved"
                    if save_failed
                    else "PCG graph commit or dependency verification failed after mutation"
                ),
                "partial_target_modified": True,
                "partial_mutation": True,
                "failed_stage": failed_stage,
                "failed_stage_effect": "mutation",
                "failed_stage_mutation_performed": True,
                "do_not_retry": True,
                "workflow_recovery_required": True,
                "recovery_guidance": (
                    "Disk state could not be verified. Inspect the target and revert or save it manually before retrying."
                    if save_failed
                    else "Revert or reload the dirty PCG graph before retrying. No save was attempted after a build failure."
                    if build_failed
                    else "Inspect the saved graph and its dependencies, then revert or correct it manually before retrying."
                ),
            })
            if save_failed:
                RESULT["mutation_state_unknown"] = True
            """
            code = code.replace(
                "__OVERWRITE_EXISTING__", repr(bool(overwrite_existing))
            )
            code = code.replace(
                "__ALLOW_OVERWRITE_NON_TEMP__", repr(bool(allow_overwrite_non_temp))
            )
            code = code.replace("__SAVE_GRAPH__", repr(bool(save)))
            code = code.replace("__DRY_RUN__", repr(bool(dry_run)))
            # Inject untrusted JSON last so placeholder-like user data remains
            # byte-for-byte data instead of being rewritten as template syntax.
            code = code.replace("__SPEC_JSON__", repr(spec_json))
            return _run_unreal_python_json(code)
        except Exception as e:
            error_msg = f"Error creating PCG graph from spec: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def audit_pcg_graph_contract(
        ctx: Context,
        graph_path: str,
        include_dependencies: bool = True,
        forbidden_dependency_prefixes: List[str] | None = None,
        fail_on_forbidden_dependencies: bool = True,
    ) -> Dict[str, Any]:
        """
        Read-only PCG graph contract audit.

        Reports node titles/classes, pins, edge endpoints, static mesh spawner
        selectors, actor tag selectors, dependencies, and forbidden dependency
        prefix hits.
        """
        if forbidden_dependency_prefixes is None:
            forbidden_dependency_prefixes = []
        payload_json = json.dumps(
            {
                "graph_path": graph_path,
                "include_dependencies": bool(include_dependencies),
                "forbidden_prefixes": forbidden_dependency_prefixes,
                "fail_on_forbidden": bool(fail_on_forbidden_dependencies),
            },
            ensure_ascii=False,
        )
        code = """
import json
import unreal

payload = json.loads(__PAYLOAD_JSON__)
graph_path = payload["graph_path"]
include_dependencies = bool(payload["include_dependencies"])
forbidden_prefixes = payload["forbidden_prefixes"]
fail_on_forbidden = bool(payload["fail_on_forbidden"])

def normalize_package_path(path):
    text = str(path).strip()
    if "." in text and text.startswith("/Game/"):
        text = text.split(".", 1)[0]
    return text.rstrip("/")

def unreal_tchar_length(value):
    return len(str(value).encode("utf-16-le", errors="surrogatepass")) // 2

def safe_unreal_string(value, field_name):
    text = str(value)
    if unreal_tchar_length(text) >= 1024:
        raise RuntimeError(str(field_name) + " exceeds Unreal FName UTF-16 code-unit limit")
    return text

def load_graph(path):
    path = safe_unreal_string(path, "graph_path")
    package_path = safe_unreal_string(normalize_package_path(path), "graph package path")
    graph = unreal.load_asset(path)
    if not graph and package_path:
        graph = unreal.load_asset(package_path)
    if not graph:
        raise RuntimeError("Could not load PCG graph: " + str(path))
    if "PCGGraph" not in graph.get_class().get_name():
        raise RuntimeError("Asset is not a PCG graph: " + graph.get_path_name())
    return graph, package_path or normalize_package_path(graph.get_path_name())

def pin_label(pin):
    props = pin.get_editor_property("properties")
    return str(props.get_editor_property("label"))

def pin_key(pin):
    return pin.get_path_name()

def collect_pin_rows(node, pin_key_name):
    rows = []
    for pin in node.get_editor_property(pin_key_name):
        rows.append({
            "label": pin_label(pin),
            "pin_name": pin.get_name(),
            "pin_path": pin.get_path_name(),
            "edge_count": len(pin.get_editor_property("edges")),
        })
    return rows

def node_title(node):
    value = node.get_editor_property("node_title")
    return str(value) if value else ""

def node_identity(node):
    title = node_title(node)
    return title or node.get_name()

def graph_dependencies(package_path):
    registry = unreal.AssetRegistryHelpers.get_asset_registry()
    options = unreal.AssetRegistryDependencyOptions(
        include_soft_package_references=True,
        include_hard_package_references=True,
        include_searchable_names=False,
        include_soft_management_references=False,
        include_hard_management_references=False,
    )
    initial_query_error = None
    refresh_error = None
    try:
        dependencies = registry.get_dependencies(package_path, options)
    except Exception as exc:
        dependencies = None
        initial_query_error = str(exc)[:512]
    if dependencies is None:
        try:
            registry.scan_modified_asset_files([str(package_path)])
        except Exception as exc:
            refresh_error = str(exc)[:512]
        if refresh_error is None:
            try:
                dependencies = registry.get_dependencies(package_path, options)
            except Exception as exc:
                dependencies = None
                refresh_error = str(exc)[:512]
    if dependencies is None:
        return [], {
            "reason": "Asset Registry dependency query unavailable after one exact-package refresh",
            "package_path": str(package_path),
            "refresh_attempted": True,
            "initial_query_error": initial_query_error,
            "refresh_error": refresh_error,
        }
    return [str(item) for item in dependencies], None

MAX_ASSET_REGISTRY_DESCRIPTION_CHARS = 800

def unsafe_asset_registry_descriptions(graph):
    offenders = []

    def read_description(obj):
        if not obj:
            return ""
        try:
            value = obj.get_editor_property("description")
        except Exception:
            try:
                value = obj.description
            except Exception:
                return ""
        return str(value or "")

    def add_if_long(scope, obj):
        text = read_description(obj)
        text_tchar_length = unreal_tchar_length(text)
        if text_tchar_length > MAX_ASSET_REGISTRY_DESCRIPTION_CHARS:
            offenders.append({
                "scope": scope,
                "length": text_tchar_length,
                "tchar_length": text_tchar_length,
                "codepoint_length": len(text),
                "preview": text[:160],
            })

    add_if_long("graph", graph)
    try:
        graph_nodes = list(graph.get_editor_property("nodes"))
    except Exception:
        graph_nodes = []
    for node in graph_nodes:
        try:
            title = str(node.get_editor_property("node_title")) or node.get_name()
        except Exception:
            title = node.get_name()
        try:
            settings_obj = node.get_settings()
        except Exception:
            settings_obj = None
        add_if_long("settings:" + title, settings_obj)
    return offenders

def dependency_scan_skip_reason(graph):
    offenders = unsafe_asset_registry_descriptions(graph)
    if not offenders:
        return None
    return {
        "reason": "PCG asset description text exceeds the safe dependency scan limit",
        "max_safe_chars": MAX_ASSET_REGISTRY_DESCRIPTION_CHARS,
        "max_safe_tchar_length": MAX_ASSET_REGISTRY_DESCRIPTION_CHARS,
        "offenders": offenders,
    }

def read_spawner(settings_obj):
    info = {}
    if settings_obj.get_class().get_name() != "PCGStaticMeshSpawnerSettings":
        return info
    selector = settings_obj.get_editor_property("mesh_selector_parameters")
    info["selector_class"] = selector.get_class().get_name()
    try:
        info["attribute_name"] = str(selector.get_editor_property("attribute_name"))
    except Exception:
        pass
    try:
        info["material_override_attributes"] = [
            str(item) for item in selector.get_editor_property("material_override_attributes")
        ]
    except Exception:
        pass
    entries = []
    try:
        for entry in selector.get_editor_property("mesh_entries"):
            descriptor = entry.get_editor_property("descriptor")
            mesh = descriptor.get_editor_property("static_mesh")
            entries.append({
                "mesh": mesh.get_path_name() if mesh else None,
                "weight": entry.get_editor_property("weight"),
            })
    except Exception:
        pass
    info["mesh_entries"] = entries
    return info

def read_actor_selector(settings_obj):
    info = {}
    try:
        selector = settings_obj.get_editor_property("actor_selector")
    except Exception:
        return info
    for property_name in [
        "actor_filter",
        "actor_selection",
        "actor_selection_tag",
        "must_overlap_self",
        "include_children",
        "component_selection",
        "component_selection_tag",
    ]:
        try:
            info[property_name] = str(selector.get_editor_property(property_name))
        except Exception:
            pass
    return info

graph, package_path = load_graph(graph_path)
node_by_pin_path = {}
pin_label_by_path = {}
pin_direction_by_path = {}
for node in list(graph.get_editor_property("nodes")) + [graph.get_input_node(), graph.get_output_node()]:
    for pin in node.get_editor_property("input_pins"):
        key = pin_key(pin)
        node_by_pin_path[key] = node_identity(node)
        pin_label_by_path[key] = pin_label(pin)
        pin_direction_by_path[key] = "input"
    for pin in node.get_editor_property("output_pins"):
        key = pin_key(pin)
        node_by_pin_path[key] = node_identity(node)
        pin_label_by_path[key] = pin_label(pin)
        pin_direction_by_path[key] = "output"

edges = {}
for node in list(graph.get_editor_property("nodes")) + [graph.get_input_node(), graph.get_output_node()]:
    for direction, pin_key_name in [("output", "output_pins"), ("input", "input_pins")]:
        for pin in node.get_editor_property(pin_key_name):
            for edge in pin.get_editor_property("edges"):
                edge_path = edge.get_path_name()
                row = edges.setdefault(edge_path, {"edge_path": edge_path})
                row[direction + "_node"] = node_identity(node)
                row[direction + "_pin"] = pin_label(pin)

nodes = []
for node in graph.get_editor_property("nodes"):
    settings_obj = node.get_settings()
    nodes.append({
        "node_name": node.get_name(),
        "title": node_title(node),
        "settings_class": settings_obj.get_class().get_name() if settings_obj else None,
        "position": list(node.get_node_position()),
        "input_pins": collect_pin_rows(node, "input_pins"),
        "output_pins": collect_pin_rows(node, "output_pins"),
        "spawner": read_spawner(settings_obj) if settings_obj else {},
        "actor_selector": read_actor_selector(settings_obj) if settings_obj else {},
    })

dependency_scan_warning = dependency_scan_skip_reason(graph) if include_dependencies else None
dependencies = []
if include_dependencies and not dependency_scan_warning:
    dependencies, dependency_scan_warning = graph_dependencies(package_path)
forbidden_hits = [
    dependency
    for dependency in dependencies
    for prefix in forbidden_prefixes
    if str(dependency).startswith(str(prefix))
]
dependency_scan_required_but_skipped = bool(forbidden_prefixes and dependency_scan_warning)
RESULT = {
    "success": not (
        (fail_on_forbidden and forbidden_hits)
        or (fail_on_forbidden and dependency_scan_required_but_skipped)
    ),
    "graph_path": package_path,
    "object_path": graph.get_path_name(),
    "node_count": len(nodes),
    "edge_count": len(edges),
    "nodes": nodes,
    "edges": list(edges.values()),
    "dependencies": dependencies,
    "dependency_scan_skipped": bool(dependency_scan_warning),
    "dependency_scan_warning": dependency_scan_warning,
    "forbidden_dependency_prefixes": forbidden_prefixes,
    "forbidden_dependency_hits": forbidden_hits,
}
"""
        code = code.replace("__PAYLOAD_JSON__", repr(payload_json))
        return _run_unreal_python_json(code)

    @mcp.tool()
    def validate_pcg_source_independence(
        ctx: Context,
        graph_path: str,
        forbidden_dependency_prefixes: List[str] | None = None,
        allowed_dependency_prefixes: List[str] | None = None,
    ) -> Dict[str, Any]:
        """
        Validate that a PCG graph avoids forbidden source dependencies.

        This is a focused wrapper around audit_pcg_graph_contract for promoted
        source-independent rebuilds. By default it forbids PCGStudy references.
        """
        if forbidden_dependency_prefixes is None:
            forbidden_dependency_prefixes = ["/Game/Cubeless/PCG/PCGStudy"]
        if allowed_dependency_prefixes is None:
            allowed_dependency_prefixes = ["/Script/PCG", "/Game/DreamscapeSeries"]

        audit = audit_pcg_graph_contract(
            ctx,
            graph_path=graph_path,
            include_dependencies=True,
            forbidden_dependency_prefixes=forbidden_dependency_prefixes,
            fail_on_forbidden_dependencies=True,
        )
        if not audit.get("success"):
            return audit

        dependencies = audit.get("dependencies", [])
        allowed_misses = [
            dependency
            for dependency in dependencies
            if not any(
                str(dependency).startswith(prefix)
                for prefix in allowed_dependency_prefixes
            )
        ]
        forbidden_hits = audit.get("forbidden_dependency_hits", [])
        return {
            "success": not forbidden_hits,
            "graph_path": audit.get("graph_path", graph_path),
            "node_count": audit.get("node_count", 0),
            "edge_count": audit.get("edge_count", 0),
            "dependency_count": len(dependencies),
            "dependencies": dependencies,
            "forbidden_dependency_prefixes": forbidden_dependency_prefixes,
            "forbidden_dependency_hits": forbidden_hits,
            "allowed_dependency_prefixes": allowed_dependency_prefixes,
            "dependencies_outside_allowed_prefixes": allowed_misses,
            "audit": audit,
        }

    @mcp.tool()
    def set_pcg_static_mesh_spawner_entries(
        ctx: Context,
        graph_path: str,
        node_id: str,
        mesh_entries: List[Dict[str, Any]] | None = None,
        selector_type: str = "weighted",
        attribute_name: str = "Meshes",
        material_override_attributes: List[str] | None = None,
        save: bool = True,
    ) -> Dict[str, Any]:
        """
        Configure a Static Mesh Spawner selector on one PCG graph node.

        This is a focused helper for the most common PCGStudy rebuild step:
        swapping source meshes for replacement meshes such as Dreamscape assets.
        Use selector_type="by_attribute" for the PCGStudy-compatible Meshes /
        Override Materials attribute contract.
        """
        mesh_entries = mesh_entries or []
        if selector_type not in {"weighted", "by_attribute", "attribute"}:
            return tool_error(
                "selector_type must be weighted or by_attribute",
                error_code="pcg_invalid_selector_type",
            )
        if selector_type == "weighted" and not mesh_entries:
            return tool_error(
                "mesh_entries must not be empty for weighted selector",
                error_code="pcg_empty_mesh_entries",
            )
        if material_override_attributes is None:
            material_override_attributes = []

        payload_json = json.dumps(
            {
                "graph_path": graph_path,
                "node_id": node_id,
                "mesh_entries": mesh_entries,
                "selector_type": selector_type,
                "attribute_name": attribute_name,
                "material_override_attributes": material_override_attributes,
                "save_graph": bool(save),
            },
            ensure_ascii=False,
        )

        code = """
import json
import unreal

payload = json.loads(__PAYLOAD_JSON__)
graph_path = payload["graph_path"]
node_id = payload["node_id"]
mesh_entries = payload["mesh_entries"]
selector_type = payload["selector_type"]
attribute_name = payload["attribute_name"]
material_override_attributes = payload["material_override_attributes"]
save_graph = bool(payload["save_graph"])

def normalize_package_path(path):
    text = str(path).strip()
    if "." in text and text.startswith("/Game/"):
        text = text.split(".", 1)[0]
    return text.rstrip("/")

def unreal_tchar_length(value):
    return len(str(value).encode("utf-16-le", errors="surrogatepass")) // 2

def safe_unreal_string(value, field_name):
    text = str(value)
    if unreal_tchar_length(text) >= 1024:
        raise RuntimeError(str(field_name) + " exceeds Unreal FName UTF-16 code-unit limit")
    return text

def safe_unreal_name(value, field_name):
    return unreal.Name(safe_unreal_string(value, field_name))

def load_graph(path):
    path = safe_unreal_string(path, "graph_path")
    package_path = safe_unreal_string(normalize_package_path(path), "graph package path")
    graph = unreal.load_asset(path) or unreal.load_asset(package_path)
    if not graph:
        raise RuntimeError("Could not load PCG graph: " + str(path))
    if "PCGGraph" not in graph.get_class().get_name():
        raise RuntimeError("Asset is not a PCG graph: " + graph.get_path_name())
    return graph

def node_title(node):
    value = node.get_editor_property("node_title")
    return str(value) if value else ""

def find_node(graph, identifier):
    matches = []
    for node in graph.get_editor_property("nodes"):
        settings = node.get_settings()
        keys = {
            node.get_name(),
            node.get_path_name(),
            node_title(node),
        }
        if settings:
            keys.add(settings.get_name())
            keys.add(settings.get_path_name())
        if str(identifier) in keys:
            matches.append(node)
    if not matches:
        raise RuntimeError("No PCG node matched node_id: " + str(identifier))
    if len(matches) > 1:
        raise RuntimeError("Multiple PCG nodes matched node_id: " + str(identifier))
    return matches[0]

def load_static_mesh(path):
    path = safe_unreal_string(path, "static mesh path")
    mesh = unreal.load_asset(path)
    if not mesh:
        raise RuntimeError("Could not load static mesh: " + str(path))
    if mesh.get_class().get_name() != "StaticMesh":
        raise RuntimeError("Asset is not a StaticMesh: " + str(path))
    return mesh

def build_entry(entry_spec):
    mesh_path = entry_spec.get("mesh") or entry_spec.get("static_mesh") or entry_spec.get("path")
    mesh = load_static_mesh(mesh_path)
    descriptor = unreal.PCGSoftISMComponentDescriptor()
    descriptor.set_editor_property("static_mesh", mesh)
    expected_fields = {
        "mesh": normalize_package_path(mesh.get_path_name()),
        "weight": int(entry_spec.get("weight", 1)),
    }
    if "cast_shadow" in entry_spec:
        expected_fields["cast_shadow"] = bool(entry_spec["cast_shadow"])
        descriptor.set_editor_property("cast_shadow", expected_fields["cast_shadow"])
    if "receives_decals" in entry_spec:
        expected_fields["receives_decals"] = bool(entry_spec["receives_decals"])
        descriptor.set_editor_property("receives_decals", expected_fields["receives_decals"])
    if "override_materials" in entry_spec:
        materials = []
        for material_path in entry_spec.get("override_materials") or []:
            material = unreal.load_asset(
                safe_unreal_string(material_path, "override material path")
            )
            if not material:
                raise RuntimeError("Could not load override material: " + str(material_path))
            materials.append(material)
        expected_fields["override_materials"] = [
            normalize_package_path(material.get_path_name()) for material in materials
        ]
        descriptor.set_editor_property("override_materials", materials)
    entry = unreal.PCGMeshSelectorWeightedEntry()
    entry.set_editor_property("descriptor", descriptor)
    entry.set_editor_property("weight", expected_fields["weight"])

    preflight_fields = {
        "mesh": normalize_package_path(descriptor.get_editor_property("static_mesh").get_path_name()),
        "weight": int(entry.get_editor_property("weight")),
        "cast_shadow": bool(descriptor.get_editor_property("cast_shadow")),
        "receives_decals": bool(descriptor.get_editor_property("receives_decals")),
        "override_materials": [
            normalize_package_path(item.get_path_name())
            for item in descriptor.get_editor_property("override_materials")
        ],
    }
    for field_name, expected_value in expected_fields.items():
        if preflight_fields.get(field_name) != expected_value:
            raise RuntimeError(
                "Weighted entry preflight readback mismatch for " + str(field_name)
            )
    return entry, preflight_fields

def selector_snapshot(selector):
    selector_class = selector.get_class().get_name()
    snapshot = {"selector_class": selector_class}
    if selector_class == "PCGMeshSelectorWeighted":
        entries = []
        for entry in selector.get_editor_property("mesh_entries"):
            descriptor = entry.get_editor_property("descriptor")
            mesh = descriptor.get_editor_property("static_mesh")
            entries.append({
                "mesh": normalize_package_path(mesh.get_path_name()) if mesh else None,
                "weight": int(entry.get_editor_property("weight")),
                "cast_shadow": bool(descriptor.get_editor_property("cast_shadow")),
                "receives_decals": bool(descriptor.get_editor_property("receives_decals")),
                "override_materials": [
                    normalize_package_path(item.get_path_name())
                    for item in descriptor.get_editor_property("override_materials")
                ],
            })
        snapshot["mesh_entries"] = entries
    elif selector_class == "PCGMeshSelectorByAttribute":
        snapshot["attribute_selector"] = {
            "attribute_name": str(selector.get_editor_property("attribute_name")),
            "material_override_attributes": [
                str(item) for item in selector.get_editor_property("material_override_attributes")
            ],
            "use_attribute_material_overrides": bool(
                selector.get_editor_property("use_attribute_material_overrides")
            ),
        }
    return snapshot

try:
    graph = load_graph(graph_path)
    node = find_node(graph, node_id)
    settings = node.get_settings()
    if not settings or settings.get_class().get_name() != "PCGStaticMeshSpawnerSettings":
        raise RuntimeError("Matched node is not PCGStaticMeshSpawnerSettings: " + node.get_name())

    selector_mode = "by_attribute" if selector_type in ("by_attribute", "attribute") else "weighted"
    validated_attribute_name = None
    validated_override_names = []
    validated_entries = []
    validated_entry_expectations = []
    if selector_mode == "by_attribute":
        validated_attribute_name = safe_unreal_name(attribute_name or "Meshes", "attribute_name")
        validated_override_names = [
            safe_unreal_name(item, "material_override_attributes")
            for item in material_override_attributes
        ]
    else:
        for entry_spec in mesh_entries:
            validated_entry, validated_entry_snapshot = build_entry(entry_spec)
            validated_entries.append(validated_entry)
            validated_entry_expectations.append(validated_entry_snapshot)
    before_selector_snapshot = selector_snapshot(
        settings.get_editor_property("mesh_selector_parameters")
    )
except Exception as exc:
    RESULT = {
        "success": False,
        "error_code": "pcg_spawner_preflight_failed",
        "message": "PCG spawner input validation failed before target mutation: " + str(exc),
        "error": "PCG spawner input validation failed before target mutation: " + str(exc),
        "graph_path": normalize_package_path(graph_path),
        "mutation_attempted": False,
        "mutation_performed": False,
        "changed": False,
        "preflight_error": str(exc),
    }
    raise MCPResultReady()

try:
    if selector_mode == "by_attribute":
        settings.set_mesh_selector_type(unreal.PCGMeshSelectorByAttribute)
        selector = settings.get_editor_property("mesh_selector_parameters")
        selector.set_editor_property("attribute_name", validated_attribute_name)
        selector.set_editor_property("material_override_attributes", validated_override_names)
        selector.set_editor_property("use_attribute_material_overrides", bool(validated_override_names))
    else:
        settings.set_mesh_selector_type(unreal.PCGMeshSelectorWeighted)
        selector = settings.get_editor_property("mesh_selector_parameters")
        selector.set_editor_property("mesh_entries", validated_entries)

    after_selector_snapshot = selector_snapshot(selector)
    readback = list(after_selector_snapshot.get("mesh_entries", []))
    attribute_readback = dict(after_selector_snapshot.get("attribute_selector", {}))
    changed = before_selector_snapshot != after_selector_snapshot
    verification_errors = []
    if selector_mode == "weighted":
        if after_selector_snapshot.get("selector_class") != "PCGMeshSelectorWeighted":
            verification_errors.append({
                "field": "selector_class",
                "expected": "PCGMeshSelectorWeighted",
                "actual": after_selector_snapshot.get("selector_class"),
            })
        if readback != validated_entry_expectations:
            verification_errors.append({
                "field": "mesh_entries",
                "expected": validated_entry_expectations,
                "actual": readback,
            })
    else:
        expected_attribute_readback = {
            "attribute_name": str(validated_attribute_name),
            "material_override_attributes": [str(item) for item in validated_override_names],
            "use_attribute_material_overrides": bool(validated_override_names),
        }
        if after_selector_snapshot.get("selector_class") != "PCGMeshSelectorByAttribute":
            verification_errors.append({
                "field": "selector_class",
                "expected": "PCGMeshSelectorByAttribute",
                "actual": after_selector_snapshot.get("selector_class"),
            })
        if attribute_readback != expected_attribute_readback:
            verification_errors.append({
                "field": "attribute_selector",
                "expected": expected_attribute_readback,
                "actual": attribute_readback,
            })
except Exception as exc:
    RESULT = {
        "success": False,
        "error_code": "pcg_spawner_mutation_failed",
        "message": "PCG spawner mutation or readback raised after the mutation attempt began: " + str(exc),
        "error": "PCG spawner mutation or readback raised after the mutation attempt began: " + str(exc),
        "graph_path": normalize_package_path(graph_path),
        "node": node.get_name(),
        "mutation_attempted": True,
        "mutation_performed": None,
        "changed": None,
        "failed_stage": "mutate_pcg_spawner_selector",
        "failed_stage_effect": "mutation",
        "mutation_state_unknown": True,
        "do_not_retry": True,
        "workflow_recovery_required": True,
        "recovery_guidance": "Reload or revert the dirty PCG graph and inspect the spawner selector before retrying.",
    }
    raise MCPResultReady()

if verification_errors:
    RESULT = {
        "success": False,
        "error_code": "pcg_spawner_readback_mismatch",
        "message": "PCG spawner setter returned without matching independent readback",
        "error": "PCG spawner setter returned without matching independent readback",
        "graph_path": normalize_package_path(graph_path),
        "node": node.get_name(),
        "selector_mode": selector_mode,
        "mesh_entries": readback,
        "attribute_selector": attribute_readback,
        "verification_errors": verification_errors,
        "before": before_selector_snapshot,
        "after": after_selector_snapshot,
        "observed_changed": changed,
        "mutation_attempted": True,
        "mutation_performed": None,
        "changed": None,
        "partial_mutation": changed,
        "failed_stage": "verify_pcg_spawner_selector",
        "failed_stage_effect": "mutation",
        "mutation_state_unknown": True,
        "do_not_retry": True,
        "workflow_recovery_required": True,
        "recovery_guidance": "Reload or revert the dirty PCG graph and inspect the spawner selector before retrying.",
        "verification": {
            "status": "failed",
            "method": "requested_vs_selector_readback",
        },
    }
    raise MCPResultReady()

saved = False
save_error = ""
if save_graph:
    try:
        saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(graph))
    except Exception as exc:
        save_error = str(exc)
        saved = False

if save_graph and not saved:
    RESULT = {
        "success": False,
        "error_code": "pcg_spawner_save_failed",
        "message": "PCG spawner changed in memory but the graph could not be saved",
        "error": "PCG spawner changed in memory but the graph could not be saved",
        "graph_path": normalize_package_path(graph_path),
        "node": node.get_name(),
        "selector_mode": selector_mode,
        "saved": False,
        "save_error": save_error or "save_loaded_asset returned false",
        "before": before_selector_snapshot,
        "after": after_selector_snapshot,
        "mutation_attempted": True,
        "mutation_performed": changed,
        "changed": changed,
        "partial_target_modified": changed,
        "partial_mutation": changed,
        "failed_stage": "save_pcg_spawner_graph",
        "failed_stage_effect": "mutation",
        "failed_stage_mutation_performed": changed,
        "mutation_state_unknown": changed,
        "do_not_retry": changed,
        "workflow_recovery_required": changed,
        "recovery_guidance": "Resolve package write access, then save or revert the dirty PCG graph manually before retrying.",
    }
else:
    RESULT = {
        "success": True,
        "graph_path": normalize_package_path(graph_path),
        "node": node.get_name(),
        "title": node_title(node),
        "selector_mode": selector_mode,
        "saved": saved,
        "mesh_entry_count": len(readback),
        "mesh_entries": readback,
        "attribute_selector": attribute_readback,
        "before": before_selector_snapshot,
        "after": after_selector_snapshot,
        "mutation_attempted": True,
        "mutation_performed": changed,
        "changed": changed,
        "verification": {
            "status": "passed",
            "method": "selector_readback_and_optional_save_result",
        },
    }
"""
        code = code.replace("__PAYLOAD_JSON__", repr(payload_json))
        return _run_unreal_python_json(code)

    @mcp.tool()
    def read_pcg_node_contract(
        ctx: Context,
        graph_path: str,
        node_id: str = "",
        include_dependencies: bool = False,
    ) -> Dict[str, Any]:
        """
        Read selected PCG node contract data from a graph.

        This is a stable readback helper for node title, settings class,
        position, pins, spawner selector state, and actor selector state. If
        node_id is empty, all authored nodes are returned.
        """
        audit = audit_pcg_graph_contract(
            ctx,
            graph_path=graph_path,
            include_dependencies=include_dependencies,
            forbidden_dependency_prefixes=[],
            fail_on_forbidden_dependencies=False,
        )
        if not audit.get("success"):
            return audit

        nodes = audit.get("nodes", [])
        if node_id:
            needle = str(node_id).lower()
            nodes = [
                node
                for node in nodes
                if needle
                in {
                    str(node.get("node_name", "")).lower(),
                    str(node.get("title", "")).lower(),
                    str(node.get("settings_class", "")).lower(),
                }
            ]
            if not nodes:
                return tool_error(
                    f"No PCG node matched node_id: {node_id}",
                    error_code="pcg_node_not_found",
                    graph_path=audit.get("graph_path", graph_path),
                    available_nodes=[
                        {
                            "node_name": node.get("node_name"),
                            "title": node.get("title"),
                            "settings_class": node.get("settings_class"),
                        }
                        for node in audit.get("nodes", [])
                    ],
                )

        return {
            "success": True,
            "graph_path": audit.get("graph_path", graph_path),
            "node_id": node_id,
            "node_count": len(nodes),
            "nodes": nodes,
            "dependencies": audit.get("dependencies", [])
            if include_dependencies
            else [],
        }

    @mcp.tool()
    def promote_pcg_temp_graph(
        ctx: Context,
        source_graph_path: str,
        target_graph_path: str,
        forbidden_dependency_prefixes: List[str] | None = None,
        allowed_dependency_prefixes: List[str] | None = None,
        overwrite_existing: bool = False,
        allow_non_temp_source: bool = False,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """
        Promote a validated temp PCG graph into a permanent content path.

        Defaults to dry-run and refuses non-temp sources unless explicitly
        allowed. No map switching or level mutation is performed.
        overwrite_existing is retained for wire compatibility but cannot
        replace a live target. Build to an unused candidate path, verify it,
        and perform any existing-target replacement manually.
        """
        if forbidden_dependency_prefixes is None:
            forbidden_dependency_prefixes = ["/Game/Cubeless/PCG/PCGStudy"]
        if allowed_dependency_prefixes is None:
            allowed_dependency_prefixes = ["/Script/PCG", "/Game/DreamscapeSeries"]

        validation = validate_pcg_source_independence(
            ctx,
            graph_path=source_graph_path,
            forbidden_dependency_prefixes=forbidden_dependency_prefixes,
            allowed_dependency_prefixes=allowed_dependency_prefixes,
        )
        if not validation.get("success"):
            return tool_error(
                "Source graph failed source-independence validation",
                error_code="pcg_source_independence_validation_failed",
                validation=validation,
                **composite_failure_safety_fields(
                    validation,
                    failed_stage="validate_pcg_source_independence",
                    mutation_relevant=False,
                ),
            )

        payload_json = json.dumps(
            {
                "source_graph_path": source_graph_path,
                "target_graph_path": target_graph_path,
                "overwrite_existing": bool(overwrite_existing),
                "allow_non_temp_source": bool(allow_non_temp_source),
                "dry_run": bool(dry_run),
                "validation": validation,
            },
            ensure_ascii=False,
        )

        code = """
import json
import unreal

payload = json.loads(__PAYLOAD_JSON__)
source_graph_path = payload["source_graph_path"]
target_graph_path = payload["target_graph_path"]
overwrite_existing = bool(payload["overwrite_existing"])
allow_non_temp_source = bool(payload["allow_non_temp_source"])
dry_run = bool(payload["dry_run"])
validation = payload["validation"]

def normalize_package_path(path):
    text = str(path).strip()
    if "." in text and text.startswith("/Game/"):
        text = text.split(".", 1)[0]
    return text.rstrip("/")

def unreal_tchar_length(value):
    return len(str(value).encode("utf-16-le", errors="surrogatepass")) // 2

def safe_unreal_string(value, field_name):
    text = str(value)
    if unreal_tchar_length(text) >= 1024:
        raise RuntimeError(str(field_name) + " exceeds Unreal FName UTF-16 code-unit limit")
    return text

def split_asset_path(path):
    package_path = safe_unreal_string(normalize_package_path(path), "target_graph_path")
    if not package_path.startswith("/Game/"):
        raise RuntimeError("target/source path must be a /Game package path")
    parent, _, asset_name = package_path.rpartition("/")
    if not parent or not asset_name:
        raise RuntimeError("Path must include a parent folder and asset name: " + package_path)
    safe_unreal_string(asset_name, "target graph asset name")
    return parent, asset_name, package_path

source_graph_path = safe_unreal_string(source_graph_path, "source_graph_path")
source_package = safe_unreal_string(normalize_package_path(source_graph_path), "source graph package path")
target_parent, target_name, target_package = split_asset_path(target_graph_path)
if not allow_non_temp_source and not source_package.startswith("/Game/_MCP_Temp/"):
    raise RuntimeError("Refusing to promote non-temp source without allow_non_temp_source=True: " + source_package)
if target_package.startswith("/Game/_MCP_Temp/"):
    raise RuntimeError("Promotion target must not be under /Game/_MCP_Temp: " + target_package)

source_graph = unreal.load_asset(source_graph_path) or unreal.load_asset(source_package)
if not source_graph:
    raise RuntimeError("Could not load source graph: " + source_graph_path)
if "PCGGraph" not in source_graph.get_class().get_name():
    raise RuntimeError("Source asset is not a PCG graph: " + source_graph.get_path_name())

target_exists = bool(unreal.EditorAssetLibrary.does_asset_exist(target_package))
can_promote = not target_exists
blocked_reason = ""
if target_exists:
    if overwrite_existing:
        blocked_reason = (
            "Live Python overwrite is disabled because it requires Force Delete. "
            "Remove or rename the target manually, then retry: " + target_package
        )
    else:
        blocked_reason = "Target graph already exists: " + target_package

if dry_run:
    RESULT = {
        "success": True,
        "dry_run": True,
        "source_graph_path": source_package,
        "target_graph_path": target_package,
        "target_exists": target_exists,
        "overwrite_existing_requested": bool(overwrite_existing),
        "can_promote": can_promote,
        "blocked_reason": blocked_reason,
        "would_create_new_target": can_promote,
        "would_overwrite": False,
        "mutation_attempted": False,
        "mutation_performed": False,
        "changed": False,
        "validation": validation,
    }
else:
    if not can_promote:
        RESULT = {
            "success": False,
            "error_code": "pcg_promotion_target_exists",
            "message": blocked_reason,
            "dry_run": False,
            "source_graph_path": source_package,
            "target_graph_path": target_package,
            "target_exists": target_exists,
            "overwrite_existing_requested": bool(overwrite_existing),
            "mutation_attempted": False,
            "mutation_performed": False,
            "changed": False,
            "validation": validation,
        }
        raise MCPResultReady()

    unreal.EditorAssetLibrary.make_directory(target_parent)
    duplicated = unreal.EditorAssetLibrary.duplicate_asset(source_package, target_package)
    if not duplicated:
        raise RuntimeError("Duplicate failed from " + source_package + " to " + target_package)
    save_error = ""
    try:
        saved = bool(unreal.EditorAssetLibrary.save_loaded_asset(duplicated))
    except Exception as exc:
        save_error = str(exc)
        saved = False
    if not saved:
        RESULT = {
            "success": False,
            "error_code": "pcg_promotion_save_failed",
            "message": "PCG candidate was duplicated in memory but could not be saved",
            "dry_run": False,
            "source_graph_path": source_package,
            "target_graph_path": target_package,
            "object_path": duplicated.get_path_name(),
            "created_new_target": True,
            "candidate_created_in_memory": True,
            "persisted": False,
            "saved": False,
            "save_error": save_error or "save_loaded_asset returned false",
            "mutation_attempted": True,
            "mutation_performed": True,
            "partial_target_modified": True,
            "partial_mutation": True,
            "failed_stage": "save_promoted_pcg_graph",
            "failed_stage_effect": "mutation",
            "failed_stage_mutation_performed": True,
            "mutation_state_unknown": True,
            "do_not_retry": True,
            "workflow_recovery_required": True,
            "recovery_guidance": "Resolve package write access, then save or remove the unsaved candidate manually before retrying.",
            "validation": validation,
        }
    else:
        RESULT = {
            "success": True,
            "dry_run": False,
            "source_graph_path": source_package,
            "target_graph_path": target_package,
            "object_path": duplicated.get_path_name(),
            "created_new_target": True,
            "saved": True,
            "mutation_attempted": True,
            "mutation_performed": True,
            "verification": {
                "status": "passed",
                "method": "duplicate_and_save_result",
            },
            "validation": validation,
        }
"""
        code = code.replace("__PAYLOAD_JSON__", repr(payload_json))
        return _run_unreal_python_json(code)

    @mcp.tool()
    def pcg_actor_smoke_test(
        ctx: Context,
        graph_path: str,
        actor_label: str = "",
        selected_only: bool = False,
        cleanup: bool = True,
        generate: bool = True,
        dry_run: bool = True,
    ) -> Dict[str, Any]:
        """
        Smoke-test a PCG graph against existing PCG components in the current level.

        The default dry-run only audits the graph and reports matching existing
        actors/components. With dry_run=False it runs cleanup/generate on matching
        PCG components. This tool does not switch maps and does not create actors.
        """
        audit = audit_pcg_graph_contract(
            ctx,
            graph_path=graph_path,
            include_dependencies=True,
            forbidden_dependency_prefixes=[],
            fail_on_forbidden_dependencies=False,
        )
        if not audit.get("success"):
            return tool_error(
                "PCG graph audit failed before actor smoke mutation",
                error_code="pcg_actor_smoke_preflight_failed",
                audit=audit,
                **composite_failure_safety_fields(
                    audit,
                    failed_stage="audit_pcg_graph_contract",
                    mutation_relevant=False,
                ),
            )

        payload_json = json.dumps(
            {
                "graph_path": graph_path,
                "actor_label": actor_label,
                "selected_only": bool(selected_only),
                "cleanup": bool(cleanup),
                "generate": bool(generate),
                "dry_run": bool(dry_run),
                "audit": audit,
            },
            ensure_ascii=False,
        )

        code = """
import json
import unreal

payload = json.loads(__PAYLOAD_JSON__)
graph_path = payload["graph_path"]
actor_label = payload["actor_label"]
selected_only = bool(payload["selected_only"])
cleanup = bool(payload["cleanup"])
generate = bool(payload["generate"])
dry_run = bool(payload["dry_run"])
audit = payload["audit"]

def normalize_package_path(path):
    text = str(path).strip()
    if "." in text and text.startswith("/Game/"):
        text = text.split(".", 1)[0]
    return text.rstrip("/")

def unreal_tchar_length(value):
    return len(str(value).encode("utf-16-le", errors="surrogatepass")) // 2

def safe_unreal_string(value, field_name):
    text = str(value)
    if unreal_tchar_length(text) >= 1024:
        raise RuntimeError(str(field_name) + " exceeds Unreal FName UTF-16 code-unit limit")
    return text

graph_path = safe_unreal_string(graph_path, "graph_path")
graph_package = safe_unreal_string(normalize_package_path(graph_path), "graph package path")
graph = unreal.load_asset(graph_path) or unreal.load_asset(graph_package)
if not graph:
    raise RuntimeError("Could not load PCG graph: " + str(graph_path))

editor_actor_subsystem = unreal.get_editor_subsystem(unreal.EditorActorSubsystem)
actors = editor_actor_subsystem.get_selected_level_actors() if selected_only else editor_actor_subsystem.get_all_level_actors()
matches = []
for actor in actors:
    label = actor.get_actor_label()
    path = actor.get_path_name()
    if actor_label and actor_label not in label and actor_label not in path:
        continue
    components = []
    for component in actor.get_components_by_class(unreal.ActorComponent):
        class_name = component.get_class().get_name()
        if "PCG" not in class_name:
            continue
        component_graph = None
        for prop_name in ["graph", "Graph"]:
            try:
                component_graph = component.get_editor_property(prop_name)
                break
            except Exception:
                pass
        graph_matches = False
        if component_graph:
            graph_matches = normalize_package_path(component_graph.get_path_name()) == graph_package
        if not graph_matches:
            continue
        components.append({
            "component": component.get_name(),
            "component_class": class_name,
            "component_path": component.get_path_name(),
            "graph_matches": graph_matches,
        })
    if components:
        matches.append({
            "actor": label,
            "actor_path": path,
            "components": components,
        })

if dry_run:
    RESULT = {
        "success": True,
        "dry_run": True,
        "graph_path": graph_package,
        "matching_actor_count": len(matches),
        "matching_component_count": sum(len(item["components"]) for item in matches),
        "matches": matches,
        "audit": audit,
        "note": "dry_run=True; no PCG component cleanup/generate was executed",
        "mutation_attempted": False,
        "mutation_performed": False,
        "changed": False,
    }
else:
    if not matches:
        raise RuntimeError("No existing PCG component in the current level references graph: " + graph_package)
    processed = []
    errors = []
    for match in matches:
        actor = unreal.load_object(None, match["actor_path"])
        if not actor:
            errors.append({"actor_path": match["actor_path"], "error": "Actor could not be reloaded"})
            continue
        for component_row in match["components"]:
            component = unreal.load_object(None, component_row["component_path"])
            if not component:
                errors.append({"component_path": component_row["component_path"], "error": "Component could not be reloaded"})
                continue
            called = []
            for method_name in (["cleanup"] if cleanup else []) + (["generate"] if generate else []):
                method = getattr(component, method_name, None)
                if not method:
                    continue
                try:
                    method()
                    called.append(method_name)
                except Exception as exc:
                    errors.append({
                        "actor": match["actor"],
                        "component": component_row["component"],
                        "method": method_name,
                        "error": str(exc),
                    })
            processed.append({
                "actor": match["actor"],
                "component": component_row["component"],
                "called_methods": called,
            })
    RESULT = {
        "success": len(errors) == 0,
        "dry_run": False,
        "graph_path": graph_package,
        "processed_count": len(processed),
        "processed": processed,
        "errors": errors,
        "audit": audit,
    }
"""
        code = code.replace("__PAYLOAD_JSON__", repr(payload_json))
        return _run_unreal_python_json(code)

    @mcp.tool()
    def resolve_pcg_graph(ctx: Context, graph_path: str) -> Dict[str, Any]:
        """
        Resolve a PCG graph by short name, package path, or object path.

        Args:
            graph_path: PCG graph name or path
        """
        try:
            return send_pcg_command("resolve_pcg_graph", {"graph_path": graph_path})
        except Exception as e:
            error_msg = f"Error resolving PCG graph: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def list_pcg_graph_nodes(
        ctx: Context,
        graph_path: str,
        node_type: str = "",
        title_contains: str = "",
        include_pins: bool = True,
    ) -> Dict[str, Any]:
        """
        List nodes, pins, and edges in a PCG graph.

        Args:
            graph_path: PCG graph name or path
            node_type: Optional settings class/name substring filter
            title_contains: Optional title substring filter
            include_pins: Include pin and edge metadata
        """
        try:
            params = {
                "graph_path": graph_path,
                "node_type": node_type,
                "title_contains": title_contains,
                "include_pins": include_pins,
            }
            return send_pcg_command("list_pcg_graph_nodes", params)
        except Exception as e:
            error_msg = f"Error listing PCG graph nodes: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def inspect_pcg_custom_hlsl_nodes(
        ctx: Context,
        graph_path: str,
        node_id: str = "",
        title_contains: str = "",
        include_pins: bool = True,
        include_declarations: bool = True,
        max_source_chars: int = 4000,
    ) -> Dict[str, Any]:
        """
        Inspect PCG Custom HLSL node source, functions, declarations, and pins.

        Args:
            graph_path: PCG graph name or path
            node_id: Optional node id/name/path/title filter
            title_contains: Optional title substring filter
            include_pins: Include graph pin and edge metadata
            include_declarations: Include reflected transient declaration text
            max_source_chars: Maximum characters per source/declaration field; 0 returns full text
        """
        try:
            command_name = "inspect_pcg_custom_hlsl_nodes"
            params = {
                "graph_path": graph_path,
                "node_id": node_id,
                "title_contains": title_contains,
                "include_pins": include_pins,
                "include_declarations": include_declarations,
                "max_source_chars": max_source_chars,
            }
            response = send_pcg_command(command_name, params)
            if _bridge_response_unknown_command(response, command_name):
                return tool_error(
                    (
                        "The running UnrealMCP editor bridge does not expose "
                        "inspect_pcg_custom_hlsl_nodes yet. Rebuild the UnrealMCP "
                        "plugin and restart the editor."
                    ),
                    error_code="native_pcg_custom_hlsl_inspect_unavailable",
                    native_response=response,
                )
            return response
        except Exception as e:
            error_msg = f"Error inspecting PCG Custom HLSL nodes: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def add_pcg_node(
        ctx: Context,
        graph_path: str,
        settings_class: str,
        node_position=None,
        node_title: str = "",
        settings: Dict[str, Any] | None = None,
    ) -> Dict[str, Any]:
        """
        Add a node to a PCG graph using a UPCGSettings class.

        Args:
            graph_path: PCG graph name or path
            settings_class: UPCGSettings class name or class path
            node_position: Optional [X, Y] editor position
            node_title: Optional authored node title
            settings: Optional settings property map
        """
        try:
            reject_mcp_dependency_reference("settings_class", settings_class)
            reject_mcp_dependency_references("settings", settings or {})
            if node_position is None:
                node_position = [0, 0]
            if settings is None:
                settings = {}
            params = {
                "graph_path": graph_path,
                "settings_class": settings_class,
                "node_position": node_position,
                "node_title": node_title,
                "settings": settings,
            }
            return send_pcg_command("add_pcg_node", params)
        except Exception as e:
            error_msg = f"Error adding PCG node: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def connect_pcg_nodes(
        ctx: Context,
        graph_path: str,
        from_node_id: str,
        from_pin: str,
        to_node_id: str,
        to_pin: str,
    ) -> Dict[str, Any]:
        """
        Connect two PCG graph nodes by pin label.

        Args:
            graph_path: PCG graph name or path
            from_node_id: Source node id/name/path
            from_pin: Source output pin label
            to_node_id: Target node id/name/path
            to_pin: Target input pin label
        """
        try:
            params = {
                "graph_path": graph_path,
                "from_node_id": from_node_id,
                "from_pin": from_pin,
                "to_node_id": to_node_id,
                "to_pin": to_pin,
            }
            reject_mcp_dependency_references("params", params)
            return send_pcg_command("connect_pcg_nodes", params)
        except Exception as e:
            error_msg = f"Error connecting PCG nodes: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def set_pcg_node_setting(
        ctx: Context,
        graph_path: str,
        node_id: str,
        property_name: str,
        value: Any,
    ) -> Dict[str, Any]:
        """
        Set a property on a PCG node settings object.

        Args:
            graph_path: PCG graph name or path
            node_id: Node id/name/path
            property_name: Settings property name
            value: New property value
        """
        try:
            params = {
                "graph_path": graph_path,
                "node_id": node_id,
                "property_name": property_name,
                "value": value,
            }
            return send_pcg_command("set_pcg_node_setting", params)
        except Exception as e:
            error_msg = f"Error setting PCG node setting: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def set_pcg_attribute_selector(
        ctx: Context,
        graph_path: str,
        node_id: str,
        selector_property_name: str,
        selector_type: str = "attribute",
        attribute_name: str = "",
        selected_property_name: str = "",
        domain_name: str = "",
        point_property: str = "",
        extra_property: str = "",
        extra_names: List[str] | None = None,
        reset_extra_names: bool = True,
        save: bool = True,
        dry_run: bool = False,
    ) -> Dict[str, Any]:
        """
        Set an FPCGAttributePropertySelector-derived property through the native bridge.

        This intentionally has no Unreal Python fallback because Python mutation
        of PCG selector structs can trip PythonScriptPlugin wrapper ensures and
        later crash the editor on shutdown.

        Args:
            graph_path: PCG graph name or path
            node_id: Node id/name/path/title
            selector_property_name: Settings selector property, e.g. WeightAttribute or weight_attribute
            selector_type: attribute, property, point_property, or extra_property
            attribute_name: Attribute name when selector_type is attribute
            selected_property_name: UE property name when selector_type is property
            domain_name: Optional metadata domain name
            point_property: EPCGPointProperties value when selector_type is point_property
            extra_property: EPCGExtraProperties value when selector_type is extra_property
            extra_names: Optional accessor names
            reset_extra_names: Reset selector extra names when changing the main selector
            save: Save the graph after changing it
            dry_run: Preview the requested selector without mutating the asset
        """
        try:
            params: Dict[str, Any] = {
                "graph_path": graph_path,
                "node_id": node_id,
                "selector_property_name": selector_property_name,
                "selector_type": selector_type,
                "reset_extra_names": reset_extra_names,
                "save": save,
                "dry_run": dry_run,
            }
            if attribute_name or selector_type == "attribute":
                params["attribute_name"] = attribute_name
            if selected_property_name or selector_type == "property":
                params["selected_property_name"] = selected_property_name
            if domain_name:
                params["domain_name"] = domain_name
            if point_property or selector_type == "point_property":
                params["point_property"] = point_property
            if extra_property or selector_type == "extra_property":
                params["extra_property"] = extra_property
            if extra_names is not None:
                params["extra_names"] = list(extra_names)

            command_name = "set_pcg_attribute_selector"
            response = send_pcg_command(command_name, params)
            if _bridge_response_unknown_command(response, command_name):
                return tool_error(
                    (
                        "The running UnrealMCP editor bridge does not expose "
                        "set_pcg_attribute_selector yet. Update/rebuild the UnrealMCP "
                        "plugin and restart the editor; this tool will not fall back "
                        "to unsafe Unreal Python selector mutation."
                    ),
                    error_code="native_pcg_attribute_selector_unavailable",
                    native_response=response,
                )
            return response
        except Exception as e:
            error_msg = f"Error setting PCG attribute selector: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def compile_or_notify_pcg_graph(ctx: Context, graph_path: str) -> Dict[str, Any]:
        """
        Notify a PCG graph of structural changes and request recompilation.

        Args:
            graph_path: PCG graph name or path
        """
        try:
            return send_pcg_command(
                "compile_or_notify_pcg_graph", {"graph_path": graph_path}
            )
        except Exception as e:
            error_msg = f"Error notifying PCG graph: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    @mcp.tool()
    def save_pcg_graph(ctx: Context, graph_path: str) -> Dict[str, Any]:
        """
        Save a loaded PCG graph asset.

        Args:
            graph_path: PCG graph name or path
        """
        try:
            return send_pcg_command("save_pcg_graph", {"graph_path": graph_path})
        except Exception as e:
            error_msg = f"Error saving PCG graph: {e}"
            logger.error(error_msg)
            return _pcg_exception_error(error_msg, e)

    logger.info("PCG tools registered successfully")
