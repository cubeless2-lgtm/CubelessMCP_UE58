"""Pre/post probe binding10; public None defaults are supplied by the domain."""
import unreal

from .controlrig_direct_gate_probe import LIBRARY, _carrier, _invoke

METHOD = "sample_controlrig_pre_post_runtime_pose"
BINDINGS = (METHOD,)


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def sample_controlrig_pre_post_runtime_pose(control_rig_path="", control_rig_class="",
        input_defaults=None, curve_values=None, cases=None, sample_elements=None,
        control_values=None, execute_events=None, should_trace_property="ShouldDoIKTrace",
        interaction_location_property="InteractionWorldLocation"):
    return _invoke(METHOD, control_rig_path, control_rig_class, _carrier(input_defaults),
        _carrier(curve_values), _carrier(cases), _carrier(sample_elements),
        _carrier(control_values), _carrier(execute_events), should_trace_property,
        interaction_location_property)
