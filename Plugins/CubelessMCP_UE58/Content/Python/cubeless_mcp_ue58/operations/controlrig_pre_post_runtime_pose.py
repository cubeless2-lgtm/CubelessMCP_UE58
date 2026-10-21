"""Pre/post transient ControlRig probe10, preserving the original public defaults.

The paired direct-gate module shares only JSON preflight and journal/adoption
logic. Native alone creates/initializes/executes rigs and samples their hierarchy.
"""
from typing import Any, Dict, List, Optional

from cubeless_mcp_ue58.compatibility.ue58 import controlrig_pre_post_runtime_pose as api
from . import state
from .state import OperationError
from .controlrig_direct_gate_probe import (_prepare_probe_arguments,
    _run_probe_action, _adopt_probe_rejection)

METHOD = "sample_controlrig_pre_post_runtime_pose"
STAGE_PREFIX = "controlrig_pre_post_runtime_pose"
PUBLIC_FIELDS = ("control_rig_path", "control_rig_class", "input_defaults", "curve_values",
    "cases", "sample_elements", "control_values", "execute_events",
    "should_trace_property", "interaction_location_property")


def _arguments(control_rig_path, control_rig_class, input_defaults, curve_values, cases,
        sample_elements, control_values, execute_events, should_trace_property, interaction_location_property):
    # Exact public wrapper behavior. Empty dictionaries must not become these defaults.
    if input_defaults is None:
        input_defaults = {"ShouldDoIKTrace": True, "InteractionWorldLocation": [80, -40, 80]}
    if curve_values is None:
        curve_values = {"IK_blend_interact": 1.0, "IKBlend_l": 1.0}
    values = dict(control_rig_path=control_rig_path, control_rig_class=control_rig_class,
        input_defaults=input_defaults, curve_values=curve_values, cases=cases,
        sample_elements=sample_elements, control_values=control_values, execute_events=execute_events,
        should_trace_property=should_trace_property, interaction_location_property=interaction_location_property)
    # Custom-case priority, fallback curves/properties, finite float checks and row
    # diagnostics are Native's original rules, not inferred from map truthiness here.
    return _prepare_probe_arguments(values, ("cases", "sample_elements", "execute_events"),
        ("input_defaults", "curve_values", "control_values"))


def _rejected(error):
    return _adopt_probe_rejection(state.new_result(), error, METHOD, STAGE_PREFIX)


def sample_controlrig_pre_post_runtime_pose(control_rig_path: str = "", control_rig_class: str = "",
        input_defaults: Optional[Dict[str, Any]] = None, curve_values: Optional[Dict[str, float]] = None,
        cases: Optional[List[Dict[str, Any]]] = None, sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None, execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation") -> Dict[str, Any]:
    try:
        arguments = _arguments(control_rig_path, control_rig_class, input_defaults, curve_values,
            cases, sample_elements, control_values, execute_events, should_trace_property, interaction_location_property)
    except Exception as error:
        return _rejected(error)
    return state.mutate(METHOD, arguments, None,
        lambda result: _run_probe_action(result, arguments, api.sample_controlrig_pre_post_runtime_pose, METHOD, STAGE_PREFIX))


def sample_controlrig_pre_post_runtime_pose_action(result, control_rig_path: str = "", control_rig_class: str = "",
        input_defaults: Optional[Dict[str, Any]] = None, curve_values: Optional[Dict[str, float]] = None,
        cases: Optional[List[Dict[str, Any]]] = None, sample_elements: Optional[List[Any]] = None,
        control_values: Optional[Dict[str, Any]] = None, execute_events: Optional[List[str]] = None,
        should_trace_property: str = "ShouldDoIKTrace",
        interaction_location_property: str = "InteractionWorldLocation", checkpoint_callback=None):
    try:
        arguments = _arguments(control_rig_path, control_rig_class, input_defaults, curve_values,
            cases, sample_elements, control_values, execute_events, should_trace_property, interaction_location_property)
    except Exception as error:
        _adopt_probe_rejection(result, error, METHOD, STAGE_PREFIX)
        result["data"]["parent_journal_only"] = True
        result["stage"] = "returned_" + STAGE_PREFIX + "_operation"
        (checkpoint_callback or state.checkpoint)(result)
        raise OperationError(result["error_code"], result["error"]) from error
    return _run_probe_action(result, arguments, api.sample_controlrig_pre_post_runtime_pose, METHOD, STAGE_PREFIX, checkpoint_callback)
