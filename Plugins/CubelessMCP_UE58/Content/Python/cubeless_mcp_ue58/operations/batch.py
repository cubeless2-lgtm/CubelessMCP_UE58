"""Explicit Blueprint/graph/component capabilities, one root journal, no nested mutation wrappers.

This does not implement arbitrary legacy commands or specialized domain batches.
All admission checks precede execution; asset-state guards run at each step so an
earlier create can supply the asset for a later compile. Never retries an action.
"""

import copy
import json
import math
import struct
import time

from cubeless_mcp_ue58.compatibility.ue58 import batch as engine
from cubeless_mcp_ue58.compatibility.ue58.blueprint import ensure_editor_idle
from . import blueprint, graph, component, variables, graph_variables, functions, blueprint_duplicates, events, pin_defaults
from .state import OperationError, checkpoint, fail, mutate, new_result

# Keep the v1 response/admission contract for existing Blueprint/graph callers;
# the additive component allowlist is identified separately in each response.
MANIFEST_VERSION = "blueprint-graph-v1"
MAX_COMMANDS = 100
MAX_INPUT_BYTES = 1024 * 1024
MAX_OBSERVED_RESULT_BYTES = 8 * 1024 * 1024
BETWEEN_STEP_BUDGET_SECONDS = 15.0
_REQUIRED = object()


def _entry(action, effect, fields, fixed=None):
    return {"action": action, "effect": effect, "fields": fields, "fixed": fixed or {}}


def _particle_receiver_action(name):
    # Resolve only these named extensions when called, keeping existing action
    # imports and normalized replay fingerprints unchanged.
    def dispatch(result, **arguments):
        from . import particle_receiver
        return getattr(particle_receiver, name)(result, **arguments)
    return dispatch


def _map_function_action(result, **arguments):
    from .maps import add_blueprint_map_function_node_action
    return add_blueprint_map_function_node_action(result, **arguments)


def _array_function_action(result, **arguments):
    from .arrays import add_blueprint_array_function_node_action
    return add_blueprint_array_function_node_action(result, **arguments)


def _ai_action(name):
    def dispatch(result, **arguments):
        from . import ai_blackboard
        return getattr(ai_blackboard, name + "_action")(result, **arguments)
    return dispatch


def _function_node_action(result, **arguments):
    from .function_node import add_blueprint_function_node_action
    return add_blueprint_function_node_action(result, **arguments)


def _pawn_action(result, **arguments):
    from .pawn import set_pawn_properties_action
    return set_pawn_properties_action(result, **arguments)


def _custom_event_metadata_action(result, **arguments):
    from .custom_event_metadata import set_blueprint_custom_event_metadata_action
    return set_blueprint_custom_event_metadata_action(result, **arguments)


def _category_sorting_action(result, **arguments):
    from .category_sorting import set_blueprint_category_sorting_action
    return set_blueprint_category_sorting_action(result, **arguments)


def _variable_removal_action(result, **arguments):
    from .variable_removal import remove_blueprint_variable_action
    return remove_blueprint_variable_action(result, **arguments)


def _variable_lifecycle_action(name):
    if name not in ("set_blueprint_variable_metadata", "set_blueprint_variable_order"):
        raise OperationError("batch_variable_lifecycle_action_invalid", "Only original metadata and order methods are admitted")
    def dispatch(result, **arguments):
        from . import variable_lifecycle
        return getattr(variable_lifecycle, name + "_action")(result, **arguments)
    return dispatch


def _data_asset_action(name):
    if name not in ("get_data_asset", "set_data_asset_properties", "create_data_asset"):
        raise OperationError("batch_data_asset_action_invalid", "Only the three original DataAsset names are admitted")
    def dispatch(result, **arguments):
        from . import data_asset
        parent_checkpoint = arguments.pop("checkpoint_callback", None)
        def observe(observed, stage=None):
            # One read callback has (result, stage), writers have (result).
            # Only a returned exact native producer may bypass generic intent.
            if observed["data"].get("native_result_returned") is True:
                observed["stage"] = "returned_data_asset_operation"
            if parent_checkpoint:
                parent_checkpoint(observed)
        try:
            return getattr(data_asset, name + "_action")(result, **arguments, checkpoint_callback=observe)
        finally:
            native = result["data"].get("native_outcome")
            if type(native) is dict:
                result["data"]["mutation_performed"] = native.get("changed") if native["change_state_known"] else None
    return dispatch


def _datatable_action(name):
    if name not in ("get_datatable_schema", "list_datatable_rows", "get_datatable_row",
            "add_or_update_datatable_row", "remove_datatable_row", "create_datatable"):
        raise OperationError("batch_datatable_action_invalid", "Only the six original DataTable names are admitted")
    def dispatch(result, **arguments):
        from . import datatable
        parent_checkpoint = arguments.pop("checkpoint_callback", None)
        def observe(observed, stage=None):
            # Read callbacks supply (result, stage), writer callbacks supply
            # (result). Preserve only these exact returned-native producers.
            if observed["data"].get("native_result_returned") is True:
                observed["stage"] = "returned_datatable_operation"
            if parent_checkpoint:
                parent_checkpoint(observed)
        try:
            return getattr(datatable, name + "_action")(result, **arguments, checkpoint_callback=observe)
        finally:
            native = result["data"].get("native_outcome")
            if type(native) is dict:
                result["data"]["mutation_performed"] = native.get("changed") if native["change_state_known"] else None
    return dispatch


def _rigid_constraint_chain_action(result, **arguments):
    from .rigid_constraint_chain import ensure_blueprint_rigid_constraint_chain_action
    return ensure_blueprint_rigid_constraint_chain_action(result, **arguments)


def _component_replacement_action(result, **arguments):
    from .component_replacement import replace_blueprint_component_class_action
    return replace_blueprint_component_class_action(result, **arguments)


def _blueprint_defaults_action(name):
    if name not in ("set_blueprint_actor_tick_settings", "set_skeletal_mesh_component_anim_defaults"):
        raise OperationError("batch_blueprint_defaults_action_invalid", "Only the exact two original Blueprint defaults actions are admitted")
    def dispatch(result, **arguments):
        from . import blueprint_defaults
        return getattr(blueprint_defaults, name + "_action")(result, **arguments)
    return dispatch


def _audio_action(name):
    if name not in ("list_audio_assets", "inspect_audio_asset", "create_sound_cue_from_wave", "create_basic_metasound_source"):
        raise OperationError("batch_audio_action_invalid", "Only the four exact original Audio names are admitted")
    def dispatch(result, **arguments):
        from . import audio
        parent_checkpoint = arguments.pop("checkpoint_callback", None)
        def observe(observed, stage=None):
            data = observed["data"]
            if name == "list_audio_assets":
                # Metadata inventory can be incomplete; it requests no asset
                # writer. Actual normalization/getter/query prefix stays in
                # data, separate from uninstrumented Registry/wrapping effects.
                data.update(native_mutation_attempted=False, mutation_performed=False,
                    mutation_state_unknown=False,
                    mutation_observation_scope="no_explicit_asset_writer_metadata_read_only")
                observed["stage"] = stage or "returned_audio_metadata_read"
            elif data.get("native_call_returned") is True:
                native = data.get("native_outcome")
                if type(native) is dict:
                    data["mutation_performed"] = native.get("changed") if native["change_state_known"] else None
                observed["stage"] = "returned_audio_operation"
            if parent_checkpoint:
                parent_checkpoint(observed)
        try:
            # Read callbacks are (result, stage); writers are (result). This
            # finite adapter accepts both without a nested public journal.
            return getattr(audio, name + "_action")(result, **arguments, checkpoint_callback=observe)
        finally:
            # Keep actual native changed/unknown flags through any later
            # observer/checkpoint exception instead of inferring from success.
            native = result["data"].get("native_outcome")
            if type(native) is dict:
                result["data"]["mutation_performed"] = native.get("changed") if native["change_state_known"] else None
    return dispatch


def _blueprint_make_struct_action(result, **arguments):
    from .blueprint_make_struct import add_blueprint_make_struct_node_action
    return add_blueprint_make_struct_node_action(result, **arguments)


def _material_compilation_action(result, **arguments):
    from .material_compilation import compile_and_save_material_action
    return compile_and_save_material_action(result, **arguments)


def _sequencer_editor_action(result, **arguments):
    from .sequencer_editor import open_level_sequence_in_editor_action
    return open_level_sequence_in_editor_action(result, **arguments)


def _sequencer_camera_action(result, **arguments):
    from .sequencer_camera import add_level_sequence_camera_cut_action
    return add_level_sequence_camera_cut_action(result, **arguments)


def _sequencer_subsequence_action(result, **arguments):
    from .sequencer_subsequence import add_level_sequence_subsequence_action
    return add_level_sequence_subsequence_action(result, **arguments)


def _sequencer_maintenance_action(name):
    if name not in ("set_level_sequence_playback_range", "remove_level_sequence_binding"):
        raise OperationError("batch_sequencer_maintenance_action_invalid", "Only the exact two original Sequencer maintenance names are admitted")
    def dispatch(result, **arguments):
        from . import sequencer_maintenance
        return getattr(sequencer_maintenance, name + "_action")(result, **arguments)
    return dispatch


def _sequencer_float_action(result, **arguments):
    from .sequencer_float import add_level_sequence_float_property_keys_action
    return add_level_sequence_float_property_keys_action(result, **arguments)


def _sequencer_action(name):
    def dispatch(result, **arguments):
        from . import sequencer
        return getattr(sequencer, name)(result, **arguments)
    return dispatch


def _loop_action(result, **arguments):
    from .loops import add_blueprint_loop_node_action
    return add_blueprint_loop_node_action(result, **arguments)


def _make_array_action(result, **arguments):
    from . import make_arrays
    from .variables import _json_value
    finite_values = ({"allow_finite_double": True} if str(arguments.get("element_type", "")).strip().casefold().replace("_", "").replace("-", "").replace(" ", "")
        in {"int64", "integer64", "float", "single", "double", "real", "text", "object", "class", "rotator", "transform", "struct", "scriptstruct"} else {})
    values = make_arrays.decode_registry_list(arguments.get("values"), "values", **finite_values)
    arguments["values"] = values
    arguments["element_defaults"] = make_arrays.decode_registry_list(
        arguments.get("element_defaults"), "element_defaults", **finite_values) if values is None else None
    position = arguments.get("node_position")
    arguments["node_position"] = _json_value(position, "node_position") if position is not None else None
    return make_arrays.add_blueprint_make_array_node_action(result, **arguments)


def _niagara_parameter_action(name):
    def dispatch(result, **arguments):
        from . import niagara_parameters
        return getattr(niagara_parameters, name)(result, **arguments)
    return dispatch


def _niagara_module_action(result, **arguments):
    from .niagara_modules import add_niagara_stock_module_to_stack_action
    return add_niagara_stock_module_to_stack_action(result, **arguments)


def _niagara_emitter_action(result, **arguments):
    from .niagara_emitters import duplicate_or_attach_emitter_from_source_action
    return duplicate_or_attach_emitter_from_source_action(result, **arguments)


def _niagara_stage_action(result, **arguments):
    from .niagara_simulation_stages import add_niagara_grid_simulation_stage_action
    return add_niagara_grid_simulation_stage_action(result, **arguments)


def _niagara_stage_settings_action(result, **arguments):
    from .niagara_simulation_stages import set_niagara_simulation_stage_settings_action
    return set_niagara_simulation_stage_settings_action(result, **arguments)


def _niagara_module_input_action(name):
    def dispatch(result, **arguments):
        from . import niagara_module_inputs
        return getattr(niagara_module_inputs, name)(result, **arguments)
    return dispatch


def _niagara_module_input_entry(name, value_key, value_admission):
    # Preserve three public contracts while sharing their unchanged selectors.
    fields = {
        "system_path": _S, "input_name": _S,
        "emitter_index": ((int, type(None)), None), "emitter_name": ((str, type(None)), None),
        "module_index": ((int, type(None)), None), "module_name": ((str, type(None)), None),
        "module_node_guid": ((str, type(None)), None), "overwrite_existing": ((bool,), False),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), False), "timeout_seconds": ((int, float), 10.0),
        "poll_interval_seconds": ((int, float), 0.1)}
    fields[value_key] = value_admission
    return _entry(_niagara_module_input_action(name + "_action"), "write", fields)


def _niagara_scratch_action(name):
    def dispatch(result, **arguments):
        from . import niagara_scratch
        return getattr(niagara_scratch, name + "_action")(result, **arguments)
    return dispatch


def _is_valid_exec_macro_action(result, **arguments):
    from .validity_macros import add_blueprint_is_valid_exec_macro_node_action
    return add_blueprint_is_valid_exec_macro_node_action(result, **arguments)


def _data_table_bool_select_action(result, **arguments):
    from .data_table_selects import add_blueprint_data_table_bool_select_node_action
    return add_blueprint_data_table_bool_select_node_action(result, **arguments)


def _data_table_row_node_action(result, **arguments):
    from .data_table_nodes import add_blueprint_get_data_table_row_node_action
    return add_blueprint_get_data_table_row_node_action(result, **arguments)


def _struct_break_action(result, **arguments):
    from .struct_breaks import add_blueprint_break_struct_node_action
    return add_blueprint_break_struct_node_action(result, **arguments)


def _material_collection_action(name):
    def dispatch(result, **arguments):
        from . import material_collections
        return getattr(material_collections, name + "_action")(result, **arguments)
    return dispatch


def _pcg_action(name):
    def dispatch(result, **arguments):
        from . import pcg_spec
        if name == "set_pcg_attribute_selector_action":
            for field in ("attribute_name", "selected_property_name", "domain_name", "point_property", "extra_property"):
                if arguments.get(field) is None:
                    arguments[field] = ""
        try:
            return getattr(pcg_spec, name)(result, **arguments)
        finally:
            # PCG observes allocation/setter/persistence dispatch separately;
            # keep the existing batch's stop/accounting contract truthful.
            observed = result["data"]
            if type(observed.get("mutation_attempted")) is bool:
                observed["native_mutation_attempted"] = observed["mutation_attempted"]
            if type(observed.get("outcome_unknown")) is bool:
                observed["mutation_state_unknown"] = observed["outcome_unknown"]
    return dispatch


# Each field: (exact accepted Python types, default or _REQUIRED). bool is not int.
_S = ((str,), _REQUIRED)
_I = ((int,), _REQUIRED)
_GRAPH = ((str,), "EventGraph")


def _niagara_module_batch_action(result, **arguments):
    from .niagara_module_batch import set_niagara_module_inputs_batch_action
    return set_niagara_module_inputs_batch_action(result, **arguments)



def _local_variable_action(result, **arguments):
    from .local_variable import add_blueprint_local_variable_action
    return add_blueprint_local_variable_action(result, **arguments)

def _skeletal_socket_action(name):
    def dispatch(result, **arguments):
        from . import skeletal_socket
        return getattr(skeletal_socket, name + "_action")(result, **arguments)
    return dispatch


def _skeletal_retarget_action(result, **arguments):
    from .skeletal_retarget import set_bone_translation_retargeting_action
    return set_bone_translation_retargeting_action(result, **arguments)


def _skeletal_physics_asset_action(result, **arguments):
    from .skeletal_physics import create_skeletal_physics_asset_action
    return create_skeletal_physics_asset_action(result, **arguments)


def _skeletal_constraint_action(name):
    def dispatch(result, **arguments):
        from .skeletal_constraint import create_skeletal_bone_constraint_fixture_action, inspect_skeletal_bone_constraint_fixture_action
        action = create_skeletal_bone_constraint_fixture_action if name == "create_skeletal_bone_constraint_fixture" else inspect_skeletal_bone_constraint_fixture_action
        return action(result, **arguments)
    return dispatch


def _material_reference_action(name):
    def dispatch(result, **arguments):
        from .material_references import replace_material_collection_references_action, replace_material_texture_references_action
        action = replace_material_collection_references_action if name == "replace_material_collection_references" else replace_material_texture_references_action
        return action(result, **arguments)
    return dispatch


def _material_collection_parameters_action(result, **arguments):
    from .material_collection_parameters import replace_material_collection_parameters_action
    return replace_material_collection_parameters_action(result, **arguments)


def _blueprint_selection_action(name):
    if name not in ("add_blueprint_select_node", "add_blueprint_switch_enum_node"):
        raise OperationError("batch_blueprint_selection_action_invalid", "Only the two exact original node constructors are admitted")
    def dispatch(result, **arguments):
        from . import blueprint_selection
        return getattr(blueprint_selection, name + "_action")(result, **arguments)
    return dispatch


def _material_collection_sync_action(result, **arguments):
    from .material_collection_sync import set_material_parameter_collection_sync_action
    return set_material_parameter_collection_sync_action(result, **arguments)


def _material_cache_action(result, **arguments):
    from .material_cache import refresh_material_cached_expression_data_action
    return refresh_material_cached_expression_data_action(result, **arguments)


def _material_sampler_action(result, **arguments):
    from .material_sampler import set_material_texture_sample_sampler_type_mcp_action
    return set_material_texture_sample_sampler_type_mcp_action(result, **arguments)


def _material_function_expansion_action(result, **arguments):
    from .material_function_expansion import expand_material_function_calls_action
    return expand_material_function_calls_action(result, **arguments)


def _blueprint_expression_action(name):
    if name not in ("add_blueprint_math_node", "add_blueprint_boolean_node"):
        raise OperationError("batch_blueprint_expression_action_invalid", "Only the two exact original Math/Boolean constructors are admitted")
    def dispatch(result, **arguments):
        from . import blueprint_expression
        return getattr(blueprint_expression, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_value_action(name):
    if name not in ("add_blueprint_compare_node", "add_blueprint_literal_node"):
        raise OperationError("batch_blueprint_value_action_invalid", "Only exact original Compare/Literal constructors are admitted")
    def dispatch(result, **arguments):
        from . import blueprint_value
        return getattr(blueprint_value, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_branching_action(name):
    if name not in ("add_blueprint_enum_literal_node", "add_blueprint_is_valid_node", "add_blueprint_switch_int_node"):
        raise OperationError("batch_blueprint_branching_action_invalid", "Only exact original enum-literal/validity/integer-switch constructors are admitted")
    def dispatch(result, **arguments):
        from . import blueprint_branching
        return getattr(blueprint_branching, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_construction_action(name):
    if name not in ("add_blueprint_get_subsystem_from_pc_node", "add_blueprint_construct_object_node"):
        raise OperationError("batch_blueprint_construction_action_invalid", "Only exact original subsystem-from-controller/construct-object constructors admitted")
    def dispatch(result, **arguments):
        from . import blueprint_construction
        return getattr(blueprint_construction, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_input_self_action(name):
    if name not in ("add_blueprint_input_action_node", "add_blueprint_self_reference"):
        raise OperationError("batch_blueprint_input_self_action_invalid", "Only exact original legacy input-action/self-reference constructors admitted")
    def dispatch(result, **arguments):
        from . import blueprint_input_self
        return getattr(blueprint_input_self, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_editor_subsystem_action(name):
    if name != "add_blueprint_get_editor_subsystem_node":
        raise OperationError("batch_blueprint_editor_subsystem_action_invalid", "Only exact original EditorSubsystem getter constructor admitted")
    def dispatch(result, **arguments):
        from . import blueprint_editor_subsystem
        return blueprint_editor_subsystem.add_blueprint_get_editor_subsystem_node_action(result, **arguments)
    return dispatch



def _blueprint_input_events_action(name):
    if name not in ("add_blueprint_input_axis_event_node", "add_blueprint_enhanced_input_action_node"):
        raise OperationError("batch_blueprint_input_events_action_invalid", "Only exact original Axis/Enhanced input-event constructors admitted")
    def dispatch(result, **arguments):
        from . import blueprint_input_events
        return getattr(blueprint_input_events, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_annotation_action(name):
    if name not in ("add_blueprint_comment_box", "set_blueprint_node_comment", "set_blueprint_function_description"):
        raise OperationError("batch_blueprint_annotation_action_invalid", "Only exact original annotation methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_annotation
        return getattr(blueprint_annotation, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_dispatcher_action(name):
    if name not in ("add_blueprint_event_dispatcher", "add_blueprint_event_dispatcher_call_node", "add_blueprint_event_dispatcher_bind_node"):
        raise OperationError("batch_blueprint_dispatcher_action_invalid", "Only exact original dispatcher declaration/call/bind methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_dispatcher
        return getattr(blueprint_dispatcher, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_widget_fallback_action(result, **arguments):
    from . import blueprint_widget_fallback
    return blueprint_widget_fallback.analyze_blueprint_widget_fallbacks_mcp_action(result, **arguments)


def _umg_text_binding_action(result, **arguments):
    from . import umg_text_binding
    return umg_text_binding.set_text_block_binding_action(result, **arguments)


def _umg_utility_action(name):
    if name not in ("add_checkbox_to_widget", "add_widget_to_viewport"):
        raise OperationError("batch_umg_utility_action_invalid", "Only checkbox native-command adapter and original viewport readiness methods admitted")
    def dispatch(result, **arguments):
        from . import umg_utility
        return getattr(umg_utility, name + "_action")(result, **arguments)
    return dispatch


def _umg_interaction_action(name):
    if name not in ("add_sliders_to_widget", "bind_widget_event"):
        raise OperationError("batch_umg_interaction_action_invalid", "Only original UMG Slider2/Bind4 methods admitted")
    def dispatch(result, **arguments):
        from . import umg_interaction
        return getattr(umg_interaction, name + "_action")(result, **arguments)
    return dispatch


def _umg_authoring_action(name):
    if name not in ("create_umg_widget_blueprint", "add_text_block_to_widget", "add_button_to_widget"):
        raise OperationError("batch_umg_authoring_action_invalid", "Only original UMG Create3/Text7/Button8 methods admitted")
    def dispatch(result, **arguments):
        from . import umg_authoring
        return getattr(umg_authoring, name + "_action")(result, **arguments)
    return dispatch


def _anim_blueprint_sample_action(result, **arguments):
    from .anim_blueprint_sample import create_anim_blueprint_sample_action
    return create_anim_blueprint_sample_action(result, **arguments)


def _controlrig_builders_action(method):
    def action(result, **arguments):
        from . import controlrig_builders
        return getattr(controlrig_builders, method + "_action")(result, **arguments)
    return action


def _anim_graph_controlrig_action(method):
    def action(result, **arguments):
        from . import anim_graph_controlrig
        return getattr(anim_graph_controlrig, method + "_action")(result, **arguments)
    return action


def _anim_graph_secondary_motion_action(result, **arguments):
    from . import anim_graph_secondary_motion
    return anim_graph_secondary_motion.ensure_anim_graph_secondary_motion_chain_action(result, **arguments)


def _anim_graph_trail_demo_action(result, **arguments):
    from . import anim_graph_trail_demo
    return anim_graph_trail_demo.ensure_anim_graph_trail_demo_action(result, **arguments)


def _anim_graph_modify_curve_demo_action(result, **arguments):
    from . import anim_graph_modify_curve_demo
    return anim_graph_modify_curve_demo.ensure_anim_graph_modify_curve_demo_action(result, **arguments)


def _anim_graph_bone_demo_action(result, **arguments):
    from . import anim_graph_bone_demo
    return anim_graph_bone_demo.ensure_anim_graph_modify_bone_demo_action(result, **arguments)


def _anim_graph_authoring_action(name):
    if name not in ("set_anim_graph_rigidbody_settings", "ensure_anim_graph_input_pose_passthrough"):
        raise OperationError("batch_anim_graph_authoring_action_invalid", "Only original RigidBody10/InputPose7 methods admitted")
    def dispatch(result, **arguments):
        from . import anim_graph_authoring
        return getattr(anim_graph_authoring, name + "_action")(result, **arguments)
    return dispatch


def _anim_graph_inspection_action(name):
    if name not in ("inspect_anim_graph_node_settings", "inspect_anim_graph_protected_topology"):
        raise OperationError("batch_anim_graph_inspection_action_invalid", "Only original AnimGraph Settings9/ProtectedTopology15 methods admitted")
    def dispatch(result, **arguments):
        from . import anim_graph_inspection
        return getattr(anim_graph_inspection, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_component_reference_action(result, **arguments):
    from . import blueprint_component_reference
    return blueprint_component_reference.add_blueprint_get_self_component_reference_action(result, **arguments)


def _physics_asset_inspection_action(result, **arguments):
    from . import physics_asset_inspection
    return physics_asset_inspection.inspect_physics_asset_constraints_guarded_action(result, **arguments)


def _animation_asset_metadata_action(result, **arguments):
    from . import animation_asset_metadata
    return animation_asset_metadata.inspect_animation_asset_metadata_guarded_action(result, **arguments)


def _controlrig_transient_probe_action(method):
    def action(result, **arguments):
        if method == "controlrig_direct_gate_probe":
            from . import controlrig_direct_gate_probe as domain
        else:
            from . import controlrig_pre_post_runtime_pose as domain
        return getattr(domain, method + "_action")(result, **arguments)
    return action


def _controlrig_solver_intents_action(result, **arguments):
    from . import controlrig_solver_intents
    return controlrig_solver_intents.validate_controlrig_solver_intents_action(result, **arguments)


def _chaos_solver_settings_action(method):
    def action(result, **arguments):
        from . import chaos_solver_settings
        return getattr(chaos_solver_settings, method + "_action")(result, **arguments)
    return action


def _chaos_cloth_component_action(method):
    def action(result, **arguments):
        from . import chaos_cloth_component
        return getattr(chaos_cloth_component, method + "_action")(result, **arguments)
    return action


def _chaos_cloth_authoring_action(method):
    def action(result, **arguments):
        from . import chaos_cloth_authoring
        return getattr(chaos_cloth_authoring, method + "_action")(result, **arguments)
    return action


def _legacy_chaos_clothing_action(method):
    def action(result, **arguments):
        from . import legacy_chaos_clothing
        return getattr(legacy_chaos_clothing, method + "_action")(result, **arguments)
    return action


def _chaos_cloth_inspection_action(method):
    def action(result, **arguments):
        from . import chaos_cloth_inspection
        return getattr(chaos_cloth_inspection, method + "_action")(result, **arguments)
    return action


def _actor_property_focus_action(method):
    def action(result, **arguments):
        from . import actor_property_focus
        return getattr(actor_property_focus, method + "_action")(result, **arguments)
    return action


def _editor_level_session_action(method):
    def action(result, **arguments):
        from . import editor_level_session
        return getattr(editor_level_session, method + "_action")(result, **arguments)
    return action


def _viewport_capture_action(method):
    def action(result, **arguments):
        from . import viewport_capture
        return getattr(viewport_capture, method + "_action")(result, **arguments)
    return action


def _chaos_cinematic_sequence_action(method):
    def action(result, **arguments):
        from . import chaos_cinematic_sequence
        return getattr(chaos_cinematic_sequence, method + "_action")(result, **arguments)
    return action


def _chaos_cache_sessions_action(method):
    def action(result, **arguments):
        from . import chaos_cache_sessions
        return getattr(chaos_cache_sessions, method + "_action")(result, **arguments)
    return action


def _chaos_destruction_performance_action(method):
    def action(result, **arguments):
        from . import chaos_destruction_performance
        return getattr(chaos_destruction_performance, method + "_action")(result, **arguments)
    return action


def _chaos_learning_action(method):
    def action(result, **arguments):
        from . import chaos_learning
        return getattr(chaos_learning, method + "_action")(result, **arguments)
    return action


def _chaos_trace_strain_action(method):
    def action(result, **arguments):
        from . import chaos_trace_strain
        return getattr(chaos_trace_strain, method + "_action")(result, **arguments)
    return action


def _chaos_geometry_authoring_action(method):
    def action(result, **arguments):
        from . import chaos_geometry_authoring
        return getattr(chaos_geometry_authoring, method + "_action")(result, **arguments)
    return action


def _gas_authoring_action(method):
    def action(result, **arguments):
        from . import gas_authoring
        return getattr(gas_authoring, method + "_action")(result, **arguments)
    return action


def _content_reconstruction_action(method):
    def action(result, **arguments):
        from . import content_reconstruction
        return getattr(content_reconstruction, method + "_action")(result, **arguments)
    return action


def _niagara_renderers_action(method):
    def action(result, **arguments):
        from . import niagara_renderers
        return getattr(niagara_renderers, method + "_action")(result, **arguments)
    return action


def _niagara_emitter_configuration_action(method):
    def action(result, **arguments):
        from . import niagara_emitter_configuration
        return getattr(niagara_emitter_configuration, method + "_action")(result, **arguments)
    return action


def _niagara_parameter_collections_action(method):
    def action(result, **arguments):
        from . import niagara_parameter_collections
        return getattr(niagara_parameter_collections, method + "_action")(result, **arguments)
    return action


def _niagara_authoring_inspection_action(method):
    def action(result, **arguments):
        from . import niagara_authoring_inspection
        return getattr(niagara_authoring_inspection, method + "_action")(result, **arguments)
    return action


def _pcg_inspection_action(method):
    def action(result, **arguments):
        from . import pcg_inspection
        return getattr(pcg_inspection, method + "_action")(result, **arguments)
    return action



def _niagara_factory_bootstrap_action(method):
    def action(result, **arguments):
        from . import niagara_factory_bootstrap
        return getattr(niagara_factory_bootstrap, method + "_action")(result, **arguments)
    return action



def _niagara_resource_bindings_action(method):
    def action(result, **arguments):
        from . import niagara_resource_bindings
        return getattr(niagara_resource_bindings, method + "_action")(result, **arguments)
    return action
def _niagara_scratch_stack_authoring_action(method):
    def action(result, **arguments):
        from . import niagara_scratch_stack_authoring
        return getattr(niagara_scratch_stack_authoring, method + "_action")(result, **arguments)
    return action
def _niagara_event_stage_authoring_action(method):
    def action(result, **arguments):
        from . import niagara_event_stage_authoring
        return getattr(niagara_event_stage_authoring, method + "_action")(result, **arguments)
    return action
def _niagara_promotion_action(method):
    def action(result, **arguments):
        from . import niagara_promotion
        return getattr(niagara_promotion, method + "_action")(result, **arguments)
    return action
def _niagara_sprite_preset_authoring_action(method):
    def action(result, **arguments):
        from . import niagara_sprite_preset_authoring
        return getattr(niagara_sprite_preset_authoring, method + "_action")(result, **arguments)
    return action
def _niagara_custom_hlsl_conversion_action(method):
    def action(result, **arguments):
        from . import niagara_custom_hlsl_conversion
        return getattr(niagara_custom_hlsl_conversion, method + "_action")(result, **arguments)
    return action
def _niagara_preview_lab_action(method):
    def action(result, **arguments):
        from . import niagara_preview_lab
        return getattr(niagara_preview_lab, method + "_action")(result, **arguments)
    return action
def _controlrig_recipe_authoring_action(method):
    def action(result, **arguments):
        from . import controlrig_recipe_authoring
        return getattr(controlrig_recipe_authoring, method + "_action")(result, **arguments)
    return action
def _texture_material_authoring_action(method):
    def action(result, **arguments):
        from . import texture_material_authoring
        return getattr(texture_material_authoring, method + "_action")(result, **arguments)
    return action
def _blueprint_resolution_connection_action(method):
    def action(result, **arguments):
        from . import blueprint_resolution_connection
        return getattr(blueprint_resolution_connection, method + "_action")(result, **arguments)
    return action
def _niagara_scratch_user_recipes_action(method):
    def action(result, **arguments):
        from . import niagara_scratch_user_recipes
        return getattr(niagara_scratch_user_recipes, method + "_action")(result, **arguments)
    return action


def _blueprint_property_graph_action(method):
    def action(result, **arguments):
        from . import blueprint_property_graph
        return getattr(blueprint_property_graph, method + "_action")(result, **arguments)
    return action



def _controlrig_recipe_planning_action(method):
    def action(result, **arguments):
        from . import controlrig_recipe_planning
        return getattr(controlrig_recipe_planning, method + "_action")(result, **arguments)
    return action

def _pcg_utilities_action(method):
    def action(result, **arguments):
        from . import pcg_utilities
        return getattr(pcg_utilities, method + "_action")(result, **arguments)
    return action

def _pcg_actor_smoke_action(result, **arguments):
    from . import pcg_actor_smoke
    return pcg_actor_smoke.pcg_actor_smoke_test_action(result, **arguments)

def _pcg_promotion_action(method):
    def action(result, **arguments):
        from . import pcg_promotion
        return getattr(pcg_promotion, method + "_action")(result, **arguments)
    return action

def _pcg_contract_authoring_action(method):
    def action(result, **arguments):
        from . import pcg_contract_authoring
        return getattr(pcg_contract_authoring, method + "_action")(result, **arguments)
    return action

def _pcg_component_generation_action(method):
    def action(result, **arguments):
        from . import pcg_component_generation
        return getattr(pcg_component_generation, method + "_action")(result, **arguments)
    return action

def _niagara_source_template_action(method):
    def action(result, **arguments):
        from . import niagara_source_template
        return niagara_source_template.create_niagara_system_from_source_template_action(result, **arguments)
    return action

def _niagara_promotion_manifest_action(method):
    def action(result, **arguments):
        from . import niagara_promotion_manifest
        return getattr(niagara_promotion_manifest, method + "_action")(result, **arguments)
    return action

def _niagara_system_analysis_action(method):
    def action(result, **arguments):
        from . import niagara_system_analysis
        return getattr(niagara_system_analysis, method + "_action")(result, **arguments)
    return action

def _pie_input_action(method):
    def action(result, **arguments):
        from . import pie_input
        return getattr(pie_input, method + "_action")(result, **arguments)
    return action

def _niagara_preview_workflows_action(method):
    def action(result, **arguments):
        from . import niagara_preview_workflows
        return getattr(niagara_preview_workflows, method + "_action")(result, **arguments)
    return action

def _blueprint_niagara_template_swap_action(method):
    def action(result, **arguments):
        from . import blueprint_niagara_template_swap
        return getattr(blueprint_niagara_template_swap, method + "_action")(result, **arguments)
    return action
def _pcg_graph_authoring_action(method):
    def action(result, **arguments):
        from . import pcg_graph_authoring
        return getattr(pcg_graph_authoring, method + "_action")(result, **arguments)
    return action










def _niagara_runtime_inspection_action(method):
    def action(result, **arguments):
        from . import niagara_runtime_inspection
        return getattr(niagara_runtime_inspection, method + "_action")(result, **arguments)
    return action


def _niagara_cleanup_action(method):
    def action(result, **arguments):
        from . import niagara_cleanup
        return getattr(niagara_cleanup, method + "_action")(result, **arguments)
    return action


def _niagara_input_parameters_action(method):
    def action(result, **arguments):
        from . import niagara_input_parameters
        return getattr(niagara_input_parameters, method + "_action")(result, **arguments)
    return action


def _niagara_module_input_paths_action(method):
    def action(result, **arguments):
        from . import niagara_module_input_paths
        return getattr(niagara_module_input_paths, method + "_action")(result, **arguments)
    return action


def _niagara_data_channel_authoring_action(method):
    def action(result, **arguments):
        from . import niagara_data_channel_authoring
        return getattr(niagara_data_channel_authoring, method + "_action")(result, **arguments)
    return action


def _geometry_collection_inspection_action(method):
    def action(result, **arguments):
        from . import geometry_collection_inspection
        return getattr(geometry_collection_inspection, method + "_action")(result, **arguments)
    return action


def _landscape_action(method):
    def action(result, **arguments):
        from . import landscape
        return getattr(landscape, method + "_action")(result, **arguments)
    return action


def _foliage_instances_action(method):
    def action(result, **arguments):
        from . import foliage_instances
        return getattr(foliage_instances, method + "_action")(result, **arguments)
    return action


def _spline_component_points_action(result, **arguments):
    from . import spline_component_points
    return spline_component_points.set_spline_component_points_action(result, **arguments)


def _blueprint_function_extraction_action(result, **arguments):
    from . import blueprint_function_extraction
    return blueprint_function_extraction.collapse_blueprint_nodes_to_function_action(result, **arguments)


def _anim_node_pre_post_runtime_pose_action(result, **arguments):
    from . import anim_node_pre_post_runtime_pose
    return anim_node_pre_post_runtime_pose.sample_anim_node_pre_post_runtime_pose_action(result, **arguments)


def _anim_bp_locomotion_driver_action(result, **arguments):
    from . import anim_bp_locomotion_driver
    return anim_bp_locomotion_driver.ensure_anim_bp_event_graph_locomotion_driver_action(result, **arguments)


def _anim_state_machine_variant_action(result, **arguments):
    from . import anim_state_machine_variant
    return anim_state_machine_variant.ensure_anim_state_machine_sample_variant_action(result, **arguments)


def _linked_anim_graph_variant_action(result, **arguments):
    from . import linked_anim_graph_variant
    return linked_anim_graph_variant.ensure_linked_anim_graph_sample_variant_action(result, **arguments)


def _blendspace_variant_action(result, **arguments):
    from . import blendspace_variant
    return blendspace_variant.ensure_blendspace_sample_variant_action(result, **arguments)


def _blendspace_pose_grid_action(result, **arguments):
    from . import blendspace_pose_grid
    return blendspace_pose_grid.sample_blendspace_runtime_pose_grid_action(result, **arguments)


def _skeletal_bone_sampling_action(result, **arguments):
    from . import skeletal_bone_sampling
    return skeletal_bone_sampling.sample_skeletal_bones_in_sie_action(result, **arguments)


def _postprocess_anim_demo_variant_action(result, **arguments):
    from . import postprocess_anim_demo_variant
    return postprocess_anim_demo_variant.ensure_postprocess_anim_demo_variant_action(result, **arguments)


def _anim_state_machine_runtime_response_action(result, **arguments):
    from . import anim_state_machine_runtime_response
    return anim_state_machine_runtime_response.sample_anim_state_machine_runtime_response_action(result, **arguments)


def _anim_instance_runtime_property_probe_action(result, **arguments):
    from . import anim_instance_runtime_property_probe
    return anim_instance_runtime_property_probe.set_anim_instance_runtime_property_for_probe_action(result, **arguments)


def _anim_instance_runtime_state_action(result, **arguments):
    from . import anim_instance_runtime_state
    return anim_instance_runtime_state.inspect_anim_instance_runtime_state_action(result, **arguments)


def _rigvm_pin_default_writer_action(result, **arguments):
    from . import rigvm_pin_default_writer
    return rigvm_pin_default_writer.set_controlrig_rigvm_pin_defaults_action(result, **arguments)


def _rigvm_link_writer_action(result, **arguments):
    from . import rigvm_link_writer
    return rigvm_link_writer.add_controlrig_rigvm_links_action(result, **arguments)


def _rigvm_pin_default_validation_action(result, **arguments):
    from . import rigvm_pin_default_validation
    return rigvm_pin_default_validation.validate_controlrig_rigvm_pin_defaults_action(result, **arguments)


def _rigvm_link_validation_action(result, **arguments):
    from . import rigvm_link_validation
    return rigvm_link_validation.validate_controlrig_rigvm_links_action(result, **arguments)


def _rigvm_topology_action(result, **arguments):
    from . import rigvm_topology
    return rigvm_topology.inspect_controlrig_rigvm_topology_action(result, **arguments)


def _user_defined_enum_action(result, **arguments):
    from . import user_defined_enum
    return user_defined_enum.create_user_defined_enum_action(result, **arguments)


def _physical_material_trace_action(result, **arguments):
    from . import physical_material_trace
    return physical_material_trace.line_trace_physical_material_action(result, **arguments)


def _asset_reference_action(result, **arguments):
    from . import asset_reference
    return asset_reference.replace_asset_references_mcp_action(result, **arguments)


def _content_audit_action(result, **arguments):
    from . import content_audit
    return content_audit.audit_content_root_mcp_action(result, **arguments)


def _input_mapping_action(result, **arguments):
    from . import input_mapping
    return input_mapping.create_input_mapping_action(result, **arguments)


def _state_tree_mutators_action(name):
    if name not in ("add_state_tree_state", "compile_state_tree"):
        raise OperationError("batch_state_tree_mutators_action_invalid", "Only original StateTree Add6/Compile2 methods admitted")
    def dispatch(result, **arguments):
        from . import state_tree_mutators
        return getattr(state_tree_mutators, name + "_action")(result, **arguments)
    return dispatch


def _state_tree_create_action(result, **arguments):
    from . import state_tree_create
    return state_tree_create.create_state_tree_action(result, **arguments)


def _state_tree_read_action(name):
    if name not in ("list_state_trees", "inspect_state_tree"):
        raise OperationError("batch_state_tree_read_action_invalid", "Only original StateTree list4/inspect2 methods admitted")
    def dispatch(result, **arguments):
        from . import state_tree_read
        return getattr(state_tree_read, name + "_action")(result, **arguments)
    return dispatch


def _anim_state_machine_inspection_action(result, **arguments):
    from . import anim_state_machine_inspection
    return anim_state_machine_inspection.inspect_anim_state_machine_transitions_action(result, **arguments)


def _blueprint_node_cleanup_action(method):
    if method not in ("list_blueprint_nodes", "delete_blueprint_node"):
        raise OperationError("batch_node_cleanup_method_invalid", "Only original List7/Delete12 admitted")
    def action(result, **arguments):
        from . import blueprint_node_cleanup
        return getattr(blueprint_node_cleanup, method + "_action")(result, **arguments)
    return action

def _blueprint_topology_action(result, **arguments):
    from . import blueprint_topology
    return blueprint_topology.inspect_blueprint_graph_call_topology_action(result, **arguments)


def _blueprint_query_layout_action(name):
    if name not in ("find_blueprint_nodes", "set_blueprint_node_positions"):
        raise OperationError("batch_blueprint_query_layout_action_invalid", "Only original Find6/Positions8 methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_query_layout
        return getattr(blueprint_query_layout, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_networking_action(name):
    if name not in ("inspect_blueprint_networking_settings", "set_blueprint_networking_settings"):
        raise OperationError("batch_blueprint_networking_action_invalid", "Only original Inspect1/Set11 methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_networking
        return getattr(blueprint_networking, name + "_action")(result, **arguments)
    return dispatch


def _graph_transfer_action(name):
    if name not in ("import_blueprint_graph_nodes_from_text", "repair_blueprint_pin_links"):
        raise OperationError("batch_graph_transfer_action_invalid", "Only exact original Import13/Repair6 methods admitted")
    def dispatch(result, **arguments):
        from . import graph_transfer
        return getattr(graph_transfer, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_binding_action(name):
    if name not in ("add_blueprint_native_delegate_bind_node", "add_blueprint_create_delegate_node", "add_blueprint_interface_message_node"):
        raise OperationError("batch_blueprint_binding_action_invalid", "Only exact original NativeBind/CreateDelegate/InterfaceMessage methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_binding
        return getattr(blueprint_binding, name + "_action")(result, **arguments)
    return dispatch


def _blueprint_dispatcher_lifecycle_action(name):
    if name not in ("add_blueprint_event_dispatcher_unbind_node", "add_blueprint_event_dispatcher_clear_node", "add_blueprint_event_dispatcher_assign_node"):
        raise OperationError("batch_blueprint_dispatcher_lifecycle_action_invalid", "Only exact original Unbind/Clear/Assign methods admitted")
    def dispatch(result, **arguments):
        from . import blueprint_dispatcher_lifecycle
        return getattr(blueprint_dispatcher_lifecycle, name + "_action")(result, **arguments)
    return dispatch


def _custom_event_signature_action(result, **arguments):
    from . import custom_event_signatures
    return custom_event_signatures.add_blueprint_custom_event_node_action(result, **arguments)


# UY: direct domain actions only; these are not calls to a guarded Named Toolset.
_ACTOR_BATCH_TOOLS = (
    "CubelessActorTools.delete_actor", "CubelessActorTools.set_actor_transform",
    "CubelessActorTools.set_actor_transform_original", "CubelessActorTools.spawn_blueprint_actor")


def _actor_batch_action(method):
    if method not in ("delete_actor", "set_actor_transform", "set_actor_transform_original", "spawn_blueprint_actor"):
        raise OperationError("batch_actor_action_invalid", "Only the original three Actor operations and distinct current transform adapter are admitted")
    def dispatch(result, **arguments):
        from . import actor
        parent_checkpoint = arguments.pop("checkpoint_callback", None)
        try:
            return getattr(actor, method + "_action")(result, **arguments, checkpoint_callback=parent_checkpoint)
        finally:
            native = result["data"].get("native_outcome")
            if type(native) is dict:
                # A failed before-read is also an actual returned Native result.
                # Never promote dispatch intent to a writer when Native said zero.
                result["data"]["actor_batch_native_result_returned"] = True
                if parent_checkpoint:
                    parent_checkpoint(result)
    return dispatch


def _actor_transform_legacy_params(params):
    allowed = {"name", "location", "rotation", "scale"}
    if set(params) - allowed:
        raise OperationError("batch_unknown_parameter", "Original set_actor_transform accepts only name/location/rotation/scale")
    values = copy.deepcopy(params)
    # Native HasField is different from public Python None omission: present
    # null/nonarray means zero, absent means keep. Native owns array first3/short.
    for key in ("location", "rotation", "scale"):
        if key in values and type(values[key]) is not list:
            values[key] = [0, 0, 0]
    return values


def _actor_spawn_legacy_params(params):
    allowed = {"blueprint_name", "actor_name", "location", "rotation", "scale"}
    if set(params) - allowed:
        raise OperationError("batch_unknown_parameter", "Original spawn_blueprint_actor accepts only blueprint_name/actor_name/location/rotation/scale")
    values = {key: value for key, value in params.items() if key in ("blueprint_name", "actor_name")}
    # Original handler defaults precede HasField. A present null/nonarray or a
    # short array becomes zero; a full array consumes exactly its first three.
    for key, default in (("location", [0, 0, 0]), ("rotation", [0, 0, 0]), ("scale", [1, 1, 1])):
        if key not in params:
            vector = default
        else:
            raw = params[key]
            if type(raw) is not list or len(raw) < 3:
                vector = [0, 0, 0]
            else:
                if any(type(item) not in (int, float) for item in raw[:3]):
                    raise OperationError("batch_actor_spawn_numeric_coercion_pending", "Original nonnumeric AsNumber variants are not implemented; no Actor was dispatched")
                try:
                    vector = [struct.unpack("<f", struct.pack("<f", item))[0] for item in raw[:3]]
                except (OverflowError, ValueError, TypeError, struct.error) as error:
                    raise OperationError("batch_actor_spawn_numeric_range_pending", "Original values outside finite float32 authoring scope remain incomplete") from error
                if any(not math.isfinite(item) for item in vector):
                    raise OperationError("batch_actor_spawn_numeric_range_pending", "Nonfinite narrowed values are outside finite authoring scope")
        values[key + "_json"] = json.dumps(vector, allow_nan=False, separators=(",", ":"))
    return values


def _actor_batch_observation(row):
    child = row["response"]
    native = child["data"].get("native_outcome")
    if type(native) is dict:
        attempted = native.get("mutation_attempted") is True
        known = native.get("change_state_known") is True
        performed = native.get("changed") if known else None
        in_flight = native.get("command_may_still_be_in_flight") is True or child["data"].get("command_may_still_be_in_flight") is True
        unknown = not known or in_flight or native.get("mutation_state_unknown") is True
    else:
        attempted = any(change.get("attempted") is True for change in child["changes"])
        performed = None if attempted else False
        in_flight = child["data"].get("command_may_still_be_in_flight") is True
        unknown = attempted or in_flight
    row.update(status="success" if child["success"] else "error", mutation_attempted=attempted,
        mutation_performed=performed, mutation_state_unknown=unknown,
        command_may_still_be_in_flight=in_flight, error=child["error"], error_code=child["error_code"])


def _python_execution_action(method):
    def action(result, **arguments):
        from . import python_execution
        return getattr(python_execution, method + "_action")(result, **arguments)
    return action


_CAPABILITIES = {
    "CubelessActorTools.delete_actor": _entry(_actor_batch_action("delete_actor"), "write", {
        "name": _S, "expected_level_path": ((str, type(None)), None)}),
    "CubelessActorTools.set_actor_transform": _entry(_actor_batch_action("set_actor_transform"), "write", {
        "name": _S, "location_json": ((str,), "null"), "rotation_json": ((str,), "null"),
        "scale_json": ((str,), "null"), "expected_level_path": ((str, type(None)), None)}),
    "CubelessActorTools.set_actor_transform_original": _entry(_actor_batch_action("set_actor_transform_original"), "write", {
        "name": _S, "location": ((list, type(None)), None), "rotation": ((list, type(None)), None),
        "scale": ((list, type(None)), None)}),
    "CubelessActorTools.spawn_blueprint_actor": _entry(_actor_batch_action("spawn_blueprint_actor"), "write", {
        "blueprint_name": _S, "actor_name": _S, "location_json": ((str,), "[0,0,0]"),
        "rotation_json": ((str,), "[0,0,0]"), "scale_json": ((str,), "[1,1,1]"),
        "expected_level_path": ((str, type(None)), None)}),
    "CubelessPythonExecutionTools.execute_unreal_python": _entry(_python_execution_action("execute_unreal_python"), "write", {
        "code": ((str,), _REQUIRED), "mode": ((str,), "ExecuteFile"), "defer_to_ticker": ((bool,), False),
        "allow_unsafe_editor_scripting_during_pie": ((bool,), False), "interchange_policy": ((str,), "auto")}),
    "execute_python": _entry(_python_execution_action("execute_python"), "write", {
        "code": ((str,), _REQUIRED), "mode": ((str,), "ExecuteStatement"), "defer_to_ticker": ((bool,), False),
        "allow_unsafe_editor_scripting_during_pie": ((bool,), False), "interchange_policy": ((str,), "auto"),
        "transaction_policy": ((str,), "command"), "allow_unsafe_pcg_selector_python": ((bool,), False)}),
    "CubelessControlRigTransientProbeTools.controlrig_direct_gate_probe": _entry(_controlrig_transient_probe_action("controlrig_direct_gate_probe"), "read_world", {
        "control_rig_path": ((str,), ""), "control_rig_class": ((str,), ""), "cases": ((list, type(None)), None),
        "sample_elements": ((list, type(None)), None), "control_values": ((dict, type(None)), None), "execute_events": ((list, type(None)), None),
        "should_trace_property": ((str,), "ShouldDoIKTrace"), "interaction_location_property": ((str,), "InteractionWorldLocation")}),
    "CubelessControlRigTransientProbeTools.sample_controlrig_pre_post_runtime_pose": _entry(_controlrig_transient_probe_action("sample_controlrig_pre_post_runtime_pose"), "read_world", {
        "control_rig_path": ((str,), ""), "control_rig_class": ((str,), ""), "input_defaults": ((dict, type(None)), None), "curve_values": ((dict, type(None)), None),
        "cases": ((list, type(None)), None), "sample_elements": ((list, type(None)), None), "control_values": ((dict, type(None)), None), "execute_events": ((list, type(None)), None),
        "should_trace_property": ((str,), "ShouldDoIKTrace"), "interaction_location_property": ((str,), "InteractionWorldLocation")}),
    "CubelessControlRigSolverIntentTools.validate_controlrig_solver_intents": _entry(_controlrig_solver_intents_action, "read", {"request": ((dict, str), _REQUIRED)}),
    'CubelessBlueprintWidgetFallbackTools.analyze_blueprint_widget_fallbacks_mcp': _entry(_blueprint_widget_fallback_action, "write", {'source_root': _S, 'target_root': _S, 'suffix': ((str,), '_MCP')}),
    'CubelessUMGTextBindingTools.set_text_block_binding': _entry(_umg_text_binding_action, "write", {'widget_name': _S, 'text_block_name': _S, 'binding_property': _S, 'binding_type': ((str,), 'Text')}),
    'CubelessUMGUtilityTools.add_checkbox_to_widget': _entry(_umg_utility_action('add_checkbox_to_widget'), "write", {'params': ((dict,), _REQUIRED)}),
    'CubelessUMGUtilityTools.add_widget_to_viewport': _entry(_umg_utility_action('add_widget_to_viewport'), "read", {'widget_name': _S, 'z_order': ((int,), 0)}),
    'CubelessUMGInteractionTools.add_sliders_to_widget': _entry(_umg_interaction_action('add_sliders_to_widget'), "write", {'widget_name': _S, 'sliders': ((list,), _REQUIRED)}),
    'CubelessUMGInteractionTools.bind_widget_event': _entry(_umg_interaction_action('bind_widget_event'), "write", {'widget_name': _S, 'widget_component_name': _S, 'event_name': _S, 'function_name': ((str,), '')}),
    'CubelessUMGAuthoringTools.create_umg_widget_blueprint': _entry(_umg_authoring_action('create_umg_widget_blueprint'), "write", {'widget_name': _S, 'parent_class': ((str,), 'UserWidget'), 'path': ((str,), '/Game/UI')}),
    'CubelessUMGAuthoringTools.add_text_block_to_widget': _entry(_umg_authoring_action('add_text_block_to_widget'), "write", {'widget_name': _S, 'text_block_name': _S, 'text': ((str,), ''), 'position': ((list,), [0.0, 0.0]), 'size': ((list,), [200.0, 50.0]), 'font_size': ((int,), 12), 'color': ((list,), [1.0, 1.0, 1.0, 1.0])}),
    'CubelessUMGAuthoringTools.add_button_to_widget': _entry(_umg_authoring_action('add_button_to_widget'), "write", {'widget_name': _S, 'button_name': _S, 'text': ((str,), ''), 'position': ((list,), [0.0, 0.0]), 'size': ((list,), [200.0, 50.0]), 'font_size': ((int,), 12), 'color': ((list,), [1.0, 1.0, 1.0, 1.0]), 'background_color': ((list,), [0.1, 0.1, 0.1, 1.0])}),
    'CubelessAnimGraphInspectionTools.inspect_anim_graph_node_settings': _entry(_anim_graph_inspection_action('inspect_anim_graph_node_settings'), "write", {'blueprint_name': _S, 'node_id': ((str,), ''), 'node_type': ((str,), ''), 'title_contains': ((str,), ''), 'include_pins': ((bool,), True), 'max_depth': ((int,), 4), 'graph_name': ((str,), 'AnimGraph'), 'graph_id': ((str,), ''), 'graph_type': ((str,), 'function')}),
    'CubelessAnimGraphInspectionTools.inspect_anim_graph_protected_topology': _entry(_anim_graph_inspection_action('inspect_anim_graph_protected_topology'), "write", {'blueprint_name': _S, 'node_id': ((str,), ''), 'node_type': ((str,), ''), 'title_contains': ((str,), ''), 'include_pins': ((bool,), True), 'include_pose_pins': ((bool,), True), 'include_links': ((bool,), True), 'include_non_pose_links': ((bool,), False), 'include_settings': ((bool,), False), 'max_depth': ((int,), 3), 'max_nodes': ((int,), 512), 'max_links': ((int,), 2048), 'graph_name': ((str,), 'AnimGraph'), 'graph_id': ((str,), ''), 'graph_type': ((str,), 'function')}),
    'CubelessBlueprintComponentReferenceTools.add_blueprint_get_self_component_reference': _entry(_blueprint_component_reference_action, "write", {'blueprint_name': _S, 'component_name': _S, 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'create_graph_if_missing': ((bool,), False)}),
    'CubelessBlueprintTopologyTools.inspect_blueprint_graph_call_topology': _entry(_blueprint_topology_action, "read", {'blueprint_name': _S, 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'graph_name_contains': ((str,), ''), 'node_type': ((str,), ''), 'title_contains': ((str,), ''), 'reference_contains': ((str,), ''), 'include_pins': ((bool,), False), 'include_links': ((bool,), True), 'include_references': ((bool,), True), 'max_graphs': ((int,), 64), 'max_nodes_per_graph': ((int,), 512), 'max_links_per_graph': ((int,), 1024), 'max_references_per_node': ((int,), 64)}),
    'CubelessBlueprintQueryLayoutTools.find_blueprint_nodes': _entry(_blueprint_query_layout_action('find_blueprint_nodes'), "write", {'blueprint_name': _S, 'node_type': ((str, int, float, bool, list, dict, type(None)), None), 'event_type': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), '')}),
    'CubelessBlueprintQueryLayoutTools.set_blueprint_node_positions': _entry(_blueprint_query_layout_action('set_blueprint_node_positions'), "write", {'blueprint_name': _S, 'graph_name': _S, 'updates': ((list,), _REQUIRED), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'dry_run': ((bool,), True), 'compile': ((bool,), False), 'save': ((bool,), False)}),
    'CubelessBlueprintNetworkingTools.inspect_blueprint_networking_settings': _entry(_blueprint_networking_action('inspect_blueprint_networking_settings'), "read", {'blueprint_name': _S}),
    'CubelessBlueprintNetworkingTools.set_blueprint_networking_settings': _entry(_blueprint_networking_action('set_blueprint_networking_settings'), "write", {'blueprint_name': _S, 'replicates': ((bool, type(None)), None), 'replicate_movement': ((bool, type(None)), None), 'net_dormancy': ((str, type(None)), None), 'always_relevant': ((bool, type(None)), None), 'only_relevant_to_owner': ((bool, type(None)), None), 'net_update_frequency': ((int, float, type(None)), None), 'min_net_update_frequency': ((int, float, type(None)), None), 'net_priority': ((int, float, type(None)), None), 'net_cull_distance_squared': ((int, float, type(None)), None), 'dry_run': ((bool,), True)}),
    'CubelessBlueprintGraphTransferTools.import_blueprint_graph_nodes_from_text': _entry(_graph_transfer_action('import_blueprint_graph_nodes_from_text'), "write", {'blueprint_name': _S, 'text': ((str,), ''), 'text_file_path': ((str,), ''), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'clear_graph': ((bool,), False), 'keep_root_node': ((bool,), True), 'allow_non_sample': ((bool,), False), 'validate_only': ((bool,), False), 'dry_run': ((bool,), False), 'compile': ((bool,), True), 'save_asset': ((bool,), False)}),
    'CubelessBlueprintGraphTransferTools.repair_blueprint_pin_links': _entry(_graph_transfer_action('repair_blueprint_pin_links'), "write", {'blueprint_name': _S, 'allow_non_sample': ((bool,), False), 'validate_only': ((bool,), False), 'dry_run': ((bool,), False), 'compile': ((bool,), True), 'save_asset': ((bool,), False)}),
    'CubelessBlueprintBindingTools.add_blueprint_native_delegate_bind_node': _entry(_blueprint_binding_action('add_blueprint_native_delegate_bind_node'), "write", {'blueprint_name': _S, 'delegate_owner_class': _S, 'delegate_name': _S, 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'create_graph_if_missing': ((bool,), False)}),
    'CubelessBlueprintBindingTools.add_blueprint_create_delegate_node': _entry(_blueprint_binding_action('add_blueprint_create_delegate_node'), "write", {'blueprint_name': _S, 'function_name': _S, 'object_node_id': _S, 'delegate_target_node_id': _S, 'object_pin_name': ((str,), 'ReturnValue'), 'delegate_target_pin_name': ((str,), 'Delegate'), 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'create_graph_if_missing': ((bool,), False)}),
    'CubelessBlueprintBindingTools.add_blueprint_interface_message_node': _entry(_blueprint_binding_action('add_blueprint_interface_message_node'), "write", {'blueprint_name': _S, 'interface_class': _S, 'function_name': _S, 'target_node_id': _S, 'target_pin_name': ((str,), 'ReturnValue'), 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'create_graph_if_missing': ((bool,), False)}),
    'CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_unbind_node': _entry(_blueprint_dispatcher_lifecycle_action('add_blueprint_event_dispatcher_unbind_node'), "write", {"blueprint_name": _S, "dispatcher_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    'CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_clear_node': _entry(_blueprint_dispatcher_lifecycle_action('add_blueprint_event_dispatcher_clear_node'), "write", {"blueprint_name": _S, "dispatcher_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    'CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_assign_node': _entry(_blueprint_dispatcher_lifecycle_action('add_blueprint_event_dispatcher_assign_node'), "write", {"blueprint_name": _S, "dispatcher_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    'CubelessBlueprintEventTools.add_blueprint_custom_event_node': _entry(_custom_event_signature_action, "write", {"blueprint_name": _S, "custom_event_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "signature_source_dispatcher_name": ((str,), ""), "signature_source_class": ((str,), ""), "signature_source_delegate_name": ((str,), ""), "native_delegate_name": ((str,), ""), "call_in_editor": ((bool, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher": _entry(_blueprint_dispatcher_action("add_blueprint_event_dispatcher"), "write", {"blueprint_name": _S, "dispatcher_name": _S, "inputs": ((list, type(None)), None), "category": ((str,), ""), "tooltip": ((str,), ""), "friendly_name": ((str,), "")}),
    "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_call_node": _entry(_blueprint_dispatcher_action("add_blueprint_event_dispatcher_call_node"), "write", {"blueprint_name": _S, "dispatcher_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_bind_node": _entry(_blueprint_dispatcher_action("add_blueprint_event_dispatcher_bind_node"), "write", {"blueprint_name": _S, "dispatcher_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintAnnotationTools.add_blueprint_comment_box": _entry(_blueprint_annotation_action("add_blueprint_comment_box"), "write", {'blueprint_name': _S, 'comment_text': _S, 'node_position': ((list, type(None)), None), 'size': ((list, type(None)), None), 'comment_color': ((list, type(None)), None), 'font_size': ((int,), 18), 'move_mode': ((str,), 'group'), 'comment_bubble_visible': ((bool,), False), 'color_comment_bubble': ((bool,), False), 'node_details': ((str,), ''), 'node_ids': ((list, type(None)), None), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), ''), 'create_graph_if_missing': ((bool,), False)}),
    "CubelessBlueprintAnnotationTools.set_blueprint_node_comment": _entry(_blueprint_annotation_action("set_blueprint_node_comment"), "write", {'blueprint_name': _S, 'node_id': _S, 'node_comment': _S, 'comment_bubble_visible': ((bool,), True), 'comment_bubble_pinned': ((bool,), False), 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), '')}),
    "CubelessBlueprintAnnotationTools.set_blueprint_function_description": _entry(_blueprint_annotation_action("set_blueprint_function_description"), "write", {'blueprint_name': _S, 'description': _S, 'graph_name': ((str,), ''), 'graph_id': ((str,), ''), 'graph_type': ((str,), 'function')}),
    "CubelessBlueprintExpressionTools.add_blueprint_input_axis_event_node": _entry(_blueprint_input_events_action("add_blueprint_input_axis_event_node"), "write", {'blueprint_name': _S, 'axis_name': _S, 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'consume_input': ((bool,), True), 'execute_when_paused': ((bool,), False), 'override_parent_binding': ((bool,), False), 'graph_name': ((str,), ""), 'graph_id': ((str,), ""), 'graph_type': ((str,), ""), 'create_graph_if_missing': ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_enhanced_input_action_node": _entry(_blueprint_input_events_action("add_blueprint_enhanced_input_action_node"), "write", {'blueprint_name': _S, 'input_action': _S, 'node_position': ((str, int, float, bool, list, dict, type(None)), None), 'graph_name': ((str,), ""), 'graph_id': ((str,), ""), 'graph_type': ((str,), ""), 'create_graph_if_missing': ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_get_editor_subsystem_node": _entry(_blueprint_editor_subsystem_action("add_blueprint_get_editor_subsystem_node"), "write", {
        "blueprint_name": _S, "subsystem_class": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_input_action_node": _entry(_blueprint_input_self_action("add_blueprint_input_action_node"), "write", {
        "blueprint_name": _S, "action_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_self_reference": _entry(_blueprint_input_self_action("add_blueprint_self_reference"), "write", {
        "blueprint_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_get_subsystem_from_pc_node": _entry(_blueprint_construction_action("add_blueprint_get_subsystem_from_pc_node"), "write", {
        "blueprint_name": _S, "subsystem_class": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_construct_object_node": _entry(_blueprint_construction_action("add_blueprint_construct_object_node"), "write", {
        "blueprint_name": _S, "object_class": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintSelectionTools.add_blueprint_enum_literal_node": _entry(_blueprint_branching_action("add_blueprint_enum_literal_node"), "write", {
        "blueprint_name": _S, "enum_type": _S, "value": ((str, int, float, bool, list, dict, type(None)), None),
        "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintSelectionTools.add_blueprint_is_valid_node": _entry(_blueprint_branching_action("add_blueprint_is_valid_node"), "write", {
        "blueprint_name": _S, "value_type": ((str,), "object"), "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintSelectionTools.add_blueprint_switch_int_node": _entry(_blueprint_branching_action("add_blueprint_switch_int_node"), "write", {
        "blueprint_name": _S, "start_index": ((int,), 0), "case_count": ((int,), 2), "case_values": ((list, type(None)), None),
        "has_default_pin": ((bool,), True), "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_compare_node": _entry(_blueprint_value_action("add_blueprint_compare_node"), "write", {
        "blueprint_name": _S, "operation": _S, "value_type": ((str,), "double"),
        "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_literal_node": _entry(_blueprint_value_action("add_blueprint_literal_node"), "write", {
        "blueprint_name": _S, "literal_type": _S, "value": ((str, int, float, bool, list, dict, type(None)), None),
        "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_math_node": _entry(_blueprint_expression_action("add_blueprint_math_node"), "write", {
        "blueprint_name": _S, "operation": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintExpressionTools.add_blueprint_boolean_node": _entry(_blueprint_expression_action("add_blueprint_boolean_node"), "write", {
        "blueprint_name": _S, "operation": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessMaterialTools.expand_material_function_calls": _entry(_material_function_expansion_action, "write", {
        "material_path": _S, "node_id": ((str,), ""), "recursive": ((bool,), True),
        "exclude_engine_functions": ((bool,), True), "max_passes": ((int,), 8),
        "save": ((bool,), True), "allow_partial_save": ((bool,), False)}),
    "CubelessBlueprintSelectionTools.add_blueprint_select_node": _entry(_blueprint_selection_action("add_blueprint_select_node"), "write", {
        "blueprint_name": _S, "value_type": ((str,), "int"), "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintSelectionTools.add_blueprint_switch_enum_node": _entry(_blueprint_selection_action("add_blueprint_switch_enum_node"), "write", {
        "blueprint_name": _S, "enum_type": _S, "has_default_pin": ((bool,), False), "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""), "create_graph_if_missing": ((bool,), False)}),
    "CubelessMaterialTools.set_material_parameter_collection_sync": _entry(_material_collection_sync_action, "write", {
        "action": ((str,), "status"), "source_collection_path": ((str,), ""), "target_collection_path": ((str,), ""),
        "parameter_names": ((list, type(None)), None), "interval_seconds": ((int, float), 0.1),
        "set_asset_defaults_on_enable": ((bool,), False), "save_on_enable": ((bool,), False)}),
    "CubelessMaterialTools.refresh_material_cached_expression_data": _entry(_material_cache_action, "write", {
        "material_path": _S, "save": ((bool,), True)}),
    "CubelessMaterialTools.set_material_texture_sample_sampler_type_mcp": _entry(_material_sampler_action, "write", {
        "material_path": _S, "texture_path": _S, "sampler_type": ((str,), "SAMPLERTYPE_LinearColor"),
        "dry_run": ((bool,), False), "save": ((bool,), True), "compile": ((bool,), True)}),
    "CubelessMaterialTools.replace_material_collection_parameters": _entry(_material_collection_parameters_action, "write", {
        "material_path": _S, "use_runtime_values": ((bool,), True), "save": ((bool,), True)}),
    "CubelessMaterialTools.replace_material_collection_references": _entry(_material_reference_action("replace_material_collection_references"), "write", {
        "material_path": _S, "target_collection_path": _S, "source_collection_path": ((str,), ""),
        "graph_type": ((str,), "auto"), "save": ((bool,), True)}),
    "CubelessMaterialTools.replace_material_texture_references": _entry(_material_reference_action("replace_material_texture_references"), "write", {
        "material_path": _S, "replacements": ((dict,), _REQUIRED), "graph_type": ((str,), "auto"), "save": ((bool,), True)}),
    "CubelessSkeletalPhysicsTools.create_skeletal_bone_constraint_fixture": _entry(_skeletal_constraint_action("create_skeletal_bone_constraint_fixture"), "write", {
        "skeletal_mesh_path": _S, "actor_name": ((str,), "MCP_SkeletalBoneConstraint_Ghost"),
        "bone_name1": ((str,), "hand_l"), "bone_name2": ((str,), "hand_r"), "world_context": ((str,), "editor"),
        "location": ((list, type(None)), None), "constraint_component_name": ((str,), "MCP_SkeletalBoneConstraint"),
        "swing1_limit_degrees": ((int, float), 45.0), "swing2_limit_degrees": ((int, float), 45.0), "twist_limit_degrees": ((int, float), 30.0),
        "disable_collision": ((bool,), True), "simulate_all_bodies": ((bool,), True), "dry_run": ((bool,), True)}),
    "CubelessSkeletalPhysicsTools.inspect_skeletal_bone_constraint_fixture": _entry(_skeletal_constraint_action("inspect_skeletal_bone_constraint_fixture"), "read", {
        "actor_name": _S, "world_context": ((str,), "auto"), "constraint_component_name": ((str,), "")}),
    "CubelessSkeletalPhysicsTools.create_skeletal_physics_asset": _entry(_skeletal_physics_asset_action, "write", {
        "skeletal_mesh_path": _S, "target_physics_asset_path": _S,
        "expected_bones": ((list, type(None)), None), "kinematic_root_bone": ((str,), ""),
        "root_kinematic": ((bool,), True), "disable_collision": ((bool,), True),
        "swing1_limit_degrees": ((int, float), 45.0), "swing2_limit_degrees": ((int, float), 45.0),
        "twist_limit_degrees": ((int, float), 25.0), "linear_damping": ((int, float), 0.15),
        "angular_damping": ((int, float), 0.5), "tip_mass_scale": ((int, float), 3.0),
        "overwrite_existing": ((bool,), False), "save": ((bool,), True), "dry_run": ((bool,), True)}),
    "CubelessSkeletalTools.set_bone_translation_retargeting": _entry(_skeletal_retarget_action, "write", {
        "skeleton_path": _S, "bone_name": _S, "mode": _S,
        "recursive": ((bool,), False), "save": ((bool,), True)}),
    "CubelessSkeletalTools.add_skeletal_socket": _entry(_skeletal_socket_action("add_skeletal_socket"), "write", {
        "skeleton_path": _S, "socket_name": _S, "bone_name": _S,
        "location": ((list, type(None)), None), "rotation": ((list, type(None)), None), "scale": ((list, type(None)), None), "save": ((bool,), True)}),
    "CubelessSkeletalTools.set_skeletal_socket": _entry(_skeletal_socket_action("set_skeletal_socket"), "write", {
        "skeleton_path": _S, "socket_name": _S, "bone_name": ((str,), ""), "new_socket_name": ((str,), ""),
        "location": ((list, type(None)), None), "rotation": ((list, type(None)), None), "scale": ((list, type(None)), None), "save": ((bool,), True)}),
    "CubelessSkeletalTools.remove_skeletal_socket": _entry(_skeletal_socket_action("remove_skeletal_socket"), "write", {
        "skeleton_path": _S, "socket_name": _S, "save": ((bool,), True)}),
    "CubelessLocalVariableTools.add_blueprint_local_variable": _entry(_local_variable_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "variable_type": _S,
        "default_value": ((dict, list, str, int, float, bool, type(None)), None),
        "category": ((str,), ""), "tooltip": ((str,), ""), "type_object": ((str,), ""),
        "is_array": ((bool,), False), "graph_name": ((str,), ""), "graph_id": ((str,), ""),
        "graph_type": ((str,), "function"), "create_graph_if_missing": ((bool,), False)}),
    "CubelessNiagaraModuleTools.set_niagara_module_inputs_batch": _entry(_niagara_module_batch_action, "write", {
        "system_path": _S, "edits": ((list,), _REQUIRED), "operation": ((str,), "set_existing"),
        "overwrite_existing": ((bool,), False), "continue_on_error": ((bool,), False),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), False), "timeout_seconds": ((int, float), 10.0),
        "poll_interval_seconds": ((int, float), 0.1)}),
    "CubelessFunctionNodeTools.add_blueprint_function_node": _entry(_function_node_action, "write", {
        "blueprint_name": _S, "target": _S, "function_name": _S,
        "params": ((dict, list, str, int, float, bool, type(None)), None),
        "node_position": ((list, type(None)), None), "graph_name": ((str,), ""),
        "graph_id": ((str,), ""), "graph_type": ((str,), ""),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintTools.set_pawn_properties": _entry(_pawn_action, "write", {
        "blueprint_name": _S, "auto_possess_player": ((str,), ""),
        "use_controller_rotation_yaw": ((bool, type(None)), None),
        "use_controller_rotation_pitch": ((bool, type(None)), None),
        "use_controller_rotation_roll": ((bool, type(None)), None),
        "can_be_damaged": ((bool, type(None)), None)}),
    "CubelessBlueprintGraphTools.set_blueprint_custom_event_metadata": _entry(_custom_event_metadata_action, "write", {
        "blueprint_name": _S, "custom_event_name": ((str,), ""), "description": ((str,), ""),
        "node_id": ((str,), ""), "event_name": ((str,), ""), "tooltip": ((str,), ""),
        "call_in_editor": ((bool, type(None)), None), "set_node_comment": ((bool,), True),
        "compile": ((bool,), True), "save": ((bool,), False)}),
    "CubelessBlueprintTools.set_blueprint_category_sorting": _entry(_category_sorting_action, "write", {
        "blueprint_name": _S, "category_order": ((list, type(None)), None), "category_to_front": ((str,), ""),
        "append_existing": ((bool,), True), "save": ((bool,), True), "compile": ((bool,), False)}),
    "CubelessBlueprintTools.remove_blueprint_variable": _entry(_variable_removal_action, "write", {
        "blueprint_name": _S, "variable_name": _S}),
    "CubelessBlueprintTools.set_blueprint_variable_metadata": _entry(_variable_lifecycle_action("set_blueprint_variable_metadata"), "write", {
        "blueprint_name": _S, "variable_name": _S, "metadata": ((dict, type(None)), None),
        "is_editable": ((bool, type(None)), None), "instance_editable": ((bool, type(None)), None),
        "expose_on_spawn": ((bool, type(None)), None)}),
    "CubelessBlueprintTools.set_blueprint_variable_order": _entry(_variable_lifecycle_action("set_blueprint_variable_order"), "write", {
        "blueprint_name": _S, "variable_order": ((list,), _REQUIRED), "strict": ((bool,), False),
        "compile": ((bool,), False), "save": ((bool,), False)}),
    "CubelessDataAssetTools.get_data_asset": _entry(_data_asset_action("get_data_asset"), "read_load", {
        "asset_path": _S}),
    "CubelessDataAssetTools.set_data_asset_properties": _entry(_data_asset_action("set_data_asset_properties"), "write", {
        "asset_path": _S, "properties": ((dict,), _REQUIRED), "save": ((bool,), True)}),
    "CubelessDataAssetTools.create_data_asset": _entry(_data_asset_action("create_data_asset"), "write", {
        "asset_path": _S, "class_path": _S, "save": ((bool,), True)}),
    "CubelessMaterialTools.compile_and_save_material": _entry(_material_compilation_action, "write", {
        "material_path": _S, "graph_type": ((str,), "auto"), "save": ((bool,), True)}),
    "CubelessDataTableTools.get_datatable_schema": _entry(_datatable_action("get_datatable_schema"), "read_load", {
        "datatable_path": _S}),
    "CubelessDataTableTools.list_datatable_rows": _entry(_datatable_action("list_datatable_rows"), "read_load", {
        "datatable_path": _S, "row_name_contains": ((str,), ""), "max_rows": ((int,), 200), "include_values": ((bool,), True)}),
    "CubelessDataTableTools.get_datatable_row": _entry(_datatable_action("get_datatable_row"), "read_load", {
        "datatable_path": _S, "row_name": _S}),
    "CubelessDataTableTools.add_or_update_datatable_row": _entry(_datatable_action("add_or_update_datatable_row"), "write", {
        "datatable_path": _S, "row_name": _S, "row_values": ((dict,), _REQUIRED), "save": ((bool,), True)}),
    "CubelessDataTableTools.remove_datatable_row": _entry(_datatable_action("remove_datatable_row"), "write", {
        "datatable_path": _S, "row_name": _S, "save": ((bool,), True)}),
    "CubelessDataTableTools.create_datatable": _entry(_datatable_action("create_datatable"), "write", {
        "asset_path": _S, "row_struct_path": _S, "save": ((bool,), True)}),
    "CubelessBlueprintTools.ensure_blueprint_rigid_constraint_chain": _entry(
        _rigid_constraint_chain_action, "write", {
            "blueprint_name": _S, "body_components": ((list,), _REQUIRED),
            "root_component_name": ((str,), "SceneRoot"), "constraint_prefix": ((str,), "PC"),
            "swing1_limit_degrees": ((int, float), 50.0), "swing2_limit_degrees": ((int, float), 50.0),
            "twist_limit_degrees": ((int, float), 10.0), "linear_damping": ((int, float), 0.05),
            "angular_damping": ((int, float), 0.15), "projection_linear_alpha": ((int, float), 0.25),
            "projection_angular_alpha": ((int, float), 0.08), "projection_linear_tolerance": ((int, float), 5.0),
            "projection_angular_tolerance": ((int, float), 15.0),
            "required_root": ((str,), "/Game/Study/ChaosSkeletalMesh"), "save": ((bool,), True), "dry_run": ((bool,), True)}),
    "CubelessBlueprintTools.replace_blueprint_component_class": _entry(
        _component_replacement_action, "write", {
            "blueprint_name": _S, "component_name": _S, "new_component_class": _S,
            "sphere_radius": ((int, float), -1.0), "dry_run": ((bool,), True),
            "compile": ((bool,), True), "save": ((bool,), True), "refresh_nodes": ((bool,), True)}),
    "CubelessBlueprintTools.set_blueprint_actor_tick_settings": _entry(
        _blueprint_defaults_action("set_blueprint_actor_tick_settings"), "write", {
            "blueprint_name": _S, "tick_hz": ((int, float), 60.0), "tick_interval": ((int, float), -1.0),
            "can_ever_tick": ((bool,), True), "start_with_tick_enabled": ((bool,), True),
            "allow_tick_on_dedicated_server": ((bool,), True), "tick_even_when_paused": ((bool,), False),
            "dry_run": ((bool,), True), "compile": ((bool,), True), "save": ((bool,), True)}),
    "CubelessBlueprintTools.set_skeletal_mesh_component_anim_defaults": _entry(
        _blueprint_defaults_action("set_skeletal_mesh_component_anim_defaults"), "write", {
            "blueprint_name": _S, "component_name": _S, "skeletal_mesh": ((str,), ""), "anim_class": ((str,), ""),
            "physics_asset": ((str, type(None)), None), "simulate_physics": ((bool, type(None)), None),
            "collision_enabled": ((str, type(None)), None), "collision_response": ((str, type(None)), None),
            "enable_gravity": ((bool, type(None)), None), "visibility_based_anim_tick_option": ((str, type(None)), None),
            "compile": ((bool,), True), "save": ((bool,), True)}),
    "CubelessAudioTools.list_audio_assets": _entry(_audio_action("list_audio_assets"), "read", {
        "directory": ((str,), "/Game"), "recursive": ((bool,), True),
        "max_results": ((int,), 200), "offset": ((int,), 0)}),
    "CubelessAudioTools.inspect_audio_asset": _entry(_audio_action("inspect_audio_asset"), "read_load", {
        "asset_path": _S}),
    "CubelessAudioTools.create_sound_cue_from_wave": _entry(_audio_action("create_sound_cue_from_wave"), "write", {
        "name": _S, "sound_wave_path": _S, "package_path": ((str,), "/Game/Audio/SoundCues")}),
    "CubelessAudioTools.create_basic_metasound_source": _entry(_audio_action("create_basic_metasound_source"), "write", {
        "name": _S, "package_path": ((str,), "/Game/Audio/MetaSounds"), "oscillator": ((str,), "sine"),
        "frequency": ((int, float), 440.0), "output_format": ((str,), "mono")}),
    "CubelessAITools.remove_ai_blackboard_key": _entry(
        _ai_action("remove_ai_blackboard_key"), "write", {
            "blackboard_path": _S, "key_name": _S,
            "save_asset": ((bool,), True), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.configure_ai_controller_perception": _entry(_ai_action("configure_ai_controller_perception"), "write", {
        "controller_blueprint": ((str,), "/Game/Cubeless/AI/BP_AIBot_Controller.BP_AIBot_Controller"),
        "component_name": ((str,), "AIPerception"), "sight_radius": ((int, float), 900.0),
        "lose_sight_radius": ((int, float), 1150.0), "peripheral_vision_angle_degrees": ((int, float), 90.0),
        "max_age": ((int, float), 3.0), "detect_enemies": ((bool,), True), "detect_neutrals": ((bool,), True),
        "detect_friendlies": ((bool,), True), "replace_existing_config": ((bool,), True),
        "save_blueprint": ((bool,), False), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.create_ai_patrol_chase_behavior_assets": _entry(_ai_action("create_ai_patrol_chase_behavior_assets"), "write", {
        "asset_root": ((str,), "/Game/Cubeless/AI/Smoke"), "behavior_tree_name": ((str,), "BT_AIDummyPatrolChase"),
        "blackboard_name": ((str,), "BB_AIDummyPatrolChase"), "target_actor_key": ((str,), "TargetActor"),
        "patrol_location_key": ((str,), "PatrolLocation"), "tree_shape": ((str,), "sequence_patrol_chase"),
        "acceptable_radius": ((int, float), 55.0), "alert_wait_seconds": ((int, float), 0.5),
        "face_precision_degrees": ((int, float), 12.0), "replace_existing": ((bool,), False),
        "save_assets": ((bool,), False), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.create_ai_eqs_query": _entry(_ai_action("create_ai_eqs_query"), "write", {
        "asset_name": _S, "asset_root": ((str,), "/Game/AI/EQS"), "save_asset": ((bool,), True),
        "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.create_ai_behavior_tree_asset": _entry(_ai_action("create_ai_behavior_tree_asset"), "write", {
        "asset_name": _S, "asset_root": ((str,), "/Game/AI"), "blackboard_path": ((str, type(None)), None),
        "save_asset": ((bool,), True), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.set_ai_behavior_tree_blackboard": _entry(_ai_action("set_ai_behavior_tree_blackboard"), "write", {
        "behavior_tree_path": _S, "blackboard_path": _S, "save_asset": ((bool,), True), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.create_ai_blackboard_asset": _entry(_ai_action("create_ai_blackboard_asset"), "write", {
        "asset_name": _S, "asset_root": ((str,), "/Game/AI"), "save_asset": ((bool,), True),
        "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.add_ai_blackboard_key": _entry(_ai_action("add_ai_blackboard_key"), "write", {
        "blackboard_path": _S, "key_name": _S, "key_type": _S,
        "base_class": ((str, type(None)), None), "enum_path": ((str, type(None)), None),
        "instance_synced": ((bool,), False), "save_asset": ((bool,), True), "timeout_seconds": ((int,), 60)}),
    "CubelessAITools.inspect_ai_asset_structure": _entry(_ai_action("inspect_ai_asset_structure"), "read_load", {
        "asset_path": _S, "timeout_seconds": ((int,), 60)}),
    "CubelessSequencerTools.create_level_sequence": _entry(
        _sequencer_action("create_level_sequence_action"), "write", {
        "sequence_path": _S, "frame_rate": ((int, float), 30.0),
        "playback_start_frame": ((int, float), 0.0), "playback_end_frame": ((int, float), 150.0),
        "save": ((bool,), True)}),
    "CubelessSequencerTools.get_level_sequence_info": _entry(
        _sequencer_action("get_level_sequence_info_action"), "read", {"sequence_path": _S}),
    "CubelessSequencerTools.add_level_sequence_actor_binding": _entry(
        _sequencer_action("add_level_sequence_actor_binding_action"), "write", {
        "sequence_path": _S, "actor_label": _S, "save": ((bool,), True)}),
    "CubelessSequencerTools.add_level_sequence_transform_keys": _entry(
        _sequencer_action("add_level_sequence_transform_keys_action"), "write", {
        "sequence_path": _S, "keys": ((list, str), _REQUIRED),
        "binding_id": ((str, type(None)), None), "actor_label": ((str, type(None)), None),
        "save": ((bool,), True)}),
    "CubelessSequencerTools.open_level_sequence_in_editor": _entry(
        _sequencer_editor_action, "write", {"sequence_path": _S}),
    "CubelessSequencerTools.add_level_sequence_camera_cut": _entry(
        _sequencer_camera_action, "write", {
        "sequence_path": _S, "start_frame": ((int, float), _REQUIRED), "end_frame": ((int, float), _REQUIRED),
        "binding_id": ((str,), ""), "actor_label": ((str,), ""), "save": ((bool,), True)}),
    "CubelessSequencerTools.add_level_sequence_subsequence": _entry(
        _sequencer_subsequence_action, "write", {
        "sequence_path": _S, "subsequence_path": _S, "start_frame": ((int, float), _REQUIRED),
        "duration_frames": ((int, float), _REQUIRED), "save": ((bool,), True)}),
    "CubelessSequencerTools.set_level_sequence_playback_range": _entry(
        _sequencer_maintenance_action("set_level_sequence_playback_range"), "write", {
        "sequence_path": _S, "playback_start_frame": ((int, float), _REQUIRED),
        "playback_end_frame": ((int, float), _REQUIRED), "save": ((bool,), True)}),
    "CubelessSequencerTools.remove_level_sequence_binding": _entry(
        _sequencer_maintenance_action("remove_level_sequence_binding"), "write", {
        "sequence_path": _S, "binding_id": ((str,), ""), "actor_label": ((str,), ""), "save": ((bool,), True)}),
    "CubelessSequencerTools.add_level_sequence_float_property_keys": _entry(
        _sequencer_float_action, "write", {
        "sequence_path": _S, "property_path": _S, "keys": ((list,), _REQUIRED),
        "binding_id": ((str,), ""), "actor_label": ((str,), ""), "save": ((bool,), True)}),
    "CubelessMaterialTools.set_material_parameter_collection_values": _entry(
        _material_collection_action("set_material_parameter_collection_values"), "write", {
        "collection_path": _S, "scalars": ((dict, str, type(None)), None),
        "vectors": ((dict, str, type(None)), None), "set_asset_defaults": ((bool,), True),
        "set_runtime": ((bool,), True), "save": ((bool,), True)}),
    "CubelessMaterialTools.mirror_material_parameter_collection": _entry(
        _material_collection_action("mirror_material_parameter_collection"), "write", {
        "source_collection_path": _S, "target_collection_path": _S,
        "parameter_names": ((list, str, type(None)), None), "copy_defaults": ((bool,), True),
        "preserve_ids": ((bool,), True), "save": ((bool,), True)}),
    "CubelessNiagaraScratchTools.create_or_duplicate_scratch_pad_module": _entry(
        _niagara_scratch_action("create_or_duplicate_scratch_pad_module"), "write", {
        "target_system_path": _S, "source_script_path": ((str, type(None)), None),
        "source_system_path": ((str, type(None)), None), "source_owner_kind": ((str,), "system"),
        "source_script_index": ((int, type(None)), None), "source_scratch_pad_name": ((str, type(None)), None),
        "source_emitter_index": ((int, type(None)), None), "source_emitter_name": ((str, type(None)), None),
        "target_owner_kind": ((str,), "system"), "target_emitter_index": ((int, type(None)), None),
        "target_emitter_name": ((str, type(None)), None), "new_script_name": ((str, type(None)), None),
        "target_supported_usage": ((str,), "ParticleUpdateScript"), "allow_source_edit": ((bool,), False),
        "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraScratchTools.add_niagara_scratch_pad_parameter_write": _entry(
        _niagara_scratch_action("add_niagara_scratch_pad_parameter_write"), "write", {
        "system_path": _S, "parameter_name": _S, "value": ((str, type(None)), None),
        "parameter_type": ((str,), "vec2"), "default_value": ((str, type(None)), None),
        "scratch_pad_script_path": ((str, type(None)), None), "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int, type(None)), None), "scratch_pad_name": ((str, type(None)), None),
        "scratch_pad_emitter_index": ((int, type(None)), None), "scratch_pad_emitter_name": ((str, type(None)), None),
        "skip_if_exists": ((bool,), True), "allow_source_edit": ((bool,), False),
        "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraScratchTools.add_scratch_pad_module_to_stack": _entry(
        _niagara_scratch_action("add_scratch_pad_module_to_stack"), "write", {
        "target_system_path": _S, "scratch_pad_script_path": ((str, type(None)), None),
        "scratch_pad_owner_kind": ((str,), "system"), "scratch_pad_script_index": ((int, type(None)), None),
        "scratch_pad_name": ((str, type(None)), None), "scratch_pad_emitter_index": ((int, type(None)), None),
        "scratch_pad_emitter_name": ((str, type(None)), None), "target_usage": ((str,), "ParticleUpdateScript"),
        "target_usage_id": ((str, type(None)), None), "target_emitter_index": ((int, type(None)), None),
        "target_emitter_name": ((str, type(None)), None), "target_index": ((int, type(None)), None),
        "suggested_name": ((str, type(None)), None), "skip_if_duplicate": ((bool,), True),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraScratchTools.insert_niagara_scratch_pad_custom_hlsl_for_pin": _entry(
        _niagara_scratch_action("insert_niagara_scratch_pad_custom_hlsl_for_pin"), "write", {
        "system_path": _S, "target_pin_name": _S, "expression": _S,
        "scratch_pad_script_path": ((str, type(None)), None), "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int, type(None)), None), "scratch_pad_name": ((str,), "RenderGrid"),
        "scratch_pad_emitter_index": ((int, type(None)), None), "scratch_pad_emitter_name": ((str, type(None)), None),
        "target_node_guid": ((str, type(None)), None), "target_node_name": ((str, type(None)), None),
        "target_node_index": ((int, type(None)), None), "preserve_existing_link_as": ((str,), "BaseValue"),
        "output_pin_name": ((str,), "CustomHLSLOutput"), "signature_name": ((str,), "IF_CustomPinValue"),
        "custom_hlsl_script_usage": ((str,), "DynamicInput"), "inputs": ((str, type(None)), None),
        "user_parameter_inputs": ((str, type(None)), None), "skip_if_already_inserted": ((bool,), True),
        "replace_existing_custom": ((bool,), False), "rebuild_existing_custom": ((bool,), False),
        "requires_context": ((bool,), False), "delete_unlinked_custom_nodes": ((bool,), False),
        "delete_unlinked_custom_input_source_nodes": ((bool,), False), "allow_source_edit": ((bool,), False),
        "save": ((bool,), True), "request_compile": ((bool,), True), "parameter_inputs": ((str, type(None)), None)}),
    "CubelessNiagaraScratchTools.rewire_niagara_custom_hlsl_input_to_module_parameter": _entry(
        _niagara_scratch_action("rewire_niagara_custom_hlsl_input_to_module_parameter"), "write", {
        "system_path": _S, "input_name": _S, "scratch_pad_name": ((str, type(None)), None),
        "custom_node_signature_name": ((str, type(None)), None), "module_parameter_name": ((str, type(None)), None),
        "delete_orphaned_source_node": ((bool,), True), "allow_source_edit": ((bool,), False),
        "save": ((bool,), True), "request_compile": ((bool,), True), "timeout_seconds": ((int, float), 60.0)}),
    "CubelessNiagaraModuleInputTools.set_niagara_module_input_literal_override": _niagara_module_input_entry(
        "set_niagara_module_input_literal_override", "value", _S),
    "CubelessNiagaraModuleInputTools.set_niagara_data_interface_module_input": _niagara_module_input_entry(
        "set_niagara_data_interface_module_input", "properties", ((str, type(None)), None)),
    "CubelessNiagaraModuleInputTools.set_niagara_module_input_linked_parameter": _niagara_module_input_entry(
        "set_niagara_module_input_linked_parameter", "linked_parameter_name", _S),
    "CubelessNiagaraSimulationStageTools.set_niagara_simulation_stage_settings": _entry(_niagara_stage_settings_action, "write", {
        "system_path": _S, "emitter_index": ((int, type(None)), None), "emitter_name": ((str, type(None)), None),
        "stage_index": ((int, type(None)), None), "stage_name": ((str, type(None)), None),
        "enabled": ((bool, type(None)), None), "iteration_source": ((str, type(None)), None),
        "data_interface_name": ((str, type(None)), None), "direct_dispatch_type": ((str, type(None)), None),
        "direct_dispatch_element_type": ((str, type(None)), None), "execute_behavior": ((str, type(None)), None),
        "element_count": ((list, type(None)), None), "element_count_x": ((int, type(None)), None),
        "element_count_y": ((int, type(None)), None), "element_count_z": ((int, type(None)), None),
        "num_iterations": ((int, type(None)), None), "num_iterations_binding": ((str, type(None)), None),
        "gpu_dispatch_force_linear": ((bool, type(None)), None), "override_gpu_dispatch_num_threads": ((bool, type(None)), None),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraSimulationStageTools.add_niagara_grid_simulation_stage": _entry(_niagara_stage_action, "write", {
        "system_path": _S, "emitter_index": ((int, type(None)), None), "emitter_name": ((str, type(None)), None),
        "stage_name": ((str,), "MCP_GridSimulationStage"), "data_interface_name": ((str,), "Emitter.Grid2D Collection"),
        "insert_index": ((int, type(None)), None), "skip_if_duplicate": ((bool,), True), "enabled": ((bool,), True),
        "iteration_source": ((str,), "DataInterface"), "direct_dispatch_type": ((str, type(None)), None),
        "direct_dispatch_element_type": ((str, type(None)), None), "execute_behavior": ((str,), "Always"),
        "element_count": ((list, type(None)), None), "element_count_x": ((int, type(None)), None),
        "element_count_y": ((int, type(None)), None), "element_count_z": ((int, type(None)), None),
        "num_iterations": ((int,), 1), "gpu_dispatch_force_linear": ((bool, type(None)), None),
        "override_gpu_dispatch_num_threads": ((bool, type(None)), None), "allow_source_edit": ((bool,), False),
        "save": ((bool,), True), "request_compile": ((bool,), True), "wait_for_completion": ((bool,), False),
        "timeout_seconds": ((int, float), 10.0), "poll_interval_seconds": ((int, float), 0.1)}),
    "CubelessNiagaraEmitterTools.duplicate_or_attach_emitter_from_source": _entry(_niagara_emitter_action, "write", {
        "target_system_path": _S, "source_emitter_path": ((str, type(None)), None),
        "source_system_path": ((str, type(None)), None), "source_emitter_index": ((int, type(None)), None),
        "source_emitter_name": ((str, type(None)), None), "new_emitter_name": ((str, type(None)), None),
        "enabled": ((bool, type(None)), None), "copy_source_user_parameters": ((bool,), False),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraModuleTools.add_niagara_stock_module_to_stack": _entry(_niagara_module_action, "write", {
        "target_system_path": _S, "module_script_path": _S,
        "target_usage": ((str,), "ParticleSpawnScriptInterpolated"),
        "target_usage_id": ((str, type(None)), None), "target_emitter_index": ((int, type(None)), None),
        "target_emitter_name": ((str, type(None)), None), "target_index": ((int, type(None)), None),
        "suggested_name": ((str, type(None)), None), "skip_if_duplicate": ((bool,), True),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessNiagaraParameterTools.inspect_niagara_user_parameters": _entry(_niagara_parameter_action("inspect_niagara_user_parameters_action"), "read", {
        "system_path": _S}),
    "CubelessNiagaraParameterTools.set_niagara_user_parameter": _entry(_niagara_parameter_action("set_niagara_user_parameter_action"), "write", {
        "system_path": _S, "parameter_name": _S, "value_json": _S,
        "allow_source_edit": ((bool,), False), "save": ((bool,), True)}),
    "CubelessNiagaraParameterTools.upsert_niagara_user_parameters": _entry(_niagara_parameter_action("upsert_niagara_user_parameters_action"), "write", {
        "system_path": _S, "parameters_json": _S, "continue_on_error": ((bool,), False),
        "allow_source_edit": ((bool,), False), "save": ((bool,), True), "request_compile": ((bool,), True)}),
    "CubelessBlueprintGraphTools.add_blueprint_loop_node": _entry(_loop_action, "write", {
        "blueprint_name": _S, "loop_type": ((str,), "for_loop"),
        "first_index": ((int, type(None)), None), "last_index": ((int, type(None)), None),
        "node_position_json": ((str,), "[0,0]"), "graph_name": ((str, type(None)), None),
        "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False),
        "element_type": ((str, type(None)), None), "type_object": ((str, type(None)), None),
        "enum_type": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_blueprint_make_array_node": _entry(_make_array_action, "write", {
        "blueprint_name": _S, "element_type": _S, "input_count": ((int, type(None)), None),
        "values": ((str, type(None)), None), "type_object": ((str, type(None)), None), "enum_type": ((str, type(None)), None),
        "node_position": ((str, type(None)), None), "graph_name": ((str, type(None)), None),
        "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False),
        "element_defaults": ((str, type(None)), None)}),
    "CubelessPCGTools.create_pcg_graph_from_spec": _entry(_pcg_action("create_pcg_graph_from_spec_action"), "write", {
        "graph_spec_json": _S, "overwrite_existing": ((bool,), False), "allow_overwrite_non_temp": ((bool,), False),
        "save": ((bool,), True), "dry_run": ((bool,), False)}),
    "CubelessPCGTools.set_pcg_attribute_selector": _entry(_pcg_action("set_pcg_attribute_selector_action"), "write", {
        "graph_path": _S, "node_id": _S, "selector_property_name": _S, "selector_type": ((str,), "attribute"),
        "attribute_name": ((str, type(None)), None), "selected_property_name": ((str, type(None)), None),
        "domain_name": ((str, type(None)), None), "point_property": ((str, type(None)), None),
        "extra_property": ((str, type(None)), None), "extra_names_json": ((str, type(None)), None),
        "reset_extra_names": ((bool,), True), "save": ((bool,), True), "dry_run": ((bool,), False)}),
    "CubelessPCGTools.compile_or_notify_pcg_graph": _entry(_pcg_action("compile_or_notify_pcg_graph_action"), "write", {"graph_path": _S}),
    "CubelessPCGTools.save_pcg_graph": _entry(_pcg_action("save_pcg_graph_action"), "write", {"graph_path": _S}),
    "CubelessBlueprintGraphTools.add_blueprint_is_valid_exec_macro_node": _entry(_is_valid_exec_macro_action, "write", {
        "blueprint_name": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_data_table_bool_select_node": _entry(_data_table_bool_select_action, "write", {
        "blueprint_name": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_get_data_table_row_node": _entry(_data_table_row_node_action, "write", {
        "blueprint_name": _S, "row_struct_path": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_break_struct_node": _entry(_struct_break_action, "write", {
        "blueprint_name": _S, "struct_type": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_map_function_node": _entry(_map_function_action, "write", {
        "blueprint_name": _S, "operation": _S, "key_type": ((str,), "string"), "value_type": ((str,), "vector"),
        "param_defaults_json": ((str,), "{}"), "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_array_function_node": _entry(_array_function_action, "write", {
        "blueprint_name": _S, "operation": _S, "element_type": _S, "param_defaults_json": ((str,), "{}"),
        "type_object": ((str, type(None)), None), "enum_type": ((str, type(None)), None), "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintTools.add_blueprint_interface": _entry(_particle_receiver_action("add_blueprint_interface_action"), "write", {
        "blueprint_name": _S, "interface_class": _S}),
    "CubelessParticleReceiverTools.inspect_particle_receiver": _entry(_particle_receiver_action("inspect_particle_receiver_action"), "read_load", {
        "blueprint_name": _S, "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessParticleReceiverTools.add_particle_data_array_length_and_wire": _entry(_particle_receiver_action("add_particle_data_array_length_and_wire_action"), "write", {
        "blueprint_name": _S, "event_node_id": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessParticleReceiverTools.add_particle_data_array_get_copy_and_wire": _entry(_particle_receiver_action("add_particle_data_array_get_copy_and_wire_action"), "write", {
        "blueprint_name": _S, "event_node_id": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessParticleReceiverTools.add_basic_particle_data_break_node": _entry(_particle_receiver_action("add_basic_particle_data_break_node_action"), "write", {
        "blueprint_name": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_blueprint_custom_event_node": _entry(events.add_blueprint_custom_event_node_action, "write", {
        "blueprint_name": _S, "custom_event_name": _S, "node_position_json": ((str,), "[0,0]"),
        "signature_source_dispatcher_name": ((str, type(None)), None), "signature_source_class": ((str, type(None)), None),
        "signature_source_delegate_name": ((str, type(None)), None), "native_delegate_name": ((str, type(None)), None),
        "call_in_editor": ((bool, type(None)), None), "graph_name": ((str, type(None)), None),
        "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_input_key_node": _entry(events.add_blueprint_input_key_node_action, "write", {
        "blueprint_name": _S, "key": _S, "node_position_json": ((str,), "[0,0]"),
        "control": ((bool,), False), "alt": ((bool,), False), "shift": ((bool,), False), "command": ((bool,), False),
        "consume_input": ((bool,), True), "execute_when_paused": ((bool,), False), "override_parent_binding": ((bool,), False),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_event_node": _entry(events.add_blueprint_event_node_action, "write", {
        "blueprint_name": _S, "event_name": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_parent_function_call": _entry(events.add_blueprint_parent_function_call_action, "write", {
        "blueprint_name": _S, "event_node_id": _S, "node_position_json": ((str,), "[0,0]"),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintTools.duplicate_blueprint_to_temp": _entry(blueprint_duplicates.duplicate_blueprint_to_temp_action, "write", {
        "blueprint_name": _S, "target_path": _S, "required_root": ((str,), "/Game/_MCP_Temp"),
        "dry_run": ((bool,), True), "overwrite_existing": ((bool,), False), "allow_dirty_source": ((bool,), False),
        "compile": ((bool,), False), "save": ((bool,), True)}),
    "CubelessBlueprintGraphTools.resolve_blueprint_graph": _entry(functions.resolve_blueprint_graph_action, "write", {
        "blueprint_name": _S, "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_function_parameter": _entry(functions.add_blueprint_function_parameter_action, "write", {
        "blueprint_name": _S, "parameter_name": _S, "parameter_type": _S, "direction": ((str,), "input"),
        "default_value_json": ((str, type(None)), None), "type_object": ((str, type(None)), None), "is_array": ((bool,), False),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str,), "function"),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_return_node": _entry(functions.add_blueprint_return_node_action, "write", {
        "blueprint_name": _S, "node_position": ((str, list, type(None)), None),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str,), "function"), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_dynamic_cast_node": _entry(functions.add_blueprint_dynamic_cast_node_action, "write", {
        "blueprint_name": _S, "target_class": _S, "node_position": ((str, list, type(None)), None),
        "pure": ((bool,), False), "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_make_struct_node": _entry(_blueprint_make_struct_action, "write", {
        "blueprint_name": _S, "struct_type": _S, "node_position": ((list, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_blueprint_call_function_node": _entry(functions.add_blueprint_call_function_node_action, "write", {
        "blueprint_name": _S, "function_class": _S, "function_name": _S, "param_defaults_json": ((str,), "{}"),
        "node_position_json": ((str,), "[0,0]"), "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False),
        "allow_latent": ((bool,), False), "allow_editor_only": ((bool,), False), "allow_wildcard": ((bool,), False),
        "allow_deprecated": ((bool,), False), "allow_internal": ((bool,), False)}),
    "CubelessBlueprintTools.add_blueprint_variable": _entry(variables.add_blueprint_variable_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "variable_type": _S, "is_exposed": ((bool,), False),
        "default_value_json": ((str, type(None)), None), "category": ((str, type(None)), None),
        "tooltip": ((str, type(None)), None), "friendly_name": ((str, type(None)), None),
        "type_object": ((str, type(None)), None), "is_array": ((bool,), False), "metadata_json": ((str,), "{}"),
        "variable_guid": ((str, type(None)), None), "is_editable": ((bool, type(None)), None), "is_transient": ((bool,), False)}),
    "CubelessBlueprintTools.inspect_blueprint_variables": _entry(variables.inspect_blueprint_variables_action, "read_load", {"blueprint_name": _S}),
    "CubelessBlueprintTools.inspect_blueprint_variable_metadata": _entry(variables.inspect_blueprint_variable_metadata_action, "read_load", {
        "blueprint_name": _S, "variable_name": _S}),
    "CubelessBlueprintTools.set_blueprint_variable_default": _entry(variables.set_blueprint_variable_default_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "default_value_json": _S,
        "compile": ((bool,), True), "save": ((bool,), True), "dry_run": ((bool,), False)}),
    "CubelessBlueprintTools.create_blueprint": _entry(blueprint.create_blueprint_action, "write", {
        "name": _S, "parent_class": _S, "package_path": ((str,), "/Game/Blueprints")}),
    "CubelessBlueprintTools.inspect_blueprint": _entry(blueprint.inspect_blueprint_action, "read_load", {"blueprint_name": _S}),
    "CubelessBlueprintTools.compile_blueprint": _entry(blueprint.compile_blueprint_action, "write", {"blueprint_name": _S}, {"save": False, "refresh_nodes": False}),
    "CubelessBlueprintTools.compile_and_save_blueprint": _entry(blueprint.compile_blueprint_action, "write", {
        "blueprint_name": _S, "save": ((bool,), True)}, {"refresh_nodes": False}),
    "CubelessBlueprintTools.compile_and_validate_blueprint": _entry(blueprint.compile_blueprint_action, "write", {
        "blueprint_name": _S, "save": ((bool,), False), "refresh_nodes": ((bool,), True)}),
    "CubelessBlueprintGraphTools.inspect_graph": _entry(graph.inspect_graph_action, "read_load", {"blueprint_name": _S, "graph_name": _GRAPH}),
    "CubelessBlueprintGraphTools.add_blueprint_variable_get_node": _entry(graph_variables.add_blueprint_variable_get_node_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0),
        "target_class": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False), "node_position_json": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_blueprint_variable_set_node": _entry(graph_variables.add_blueprint_variable_set_node_action, "write", {
        "blueprint_name": _S, "variable_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0),
        "target_class": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "create_graph_if_missing": ((bool,), False), "node_position_json": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.set_blueprint_pin_default": _entry(pin_defaults.set_blueprint_pin_default_action, "write", {
        "blueprint_name": _S, "node_id": _S, "pin_name": _S, "value_json": _S, "direction": ((str, type(None)), None),
        "graph_name": _GRAPH, "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None)}),
    "CubelessBlueprintGraphTools.add_blueprint_branch_node": _entry(graph.add_blueprint_branch_node_action, "write", {
        "blueprint_name": _S, "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""),
        "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.add_branch_node": _entry(graph.add_branch_node_action, "write", {
        "blueprint_name": _S, "graph_name": _GRAPH, "x": ((int,), 0), "y": ((int,), 0)}),
    "CubelessBlueprintGraphTools.add_blueprint_sequence_node": _entry(graph.add_blueprint_sequence_node_action, "write", {
        "blueprint_name": _S, "node_position_json": ((str,), "[0,0]"), "output_count": ((int,), 2),
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None),
        "graph_type": ((str, type(None)), None), "create_graph_if_missing": ((bool,), False)}),
    "CubelessBlueprintGraphTools.set_node_position": _entry(graph.set_node_position_action, "write", {
        "blueprint_name": _S, "node_id": _S, "x": _I, "y": _I, "graph_name": _GRAPH,
        "expected_x": ((int, type(None)), None), "expected_y": ((int, type(None)), None)}),
    "CubelessBlueprintGraphTools.list_blueprint_nodes": _entry(_blueprint_node_cleanup_action("list_blueprint_nodes"), "read_load", {
        "blueprint_name": _S, "node_type": ((str,), ""), "title_contains": ((str,), ""), "include_pins": ((bool,), True),
        "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), "")}),
    "CubelessBlueprintGraphTools.delete_blueprint_node": _entry(_blueprint_node_cleanup_action("delete_blueprint_node"), "write", {
        "blueprint_name": _S, "node_id": _S, "graph_name": ((str,), ""), "graph_id": ((str,), ""), "graph_type": ((str,), ""),
        "expected_node_name": ((str,), ""), "expected_node_class": ((str,), ""), "expected_title_contains": ((str,), ""),
        "allow_non_exec_linked_delete": ((bool,), False), "allow_exec_linked_delete": ((bool,), False),
        "allow_any_linked_delete": ((bool,), False), "allow_orphaned_event_delete": ((bool,), False)}),
    "CubelessBlueprintGraphTools.delete_node": _entry(graph.delete_node_action, "write", {
        "blueprint_name": _S, "node_id": _S, "graph_name": _GRAPH, "allow_exec_linked_delete": ((bool,), False)}),
    "CubelessBlueprintGraphTools.connect_graph_pins": _entry(graph.connect_graph_pins_action, "write", {
        "blueprint_name": _S, "source_node_id": _S, "source_pin_name": _S,
        "target_node_id": _S, "target_pin_name": _S, "graph_name": _GRAPH}),
    "CubelessBlueprintGraphTools.connect_blueprint_graph_pins": _entry(graph.connect_blueprint_graph_pins_action, "write", {
        "blueprint_name": _S, "source_node_id": _S, "source_pin": _S, "target_node_id": _S, "target_pin": _S,
        "graph_name": ((str, type(None)), None), "graph_id": ((str, type(None)), None), "graph_type": ((str, type(None)), None),
        "allow_pin_link_replacement": ((bool,), False), "compile": ((bool,), False), "save_asset": ((bool,), False),
        "expected_existing_source_link_count": ((int, type(None)), None),
        "expected_existing_source_link_target_node_id": ((str, type(None)), None),
        "expected_existing_source_link_target_pin": ((str, type(None)), None), "expected_schema_response": ((str, type(None)), None)}),
    "CubelessComponentTools.list_blueprint_components": _entry(component.list_blueprint_components_action, "read_load", {
        "blueprint_name": _S, "component_name": ((str, type(None)), None), "max_components": ((int,), 10000)}),
    "CubelessComponentTools.get_component_property": _entry(component.get_component_property_action, "read_load", {
        "blueprint_name": _S, "component_name": _S, "property_name": _S,
        "max_components": ((int,), 10000), "max_value_chars": ((int,), 65536)}),
    "CubelessComponentTools.add_component_to_blueprint": _entry(component.add_component_to_blueprint_action, "write", {
        "blueprint_name": _S, "component_type": _S, "component_name": _S,
        "location_json": ((str,), "[0,0,0]"), "rotation_json": ((str,), "[0,0,0]"), "scale_json": ((str,), "[1,1,1]"),
        "parent_component": ((str, type(None)), None), "component_properties_json": ((str,), "{}"), "compile_blueprint": ((bool,), True)}),
    "CubelessComponentTools.set_component_property": _entry(component.set_component_property_action, "write", {
        "blueprint_name": _S, "component_name": _S, "property_name": _S, "property_value_json": _S}),
    "CubelessComponentTools.set_static_mesh_properties": _entry(component.set_static_mesh_properties_action, "write", {
        "blueprint_name": _S, "component_name": _S, "static_mesh": ((str,), "/Engine/BasicShapes/Cube.Cube"), "material": ((str, type(None)), None)}),
    "CubelessComponentTools.set_physics_properties": _entry(component.set_physics_properties_action, "write", {
        "blueprint_name": _S, "component_name": _S, "simulate_physics": ((bool,), True), "gravity_enabled": ((bool,), True),
        "mass": ((int, float), 1.0), "linear_damping": ((int, float), 0.01), "angular_damping": ((int, float), 0.0)}),
    "CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset": _entry(_chaos_cloth_inspection_action("inspect_chaos_cloth_asset"), "read_world", {
        'cloth_asset_path': ((str,), _REQUIRED),
        'lod_index': ((int,), -1),
        'property_name_filter': ((str,), ''),
        'include_property_values': ((bool,), True),
        'include_property_flags': ((bool,), True),
        'include_dataflow_nodes': ((bool,), True),
        'max_properties': ((int,), 512),
        'max_nodes': ((int,), 256),
    }),
    "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh": _entry(_chaos_cloth_inspection_action("inspect_chaos_cloth_simulation_mesh"), "read_world", {
        'cloth_asset_path': ((str,), _REQUIRED),
        'lod_index': ((int,), 0),
        'weight_map_name': ((str,), ''),
        'vertex_start': ((int,), 0),
        'max_vertices': ((int,), 4096),
        'face_start': ((int,), 0),
        'max_faces': ((int,), 4096),
        'include_positions': ((bool,), True),
        'include_normals': ((bool,), False),
        'include_indices': ((bool,), True),
        'include_weight_values': ((bool,), True),
    }),
    "CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections": _entry(_legacy_chaos_clothing_action("create_legacy_chaos_clothing_from_skeletal_sections"), "write", {
        'source_skeletal_mesh_path': ((str,), _REQUIRED),
        'output_skeletal_mesh_path': ((str,), _REQUIRED),
        'section_configs': ((list,), _REQUIRED),
        'dry_run': ((bool,), True),
        'save_assets': ((bool,), True),
        'use_source_physics_asset': ((bool,), True),
        'create_vroid_lower_body_cloth_physics_asset': ((bool,), False),
    }),
    "CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node": _entry(_chaos_cloth_authoring_action("configure_chaos_cloth_dataflow_node"), 'write', {
        'cloth_asset_path': ((str,), _REQUIRED),
        'node_name': ((str,), _REQUIRED),
        'property_values': ((dict,), _REQUIRED),
        'expected_node_type': ((str,), ''),
        'expected_dataflow_asset_path': ((str,), ''),
        'expected_current': ((dict, type(None),), None),
        'allow_connected_inputs': ((bool,), False),
        'regenerate_asset': ((bool,), True),
        'save_assets': ((bool,), False),
        'dry_run': ((bool,), True),
    }),
    "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map": _entry(_chaos_cloth_authoring_action("configure_chaos_cloth_weight_map"), 'write', {
        'cloth_asset_path': ((str,), _REQUIRED),
        'node_name': ((str,), _REQUIRED),
        'weight_map_name': ((str,), _REQUIRED),
        'weights': ((list, type(None),), None),
        'fill_value': ((float, type(None),), None),
        'lod_index': ((int,), 0),
        'expected_node_type': ((str,), ''),
        'expected_dataflow_asset_path': ((str,), ''),
        'expected_vertex_count': ((int,), -1),
        'expected_topology_md5': ((str,), ''),
        'expected_weight_map_md5': ((str,), ''),
        'expected_node_state_md5': ((str,), ''),
        'expected_path_md5': ((str,), ''),
        'require_runtime_binding': ((bool,), True),
        'save_assets': ((bool,), False),
        'dry_run': ((bool,), True),
    }),
    "CubelessChaosClothComponentTools.inspect_chaos_cloth_component": _entry(_chaos_cloth_component_action("inspect_chaos_cloth_component"), 'read_world', {
        'actor_name': ((str,), ''),
        'actor_label': ((str,), ''),
        'component_name': ((str,), ''),
        'model_index': ((int,), 0),
        'lod_index': ((int,), -1),
        'world_context': ((str,), 'auto'),
        'include_runtime_properties': ((bool,), True),
        'max_properties': ((int,), 512),
        'max_results': ((int,), 16),
    }),
    "CubelessChaosClothComponentTools.configure_chaos_cloth_component": _entry(_chaos_cloth_component_action("configure_chaos_cloth_component"), 'write', {
        'actor_name': ((str,), ''),
        'property_updates': ((list, type(None),), None),
        'component_settings': ((dict, type(None),), None),
        'component_name': ((str,), ''),
        'actor_label': ((str,), ''),
        'model_index': ((int,), 0),
        'lod_index': ((int,), -1),
        'world_context': ((str,), 'auto'),
        'expected_current': ((dict, type(None),), None),
        'allow_intrinsic_recreate': ((bool,), False),
        'dry_run': ((bool,), True),
        'recreate_simulation': ((str,), 'auto'),
        'save_level': ((bool,), False),
    }),
    "CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings": _entry(_chaos_solver_settings_action("inspect_chaos_solver_settings"), "read_world", {
        "actor_name": ((str,), ""), "actor_label": ((str,), ""), "world_context": ((str,), "auto"),
        "include_runtime_state": ((bool,), True), "max_results": ((int,), 16),
    }),
    "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings": _entry(_chaos_solver_settings_action("configure_chaos_solver_settings"), "write", {
        "settings": ((dict,), _REQUIRED),
        "actor_name": ((str,), ""), "actor_label": ((str,), ""), "world_context": ((str,), "editor"),
        "expected_current": ((dict, type(None)), None), "dry_run": ((bool,), True), "save_level": ((bool,), False),
    }),
    "CubelessActorPropertyFocusTools.set_actor_property": _entry(_actor_property_focus_action("set_actor_property"), "write", {
        "name": _S,
        "property_name": _S,
        "property_value": ((str, int, float, bool, list, dict, type(None)), _REQUIRED),
    }),
    "CubelessActorPropertyFocusTools.focus_viewport": _entry(_actor_property_focus_action("focus_viewport"), "write", {
        "target": ((str, type(None)), None),
        "location": ((list, type(None)), None),
        "distance": ((int, float), 1000.0),
        "orientation": ((list, type(None)), None),
    }),
    "CubelessEditorLevelSessionTools.open_editor_level": _entry(_editor_level_session_action("open_editor_level"), "write", {
        "level_path": _S,
        "dry_run": ((bool,), True),
        "allow_dirty_packages": ((bool,), False),
        "load_as_template": ((bool,), False),
        "show_progress": ((bool,), True),
        "deferred_load_delay_seconds": ((int, float), 1),
    }),
    "CubelessEditorLevelSessionTools.get_editor_level_open_status": _entry(_editor_level_session_action("get_editor_level_open_status"), "read", {
        "request_id": ((str,), ""),
    }),
    "CubelessEditorLevelSessionTools.clear_dirty_package_flags": _entry(_editor_level_session_action("clear_dirty_package_flags"), "write", {
        "package_paths": ((list, type(None)), None),
        "package_path": ((str,), ""),
        "clear_all_dirty": ((bool,), False),
        "dry_run": ((bool,), True),
        "allow_non_temp_path": ((bool,), False),
        "required_root": ((str,), "/Game/_MCP_Temp"),
    }),
    "CubelessEditorLevelSessionTools.safe_new_preview_map": _entry(_editor_level_session_action("safe_new_preview_map"), "write", {
        "target_path": ((str,), ""),
        "map_path": ((str,), ""),
        "dry_run": ((bool,), True),
        "allow_dirty_packages": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "allow_non_temp_path": ((bool,), False),
        "required_root": ((str,), "/Game/_MCP_Temp"),
        "is_partitioned_world": ((bool,), False),
    }),
    "CubelessNiagaraDataChannelAuthoringTools.configure_niagara_data_channel_asset": _entry(_niagara_data_channel_authoring_action("configure_niagara_data_channel_asset"), "write", {
        "asset_path": ((str,), _REQUIRED),
        "variables": ((list, type(None),), None),
        "create_if_missing": ((bool,), True),
        "keep_previous_frame_data": ((bool,), True),
        "save": ((bool,), True),
    }),
    "CubelessNiagaraDataChannelAuthoringTools.create_niagara_ndc_reader_scratch_pad_module": _entry(_niagara_data_channel_authoring_action("create_niagara_ndc_reader_scratch_pad_module"), "write", {
        "system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), _REQUIRED),
        "data_channel_asset_path": ((str,), _REQUIRED),
        "stage": ((str,), "particle_spawn_read"),
        "position_variable": ((str,), "Position"),
        "velocity_variable": ((str,), "Velocity"),
        "spawn_count_variable": ((str,), "SpawnCount"),
        "event_type_variable": ((str,), "EventType"),
        "position_target": ((str,), "Particles.Position"),
        "velocity_target": ((str,), "Particles.Velocity"),
        "spawn_count_target": ((str,), "Particles.NDCSpawnCount"),
        "event_type_target": ((str,), "Particles.NDCEventType"),
        "read_success_target": ((str,), "Particles.NDCReadSuccess"),
        "read_current_frame": ((bool,), False),
        "dry_run": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
    }),
    "CubelessNiagaraDataChannelAuthoringTools.add_niagara_scratch_pad_di_variadic_input": _entry(_niagara_data_channel_authoring_action("add_niagara_scratch_pad_di_variadic_input"), "write", {
        "system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), _REQUIRED),
        "function_node_guid": ((str,), _REQUIRED),
        "expected_function_name": ((str,), _REQUIRED),
        "expected_di_class": ((str,), _REQUIRED),
        "expected_existing_inputs": ((list,), _REQUIRED),
        "pin_name": ((str,), _REQUIRED),
        "module_input_name": ((str,), _REQUIRED),
        "expected_referencing_module_node_guid": ((str,), _REQUIRED),
        "default_value": ((str, int, float, bool, list, dict, type(None),), 0),
        "input_type": ((str,), "int"),
        "scratch_pad_script_path": ((str,), ""),
        "expected_input_map_node_guid": ((str,), ""),
        "expected_output_node_guid": ((str,), ""),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int, float,), 60.0),
        "poll_interval_seconds": ((int, float,), 0.1),
    }),
    # Original Native-only command: raw params remain Native-owned, without public Python arity.
    "CubelessNiagaraDataChannelAuthoringInternal.add_niagara_scratch_pad_di_function_call": _entry(_niagara_data_channel_authoring_action("add_niagara_scratch_pad_di_function_call"), "write", {}),
    "CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded": _entry(_geometry_collection_inspection_action("inspect_geometry_collection_performance_guarded"), "read_world", {
        'geometry_collection_path': ((str,), _REQUIRED),
        'actor_name': ((str,), ''),
        'tiny_relative_threshold': ((int, float), 0.04),
    }),
    "CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state": _entry(_geometry_collection_inspection_action("inspect_geometry_collection_runtime_state"), "read_world", {
        'component_name': ((str,), _REQUIRED),
        'actor_name': ((str,), ''),
        'actor_label': ((str,), ''),
        'include_pieces': ((bool,), True),
        'max_pieces': ((int,), 512),
        'pie_instance': ((int,), -1),
    }),
    "CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path": _entry(_niagara_module_input_paths_action("remove_niagara_unused_module_input_path"), "write", {
        'system_path': ((str,), _REQUIRED),
        'scratch_pad_name': ((str,), _REQUIRED),
        'module_name': ((str,), _REQUIRED),
        'module_node_guid': ((str,), _REQUIRED),
        'input_name': ((str,), _REQUIRED),
        'linked_user_parameter_name': ((str,), _REQUIRED),
        'custom_hlsl_signature_name': ((str,), _REQUIRED),
        'expected_hlsl_identifier_absent': ((str,), _REQUIRED),
        'custom_hlsl_input_name': ((str,), ''),
        'scratch_pad_script_path': ((str,), ''),
        'expected_hlsl_body_hash': ((str,), ''),
        'expected_custom_hlsl_node_guid': ((str,), ''),
        'expected_override_node_guid': ((str,), ''),
        'expected_input_type': ((str,), ''),
        'expected_user_source_node_guid': ((str,), ''),
        'expected_scratch_map_get_node_guids': ((list,type(None),), None),
        'dry_run': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), False),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter": _entry(_niagara_module_input_paths_action("set_niagara_module_input_user_parameter"), "write", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), _REQUIRED),
        'user_parameter_name': ((str,), _REQUIRED),
        'default_value': ((str,int,float,bool,list,dict,type(None),type(None),), None),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'overwrite_existing': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter": _entry(_niagara_module_input_paths_action("set_niagara_array_data_interface_module_input_user_parameter"), "write", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), _REQUIRED),
        'user_parameter_name': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'overwrite_existing': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters": _entry(_niagara_input_parameters_action("inspect_niagara_rapid_iteration_parameters"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'contains': ((list,type(None),), None),
        'max_parameters_per_script': ((int,), 256),
    }),
    "CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter": _entry(_niagara_input_parameters_action("set_niagara_rapid_iteration_parameter"), "write", {
        'system_path': ((str,), _REQUIRED),
        'parameter_name': ((str,), _REQUIRED),
        'value': ((str,int,float,bool,list,dict,type(None),), _REQUIRED),
        'owner_kind': ((str,), ''),
        'owner_name': ((str,), ''),
        'script_usage': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraInputParameterTools.set_niagara_module_static_switch": _entry(_niagara_input_parameters_action("set_niagara_module_static_switch"), "write", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), ''),
        'value': ((str,int,float,bool,list,dict,type(None),type(None),), None),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'expected_current_value': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
        'timeout_seconds': ((int,float,), 60.0),
        'poll_interval_seconds': ((int,float,), 0.1),
        'require_compile_gate_for_save': ((bool,), True),
    }),
    "CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs": _entry(_niagara_input_parameters_action("inspect_niagara_module_inputs"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'include_linked_sources': ((bool,), True),
        'include_resolved_stack_inputs': ((bool,), False),
        'max_modules': ((int,), 200),
        'max_candidates_per_module': ((int,), 24),
        'max_resolved_inputs_per_module': ((int,), 8),
        'max_top_candidates': ((int,), 80),
    }),
    "CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides": _entry(_niagara_input_parameters_action("inspect_niagara_data_interface_overrides"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'max_modules': ((int,), 200),
        'max_inputs_per_module': ((int,), 64),
        'include_data_interface_properties': ((bool,), False),
        'max_data_interface_properties': ((int,), 120),
        'max_data_interface_property_value_length': ((int,), 512),
    }),
    "CubelessNiagaraInputParameterTools.set_niagara_module_input_value": _entry(_niagara_input_parameters_action("set_niagara_module_input_value"), "write", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), _REQUIRED),
        'value': ((str,int,float,bool,list,dict,type(None),), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraInputParameterTools.create_niagara_module_input_override": _entry(_niagara_input_parameters_action("create_niagara_module_input_override"), "write", {
        'system_path': ((str,), _REQUIRED),
        'input_name': ((str,), _REQUIRED),
        'value': ((str,int,float,bool,list,dict,type(None),), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'module_index': ((int,type(None),), None),
        'module_name': ((str,), ''),
        'module_node_guid': ((str,), ''),
        'overwrite_existing': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraCleanupTools.cleanup_generated_niagara_assets_offline": _entry(_niagara_cleanup_action("cleanup_generated_niagara_assets_offline"), "write", {
        'package_paths': ((list, str, type(None),), None),
        'name_prefix': ((str,), ''),
        'older_than_minutes': ((int, float, type(None),), None),
        'dry_run': ((bool,), True),
        'require_editor_closed': ((bool,), True),
        'allow_all': ((bool,), False),
    }),

    "CubelessNiagaraCleanupTools.remove_niagara_modules": _entry(_niagara_cleanup_action("remove_niagara_modules"), "write", {
        'system_path': ((str,), _REQUIRED),
        'module_names': ((list,type(None),), None),
        'module_name_prefixes': ((list,type(None),), None),
        'module_name_contains': ((list,type(None),), None),
        'module_name': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'include_system_modules': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes": _entry(_niagara_cleanup_action("remove_niagara_parameter_map_set_nodes"), "write", {
        'system_path': ((str,), _REQUIRED),
        'parameter_pin_names': ((list,type(None),), None),
        'parameter_pin_name': ((str,), ''),
        'node_guid': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'dry_run': ((bool,), True),
        'allow_multiple': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes": _entry(_niagara_cleanup_action("remove_niagara_orphan_parameter_map_get_nodes"), "write", {
        'system_path': ((str,), _REQUIRED),
        'parameter_pin_names': ((list,type(None),), None),
        'parameter_pin_name': ((str,), ''),
        'node_guid': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'dry_run': ((bool,), True),
        'allow_multiple': ((bool,), False),
        'only_if_unlinked': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraCleanupTools.remove_niagara_user_parameters": _entry(_niagara_cleanup_action("remove_niagara_user_parameters"), "write", {
        'system_path': ((str,), _REQUIRED),
        'parameter_names': ((list,type(None),), None),
        'parameter_name': ((str,), ''),
        'dry_run': ((bool,), True),
        'remove_graph_references': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
        'continue_on_error': ((bool,), False),
    }),
    "CubelessNiagaraCleanupTools.remove_niagara_simulation_stage": _entry(_niagara_cleanup_action("remove_niagara_simulation_stage"), "write", {
        'system_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'stage_index': ((int,type(None),), None),
        'stage_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script": _entry(_niagara_cleanup_action("remove_niagara_scratch_pad_script"), "write", {
        'system_path': ((str,), _REQUIRED),
        'scratch_pad_name': ((str,), ''),
        'scratch_pad_owner_kind': ((str,), 'any'),
        'scratch_pad_script_index': ((int,type(None),), None),
        'scratch_pad_emitter_index': ((int,type(None),), None),
        'scratch_pad_emitter_name': ((str,), ''),
        'force': ((bool,), False),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_runtime_state": _entry(_niagara_runtime_inspection_action("inspect_niagara_component_runtime_state"), "read_world", {
        "actor_name": ((str,), ""),
        "actor_label": ((str,), ""),
        "component_name": ((str,), ""),
        "system_path": ((str,), ""),
        "parameter_name_contains": ((str,), ""),
        "max_components": ((int,), 32),
        "max_parameters": ((int,), 256),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_asset_exposed_parameters": ((bool,), False),
        "include_instance_parameters": ((bool,), False),
        "include_parameter_object_properties": ((bool,), False),
        "include_emitter_runtime": ((bool,), True),
        "max_emitter_instances": ((int,), 128),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_parameter_store": _entry(_niagara_runtime_inspection_action("inspect_niagara_component_parameter_store"), "read_world", {
        "actor_name": ((str,), ""),
        "actor_label": ((str,), ""),
        "component_name": ((str,), ""),
        "system_path": ((str,), ""),
        "parameter_name_contains": ((str,), ""),
        "max_components": ((int,), 8),
        "max_parameters": ((int,), 64),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
    }),
    "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_grid2d_runtime": _entry(_niagara_runtime_inspection_action("inspect_niagara_component_grid2d_runtime"), "read_world", {
        "actor_name": ((str,), ""),
        "actor_label": ((str,), ""),
        "component_name": ((str,), ""),
        "system_path": ((str,), ""),
        "max_components": ((int,), 16),
        "max_grid_interfaces": ((int,), 16),
        "include_internal": ((bool,), True),
    }),
    "CubelessNiagaraRuntimeInspectionTools.sample_render_target2d_region_stats": _entry(_niagara_runtime_inspection_action("sample_render_target2d_region_stats"), "read_world", {
        "render_target_path": ((str,), _REQUIRED),
        "x0": ((int,), 0),
        "x1": ((int,), -1),
        "y0": ((int,), 0),
        "y1": ((int,), -1),
        "channel": ((str,), "max"),
        "threshold": ((int,float,), 0.01),
        "max_first_hits": ((int,), 8),
        "probe_x": ((int,), -1),
        "probe_y": ((int,), -1),
        "probe_radius": ((int,), 8),
    }),
    "CubelessNiagaraCustomHlslConversionTools.apply_niagara_scratch_pad_graph_recipe": _entry(_niagara_custom_hlsl_conversion_action("apply_niagara_scratch_pad_graph_recipe"), "write", {
        "system_path": ((str,), _REQUIRED),
        "custom_hlsl_node_guid": ((str,), _REQUIRED),
        "nodes": ((list,tuple,), _REQUIRED),
        "links": ((list,tuple,), _REQUIRED),
        "output_mappings": ((list,tuple,), _REQUIRED),
        "scratch_pad_name": ((str,), ""),
        "scratch_pad_script_path": ((str,), ""),
        "scratch_pad_script_index": ((int,type(None),), None),
        "expected_hlsl_body_hash": ((str,), ""),
        "dry_run": ((bool,), True),
        "remove_custom_hlsl": ((bool,), True),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "timeout_seconds": ((int,float,), 30.0),
        "poll_interval_seconds": ((int,float,), 0.05),
    }),
    "CubelessPCGGraphAuthoringTools.add_pcg_node": _entry(_pcg_graph_authoring_action("add_pcg_node"), "write", {
        "graph_path": ((str,), _REQUIRED),
        "settings_class": ((str,), _REQUIRED),
        "node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "node_title": ((str,), ""),
        "settings": ((dict, type(None)), None),
    }),
    "CubelessPCGGraphAuthoringTools.connect_pcg_nodes": _entry(_pcg_graph_authoring_action("connect_pcg_nodes"), "write", {
        "graph_path": ((str,), _REQUIRED),
        "from_node_id": ((str,), _REQUIRED),
        "from_pin": ((str,), _REQUIRED),
        "to_node_id": ((str,), _REQUIRED),
        "to_pin": ((str,), _REQUIRED),
    }),
    "CubelessPCGGraphAuthoringTools.set_pcg_node_setting": _entry(_pcg_graph_authoring_action("set_pcg_node_setting"), "write", {
        "graph_path": ((str,), _REQUIRED),
        "node_id": ((str,), _REQUIRED),
        "property_name": ((str,), _REQUIRED),
        "value": ((str, int, float, bool, list, dict, type(None)), _REQUIRED),
    }),
    "CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template": _entry(_blueprint_niagara_template_swap_action("inspect_blueprint_niagara_component_template"), "read_world", {
        "blueprint_path": ((str,), _REQUIRED),
        "component_name": ((str,), ""),
        "expected_system_path": ((str,), ""),
        "parameter_name_contains": ((str,), ""),
        "include_inherited_scs_templates": ((bool,), True),
        "require_exactly_one": ((bool,), False),
        "require_expected_system_match": ((bool,), False),
        "max_components": ((int,), 64),
        "max_parameters": ((int,), 256),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_parameter_object_properties": ((bool,), False),
        "include_component_object_properties": ((bool,), False),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap": _entry(_blueprint_niagara_template_swap_action("dry_run_blueprint_niagara_component_template_asset_swap"), "read_world", {
        "blueprint_path": ((str,), _REQUIRED),
        "component_name": ((str,), _REQUIRED),
        "source_system_path": ((str,), _REQUIRED),
        "replacement_system_path": ((str,), _REQUIRED),
        "confirmation_token": ((str,), _REQUIRED),
        "operation": ((str,), "durable_bp_component_template_asset_swap"),
        "dry_run": ((bool,), True),
        "preserve_override_parameters": ((bool,), True),
        "allow_unrealmcp_dependency": ((bool,), False),
        "parameter_name_contains": ((str,), ""),
        "include_inherited_scs_templates": ((bool,), True),
        "require_exactly_one": ((bool,), True),
        "require_source_system_match": ((bool,), True),
        "max_components": ((int,), 8),
        "max_parameters": ((int,), 256),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_parameter_object_properties": ((bool,), False),
        "include_component_object_properties": ((bool,), False),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary": _entry(_blueprint_niagara_template_swap_action("execute_blueprint_niagara_component_template_asset_swap_canary"), "write", {
        "source_blueprint_path": ((str,), _REQUIRED),
        "target_blueprint_path": ((str,), _REQUIRED),
        "component_name": ((str,), _REQUIRED),
        "source_system_path": ((str,), _REQUIRED),
        "replacement_system_path": ((str,), _REQUIRED),
        "confirmation_token": ((str,), _REQUIRED),
        "operation": ((str,), "temp_bp_component_template_asset_swap_canary"),
        "dry_run": ((bool,), True),
        "preserve_override_parameters": ((bool,), True),
        "allow_unrealmcp_dependency": ((bool,), False),
        "replace_existing_target": ((bool,), False),
        "allow_dirty_source_blueprint": ((bool,), False),
        "compile_blueprint": ((bool,), False),
        "save_blueprint": ((bool,), False),
        "parameter_name_contains": ((str,), ""),
        "include_inherited_scs_templates": ((bool,), False),
        "require_exactly_one": ((bool,), True),
        "require_source_system_match": ((bool,), True),
        "max_components": ((int,), 8),
        "max_parameters": ((int,), 256),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_parameter_object_properties": ((bool,), False),
        "include_component_object_properties": ((bool,), False),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable": _entry(_blueprint_niagara_template_swap_action("execute_blueprint_niagara_component_template_asset_swap_durable"), "write", {
        "blueprint_path": ((str,), _REQUIRED),
        "component_name": ((str,), _REQUIRED),
        "source_system_path": ((str,), _REQUIRED),
        "replacement_system_path": ((str,), _REQUIRED),
        "confirmation_token": ((str,), _REQUIRED),
        "operation": ((str,), "durable_bp_component_template_asset_swap"),
        "dry_run": ((bool,), True),
        "preserve_override_parameters": ((bool,), True),
        "allow_unrealmcp_dependency": ((bool,), False),
        "allow_dirty_blueprint": ((bool,), False),
        "compile_blueprint": ((bool,), False),
        "save_blueprint": ((bool,), False),
        "parameter_name_contains": ((str,), ""),
        "include_inherited_scs_templates": ((bool,), False),
        "require_exactly_one": ((bool,), True),
        "require_source_system_match": ((bool,), True),
        "max_components": ((int,), 8),
        "max_parameters": ((int,), 256),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_parameter_object_properties": ((bool,), False),
        "include_component_object_properties": ((bool,), False),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessNiagaraPreviewWorkflowTools.open_niagara_preview_player": _entry(_niagara_preview_workflows_action("open_niagara_preview_player"), "write", {
        "system_path": ((str,), ""),
    }),
    "CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_player_state": _entry(_niagara_preview_workflows_action("get_niagara_preview_player_state"), "read_world", {
    }),
    "CubelessNiagaraPreviewWorkflowTools.preview_niagara_component_asset_swap": _entry(_niagara_preview_workflows_action("preview_niagara_component_asset_swap"), "write", {
        "replacement_system_path": ((str,), _REQUIRED),
        "actor_name": ((str,), ""),
        "actor_label": ((str,), ""),
        "component_name": ((str,), ""),
        "system_path": ((str,), ""),
        "parameter_name_contains": ((str,), ""),
        "restore_original_asset": ((bool,), True),
        "preserve_override_parameters": ((bool,), True),
        "activate_after_swap": ((bool,), True),
        "advance_time": ((int, float,), 0.35),
        "advance_tick_delta": ((int, float,), 0.03333333333333333),
        "max_parameters": ((int,), 256),
        "max_emitter_instances": ((int,), 128),
        "include_parameter_values": ((bool,), True),
        "include_raw_bytes": ((bool,), False),
        "max_raw_bytes": ((int,), 64),
        "include_parameter_object_properties": ((bool,), False),
        "include_emitter_runtime": ((bool,), True),
        "max_object_properties": ((int,), 80),
        "max_array_elements": ((int,), 16),
        "max_property_value_chars": ((int,), 512),
    }),
    "CubelessNiagaraPreviewWorkflowTools.sample_niagara_system_in_preview_lab": _entry(_niagara_preview_workflows_action("sample_niagara_system_in_preview_lab"), "write", {
        "system_path": ((str,), _REQUIRED),
        "output_dir": ((str,), ""),
        "label": ((str,), ""),
        "warmup_times": ((list, type(None),), None),
        "views": ((list, type(None),), None),
        "warmup_tick_delta": ((int, float,), 0.03333333333333333),
        "cleanup_before": ((bool,), True),
        "cleanup_after_each": ((bool,), True),
        "cleanup_after_all": ((bool,), True),
        "location": ((list, type(None),), None),
        "scale": ((list, type(None),), None),
        "auto_frame": ((bool,), True),
        "camera_location": ((list, type(None),), None),
        "camera_rotation": ((list, type(None),), None),
        "camera_fov": ((int, float, type(None),), None),
        "user_parameter_overrides": ((dict, type(None),), None),
        "preserve_override_parameters": ((bool,), True),
        "allow_current_map_preview": ((bool,), False),
    }),

    "CubelessPIEInputTools.inject_pie_input_key": _entry(_pie_input_action("inject_pie_input_key"), "runtime_write", {
        "key": ((str,), _REQUIRED),
        "event": ((str,), _REQUIRED),
        "pie_instance": ((int,), -1),
    }),

    "CubelessNiagaraSystemAnalysisTools.analyze_niagara_system": _entry(_niagara_system_analysis_action("analyze_niagara_system"), "read_world", {
        "system_path": ((str,), _REQUIRED),
        "include_renderers": ((bool,), True),
        "include_user_parameters": ((bool,), True),
        "include_stack": ((bool,), True),
        "include_graph": ((bool,), True),
        "include_module_inputs": ((bool,), True),
        "include_compile_status": ((bool,), True),
        "include_pins": ((bool,), False),
        "include_links": ((bool,), False),
        "include_scratch_pads": ((bool,), True),
        "include_resolved_stack_inputs": ((bool,), False),
        "max_function_calls": ((int,), 200),
        "max_nodes_per_graph": ((int,), 300),
        "max_links_per_graph": ((int,), 0),
        "max_modules": ((int,), 200),
        "max_candidates_per_module": ((int,), 24),
        "max_resolved_inputs_per_module": ((int,), 8),
        "max_top_candidates": ((int,), 80),
    }),

    "CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest": _entry(_niagara_promotion_manifest_action("plan_niagara_promotion_manifest"), "read_world", {
        "source_system_path": ((str,), ""),
        "source_system_paths": ((list,tuple,type(None),), None),
        "target_root": ((str,), "/Game/MCPWeather/Generated"),
        "target_system_paths": ((list,tuple,type(None),), None),
        "asset_name_prefix": ((str,), ""),
        "asset_name_suffix": ((str,), ""),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "forbidden_source_roots": ((list,tuple,type(None),), None),
        "allow_non_temp_source": ((bool,), False),
        "allow_temp_target": ((bool,), False),
        "allow_temp_dependencies": ((bool,), False),
        "write_manifest": ((bool,), False),
        "manifest_name": ((str,), ""),
        "timeout_seconds": ((int,float,), 120.0),
    }),

    "CubelessNiagaraPromotionManifestTools.execute_niagara_promotion_manifest": _entry(_niagara_promotion_manifest_action("execute_niagara_promotion_manifest"), "write", {
        "manifest": ((dict,list,tuple,str,int,float,bool,type(None),), None),
        "manifest_path": ((str,), ""),
        "confirmation_token": ((str,), ""),
        "require_confirmation": ((bool,), True),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "write_execution_report": ((bool,), False),
        "execution_report_name": ((str,), ""),
        "timeout_seconds": ((int,float,), 180.0),
    }),

    "CubelessPCGComponentGenerationTools.refresh_pcg_components": _entry(_pcg_component_generation_action("refresh_pcg_components"), "write", {
        "actor_name": ((str,), ""),
        "selected_only": ((bool,), False),
        "cleanup": ((bool,), True),
        "generate": ((bool,), True),
        "refresh": ((bool,), False),
        "force": ((bool,), True),
        "wait_until_complete": ((bool,), False),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.05),
        "min_generated_output_data_count": ((int,), -1),
        "min_managed_resource_count": ((int,), -1),
        "max_components": ((int,), 1000),
        "native_first": ((bool,), True),
    }),

    "CubelessPCGComponentGenerationTools.bind_pcg_generation_settings_to_blueprint": _entry(_pcg_component_generation_action("bind_pcg_generation_settings_to_blueprint"), "write", {
        "blueprint_name": ((str,), _REQUIRED),
        "component_variable": ((str,), "ForestPCG"),
        "generation_trigger_variable": ((str,), "PCGGenerationTrigger"),
        "regenerate_in_editor_variable": ((str,), "PCGRegenerateInEditor"),
        "generate_on_drop_variable": ((str,), ""),
        "generate_on_drop_default": ((bool,), False),
        "graph_name": ((str,), "UserConstructionScript"),
        "graph_id": ((str,), ""),
        "graph_type": ((str,), "function"),
        "replace_existing": ((bool,), True),
        "sync_component_template_defaults": ((bool,), True),
        "function_class": ((str,), ""),
        "function_name": ((str,), "ApplyPCGGenerationSettings"),
        "compile": ((bool,), False),
        "save": ((bool,), False),
    }),

    "CubelessPCGTools.pcg_actor_smoke_test": _entry(_pcg_actor_smoke_action, "write", {
        "graph_path": ((str,), _REQUIRED), "actor_label": ((str,), ""),
        "selected_only": ((bool,), False), "cleanup": ((bool,), True),
        "generate": ((bool,), True), "dry_run": ((bool,), True),
    }),
    "CubelessPCGTools.validate_pcg_source_independence": _entry(_pcg_promotion_action("validate_pcg_source_independence"), "read_world", {
        "graph_path": ((str,), _REQUIRED),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "allowed_dependency_prefixes": ((list,tuple,type(None),), None),
    }),
    "CubelessPCGTools.promote_pcg_temp_graph": _entry(_pcg_promotion_action("promote_pcg_temp_graph"), "write", {
        "source_graph_path": ((str,), _REQUIRED),
        "target_graph_path": ((str,), _REQUIRED),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "allowed_dependency_prefixes": ((list,tuple,type(None),), None),
        "overwrite_existing": ((bool,), False),
        "allow_non_temp_source": ((bool,), False),
        "dry_run": ((bool,), True),
    }),

    "CubelessPCGContractAuthoringTools.audit_pcg_graph_contract": _entry(_pcg_contract_authoring_action("audit_pcg_graph_contract"), "read_world", {
        "graph_path": ((str,), _REQUIRED),
        "include_dependencies": ((bool,), True),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "fail_on_forbidden_dependencies": ((bool,), True),
    }),

    "CubelessPCGContractAuthoringTools.read_pcg_node_contract": _entry(_pcg_contract_authoring_action("read_pcg_node_contract"), "read_world", {
        "graph_path": ((str,), _REQUIRED),
        "node_id": ((str,), ""),
        "include_dependencies": ((bool,), False),
    }),

    "CubelessPCGContractAuthoringTools.set_pcg_static_mesh_spawner_entries": _entry(_pcg_contract_authoring_action("set_pcg_static_mesh_spawner_entries"), "write", {
        "graph_path": ((str,), _REQUIRED),
        "node_id": ((str,), _REQUIRED),
        "mesh_entries": ((list,tuple,type(None),), None),
        "selector_type": ((str,), "weighted"),
        "attribute_name": ((str,), "Meshes"),
        "material_override_attributes": ((list,tuple,type(None),), None),
        "save": ((bool,), True),
    }),

    "CubelessPCGUtilityTools.list_pcg_assets": _entry(_pcg_utilities_action("list_pcg_assets"), "read_world", {
        "root_path": ((str,), "/Game"),
    }),

    "CubelessPCGUtilityTools.list_pcg_components": _entry(_pcg_utilities_action("list_pcg_components"), "read_world", {

    }),

    "CubelessPCGUtilityTools.set_pcg_debug_enabled": _entry(_pcg_utilities_action("set_pcg_debug_enabled"), "write", {
        "enabled": ((bool,), False),
        "actor_name": ((str,), ""),
        "selected_only": ((bool,), False),
    }),

    "CubelessPCGUtilityTools.resave_pcg_assets": _entry(_pcg_utilities_action("resave_pcg_assets"), "write", {
        "root_path": ((str,), "/Game"),
    }),

    "CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton": _entry(_controlrig_recipe_planning_action("profile_controlrig_skeleton"), "read_world", {
        "request": ((dict,str), _REQUIRED),
    }),

    "CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe": _entry(_controlrig_recipe_planning_action("compile_controlrig_profile_recipe"), "read_world", {
        "profile": ((dict,str), _REQUIRED),
        "request": ((dict,str), _REQUIRED),
    }),

    "CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe": _entry(_controlrig_recipe_planning_action("make_controlrig_preset_recipe"), "read_world", {
        "preset": ((dict,str), _REQUIRED),
    }),

    "CubelessBlueprintPropertyGraphTools.set_blueprint_property": _entry(_blueprint_property_graph_action("set_blueprint_property"), "write", {
        "blueprint_name": ((str,), _REQUIRED),
        "property_name": ((str,), _REQUIRED),
        "property_value": ((str, int, float, bool, list, dict, type(None),), _REQUIRED),
    }),

    "CubelessBlueprintPropertyGraphTools.list_blueprint_graphs": _entry(_blueprint_property_graph_action("list_blueprint_graphs"), "read_world", {
        "blueprint_name": ((str,), _REQUIRED),
        "graph_type": ((str,), ""),
    }),

    'CubelessNiagaraScratchUserRecipeTools.create_niagara_visible_scratch_pad_snow_seed': _entry(_niagara_scratch_user_recipes_action('create_niagara_visible_scratch_pad_snow_seed'), "write", {
        'target_system_path': ((str,), _REQUIRED),
        'preset': ((str,), 'snow'),
        'emitter_name': ((str,), 'Emitter'),
        'material_path': ((str,), ''),
        'use_project_weather_materials': ((bool,), True),
        'scratch_pad_name': ((str,), 'MCP_VisibleSizeScratch'),
        'parameter_name': ((str,), 'Particles.SpriteSize'),
        'parameter_value': ((str, int, float, bool, list, dict, type(None),), None),
        'user_parameter_name': ((str,), 'User.MCP_VisibleScratchPad_SpriteSize'),
        'user_parameter_default_value': ((str, int, float, bool, list, dict, type(None),), None),
        'target_usage': ((str,), 'ParticleUpdateScript'),
        'spawn_rate': ((int, float, type(None),), 180.0),
        'velocity': ((list, type(None),), None),
        'sprite_size': ((list, type(None),), None),
        'lifetime': ((int, float, type(None),), 10.0),
        'add_spawn_box_location': ((bool, type(None),), True),
        'spawn_box_size': ((list, type(None),), None),
        'spawn_box_offset': ((list, type(None),), None),
        'add_weather_detail_modules': ((bool, type(None),), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
        'timeout_seconds': ((int,), 180),
    }),

    "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_vec2_user_parameter_module": _entry(_niagara_scratch_user_recipes_action("create_niagara_scratch_pad_vec2_user_parameter_module"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), "MCP_BlankUpdateScratch"),
        "parameter_name": ((str,), "Particles.SpriteSize"),
        "parameter_value": ((str, int, float, bool, list, dict, type(None),), None),
        "user_parameter_name": ((str,), "User.MCP_BlankScratchPad_SpriteSize"),
        "user_parameter_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "target_usage": ((str,), "ParticleUpdateScript"),
        "target_owner_kind": ((str,), "system"),
        "target_emitter_index": ((int, type(None),), None),
        "target_emitter_name": ((str,), ""),
        "target_stack_emitter_index": ((int, type(None),), 0),
        "target_stack_emitter_name": ((str,), ""),
        "stack_suggested_name": ((str,), ""),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,), 180),
    }),

    "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_color_user_parameter_module": _entry(_niagara_scratch_user_recipes_action("create_niagara_scratch_pad_color_user_parameter_module"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), "MCP_ColorScratch"),
        "parameter_name": ((str,), "Particles.Color"),
        "parameter_value": ((str, int, float, bool, list, dict, type(None),), None),
        "user_parameter_name": ((str,), "User.MCP_ScratchPad_Color"),
        "user_parameter_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "target_usage": ((str,), "ParticleUpdateScript"),
        "target_owner_kind": ((str,), "system"),
        "target_emitter_index": ((int, type(None),), None),
        "target_emitter_name": ((str,), ""),
        "target_stack_emitter_index": ((int, type(None),), 0),
        "target_stack_emitter_name": ((str,), ""),
        "stack_suggested_name": ((str,), ""),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,), 180),
    }),

    "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_user_parameter_module": _entry(_niagara_scratch_user_recipes_action("create_niagara_scratch_pad_bool_user_parameter_module"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), "MCP_BoolUserScratch"),
        "parameter_name": ((str,), "Particles.MCPBoolProbe"),
        "parameter_value": ((str, int, float, bool, list, dict, type(None),), None),
        "module_input_name": ((str,), "MCPBoolInput"),
        "module_input_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "user_parameter_name": ((str,), "User.MCP_ScratchPad_Bool"),
        "user_parameter_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "target_usage": ((str,), "ParticleUpdateScript"),
        "target_owner_kind": ((str,), "system"),
        "target_emitter_index": ((int, type(None),), None),
        "target_emitter_name": ((str,), ""),
        "target_stack_emitter_index": ((int, type(None),), 0),
        "target_stack_emitter_name": ((str,), ""),
        "stack_suggested_name": ((str,), ""),
        "overwrite_existing": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,), 180),
    }),

    "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_visible_toggle_module": _entry(_niagara_scratch_user_recipes_action("create_niagara_scratch_pad_bool_visible_toggle_module"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "scratch_pad_name": ((str,), "MCP_BoolVisibleScratch"),
        "parameter_name": ((str,), "Particles.SpriteSize"),
        "parameter_value": ((str, int, float, bool, list, dict, type(None),), None),
        "base_module_input_name": ((str,), "MCPBaseSpriteSize"),
        "base_module_input_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "user_parameter_name": ((str,), "User.MCP_VisibleToggle"),
        "user_parameter_default_value": ((str, int, float, bool, list, dict, type(None),), None),
        "custom_hlsl_input_name": ((str,), "MCPVisibleToggle"),
        "custom_hlsl_expression": ((str,), "BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)"),
        "custom_hlsl_output_pin_name": ((str,), "MCPVisibleSize"),
        "custom_hlsl_signature_name": ((str,), "MCP_ScratchPadBoolVisibleSize"),
        "target_usage": ((str,), "ParticleUpdateScript"),
        "target_owner_kind": ((str,), "system"),
        "target_emitter_index": ((int, type(None),), None),
        "target_emitter_name": ((str,), ""),
        "target_stack_emitter_index": ((int, type(None),), 0),
        "target_stack_emitter_name": ((str,), ""),
        "stack_suggested_name": ((str,), ""),
        "overwrite_existing": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,), 180),
    }),

    "CubelessBlueprintResolutionConnectionTools.resolve_blueprint": _entry(_blueprint_resolution_connection_action("resolve_blueprint"), "read_world", {
        "blueprint_name": ((str,), _REQUIRED),
    }),

    "CubelessBlueprintResolutionConnectionTools.connect_blueprint_nodes": _entry(_blueprint_resolution_connection_action("connect_blueprint_nodes"), "write", {
        "blueprint_name": ((str,), _REQUIRED),
        "source_node_id": ((str,), _REQUIRED),
        "source_pin": ((str,), _REQUIRED),
        "target_node_id": ((str,), _REQUIRED),
        "target_pin": ((str,), _REQUIRED),
        "graph_name": ((str,), ""),
        "graph_id": ((str,), ""),
        "graph_type": ((str,), ""),
        "allow_pin_link_replacement": ((bool,), False),
    }),

    "CubelessTextureMaterialTools.import_create_and_apply_texture_set": _entry(_texture_material_authoring_action("import_create_and_apply_texture_set"), "write", {
        'base_color_image_path': ((str,), _REQUIRED),
        'output_name': ((str,), _REQUIRED),
        'unreal_folder': ((str,), '/Game/AI_Generated'),
        'normal_image_path': ((str,), ''),
        'roughness_image_path': ((str,), ''),
        'ambient_occlusion_image_path': ((str,), ''),
        'metallic_image_path': ((str,), ''),
        'target': ((str,), ''),
        'material_slot': ((int,), 0),
        'base_material_path': ((str,), ''),
    }),

    "CubelessTextureMaterialTools.generate_pbr_companion_textures": _entry(_texture_material_authoring_action("generate_pbr_companion_textures"), "write", {
        'base_color_image_path': ((str,), _REQUIRED),
        'output_name': ((str,), ''),
        'output_dir': ((str,), ''),
        'normal_strength': ((int,float,), 2.0),
        'roughness': ((int,float,), 0.75),
        'roughness_detail': ((int,float,), 0.15),
        'ambient_occlusion_strength': ((int,float,), 0.35),
        'metallic': ((int,float,), 0.0),
    }),

    "CubelessTextureMaterialTools.import_texture_to_unreal": _entry(_texture_material_authoring_action("import_texture_to_unreal"), "write", {
        'image_path': ((str,), _REQUIRED),
        'unreal_folder': ((str,), _REQUIRED),
        'asset_name': ((str,), _REQUIRED),
    }),

    "CubelessTextureMaterialTools.import_texture_set_to_unreal": _entry(_texture_material_authoring_action("import_texture_set_to_unreal"), "write", {
        'base_color_image_path': ((str,), _REQUIRED),
        'unreal_folder': ((str,), _REQUIRED),
        'asset_name': ((str,), _REQUIRED),
        'normal_image_path': ((str,), ''),
        'roughness_image_path': ((str,), ''),
        'ambient_occlusion_image_path': ((str,), ''),
        'metallic_image_path': ((str,), ''),
    }),

    "CubelessTextureMaterialTools.apply_material_to_mesh": _entry(_texture_material_authoring_action("apply_material_to_mesh"), "write", {
        'target': ((str,), _REQUIRED),
        'material_asset_path': ((str,), _REQUIRED),
        'material_slot': ((int,), 0),
    }),

    "CubelessTextureMaterialTools.generate_texture_from_prompt": _entry(_texture_material_authoring_action("generate_texture_from_prompt"), "write", {
        'prompt': ((str,), _REQUIRED),
        'output_name': ((str,), _REQUIRED),
        'output_dir': ((str,), _REQUIRED),
        'size': ((str,), '1024x1024'),
    }),

    "CubelessTextureMaterialTools.generate_texture_for_mesh_uv": _entry(_texture_material_authoring_action("generate_texture_for_mesh_uv"), "write", {
        'mesh_path': ((str,), _REQUIRED),
        'prompt': ((str,), _REQUIRED),
        'uv_channel': ((int,), 0),
        'output_name': ((str,), 'T_AI_Texture'),
        'output_dir': ((str,), ''),
        'size': ((str,), '1024x1024'),
    }),

    "CubelessTextureMaterialTools.generate_and_apply_ai_texture": _entry(_texture_material_authoring_action("generate_and_apply_ai_texture"), "write", {
        'target': ((str,), _REQUIRED),
        'prompt': ((str,), _REQUIRED),
        'output_name': ((str,), _REQUIRED),
        'unreal_folder': ((str,), '/Game/AI_Generated'),
        'uv_channel': ((int,), 0),
        'size': ((str,), '1024x1024'),
    }),

    "CubelessTextureMaterialTools.get_static_mesh_uv_layout": _entry(_texture_material_authoring_action("get_static_mesh_uv_layout"), "write", {
        "mesh_path": ((str,), _REQUIRED),
        "uv_channel": ((int,), 0),
        "output_path": ((str,), ""),
    }),

    "CubelessTextureMaterialTools.create_material_instance_with_texture": _entry(_texture_material_authoring_action("create_material_instance_with_texture"), "write", {
        "texture_asset_path": ((str,), _REQUIRED),
        "material_name": ((str,), _REQUIRED),
        "unreal_folder": ((str,), _REQUIRED),
        "base_material_path": ((str,), ""),
    }),

    "CubelessTextureMaterialTools.create_material_with_texture_set": _entry(_texture_material_authoring_action("create_material_with_texture_set"), "write", {
        "base_color_texture_asset_path": ((str,), _REQUIRED),
        "material_name": ((str,), _REQUIRED),
        "unreal_folder": ((str,), _REQUIRED),
        "normal_texture_asset_path": ((str,), ""),
        "roughness_texture_asset_path": ((str,), ""),
        "ambient_occlusion_texture_asset_path": ((str,), ""),
        "metallic_texture_asset_path": ((str,), ""),
        "base_material_path": ((str,), ""),
    }),

    "CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe": _entry(_controlrig_recipe_authoring_action("validate_controlrig_recipe"), "read_world", {
        "recipe": ((dict,str), _REQUIRED),
    }),

    "CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe": _entry(_controlrig_recipe_authoring_action("dry_run_controlrig_recipe"), "read_world", {
        "recipe": ((dict,str), _REQUIRED),
    }),

    "CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe": _entry(_controlrig_recipe_authoring_action("build_controlrig_recipe"), "write", {
        "recipe": ((dict,str), _REQUIRED),
        "options": ((dict,str,type(None)), None),
    }),

    "CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe_with_animbp_binding": _entry(_controlrig_recipe_authoring_action("build_controlrig_recipe_with_animbp_binding"), "write", {
        "recipe": ((dict,str), _REQUIRED),
        "options": ((dict,str,type(None)), None),
    }),

    "CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state": _entry(_niagara_preview_lab_action("get_niagara_preview_lab_state"), "read_world", {

    }),
    "CubelessNiagaraPreviewLabTools.cleanup_niagara_preview_lab": _entry(_niagara_preview_lab_action("cleanup_niagara_preview_lab"), "write", {

    }),
    "CubelessNiagaraPreviewLabTools.capture_niagara_preview_lab_view": _entry(_niagara_preview_lab_action("capture_niagara_preview_lab_view"), "write", {
        "filepath": ((str,), _REQUIRED),
        "view": ((int,), 1),
        "auto_frame": ((bool,), True),
        "camera_location": ((list,tuple,type(None),), None),
        "camera_rotation": ((list,tuple,type(None),), None),
        "camera_fov": ((int,float,type(None),), None),
        "allow_current_map_preview": ((bool,), False),
    }),
    "CubelessNiagaraPreviewLabTools.preview_niagara_system_in_preview_lab": _entry(_niagara_preview_lab_action("preview_niagara_system_in_preview_lab"), "write", {
        "system_path": ((str,), _REQUIRED),
        "filepath": ((str,), _REQUIRED),
        "view": ((int,), 1),
        "label": ((str,), ""),
        "warmup_time": ((int,float,), 0.35),
        "warmup_tick_delta": ((int,float,), 0.03333333333333333),
        "cleanup_before": ((bool,), True),
        "cleanup_after": ((bool,), True),
        "location": ((list,tuple,type(None),), None),
        "scale": ((list,tuple,type(None),), None),
        "auto_frame": ((bool,), True),
        "camera_location": ((list,tuple,type(None),), None),
        "camera_rotation": ((list,tuple,type(None),), None),
        "camera_fov": ((int,float,type(None),), None),
        "user_parameter_overrides": ((dict,type(None),), None),
        "preserve_override_parameters": ((bool,), True),
        "allow_current_map_preview": ((bool,), False),
    }),
    "CubelessNiagaraCustomHlslConversionTools.replace_niagara_custom_hlsl": _entry(_niagara_custom_hlsl_conversion_action("replace_niagara_custom_hlsl"), "write", {
        "system_path": ((str,), _REQUIRED),
        "replacement_hlsl": ((str,), _REQUIRED),
        "node_guid": ((str,), ""),
        "signature_name": ((str,), ""),
        "contains_text": ((str,), ""),
        "scratch_pad_name": ((str,), ""),
        "source_path_contains": ((str,), ""),
        "expected_contains": ((str,), ""),
        "dry_run": ((bool,), True),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "timeout_seconds": ((int,float,), 30.0),
        "poll_interval_seconds": ((int,float,), 0.05),
        "max_hlsl_chars": ((int,), 8000),
    }),
    "CubelessNiagaraSpritePresetTools.create_sprite_niagara_preset_system": _entry(_niagara_sprite_preset_authoring_action("create_sprite_niagara_preset_system"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "preset": ((str,), "basic"),
        "emitter_name": ((str,), "Emitter"),
        "material_path": ((str,), ""),
        "spawn_rate": ((int,float,type(None),), None),
        "velocity": ((list,tuple,type(None),), None),
        "alignment": ((str,), ""),
        "facing_mode": ((str,), ""),
        "sub_image_size": ((list,tuple,type(None),), None),
        "fixed_bounds_min": ((list,tuple,type(None),), None),
        "fixed_bounds_max": ((list,tuple,type(None),), None),
        "local_space": ((bool,type(None),), None),
        "sprite_size": ((list,tuple,type(None),), None),
        "lifetime": ((int,float,type(None),), None),
        "use_project_weather_materials": ((bool,), False),
        "add_spawn_box_location": ((bool,type(None),), None),
        "spawn_box_size": ((list,tuple,type(None),), None),
        "spawn_box_offset": ((list,tuple,type(None),), None),
        "add_weather_detail_modules": ((bool,type(None),), None),
        "allow_non_temp_path": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraPromotionTools.create_niagara_system_from_source_template": _entry(_niagara_source_template_action("create_niagara_system_from_source_template"), "write", {
        "target_system_path": ((str,), _REQUIRED), "source_system_path": ((str,), _REQUIRED),
        "source_emitter_indices": ((list,tuple,type(None),), None),
        "copy_source_user_parameters": ((bool,), True), "save": ((bool,), True),
        "request_compile": ((bool,), True), "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,), 180),
    }),

    "CubelessNiagaraPromotionTools.audit_niagara_promotion_candidate": _entry(_niagara_promotion_action("audit_niagara_promotion_candidate"), "read_world", {
        "system_path": ((str,), _REQUIRED),
        "proposed_target_path": ((str,), ""),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "forbidden_source_roots": ((list,tuple,type(None),), None),
        "allow_temp_target": ((bool,), False),
        "allow_temp_dependencies": ((bool,), False),
        "timeout_seconds": ((int,float,), 60.0),
    }),
    "CubelessNiagaraPromotionTools.promote_niagara_system_candidate": _entry(_niagara_promotion_action("promote_niagara_system_candidate"), "write", {
        "source_system_path": ((str,), _REQUIRED),
        "target_system_path": ((str,), _REQUIRED),
        "dry_run": ((bool,), True),
        "forbidden_dependency_prefixes": ((list,tuple,type(None),), None),
        "forbidden_source_roots": ((list,tuple,type(None),), None),
        "allow_non_temp_source": ((bool,), False),
        "allow_temp_target": ((bool,), False),
        "allow_temp_dependencies": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "timeout_seconds": ((int,float,), 120.0),
    }),
    "CubelessNiagaraEventStageTools.inspect_niagara_simulation_stages": _entry(_niagara_event_stage_authoring_action("inspect_niagara_simulation_stages"), "read_world", {
        "system_path": ((str,), _REQUIRED),
        "include_compile_data": ((bool,), True),
        "include_script_compile_status": ((bool,), True),
        "max_stages": ((int,), 128),
    }),
    "CubelessNiagaraEventStageTools.inspect_niagara_event_handlers": _entry(_niagara_event_stage_authoring_action("inspect_niagara_event_handlers"), "read_world", {
        "system_path": ((str,), _REQUIRED),
        "emitter_index": ((int,type(None),), None),
        "emitter_name": ((str,), ""),
        "max_handlers": ((int,), 256),
    }),
    "CubelessNiagaraEventStageTools.add_niagara_event_handler": _entry(_niagara_event_stage_authoring_action("add_niagara_event_handler"), "write", {
        "system_path": ((str,), _REQUIRED),
        "emitter_index": ((int,type(None),), None),
        "emitter_name": ((str,), ""),
        "source_emitter_index": ((int,type(None),), None),
        "source_emitter_name": ((str,), ""),
        "source_emitter_id": ((str,), ""),
        "source_event_name": ((str,), "CollisionEvent"),
        "execution_mode": ((str,), "SpawnedParticles"),
        "spawn_number": ((int,), 1),
        "max_events_per_frame": ((int,), 64),
        "random_spawn_number": ((bool,), False),
        "min_spawn_number": ((int,), 0),
        "update_attribute_initial_values": ((bool,), True),
        "skip_if_duplicate": ((bool,), True),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,float,), 60.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_module_input": _entry(_niagara_scratch_stack_authoring_action("add_niagara_scratch_pad_module_input"), "write", {
        "system_path": ((str,), _REQUIRED),
        "input_name": ((str,), _REQUIRED),
        "default_value": ((str,int,float,bool,list,dict,type(None),), _REQUIRED),
        "input_type": ((str,), "bool"),
        "consumer_pin_name": ((str,), ""),
        "scratch_pad_script_path": ((str,), ""),
        "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int,type(None),), None),
        "scratch_pad_name": ((str,), ""),
        "scratch_pad_emitter_index": ((int,type(None),), None),
        "scratch_pad_emitter_name": ((str,), ""),
        "require_consumer_link": ((bool,), True),
        "overwrite_existing_link": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), False),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_assignment_module_to_stack": _entry(_niagara_scratch_stack_authoring_action("add_niagara_scratch_pad_assignment_module_to_stack"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "parameter_name": ((str,), "Particles.SpriteSize"),
        "parameter_type": ((str,), "vec2"),
        "default_value": ((str,), "50.000000,50.000000"),
        "value": ((str,int,float,bool,list,dict,type(None),), None),
        "target_usage": ((str,), "ParticleUpdateScript"),
        "target_usage_id": ((str,), ""),
        "target_emitter_index": ((int,type(None),), None),
        "target_emitter_name": ((str,), ""),
        "target_index": ((int,type(None),), None),
        "skip_if_duplicate": ((bool,), True),
        "allow_assignment_module_route": ((str,), ""),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraScratchStackAuthoringTools.wrap_niagara_scratch_pad_output_with_stack_context": _entry(_niagara_scratch_stack_authoring_action("wrap_niagara_scratch_pad_output_with_stack_context"), "write", {
        "system_path": ((str,), _REQUIRED),
        "target_pin_name": ((str,), "StackContext.RGBA"),
        "scratch_pad_script_path": ((str,), ""),
        "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int,type(None),), None),
        "scratch_pad_name": ((str,), "RenderCircleToGrid"),
        "scratch_pad_emitter_index": ((int,type(None),), None),
        "scratch_pad_emitter_name": ((str,), ""),
        "target_node_guid": ((str,), ""),
        "target_node_index": ((int,type(None),), None),
        "parameter_map_set_index": ((int,type(None),), None),
        "previous_stack_value_pin_name": ((str,), "PreviousStackValue"),
        "local_value_pin_name": ((str,), "LocalStampValue"),
        "expression": ((str,), "max(PreviousStackValue, LocalStampValue)"),
        "skip_if_already_wrapped": ((bool,), True),
        "replace_existing_custom": ((bool,), True),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
    }),
    "CubelessNiagaraResourceBindingTools.set_niagara_render_target2d_module_input": _entry(_niagara_resource_bindings_action("set_niagara_render_target2d_module_input"), "write", {
        "system_path": ((str,), _REQUIRED),
        "input_name": ((str,), _REQUIRED),
        "user_parameter_name": ((str,), "User.RT_IF_Deform"),
        "render_target_asset_path": ((str,), ""),
        "emitter_index": ((int,type(None),), None),
        "emitter_name": ((str,), ""),
        "module_index": ((int,type(None),), None),
        "module_name": ((str,), ""),
        "module_node_guid": ((str,), ""),
        "inherit_user_parameter_settings": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), False),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraResourceBindingTools.set_niagara_texture2d_module_input": _entry(_niagara_resource_bindings_action("set_niagara_texture2d_module_input"), "write", {
        "system_path": ((str,), _REQUIRED),
        "input_name": ((str,), _REQUIRED),
        "user_parameter_name": ((str,), "User.T_Mask"),
        "texture_asset_path": ((str,), ""),
        "emitter_index": ((int,type(None),), None),
        "emitter_name": ((str,), ""),
        "module_index": ((int,type(None),), None),
        "module_name": ((str,), ""),
        "module_node_guid": ((str,), ""),
        "overwrite_existing": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
        "wait_for_completion": ((bool,), False),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraResourceBindingTools.set_niagara_scratch_pad_function_input_default": _entry(_niagara_resource_bindings_action("set_niagara_scratch_pad_function_input_default"), "write", {
        "system_path": ((str,), _REQUIRED),
        "function_name": ((str,), ""),
        "input_pin_name": ((str,), ""),
        "value": ((str,int,float,bool,list,dict,type(None),), None),
        "default_value": ((str,), ""),
        "function_node_guid": ((str,), ""),
        "function_node_index": ((int,type(None),), None),
        "function_call_index": ((int,type(None),), None),
        "scratch_pad_script_path": ((str,), ""),
        "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int,type(None),), None),
        "scratch_pad_name": ((str,), ""),
        "scratch_pad_emitter_index": ((int,type(None),), None),
        "scratch_pad_emitter_name": ((str,), ""),
        "break_links": ((bool,), True),
        "allow_multi_link_break": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
    }),
    "CubelessNiagaraResourceBindingTools.link_niagara_scratch_pad_pin_to_user_parameter": _entry(_niagara_resource_bindings_action("link_niagara_scratch_pad_pin_to_user_parameter"), "write", {
        "system_path": ((str,), _REQUIRED),
        "target_pin_name": ((str,), _REQUIRED),
        "user_parameter_name": ((str,), _REQUIRED),
        "scratch_pad_script_path": ((str,), ""),
        "scratch_pad_owner_kind": ((str,), "system"),
        "scratch_pad_script_index": ((int,type(None),), None),
        "scratch_pad_name": ((str,), ""),
        "scratch_pad_emitter_index": ((int,type(None),), None),
        "scratch_pad_emitter_name": ((str,), ""),
        "target_node_guid": ((str,), ""),
        "target_node_index": ((int,type(None),), None),
        "parameter_map_set_index": ((int,type(None),), None),
        "default_value": ((str,int,float,bool,list,dict,type(None),), None),
        "overwrite_existing": ((bool,), False),
        "allow_multi_link_break": ((bool,), False),
        "allow_source_edit": ((bool,), False),
        "save": ((bool,), True),
        "request_compile": ((bool,), True),
    }),
    "CubelessNiagaraFactoryBootstrapTools.inspect_niagara_factory_bootstrap_defaults": _entry(_niagara_factory_bootstrap_action("inspect_niagara_factory_bootstrap_defaults"), "read_world", {
        "kind": ((str,), "all"),
        "create_default_system_nodes": ((bool,), True),
        "add_default_modules_and_renderers": ((bool,), False),
        "include_pins": ((bool,), False),
        "max_function_calls": ((int,), 80),
    }),
    "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_system": _entry(_niagara_factory_bootstrap_action("create_empty_niagara_system"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "allow_non_temp_path": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_emitter": _entry(_niagara_factory_bootstrap_action("create_empty_niagara_emitter"), "write", {
        "target_emitter_path": ((str,), _REQUIRED),
        "add_default_modules_and_renderers": ((bool,), False),
        "allow_non_temp_path": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
    }),
    "CubelessNiagaraFactoryBootstrapTools.create_basic_sprite_niagara_system": _entry(_niagara_factory_bootstrap_action("create_basic_sprite_niagara_system"), "write", {
        "target_system_path": ((str,), _REQUIRED),
        "emitter_name": ((str,), "Emitter"),
        "material_path": ((str,), ""),
        "allow_non_temp_path": ((bool,), False),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
        "wait_for_completion": ((bool,), True),
        "timeout_seconds": ((int,float,), 10.0),
        "poll_interval_seconds": ((int,float,), 0.1),
    }),
    "CubelessPCGInspectionTools.resolve_pcg_graph": _entry(_pcg_inspection_action("resolve_pcg_graph"), "write", {
        'graph_path': ((str,), _REQUIRED),
    }),
    "CubelessPCGInspectionTools.list_pcg_graph_nodes": _entry(_pcg_inspection_action("list_pcg_graph_nodes"), "read_world", {
        'graph_path': ((str,), _REQUIRED),
        'node_type': ((str,), ''),
        'title_contains': ((str,), ''),
        'include_pins': ((bool,), True),
    }),
    "CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes": _entry(_pcg_inspection_action("inspect_pcg_custom_hlsl_nodes"), "read_world", {
        'graph_path': ((str,), _REQUIRED),
        'node_id': ((str,), ''),
        'title_contains': ((str,), ''),
        'include_pins': ((bool,), True),
        'include_declarations': ((bool,), True),
        'max_source_chars': ((int,), 4000),
    }),
    "CubelessPCGInspectionTools.inspect_pcg_component_outputs": _entry(_pcg_inspection_action("inspect_pcg_component_outputs"), "read_world", {
        'actor_label': ((str,), ''),
        'actor_name': ((str,), ''),
        'actor_path': ((str,), ''),
        'actor_tag': ((str,), ''),
        'actor_label_prefix': ((str,), ''),
        'component_name': ((str,), ''),
        'component_tag': ((str,), ''),
        'selected_only': ((bool,), False),
        'max_components': ((int,), 16),
        'max_tagged_data': ((int,), 64),
        'max_points': ((int,), 8),
        'include_point_samples': ((bool,), True),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack": _entry(_niagara_authoring_inspection_action("inspect_niagara_stack"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'include_pins': ((bool,), False),
        'max_function_calls': ((int,), 200),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph": _entry(_niagara_authoring_inspection_action("inspect_niagara_graph"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'include_pins': ((bool,), True),
        'include_links': ((bool,), True),
        'include_scratch_pads': ((bool,), True),
        'max_nodes_per_graph': ((int,), 600),
        'max_links_per_graph': ((int,), 2000),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions": _entry(_niagara_authoring_inspection_action("inspect_niagara_graph_actions"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'scratch_pad_name': ((str,), ''),
        'scratch_pad_script_path': ((str,), ''),
        'scratch_pad_script_index': ((int,type(None),), None),
        'safe_only': ((bool,), True),
        'search_text': ((str,), ''),
        'max_actions': ((int,), 600),
        'include_canonical': ((bool,), False),
        'include_template_pins': ((bool,), False),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes": _entry(_niagara_authoring_inspection_action("inspect_niagara_custom_hlsl_nodes"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'node_guid': ((str,), ''),
        'signature_name': ((str,), ''),
        'contains_text': ((str,), ''),
        'max_hlsl_chars': ((int,), 4000),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface": _entry(_niagara_authoring_inspection_action("inspect_niagara_scratch_pad_interface"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'include_graph_summary': ((bool,), True),
        'include_parent_scratch_pads': ((bool,), True),
        'max_scripts': ((int,), 200),
        'max_function_calls': ((int,), 80),
    }),
    "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status": _entry(_niagara_authoring_inspection_action("inspect_niagara_compile_status"), "write", {
        'system_path': ((str,), _REQUIRED),
        'request_compile': ((bool,), False),
        'force': ((bool,), False),
        'allow_source_compile': ((bool,), False),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values": _entry(_niagara_parameter_collections_action("inspect_niagara_parameter_collection_values"), "read_world", {
        'collection_path': ((str,), _REQUIRED),
        'parameter_names': ((list,type(None),), None),
        'namespace': ((str,), ''),
        'max_parameters': ((int,), 256),
    }),
    "CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values": _entry(_niagara_parameter_collections_action("set_niagara_parameter_collection_values"), "write", {
        'collection_path': ((str,), _REQUIRED),
        'values': ((dict,), _REQUIRED),
        'namespace': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
    }),
    "CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection": _entry(_niagara_parameter_collections_action("repair_niagara_parameter_collection"), "write", {
        'collection_path': ((str,), _REQUIRED),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
    }),
    "CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references": _entry(_niagara_parameter_collections_action("remap_niagara_parameter_collection_references"), "write", {
        'system_path': ((str,), _REQUIRED),
        'new_collection_path': ((str,), _REQUIRED),
        'old_collection_path': ((str,), ''),
        'old_namespace': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'replace_object_references': ((bool,), False),
    }),
    "CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references": _entry(_niagara_parameter_collections_action("remap_niagara_script_parameter_collection_references"), "write", {
        'new_collection_path': ((str,), _REQUIRED),
        'old_collection_path': ((str,), ''),
        'old_namespace': ((str,), ''),
        'script_path': ((str,), ''),
        'script_paths': ((list,type(None),), None),
        'root_path': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
    }),
    "CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root": _entry(_niagara_parameter_collections_action("recompile_niagara_scripts_in_root"), "write", {
        'root_path': ((str,), ''),
        'script_paths': ((list,type(None),), None),
        'allow_source_compile': ((bool,), False),
        'force': ((bool,), True),
        'save': ((bool,), True),
    }),
    "CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties": _entry(_niagara_emitter_configuration_action("inspect_niagara_emitter_properties"), "read_world", {
        'system_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
    }),
    "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled": _entry(_niagara_emitter_configuration_action("set_niagara_emitter_enabled"), "write", {
        'system_path': ((str,), _REQUIRED),
        'enabled': ((bool,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space": _entry(_niagara_emitter_configuration_action("set_niagara_emitter_local_space"), "write", {
        'system_path': ((str,), _REQUIRED),
        'value': ((bool,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target": _entry(_niagara_emitter_configuration_action("set_niagara_emitter_sim_target"), "write", {
        'system_path': ((str,), _REQUIRED),
        'sim_target': ((str,), _REQUIRED),
        'auto_fixed_bounds': ((bool,), True),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds": _entry(_niagara_emitter_configuration_action("set_niagara_emitter_fixed_bounds"), "write", {
        'system_path': ((str,), _REQUIRED),
        'min': ((list,type(None),), None),
        'max': ((list,type(None),), None),
        'mode': ((str,), ''),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids": _entry(_niagara_emitter_configuration_action("set_niagara_emitter_persistent_ids"), "write", {
        'system_path': ((str,), _REQUIRED),
        'enabled': ((bool,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), True),
        'timeout_seconds': ((int,float,), 60.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter": _entry(_niagara_emitter_configuration_action("remove_niagara_emitter"), "write", {
        'system_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.inspect_niagara_renderers": _entry(_niagara_renderers_action("inspect_niagara_renderers"), "read_world", {
        'system_path': ((str,), _REQUIRED),
    }),
    "CubelessNiagaraRendererTools.set_niagara_renderer_material": _entry(_niagara_renderers_action("set_niagara_renderer_material"), "write", {
        'system_path': ((str,), _REQUIRED),
        'material_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'renderer_index': ((int,type(None),), None),
        'material_slot_index': ((int,), 0),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings": _entry(_niagara_renderers_action("set_niagara_sprite_renderer_settings"), "write", {
        'system_path': ((str,), _REQUIRED),
        'alignment': ((str,), ''),
        'facing_mode': ((str,), ''),
        'sub_image_size': ((list,type(None),), None),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'renderer_index': ((int,type(None),), None),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.add_niagara_sprite_renderer": _entry(_niagara_renderers_action("add_niagara_sprite_renderer"), "write", {
        'system_path': ((str,), _REQUIRED),
        'material_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'alignment': ((str,), 'Automatic'),
        'facing_mode': ((str,), 'FaceCamera'),
        'sub_image_size': ((list,type(None),), None),
        'skip_if_existing': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.add_niagara_mesh_renderer": _entry(_niagara_renderers_action("add_niagara_mesh_renderer"), "write", {
        'system_path': ((str,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'meshes_binding_parameter': ((str,), 'User.MeshList'),
        'mesh_index_binding': ((str,), 'Particles.MeshIndex'),
        'mesh_paths': ((list,type(None),), None),
        'skip_if_existing': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.set_niagara_renderer_enabled": _entry(_niagara_renderers_action("set_niagara_renderer_enabled"), "write", {
        'system_path': ((str,), _REQUIRED),
        'enabled': ((bool,), _REQUIRED),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'renderer_index': ((int,type(None),), None),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
    }),
    "CubelessNiagaraRendererTools.cleanup_niagara_renderers": _entry(_niagara_renderers_action("cleanup_niagara_renderers"), "write", {
        'system_path': ((str,), _REQUIRED),
        'remove_renderer_indices': ((list,type(None),), None),
        'clear_meshes_binding_renderer_indices': ((list,type(None),), None),
        'expected_renderers': ((list,type(None),), None),
        'expected_renderer_count': ((int,type(None),), None),
        'emitter_index': ((int,type(None),), None),
        'emitter_name': ((str,), ''),
        'dry_run': ((bool,), True),
        'allow_source_edit': ((bool,), False),
        'save': ((bool,), True),
        'request_compile': ((bool,), True),
        'wait_for_completion': ((bool,), False),
        'timeout_seconds': ((int,float,), 10.0),
        'poll_interval_seconds': ((int,float,), 0.1),
    }),
    "CubelessContentReconstructionTools.recreate_content_folder_mcp": _entry(_content_reconstruction_action("recreate_content_folder_mcp"), "write", {
        'source_root': ((str,), '/Game/UltraDynamicSky'),
        'target_root': ((str,), '/Game/_MCP_Temp/UltraDynamicSky_MCP'),
        'suffix': ((str,), '_MCP'),
        'allow_fallback_duplicate': ((bool,), True),
        'dry_run': ((bool,), False),
        'overwrite_existing': ((bool,), False),
        'delete_target_root_first': ((bool,), False),
        'remap_references': ((bool,), True),
        'compile_blueprints': ((bool,), True),
        'log_archive_remap_objects': ((bool,), False),
        'refresh_blueprint_nodes': ((bool,), False),
        'refresh_failed_blueprint_nodes': ((bool,), True),
        'save_assets': ((bool,), True),
        'repair_level_actors': ((bool,), True),
        'force_repair_level_actors': ((bool,), True),
        'compile_passes': ((int,), 2),
        'check_editor_log_health': ((bool,), True),
        'fail_on_editor_log_issues': ((bool,), True),
        'suppress_editor_prompts': ((bool,), True),
    }),
    "CubelessContentReconstructionTools.postprocess_content_folder_mcp": _entry(_content_reconstruction_action("postprocess_content_folder_mcp"), "write", {
        'source_root': ((str,), '/Game/UltraDynamicSky'),
        'target_root': ((str,), '/Game/_MCP_Temp/UltraDynamicSky_MCP'),
        'suffix': ((str,), '_MCP'),
        'dry_run': ((bool,), False),
        'remap_references': ((bool,), True),
        'compile_blueprints': ((bool,), True),
        'refresh_blueprint_nodes': ((bool,), False),
        'refresh_failed_blueprint_nodes': ((bool,), True),
        'save_assets': ((bool,), True),
        'repair_level_actors': ((bool,), False),
        'force_repair_level_actors': ((bool,), False),
        'compile_passes': ((int,), 2),
        'check_editor_log_health': ((bool,), True),
        'fail_on_editor_log_issues': ((bool,), True),
        'suppress_editor_prompts': ((bool,), True),
    }),
    "CubelessContentReconstructionTools.repair_world_actor_instances_mcp": _entry(_content_reconstruction_action("repair_world_actor_instances_mcp"), "write", {
        'map_path': ((str,), _REQUIRED),
        'source_root': ((str,), '/Game/UltraDynamicSky'),
        'target_root': ((str,), '/Game/_MCP_Temp/UltraDynamicSky_MCP'),
        'suffix': ((str,), '_MCP'),
        'dry_run': ((bool,), False),
        'save_map': ((bool,), True),
        'remap_level_blueprint': ((bool,), False),
        'repair_actor_classes': ((bool,), True),
        'repair_component_classes': ((bool,), True),
        'remap_actor_references': ((bool,), True),
        'remove_source_object_map_keys': ((bool,), True),
        'check_editor_log_health': ((bool,), True),
        'fail_on_editor_log_issues': ((bool,), True),
        'suppress_editor_prompts': ((bool,), True),
    }),
    "CubelessContentReconstructionTools.run_content_validation_pipeline_mcp": _entry(_content_reconstruction_action("run_content_validation_pipeline_mcp"), "write", {
        'source_root': ((str,), _REQUIRED),
        'target_root': ((str,), _REQUIRED),
        'suffix': ((str,), '_MCP'),
        'map_path': ((str,), ''),
        'continue_on_failure': ((bool,), False),
        'run_world_repair': ((bool,), False),
        'allow_fallback_duplicate': ((bool,), True),
        'dry_run': ((bool,), False),
        'overwrite_existing': ((bool,), True),
        'delete_target_root_first': ((bool,), True),
        'remap_references': ((bool,), True),
        'compile_blueprints': ((bool,), True),
        'refresh_blueprint_nodes': ((bool,), False),
        'refresh_failed_blueprint_nodes': ((bool,), True),
        'save_assets': ((bool,), True),
        'repair_level_actors': ((bool,), True),
        'force_repair_level_actors': ((bool,), True),
        'compile_passes': ((int,), 3),
        'check_editor_log_health': ((bool,), True),
        'fail_on_editor_log_issues': ((bool,), True),
        'suppress_editor_prompts': ((bool,), True),
        'save_map': ((bool,), True),
        'repair_actor_classes': ((bool,), True),
        'repair_component_classes': ((bool,), True),
        'remap_actor_references': ((bool,), True),
        'remove_source_object_map_keys': ((bool,), True),
    }),
    "CubelessGASAuthoringTools.inspect_gas_blueprint": _entry(_gas_authoring_action("inspect_gas_blueprint"), "read_world", {
        'blueprint_path': ((str,), _REQUIRED),
    }),
    "CubelessGASAuthoringTools.create_gameplay_ability_blueprint": _entry(_gas_authoring_action("create_gameplay_ability_blueprint"), "write", {
        'asset_name': ((str,), _REQUIRED),
        'package_path': ((str,), '/Game/GAS/Abilities'),
        'save': ((bool,), True),
    }),
    "CubelessGASAuthoringTools.create_gameplay_effect_blueprint": _entry(_gas_authoring_action("create_gameplay_effect_blueprint"), "write", {
        'asset_name': ((str,), _REQUIRED),
        'package_path': ((str,), '/Game/GAS/Effects'),
        'save': ((bool,), True),
    }),
    "CubelessGASAuthoringTools.create_attribute_set_blueprint": _entry(_gas_authoring_action("create_attribute_set_blueprint"), "write", {
        'asset_name': ((str,), _REQUIRED),
        'package_path': ((str,), '/Game/GAS/Attributes'),
        'save': ((bool,), True),
    }),
    "CubelessGASAuthoringTools.add_ability_system_component_to_blueprint": _entry(_gas_authoring_action("add_ability_system_component_to_blueprint"), "write", {
        'blueprint_path': ((str,), _REQUIRED),
        'component_name': ((str,), 'AbilitySystem'),
        'save': ((bool,), True),
    }),
    "CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set": _entry(_gas_authoring_action("add_gameplay_attribute_to_attribute_set"), "write", {
        'attribute_set_path': ((str,), _REQUIRED),
        'attribute_name': ((str,), _REQUIRED),
        'category': ((str,), 'GAS|Attributes'),
        'save': ((bool,), True),
    }),
    "CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh": _entry(_chaos_geometry_authoring_action("create_geometry_collection_from_mesh"), "write", {
        'source_static_mesh_paths': ((list,), _REQUIRED),
        'output_geometry_collection_path': ((str,), _REQUIRED),
        'source_transforms': ((list, type(None)), None),
        'root_mesh_proxy_static_mesh_path': ((str,), ''),
        'material_reindex': ((bool,), True),
        'overwrite_existing': ((bool,), False),
    }),
    "CubelessChaosGeometryAuthoringTools.configure_building_fracture": _entry(_chaos_geometry_authoring_action("configure_building_fracture"), "write", {
        'source_geometry_collection_path': ((str,), _REQUIRED),
        'output_geometry_collection_path': ((str,), _REQUIRED),
        'fracture_method': ((str,), 'uniform_voronoi'),
        'site_count': ((int,), 64),
        'random_seed': ((int,), 1337),
        'cluster_levels': ((int,), 2),
        'cluster_size': ((int,), 8),
        'damage_thresholds': ((list, type(None)), None),
        'merge_tiny_geometry': ((bool,), True),
        'tiny_relative_threshold': ((int, float), 0.01),
        'collision_object_reduction_percentage': ((int, float), 20),
        'maximum_collision_particles': ((int,), 60),
        'collision_implicit_type': ((str,), 'convex'),
        'remove_on_sleep': ((bool,), True),
        'remove_on_break': ((bool,), True),
        'sleep_time_seconds': ((int, float), 5),
        'removal_duration_seconds': ((int, float), 1),
        'break_timer_seconds': ((int, float), 1),
        'root_mesh_proxy_static_mesh_path': ((str,), ''),
        'overwrite_existing': ((bool,), False),
    }),
    "CubelessChaosGeometryAuthoringTools.configure_anchor_field": _entry(_chaos_geometry_authoring_action("configure_anchor_field"), "write", {
        'geometry_collection_actor_name': ((str,), _REQUIRED),
        'field_actor_name': ((str,), 'Chaos_AnchorField'),
        'box_center': ((list, type(None)), None),
        'box_extent': ((list, type(None)), None),
        'field_magnitude': ((int,), 1),
        'save_level': ((bool,), True),
    }),
    "CubelessChaosTraceStrainTools.trigger_trace_external_strain": _entry(_chaos_trace_strain_action("trigger_trace_external_strain"), "runtime_write", {
        'operation': ((str,), 'begin'),
        'actor_name': ((str,), ''),
        'start': ((list, type(None)), None),
        'end': ((list, type(None)), None),
        'expected_hit': ((bool,), True),
        'trace_complex': ((bool,), True),
        'radius': ((int, float), 100),
        'strain': ((int, float), 100000),
        'propagation_depth': ((int,), 1),
        'propagation_factor': ((int, float), 0.5),
        'breaking_velocity': ((int, float), 800),
    }),
    "CubelessChaosLearningTools.create_chaos_destruction_learning_fixture": _entry(_chaos_learning_action("create_chaos_destruction_learning_fixture"), "write", {
        'dry_run': ((bool,), True),
        'force_rebuild': ((bool,), False),
        'save_assets': ((bool,), True),
        'configure_current_level': ((bool,), False),
        'asset_root': ((str,), '/Game/_MCP_Temp/ChaosDestructionLearning'),
    }),
    "CubelessChaosLearningTools.run_chaos_cache_smoke": _entry(_chaos_learning_action("run_chaos_cache_smoke"), "runtime_write", {
        'operation': ((str,), 'inspect'),
        'cache_collection_path': ((str,), '/Game/_MCP_Temp/ChaosDestructionLearning/CC_ChaosLearning'),
        'cache_manager_actor_name': ((str,), ''),
        'component_actor_name': ((str,), ''),
        'cache_name': ((str,), 'ChaosLearning'),
    }),
    "CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke": _entry(_chaos_destruction_performance_action("run_chaos_destruction_performance_smoke"), "runtime_write", {
        'operation': ((str,), 'sample'),
        'actor_name': ((str,), ''),
        'solver_actor_name': ((str,), ''),
        'start': ((list, type(None)), None),
        'end': ((list, type(None)), None),
        'expected_hit': ((bool,), True),
        'trace_count': ((int,), 100),
        'benchmark_path': ((str,), 'both'),
        'strain': ((int, float, type(None)), None),
        'radius': ((int, float), 100),
        'breaking_velocity': ((int, float), 600),
        'breaking_velocity_vector': ((list, type(None)), None),
        'item_index': ((int,), -1),
        'expected_direction': ((list, type(None)), None),
        'dispatch_world_time_seconds': ((int, float), -1),
        'witness_actor_name': ((str,), ''),
        'floor_actor_name': ((str,), 'Chaos_StaticFloor'),
        'one_way_level': ((int,), -1),
        'initial_witness_location': ((list, type(None)), None),
        'initial_witness_velocity': ((list, type(None)), None),
        'per_advance_breaks_allowed': ((int,), 2),
        'per_advance_breaks_reschedule_limit': ((int,), 64),
        'particle_release_throttling_min_count': ((int,), 64),
        'particle_release_throttling_max_count': ((int,), 128),
        'optimize_for_runtime_memory': ((bool,), True),
        'performance_scenario': ((str,), ''),
        'capture_limit_frames': ((int,), 90),
        'require_play_session': ((bool,), True),
    }),
    "CubelessChaosCacheSessionTools.record_chaos_cache": _entry(_chaos_cache_sessions_action("record_chaos_cache"), "runtime_write", {
        'operation': ((str,), _REQUIRED),
        'cache_manager_actor_name': ((str,), _REQUIRED),
        'geometry_collection_actor_names': ((list,), _REQUIRED),
        'cache_collection_path': ((str,), _REQUIRED),
        'cache_names': ((list, type(None)), None),
        'solver_actor_name': ((str,), ''),
        'duration_seconds': ((int, float), 2.0),
        'frame_rate': ((int, float), 30.0),
        'timeout_seconds': ((int, float), 10.0),
        'initial_trigger_mode': ((str,), 'strain_velocity'),
        'motion_velocity': ((list, type(None)), None),
        'per_advance_breaks_allowed': ((int,), 8),
        'per_advance_breaks_reschedule_limit': ((int,), 64),
        'particle_release_throttling_min_count': ((int,), 64),
        'particle_release_throttling_max_count': ((int,), 128),
    }),
    "CubelessChaosCacheSessionTools.validate_chaos_cache": _entry(_chaos_cache_sessions_action("validate_chaos_cache"), "runtime_write", {
        'operation': ((str,), _REQUIRED),
        'cache_manager_actor_name': ((str,), _REQUIRED),
        'cache_collection_path': ((str,), _REQUIRED),
        'sequence_path': ((str,), _REQUIRED),
        'geometry_collection_actor_names': ((list, type(None)), None),
        'sample_times': ((list, type(None)), None),
        'scrub_frames': ((list, type(None)), None),
        'repeat_count': ((int,), 2),
        'timeout_seconds': ((int, float), 10.0),
    }),
    "CubelessChaosCinematicSequenceTools.create_chaos_level_sequence": _entry(_chaos_cinematic_sequence_action("create_chaos_level_sequence"), "write", {
        'output_sequence_path': ((str,), _REQUIRED),
        'camera_actor_name': ((str,), ''),
        'display_rate': ((int, float), 30.0),
        'tick_resolution': ((int,), 24000),
        'start_frame': ((int,), 0),
        'end_frame': ((int,), 60),
        'overwrite_existing': ((bool,), False),
    }),
    "CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence": _entry(_chaos_cinematic_sequence_action("bind_chaos_cache_to_sequence"), "write", {
        'sequence_path': ((str,), _REQUIRED),
        'cache_manager_actor_name': ((str,), _REQUIRED),
        'cache_collection_path': ((str,), _REQUIRED),
        'section_start_frame': ((int,), 0),
        'section_end_frame': ((int,), 60),
        'cache_play_rate': ((int, float), 1.0),
        'cache_start_offset_seconds': ((int, float), 0.0),
    }),
    "CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing": _entry(_chaos_cinematic_sequence_action("configure_chaos_sequence_timing"), "write", {
        'sequence_path': ((str,), _REQUIRED),
        'display_rate': ((int, float), 30.0),
        'tick_resolution': ((int,), 24000),
        'start_frame': ((int,), 0),
        'end_frame': ((int,), 60),
        'section_start_frame': ((int,), 0),
        'section_end_frame': ((int,), 60),
        'cache_play_rate': ((int, float), 1.0),
        'cache_start_offset_seconds': ((int, float), 0.0),
        'reverse': ((bool,), False),
    }),
    "CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline": _entry(_chaos_cinematic_sequence_action("inspect_chaos_cinematic_pipeline"), "read_world", {
        'geometry_collection_path': ((str,), _REQUIRED),
        'sequence_path': ((str,), _REQUIRED),
        'cache_manager_actor_name': ((str,), _REQUIRED),
        'map_path': ((str,), _REQUIRED),
        'chaos_state_sha256': ((str,), _REQUIRED),
        'field_actor_name': ((str,), 'Chaos_AnchorField'),
        'require_movie_render_pipeline': ((bool,), True),
        'mrq_output_directory': ((str,), ''),
        'mrq_manifest_path': ((str,), ''),
        'expected_display_rate': ((int,), 30),
        'expected_tick_resolution': ((int,), 24000),
        'expected_playback_start_frame': ((int,), 0),
        'expected_playback_end_frame': ((int,), 60),
    }),
    "CubelessViewportCaptureTools.list_viewport_bookmarks": _entry(_viewport_capture_action("list_viewport_bookmarks"), "read", {}),
    "CubelessViewportCaptureTools.set_active_viewport_realtime": _entry(_viewport_capture_action("set_active_viewport_realtime"), "write", {
        "realtime": ((bool,), True), "realtime_frame_count": ((int,), 300),
    }),
    "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot": _entry(_viewport_capture_action("capture_viewport_bookmark_screenshot"), "write", {
        "filepath": _S, "bookmark_index": ((int,), -1), "redraw_count": ((int,), 2),
        "clean_game_view": ((bool,), False), "view_location": ((list, type(None)), None),
        "view_rotation": ((list, type(None)), None), "view_fov": ((int, float, type(None)), None),
        "restore_view_after_capture": ((bool,), True),
    }),
    "CubelessViewportCaptureTools.take_screenshot": _entry(_viewport_capture_action("take_screenshot"), "write", {"filepath": _S}),
    "CubelessLandscapeTools.list_landscapes": _entry(_landscape_action("list_landscapes"), "read", {}),
    "CubelessLandscapeTools.get_landscape_info": _entry(_landscape_action("get_landscape_info"), "read", {"actor_label": ((str,), "")}),
    "CubelessLandscapeTools.create_landscape": _entry(_landscape_action("create_landscape"), "write", {
        "actor_label": ((str,), "MCP_Landscape"), "material_path": ((str,), ""),
        "location": ((list, type(None)), None), "scale": ((list, type(None)), None),
        "component_count_x": ((int,), 1), "component_count_y": ((int,), 1),
        "sections_per_component": ((int,), 1), "quads_per_section": ((int,), 7),
        "height_value": ((int,), 32768), "allow_non_temp_level": ((bool,), False),
        "required_root": ((str,), "/Game/_MCP_Temp"),
    }),
    "CubelessLandscapeTools.set_landscape_material": _entry(_landscape_action("set_landscape_material"), "write", {
        "material_path": _S, "actor_label": ((str,), ""),
    }),
    "CubelessFoliageInstanceTools.list_foliage_types": _entry(_foliage_instances_action("list_foliage_types"), "read", {}),
    "CubelessFoliageInstanceTools.add_foliage_instances": _entry(_foliage_instances_action("add_foliage_instances"), "write", {
        "foliage_type_path": _S, "transforms": ((list,), _REQUIRED),
    }),
    "CubelessFoliageInstanceTools.remove_foliage_instances": _entry(_foliage_instances_action("remove_foliage_instances"), "write", {
        "foliage_type_path": _S, "instance_indices": ((list, type(None)), None),
        "remove_all": ((bool,), False), "max_remove_count": ((int,), 5000),
    }),
    "CubelessSplineComponentPointsTools.set_spline_component_points": _entry(_spline_component_points_action, "write", {
        "points": ((list,), _REQUIRED),
        "actor_label": ((str,), ""),
        "actor_name": ((str,), ""),
        "actor_tag": ((str,), ""),
        "actor_label_prefix": ((str,), ""),
        "component_name": ((str,), "Road_SourceSpline"),
        "component_tag": ((str,), ""),
        "selected_only": ((bool,), False),
        "closed_loop": ((bool,), False),
        "native_first": ((bool,), True),
    }),
    "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function": _entry(_blueprint_function_extraction_action, "write", {
        "blueprint_name": ((str,), _REQUIRED),
        "graph_name": ((str,), _REQUIRED),
        "function_name": ((str,), _REQUIRED),
        "node_ids": ((list,), _REQUIRED),
        "graph_type": ((str,), "event"),
        "expected_node_count": ((int,), -1),
        "allow_non_temp": ((bool,), False),
        "dry_run": ((bool,), True),
        "compile": ((bool,), True),
        "save_asset": ((bool,), False),
    }),
    "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose": _entry(_anim_node_pre_post_runtime_pose_action, "runtime_write", {
        "blueprint_name": ((str,), ""),
        "anim_blueprint": ((str,), ""),
        "graph_name": ((str,), "AnimGraph"),
        "graph_id": ((str,), ""),
        "graph_type": ((str,), "function"),
        "node_id": ((str,), ""),
        "node_type": ((str,), ""),
        "title_contains": ((str,), ""),
        "sample_bones": ((list, type(None)), None),
        "sample_sockets": ((list, type(None)), None),
        "mode": ((str,), "active_component_tick_delta"),
        "skeletal_mesh": ((str,), ""),
        "temp_root": ((str,), "/Game/_MCP_Temp/AnimNodePrePost"),
        "cleanup": ((bool,), True),
        "overwrite_existing_temp_assets": ((bool,), True),
        "run_id": ((str,), ""),
        "actor_label": ((str,), ""),
        "actor_name": ((str,), ""),
        "actor_path": ((str,), ""),
        "component_name": ((str,), ""),
        "anim_instance_source": ((str,), "main"),
        "prefer_pie_world": ((bool,), True),
        "require_pie_world": ((bool,), False),
        "tick_count": ((int,), 1),
        "tick_delta_time": ((int, float), 1.0 / 30.0),
        "settle_tick_count": ((int,), 0),
        "refresh_bone_transforms": ((bool,), True),
        "allow_missing_bones": ((bool,), False),
        "pose_link_max_depth": ((int,), 4),
        "input_pose_mode": ((str,), "first"),
        "input_pose_field_path": ((str,), ""),
        "input_pose_index": ((int,), -1),
        "include_pins": ((bool,), True),
        "max_depth": ((int,), 3),
        "dry_run": ((bool,), True),
    }),
    "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver": _entry(_anim_bp_locomotion_driver_action, "write", {
        "source_anim_bp_path": _S,
        "target_anim_bp_path": ((str,), ""),
        "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "variant_name": ((str,), "LocomotionEventGraph"),
        "speed_variable": ((str,), "Speed"),
        "create_variable_if_missing": ((bool,), True),
        "include_direction": ((bool,), False),
        "direction_variable": ((str,), "Direction"),
        "create_direction_variable_if_missing": ((bool,), True),
        "include_is_in_air": ((bool,), False),
        "is_in_air_variable": ((str,), "IsInAir"),
        "create_is_in_air_variable_if_missing": ((bool,), True),
        "include_acceleration": ((bool,), False),
        "acceleration_variable": ((str,), "Acceleration"),
        "create_acceleration_variable_if_missing": ((bool,), True),
        "include_is_accelerating": ((bool,), False),
        "is_accelerating_variable": ((str,), "IsAccelerating"),
        "create_is_accelerating_variable_if_missing": ((bool,), True),
        "is_accelerating_threshold": ((int, float), 3.0),
        "guard_pawn_owner": ((bool,), True),
        "replace_existing_links": ((bool,), False),
        "allow_existing_target_reuse": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "compile": ((bool,), True),
        "save_asset": ((bool,), True),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
    }),
    "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant": _entry(_anim_state_machine_variant_action, "write", {
        "source_anim_bp_path": _S,
        "states": ((list, type(None)), None),
        "transitions": ((list, type(None)), None),
        "target_anim_bp_path": ((str,), ""),
        "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "variant_name": ((str,), "StateMachineSample"),
        "state_machine_name": ((str,), "MCP_StateMachine"),
        "entry_state": ((str,), ""),
        "animation_asset_path": ((str,), ""),
        "connect_demo_machine_to_output": ((bool,), True),
        "replace_existing_root_link": ((bool,), True),
        "replace_existing_entry_link": ((bool,), True),
        "allow_existing_target_reuse": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "compile": ((bool,), True),
        "save_asset": ((bool,), True),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
        "max_states": ((int,), 16),
        "max_transitions": ((int,), 32),
    }),
    "CubelessLinkedAnimGraphVariantTools.ensure_linked_anim_graph_sample_variant": _entry(_linked_anim_graph_variant_action, "write", {
        "source_anim_bp_path": _S,
        "linked_anim_bp_path": _S,
        "target_anim_bp_path": ((str,), ""),
        "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "variant_name": ((str,), "LinkedAnimGraphSample"),
        "insert_mode": ((str,), "input_pose_to_linked_graph_to_root"),
        "allow_existing_target_reuse": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "replace_existing_links": ((bool,), True),
        "require_input_pose_pin": ((bool,), False),
        "compile": ((bool,), True),
        "save_asset": ((bool,), False),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
    }),
    "CubelessBlendSpaceVariantTools.ensure_blendspace_sample_variant": _entry(_blendspace_variant_action, "write", {
        "source_blendspace": _S,
        "variant_name": ((str,), "Variant"),
        "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "target_blendspace": ((str,), ""),
        "axis_edits": ((list, type(None)), None),
        "sample_edits": ((list, type(None)), None),
        "add_samples": ((list, type(None)), None),
        "overwrite_existing": ((bool,), False),
        "save": ((bool,), True),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
        "allow_non_sample": ((bool,), False),
        "force_resample": ((bool,), False)}),
    "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid": _entry(_blendspace_pose_grid_action, "runtime_write", {
        "skeletal_mesh": _S,
        "blendspace_path": ((str,), ""),
        "blendspaces": ((list, type(None)), None),
        "samples": ((list, type(None)), None),
        "sample_bones": ((list, type(None)), None),
        "sample_sockets": ((list, type(None)), None),
        "actor_label": ((str,), "MCP_BlendSpace_RuntimePoseGrid"),
        "sample_time": ((int, float), 0.25),
        "settle_tick_count": ((int,), 2),
        "tick_count": ((int,), 1),
        "tick_delta_time": ((int, float), 0.03333333333333333),
        "include_axis_extremes": ((bool,), False),
        "allow_missing_bones": ((bool,), False),
        "refresh_bone_transforms": ((bool,), True),
        "cleanup": ((bool,), True),
        "prefer_pie_world": ((bool,), True),
        "require_pie_world": ((bool,), True)}),
    "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie": _entry(_skeletal_bone_sampling_action, "read", {
        "actor_label": ((str,), ""), "actor_name": ((str,), ""), "actor_path": ((str,), ""), "component_name": ((str,), ""),
        "bones": ((list, type(None)), None), "sockets": ((list, type(None)), None),
        "prefer_pie_world": ((bool,), True), "require_pie_world": ((bool,), False)}),
    "CubelessPostProcessAnimDemoVariantTools.ensure_postprocess_anim_demo_variant": _entry(_postprocess_anim_demo_variant_action, "write", {
        "source_blueprint_name": ((str,), _REQUIRED), "source_skeletal_mesh": ((str,), _REQUIRED),
        "variant_name": ((str,), "Variant"), "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "target_blueprint_name": ((str,), ""), "target_skeletal_mesh": ((str,), ""), "bone_name": ((str,), "head"),
        "rotation": ((str, int, float, bool, list, dict, type(None)), None), "graph_name": ((str,), "AnimGraph"),
        "graph_id": ((str,), ""), "graph_type": ((str,), "function"), "replace_existing": ((bool,), True),
        "overwrite_existing": ((bool,), False), "compile": ((bool,), True), "save": ((bool,), True),
        "dry_run": ((bool,), False), "allow_non_sample": ((bool,), False)}),
    "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response": _entry(_anim_state_machine_runtime_response_action, "runtime_write", {
        "actor_label": ((str,), ""), "actor_name": ((str,), ""), "actor_path": ((str,), ""), "component_name": ((str,), ""),
        "cases": ((list, type(None)), None), "properties": ((dict, type(None)), None), "name": ((str,), "case_0"),
        "tick_count": ((int,), 1), "tick_delta_time": ((int, float), 1.0 / 30.0), "refresh_bone_transforms": ((bool,), True),
        "restore_after_case": ((bool,), True), "include_baseline": ((bool,), True), "state_machine_name": ((str,), ""),
        "include_states": ((bool,), True), "include_state_weights": ((bool,), True), "include_relevant_anim_times": ((bool,), False),
        "include_transition_progress": ((bool,), True), "include_inactive_transition_progress": ((bool,), False),
        "include_montages": ((bool,), True), "include_curves": ((bool,), False), "curve_names": ((list, type(None)), None),
        "max_state_machines": ((int,), 32), "max_state_machine_instances_to_probe": ((int,), 256), "max_states_per_machine": ((int,), 64),
        "max_transitions_per_machine": ((int,), 128), "max_curves": ((int,), 128), "prefer_pie_world": ((bool,), True), "require_pie_world": ((bool,), False)}),
    "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe": _entry(_anim_instance_runtime_property_probe_action, "runtime_write", {
        "actor_label": ((str,), ""), "actor_name": ((str,), ""), "actor_path": ((str,), ""), "component_name": ((str,), ""),
        "properties": ((dict, type(None)), None), "property_name": ((str,), ""), "value": ((str, int, float, bool, list, dict, type(None)), None),
        "tick_after_set": ((bool,), False), "tick_count": ((int,), 1), "tick_delta_time": ((int, float), 1.0 / 30.0),
        "refresh_bone_transforms": ((bool,), True), "include_snapshot_after": ((bool,), True), "include_previous_values": ((bool,), True),
        "state_machine_name": ((str,), ""), "include_states": ((bool,), True), "include_state_weights": ((bool,), True),
        "include_relevant_anim_times": ((bool,), False), "include_transition_progress": ((bool,), True), "include_inactive_transition_progress": ((bool,), False),
        "include_montages": ((bool,), True), "include_curves": ((bool,), False), "curve_names": ((list, type(None)), None),
        "max_transitions_per_machine": ((int,), 128), "prefer_pie_world": ((bool,), True), "require_pie_world": ((bool,), False)}),
    "CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state": _entry(_anim_instance_runtime_state_action, "read_world", {
        "actor_label": ((str,), ""), "actor_name": ((str,), ""), "actor_path": ((str,), ""), "component_name": ((str,), ""), "state_machine_name": ((str,), ""),
        "include_states": ((bool,), True), "include_state_weights": ((bool,), True), "include_relevant_anim_times": ((bool,), False),
        "include_transition_progress": ((bool,), True), "include_inactive_transition_progress": ((bool,), False), "include_montages": ((bool,), True), "include_curves": ((bool,), False),
        "curve_names": ((list, type(None)), None), "max_state_machines": ((int,), 32), "max_state_machine_instances_to_probe": ((int,), 256),
        "max_states_per_machine": ((int,), 64), "max_transitions_per_machine": ((int,), 128), "max_curves": ((int,), 128),
        "prefer_pie_world": ((bool,), True), "require_pie_world": ((bool,), False)}),
    "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults": _entry(_rigvm_pin_default_writer_action, "write", {
        "control_rig_path": ((str,), ""), "control_rig_asset_path": ((str,), ""), "control_rig_class_path": ((str,), ""),
        "pin_defaults": ((list, type(None)), None), "defaults": ((list, type(None)), None),
        "resize_arrays": ((bool,), False), "set_value_on_linked_pins": ((bool,), False), "dry_run": ((bool,), False),
        "compile": ((bool,), True), "save_asset": ((bool,), False), "max_pins": ((int,), 64)}),
    "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links": _entry(_rigvm_link_writer_action, "write", {
        "control_rig_path": ((str,), ""), "control_rig_asset_path": ((str,), ""), "control_rig_class_path": ((str,), ""),
        "links": ((list, type(None)), None), "unit_links": ((list, type(None)), None),
        "allow_type_casting": ((bool,), False), "dry_run": ((bool,), False), "compile": ((bool,), True), "save_asset": ((bool,), False), "max_links": ((int,), 64)}),
    "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults": _entry(_rigvm_pin_default_validation_action, "read_load", {
        "control_rig_path": ((str,), ""), "control_rig_asset_path": ((str,), ""), "control_rig_class_path": ((str,), ""),
        "pin_defaults": ((list, type(None)), None), "defaults": ((list, type(None)), None), "pins": ((list, type(None)), None), "max_pins": ((int,), 64)}),
    "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links": _entry(_rigvm_link_validation_action, "read_load", {
        "control_rig_path": ((str,), ""), "control_rig_asset_path": ((str,), ""), "control_rig_class_path": ((str,), ""),
        "links": ((list, type(None)), None), "unit_links": ((list, type(None)), None),
        "allow_type_casting": ((bool,), True), "max_links": ((int,), 64)}),
    "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology": _entry(_rigvm_topology_action, "read_load", {
        "control_rig_path": ((str,), ""), "control_rig_asset_path": ((str,), ""), "control_rig_class_path": ((str,), ""),
        "include_pins": ((bool,), True), "include_sub_pins": ((bool,), True), "include_links": ((bool,), True), "include_pin_links": ((bool,), False),
        "max_nodes": ((int,), 256), "max_pins_per_node": ((int,), 64), "max_sub_pins_per_pin": ((int,), 32), "max_links": ((int,), 512)}),
    "CubelessEnumTools.create_user_defined_enum": _entry(_user_defined_enum_action, "write", {
        "asset_path": _S, "display_names": ((list,), _REQUIRED), "tooltips": ((list, type(None)), None),
        "dry_run": ((bool,), True), "save": ((bool,), False), "skip_if_exists": ((bool,), False)}),
    "CubelessEditorTraceTools.line_trace_physical_material": _entry(_physical_material_trace_action, "read_world", {
        "start": ((list,), _REQUIRED), "end": ((list,), _REQUIRED), "trace_channel": ((str,), "visibility"),
        "trace_complex": ((bool,), False), "return_physical_material": ((bool,), True)}),
    "CubelessProjectTools.replace_asset_references_mcp": _entry(_asset_reference_action, "write", {
        "replacements": ((dict, type(None)), None), "target_root": ((str,), ""), "target_paths": ((list, type(None)), None),
        "source_root": ((str,), ""), "string_replacements": ((dict, type(None)), None), "name_replacements": ((dict, type(None)), None),
        "dry_run": ((bool,), True), "save_assets": ((bool,), False), "scan_paths": ((bool,), True), "refresh_materials": ((bool,), True),
        "include_package_objects": ((bool,), False), "allow_non_game_targets": ((bool,), False)}),
    "CubelessProjectTools.audit_content_root_mcp": _entry(_content_audit_action, "read_load_report", {
        "root_path": _S, "recursive": ((bool,), True), "scan_paths": ((bool,), True),
        "include_dependencies": ((bool,), True), "include_assets": ((bool,), False), "write_report": ((bool,), False),
        "forbidden_dependency_prefixes": ((list, type(None)), None), "required_asset_paths": ((list, type(None)), None),
        "expected_asset_count": ((int, type(None)), None), "expected_class_counts": ((dict, type(None)), None),
        "max_samples": ((int,), 50), "fail_on_dirty_packages": ((bool,), False), "fail_on_redirectors": ((bool,), True),
        "fail_on_forbidden_dependencies": ((bool,), True)}),
    "CubelessProjectTools.create_input_mapping": _entry(_input_mapping_action, "write", {
        "action_name": _S, "key": _S, "input_type": ((str,), "Action"),
        "shift": ((bool,), False), "ctrl": ((bool,), False), "alt": ((bool,), False), "cmd": ((bool,), False)}),
    "CubelessStateTreeTools.add_state_tree_state": _entry(_state_tree_mutators_action("add_state_tree_state"), "write", {
        "asset_path": _S, "state_name": _S, "parent_path": ((str,), "Root"),
        "state_type": ((str,), "state"), "selection_behavior": ((str,), "children_in_order"), "dry_run": ((bool,), True)}),
    "CubelessStateTreeTools.compile_state_tree": _entry(_state_tree_mutators_action("compile_state_tree"), "write", {
        "asset_path": _S, "save": ((bool,), True)}),
    "CubelessStateTreeTools.create_state_tree": _entry(_state_tree_create_action, "write", {
        "name": _S, "package_path": ((str,), "/Game/AI/StateTrees"), "schema_type": ((str,), "component")}),
    "CubelessStateTreeTools.list_state_trees": _entry(_state_tree_read_action("list_state_trees"), "read", {
        "directory": ((str,), "/Game"), "recursive": ((bool,), True),
        "max_results": ((int,), 200), "offset": ((int,), 0)}),
    "CubelessStateTreeTools.inspect_state_tree": _entry(_state_tree_read_action("inspect_state_tree"), "read_load", {
        "asset_path": _S, "max_states": ((int,), 500)}),
    "CubelessAnimStateMachineInspectionTools.inspect_anim_state_machine_transitions": _entry(_anim_state_machine_inspection_action, "read_load", {
        "blueprint_name": _S, "state_machine_name": ((str,), ""), "include_pins": ((bool,), True),
        "include_rule_graph_nodes": ((bool,), True), "include_state_nodes": ((bool,), True),
        "max_rule_graph_nodes": ((int,), 256)}),
    "CubelessAnimationAssetMetadataTools.inspect_animation_asset_metadata_guarded": _entry(_animation_asset_metadata_action, "read_load", {
        "asset_path": _S,
        "include_notifies": ((bool,), True),
        "include_notify_details": ((bool,), True),
        "include_curves": ((bool,), True),
        "include_curve_keys": ((bool,), False),
        "include_sync_markers": ((bool,), True),
        "include_montage_sections": ((bool,), True),
        "include_montage_slots": ((bool,), True),
        "include_metadata": ((bool,), False),
        "max_notifies": ((int,), 256),
        "max_curves": ((int,), 256),
        "max_curve_keys_per_curve": ((int,), 0),
        "max_markers": ((int,), 256),
        "max_sections": ((int,), 128),
        "max_slots": ((int,), 64),
        "max_segments_per_slot": ((int,), 256),
        "require_supported_class": ((bool,), False)}),
    "CubelessPhysicsAssetInspectionTools.inspect_physics_asset_constraints_guarded": _entry(_physics_asset_inspection_action, "read_load", {
        "physics_asset_path": ((str,), ''),
        "skeletal_mesh_path": ((str,), ''),
        "include_bodies": ((bool,), True),
        "include_constraints": ((bool,), True),
        "include_profiles": ((bool,), True),
        "include_solver_settings": ((bool,), True),
        "include_collision_summary": ((bool,), True),
        "max_bodies": ((int,), 128),
        "max_constraints": ((int,), 128),
        "max_profiles_per_item": ((int,), 16),
        "max_shapes_per_body": ((int,), 32),
        "require_supported_class": ((bool,), False)}),
    "CubelessAnimGraphAuthoringTools.create_anim_blueprint_sample": _entry(_anim_blueprint_sample_action, "write", {
        "target_anim_bp_path": _S,
        "skeleton_path": ((str,), ""),
        "skeletal_mesh_path": ((str,), ""),
        "preview_skeletal_mesh_path": ((str,), ""),
        "parent_class_path": ((str,), "/Script/Engine.AnimInstance"),
        "overwrite_existing": ((bool,), False),
        "compile": ((bool,), True),
        "save_asset": ((bool,), False),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_controlrig_node_sample": _entry(_controlrig_builders_action("ensure_anim_graph_controlrig_node_sample"), "write", {
        "source_anim_bp_path": _S,
        "control_rig_asset_path": ((str,), ""),
        "control_rig_class_path": ((str,), ""),
        "target_anim_bp_path": ((str,), ""),
        "target_root": ((str,), "/Game/_MCP_Sample/AnimStudy"),
        "variant_name": ((str,), "ControlRigNodeSample"),
        "allow_existing_target_reuse": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "replace_existing_links": ((bool,), True),
        "compile": ((bool,), True),
        "save_asset": ((bool,), False),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_controlrig_sample_asset": _entry(_controlrig_builders_action("ensure_controlrig_sample_asset"), "write", {
        "target_control_rig_path": _S,
        "skeletal_mesh_path": ((str,), ""),
        "skeleton_path": ((str,), ""),
        "preview_skeletal_mesh_path": ((str,), ""),
        "setup_mode": ((str,), "empty_with_preview"),
        "curves": ((list, type(None)), None),
        "nulls": ((list, type(None)), None),
        "controls": ((list, type(None)), None),
        "unit_nodes": ((list, type(None)), None),
        "max_direct_curves": ((int,), 32),
        "max_direct_nulls": ((int,), 32),
        "max_direct_controls": ((int,), 32),
        "max_unit_nodes": ((int,), 6),
        "allow_existing_target_reuse": ((bool,), True),
        "overwrite_existing": ((bool,), False),
        "modular_rig": ((bool,), False),
        "compile": ((bool,), True),
        "save_asset": ((bool,), False),
        "dry_run": ((bool,), False),
        "validate_only": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.set_anim_graph_controlrig_input_defaults": _entry(_anim_graph_controlrig_action("set_anim_graph_controlrig_input_defaults"), "write", {
        "blueprint_name": _S, "input_defaults": ((dict, type(None)), None), "expose_input_pins": ((list, type(None)), None),
        "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""), "graph_type": ((str,), "function"),
        "node_id": ((str,), ""), "node_name": ((str,), ""), "title_contains": ((str,), ""), "control_rig_class": ((str,), ""),
        "disconnect_existing_links": ((bool,), False), "allow_non_sample": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_controlrig_forced_driver_animbp": _entry(_anim_graph_controlrig_action("ensure_controlrig_forced_driver_animbp"), "write", {
        "blueprint_name": _S, "curve_values": ((dict, type(None)), None), "input_defaults": ((dict, type(None)), None),
        "alpha": ((int, float), 1.0), "apply_mode": ((str,), "Add"),
        "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""), "graph_type": ((str,), "function"),
        "node_id": ((str,), ""), "node_name": ((str,), ""), "title_contains": ((str,), ""), "control_rig_class": ((str,), ""),
        "replace_existing": ((bool,), True), "disconnect_existing_links": ((bool,), True), "allow_non_sample": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_secondary_motion_chain": _entry(_anim_graph_secondary_motion_action, "write", {
        "blueprint_name": _S, "dynamics_type": _S, "root_bone": _S, "chain_end_bone": _S,
        "alpha": ((int, float), 1.0), "gravity": ((list, type(None)), None),
        "angular_limit_degrees": ((int, float), 60.0), "body_extents": ((list, type(None)), None),
        "linear_damping": ((int, float), 0.75), "angular_damping": ((int, float), 0.75),
        "kawaii_damping": ((int, float), 0.08), "kawaii_stiffness": ((int, float), 0.03),
        "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""), "graph_type": ((str,), "function"),
        "replace_existing": ((bool,), True), "allow_non_sample": ((bool,), False),
        "compile": ((bool,), True), "save": ((bool,), True),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_trail_demo": _entry(_anim_graph_trail_demo_action, "write", {
        "blueprint_name": _S, "trail_bone": ((str,), "VB VBHead"), "base_joint": ((str,), "head"),
        "chain_length": ((int,), 2), "chain_bone_axis": ((str,), "X"), "alpha": ((int, float), 1.0),
        "fake_velocity": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""), "graph_type": ((str,), "function"),
        "replace_existing": ((bool,), False), "allow_non_sample": ((bool,), False), "invert_chain_bone_axis": ((bool,), False),
        "reorient_parent_to_child": ((bool,), True), "actor_space_fake_velocity": ((bool,), False),
        "relaxation_speed_scale": ((int, float), 1.0), "limit_stretch": ((bool,), False),
        "stretch_limit": ((int, float), 0.0), "max_delta_time": ((int, float), 0.0),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_curve_demo": _entry(_anim_graph_modify_curve_demo_action, "write", {
        "blueprint_name": _S, "curve_values": ((dict, type(None)), None), "alpha": ((int, float), 1.0),
        "apply_mode": ((str,), "Add"), "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""),
        "graph_type": ((str,), "function"), "replace_existing": ((bool,), False), "allow_non_sample": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_bone_demo": _entry(_anim_graph_bone_demo_action, "write", {
        "blueprint_name": _S, "bone_name": ((str,), "head"), "rotation": ((str, int, float, bool, list, dict, type(None)), None),
        "graph_name": ((str,), "AnimGraph"), "graph_id": ((str,), ""), "graph_type": ((str,), "function"),
        "replace_existing": ((bool,), False), "allow_non_sample": ((bool,), False),
    }),
    "CubelessAnimGraphAuthoringTools.set_anim_graph_rigidbody_settings": _entry(_anim_graph_authoring_action('set_anim_graph_rigidbody_settings'), "write", {
        "blueprint_name": _S,
        "node_id": ((str,), ''),
        "alpha": ((str, int, float, bool, list, dict, type(None)), None),
        "external_force": ((str, int, float, bool, list, dict, type(None)), None),
        "simulation_space": ((str,), ''),
        "enable_world_geometry": ((str, int, float, bool, list, dict, type(None)), None),
        "allow_non_sample": ((bool,), False),
        "graph_name": ((str,), 'AnimGraph'),
        "graph_id": ((str,), ''),
        "graph_type": ((str,), 'function')}),
    "CubelessAnimGraphAuthoringTools.ensure_anim_graph_input_pose_passthrough": _entry(_anim_graph_authoring_action('ensure_anim_graph_input_pose_passthrough'), "write", {
        "blueprint_name": _S,
        "graph_name": ((str,), 'AnimGraph'),
        "graph_id": ((str,), ''),
        "graph_type": ((str,), 'function'),
        "replace_existing": ((bool,), False),
        "input_node_position": ((str, int, float, bool, list, dict, type(None)), None),
        "allow_non_sample": ((bool,), False)}),
}


def _unique_keys(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise OperationError("batch_duplicate_key", "Duplicate JSON key: " + key)
        result[key] = value
    return result


def _bad_constant(value):
    raise OperationError("batch_invalid_json", "Non-finite JSON constant: " + value)


def _rigvm_topology_legacy_paths(params, error_code="rigvm_topology_legacy_scalar_path_unsupported"):
    # Original Shared.h3453..3471 selects by raw FString.IsEmpty, then trims.
    # Whitespace masks later asset aliases and may select the class branch.
    # Keep formal invalid types for the normal schema rejection, never coerce them.
    values = dict(params)
    for alias in ("control_rig_asset", "control_rig", "control_rig_class"):
        if type(values.get(alias)) in (bool, int, float):
            raise OperationError(error_code,
                "Legacy numeric/boolean path coercion is not supported; use explicit string paths. No action executed.")
    raw_path = values.get("control_rig_path", "")
    raw_asset = values.get("control_rig_asset_path", "")
    if type(raw_path) is str and type(raw_asset) is str and raw_path == "" and raw_asset == "":
        for alias in ("control_rig_asset", "control_rig"):
            candidate = values.get(alias)
            if type(candidate) is str and candidate != "":
                values["control_rig_asset_path"] = candidate
                break
    raw_class = values.get("control_rig_class_path", "")
    if type(raw_class) is str and raw_class == "":
        candidate = values.get("control_rig_class")
        if type(candidate) is str and candidate != "":
            values["control_rig_class_path"] = candidate
    for alias in ("control_rig_asset", "control_rig", "control_rig_class"):
        values.pop(alias, None)
    return values


def _rigvm_link_validation_legacy_params(params):
    values = _rigvm_topology_legacy_paths(params, "rigvm_link_validation_legacy_scalar_path_unsupported")
    # Original key presence, including null, masks the hidden enable_type_casting alias.
    # Preserve valid hidden booleans; do not approximate UE FString/number/invalid AsBool coercion.
    if "allow_type_casting" not in values and "enable_type_casting" in values:
        alias = values["enable_type_casting"]
        if type(alias) is not bool:
            raise OperationError("rigvm_link_validation_legacy_bool_coercion_unsupported",
                "Legacy nonboolean enable_type_casting coercion is unsupported; use an explicit boolean. No action executed.")
        values["allow_type_casting"] = alias
    values.pop("enable_type_casting", None)
    return values


def _rigvm_link_writer_legacy_params(params):
    values = _rigvm_topology_legacy_paths(params, "rigvm_link_writer_legacy_scalar_path_unsupported")
    for formal, alias in (("allow_type_casting", "enable_type_casting"), ("save_asset", "save")):
        if formal not in values and alias in values:
            if type(values[alias]) is not bool:
                raise OperationError("rigvm_link_writer_legacy_bool_coercion_unsupported",
                    "Legacy nonboolean alias coercion is unsupported; use exact booleans. No action executed.")
            values[formal] = values[alias]
        values.pop(alias, None)
    # Original dry_run OR validate_only short-circuits a true formal dry_run.
    # Invalid formal types remain for normal schema rejection; an alias cannot mask them.
    if "validate_only" in values:
        dry = values.get("dry_run", False)
        if type(dry) is bool and not dry:
            if type(values["validate_only"]) is not bool:
                raise OperationError("rigvm_link_writer_legacy_bool_coercion_unsupported",
                    "Legacy nonboolean validate_only coercion is unsupported; use an exact boolean. No action executed.")
            values["dry_run"] = values["validate_only"]
        values.pop("validate_only", None)
    return values


def _rigvm_pin_default_writer_legacy_params(params):
    values = _rigvm_topology_legacy_paths(params, "rigvm_pin_default_writer_legacy_scalar_path_unsupported")
    for formal, alias in (("save_asset", "save"),):
        if formal not in values and alias in values:
            if type(values[alias]) is not bool:
                raise OperationError("rigvm_pin_default_writer_legacy_bool_coercion_unsupported",
                    "Legacy nonboolean alias coercion is unsupported; use exact booleans. No action executed.")
            values[formal] = values[alias]
        values.pop(alias, None)
    # Original dry_run OR validate_only short-circuits a true formal dry_run.
    # Invalid formal types remain for normal schema rejection; an alias cannot mask them.
    if "validate_only" in values:
        dry = values.get("dry_run", False)
        if type(dry) is bool and not dry:
            if type(values["validate_only"]) is not bool:
                raise OperationError("rigvm_pin_default_writer_legacy_bool_coercion_unsupported",
                    "Legacy nonboolean validate_only coercion is unsupported; use an exact boolean. No action executed.")
            values["dry_run"] = values["validate_only"]
        values.pop("validate_only", None)
    # Shared UE56 parser tries hidden pins only when neither public array is selected.
    if type(values.get("pin_defaults")) is not list and type(values.get("defaults")) is not list and "pins" in values:
        values["defaults"] = values["pins"]
    values.pop("pins", None)
    return values


def _controlrig_transient_probe_legacy_params(params):
    # Preserve the original resolver's raw-empty and presence order for string aliases.
    aliases = ("control_rig_class_path", "control_rig_asset", "control_rig_asset_path", "control_rig")
    values = dict(params)
    for field in ("control_rig_path", "control_rig_class") + aliases:
        if field in values and type(values[field]) is not str:
            raise OperationError("batch_controlrig_transient_probe_string_alias_required", field + " requires a string; hidden numeric/bool path coercion is unsupported")
    rig_class = values.get("control_rig_class", "")
    if not rig_class:
        rig_class = values.get("control_rig_class_path", "")
    rig_path = values["control_rig_path"] if "control_rig_path" in values else values.get("control_rig_asset", "")
    if not rig_path:
        rig_path = values.get("control_rig_asset_path", "")
    if not rig_path:
        rig_path = values.get("control_rig", "")
    for field in aliases:
        values.pop(field, None)
    values["control_rig_path"] = rig_path
    values["control_rig_class"] = rig_class
    return values


def _normalize(commands_json, stop_on_error):
    if type(stop_on_error) is not bool:
        raise OperationError("batch_invalid_stop_on_error", "stop_on_error must be a boolean")
    if type(commands_json) is not str:
        raise OperationError("batch_invalid_json", "commands_json must be a JSON string")
    if len(commands_json.encode("utf-8")) > MAX_INPUT_BYTES:
        raise OperationError("batch_input_too_large", "commands_json exceeds 1 MiB")
    try:
        commands = json.loads(commands_json, object_pairs_hook=_unique_keys, parse_constant=_bad_constant)
    except (ValueError, RecursionError) as error:
        raise OperationError("batch_invalid_json", str(error)) from error
    if type(commands) is not list or not commands:
        raise OperationError("batch_invalid_commands", "commands must be a nonempty JSON array")
    if len(commands) > MAX_COMMANDS:
        raise OperationError("batch_too_large", "commands must contain at most 100 entries")
    normalized = []
    for index, command in enumerate(commands):
        if type(command) is not dict or set(command) - {"type", "params"}:
            raise OperationError("batch_invalid_entry", f"commands[{index}] must contain only type and optional params")
        name = command.get("type")
        if name in ("batch", "execute_batch", "CubelessBatchTools.execute_batch"):
            raise OperationError("batch_nested", f"commands[{index}]: nested batches are not supported")
        if type(name) is not str or not name:
            raise OperationError("batch_invalid_entry", f"commands[{index}].type must be a nonempty string")
        actor_original_command = name if name in ("delete_actor", "set_actor_transform", "spawn_blueprint_actor") else None
        if actor_original_command is not None:
            name = "CubelessActorTools." + ("set_actor_transform_original" if actor_original_command == "set_actor_transform" else actor_original_command)
        if name == "set_actor_transform_original":
            name = "CubelessActorTools.set_actor_transform_original"
        if name in ("execute_unreal_python", "CubelessMCPUE58Editor.CubelessPythonExecutionTools.execute_unreal_python"):
            name = "CubelessPythonExecutionTools.execute_unreal_python"
        if name == "create_input_mapping":
            name = "CubelessProjectTools.create_input_mapping"
        if name in ("controlrig_direct_gate_probe", "sample_controlrig_pre_post_runtime_pose"):
            name = "CubelessControlRigTransientProbeTools." + name
        if name == "validate_controlrig_solver_intents":
            name = "CubelessControlRigSolverIntentTools.validate_controlrig_solver_intents"
        chaos_solver_settings_raw_method = name if name in ("inspect_chaos_solver_settings", "configure_chaos_solver_settings") else None
        if chaos_solver_settings_raw_method is not None:
            name = "CubelessChaosSolverSettingsTools." + chaos_solver_settings_raw_method
        chaos_cloth_component_raw_method = name if name in ("inspect_chaos_cloth_component", "configure_chaos_cloth_component") else None
        if chaos_cloth_component_raw_method is not None:
            name = "CubelessChaosClothComponentTools." + chaos_cloth_component_raw_method
        chaos_cloth_authoring_raw_method = name if name in ("configure_chaos_cloth_dataflow_node", "configure_chaos_cloth_weight_map") else None
        if chaos_cloth_authoring_raw_method is not None:
            name = "CubelessChaosClothAuthoringTools." + chaos_cloth_authoring_raw_method
        legacy_chaos_clothing_raw_method = name if name in ("create_legacy_chaos_clothing_from_skeletal_sections",) else None
        if legacy_chaos_clothing_raw_method is not None:
            name = "CubelessLegacyChaosClothingTools." + legacy_chaos_clothing_raw_method
        chaos_cloth_inspection_raw_method = name if name in ("inspect_chaos_cloth_asset", "inspect_chaos_cloth_simulation_mesh") else None
        if chaos_cloth_inspection_raw_method is not None:
            name = "CubelessChaosClothInspectionTools." + chaos_cloth_inspection_raw_method
        actor_property_focus_raw_method = name if name in ("set_actor_property", "focus_viewport") else None
        if actor_property_focus_raw_method is not None:
            name = "CubelessActorPropertyFocusTools." + actor_property_focus_raw_method
        editor_level_session_raw_method = name if name in ("open_editor_level","get_editor_level_open_status","clear_dirty_package_flags","safe_new_preview_map") else None
        if editor_level_session_raw_method is not None:
            name = "CubelessEditorLevelSessionTools." + editor_level_session_raw_method
        viewport_capture_raw_method = name if name in ("list_viewport_bookmarks", "set_active_viewport_realtime", "capture_viewport_bookmark_screenshot", "take_screenshot") else None
        if viewport_capture_raw_method is not None:
            name = "CubelessViewportCaptureTools." + viewport_capture_raw_method
        chaos_cinematic_sequence_raw_method = name if name in ("create_chaos_level_sequence", "bind_chaos_cache_to_sequence", "configure_chaos_sequence_timing", "inspect_chaos_cinematic_pipeline") else None
        if chaos_cinematic_sequence_raw_method is not None:
            name = "CubelessChaosCinematicSequenceTools." + chaos_cinematic_sequence_raw_method
        chaos_cache_sessions_raw_method = name if name in ("record_chaos_cache", "validate_chaos_cache") else None
        if chaos_cache_sessions_raw_method is not None:
            name = "CubelessChaosCacheSessionTools." + chaos_cache_sessions_raw_method
        chaos_destruction_performance_raw_method = name if name in ("run_chaos_destruction_performance_smoke",) else None
        if chaos_destruction_performance_raw_method is not None:
            name = "CubelessChaosDestructionPerformanceTools." + chaos_destruction_performance_raw_method
        chaos_learning_raw_method = name if name in ("create_chaos_destruction_learning_fixture", "run_chaos_cache_smoke") else None
        if chaos_learning_raw_method is not None:
            name = "CubelessChaosLearningTools." + chaos_learning_raw_method
        chaos_trace_strain_raw_method = name if name in ("trigger_trace_external_strain",) else None
        if chaos_trace_strain_raw_method is not None:
            name = "CubelessChaosTraceStrainTools." + chaos_trace_strain_raw_method
        chaos_geometry_authoring_raw_method = name if name in ("create_geometry_collection_from_mesh", "configure_building_fracture", "configure_anchor_field") else None
        if chaos_geometry_authoring_raw_method is not None:
            name = "CubelessChaosGeometryAuthoringTools." + chaos_geometry_authoring_raw_method
        gas_authoring_raw_method = {'gas_inspect_blueprint': 'inspect_gas_blueprint', 'gas_create_gameplay_ability_blueprint': 'create_gameplay_ability_blueprint', 'gas_create_gameplay_effect_blueprint': 'create_gameplay_effect_blueprint', 'gas_create_attribute_set_blueprint': 'create_attribute_set_blueprint', 'gas_add_ability_system_component_to_blueprint': 'add_ability_system_component_to_blueprint', 'gas_add_gameplay_attribute_to_attribute_set': 'add_gameplay_attribute_to_attribute_set'}.get(name)
        if gas_authoring_raw_method is not None:
            name = "CubelessGASAuthoringTools." + gas_authoring_raw_method
        content_reconstruction_raw_method = {'recreate_content_folder_mcp': 'recreate_content_folder_mcp', 'postprocess_content_folder_mcp': 'postprocess_content_folder_mcp', 'repair_world_actor_instances_mcp': 'repair_world_actor_instances_mcp', 'run_content_validation_pipeline_mcp': 'run_content_validation_pipeline_mcp'}.get(name)
        if content_reconstruction_raw_method is not None:
            name = "CubelessContentReconstructionTools." + content_reconstruction_raw_method
        niagara_renderers_raw_method = {'inspect_niagara_renderers': 'inspect_niagara_renderers', 'set_niagara_renderer_material': 'set_niagara_renderer_material', 'set_niagara_sprite_renderer_settings': 'set_niagara_sprite_renderer_settings', 'add_niagara_sprite_renderer': 'add_niagara_sprite_renderer', 'add_niagara_mesh_renderer': 'add_niagara_mesh_renderer', 'set_niagara_renderer_enabled': 'set_niagara_renderer_enabled', 'cleanup_niagara_renderers': 'cleanup_niagara_renderers'}.get(name)
        if niagara_renderers_raw_method is not None:
            name = "CubelessNiagaraRendererTools." + niagara_renderers_raw_method
        niagara_emitter_configuration_raw_method = {'inspect_niagara_emitter_properties': 'inspect_niagara_emitter_properties', 'set_niagara_emitter_enabled': 'set_niagara_emitter_enabled', 'set_niagara_emitter_local_space': 'set_niagara_emitter_local_space', 'set_niagara_emitter_sim_target': 'set_niagara_emitter_sim_target', 'set_niagara_emitter_fixed_bounds': 'set_niagara_emitter_fixed_bounds', 'set_niagara_emitter_persistent_ids': 'set_niagara_emitter_persistent_ids', 'remove_niagara_emitter': 'remove_niagara_emitter'}.get(name)
        if niagara_emitter_configuration_raw_method is not None:
            name = "CubelessNiagaraEmitterConfigurationTools." + niagara_emitter_configuration_raw_method
        niagara_parameter_collections_raw_method = {'inspect_niagara_parameter_collection_values': 'inspect_niagara_parameter_collection_values', 'set_niagara_parameter_collection_values': 'set_niagara_parameter_collection_values', 'repair_niagara_parameter_collection': 'repair_niagara_parameter_collection', 'remap_niagara_parameter_collection_references': 'remap_niagara_parameter_collection_references', 'remap_niagara_script_parameter_collection_references': 'remap_niagara_script_parameter_collection_references', 'recompile_niagara_scripts_in_root': 'recompile_niagara_scripts_in_root'}.get(name)
        if niagara_parameter_collections_raw_method is not None:
            name = "CubelessNiagaraParameterCollectionTools." + niagara_parameter_collections_raw_method
        niagara_authoring_inspection_raw_method = {'inspect_niagara_stack': 'inspect_niagara_stack', 'inspect_niagara_graph': 'inspect_niagara_graph', 'inspect_niagara_graph_actions': 'inspect_niagara_graph_actions', 'inspect_niagara_custom_hlsl_nodes': 'inspect_niagara_custom_hlsl_nodes', 'inspect_niagara_scratch_pad_interface': 'inspect_niagara_scratch_pad_interface', 'inspect_niagara_compile_status': 'inspect_niagara_compile_status'}.get(name)
        if niagara_authoring_inspection_raw_method is not None:
            name = "CubelessNiagaraAuthoringInspectionTools." + niagara_authoring_inspection_raw_method
        pcg_inspection_raw_method = {'resolve_pcg_graph': 'resolve_pcg_graph', 'list_pcg_graph_nodes': 'list_pcg_graph_nodes', 'inspect_pcg_custom_hlsl_nodes': 'inspect_pcg_custom_hlsl_nodes', 'inspect_pcg_component_outputs': 'inspect_pcg_component_outputs'}.get(name)
        if pcg_inspection_raw_method is not None:
            name = "CubelessPCGInspectionTools." + pcg_inspection_raw_method

        niagara_factory_bootstrap_raw_method = {'inspect_niagara_factory_bootstrap_defaults': 'inspect_niagara_factory_bootstrap_defaults', 'create_empty_niagara_system': 'create_empty_niagara_system', 'create_empty_niagara_emitter': 'create_empty_niagara_emitter', 'create_basic_sprite_niagara_system': 'create_basic_sprite_niagara_system'}.get(name)
        if niagara_factory_bootstrap_raw_method is not None:
            name = "CubelessNiagaraFactoryBootstrapTools." + niagara_factory_bootstrap_raw_method

        niagara_resource_bindings_raw_method = {'set_niagara_render_target2d_module_input': 'set_niagara_render_target2d_module_input', 'set_niagara_texture2d_module_input': 'set_niagara_texture2d_module_input', 'set_niagara_scratch_pad_function_input_default': 'set_niagara_scratch_pad_function_input_default', 'link_niagara_scratch_pad_pin_to_user_parameter': 'link_niagara_scratch_pad_pin_to_user_parameter'}.get(name)
        niagara_scratch_stack_authoring_raw_method = {'add_niagara_scratch_pad_module_input': 'add_niagara_scratch_pad_module_input', 'add_niagara_scratch_pad_assignment_module_to_stack': 'add_niagara_scratch_pad_assignment_module_to_stack', 'wrap_niagara_scratch_pad_output_with_stack_context': 'wrap_niagara_scratch_pad_output_with_stack_context'}.get(name)
        niagara_event_stage_authoring_raw_method = {'inspect_niagara_simulation_stages': 'inspect_niagara_simulation_stages', 'inspect_niagara_event_handlers': 'inspect_niagara_event_handlers', 'add_niagara_event_handler': 'add_niagara_event_handler'}.get(name)
        niagara_promotion_raw_method = {'audit_niagara_promotion_candidate': 'audit_niagara_promotion_candidate', 'promote_niagara_system_candidate': 'promote_niagara_system_candidate'}.get(name)
        niagara_sprite_preset_authoring_raw_method = {'create_sprite_niagara_preset_system': 'create_sprite_niagara_preset_system'}.get(name)
        niagara_custom_hlsl_conversion_raw_method = {'replace_niagara_custom_hlsl': 'replace_niagara_custom_hlsl', 'apply_niagara_scratch_pad_graph_recipe': 'apply_niagara_scratch_pad_graph_recipe'}.get(name)
        niagara_preview_lab_raw_method = {'get_niagara_preview_lab_state': 'get_niagara_preview_lab_state', 'cleanup_niagara_preview_lab': 'cleanup_niagara_preview_lab', 'capture_niagara_preview_lab_view': 'capture_niagara_preview_lab_view', 'preview_niagara_system_in_preview_lab': 'preview_niagara_system_in_preview_lab'}.get(name)
        controlrig_recipe_authoring_raw_method = {'validate_controlrig_recipe': 'validate_controlrig_recipe', 'dry_run_controlrig_recipe': 'dry_run_controlrig_recipe', 'build_controlrig_recipe': 'build_controlrig_recipe', 'build_controlrig_recipe_with_animbp_binding': 'build_controlrig_recipe_with_animbp_binding'}.get(name)
        texture_material_authoring_raw_method = {'get_static_mesh_uv_layout': 'get_static_mesh_uv_layout', 'create_material_instance_with_texture': 'create_material_instance_with_texture', 'create_material_with_texture_set': 'create_material_with_texture_set', 'import_create_and_apply_texture_set': 'import_create_and_apply_texture_set', 'generate_pbr_companion_textures': 'generate_pbr_companion_textures', 'import_texture_to_unreal':'import_texture_to_unreal', 'import_texture_set_to_unreal':'import_texture_set_to_unreal', 'apply_material_to_mesh':'apply_material_to_mesh','generate_texture_from_prompt':'generate_texture_from_prompt','generate_texture_for_mesh_uv':'generate_texture_for_mesh_uv','generate_and_apply_ai_texture':'generate_and_apply_ai_texture'}.get(name)
        blueprint_resolution_connection_raw_method = {'resolve_blueprint': 'resolve_blueprint', 'connect_blueprint_nodes': 'connect_blueprint_nodes'}.get(name)
        niagara_scratch_user_recipes_raw_method = {'create_niagara_visible_scratch_pad_snow_seed': 'create_niagara_visible_scratch_pad_snow_seed', 'create_niagara_scratch_pad_vec2_user_parameter_module': 'create_niagara_scratch_pad_vec2_user_parameter_module', 'create_niagara_scratch_pad_color_user_parameter_module': 'create_niagara_scratch_pad_color_user_parameter_module', 'create_niagara_scratch_pad_bool_user_parameter_module': 'create_niagara_scratch_pad_bool_user_parameter_module', 'create_niagara_scratch_pad_bool_visible_toggle_module': 'create_niagara_scratch_pad_bool_visible_toggle_module'}.get(name)
        blueprint_property_graph_raw_method = {'set_blueprint_property': 'set_blueprint_property', 'list_blueprint_graphs': 'list_blueprint_graphs'}.get(name)
        controlrig_recipe_planning_raw_method = {'profile_controlrig_skeleton': 'profile_controlrig_skeleton', 'compile_controlrig_profile_recipe': 'compile_controlrig_profile_recipe', 'make_controlrig_preset_recipe': 'make_controlrig_preset_recipe'}.get(name)
        pcg_utilities_raw_method = {'list_pcg_assets': 'list_pcg_assets', 'list_pcg_components': 'list_pcg_components', 'set_pcg_debug_enabled': 'set_pcg_debug_enabled', 'resave_pcg_assets': 'resave_pcg_assets'}.get(name)
        if name in ("list_blueprint_nodes", "delete_blueprint_node"):
            name = "CubelessBlueprintGraphTools." + name
        if name == "pcg_actor_smoke_test":
            name = "CubelessPCGTools.pcg_actor_smoke_test"
        pcg_promotion_raw_method = {'validate_pcg_source_independence':'validate_pcg_source_independence','promote_pcg_temp_graph':'promote_pcg_temp_graph'}.get(name)
        pcg_contract_authoring_raw_method = {'audit_pcg_graph_contract': 'audit_pcg_graph_contract', 'read_pcg_node_contract': 'read_pcg_node_contract', 'set_pcg_static_mesh_spawner_entries': 'set_pcg_static_mesh_spawner_entries'}.get(name)
        pcg_component_generation_raw_method = {'refresh_pcg_components': 'refresh_pcg_components', 'bind_pcg_generation_settings_to_blueprint': 'bind_pcg_generation_settings_to_blueprint'}.get(name)
        if name == "add_blueprint_branch_node":
            name = "CubelessBlueprintGraphTools.add_blueprint_branch_node"
        niagara_source_template_raw_method = "create_niagara_system_from_source_template" if name=="create_niagara_system_from_source_template" else None
        niagara_promotion_manifest_raw_method = {'plan_niagara_promotion_manifest': 'plan_niagara_promotion_manifest', 'execute_niagara_promotion_manifest': 'execute_niagara_promotion_manifest'}.get(name)
        niagara_system_analysis_raw_method = {'analyze_niagara_system': 'analyze_niagara_system'}.get(name)
        pie_input_raw_method = {'inject_pie_input_key': 'inject_pie_input_key'}.get(name)
        niagara_preview_workflows_raw_method = {'open_niagara_preview_player': 'open_niagara_preview_player', 'get_niagara_preview_player_state': 'get_niagara_preview_player_state', 'preview_niagara_component_asset_swap': 'preview_niagara_component_asset_swap', 'sample_niagara_system_in_preview_lab': 'sample_niagara_system_in_preview_lab'}.get(name)
        blueprint_niagara_template_swap_raw_method = {'inspect_blueprint_niagara_component_template': 'inspect_blueprint_niagara_component_template', 'dry_run_blueprint_niagara_component_template_asset_swap': 'dry_run_blueprint_niagara_component_template_asset_swap', 'execute_blueprint_niagara_component_template_asset_swap_canary': 'execute_blueprint_niagara_component_template_asset_swap_canary', 'execute_blueprint_niagara_component_template_asset_swap_durable': 'execute_blueprint_niagara_component_template_asset_swap_durable'}.get(name)
        pcg_graph_authoring_raw_method = {'add_pcg_node': 'add_pcg_node', 'connect_pcg_nodes': 'connect_pcg_nodes', 'set_pcg_node_setting': 'set_pcg_node_setting'}.get(name)
        if niagara_resource_bindings_raw_method is not None:
            name = "CubelessNiagaraResourceBindingTools." + niagara_resource_bindings_raw_method
        if niagara_scratch_stack_authoring_raw_method is not None:
            name = "CubelessNiagaraScratchStackAuthoringTools." + niagara_scratch_stack_authoring_raw_method
        if niagara_event_stage_authoring_raw_method is not None:
            name = "CubelessNiagaraEventStageTools." + niagara_event_stage_authoring_raw_method
        if niagara_promotion_raw_method is not None:
            name = "CubelessNiagaraPromotionTools." + niagara_promotion_raw_method
        if niagara_sprite_preset_authoring_raw_method is not None:
            name = "CubelessNiagaraSpritePresetTools." + niagara_sprite_preset_authoring_raw_method
        if niagara_custom_hlsl_conversion_raw_method is not None:
            name = "CubelessNiagaraCustomHlslConversionTools." + niagara_custom_hlsl_conversion_raw_method
        if niagara_preview_lab_raw_method is not None:
            name = "CubelessNiagaraPreviewLabTools." + niagara_preview_lab_raw_method
        if controlrig_recipe_authoring_raw_method is not None:
            name = "CubelessControlRigRecipeAuthoringTools." + controlrig_recipe_authoring_raw_method
        if texture_material_authoring_raw_method is not None:
            name = "CubelessTextureMaterialTools." + texture_material_authoring_raw_method
        if blueprint_resolution_connection_raw_method is not None:
            name = "CubelessBlueprintResolutionConnectionTools." + blueprint_resolution_connection_raw_method
        if niagara_scratch_user_recipes_raw_method is not None:
            name = "CubelessNiagaraScratchUserRecipeTools." + niagara_scratch_user_recipes_raw_method
        if blueprint_property_graph_raw_method is not None:
            name = "CubelessBlueprintPropertyGraphTools." + blueprint_property_graph_raw_method
        if controlrig_recipe_planning_raw_method is not None:
            name = "CubelessControlRigRecipePlanningTools." + controlrig_recipe_planning_raw_method
        if pcg_utilities_raw_method is not None:
            name = "CubelessPCGUtilityTools." + pcg_utilities_raw_method
        if pcg_promotion_raw_method is not None:
            name = "CubelessPCGTools." + pcg_promotion_raw_method
        if pcg_contract_authoring_raw_method is not None:
            name = "CubelessPCGContractAuthoringTools." + pcg_contract_authoring_raw_method
        if pcg_component_generation_raw_method is not None:
            name = "CubelessPCGComponentGenerationTools." + pcg_component_generation_raw_method
        if niagara_source_template_raw_method is not None:
            name = "CubelessNiagaraPromotionTools." + niagara_source_template_raw_method
        if niagara_promotion_manifest_raw_method is not None:
            name = "CubelessNiagaraPromotionManifestTools." + niagara_promotion_manifest_raw_method
        if niagara_system_analysis_raw_method is not None:
            name = "CubelessNiagaraSystemAnalysisTools." + niagara_system_analysis_raw_method
        if pie_input_raw_method is not None:
            name = "CubelessPIEInputTools." + pie_input_raw_method
        if niagara_preview_workflows_raw_method is not None:
            name = "CubelessNiagaraPreviewWorkflowTools." + niagara_preview_workflows_raw_method
        if blueprint_niagara_template_swap_raw_method is not None:
            name = "CubelessBlueprintNiagaraTemplateSwapTools." + blueprint_niagara_template_swap_raw_method
        if pcg_graph_authoring_raw_method is not None:
            name = "CubelessPCGGraphAuthoringTools." + pcg_graph_authoring_raw_method
        niagara_runtime_inspection_raw_method = {'inspect_niagara_component_runtime_state': 'inspect_niagara_component_runtime_state', 'inspect_niagara_component_parameter_store': 'inspect_niagara_component_parameter_store', 'inspect_niagara_component_grid2d_runtime': 'inspect_niagara_component_grid2d_runtime', 'sample_render_target2d_region_stats': 'sample_render_target2d_region_stats'}.get(name)
        if niagara_runtime_inspection_raw_method is not None:
            name = "CubelessNiagaraRuntimeInspectionTools." + niagara_runtime_inspection_raw_method
        niagara_cleanup_raw_method = {'remove_niagara_modules': 'remove_niagara_modules', 'remove_niagara_parameter_map_set_nodes': 'remove_niagara_parameter_map_set_nodes', 'remove_niagara_orphan_parameter_map_get_nodes': 'remove_niagara_orphan_parameter_map_get_nodes', 'remove_niagara_user_parameters': 'remove_niagara_user_parameters', 'remove_niagara_simulation_stage': 'remove_niagara_simulation_stage', 'remove_niagara_scratch_pad_script': 'remove_niagara_scratch_pad_script', 'cleanup_generated_niagara_assets_offline':'cleanup_generated_niagara_assets_offline'}.get(name)
        if niagara_cleanup_raw_method is not None:
            name = "CubelessNiagaraCleanupTools." + niagara_cleanup_raw_method
        niagara_input_parameters_raw_method = {'inspect_niagara_rapid_iteration_parameters': 'inspect_niagara_rapid_iteration_parameters', 'set_niagara_rapid_iteration_parameter': 'set_niagara_rapid_iteration_parameter', 'set_niagara_module_static_switch': 'set_niagara_module_static_switch', 'inspect_niagara_module_inputs': 'inspect_niagara_module_inputs', 'inspect_niagara_data_interface_overrides': 'inspect_niagara_data_interface_overrides', 'set_niagara_module_input_value': 'set_niagara_module_input_value', 'create_niagara_module_input_override': 'create_niagara_module_input_override'}.get(name)
        if niagara_input_parameters_raw_method is not None:
            name = "CubelessNiagaraInputParameterTools." + niagara_input_parameters_raw_method
        niagara_module_input_paths_raw_method = {'remove_niagara_unused_module_input_path': 'remove_niagara_unused_module_input_path', 'set_niagara_module_input_user_parameter': 'set_niagara_module_input_user_parameter', 'set_niagara_array_data_interface_module_input_user_parameter': 'set_niagara_array_data_interface_module_input_user_parameter'}.get(name)
        if niagara_module_input_paths_raw_method is not None:
            name = "CubelessNiagaraModuleInputPathTools." + niagara_module_input_paths_raw_method
        niagara_data_channel_authoring_raw_method = {"configure_niagara_data_channel_asset": "configure_niagara_data_channel_asset", "create_niagara_ndc_reader_scratch_pad_module": "create_niagara_ndc_reader_scratch_pad_module", "add_niagara_scratch_pad_di_function_call": "add_niagara_scratch_pad_di_function_call", "add_niagara_scratch_pad_di_variadic_input": "add_niagara_scratch_pad_di_variadic_input", "CubelessNiagaraDataChannelAuthoringInternal.add_niagara_scratch_pad_di_function_call": "add_niagara_scratch_pad_di_function_call"}.get(name)
        if niagara_data_channel_authoring_raw_method is not None:
            name = {"configure_niagara_data_channel_asset": "CubelessNiagaraDataChannelAuthoringTools.configure_niagara_data_channel_asset", "create_niagara_ndc_reader_scratch_pad_module": "CubelessNiagaraDataChannelAuthoringTools.create_niagara_ndc_reader_scratch_pad_module", "add_niagara_scratch_pad_di_function_call": "CubelessNiagaraDataChannelAuthoringInternal.add_niagara_scratch_pad_di_function_call", "add_niagara_scratch_pad_di_variadic_input": "CubelessNiagaraDataChannelAuthoringTools.add_niagara_scratch_pad_di_variadic_input"}[niagara_data_channel_authoring_raw_method]
        geometry_collection_inspection_raw_method = name if name in ("inspect_geometry_collection_performance_guarded", "inspect_geometry_collection_runtime_state") else None
        if geometry_collection_inspection_raw_method is not None:
            name = "CubelessGeometryCollectionInspectionTools." + geometry_collection_inspection_raw_method
        landscape_raw_method = name if name in ("list_landscapes", "get_landscape_info", "create_landscape", "set_landscape_material") else None
        if landscape_raw_method is not None:
            name = "CubelessLandscapeTools." + landscape_raw_method
        foliage_instances_raw_method = name if name in ("list_foliage_types", "add_foliage_instances", "remove_foliage_instances") else None
        if foliage_instances_raw_method is not None:
            name = "CubelessFoliageInstanceTools." + foliage_instances_raw_method
        spline_component_points_raw_entry = name == "set_spline_component_points"
        if spline_component_points_raw_entry:
            name = "CubelessSplineComponentPointsTools.set_spline_component_points"
        if name == "collapse_blueprint_nodes_to_function":
            name = "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function"
        if name == "sample_anim_node_pre_post_runtime_pose":
            name = "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose"
        if name == "ensure_anim_bp_event_graph_locomotion_driver":
            name = "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver"
        if name == "ensure_anim_state_machine_sample_variant":
            name = "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant"
        if name == "ensure_linked_anim_graph_sample_variant":
            name = "CubelessLinkedAnimGraphVariantTools.ensure_linked_anim_graph_sample_variant"
        if name == "ensure_blendspace_sample_variant":
            name = "CubelessBlendSpaceVariantTools.ensure_blendspace_sample_variant"
        if name == "sample_blendspace_runtime_pose_grid":
            name = "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid"
        if name == "sample_skeletal_bones_in_sie":
            name = "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie"
        if name == "ensure_postprocess_anim_demo_variant":
            name = "CubelessPostProcessAnimDemoVariantTools.ensure_postprocess_anim_demo_variant"
        if name == "sample_anim_state_machine_runtime_response":
            name = "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response"
        if name == "set_anim_instance_runtime_property_for_probe":
            name = "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe"
        if name == "inspect_anim_instance_runtime_state":
            name = "CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state"
        if name == "set_controlrig_rigvm_pin_defaults":
            name = "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults"
        if name == "add_controlrig_rigvm_links":
            name = "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links"
        if name == "validate_controlrig_rigvm_pin_defaults":
            name = "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults"
        if name == "validate_controlrig_rigvm_links":
            name = "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links"
        if name == "inspect_controlrig_rigvm_topology":
            name = "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology"
        if name == "create_user_defined_enum":
            name = "CubelessEnumTools.create_user_defined_enum"
        if name == "line_trace_physical_material":
            name = "CubelessEditorTraceTools.line_trace_physical_material"
        if name == "replace_asset_references_mcp":
            name = "CubelessProjectTools.replace_asset_references_mcp"
        if name == "audit_content_root_mcp":
            name = "CubelessProjectTools.audit_content_root_mcp"
        if name not in _CAPABILITIES:
            raise OperationError("batch_unsupported_command", f"commands[{index}]: unsupported command {name}; no action executed")
        params = command.get("params")
        if params is None:
            params = {}
        fields = _CAPABILITIES[name]["fields"]
        if type(params) is not dict:
            raise OperationError("batch_invalid_entry_params", f"commands[{index}].params must be an object or null")
        actor_original_params = copy.deepcopy(params) if actor_original_command is not None else None
        if actor_original_command == "spawn_blueprint_actor":
            params = _actor_spawn_legacy_params(params)
        elif actor_original_command == "set_actor_transform":
            params = _actor_transform_legacy_params(params)
        if name in ("CubelessControlRigTransientProbeTools.controlrig_direct_gate_probe", "CubelessControlRigTransientProbeTools.sample_controlrig_pre_post_runtime_pose"):
            params = _controlrig_transient_probe_legacy_params(params)
        if name in ("CubelessPCGTools.validate_pcg_source_independence","CubelessPCGTools.promote_pcg_temp_graph"):
            from . import pcg_promotion
            params = pcg_promotion.normalize_public_mcp_arguments(name.rsplit(".",1)[1],params)
        if name == "CubelessNiagaraPromotionTools.create_niagara_system_from_source_template":
            from . import niagara_source_template
            params = niagara_source_template.normalize_public_mcp_arguments("create_niagara_system_from_source_template",params)
        chaos_solver_settings_legacy = None
        chaos_solver_settings_original = None
        if name in ("CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings", "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings"):
            from . import chaos_solver_settings
            chaos_solver_settings._reject_legacy_command_dependency_refs(params)
            chaos_solver_settings_original = copy.deepcopy(params)
            if chaos_solver_settings_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                chaos_solver_settings_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: ({} if key == "settings" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_solver_settings.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_cloth_component_legacy = None
        chaos_cloth_component_original = None
        if name in ("CubelessChaosClothComponentTools.inspect_chaos_cloth_component", "CubelessChaosClothComponentTools.configure_chaos_cloth_component"):
            from . import chaos_cloth_component
            chaos_cloth_component._reject_legacy_command_dependency_refs(params)
            chaos_cloth_component_original = copy.deepcopy(params)
            if chaos_cloth_component_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                chaos_cloth_component_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: "" if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_cloth_component.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_cloth_authoring_legacy = None
        chaos_cloth_authoring_original = None
        if name in ("CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map"):
            from . import chaos_cloth_authoring
            chaos_cloth_authoring._reject_legacy_command_dependency_refs(params)
            chaos_cloth_authoring_original = copy.deepcopy(params)
            if chaos_cloth_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                chaos_cloth_authoring_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: ({} if key == "property_values" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_cloth_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        legacy_chaos_clothing_legacy = None
        legacy_chaos_clothing_original = None
        if name in ("CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections",):
            from . import legacy_chaos_clothing
            legacy_chaos_clothing._reject_legacy_command_dependency_refs(params)
            legacy_chaos_clothing_original = copy.deepcopy(params)
            if legacy_chaos_clothing_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                legacy_chaos_clothing_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: ([] if key == "section_configs" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = legacy_chaos_clothing.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_cloth_inspection_legacy = None
        chaos_cloth_inspection_original = None
        if name in ("CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh"):
            from . import chaos_cloth_inspection
            chaos_cloth_inspection._reject_legacy_command_dependency_refs(params)
            chaos_cloth_inspection_original = copy.deepcopy(params)
            if chaos_cloth_inspection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                chaos_cloth_inspection_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: "" if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_cloth_inspection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        actor_property_focus_legacy = None
        actor_property_focus_original = None
        if name in ("CubelessActorPropertyFocusTools.set_actor_property", "CubelessActorPropertyFocusTools.focus_viewport"):
            from . import actor_property_focus
            actor_property_focus._reject_legacy_command_dependency_refs(params)
            actor_property_focus_original = copy.deepcopy(params)
            if actor_property_focus_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                actor_property_focus_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = actor_property_focus.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        editor_level_session_legacy = None
        editor_level_session_original = None
        if name in ("CubelessEditorLevelSessionTools.open_editor_level", "CubelessEditorLevelSessionTools.get_editor_level_open_status", "CubelessEditorLevelSessionTools.clear_dirty_package_flags", "CubelessEditorLevelSessionTools.safe_new_preview_map"):
            from . import editor_level_session
            editor_level_session._reject_legacy_command_dependency_refs(params)
            editor_level_session_original = copy.deepcopy(params)
            if editor_level_session_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                hidden = ("map_path",) if editor_level_session_raw_method == "open_editor_level" else (("level_path",) if editor_level_session_raw_method == "safe_new_preview_map" else ())
                editor_level_session_legacy = {key: copy.deepcopy(params[key]) for key in tuple(public) + hidden if key in params}
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = editor_level_session.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        viewport_capture_legacy = None
        viewport_capture_original = None
        if name in ("CubelessViewportCaptureTools.list_viewport_bookmarks", "CubelessViewportCaptureTools.set_active_viewport_realtime", "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot", "CubelessViewportCaptureTools.take_screenshot"):
            from . import viewport_capture
            viewport_capture._reject_legacy_command_dependency_refs(params)
            viewport_capture_original = copy.deepcopy(params)
            if viewport_capture_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(public) + (("fov",) if viewport_capture_raw_method == "capture_viewport_bookmark_screenshot" else ())
                viewport_capture_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = viewport_capture.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_cinematic_sequence_legacy = None
        chaos_cinematic_sequence_original = None
        if name in ("CubelessChaosCinematicSequenceTools.create_chaos_level_sequence", "CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence", "CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing", "CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline"):
            from . import chaos_cinematic_sequence
            chaos_cinematic_sequence._reject_legacy_command_dependency_refs(params)
            chaos_cinematic_sequence_original = copy.deepcopy(params)
            if chaos_cinematic_sequence_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(public) + (("geometry_collection_actor_names", "cache_names") if chaos_cinematic_sequence_raw_method == "bind_chaos_cache_to_sequence" else ())
                chaos_cinematic_sequence_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = chaos_cinematic_sequence.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_cache_sessions_legacy = None
        chaos_cache_sessions_original = None
        if name in ("CubelessChaosCacheSessionTools.record_chaos_cache", "CubelessChaosCacheSessionTools.validate_chaos_cache"):
            from . import chaos_cache_sessions
            chaos_cache_sessions._reject_legacy_command_dependency_refs(params)
            chaos_cache_sessions_original = copy.deepcopy(params)
            if chaos_cache_sessions_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(chaos_cache_sessions.PARAM_FIELDS[chaos_cache_sessions_raw_method])
                chaos_cache_sessions_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if key == "geometry_collection_actor_names" and chaos_cache_sessions_raw_method == "record_chaos_cache" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_cache_sessions.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_destruction_performance_legacy = None
        chaos_destruction_performance_original = None
        if name in ("CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke",):
            from . import chaos_destruction_performance
            chaos_destruction_performance._reject_legacy_command_dependency_refs(params)
            chaos_destruction_performance_original = copy.deepcopy(params)
            if chaos_destruction_performance_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(chaos_destruction_performance.PARAM_FIELDS[chaos_destruction_performance_raw_method])
                chaos_destruction_performance_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = chaos_destruction_performance.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_learning_legacy = None
        chaos_learning_original = None
        if name in ("CubelessChaosLearningTools.create_chaos_destruction_learning_fixture", "CubelessChaosLearningTools.run_chaos_cache_smoke"):
            from . import chaos_learning
            chaos_learning._reject_legacy_command_dependency_refs(params)
            chaos_learning_original = copy.deepcopy(params)
            if chaos_learning_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(chaos_learning.PARAM_FIELDS[chaos_learning_raw_method])
                chaos_learning_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = chaos_learning.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_trace_strain_legacy = None
        chaos_trace_strain_original = None
        if name in ("CubelessChaosTraceStrainTools.trigger_trace_external_strain",):
            from . import chaos_trace_strain
            chaos_trace_strain._reject_legacy_command_dependency_refs(params)
            chaos_trace_strain_original = copy.deepcopy(params)
            if chaos_trace_strain_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(chaos_trace_strain.PARAM_FIELDS[chaos_trace_strain_raw_method])
                chaos_trace_strain_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = chaos_trace_strain.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        chaos_geometry_authoring_legacy = None
        chaos_geometry_authoring_original = None
        if name in ("CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh", "CubelessChaosGeometryAuthoringTools.configure_building_fracture", "CubelessChaosGeometryAuthoringTools.configure_anchor_field"):
            from . import chaos_geometry_authoring
            chaos_geometry_authoring._reject_legacy_command_dependency_refs(params)
            chaos_geometry_authoring_original = copy.deepcopy(params)
            if chaos_geometry_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(chaos_geometry_authoring.PARAM_FIELDS[chaos_geometry_authoring_raw_method])
                chaos_geometry_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if key == "source_static_mesh_paths" and chaos_geometry_authoring_raw_method == "create_geometry_collection_from_mesh" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = chaos_geometry_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        gas_authoring_legacy = None
        gas_authoring_original = None
        if name in ("CubelessGASAuthoringTools.inspect_gas_blueprint", "CubelessGASAuthoringTools.create_gameplay_ability_blueprint", "CubelessGASAuthoringTools.create_gameplay_effect_blueprint", "CubelessGASAuthoringTools.create_attribute_set_blueprint", "CubelessGASAuthoringTools.add_ability_system_component_to_blueprint", "CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set"):
            from . import gas_authoring
            gas_authoring._reject_legacy_command_dependency_refs(params)
            gas_authoring_original = copy.deepcopy(params)
            if gas_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(gas_authoring.PARAM_FIELDS[gas_authoring_raw_method])
                gas_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = gas_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        content_reconstruction_legacy = None
        content_reconstruction_original = None
        if name in ("CubelessContentReconstructionTools.recreate_content_folder_mcp", "CubelessContentReconstructionTools.postprocess_content_folder_mcp", "CubelessContentReconstructionTools.repair_world_actor_instances_mcp", "CubelessContentReconstructionTools.run_content_validation_pipeline_mcp"):
            from . import content_reconstruction
            content_reconstruction._reject_legacy_command_dependency_refs(params)
            content_reconstruction_original = copy.deepcopy(params)
            if content_reconstruction_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(content_reconstruction.PARAM_FIELDS[content_reconstruction_raw_method])
                content_reconstruction_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = content_reconstruction.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_renderers_legacy = None
        niagara_renderers_original = None
        if name in ("CubelessNiagaraRendererTools.inspect_niagara_renderers", "CubelessNiagaraRendererTools.set_niagara_renderer_material", "CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings", "CubelessNiagaraRendererTools.add_niagara_sprite_renderer", "CubelessNiagaraRendererTools.add_niagara_mesh_renderer", "CubelessNiagaraRendererTools.set_niagara_renderer_enabled", "CubelessNiagaraRendererTools.cleanup_niagara_renderers"):
            from . import niagara_renderers
            niagara_renderers._reject_legacy_command_dependency_refs(params)
            niagara_renderers_original = copy.deepcopy(params)
            if niagara_renderers_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_renderers.PARAM_FIELDS[niagara_renderers_raw_method])
                niagara_renderers_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ((False if key == "enabled" else "") if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_renderers.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_emitter_configuration_legacy = None
        niagara_emitter_configuration_original = None
        if name in ("CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids", "CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter"):
            from . import niagara_emitter_configuration
            niagara_emitter_configuration._reject_legacy_command_dependency_refs(params)
            niagara_emitter_configuration_original = copy.deepcopy(params)
            if niagara_emitter_configuration_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_emitter_configuration.PARAM_FIELDS[niagara_emitter_configuration_raw_method])
                niagara_emitter_configuration_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ((False if key in ("enabled","value") else "") if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_emitter_configuration.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_parameter_collections_legacy = None
        niagara_parameter_collections_original = None
        if name in ("CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values", "CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values", "CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection", "CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references", "CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references", "CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root"):
            from . import niagara_parameter_collections
            niagara_parameter_collections._reject_legacy_command_dependency_refs(params)
            niagara_parameter_collections_original = copy.deepcopy(params)
            if niagara_parameter_collections_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_parameter_collections.PARAM_FIELDS[niagara_parameter_collections_raw_method])
                niagara_parameter_collections_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: (({} if key=="values" else "") if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_parameter_collections.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_authoring_inspection_legacy = None
        niagara_authoring_inspection_original = None
        if name in ("CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status"):
            from . import niagara_authoring_inspection
            niagara_authoring_inspection._reject_legacy_command_dependency_refs(params)
            niagara_authoring_inspection_original = copy.deepcopy(params)
            if niagara_authoring_inspection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_authoring_inspection.PARAM_FIELDS[niagara_authoring_inspection_raw_method])
                niagara_authoring_inspection_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_authoring_inspection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        pcg_inspection_legacy = None
        pcg_inspection_original = None
        if name in ("CubelessPCGInspectionTools.resolve_pcg_graph", "CubelessPCGInspectionTools.list_pcg_graph_nodes", "CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes", "CubelessPCGInspectionTools.inspect_pcg_component_outputs"):
            from . import pcg_inspection
            pcg_inspection._reject_legacy_command_dependency_refs(params)
            pcg_inspection_original = copy.deepcopy(params)
            if pcg_inspection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pcg_inspection.PARAM_FIELDS[pcg_inspection_raw_method])
                pcg_inspection_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = pcg_inspection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)

        niagara_factory_bootstrap_legacy = None
        niagara_factory_bootstrap_original = None
        if name in ("CubelessNiagaraFactoryBootstrapTools.inspect_niagara_factory_bootstrap_defaults", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_system", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_emitter", "CubelessNiagaraFactoryBootstrapTools.create_basic_sprite_niagara_system"):
            from . import niagara_factory_bootstrap
            niagara_factory_bootstrap._reject_legacy_command_dependency_refs(params)
            niagara_factory_bootstrap_original = copy.deepcopy(params)
            if niagara_factory_bootstrap_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_factory_bootstrap.PARAM_FIELDS[niagara_factory_bootstrap_raw_method])
                niagara_factory_bootstrap_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_factory_bootstrap.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)

        niagara_resource_bindings_legacy = None
        niagara_scratch_stack_authoring_legacy = None
        niagara_event_stage_authoring_legacy = None
        niagara_promotion_legacy = None
        niagara_sprite_preset_authoring_legacy = None
        niagara_custom_hlsl_conversion_legacy = None
        niagara_preview_lab_legacy = None
        controlrig_recipe_authoring_legacy = None
        texture_material_authoring_legacy = None
        blueprint_resolution_connection_legacy = None
        niagara_scratch_user_recipes_legacy = None
        blueprint_property_graph_legacy = None
        controlrig_recipe_planning_legacy = None
        pcg_utilities_legacy = None
        pcg_contract_authoring_legacy = None
        pcg_component_generation_legacy = None
        niagara_promotion_manifest_legacy = None
        niagara_system_analysis_legacy = None
        pie_input_legacy = None
        niagara_preview_workflows_legacy = None
        blueprint_niagara_template_swap_legacy = None
        pcg_graph_authoring_legacy = None
        niagara_resource_bindings_original = None
        niagara_scratch_stack_authoring_original = None
        niagara_event_stage_authoring_original = None
        niagara_promotion_original = None
        niagara_sprite_preset_authoring_original = None
        niagara_custom_hlsl_conversion_original = None
        niagara_preview_lab_original = None
        controlrig_recipe_authoring_original = None
        texture_material_authoring_original = None
        blueprint_resolution_connection_original = None
        niagara_scratch_user_recipes_original = None
        blueprint_property_graph_original = None
        controlrig_recipe_planning_original = None
        pcg_utilities_original = None
        pcg_contract_authoring_original = None
        pcg_component_generation_original = None
        niagara_promotion_manifest_original = None
        niagara_system_analysis_original = None
        pie_input_original = None
        niagara_preview_workflows_original = None
        blueprint_niagara_template_swap_original = None
        pcg_graph_authoring_original = None
        if name in ("CubelessNiagaraResourceBindingTools.set_niagara_render_target2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_texture2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_scratch_pad_function_input_default", "CubelessNiagaraResourceBindingTools.link_niagara_scratch_pad_pin_to_user_parameter"):
            from . import niagara_resource_bindings
            niagara_resource_bindings._reject_legacy_command_dependency_refs(params)
            niagara_resource_bindings_original = copy.deepcopy(params)
            if niagara_resource_bindings_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_resource_bindings.PARAM_FIELDS[niagara_resource_bindings_raw_method])
                niagara_resource_bindings_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_resource_bindings.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_module_input', 'CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_assignment_module_to_stack', 'CubelessNiagaraScratchStackAuthoringTools.wrap_niagara_scratch_pad_output_with_stack_context'):
            from . import niagara_scratch_stack_authoring
            niagara_scratch_stack_authoring._reject_legacy_command_dependency_refs(params)
            niagara_scratch_stack_authoring_original = copy.deepcopy(params)
            if niagara_scratch_stack_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_scratch_stack_authoring.PARAM_FIELDS[niagara_scratch_stack_authoring_raw_method])
                niagara_scratch_stack_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_scratch_stack_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraEventStageTools.inspect_niagara_simulation_stages', 'CubelessNiagaraEventStageTools.inspect_niagara_event_handlers', 'CubelessNiagaraEventStageTools.add_niagara_event_handler'):
            from . import niagara_event_stage_authoring
            niagara_event_stage_authoring._reject_legacy_command_dependency_refs(params)
            niagara_event_stage_authoring_original = copy.deepcopy(params)
            if niagara_event_stage_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_event_stage_authoring.PARAM_FIELDS[niagara_event_stage_authoring_raw_method])
                niagara_event_stage_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_event_stage_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraPromotionTools.audit_niagara_promotion_candidate', 'CubelessNiagaraPromotionTools.promote_niagara_system_candidate'):
            from . import niagara_promotion
            niagara_promotion._reject_legacy_command_dependency_refs(params)
            niagara_promotion_original = copy.deepcopy(params)
            if niagara_promotion_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_promotion.PARAM_FIELDS[niagara_promotion_raw_method])
                niagara_promotion_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_promotion.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraSpritePresetTools.create_sprite_niagara_preset_system',):
            from . import niagara_sprite_preset_authoring
            niagara_sprite_preset_authoring._reject_legacy_command_dependency_refs(params)
            niagara_sprite_preset_authoring_original = copy.deepcopy(params)
            if niagara_sprite_preset_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_sprite_preset_authoring.PARAM_FIELDS[niagara_sprite_preset_authoring_raw_method])
                niagara_sprite_preset_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_sprite_preset_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraCustomHlslConversionTools.replace_niagara_custom_hlsl', 'CubelessNiagaraCustomHlslConversionTools.apply_niagara_scratch_pad_graph_recipe'):
            from . import niagara_custom_hlsl_conversion
            niagara_custom_hlsl_conversion._reject_legacy_command_dependency_refs(params)
            niagara_custom_hlsl_conversion_original = copy.deepcopy(params)
            if niagara_custom_hlsl_conversion_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_custom_hlsl_conversion.PARAM_FIELDS[niagara_custom_hlsl_conversion_raw_method])
                niagara_custom_hlsl_conversion_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_custom_hlsl_conversion.FIELD_TYPES[niagara_custom_hlsl_conversion_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_custom_hlsl_conversion.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state', 'CubelessNiagaraPreviewLabTools.cleanup_niagara_preview_lab', 'CubelessNiagaraPreviewLabTools.capture_niagara_preview_lab_view', 'CubelessNiagaraPreviewLabTools.preview_niagara_system_in_preview_lab'):
            from . import niagara_preview_lab
            niagara_preview_lab._reject_legacy_command_dependency_refs(params)
            niagara_preview_lab_original = copy.deepcopy(params)
            if niagara_preview_lab_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_preview_lab.PARAM_FIELDS[niagara_preview_lab_raw_method])
                niagara_preview_lab_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_preview_lab.FIELD_TYPES[niagara_preview_lab_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_preview_lab.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe', 'CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe', 'CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe', 'CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe_with_animbp_binding'):
            from . import controlrig_recipe_authoring
            controlrig_recipe_authoring._reject_legacy_command_dependency_refs(params)
            controlrig_recipe_authoring_original = copy.deepcopy(params)
            if controlrig_recipe_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(controlrig_recipe_authoring.PARAM_FIELDS[controlrig_recipe_authoring_raw_method])
                controlrig_recipe_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if controlrig_recipe_authoring.FIELD_TYPES[controlrig_recipe_authoring_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = controlrig_recipe_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessTextureMaterialTools.get_static_mesh_uv_layout', 'CubelessTextureMaterialTools.create_material_instance_with_texture', 'CubelessTextureMaterialTools.create_material_with_texture_set','CubelessTextureMaterialTools.import_create_and_apply_texture_set','CubelessTextureMaterialTools.generate_pbr_companion_textures','CubelessTextureMaterialTools.import_texture_to_unreal','CubelessTextureMaterialTools.import_texture_set_to_unreal','CubelessTextureMaterialTools.apply_material_to_mesh','CubelessTextureMaterialTools.generate_texture_from_prompt','CubelessTextureMaterialTools.generate_texture_for_mesh_uv','CubelessTextureMaterialTools.generate_and_apply_ai_texture'):
            from . import texture_material_authoring
            texture_material_authoring._reject_legacy_command_dependency_refs(params)
            texture_material_authoring_original = copy.deepcopy(params)
            if texture_material_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(texture_material_authoring.PARAM_FIELDS[texture_material_authoring_raw_method])
                texture_material_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if texture_material_authoring.FIELD_TYPES[texture_material_authoring_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = texture_material_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessBlueprintResolutionConnectionTools.resolve_blueprint', 'CubelessBlueprintResolutionConnectionTools.connect_blueprint_nodes'):
            from . import blueprint_resolution_connection
            blueprint_resolution_connection._reject_legacy_command_dependency_refs(params)
            blueprint_resolution_connection_original = copy.deepcopy(params)
            if blueprint_resolution_connection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(blueprint_resolution_connection.PARAM_FIELDS[blueprint_resolution_connection_raw_method])
                blueprint_resolution_connection_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if blueprint_resolution_connection.FIELD_TYPES[blueprint_resolution_connection_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = blueprint_resolution_connection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraScratchUserRecipeTools.create_niagara_visible_scratch_pad_snow_seed', 'CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_vec2_user_parameter_module', 'CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_color_user_parameter_module', 'CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_user_parameter_module', 'CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_visible_toggle_module'):
            from . import niagara_scratch_user_recipes
            niagara_scratch_user_recipes._reject_legacy_command_dependency_refs(params)
            niagara_scratch_user_recipes_original = copy.deepcopy(params)
            if niagara_scratch_user_recipes_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_scratch_user_recipes.PARAM_FIELDS[niagara_scratch_user_recipes_raw_method])
                niagara_scratch_user_recipes_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_scratch_user_recipes.FIELD_TYPES[niagara_scratch_user_recipes_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_scratch_user_recipes.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessBlueprintPropertyGraphTools.set_blueprint_property', 'CubelessBlueprintPropertyGraphTools.list_blueprint_graphs'):
            from . import blueprint_property_graph
            blueprint_property_graph._reject_legacy_command_dependency_refs(params)
            blueprint_property_graph_original = copy.deepcopy(params)
            if blueprint_property_graph_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(blueprint_property_graph.PARAM_FIELDS[blueprint_property_graph_raw_method])
                blueprint_property_graph_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if blueprint_property_graph.FIELD_TYPES[blueprint_property_graph_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = blueprint_property_graph.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton', 'CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe', 'CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe'):
            from . import controlrig_recipe_planning
            controlrig_recipe_planning._reject_legacy_command_dependency_refs(params)
            controlrig_recipe_planning_original = copy.deepcopy(params)
            if controlrig_recipe_planning_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(controlrig_recipe_planning.PARAM_FIELDS[controlrig_recipe_planning_raw_method])
                controlrig_recipe_planning_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if controlrig_recipe_planning.FIELD_TYPES[controlrig_recipe_planning_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = controlrig_recipe_planning.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessPCGUtilityTools.list_pcg_assets', 'CubelessPCGUtilityTools.list_pcg_components', 'CubelessPCGUtilityTools.set_pcg_debug_enabled', 'CubelessPCGUtilityTools.resave_pcg_assets'):
            from . import pcg_utilities
            pcg_utilities._reject_legacy_command_dependency_refs(params)
            pcg_utilities_original = copy.deepcopy(params)
            if pcg_utilities_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pcg_utilities.PARAM_FIELDS[pcg_utilities_raw_method])
                pcg_utilities_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if pcg_utilities.FIELD_TYPES[pcg_utilities_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = pcg_utilities.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessPCGContractAuthoringTools.audit_pcg_graph_contract', 'CubelessPCGContractAuthoringTools.read_pcg_node_contract', 'CubelessPCGContractAuthoringTools.set_pcg_static_mesh_spawner_entries'):
            from . import pcg_contract_authoring
            pcg_contract_authoring._reject_legacy_command_dependency_refs(params)
            pcg_contract_authoring_original = copy.deepcopy(params)
            if pcg_contract_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pcg_contract_authoring.PARAM_FIELDS[pcg_contract_authoring_raw_method])
                pcg_contract_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if pcg_contract_authoring.FIELD_TYPES[pcg_contract_authoring_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = pcg_contract_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessPCGComponentGenerationTools.refresh_pcg_components', 'CubelessPCGComponentGenerationTools.bind_pcg_generation_settings_to_blueprint'):
            from . import pcg_component_generation
            pcg_component_generation._reject_legacy_command_dependency_refs(params)
            pcg_component_generation_original = copy.deepcopy(params)
            if pcg_component_generation_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pcg_component_generation.PARAM_FIELDS[pcg_component_generation_raw_method])
                pcg_component_generation_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if pcg_component_generation.FIELD_TYPES[pcg_component_generation_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = pcg_component_generation.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest', 'CubelessNiagaraPromotionManifestTools.execute_niagara_promotion_manifest'):
            from . import niagara_promotion_manifest
            niagara_promotion_manifest._reject_legacy_command_dependency_refs(params)
            niagara_promotion_manifest_original = copy.deepcopy(params)
            if niagara_promotion_manifest_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_promotion_manifest.PARAM_FIELDS[niagara_promotion_manifest_raw_method])
                niagara_promotion_manifest_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_promotion_manifest.FIELD_TYPES[niagara_promotion_manifest_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_promotion_manifest.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraSystemAnalysisTools.analyze_niagara_system',):
            from . import niagara_system_analysis
            niagara_system_analysis._reject_legacy_command_dependency_refs(params)
            niagara_system_analysis_original = copy.deepcopy(params)
            if niagara_system_analysis_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_system_analysis.PARAM_FIELDS[niagara_system_analysis_raw_method])
                niagara_system_analysis_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_system_analysis.FIELD_TYPES[niagara_system_analysis_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_system_analysis.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessPIEInputTools.inject_pie_input_key',):
            from . import pie_input
            pie_input._reject_legacy_command_dependency_refs(params)
            pie_input_original = copy.deepcopy(params)
            if pie_input_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pie_input.PARAM_FIELDS[pie_input_raw_method])
                pie_input_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if pie_input.FIELD_TYPES[pie_input_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = pie_input.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessNiagaraPreviewWorkflowTools.open_niagara_preview_player', 'CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_player_state', 'CubelessNiagaraPreviewWorkflowTools.preview_niagara_component_asset_swap', 'CubelessNiagaraPreviewWorkflowTools.sample_niagara_system_in_preview_lab'):
            from . import niagara_preview_workflows
            niagara_preview_workflows._reject_legacy_command_dependency_refs(params)
            niagara_preview_workflows_original = copy.deepcopy(params)
            if niagara_preview_workflows_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_preview_workflows.PARAM_FIELDS[niagara_preview_workflows_raw_method])
                niagara_preview_workflows_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if niagara_preview_workflows.FIELD_TYPES[niagara_preview_workflows_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = niagara_preview_workflows.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template', 'CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap', 'CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary', 'CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable'):
            from . import blueprint_niagara_template_swap
            blueprint_niagara_template_swap._reject_legacy_command_dependency_refs(params)
            blueprint_niagara_template_swap_original = copy.deepcopy(params)
            if blueprint_niagara_template_swap_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(blueprint_niagara_template_swap.PARAM_FIELDS[blueprint_niagara_template_swap_raw_method])
                blueprint_niagara_template_swap_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if blueprint_niagara_template_swap.FIELD_TYPES[blueprint_niagara_template_swap_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = blueprint_niagara_template_swap.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        if name in ('CubelessPCGGraphAuthoringTools.add_pcg_node', 'CubelessPCGGraphAuthoringTools.connect_pcg_nodes', 'CubelessPCGGraphAuthoringTools.set_pcg_node_setting'):
            from . import pcg_graph_authoring
            pcg_graph_authoring._reject_legacy_command_dependency_refs(params)
            pcg_graph_authoring_original = copy.deepcopy(params)
            if pcg_graph_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(pcg_graph_authoring.PARAM_FIELDS[pcg_graph_authoring_raw_method])
                pcg_graph_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ([] if pcg_graph_authoring.FIELD_TYPES[pcg_graph_authoring_raw_method][key] == "array" else "") if default is _REQUIRED else copy.deepcopy(default) for key, (_, default) in public.items()}
            else:
                params = pcg_graph_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_runtime_inspection_legacy = None
        niagara_runtime_inspection_original = None
        if name in ("CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_runtime_state", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_parameter_store", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_grid2d_runtime", "CubelessNiagaraRuntimeInspectionTools.sample_render_target2d_region_stats"):
            from . import niagara_runtime_inspection
            niagara_runtime_inspection._reject_legacy_command_dependency_refs(params)
            niagara_runtime_inspection_original = copy.deepcopy(params)
            if niagara_runtime_inspection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_runtime_inspection.PARAM_FIELDS[niagara_runtime_inspection_raw_method])
                niagara_runtime_inspection_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_runtime_inspection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_cleanup_legacy = None
        niagara_cleanup_original = None
        if name in ("CubelessNiagaraCleanupTools.remove_niagara_modules", "CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes", "CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes", "CubelessNiagaraCleanupTools.remove_niagara_user_parameters", "CubelessNiagaraCleanupTools.remove_niagara_simulation_stage", "CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script","CubelessNiagaraCleanupTools.cleanup_generated_niagara_assets_offline"):
            from . import niagara_cleanup
            niagara_cleanup._reject_legacy_command_dependency_refs(params)
            niagara_cleanup_original = copy.deepcopy(params)
            if niagara_cleanup_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_cleanup.PARAM_FIELDS[niagara_cleanup_raw_method])
                niagara_cleanup_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_cleanup.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_input_parameters_legacy = None
        niagara_input_parameters_original = None
        if name in ("CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters", "CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter", "CubelessNiagaraInputParameterTools.set_niagara_module_static_switch", "CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs", "CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides", "CubelessNiagaraInputParameterTools.set_niagara_module_input_value", "CubelessNiagaraInputParameterTools.create_niagara_module_input_override"):
            from . import niagara_input_parameters
            niagara_input_parameters._reject_legacy_command_dependency_refs(params)
            niagara_input_parameters_original = copy.deepcopy(params)
            if niagara_input_parameters_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_input_parameters.PARAM_FIELDS[niagara_input_parameters_raw_method])
                niagara_input_parameters_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_input_parameters.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_module_input_paths_legacy = None
        niagara_module_input_paths_original = None
        if name in ("CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path", "CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter", "CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter"):
            from . import niagara_module_input_paths
            niagara_module_input_paths._reject_legacy_command_dependency_refs(params)
            niagara_module_input_paths_original = copy.deepcopy(params)
            if niagara_module_input_paths_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_module_input_paths.PARAM_FIELDS[niagara_module_input_paths_raw_method])
                niagara_module_input_paths_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = niagara_module_input_paths.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        niagara_data_channel_authoring_legacy = None
        niagara_data_channel_authoring_original = None
        if name in ["CubelessNiagaraDataChannelAuthoringTools.configure_niagara_data_channel_asset", "CubelessNiagaraDataChannelAuthoringTools.create_niagara_ndc_reader_scratch_pad_module", "CubelessNiagaraDataChannelAuthoringInternal.add_niagara_scratch_pad_di_function_call", "CubelessNiagaraDataChannelAuthoringTools.add_niagara_scratch_pad_di_variadic_input"]:
            from . import niagara_data_channel_authoring
            niagara_data_channel_authoring._reject_legacy_command_dependency_refs(params)
            niagara_data_channel_authoring_original = copy.deepcopy(params)
            if niagara_data_channel_authoring_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(niagara_data_channel_authoring.PARAM_FIELDS[niagara_data_channel_authoring_raw_method])
                niagara_data_channel_authoring_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values/presence are Native-owned; placeholders only satisfy public slot storage.
                params = {key: (copy.deepcopy(default) if default is not _REQUIRED else [] if list in kinds else {} if dict in kinds else False if bool in kinds else 0 if int in kinds else "") for key, (kinds, default) in public.items()}
            else:
                params = niagara_data_channel_authoring.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        geometry_collection_inspection_legacy = None
        geometry_collection_inspection_original = None
        if name in ("CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded", "CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state"):
            from . import geometry_collection_inspection
            geometry_collection_inspection._reject_legacy_command_dependency_refs(params)
            geometry_collection_inspection_original = copy.deepcopy(params)
            if geometry_collection_inspection_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                read_fields = tuple(geometry_collection_inspection.PARAM_FIELDS[geometry_collection_inspection_raw_method])
                geometry_collection_inspection_legacy = {key: copy.deepcopy(params[key]) for key in read_fields if key in params}
                # Raw values and absence remain Native-owned; these public slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
            else:
                params = geometry_collection_inspection.normalize_public_mcp_arguments(name.rsplit(".", 1)[1], params)
        landscape_legacy = None
        landscape_original = None
        if name in ("CubelessLandscapeTools.list_landscapes", "CubelessLandscapeTools.get_landscape_info", "CubelessLandscapeTools.create_landscape", "CubelessLandscapeTools.set_landscape_material"):
            from . import landscape
            landscape._reject_legacy_command_dependency_refs(params)
            landscape_original = copy.deepcopy(params)
            if landscape_raw_method is not None:
                public = _CAPABILITIES[name]["fields"]
                landscape_legacy = {key: copy.deepcopy(params[key]) for key in public if key in params}
                # Exact raw public values/presence are Native-owned; typed slots are placeholders only.
                params = {key: ("" if default is _REQUIRED else copy.deepcopy(default)) for key, (_, default) in public.items()}
        foliage_instances_legacy = None
        foliage_instances_original = None
        if name in ("CubelessFoliageInstanceTools.list_foliage_types", "CubelessFoliageInstanceTools.add_foliage_instances", "CubelessFoliageInstanceTools.remove_foliage_instances"):
            from . import foliage_instances
            foliage_instances._reject_legacy_command_dependency_refs(params)
            foliage_instances_original = copy.deepcopy(params)
            if foliage_instances_raw_method is not None:
                foliage_instances_legacy = {}
                if foliage_instances_raw_method == "remove_foliage_instances":
                    foliage_instances_legacy = {key: params[key] for key in ("instance_indices", "remove_all", "max_remove_count") if key in params}
                    # Typed slots are placeholders; only the exact raw context is authoritative in Native.
                    params = dict(params)
                    params.update(instance_indices=[], remove_all=False, max_remove_count=5000)
        spline_component_points_legacy = None
        spline_component_points_original = None
        if name == "CubelessSplineComponentPointsTools.set_spline_component_points":
            from . import spline_component_points as spline_points
            spline_points._reject_legacy_command_dependency_refs(params)
            spline_component_points_original = copy.deepcopy(params)
            if spline_component_points_raw_entry:
                # Old raw Batch bypassed Named/native_first and forwarded original Native params.
                spline_component_points_legacy = {key: params[key] for key in spline_points.LEGACY_COMMAND_FIELDS if key in params}
                params = {key: value for key, value in params.items() if key not in spline_points.LEGACY_COMMAND_FIELDS and key != "native_first"}
                for field in ("actor_label", "actor_name", "actor_tag", "actor_label_prefix", "component_tag"):
                    if field in params and params[field] is None:
                        params[field] = ""  # Original TryGetStringField null observed as empty FString.
        blueprint_function_extraction_legacy = None
        blueprint_function_extraction_original = None
        if name == "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function":
            from . import blueprint_function_extraction as extraction
            extraction._reject_legacy_command_dependency_refs(params)
            blueprint_function_extraction_original = copy.deepcopy(params)
            blueprint_function_extraction_legacy = {key: params[key] for key in ("graph_id",) if key in params}
            params = {key: value for key, value in params.items() if key not in blueprint_function_extraction_legacy}
            # The original command uses save_asset OR save, including explicit canonical False.
            if "save" in params:
                if type(params["save"]) is not bool or ("save_asset" in params and type(params["save_asset"]) is not bool):
                    raise OperationError("batch_parameter_type_mismatch", "FJ save_asset and save must be exact Boolean values")
                legacy_save = params.pop("save")
                params["save_asset"] = params.get("save_asset", False) or legacy_save
        anim_node_pre_post_runtime_pose_legacy = None
        anim_node_pre_post_runtime_pose_original = None
        if name == "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose":
            from . import anim_node_pre_post_runtime_pose as node_prepost
            node_prepost._reject_legacy_command_dependency_refs(params)
            anim_node_pre_post_runtime_pose_original = copy.deepcopy(params)
            # Preserve actual command presence before public defaults; even {} differs from Named None.
            anim_node_pre_post_runtime_pose_legacy = {key: params[key] for key in ("sample_bones", "sample_sockets", "anim_blueprint_path", "allow_missing_samples", "graph_name", "graph_id", "graph_type", "input_pose_index", "properties", "property_name", "curve_values") if key in params}
            params = {key: value for key, value in params.items() if key not in anim_node_pre_post_runtime_pose_legacy}
        anim_bp_locomotion_driver_legacy = None
        anim_bp_locomotion_driver_original = None
        if name == "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver":
            from .anim_bp_locomotion_driver import _reject_legacy_command_dependency_refs
            _reject_legacy_command_dependency_refs(params)
            anim_bp_locomotion_driver_original = copy.deepcopy(params)
            params = dict(params)
            # Source aliases use original raw IsEmpty before Native trim.
            for alias in ("source_blueprint_name", "blueprint_name"):
                candidate = params.pop(alias, "")
                if params.get("source_anim_bp_path", "") == "" and candidate != "":
                    if type(candidate) is not str:
                        raise OperationError("anim_bp_locomotion_driver_legacy_path_unsupported", "Selected source alias must be an exact string; no action executed")
                    params["source_anim_bp_path"] = candidate
            if "save_asset" not in params and "save" in params:
                if type(params["save"]) is not bool:
                    raise OperationError("anim_bp_locomotion_driver_legacy_bool_unsupported", "Selected save alias must be exact bool; no action executed")
                params["save_asset"] = params["save"]
            params.pop("save", None)
            # Only hidden-command key omission inherits global create, before defaults.
            # Named public optional flags keep their original explicit True defaults.
            create_global = params.get("create_variable_if_missing", True)
            for optional in ("create_direction_variable_if_missing", "create_is_in_air_variable_if_missing",
                    "create_acceleration_variable_if_missing", "create_is_accelerating_variable_if_missing"):
                if optional not in params:
                    params[optional] = create_global
            anim_bp_locomotion_driver_legacy = {key: params.pop(key) for key in ("target_blueprint_path",) if key in params}
            # Native alone applies target normalize-empty fallback to this raw alias.
        anim_state_machine_variant_legacy = None
        anim_state_machine_variant_original = None
        if name == "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant":
            anim_state_machine_variant_original = copy.deepcopy(params)
            params = dict(params)
            # Original source aliases use raw IsEmpty before Native's final trim.
            for alias in ("source_blueprint_name", "blueprint_name"):
                candidate = params.pop(alias, "")
                if params.get("source_anim_bp_path", "") == "" and candidate != "":
                    if type(candidate) is not str:
                        raise OperationError("anim_state_machine_variant_legacy_path_unsupported", "Selected legacy source alias must be an exact string; no action executed")
                    params["source_anim_bp_path"] = candidate
            for public, alias in (("save_asset", "save"), ("replace_existing_root_link", "replace_existing")):
                if public not in params and alias in params:
                    if type(params[alias]) is not bool:
                        raise OperationError("anim_state_machine_variant_legacy_bool_unsupported", "Selected legacy boolean alias must be exact; no action executed")
                    params[public] = params[alias]
                params.pop(alias, None)
            # Missing/null/empty/raw states and transitions, entry key presence,
            # target normalization, graph selectors and shorthand stay Native-owned.
            anim_state_machine_variant_legacy = {key: params[key] for key in ("graph_name", "graph_type", "target_blueprint_path", "entry_state", "replace_existing_sample_machine", "state_name", "animation_asset", "position", "always_reset_on_entry", "player_inputs", "asset_player_inputs", "blendspace_inputs", "states", "transitions") if key in params}
            params = {key: value for key, value in params.items() if key not in anim_state_machine_variant_legacy}
        linked_anim_graph_legacy = None
        linked_anim_graph_original = None
        if name == "CubelessLinkedAnimGraphVariantTools.ensure_linked_anim_graph_sample_variant":
            linked_anim_graph_original = copy.deepcopy(params)
            params = dict(params)
            # Source fallback tests raw IsEmpty before any path normalization.
            for alias in ("source_blueprint_name", "blueprint_name"):
                candidate = params.pop(alias, "")
                if params.get("source_anim_bp_path", "") == "" and candidate != "":
                    if type(candidate) is not str:
                        raise OperationError("linked_anim_graph_variant_legacy_path_unsupported", "Legacy source fallback must be an exact string; no action executed")
                    params["source_anim_bp_path"] = candidate
            # Presence wins over these aliases even for false canonical values.
            for public, alias in (("replace_existing_links", "replace_existing_sample_graph"), ("require_input_pose_pin", "require_linked_input_pose"), ("save_asset", "save")):
                if public not in params and alias in params:
                    if type(params[alias]) is not bool:
                        raise OperationError("linked_anim_graph_variant_legacy_bool_unsupported", "Legacy selected boolean alias must be exact; no action executed")
                    params[public] = params[alias]
                params.pop(alias, None)
            # Target fallback must follow the Engine path normalizer, not Python truthiness.
            # Keep graph values and unsupported Layer key presence for this one Native operation.
            linked_anim_graph_legacy = {key: params[key] for key in ("graph_name", "graph_type", "anim_layer_interface_path", "layer_name", "linked_layer_instance_bp_path", "target_blueprint_path") if key in params}
            params = {key: value for key, value in params.items() if key not in linked_anim_graph_legacy}
        blendspace_variant_legacy = None
        blendspace_variant_original = None
        blendspace_grid_legacy = None
        blendspace_grid_original = None
        if name == "CubelessBlendSpaceVariantTools.ensure_blendspace_sample_variant":
            blendspace_variant_original = copy.deepcopy(params)
            params = dict(params)
            for public, alias in (("source_blendspace", "source_blendspace_path"), ("target_blendspace", "target_blendspace_path")):
                alias_value = params.pop(alias, "")
                if params.get(public, "") == "" and alias_value != "":
                    if type(alias_value) is not str:
                        raise OperationError("blendspace_variant_legacy_path_unsupported", "Legacy fallback path must be an exact string; no action executed")
                    params[public] = alias_value
            alias_resample = params.pop("resample", False)
            if type(alias_resample) is not bool and params.get("force_resample", False) is not True:
                raise OperationError("blendspace_variant_legacy_bool_unsupported", "Legacy resample must be an exact boolean; no action executed")
            if type(params.get("force_resample", False)) is bool:
                params["force_resample"] = params.get("force_resample", False) or alias_resample
            blendspace_variant_legacy = {key: params[key] for key in ("axis_edits", "sample_edits", "add_samples") if key in params}
            params = {key: value for key, value in params.items() if key not in blendspace_variant_legacy}
        if name == "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid":
            blendspace_grid_original = copy.deepcopy(params)
            params = dict(params)
            alias_path = params.pop("blendspace", "")
            if params.get("blendspace_path", "") == "" and alias_path != "":
                if type(alias_path) is not str:
                    raise OperationError("blendspace_pose_grid_legacy_path_unsupported", "Legacy BlendSpace fallback must be an exact string; no action executed")
                params["blendspace_path"] = alias_path
            alias_missing = params.pop("allow_missing_samples", False)
            if type(alias_missing) is not bool and params.get("allow_missing_bones", False) is not True:
                raise OperationError("blendspace_pose_grid_legacy_bool_unsupported", "Legacy allow_missing_samples must be an exact boolean; no action executed")
            if type(params.get("allow_missing_bones", False)) is bool:
                params["allow_missing_bones"] = params.get("allow_missing_bones", False) or alias_missing
            blendspace_grid_legacy = {key: params[key] for key in ("blendspaces", "samples", "sample_bones", "sample_sockets", "bones", "sockets", "blendspace_name") if key in params}
            params = {key: value for key, value in params.items() if key not in blendspace_grid_legacy}
        skeletal_sampling_legacy = None
        skeletal_sampling_original = None
        if name == "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie":
            # Preserve descriptor/nonarray/null/empty/ignored-row fallback inputs for this original command only.
            skeletal_sampling_original = copy.deepcopy(params)
            skeletal_sampling_legacy = {key: params[key] for key in ("bones", "sockets") if key in params}
            params = {key: value for key, value in params.items() if key not in skeletal_sampling_legacy}
        postprocess_variant_legacy = None
        postprocess_variant_original = None
        if name == "CubelessPostProcessAnimDemoVariantTools.ensure_postprocess_anim_demo_variant":
            # Raw explicit empty/null/rotation presence differs from public Named omission/defaults.
            postprocess_variant_original = copy.deepcopy(params)
            postprocess_variant_legacy = {key: params[key] for key in ("target_blueprint_name", "target_skeletal_mesh", "graph_name", "graph_type", "rotation") if key in params}
            params = {key: value for key, value in params.items() if key not in postprocess_variant_legacy}
        runtime_response_legacy = None
        runtime_response_original = None
        if name == "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response":
            # Preserve original key presence, object+array sources, duplicate rows and ignored-case fallback.
            runtime_response_original = copy.deepcopy(params)
            runtime_response_legacy = {key: params[key] for key in ("cases", "properties", "property_values", "property_name", "value", "curve_names", "anim_instance_source") if key in params}
            params = {key: value for key, value in params.items() if key not in runtime_response_legacy}
        runtime_probe_legacy = None
        runtime_probe_original = None
        if name == "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe":
            # Preserve original object+array selection, duplicate rows and key presence in this specific Native command.
            runtime_probe_original = copy.deepcopy(params)
            runtime_probe_legacy = {key: params[key] for key in ("properties", "property_values", "curve_names", "anim_instance_source") if key in params}
            params = {key: value for key, value in params.items() if key not in runtime_probe_legacy}
            if "property_name" in params and "value" not in params:
                # Raw command omits the single assignment; public Named25 still passes its explicit default None.
                params["property_name"] = ""
        if name == "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults":
            params = _rigvm_pin_default_writer_legacy_params(params)
        if name == "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links":
            params = _rigvm_link_writer_legacy_params(params)
        if name == "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults":
            params = _rigvm_topology_legacy_paths(params, "rigvm_pin_default_validation_legacy_scalar_path_unsupported")
        if name == "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links":
            params = _rigvm_link_validation_legacy_params(params)
        if name == "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology":
            params = _rigvm_topology_legacy_paths(params)
        unexpected = set(params) - set(fields)
        if unexpected:
            raise OperationError("batch_unknown_parameter", f"commands[{index}]: unsupported parameters {sorted(unexpected)}; child operation_id is forbidden")
        values = {}
        for key, (types, default) in fields.items():
            value = params.get(key, default)
            if value is _REQUIRED:
                raise OperationError("batch_missing_parameter", f"commands[{index}].params.{key} is required")
            if type(value) not in types:
                raise OperationError("batch_invalid_parameter_type", f"commands[{index}].params.{key} has the wrong type")
            if name == "CubelessComponentTools.set_static_mesh_properties" and key == "material" and value is None:
                # Keep pre-R mesh-only batch fingerprints unchanged. Explicit
                # null and omission both mean no material assignment.
                continue
            if name == "CubelessBlueprintTools.add_blueprint_variable" and key == "is_transient" and value is False:
                # Additive AB option must preserve pre-AB default batch replay.
                continue
            if name == "CubelessBlueprintGraphTools.add_blueprint_loop_node" and key in ("element_type", "type_object", "enum_type") and value is None:
                # Preserve pre-AO normalized ForLoop batch replay fingerprints.
                continue
            if name in ("CubelessBlueprintGraphTools.add_blueprint_variable_get_node", "CubelessBlueprintGraphTools.add_blueprint_variable_set_node"):
                # AA additive options must not invalidate pre-AA batch replay.
                if key in ("target_class", "graph_id", "graph_type") and value in (None, ""):
                    continue
                if key == "node_position_json" and value is None or key == "create_graph_if_missing" and value is False:
                    continue
            # Cast numeric-only fields; unrestricted JSON values retain their types.
            if (name == "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver" and key == "is_accelerating_threshold"
                    or name == "CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_curve_demo" and key == "alpha"
                    or name == "CubelessAnimGraphAuthoringTools.ensure_anim_graph_trail_demo" and key in ("alpha", "relaxation_speed_scale", "stretch_limit", "max_delta_time")
                    or name == "CubelessAnimGraphAuthoringTools.ensure_anim_graph_secondary_motion_chain" and key in ("alpha", "angular_limit_degrees", "linear_damping", "angular_damping", "kawaii_damping", "kawaii_stiffness")
                    or name == "CubelessAnimGraphAuthoringTools.ensure_controlrig_forced_driver_animbp" and key == "alpha"):
                # Preserve original public numbers; Native owns float32 and original field rules.
                try:
                    if not math.isfinite(value):
                        raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite")
                except OverflowError as error:
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite") from error
            elif float in types and all(kind in (int, float) for kind in types):
                try:
                    value = float(value)
                except OverflowError as error:
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite") from error
                if not math.isfinite(value):
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must be finite")
            elif ((name == "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology" and key in ("max_nodes", "max_pins_per_node", "max_sub_pins_per_pin", "max_links"))
                    or (name == "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links" and key == "max_links")
                    or (name == "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults" and key == "max_pins")
                    or (name == "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links" and key == "max_links")
                    or (name == "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults" and key == "max_pins")
                    or (name == "CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state" and key in ("max_state_machines", "max_state_machine_instances_to_probe", "max_states_per_machine", "max_transitions_per_machine", "max_curves"))
                    or (name == "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe" and key in ("tick_count", "max_transitions_per_machine"))
                    or (name == "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response" and key in ("tick_count", "max_state_machines", "max_state_machine_instances_to_probe", "max_states_per_machine", "max_transitions_per_machine", "max_curves"))
                    or (name == "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant" and key in ("max_states", "max_transitions"))
                    or (name == "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid" and key in ("settle_tick_count", "tick_count"))
                    or (name == "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose" and key in ("tick_count", "settle_tick_count", "pose_link_max_depth", "input_pose_index", "max_depth"))
                    or (name == "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function" and key == "expected_node_count")
                    or (name == "CubelessFoliageInstanceTools.remove_foliage_instances" and key == "max_remove_count")
                    or (name == "CubelessLandscapeTools.create_landscape" and key in ("component_count_x", "component_count_y", "sections_per_component", "quads_per_section", "height_value"))):
                # This finite Native API takes int64 directly, without a Python reflected int32 facade.
                if not -(2 ** 63) <= value < 2 ** 63:
                    raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must fit the Native int64")
            elif (name not in ("CubelessNiagaraResourceBindingTools.set_niagara_render_target2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_texture2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_scratch_pad_function_input_default", "CubelessNiagaraResourceBindingTools.link_niagara_scratch_pad_pin_to_user_parameter", "CubelessNiagaraFactoryBootstrapTools.inspect_niagara_factory_bootstrap_defaults", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_system", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_emitter", "CubelessNiagaraFactoryBootstrapTools.create_basic_sprite_niagara_system", "CubelessViewportCaptureTools.list_viewport_bookmarks", "CubelessViewportCaptureTools.set_active_viewport_realtime", "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot", "CubelessViewportCaptureTools.take_screenshot", 'CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path', 'CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter', 'CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter', 'CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters', 'CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter', 'CubelessNiagaraInputParameterTools.set_niagara_module_static_switch', 'CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs', 'CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides', 'CubelessNiagaraInputParameterTools.set_niagara_module_input_value', 'CubelessNiagaraInputParameterTools.create_niagara_module_input_override', 'CubelessNiagaraCleanupTools.remove_niagara_modules', 'CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_user_parameters', 'CubelessNiagaraCleanupTools.remove_niagara_simulation_stage', 'CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script', 'CubelessPCGInspectionTools.resolve_pcg_graph', 'CubelessPCGInspectionTools.list_pcg_graph_nodes', 'CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes', 'CubelessPCGInspectionTools.inspect_pcg_component_outputs', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status', 'CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection', 'CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root', 'CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids', 'CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter', 'CubelessNiagaraRendererTools.inspect_niagara_renderers', 'CubelessNiagaraRendererTools.set_niagara_renderer_material', 'CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings', 'CubelessNiagaraRendererTools.add_niagara_sprite_renderer', 'CubelessNiagaraRendererTools.add_niagara_mesh_renderer', 'CubelessNiagaraRendererTools.set_niagara_renderer_enabled', 'CubelessNiagaraRendererTools.cleanup_niagara_renderers', 'CubelessContentReconstructionTools.recreate_content_folder_mcp', 'CubelessContentReconstructionTools.postprocess_content_folder_mcp', 'CubelessContentReconstructionTools.repair_world_actor_instances_mcp', 'CubelessContentReconstructionTools.run_content_validation_pipeline_mcp', 'CubelessGASAuthoringTools.inspect_gas_blueprint', 'CubelessGASAuthoringTools.create_gameplay_ability_blueprint', 'CubelessGASAuthoringTools.create_gameplay_effect_blueprint', 'CubelessGASAuthoringTools.create_attribute_set_blueprint', 'CubelessGASAuthoringTools.add_ability_system_component_to_blueprint', 'CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set', 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke', 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture', 'CubelessChaosLearningTools.run_chaos_cache_smoke', 'CubelessChaosTraceStrainTools.trigger_trace_external_strain', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state', 'CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh', 'CubelessChaosGeometryAuthoringTools.configure_building_fracture', 'CubelessChaosGeometryAuthoringTools.configure_anchor_field', 'CubelessChaosCacheSessionTools.record_chaos_cache', 'CubelessChaosCacheSessionTools.validate_chaos_cache', 'CubelessChaosCinematicSequenceTools.create_chaos_level_sequence', 'CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence', 'CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing', 'CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline')
                    and all(kind in (int, type(None)) for kind in types)
                    and type(value) is int and not -(2 ** 31) <= value < 2 ** 31):
                raise OperationError("batch_parameter_out_of_range", f"commands[{index}].params.{key} must fit a reflected int32")
            if name == "CubelessBlueprintGraphTools.add_blueprint_array_function_node" and key in ("type_object", "enum_type") and value is None:
                # Match nullable individual inputs while retaining the previous
                # normalized empty-string request and durable replay identity.
                value = ""
            values[key] = value
        entry = {"type": name, "params": values}
        if actor_original_params is not None:
            entry["legacy_original_params"] = actor_original_params
        if chaos_solver_settings_original is not None:
            entry["legacy_original_params"] = chaos_solver_settings_original
            entry["legacy_raw_native_entry"] = chaos_solver_settings_raw_method is not None
            values["_public_input"] = chaos_solver_settings_raw_method is None
        if chaos_solver_settings_legacy is not None:
            values["_legacy_command_params"] = chaos_solver_settings_legacy
        if chaos_cloth_component_original is not None:
            entry["legacy_original_params"] = chaos_cloth_component_original
            entry["legacy_raw_native_entry"] = chaos_cloth_component_raw_method is not None
            values["_public_input"] = chaos_cloth_component_raw_method is None
        if chaos_cloth_component_legacy is not None:
            values["_legacy_command_params"] = chaos_cloth_component_legacy
        if chaos_cloth_authoring_original is not None:
            entry["legacy_original_params"] = chaos_cloth_authoring_original
            entry["legacy_raw_native_entry"] = chaos_cloth_authoring_raw_method is not None
            values["_public_input"] = chaos_cloth_authoring_raw_method is None
        if chaos_cloth_authoring_legacy is not None:
            values["_legacy_command_params"] = chaos_cloth_authoring_legacy
        if legacy_chaos_clothing_original is not None:
            entry["legacy_original_params"] = legacy_chaos_clothing_original
            entry["legacy_raw_native_entry"] = legacy_chaos_clothing_raw_method is not None
            values["_public_input"] = legacy_chaos_clothing_raw_method is None
        if legacy_chaos_clothing_legacy is not None:
            values["_legacy_command_params"] = legacy_chaos_clothing_legacy
        if chaos_cloth_inspection_original is not None:
            entry["legacy_original_params"] = chaos_cloth_inspection_original
            entry["legacy_raw_native_entry"] = chaos_cloth_inspection_raw_method is not None
            values["_public_input"] = chaos_cloth_inspection_raw_method is None
        if chaos_cloth_inspection_legacy is not None:
            values["_legacy_command_params"] = chaos_cloth_inspection_legacy
        if actor_property_focus_original is not None:
            entry["legacy_original_params"] = actor_property_focus_original
            entry["legacy_raw_native_entry"] = actor_property_focus_raw_method is not None
            values["_public_input"] = actor_property_focus_raw_method is None
        if actor_property_focus_legacy is not None:
            values["_legacy_command_params"] = actor_property_focus_legacy
        if editor_level_session_original is not None:
            entry["legacy_original_params"] = editor_level_session_original
            entry["legacy_raw_native_entry"] = editor_level_session_raw_method is not None
            values["_public_input"] = editor_level_session_raw_method is None
        if editor_level_session_legacy is not None:
            values["_legacy_command_params"] = editor_level_session_legacy
        if viewport_capture_original is not None:
            entry["legacy_original_params"] = viewport_capture_original
            entry["legacy_raw_native_entry"] = viewport_capture_raw_method is not None
            values["_public_input"] = viewport_capture_raw_method is None
        if viewport_capture_legacy is not None:
            values["_legacy_command_params"] = viewport_capture_legacy
        if chaos_cinematic_sequence_original is not None:
            entry["legacy_original_params"] = chaos_cinematic_sequence_original
            entry["legacy_raw_native_entry"] = chaos_cinematic_sequence_raw_method is not None
            values["_public_input"] = chaos_cinematic_sequence_raw_method is None
        if chaos_cinematic_sequence_legacy is not None:
            values["_legacy_command_params"] = chaos_cinematic_sequence_legacy
        if chaos_cache_sessions_original is not None:
            entry["legacy_original_params"] = chaos_cache_sessions_original
            entry["legacy_raw_native_entry"] = chaos_cache_sessions_raw_method is not None
            values["_public_input"] = chaos_cache_sessions_raw_method is None
        if chaos_cache_sessions_legacy is not None:
            values["_legacy_command_params"] = chaos_cache_sessions_legacy
        if chaos_destruction_performance_original is not None:
            entry["legacy_original_params"] = chaos_destruction_performance_original
            entry["legacy_raw_native_entry"] = chaos_destruction_performance_raw_method is not None
            values["_public_input"] = chaos_destruction_performance_raw_method is None
        if chaos_destruction_performance_legacy is not None:
            values["_legacy_command_params"] = chaos_destruction_performance_legacy
        if chaos_learning_original is not None:
            entry["legacy_original_params"] = chaos_learning_original
            entry["legacy_raw_native_entry"] = chaos_learning_raw_method is not None
            values["_public_input"] = chaos_learning_raw_method is None
        if chaos_learning_legacy is not None:
            values["_legacy_command_params"] = chaos_learning_legacy
        if chaos_trace_strain_original is not None:
            entry["legacy_original_params"] = chaos_trace_strain_original
            entry["legacy_raw_native_entry"] = chaos_trace_strain_raw_method is not None
            values["_public_input"] = chaos_trace_strain_raw_method is None
        if chaos_trace_strain_legacy is not None:
            values["_legacy_command_params"] = chaos_trace_strain_legacy
        if chaos_geometry_authoring_original is not None:
            entry["legacy_original_params"] = chaos_geometry_authoring_original
            entry["legacy_raw_native_entry"] = chaos_geometry_authoring_raw_method is not None
            values["_public_input"] = chaos_geometry_authoring_raw_method is None
        if chaos_geometry_authoring_legacy is not None:
            values["_legacy_command_params"] = chaos_geometry_authoring_legacy
        if gas_authoring_original is not None:
            entry["legacy_original_params"] = gas_authoring_original
            entry["legacy_raw_native_entry"] = gas_authoring_raw_method is not None
            values["_public_input"] = gas_authoring_raw_method is None
        if gas_authoring_legacy is not None:
            values["_legacy_command_params"] = gas_authoring_legacy
        if content_reconstruction_original is not None:
            entry["legacy_original_params"] = content_reconstruction_original
            entry["legacy_raw_native_entry"] = content_reconstruction_raw_method is not None
            values["_public_input"] = content_reconstruction_raw_method is None
        if content_reconstruction_legacy is not None:
            values["_legacy_command_params"] = content_reconstruction_legacy
        if niagara_renderers_original is not None:
            entry["legacy_original_params"] = niagara_renderers_original
            entry["legacy_raw_native_entry"] = niagara_renderers_raw_method is not None
            values["_public_input"] = niagara_renderers_raw_method is None
        if niagara_renderers_legacy is not None:
            values["_legacy_command_params"] = niagara_renderers_legacy
        if niagara_emitter_configuration_original is not None:
            entry["legacy_original_params"] = niagara_emitter_configuration_original
            entry["legacy_raw_native_entry"] = niagara_emitter_configuration_raw_method is not None
            values["_public_input"] = niagara_emitter_configuration_raw_method is None
        if niagara_emitter_configuration_legacy is not None:
            values["_legacy_command_params"] = niagara_emitter_configuration_legacy
        if niagara_parameter_collections_original is not None:
            entry["legacy_original_params"] = niagara_parameter_collections_original
            entry["legacy_raw_native_entry"] = niagara_parameter_collections_raw_method is not None
            values["_public_input"] = niagara_parameter_collections_raw_method is None
        if niagara_parameter_collections_legacy is not None:
            values["_legacy_command_params"] = niagara_parameter_collections_legacy
        if niagara_authoring_inspection_original is not None:
            entry["legacy_original_params"] = niagara_authoring_inspection_original
            entry["legacy_raw_native_entry"] = niagara_authoring_inspection_raw_method is not None
            values["_public_input"] = niagara_authoring_inspection_raw_method is None
        if niagara_authoring_inspection_legacy is not None:
            values["_legacy_command_params"] = niagara_authoring_inspection_legacy
        if pcg_inspection_original is not None:
            entry["legacy_original_params"] = pcg_inspection_original
            entry["legacy_raw_native_entry"] = pcg_inspection_raw_method is not None
            values["_public_input"] = pcg_inspection_raw_method is None
        if pcg_inspection_legacy is not None:
            values["_legacy_command_params"] = pcg_inspection_legacy

        if niagara_factory_bootstrap_original is not None:
            entry["legacy_original_params"] = niagara_factory_bootstrap_original
            entry["legacy_raw_native_entry"] = niagara_factory_bootstrap_raw_method is not None
            values["_public_input"] = niagara_factory_bootstrap_raw_method is None
        if niagara_factory_bootstrap_legacy is not None:
            values["_legacy_command_params"] = niagara_factory_bootstrap_legacy

        if niagara_resource_bindings_original is not None:
            entry["legacy_original_params"] = niagara_resource_bindings_original
            entry["legacy_raw_native_entry"] = niagara_resource_bindings_raw_method is not None
            values["_public_input"] = niagara_resource_bindings_raw_method is None
        if niagara_scratch_stack_authoring_original is not None:
            entry["legacy_original_params"] = niagara_scratch_stack_authoring_original
            entry["legacy_raw_native_entry"] = niagara_scratch_stack_authoring_raw_method is not None
            values["_public_input"] = niagara_scratch_stack_authoring_raw_method is None
        if niagara_event_stage_authoring_original is not None:
            entry["legacy_original_params"] = niagara_event_stage_authoring_original
            entry["legacy_raw_native_entry"] = niagara_event_stage_authoring_raw_method is not None
            values["_public_input"] = niagara_event_stage_authoring_raw_method is None
        if niagara_promotion_original is not None:
            entry["legacy_original_params"] = niagara_promotion_original
            entry["legacy_raw_native_entry"] = niagara_promotion_raw_method is not None
            values["_public_input"] = niagara_promotion_raw_method is None
        if niagara_sprite_preset_authoring_original is not None:
            entry["legacy_original_params"] = niagara_sprite_preset_authoring_original
            entry["legacy_raw_native_entry"] = niagara_sprite_preset_authoring_raw_method is not None
            values["_public_input"] = niagara_sprite_preset_authoring_raw_method is None
        if niagara_custom_hlsl_conversion_original is not None:
            entry["legacy_original_params"] = niagara_custom_hlsl_conversion_original
            entry["legacy_raw_native_entry"] = niagara_custom_hlsl_conversion_raw_method is not None
            values["_public_input"] = niagara_custom_hlsl_conversion_raw_method is None
        if niagara_preview_lab_original is not None:
            entry["legacy_original_params"] = niagara_preview_lab_original
            entry["legacy_raw_native_entry"] = niagara_preview_lab_raw_method is not None
            values["_public_input"] = niagara_preview_lab_raw_method is None
        if controlrig_recipe_authoring_original is not None:
            entry["legacy_original_params"] = controlrig_recipe_authoring_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = controlrig_recipe_authoring_raw_method is None
        if texture_material_authoring_original is not None:
            entry["legacy_original_params"] = texture_material_authoring_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = texture_material_authoring_raw_method is None
        if blueprint_resolution_connection_original is not None:
            entry["legacy_original_params"] = blueprint_resolution_connection_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = blueprint_resolution_connection_raw_method is None
        if niagara_scratch_user_recipes_original is not None:
            entry["legacy_original_params"] = niagara_scratch_user_recipes_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = niagara_scratch_user_recipes_raw_method is None
        if blueprint_property_graph_original is not None:
            entry["legacy_original_params"] = blueprint_property_graph_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = blueprint_property_graph_raw_method is None
        if controlrig_recipe_planning_original is not None:
            entry["legacy_original_params"] = controlrig_recipe_planning_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = controlrig_recipe_planning_raw_method is None
        if pcg_utilities_original is not None:
            entry["legacy_original_params"] = pcg_utilities_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = pcg_utilities_raw_method is None
        if pcg_contract_authoring_original is not None:
            entry["legacy_original_params"] = pcg_contract_authoring_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = pcg_contract_authoring_raw_method is None
        if pcg_component_generation_original is not None:
            entry["legacy_original_params"] = pcg_component_generation_original
            entry["legacy_raw_native_entry"] = pcg_component_generation_raw_method is not None
            values["_public_input"] = pcg_component_generation_raw_method is None
        if niagara_promotion_manifest_original is not None:
            entry["legacy_original_params"] = niagara_promotion_manifest_original
            entry["legacy_raw_native_entry"] = False
            values["_public_input"] = niagara_promotion_manifest_raw_method is None
        if niagara_system_analysis_original is not None:
            entry["legacy_original_params"] = niagara_system_analysis_original
            entry["legacy_raw_native_entry"] = niagara_system_analysis_raw_method is not None
            values["_public_input"] = niagara_system_analysis_raw_method is None
        if pie_input_original is not None:
            entry["legacy_original_params"] = pie_input_original
            entry["legacy_raw_native_entry"] = pie_input_raw_method is not None
            values["_public_input"] = pie_input_raw_method is None
        if niagara_preview_workflows_original is not None:
            entry["legacy_original_params"] = niagara_preview_workflows_original
            entry["legacy_raw_native_entry"] = niagara_preview_workflows_raw_method is not None
            values["_public_input"] = niagara_preview_workflows_raw_method is None
        if blueprint_niagara_template_swap_original is not None:
            entry["legacy_original_params"] = blueprint_niagara_template_swap_original
            entry["legacy_raw_native_entry"] = blueprint_niagara_template_swap_raw_method is not None
            values["_public_input"] = blueprint_niagara_template_swap_raw_method is None
        if pcg_graph_authoring_original is not None:
            entry["legacy_original_params"] = pcg_graph_authoring_original
            entry["legacy_raw_native_entry"] = pcg_graph_authoring_raw_method is not None
            values["_public_input"] = pcg_graph_authoring_raw_method is None
        if niagara_resource_bindings_legacy is not None:
            values["_legacy_command_params"] = niagara_resource_bindings_legacy
        if niagara_scratch_stack_authoring_legacy is not None:
            values["_legacy_command_params"] = niagara_scratch_stack_authoring_legacy
        if niagara_event_stage_authoring_legacy is not None:
            values["_legacy_command_params"] = niagara_event_stage_authoring_legacy
        if niagara_promotion_legacy is not None:
            values["_legacy_command_params"] = niagara_promotion_legacy
        if niagara_sprite_preset_authoring_legacy is not None:
            values["_legacy_command_params"] = niagara_sprite_preset_authoring_legacy
        if niagara_custom_hlsl_conversion_legacy is not None:
            values["_legacy_command_params"] = niagara_custom_hlsl_conversion_legacy
        if niagara_preview_lab_legacy is not None:
            values["_legacy_command_params"] = niagara_preview_lab_legacy
        if controlrig_recipe_authoring_legacy is not None:
            values["_legacy_command_params"] = controlrig_recipe_authoring_legacy
        if texture_material_authoring_legacy is not None:
            values["_legacy_command_params"] = texture_material_authoring_legacy
        if blueprint_resolution_connection_legacy is not None:
            values["_legacy_command_params"] = blueprint_resolution_connection_legacy
        if niagara_scratch_user_recipes_legacy is not None:
            values["_legacy_command_params"] = niagara_scratch_user_recipes_legacy
        if blueprint_property_graph_legacy is not None:
            values["_legacy_command_params"] = blueprint_property_graph_legacy
        if controlrig_recipe_planning_legacy is not None:
            values["_legacy_command_params"] = controlrig_recipe_planning_legacy
        if pcg_utilities_legacy is not None:
            values["_legacy_command_params"] = pcg_utilities_legacy
        if pcg_contract_authoring_legacy is not None:
            values["_legacy_command_params"] = pcg_contract_authoring_legacy
        if pcg_component_generation_legacy is not None:
            values["_legacy_command_params"] = pcg_component_generation_legacy
        if niagara_promotion_manifest_legacy is not None:
            values["_legacy_command_params"] = niagara_promotion_manifest_legacy
        if niagara_system_analysis_legacy is not None:
            values["_legacy_command_params"] = niagara_system_analysis_legacy
        if pie_input_legacy is not None:
            values["_legacy_command_params"] = pie_input_legacy
        if niagara_preview_workflows_legacy is not None:
            values["_legacy_command_params"] = niagara_preview_workflows_legacy
        if blueprint_niagara_template_swap_legacy is not None:
            values["_legacy_command_params"] = blueprint_niagara_template_swap_legacy
        if pcg_graph_authoring_legacy is not None:
            values["_legacy_command_params"] = pcg_graph_authoring_legacy
        if niagara_runtime_inspection_original is not None:
            entry["legacy_original_params"] = niagara_runtime_inspection_original
            entry["legacy_raw_native_entry"] = niagara_runtime_inspection_raw_method is not None
            values["_public_input"] = niagara_runtime_inspection_raw_method is None
        if niagara_runtime_inspection_legacy is not None:
            values["_legacy_command_params"] = niagara_runtime_inspection_legacy
        if niagara_cleanup_original is not None:
            entry["legacy_original_params"] = niagara_cleanup_original
            entry["legacy_raw_native_entry"] = niagara_cleanup_raw_method is not None
            values["_public_input"] = niagara_cleanup_raw_method is None
        if niagara_cleanup_legacy is not None:
            values["_legacy_command_params"] = niagara_cleanup_legacy
        if niagara_input_parameters_original is not None:
            entry["legacy_original_params"] = niagara_input_parameters_original
            entry["legacy_raw_native_entry"] = niagara_input_parameters_raw_method is not None
            values["_public_input"] = niagara_input_parameters_raw_method is None
        if niagara_input_parameters_legacy is not None:
            values["_legacy_command_params"] = niagara_input_parameters_legacy
        if niagara_module_input_paths_original is not None:
            entry["legacy_original_params"] = niagara_module_input_paths_original
            entry["legacy_raw_native_entry"] = niagara_module_input_paths_raw_method is not None
            values["_public_input"] = niagara_module_input_paths_raw_method is None
        if niagara_module_input_paths_legacy is not None:
            values["_legacy_command_params"] = niagara_module_input_paths_legacy
        if niagara_data_channel_authoring_original is not None:
            entry["legacy_original_params"] = niagara_data_channel_authoring_original
            entry["legacy_raw_native_entry"] = niagara_data_channel_authoring_raw_method is not None
            values["_public_input"] = niagara_data_channel_authoring_raw_method is None
        if niagara_data_channel_authoring_legacy is not None:
            values["_legacy_command_params"] = niagara_data_channel_authoring_legacy
        if geometry_collection_inspection_original is not None:
            entry["legacy_original_params"] = geometry_collection_inspection_original
            entry["legacy_raw_native_entry"] = geometry_collection_inspection_raw_method is not None
            values["_public_input"] = geometry_collection_inspection_raw_method is None
        if geometry_collection_inspection_legacy is not None:
            values["_legacy_command_params"] = geometry_collection_inspection_legacy
        if landscape_original is not None:
            entry["legacy_original_params"] = landscape_original
            entry["legacy_raw_native_entry"] = landscape_raw_method is not None
        if landscape_legacy is not None:
            values["_legacy_command_params"] = landscape_legacy
        if foliage_instances_original is not None:
            entry["legacy_original_params"] = foliage_instances_original
            entry["legacy_raw_native_entry"] = foliage_instances_raw_method is not None
        if foliage_instances_legacy is not None:
            values["_legacy_command_params"] = foliage_instances_legacy
        if spline_component_points_original is not None:
            entry["legacy_original_params"] = spline_component_points_original
            entry["legacy_raw_native_entry"] = spline_component_points_raw_entry
        if spline_component_points_legacy is not None:
            values["_legacy_command_params"] = spline_component_points_legacy
        if blueprint_function_extraction_legacy is not None:
            values["_legacy_command_params"] = blueprint_function_extraction_legacy
            entry["legacy_original_params"] = blueprint_function_extraction_original
        if anim_node_pre_post_runtime_pose_legacy is not None:
            values["_legacy_command_params"] = anim_node_pre_post_runtime_pose_legacy
            entry["legacy_original_params"] = anim_node_pre_post_runtime_pose_original
        if anim_bp_locomotion_driver_legacy is not None:
            values["_legacy_command_params"] = anim_bp_locomotion_driver_legacy
            entry["legacy_original_params"] = anim_bp_locomotion_driver_original
        if anim_state_machine_variant_legacy is not None:
            values["_legacy_command_params"] = anim_state_machine_variant_legacy
            entry["legacy_original_params"] = anim_state_machine_variant_original
        if linked_anim_graph_legacy is not None:
            values["_legacy_command_params"] = linked_anim_graph_legacy
            entry["legacy_original_params"] = linked_anim_graph_original
        if blendspace_variant_legacy is not None:
            values["_legacy_command_params"] = blendspace_variant_legacy
            entry["legacy_original_params"] = blendspace_variant_original
        if blendspace_grid_legacy is not None:
            values["_legacy_command_params"] = blendspace_grid_legacy
            entry["legacy_original_params"] = blendspace_grid_original
        if skeletal_sampling_legacy is not None:
            values["_legacy_command_params"] = skeletal_sampling_legacy
            entry["legacy_original_params"] = skeletal_sampling_original
        if postprocess_variant_legacy is not None:
            values["_legacy_command_params"] = postprocess_variant_legacy
            entry["legacy_original_params"] = postprocess_variant_original
        if runtime_response_legacy is not None:
            values["_legacy_command_params"] = runtime_response_legacy
            entry["legacy_original_params"] = runtime_response_original
        if runtime_probe_legacy is not None:
            values["_legacy_command_params"] = runtime_probe_legacy
            entry["legacy_original_params"] = runtime_probe_original
        normalized.append(entry)
    return normalized


def _command_effect(command):
    if command["type"] == "CubelessPCGTools.promote_pcg_temp_graph" and command["params"]["dry_run"] is True:
        return "read_world"
    if command["type"] in ["CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable"]:
        params=command["params"];raw=params.get("_legacy_command_params")
        selected=raw if type(raw) is dict else params
        if selected.get("dry_run",True) is True:return "read_world"
    # Only FI has a mode-dependent asset path; keep every existing capability unchanged.
    if command["type"] == "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose":
        params = command["params"]
        if params["dry_run"] is False and params["mode"].lower() == "isolated_temp_components":
            return "write"
        return "runtime_write"
    if command["type"] == "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings":
        params = command["params"]
        raw = params.get("_legacy_command_params")
        selected = raw if type(raw) is dict else params
        if selected.get("dry_run", True) is True:
            return "read_world"
        context = selected.get("world_context", "editor")
        if type(context) is str and context.lower() in ("play", "pie", "sie", "auto", ""):
            try:
                ensure_editor_idle()
            except OperationError as error:
                if error.code == "play_session_active":
                    return "runtime_write"
                raise
    if command["type"] == "CubelessChaosClothComponentTools.configure_chaos_cloth_component":
        params = command["params"]
        raw = params.get("_legacy_command_params")
        selected = raw if type(raw) is dict else params
        if selected.get("dry_run", True) is True:
            return "read_world"
        context = selected.get("world_context", "auto")
        if type(context) is str and context.lower() in ("play", "pie", "sie", "auto", ""):
            try:
                ensure_editor_idle()
            except OperationError as error:
                if error.code == "play_session_active":
                    return "runtime_write"
                raise
    if command["type"] in ("CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map"):
        params = command["params"]
        raw = params.get("_legacy_command_params")
        selected = raw if type(raw) is dict else params
        if selected.get("dry_run", True) is True:
            return "read_world"
    if command["type"] == "CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections":
        params = command["params"]
        raw = params.get("_legacy_command_params")
        selected = raw if type(raw) is dict else params
        if selected.get("dry_run", True) is True:
            return "read_world"
    if command["type"] in ('CubelessChaosGeometryAuthoringTools.configure_building_fracture', 'CubelessChaosGeometryAuthoringTools.configure_anchor_field'):
        raw = command["params"].get("_legacy_command_params")
        if type(raw) is dict and raw.get("operation") == "inspect_saved":
            return "read_world"
    if command["type"] == 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture':
        params=command["params"];raw=params.get("_legacy_command_params")
        selected=raw if type(raw) is dict else params
        if selected.get("dry_run",True) is True:return "read_world"
    if command["type"] in ('CubelessContentReconstructionTools.recreate_content_folder_mcp', 'CubelessContentReconstructionTools.postprocess_content_folder_mcp', 'CubelessContentReconstructionTools.repair_world_actor_instances_mcp', 'CubelessContentReconstructionTools.run_content_validation_pipeline_mcp'):
        params=command["params"];raw=params.get("_legacy_command_params")
        selected=raw if type(raw) is dict else params
        if selected.get("dry_run",False) is True:return "read_world"
    if command["type"] == "CubelessNiagaraRendererTools.cleanup_niagara_renderers":
        params=command["params"];raw=params.get("_legacy_command_params")
        selected=raw if type(raw) is dict else params
        if selected.get("dry_run",True) is True:return "read_world"
    return _CAPABILITIES[command["type"]]["effect"]


def _summary(commands, stop_on_error):
    explicit_writes = [c for c in commands if _command_effect(c) in ("write", "runtime_write")]
    native_content_only = bool(explicit_writes) and all(c["type"] in (
        "CubelessContentReconstructionTools.recreate_content_folder_mcp",
        "CubelessContentReconstructionTools.postprocess_content_folder_mcp",
        "CubelessContentReconstructionTools.repair_world_actor_instances_mcp",
        "CubelessContentReconstructionTools.run_content_validation_pipeline_mcp") for c in explicit_writes)
    return {"manifest_version": MANIFEST_VERSION, "capability_extension": "local-scs-component-v1", "variable_capability_extension": "declared-variable-state-v1", "success": False, "total": len(commands),
            "executed": 0, "dispatched_count": 0, "succeeded": 0, "failed": 0,
            "stopped_at_index": -1, "stop_on_error": stop_on_error, "results": [],
            "not_run": list(range(len(commands))), "forced_stop": False, "stop_reason": "",
            "mutation_attempted": False, "mutation_performed": False,
            "mutation_state_unknown": False, "partial_mutation": False,
            "command_may_still_be_in_flight": False, "do_not_retry": False,
            "transaction": {"scope_requested": any(_command_effect(c) == "write" for c in commands),
                            "atomic_rollback": False, "group_undo_verified": False, "disk_save_rollback": False,
                            "native_owned_content_only": native_content_only, "batch_scope_entered": False},
            "limits": {"max_commands": MAX_COMMANDS, "input_bytes": MAX_INPUT_BYTES,
                       "observed_result_bytes_stop_threshold": MAX_OBSERVED_RESULT_BYTES,
                       "between_step_budget_seconds": BETWEEN_STEP_BUDGET_SECONDS,
                       "engine_call_preemption": False}, "checkpoint_sequence": 0}


def _chaos_cache_session_snapshot(row, data):
    if row["type"] not in ("CubelessChaosCacheSessionTools.record_chaos_cache", "CubelessChaosCacheSessionTools.validate_chaos_cache", 'CubelessChaosTraceStrainTools.trigger_trace_external_strain', 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture', 'CubelessChaosLearningTools.run_chaos_cache_smoke', 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke'):
        return
    # Preserve historical callback effects separately from this request's compile/save/mutation.
    for key in ("session_execution", "session_actual_saved_observed", "session_mutation_state_unknown", "session_in_flight", "session_operation_complete", "session_id_observed", "session_verification_passed"):
        if key in data:row[key] = copy.deepcopy(data[key])
    row["session_saved_history_promoted_to_current_request_save"] = False
    row["mutation_state_unknown"] = bool(row.get("mutation_state_unknown") or row.get("session_mutation_state_unknown") or row.get("session_in_flight"))
    row["command_may_still_be_in_flight"] = bool(row.get("command_may_still_be_in_flight") or row.get("session_in_flight"))


def _observation(row):
    if row["type"] in _ACTOR_BATCH_TOOLS:
        _actor_batch_observation(row)
        return
    child = row["response"]
    if row["type"] in ("execute_python", "CubelessPythonExecutionTools.execute_unreal_python"):
        data = child["data"]
        in_flight = data.get("command_may_still_be_in_flight") is True
        # Continuation known is distinct from unobserved arbitrary script effects.
        # ExecFalse may have changed state; do not infer writer0 or performed=True.
        row.update(status="success" if child["success"] else "error",
                   mutation_attempted=data.get("execution_mutation_attempted", False),
                   mutation_performed=data.get("mutation_performed", False),
                   mutation_state_unknown=bool(in_flight or data.get("mutation_state_unknown") is True
                       or data.get("change_state_known") is False),
                   command_may_still_be_in_flight=in_flight,
                   full_effect_state_known=False, error=child["error"], error_code=child["error_code"])
        return
    # Component helpers distinguish entering their guarded native operation
    # from actually editing an engine object (preflight rejection / no-op).
    if (row["type"] in ("CubelessBlueprintGraphTools.list_blueprint_nodes","CubelessBlueprintGraphTools.delete_blueprint_node","CubelessPCGTools.pcg_actor_smoke_test","CubelessNiagaraPromotionTools.create_niagara_system_from_source_template","CubelessPCGTools.validate_pcg_source_independence","CubelessPCGTools.promote_pcg_temp_graph","CubelessNiagaraScratchUserRecipeTools.create_niagara_visible_scratch_pad_snow_seed", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_vec2_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_color_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_visible_toggle_module", "CubelessBlueprintResolutionConnectionTools.resolve_blueprint","CubelessBlueprintResolutionConnectionTools.connect_blueprint_nodes", "CubelessBlueprintPropertyGraphTools.set_blueprint_property","CubelessBlueprintPropertyGraphTools.list_blueprint_graphs", "CubelessTextureMaterialTools.generate_texture_from_prompt","CubelessTextureMaterialTools.generate_texture_for_mesh_uv","CubelessTextureMaterialTools.generate_and_apply_ai_texture","CubelessNiagaraCleanupTools.cleanup_generated_niagara_assets_offline","CubelessTextureMaterialTools.get_static_mesh_uv_layout","CubelessTextureMaterialTools.create_material_instance_with_texture","CubelessTextureMaterialTools.create_material_with_texture_set", "CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe","CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe","CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe", "CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton","CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe","CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe", "CubelessPCGUtilityTools.list_pcg_assets","CubelessPCGUtilityTools.list_pcg_components","CubelessPCGUtilityTools.set_pcg_debug_enabled","CubelessPCGUtilityTools.resave_pcg_assets", "CubelessPCGContractAuthoringTools.audit_pcg_graph_contract","CubelessPCGContractAuthoringTools.read_pcg_node_contract","CubelessPCGContractAuthoringTools.set_pcg_static_mesh_spawner_entries", "CubelessPCGComponentGenerationTools.refresh_pcg_components","CubelessPCGComponentGenerationTools.bind_pcg_generation_settings_to_blueprint", "CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest","CubelessNiagaraPromotionManifestTools.execute_niagara_promotion_manifest", "CubelessNiagaraSystemAnalysisTools.analyze_niagara_system", "CubelessPIEInputTools.inject_pie_input_key", "CubelessNiagaraPreviewWorkflowTools.open_niagara_preview_player", "CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_player_state", "CubelessNiagaraPreviewWorkflowTools.preview_niagara_component_asset_swap", "CubelessNiagaraPreviewWorkflowTools.sample_niagara_system_in_preview_lab", "CubelessSplineComponentPointsTools.set_spline_component_points", "CubelessFoliageInstanceTools.list_foliage_types", "CubelessFoliageInstanceTools.add_foliage_instances", "CubelessFoliageInstanceTools.remove_foliage_instances", "CubelessLandscapeTools.list_landscapes", "CubelessLandscapeTools.get_landscape_info", "CubelessLandscapeTools.create_landscape", "CubelessLandscapeTools.set_landscape_material", "CubelessViewportCaptureTools.list_viewport_bookmarks", "CubelessViewportCaptureTools.set_active_viewport_realtime", "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot", "CubelessViewportCaptureTools.take_screenshot", "CubelessEditorLevelSessionTools.open_editor_level", "CubelessEditorLevelSessionTools.get_editor_level_open_status", "CubelessEditorLevelSessionTools.clear_dirty_package_flags", "CubelessEditorLevelSessionTools.safe_new_preview_map", "CubelessActorPropertyFocusTools.set_actor_property", "CubelessActorPropertyFocusTools.focus_viewport", "CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings", "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh", "CubelessChaosClothComponentTools.inspect_chaos_cloth_component", "CubelessChaosClothComponentTools.configure_chaos_cloth_component", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map", "CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections", 'CubelessChaosCinematicSequenceTools.create_chaos_level_sequence', 'CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence', 'CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing', 'CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline', 'CubelessChaosCacheSessionTools.record_chaos_cache', 'CubelessChaosCacheSessionTools.validate_chaos_cache', 'CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh', 'CubelessChaosGeometryAuthoringTools.configure_building_fracture', 'CubelessChaosGeometryAuthoringTools.configure_anchor_field', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state', 'CubelessChaosTraceStrainTools.trigger_trace_external_strain', 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture', 'CubelessChaosLearningTools.run_chaos_cache_smoke', 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke', 'CubelessGASAuthoringTools.inspect_gas_blueprint', 'CubelessGASAuthoringTools.create_gameplay_ability_blueprint', 'CubelessGASAuthoringTools.create_gameplay_effect_blueprint', 'CubelessGASAuthoringTools.create_attribute_set_blueprint', 'CubelessGASAuthoringTools.add_ability_system_component_to_blueprint', 'CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set', 'CubelessContentReconstructionTools.recreate_content_folder_mcp', 'CubelessContentReconstructionTools.postprocess_content_folder_mcp', 'CubelessContentReconstructionTools.repair_world_actor_instances_mcp', 'CubelessContentReconstructionTools.run_content_validation_pipeline_mcp', 'CubelessNiagaraRendererTools.inspect_niagara_renderers', 'CubelessNiagaraRendererTools.set_niagara_renderer_material', 'CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings', 'CubelessNiagaraRendererTools.add_niagara_sprite_renderer', 'CubelessNiagaraRendererTools.add_niagara_mesh_renderer', 'CubelessNiagaraRendererTools.set_niagara_renderer_enabled', 'CubelessNiagaraRendererTools.cleanup_niagara_renderers', 'CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids', 'CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter', 'CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection', 'CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status', 'CubelessPCGInspectionTools.resolve_pcg_graph', 'CubelessPCGInspectionTools.list_pcg_graph_nodes', 'CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes', 'CubelessPCGInspectionTools.inspect_pcg_component_outputs', 'CubelessNiagaraCleanupTools.remove_niagara_modules', 'CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_user_parameters', 'CubelessNiagaraCleanupTools.remove_niagara_simulation_stage', 'CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script', 'CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters', 'CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter', 'CubelessNiagaraInputParameterTools.set_niagara_module_static_switch', 'CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs', 'CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides', 'CubelessNiagaraInputParameterTools.set_niagara_module_input_value', 'CubelessNiagaraInputParameterTools.create_niagara_module_input_override', 'CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path', 'CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter', 'CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter', "CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state", "CubelessNiagaraPreviewLabTools.cleanup_niagara_preview_lab", "CubelessNiagaraPreviewLabTools.capture_niagara_preview_lab_view", "CubelessNiagaraPreviewLabTools.preview_niagara_system_in_preview_lab", "CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template", "CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable", "CubelessPCGGraphAuthoringTools.add_pcg_node", "CubelessPCGGraphAuthoringTools.connect_pcg_nodes", "CubelessPCGGraphAuthoringTools.set_pcg_node_setting")
            and type(child["data"].get("execution_mutation_attempted")) is bool):
        row["mutation_attempted"] = child["data"]["execution_mutation_attempted"]
    elif type(child["data"].get("native_mutation_attempted")) is bool:
        row["mutation_attempted"] = child["data"]["native_mutation_attempted"]
    attempted = row["mutation_attempted"]
    known_native = type(child["data"].get("mutation_performed")) is bool
    performed = child["data"].get("mutation_performed") if known_native else bool(attempted) if child["success"] else None if attempted else False
    if (row["type"] == "CubelessPCGTools.pcg_actor_smoke_test" and child["data"].get("full_effect_state_known") is False
            and child["data"].get("mutation_performed") is None):
        performed = None  # A safe noarg return is neither completion nor observed PCG mutation.
    if (row["type"] in ("CubelessBlueprintGraphTools.list_blueprint_nodes","CubelessBlueprintGraphTools.delete_blueprint_node","CubelessPCGTools.pcg_actor_smoke_test","CubelessNiagaraPromotionTools.create_niagara_system_from_source_template","CubelessPCGTools.validate_pcg_source_independence","CubelessPCGTools.promote_pcg_temp_graph","CubelessNiagaraScratchUserRecipeTools.create_niagara_visible_scratch_pad_snow_seed", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_vec2_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_color_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_user_parameter_module","CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_visible_toggle_module", "CubelessBlueprintResolutionConnectionTools.resolve_blueprint","CubelessBlueprintResolutionConnectionTools.connect_blueprint_nodes", "CubelessBlueprintPropertyGraphTools.set_blueprint_property","CubelessBlueprintPropertyGraphTools.list_blueprint_graphs", "CubelessTextureMaterialTools.generate_texture_from_prompt","CubelessTextureMaterialTools.generate_texture_for_mesh_uv","CubelessTextureMaterialTools.generate_and_apply_ai_texture","CubelessNiagaraCleanupTools.cleanup_generated_niagara_assets_offline","CubelessTextureMaterialTools.get_static_mesh_uv_layout","CubelessTextureMaterialTools.create_material_instance_with_texture","CubelessTextureMaterialTools.create_material_with_texture_set", "CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe","CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe","CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe", "CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton","CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe","CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe", "CubelessPCGUtilityTools.list_pcg_assets","CubelessPCGUtilityTools.list_pcg_components","CubelessPCGUtilityTools.set_pcg_debug_enabled","CubelessPCGUtilityTools.resave_pcg_assets", "CubelessPCGContractAuthoringTools.audit_pcg_graph_contract","CubelessPCGContractAuthoringTools.read_pcg_node_contract","CubelessPCGContractAuthoringTools.set_pcg_static_mesh_spawner_entries", "CubelessPCGComponentGenerationTools.refresh_pcg_components","CubelessPCGComponentGenerationTools.bind_pcg_generation_settings_to_blueprint", "CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest","CubelessNiagaraPromotionManifestTools.execute_niagara_promotion_manifest", "CubelessNiagaraSystemAnalysisTools.analyze_niagara_system", "CubelessPIEInputTools.inject_pie_input_key", "CubelessNiagaraPreviewWorkflowTools.open_niagara_preview_player", "CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_player_state", "CubelessNiagaraPreviewWorkflowTools.preview_niagara_component_asset_swap", "CubelessNiagaraPreviewWorkflowTools.sample_niagara_system_in_preview_lab", "CubelessStateTreeTools.list_state_trees", "CubelessStateTreeTools.inspect_state_tree", "CubelessStateTreeTools.create_state_tree",
                        "CubelessStateTreeTools.add_state_tree_state", "CubelessStateTreeTools.compile_state_tree",
                        "CubelessProjectTools.create_input_mapping", "CubelessProjectTools.audit_content_root_mcp",
                        "CubelessProjectTools.replace_asset_references_mcp",
                        "CubelessEditorTraceTools.line_trace_physical_material",
                        "CubelessEnumTools.create_user_defined_enum",
                        "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology",
                        "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links",
                        "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults",
                        "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links",
                        "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults",
                        "CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state",
                        "CubelessControlRigTransientProbeTools.controlrig_direct_gate_probe",
                        "CubelessControlRigTransientProbeTools.sample_controlrig_pre_post_runtime_pose",
                        "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe",
                        "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response",
                        "CubelessPostProcessAnimDemoVariantTools.ensure_postprocess_anim_demo_variant",
                        "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie",
                        "CubelessBlendSpaceVariantTools.ensure_blendspace_sample_variant",
                        "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid",
                        "CubelessLinkedAnimGraphVariantTools.ensure_linked_anim_graph_sample_variant",
                        "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant",
                        "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver",
                        "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose",
                        "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function",
                        "CubelessSplineComponentPointsTools.set_spline_component_points",
                        "CubelessFoliageInstanceTools.list_foliage_types", "CubelessFoliageInstanceTools.add_foliage_instances", "CubelessFoliageInstanceTools.remove_foliage_instances",
                        "CubelessLandscapeTools.list_landscapes", "CubelessLandscapeTools.get_landscape_info", "CubelessLandscapeTools.create_landscape", "CubelessLandscapeTools.set_landscape_material",
                        "CubelessViewportCaptureTools.list_viewport_bookmarks", "CubelessViewportCaptureTools.set_active_viewport_realtime", "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot", "CubelessViewportCaptureTools.take_screenshot",
                        "CubelessEditorLevelSessionTools.open_editor_level", "CubelessEditorLevelSessionTools.get_editor_level_open_status", "CubelessEditorLevelSessionTools.clear_dirty_package_flags", "CubelessEditorLevelSessionTools.safe_new_preview_map", "CubelessActorPropertyFocusTools.set_actor_property", "CubelessActorPropertyFocusTools.focus_viewport", "CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings", "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh", "CubelessChaosClothComponentTools.inspect_chaos_cloth_component", "CubelessChaosClothComponentTools.configure_chaos_cloth_component", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map", "CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections", 'CubelessChaosCinematicSequenceTools.create_chaos_level_sequence', 'CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence', 'CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing', 'CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline', 'CubelessChaosCacheSessionTools.record_chaos_cache', 'CubelessChaosCacheSessionTools.validate_chaos_cache', 'CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh', 'CubelessChaosGeometryAuthoringTools.configure_building_fracture', 'CubelessChaosGeometryAuthoringTools.configure_anchor_field', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state', 'CubelessChaosTraceStrainTools.trigger_trace_external_strain', 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture', 'CubelessChaosLearningTools.run_chaos_cache_smoke', 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke', 'CubelessGASAuthoringTools.inspect_gas_blueprint', 'CubelessGASAuthoringTools.create_gameplay_ability_blueprint', 'CubelessGASAuthoringTools.create_gameplay_effect_blueprint', 'CubelessGASAuthoringTools.create_attribute_set_blueprint', 'CubelessGASAuthoringTools.add_ability_system_component_to_blueprint', 'CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set', 'CubelessContentReconstructionTools.recreate_content_folder_mcp', 'CubelessContentReconstructionTools.postprocess_content_folder_mcp', 'CubelessContentReconstructionTools.repair_world_actor_instances_mcp', 'CubelessContentReconstructionTools.run_content_validation_pipeline_mcp', 'CubelessNiagaraRendererTools.inspect_niagara_renderers', 'CubelessNiagaraRendererTools.set_niagara_renderer_material', 'CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings', 'CubelessNiagaraRendererTools.add_niagara_sprite_renderer', 'CubelessNiagaraRendererTools.add_niagara_mesh_renderer', 'CubelessNiagaraRendererTools.set_niagara_renderer_enabled', 'CubelessNiagaraRendererTools.cleanup_niagara_renderers', 'CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds', 'CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids', 'CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter', 'CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values', 'CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection', 'CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references', 'CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface', 'CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status', 'CubelessPCGInspectionTools.resolve_pcg_graph', 'CubelessPCGInspectionTools.list_pcg_graph_nodes', 'CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes', 'CubelessPCGInspectionTools.inspect_pcg_component_outputs', 'CubelessNiagaraCleanupTools.remove_niagara_modules', 'CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes', 'CubelessNiagaraCleanupTools.remove_niagara_user_parameters', 'CubelessNiagaraCleanupTools.remove_niagara_simulation_stage', 'CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script', 'CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters', 'CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter', 'CubelessNiagaraInputParameterTools.set_niagara_module_static_switch', 'CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs', 'CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides', 'CubelessNiagaraInputParameterTools.set_niagara_module_input_value', 'CubelessNiagaraInputParameterTools.create_niagara_module_input_override', 'CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path', 'CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter', 'CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter', "CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state", "CubelessNiagaraPreviewLabTools.cleanup_niagara_preview_lab", "CubelessNiagaraPreviewLabTools.capture_niagara_preview_lab_view", "CubelessNiagaraPreviewLabTools.preview_niagara_system_in_preview_lab", "CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template", "CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable", "CubelessPCGGraphAuthoringTools.add_pcg_node", "CubelessPCGGraphAuthoringTools.connect_pcg_nodes", "CubelessPCGGraphAuthoringTools.set_pcg_node_setting")
            and child["data"].get("mutation_state_unknown") is True
            and child["data"].get("mutation_performed") is None):
        # No explicit writer attempt does not prove SDK load/readiness made no change.
        performed = None
    if any(change.get("created") is True for change in child["changes"]):
        performed = True
    in_flight = child["data"].get("command_may_still_be_in_flight") is True
    unknown = bool(in_flight or child["data"].get("mutation_state_unknown") is True or child["data"].get("outcome_unknown") is True or
                   attempted and not child["success"] and (not known_native or child["stage"] == "readback"))
    row.update({"status": "success" if child["success"] else "error", "mutation_performed": performed,
                "mutation_state_unknown": unknown, "command_may_still_be_in_flight": in_flight,
                "error": child["error"], "error_code": child["error_code"]})
    _chaos_cache_session_snapshot(row, child["data"])


def _completed_skeletal_sampling_observation(row):
    """Allow only verified completed FC reads to continue; total SDK unknown remains evidence."""
    if row.get("type") != "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie":
        return False
    child = row["response"]
    data = child["data"]
    native = data.get("native_outcome")
    counts = data.get("native_call_counts")
    if (child["success"] is not True or child["error_code"] or child["error"]
            or data.get("native_result_returned") is not True or data.get("native_mutation_attempted") is not False
            or data.get("explicit_asset_mutation_performed") is not False
            or row.get("command_may_still_be_in_flight") is not False
            or type(native) is not dict or native.get("operation") != "sample_skeletal_bones_in_sie"
            or native.get("success") is not True or native.get("postcondition_passed") is not True
            or native.get("mutation_attempted") is not False or type(counts) is not dict):
        return False
    for key in ("asset_load_calls", "modify_calls", "tick_calls", "update_calls", "refresh_bone_transforms_calls",
                "initialize_calls", "compile_calls", "save_calls", "start_pie_calls", "stop_pie_calls", "transaction_calls"):
        if type(counts.get(key)) not in (int, float) or counts[key] != 0:
            return False
    if (native.get("compile_requested") is not False or native.get("compile_attempted") is not False
            or native.get("save_requested") is not False or native.get("save_attempted") is not False or native.get("saved") is not False
            or child["compile"].get("requested") is not False or child["compile"].get("attempted") is not False
            or child["save"].get("requested") is not False or child["save"].get("attempted") is not False
            or child["save"].get("saved") is not False or any(change.get("created") is True for change in child["changes"])):
        return False
    return True


def _aggregate(result, summary):
    rows = summary["results"]
    result["changes"] = [dict(change, batch_index=row["index"], canonical_tool=row["type"])
                         for row in rows for change in row["response"]["changes"]]
    for field in ("compile", "save"):
        entries = [{"index": row["index"], "type": row["type"],
                    "blueprint_name": row["params"].get("blueprint_name"), **copy.deepcopy(row["response"][field])}
                   for row in rows if row["response"][field].get("requested")]
        result[field]["entries"] = entries
        result[field]["requested"] = bool(entries)
        attempts = [entry.get("attempted") for entry in entries]
        result[field]["attempted"] = True if True in attempts else None if None in attempts else False
        if field == "save":
            result[field]["saved"] = all(e.get("saved") is True for e in entries) if entries else False
        result[field]["status"] = "per_entry" if entries else "not_requested"
    summary["mutation_attempted"] = any(row["mutation_attempted"] for row in rows)
    values = [row.get("mutation_performed") for row in rows]
    summary["mutation_performed"] = True if True in values else None if None in values else False
    summary["mutation_state_unknown"] = any(row.get("mutation_state_unknown", False) for row in rows)
    summary["command_may_still_be_in_flight"] = any(row.get("command_may_still_be_in_flight", False) for row in rows)
    summary["partial_mutation"] = bool((summary["failed"] or summary["forced_stop"] or summary["not_run"]) and
                                       (summary["mutation_attempted"] or summary["mutation_state_unknown"]))
    summary["do_not_retry"] = bool(summary["mutation_attempted"] or summary["mutation_state_unknown"])
    session_rows = [row for row in rows if row["type"] in ("CubelessChaosCacheSessionTools.record_chaos_cache", "CubelessChaosCacheSessionTools.validate_chaos_cache", 'CubelessChaosTraceStrainTools.trigger_trace_external_strain', 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture', 'CubelessChaosLearningTools.run_chaos_cache_smoke', 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke')]
    if session_rows:
        summary["session_snapshots"] = [{"index": row["index"], "type": row["type"], **{key: copy.deepcopy(row[key]) for key in ("session_execution", "session_actual_saved_observed", "session_mutation_state_unknown", "session_in_flight", "session_operation_complete", "session_id_observed", "session_verification_passed") if key in row}} for row in session_rows]
        summary["session_actual_saved_observed"] = any(row.get("session_actual_saved_observed") for row in session_rows)
        summary["session_mutation_state_unknown"] = any(row.get("session_mutation_state_unknown") for row in session_rows)
        summary["session_in_flight"] = any(row.get("session_in_flight") for row in session_rows)
        summary["session_saved_history_promoted_to_current_request_save"] = False
    result["retry"].update({"automatic_retry": False,
        "duplicate_risk": "unknown_outcome" if summary["mutation_state_unknown"] else "batch_mutations_would_repeat" if summary["mutation_attempted"] else "none_observed",
        "policy": "Same ID returns the historical complete response. Interrupted IDs never resume; inspect journal and assets before a new ID."})


def _run(result, commands, stop_on_error, _compile_status_owner=None):
    summary = _summary(commands, stop_on_error)
    if any(c["type"] in ("execute_python", "CubelessPythonExecutionTools.execute_unreal_python") for c in commands):
        # Arbitrary script can retire/change the Editor/transaction. Do not open
        # an outer ScopedEditorTransaction whose destructor cannot honor the
        # native unknown-stop/own-transaction identity and depth checks.
        summary["transaction"].update(batch_scope_skipped_for_python_execution=True,
            transaction_scope="per_child_native_scopes_without_outer_batch_transaction",
            grouping_preserved=False, full_batch_transaction_contract_complete=False,
            reason="Arbitrary Python may change transaction/Editor roles; unguarded outer cleanup is unsafe",
            original_single_undo_grouping="valid_original_contract_pending")
    if any(c["type"] in _ACTOR_BATCH_TOOLS for c in commands):
        # Actor delegates/current loss can retire the Editor/transaction too.
        # Preserve actual Native stop state without an unguarded Python exit.
        summary["transaction"].update(batch_scope_skipped_for_actor_operations=True,
            transaction_scope="per_child_native_scopes_without_outer_batch_transaction",
            grouping_preserved=False, full_batch_transaction_contract_complete=False,
            actor_scope_reason="Actor SDK callbacks may invalidate Editor/transaction roles; outer unguarded cleanup is unsafe",
            original_single_undo_grouping="valid_original_contract_pending")
    if _compile_status_owner is not None:
        summary["transaction"].update(batch_scope_skipped_for_compile_status_wait=True,
            transaction_scope="per_child_native_scopes_without_outer_batch_transaction",
            grouping_preserved=False, full_batch_transaction_contract_complete=False,
            original_single_undo_grouping="valid_original_contract_pending")
        summary["compile_status_continuation"] = dict(exact_method="inspect_niagara_compile_status",
            exact_factory_methods=["create_empty_niagara_system", "create_basic_sprite_niagara_system"],
            parent_journal_only=True, child_journals_created=0, HN_wait_seconds=0.0,
            active_dispatch_budget_seconds=BETWEEN_STEP_BUDGET_SECONDS, HN_timeout_changed=False)
    result["data"]["batch"] = summary
    started = time.perf_counter()
    suspended_seconds = 0.0
    runtime_query_only = bool(commands) and all((c["type"] in ("CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state",
        "CubelessControlRigSolverIntentTools.validate_controlrig_solver_intents",
        "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe",
        "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response",
        "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie",
        "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid")
        or c["type"] == "CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings"
        or c["type"] == "CubelessChaosClothComponentTools.inspect_chaos_cloth_component"
        or c["type"] in ("CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map") and _command_effect(c) == "read_world"
        or c["type"] == "CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections" and _command_effect(c) == "read_world"
        or c["type"] == "CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline"
        or c["type"] == "CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters"
        or c["type"] == "CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs"
        or c["type"] == "CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides"
        or c["type"] == "CubelessNiagaraFactoryBootstrapTools.inspect_niagara_factory_bootstrap_defaults"
        or c["type"] == "CubelessPCGInspectionTools.list_pcg_graph_nodes"
        or c["type"] == "CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes"
        or c["type"] == "CubelessPCGInspectionTools.inspect_pcg_component_outputs"
        or c["type"] == "CubelessNiagaraPromotionTools.audit_niagara_promotion_candidate"
        or c["type"] in ["CubelessNiagaraEventStageTools.inspect_niagara_simulation_stages", "CubelessNiagaraEventStageTools.inspect_niagara_event_handlers"]
        or c["type"] in ["CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_runtime_state", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_parameter_store", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_grid2d_runtime", "CubelessNiagaraRuntimeInspectionTools.sample_render_target2d_region_stats"]
        or c["type"] == "CubelessBlueprintGraphTools.list_blueprint_nodes"
        or c["type"] == "CubelessBlueprintPropertyGraphTools.list_blueprint_graphs"
        or c["type"] == "CubelessBlueprintResolutionConnectionTools.resolve_blueprint"
        or c["type"] == "CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state"
        or c["type"] in ('CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe', 'CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe')
        or c["type"] in ('CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton', 'CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe', 'CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe')
        or c["type"] in ("CubelessPCGUtilityTools.list_pcg_assets", "CubelessPCGUtilityTools.list_pcg_components")
        or c["type"] in ("CubelessPCGTools.validate_pcg_source_independence","CubelessPCGTools.promote_pcg_temp_graph") and _command_effect(c) == "read_world"
        or c["type"] in ("CubelessPCGContractAuthoringTools.audit_pcg_graph_contract", "CubelessPCGContractAuthoringTools.read_pcg_node_contract")
        or c["type"] == "CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest"
        or c["type"] == "CubelessNiagaraSystemAnalysisTools.analyze_niagara_system"
        or c["type"] == "CubelessPIEInputTools.inject_pie_input_key"
        or c["type"] == "CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_player_state"
        or c["type"] in ["CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template", "CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap"]
        or c["type"] in ["CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary", "CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable"] and _command_effect(c) == "read_world"
        or c["type"] == "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack"
        or c["type"] == "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph"
        or c["type"] == "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions"
        or c["type"] == "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes"
        or c["type"] == "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface"
        or c["type"] == "CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values"
        or c["type"] == "CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties"
        or c["type"] == "CubelessNiagaraRendererTools.inspect_niagara_renderers"
        or c["type"] in ('CubelessContentReconstructionTools.recreate_content_folder_mcp', 'CubelessContentReconstructionTools.postprocess_content_folder_mcp', 'CubelessContentReconstructionTools.repair_world_actor_instances_mcp', 'CubelessContentReconstructionTools.run_content_validation_pipeline_mcp') and _command_effect(c) == "read_world"
        or c["type"] == 'CubelessGASAuthoringTools.inspect_gas_blueprint'
        or c["type"] == 'CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke'
        or c["type"] == 'CubelessChaosLearningTools.run_chaos_cache_smoke'
        or c["type"] == 'CubelessChaosLearningTools.create_chaos_destruction_learning_fixture' and _command_effect(c) == "read_world"
        or c["type"] in ('CubelessChaosTraceStrainTools.trigger_trace_external_strain',)
        or c["type"] in ('CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded', 'CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state')
        or c["type"] in ('CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh', 'CubelessChaosGeometryAuthoringTools.configure_building_fracture', 'CubelessChaosGeometryAuthoringTools.configure_anchor_field') and _command_effect(c) == "read_world"
        or c["type"] in ('CubelessChaosCacheSessionTools.record_chaos_cache', 'CubelessChaosCacheSessionTools.validate_chaos_cache')
        or c["type"] == "CubelessChaosClothComponentTools.configure_chaos_cloth_component" and _command_effect(c) in ("read_world", "runtime_write")
        or c["type"] in ("CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh")
        or c["type"] == "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings" and _command_effect(c) in ("read_world", "runtime_write")
        or c["type"] == "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose" and _command_effect(c) == "runtime_write") for c in commands)
    # Only these exact runtime-reader/runtime-property-probe/runtime-response/pure-validator actions are admitted during PIE/SIE.
    # Any other action retains the existing idle gate before dispatch or a transaction.
    if _compile_status_owner is not None:
        _compile_status_batch_current(_compile_status_owner)
    if not runtime_query_only:
        ensure_editor_idle()
    if _compile_status_owner is not None:
        _compile_status_batch_current(_compile_status_owner)

    def persist(stage):
        if _compile_status_owner is not None:
            _compile_status_batch_current(_compile_status_owner)
        result["stage"] = stage
        # Sequence in the snapshot names the checkpoint being persisted.
        summary["checkpoint_sequence"] += 1
        checkpoint(result)
        if _compile_status_owner is not None:
            _compile_status_batch_current(_compile_status_owner)

    def run_steps():
        nonlocal suspended_seconds
        for index, command in enumerate(commands):
            if index and time.perf_counter() - started - suspended_seconds >= BETWEEN_STEP_BUDGET_SECONDS:
                summary.update(forced_stop=True, stop_reason="between_step_time_budget", stopped_at_index=index)
                break
            child = new_result(result["operation_id"] + ":" + str(index))
            capability = _CAPABILITIES[command["type"]]
            row = {"index": index, "type": command["type"], "params": command["params"], "effect": _command_effect(command),
                   "phase": "step_intent", "status": "pending", "mutation_attempted": False,
                   "mutation_performed": False, "mutation_state_unknown": False,
                   "response": child, "elapsed_seconds": 0.0}
            if "legacy_original_params" in command:
                row["legacy_original_params"] = command["legacy_original_params"]
            if "legacy_raw_native_entry" in command:
                row["legacy_raw_native_entry"] = command["legacy_raw_native_entry"]
            summary["results"].append(row)
            summary["executed"] += 1
            summary["not_run"] = list(range(index + 1, len(commands)))
            child_started = time.perf_counter()

            def before_mutation(observed):
                if row["type"] in _ACTOR_BATCH_TOOLS and observed["data"].get("actor_batch_native_result_returned") is True:
                    _actor_batch_observation(row)
                    row["phase"] = "engine_returned"
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:returned_actor_operation")
                    return
                if row["type"] in ("execute_python", "CubelessPythonExecutionTools.execute_unreal_python"):
                    data = observed["data"]
                    row.update(phase="engine_returned" if observed["stage"] == "returned_python_execution_operation" else "mutation_intent",
                               mutation_attempted=data.get("execution_mutation_attempted", False),
                               mutation_performed=data.get("mutation_performed", False),
                               mutation_state_unknown=data.get("mutation_state_unknown") is True,
                               command_may_still_be_in_flight=data.get("command_may_still_be_in_flight") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessFoliageInstanceTools.list_foliage_types", "CubelessFoliageInstanceTools.add_foliage_instances", "CubelessFoliageInstanceTools.remove_foliage_instances")
                        and observed["stage"] in ("before_foliage_instances_checkpoint", "returned_foliage_instances_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosSolverSettingsTools.inspect_chaos_solver_settings", "CubelessChaosSolverSettingsTools.configure_chaos_solver_settings")
                        and observed["stage"] in ("before_chaos_solver_settings_checkpoint", "returned_chaos_solver_settings_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosClothComponentTools.inspect_chaos_cloth_component", "CubelessChaosClothComponentTools.configure_chaos_cloth_component")
                        and observed["stage"] in ("before_chaos_cloth_component_checkpoint", "returned_chaos_cloth_component_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosClothAuthoringTools.configure_chaos_cloth_dataflow_node", "CubelessChaosClothAuthoringTools.configure_chaos_cloth_weight_map")
                        and observed["stage"] in ("before_chaos_cloth_authoring_checkpoint", "returned_chaos_cloth_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessLegacyChaosClothingTools.create_legacy_chaos_clothing_from_skeletal_sections",)
                        and observed["stage"] in ("before_legacy_chaos_clothing_checkpoint", "returned_legacy_chaos_clothing_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosClothInspectionTools.inspect_chaos_cloth_asset", "CubelessChaosClothInspectionTools.inspect_chaos_cloth_simulation_mesh")
                        and observed["stage"] in ("before_chaos_cloth_inspection_checkpoint", "returned_chaos_cloth_inspection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessActorPropertyFocusTools.set_actor_property", "CubelessActorPropertyFocusTools.focus_viewport")
                        and observed["stage"] in ("before_actor_property_focus_checkpoint", "returned_actor_property_focus_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessEditorLevelSessionTools.open_editor_level", "CubelessEditorLevelSessionTools.get_editor_level_open_status", "CubelessEditorLevelSessionTools.clear_dirty_package_flags", "CubelessEditorLevelSessionTools.safe_new_preview_map")
                        and observed["stage"] in ("before_editor_level_session_checkpoint", "returned_editor_level_session_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessViewportCaptureTools.list_viewport_bookmarks", "CubelessViewportCaptureTools.set_active_viewport_realtime", "CubelessViewportCaptureTools.capture_viewport_bookmark_screenshot", "CubelessViewportCaptureTools.take_screenshot")
                        and observed["stage"] in ("before_viewport_capture_checkpoint", "returned_viewport_capture_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosCinematicSequenceTools.create_chaos_level_sequence", "CubelessChaosCinematicSequenceTools.bind_chaos_cache_to_sequence", "CubelessChaosCinematicSequenceTools.configure_chaos_sequence_timing", "CubelessChaosCinematicSequenceTools.inspect_chaos_cinematic_pipeline")
                        and observed["stage"] in ("before_chaos_cinematic_sequence_checkpoint", "returned_chaos_cinematic_sequence_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosCacheSessionTools.record_chaos_cache", "CubelessChaosCacheSessionTools.validate_chaos_cache")
                        and observed["stage"] in ("before_chaos_cache_sessions_checkpoint", "returned_chaos_cache_sessions_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _chaos_cache_session_snapshot(row, data)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosDestructionPerformanceTools.run_chaos_destruction_performance_smoke",)
                        and observed["stage"] in ("before_chaos_destruction_performance_checkpoint", "returned_chaos_destruction_performance_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _chaos_cache_session_snapshot(row, data)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosLearningTools.create_chaos_destruction_learning_fixture", "CubelessChaosLearningTools.run_chaos_cache_smoke")
                        and observed["stage"] in ("before_chaos_learning_checkpoint", "returned_chaos_learning_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _chaos_cache_session_snapshot(row, data)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosTraceStrainTools.trigger_trace_external_strain",)
                        and observed["stage"] in ("before_chaos_trace_strain_checkpoint", "returned_chaos_trace_strain_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _chaos_cache_session_snapshot(row, data)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessChaosGeometryAuthoringTools.create_geometry_collection_from_mesh", "CubelessChaosGeometryAuthoringTools.configure_building_fracture", "CubelessChaosGeometryAuthoringTools.configure_anchor_field")
                        and observed["stage"] in ("before_chaos_geometry_authoring_checkpoint", "returned_chaos_geometry_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessGASAuthoringTools.inspect_gas_blueprint", "CubelessGASAuthoringTools.create_gameplay_ability_blueprint", "CubelessGASAuthoringTools.create_gameplay_effect_blueprint", "CubelessGASAuthoringTools.create_attribute_set_blueprint", "CubelessGASAuthoringTools.add_ability_system_component_to_blueprint", "CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set")
                        and observed["stage"] in ("before_gas_authoring_checkpoint", "returned_gas_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessContentReconstructionTools.recreate_content_folder_mcp", "CubelessContentReconstructionTools.postprocess_content_folder_mcp", "CubelessContentReconstructionTools.repair_world_actor_instances_mcp", "CubelessContentReconstructionTools.run_content_validation_pipeline_mcp")
                        and observed["stage"] in ("before_content_reconstruction_checkpoint", "returned_content_reconstruction_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraRendererTools.inspect_niagara_renderers", "CubelessNiagaraRendererTools.set_niagara_renderer_material", "CubelessNiagaraRendererTools.set_niagara_sprite_renderer_settings", "CubelessNiagaraRendererTools.add_niagara_sprite_renderer", "CubelessNiagaraRendererTools.add_niagara_mesh_renderer", "CubelessNiagaraRendererTools.set_niagara_renderer_enabled", "CubelessNiagaraRendererTools.cleanup_niagara_renderers")
                        and observed["stage"] in ("before_niagara_renderers_checkpoint", "returned_niagara_renderers_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraEmitterConfigurationTools.inspect_niagara_emitter_properties", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_enabled", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_local_space", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_sim_target", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_fixed_bounds", "CubelessNiagaraEmitterConfigurationTools.set_niagara_emitter_persistent_ids", "CubelessNiagaraEmitterConfigurationTools.remove_niagara_emitter")
                        and observed["stage"] in ("before_niagara_emitter_configuration_checkpoint", "returned_niagara_emitter_configuration_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraParameterCollectionTools.inspect_niagara_parameter_collection_values", "CubelessNiagaraParameterCollectionTools.set_niagara_parameter_collection_values", "CubelessNiagaraParameterCollectionTools.repair_niagara_parameter_collection", "CubelessNiagaraParameterCollectionTools.remap_niagara_parameter_collection_references", "CubelessNiagaraParameterCollectionTools.remap_niagara_script_parameter_collection_references", "CubelessNiagaraParameterCollectionTools.recompile_niagara_scripts_in_root")
                        and observed["stage"] in ("before_niagara_parameter_collections_checkpoint", "returned_niagara_parameter_collections_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraAuthoringInspectionTools.inspect_niagara_stack", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_graph_actions", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_custom_hlsl_nodes", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_scratch_pad_interface", "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status")
                        and observed["stage"] in ("before_niagara_authoring_inspection_checkpoint", "returned_niagara_authoring_inspection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessPCGInspectionTools.resolve_pcg_graph", "CubelessPCGInspectionTools.list_pcg_graph_nodes", "CubelessPCGInspectionTools.inspect_pcg_custom_hlsl_nodes", "CubelessPCGInspectionTools.inspect_pcg_component_outputs")
                        and observed["stage"] in ("before_pcg_inspection_checkpoint", "returned_pcg_inspection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraFactoryBootstrapTools.inspect_niagara_factory_bootstrap_defaults", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_system", "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_emitter", "CubelessNiagaraFactoryBootstrapTools.create_basic_sprite_niagara_system")
                        and observed["stage"] in ("before_niagara_factory_bootstrap_checkpoint", "returned_niagara_factory_bootstrap_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraResourceBindingTools.set_niagara_render_target2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_texture2d_module_input", "CubelessNiagaraResourceBindingTools.set_niagara_scratch_pad_function_input_default", "CubelessNiagaraResourceBindingTools.link_niagara_scratch_pad_pin_to_user_parameter")
                        and observed["stage"] in ("before_niagara_resource_bindings_checkpoint", "returned_niagara_resource_bindings_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_module_input', 'CubelessNiagaraScratchStackAuthoringTools.add_niagara_scratch_pad_assignment_module_to_stack', 'CubelessNiagaraScratchStackAuthoringTools.wrap_niagara_scratch_pad_output_with_stack_context')
                        and observed["stage"] in ("before_niagara_scratch_stack_authoring_checkpoint", "returned_niagara_scratch_stack_authoring_operation", "assignment_compile_status")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraEventStageTools.inspect_niagara_simulation_stages', 'CubelessNiagaraEventStageTools.inspect_niagara_event_handlers', 'CubelessNiagaraEventStageTools.add_niagara_event_handler')
                        and observed["stage"] in ("before_niagara_event_stage_authoring_checkpoint", "returned_niagara_event_stage_authoring_operation", "assignment_compile_status")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraPromotionTools.audit_niagara_promotion_candidate', 'CubelessNiagaraPromotionTools.promote_niagara_system_candidate')
                        and observed["stage"] in ("before_niagara_promotion_checkpoint", "returned_niagara_promotion_operation", "assignment_compile_status")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraSpritePresetTools.create_sprite_niagara_preset_system',)
                        and observed["stage"] in ("before_niagara_sprite_preset_authoring_checkpoint", "returned_niagara_sprite_preset_authoring_operation", "assignment_compile_status")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraCustomHlslConversionTools.replace_niagara_custom_hlsl', 'CubelessNiagaraCustomHlslConversionTools.apply_niagara_scratch_pad_graph_recipe')
                        and observed["stage"] in ("before_niagara_custom_hlsl_conversion_checkpoint", "returned_niagara_custom_hlsl_conversion_operation", "assignment_compile_status")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraPreviewLabTools.get_niagara_preview_lab_state', 'CubelessNiagaraPreviewLabTools.cleanup_niagara_preview_lab', 'CubelessNiagaraPreviewLabTools.capture_niagara_preview_lab_view', 'CubelessNiagaraPreviewLabTools.preview_niagara_system_in_preview_lab')
                        and observed["stage"] in ("before_niagara_preview_lab_checkpoint", "returned_niagara_preview_lab_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessNiagaraPreviewWorkflowTools.get_niagara_preview_workflows_state', 'CubelessNiagaraPreviewWorkflowTools.cleanup_niagara_preview_workflows', 'CubelessNiagaraPreviewWorkflowTools.capture_niagara_preview_workflows_view', 'CubelessNiagaraPreviewWorkflowTools.sample_niagara_system_in_preview_lab')
                        and observed["stage"] in ("before_niagara_preview_workflows_checkpoint", "returned_niagara_preview_workflows_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ('CubelessBlueprintNiagaraTemplateSwapTools.inspect_blueprint_niagara_component_template', 'CubelessBlueprintNiagaraTemplateSwapTools.dry_run_blueprint_niagara_component_template_asset_swap', 'CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_canary', 'CubelessBlueprintNiagaraTemplateSwapTools.execute_blueprint_niagara_component_template_asset_swap_durable')
                        and observed["stage"] in ("before_blueprint_niagara_template_swap_checkpoint", "returned_blueprint_niagara_template_swap_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessPIEInputTools.inject_pie_input_key"
                        and observed["stage"] in ("before_pie_input_checkpoint", "returned_pie_input_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessNiagaraSystemAnalysisTools.analyze_niagara_system"
                        and observed["stage"] in ("before_niagara_system_analysis_checkpoint", "returned_niagara_system_analysis_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraPromotionManifestTools.plan_niagara_promotion_manifest", "CubelessNiagaraPromotionManifestTools.execute_niagara_promotion_manifest")
                        and observed["stage"] in ("before_niagara_promotion_manifest_checkpoint", "returned_niagara_promotion_manifest_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessPCGComponentGenerationTools.refresh_pcg_components", "CubelessPCGComponentGenerationTools.bind_pcg_generation_settings_to_blueprint")
                        and observed["stage"] in ("before_pcg_component_generation_checkpoint", "returned_pcg_component_generation_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessNiagaraPromotionTools.create_niagara_system_from_source_template"
                        and observed["stage"] in ("before_niagara_source_template_checkpoint","returned_niagara_source_template_operation")):
                    data = observed["data"]
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=data.get("execution_mutation_attempted") is True,
                        mutation_performed=data.get("mutation_performed"),mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result,summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessPCGTools.pcg_actor_smoke_test"
                        and observed["stage"] in ("before_pcg_actor_smoke_checkpoint","returned_pcg_actor_smoke_operation","returned_pcg_actor_smoke_selection")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessPCGTools.validate_pcg_source_independence","CubelessPCGTools.promote_pcg_temp_graph")
                        and observed["stage"] in ("before_pcg_promotion_checkpoint","returned_pcg_promotion_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessPCGContractAuthoringTools.audit_pcg_graph_contract", "CubelessPCGContractAuthoringTools.read_pcg_node_contract", "CubelessPCGContractAuthoringTools.set_pcg_static_mesh_spawner_entries")
                        and observed["stage"] in ("before_pcg_contract_authoring_checkpoint", "returned_pcg_contract_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessPCGUtilityTools.list_pcg_assets", "CubelessPCGUtilityTools.list_pcg_components", "CubelessPCGUtilityTools.set_pcg_debug_enabled", "CubelessPCGUtilityTools.resave_pcg_assets")
                        and observed["stage"] in ("before_pcg_utilities_checkpoint", "returned_pcg_utilities_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessControlRigRecipePlanningTools.profile_controlrig_skeleton", "CubelessControlRigRecipePlanningTools.compile_controlrig_profile_recipe", "CubelessControlRigRecipePlanningTools.make_controlrig_preset_recipe")
                        and observed["stage"] in ("before_controlrig_recipe_planning_checkpoint", "returned_controlrig_recipe_planning_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessBlueprintPropertyGraphTools.set_blueprint_property", "CubelessBlueprintPropertyGraphTools.list_blueprint_graphs")
                        and observed["stage"] in ("before_blueprint_property_graph_checkpoint", "returned_blueprint_property_graph_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraScratchUserRecipeTools.create_niagara_visible_scratch_pad_snow_seed", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_vec2_user_parameter_module", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_color_user_parameter_module", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_user_parameter_module", "CubelessNiagaraScratchUserRecipeTools.create_niagara_scratch_pad_bool_visible_toggle_module")
                        and observed["stage"] in ("before_niagara_scratch_user_recipes_checkpoint", "returned_niagara_scratch_user_recipes_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessBlueprintResolutionConnectionTools.resolve_blueprint", "CubelessBlueprintResolutionConnectionTools.connect_blueprint_nodes")
                        and observed["stage"] in ("before_blueprint_resolution_connection_checkpoint", "returned_blueprint_resolution_connection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessTextureMaterialTools.get_static_mesh_uv_layout", "CubelessTextureMaterialTools.create_material_instance_with_texture", "CubelessTextureMaterialTools.create_material_with_texture_set", "CubelessTextureMaterialTools.generate_pbr_companion_textures", "CubelessTextureMaterialTools.apply_material_to_mesh", "CubelessTextureMaterialTools.generate_texture_from_prompt", "CubelessTextureMaterialTools.generate_texture_for_mesh_uv", "CubelessTextureMaterialTools.generate_and_apply_ai_texture")
                        and observed["stage"] in ("before_texture_material_authoring_checkpoint", "returned_texture_material_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessControlRigRecipeAuthoringTools.validate_controlrig_recipe", "CubelessControlRigRecipeAuthoringTools.dry_run_controlrig_recipe", "CubelessControlRigRecipeAuthoringTools.build_controlrig_recipe")
                        and observed["stage"] in ("before_controlrig_recipe_authoring_checkpoint", "returned_controlrig_recipe_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_runtime_state", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_parameter_store", "CubelessNiagaraRuntimeInspectionTools.inspect_niagara_component_grid2d_runtime", "CubelessNiagaraRuntimeInspectionTools.sample_render_target2d_region_stats")
                        and observed["stage"] in ("before_niagara_runtime_inspection_checkpoint", "returned_niagara_runtime_inspection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraCleanupTools.remove_niagara_modules", "CubelessNiagaraCleanupTools.remove_niagara_parameter_map_set_nodes", "CubelessNiagaraCleanupTools.remove_niagara_orphan_parameter_map_get_nodes", "CubelessNiagaraCleanupTools.remove_niagara_user_parameters", "CubelessNiagaraCleanupTools.remove_niagara_simulation_stage", "CubelessNiagaraCleanupTools.remove_niagara_scratch_pad_script","CubelessNiagaraCleanupTools.cleanup_generated_niagara_assets_offline")
                        and observed["stage"] in ("before_niagara_cleanup_checkpoint", "returned_niagara_cleanup_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraInputParameterTools.inspect_niagara_rapid_iteration_parameters", "CubelessNiagaraInputParameterTools.set_niagara_rapid_iteration_parameter", "CubelessNiagaraInputParameterTools.set_niagara_module_static_switch", "CubelessNiagaraInputParameterTools.inspect_niagara_module_inputs", "CubelessNiagaraInputParameterTools.inspect_niagara_data_interface_overrides", "CubelessNiagaraInputParameterTools.set_niagara_module_input_value", "CubelessNiagaraInputParameterTools.create_niagara_module_input_override")
                        and observed["stage"] in ("before_niagara_input_parameters_checkpoint", "returned_niagara_input_parameters_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraModuleInputPathTools.remove_niagara_unused_module_input_path", "CubelessNiagaraModuleInputPathTools.set_niagara_module_input_user_parameter", "CubelessNiagaraModuleInputPathTools.set_niagara_array_data_interface_module_input_user_parameter")
                        and observed["stage"] in ("before_niagara_module_input_paths_checkpoint", "returned_niagara_module_input_paths_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessNiagaraDataChannelAuthoringTools.configure_niagara_data_channel_asset", "CubelessNiagaraDataChannelAuthoringTools.create_niagara_ndc_reader_scratch_pad_module", "CubelessNiagaraDataChannelAuthoringInternal.add_niagara_scratch_pad_di_function_call", "CubelessNiagaraDataChannelAuthoringTools.add_niagara_scratch_pad_di_variadic_input")
                        and observed["stage"] in ("before_niagara_data_channel_authoring_checkpoint", "returned_niagara_data_channel_authoring_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_performance_guarded", "CubelessGeometryCollectionInspectionTools.inspect_geometry_collection_runtime_state")
                        and observed["stage"] in ("before_geometry_collection_inspection_checkpoint", "returned_geometry_collection_inspection_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessLandscapeTools.list_landscapes", "CubelessLandscapeTools.get_landscape_info", "CubelessLandscapeTools.create_landscape", "CubelessLandscapeTools.set_landscape_material")
                        and observed["stage"] in ("before_landscape_checkpoint", "returned_landscape_operation")):
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessSplineComponentPointsTools.set_spline_component_points"
                        and observed["stage"] in ("before_spline_component_points_checkpoint", "returned_spline_component_points_operation")):
                    # Native and direct Python share engine accounting, not Native call counts.
                    data = observed["data"]
                    attempted = data.get("execution_mutation_attempted", data.get("native_mutation_attempted"))
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessBlueprintFunctionExtractionTools.collapse_blueprint_nodes_to_function"
                        and observed["stage"] in ("before_blueprint_function_extraction_checkpoint", "returned_blueprint_function_extraction_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "mutation_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimNodePrePostRuntimePoseTools.sample_anim_node_pre_post_runtime_pose"
                        and observed["stage"] in ("before_anim_node_pre_post_runtime_pose_checkpoint", "returned_anim_node_pre_post_runtime_pose_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimBPLocomotionDriverTools.ensure_anim_bp_event_graph_locomotion_driver"
                        and observed["stage"] in ("before_anim_bp_locomotion_driver_checkpoint", "returned_anim_bp_locomotion_driver_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimStateMachineVariantTools.ensure_anim_state_machine_sample_variant"
                        and observed["stage"] in ("before_anim_state_machine_variant_checkpoint", "returned_anim_state_machine_variant_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessLinkedAnimGraphVariantTools.ensure_linked_anim_graph_sample_variant"
                        and observed["stage"] in ("before_linked_anim_graph_variant_checkpoint", "returned_linked_anim_graph_variant_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessBlendSpaceVariantTools.ensure_blendspace_sample_variant"
                        and observed["stage"] in ("before_blendspace_variant_checkpoint", "returned_blendspace_variant_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessBlendSpacePoseGridTools.sample_blendspace_runtime_pose_grid"
                        and observed["stage"] in ("before_blendspace_pose_grid_checkpoint", "returned_blendspace_pose_grid_operation")):
                    # Preserve complete Native/raw partial edits, save and cleanup receipts before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessSkeletalBoneSamplingTools.sample_skeletal_bones_in_sie"
                        and observed["stage"] in ("before_skeletal_bone_sampling_checkpoint", "returned_skeletal_bone_sampling_operation")):
                    # Preserve current bone/socket warnings/partial results and unknown SDK effects even without writers.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessPostProcessAnimDemoVariantTools.ensure_postprocess_anim_demo_variant"
                        and observed["stage"] in ("before_postprocess_anim_demo_variant_checkpoint", "returned_postprocess_anim_demo_variant_operation")):
                    # Actual asset2/graph/compiler/class-assignment/save2 partial receipts precede terminal failure.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "asset_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimStateRuntimeResponseTools.sample_anim_state_machine_runtime_response"
                        and observed["stage"] in ("before_anim_state_machine_runtime_response_checkpoint", "returned_anim_state_machine_runtime_response_operation")):
                    # Baseline/case/apply/tick/main snapshot/forward restore and actual partial receipts belong to the parent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "runtime_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimInstanceRuntimePropertyProbeTools.set_anim_instance_runtime_property_for_probe"
                        and observed["stage"] in ("before_anim_instance_runtime_property_probe_checkpoint", "returned_anim_instance_runtime_property_probe_operation")):
                    # Actual assignment rows/previous values/tick/refresh/snapshot and failure prefix belong to the parent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "runtime_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessControlRigTransientProbeTools.controlrig_direct_gate_probe", "CubelessControlRigTransientProbeTools.sample_controlrig_pre_post_runtime_pose")
                        and observed["stage"] in ("before_controlrig_direct_gate_probe_operation", "returned_controlrig_direct_gate_probe_operation",
                            "before_controlrig_pre_post_runtime_pose_operation", "returned_controlrig_pre_post_runtime_pose_operation")):
                    # Native fresh transient rig/Initialize/Execute/echo/sample and failure prefix precedes criteria.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessControlRigSolverIntentTools.validate_controlrig_solver_intents"
                        and observed["stage"] in ("before_controlrig_solver_intents_checkpoint", "returned_controlrig_solver_intents_validation")):
                    # Full ordered issues/warnings/validated subset or input rejection belongs to this parent.
                    data = observed["data"]
                    row.update(phase="validation_returned" if observed["stage"].startswith("returned_") else "read_intent",
                        mutation_attempted=False, mutation_performed=False, mutation_state_unknown=False)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessAnimGraphAuthoringTools.set_anim_graph_rigidbody_settings", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_input_pose_passthrough", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_bone_demo", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_modify_curve_demo", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_trail_demo", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_secondary_motion_chain", "CubelessAnimGraphAuthoringTools.set_anim_graph_controlrig_input_defaults", "CubelessAnimGraphAuthoringTools.ensure_controlrig_forced_driver_animbp", "CubelessAnimGraphAuthoringTools.ensure_anim_graph_controlrig_node_sample", "CubelessAnimGraphAuthoringTools.ensure_controlrig_sample_asset", "CubelessAnimGraphAuthoringTools.create_anim_blueprint_sample")
                        and observed["stage"] == "returned_anim_graph_authoring_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Complete ordered field/pin/node/link/current/partial prefix precedes criteria.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessPhysicsAssetInspectionTools.inspect_physics_asset_constraints_guarded"
                        and observed["stage"] == "returned_physics_asset_inspection_observation"
                        and observed["data"].get("native_result_returned") is True):
                    # Complete diagnostic/read/load/known-null prefix precedes criteria.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimationAssetMetadataTools.inspect_animation_asset_metadata_guarded"
                        and observed["stage"] == "returned_animation_asset_metadata_observation"
                        and observed["data"].get("native_result_returned") is True):
                    # Full diagnostic/read/load/known-null prefix precedes success and failure criteria.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessAnimInstanceRuntimeTools.inspect_anim_instance_runtime_state"
                        and observed["stage"] in ("before_anim_instance_runtime_state_checkpoint", "returned_anim_instance_runtime_state_operation")):
                    # Native world/actor/component/AnimInstance partial observation and changed-or-unknown prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_anim_instance_runtime_state_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_anim_instance_runtime_state_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessRigVMPinDefaultWriterTools.set_controlrig_rigvm_pin_defaults"
                        and observed["stage"] in ("before_rigvm_pin_default_writer_checkpoint", "returned_rigvm_pin_default_writer_operation")):
                    # Native writer preflight/partial SetPinDefaultValue/compile/save/changed-or-unknown prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_rigvm_pin_default_writer_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_rigvm_pin_default_writer_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessRigVMLinkWriterTools.add_controlrig_rigvm_links"
                        and observed["stage"] in ("before_rigvm_link_writer_checkpoint", "returned_rigvm_link_writer_operation")):
                    # Native writer preflight/partial AddLink/compile/save/changed-or-unknown prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_rigvm_link_writer_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_rigvm_link_writer_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessRigVMPinDefaultValidationTools.validate_controlrig_rigvm_pin_defaults"
                        and observed["stage"] in ("before_rigvm_pin_default_validation_checkpoint", "returned_rigvm_pin_default_validation_operation")):
                    # Native pin-default proposal diagnostics/counts/dirty/known-null prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_rigvm_pin_default_validation_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_rigvm_pin_default_validation_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessRigVMLinkValidationTools.validate_controlrig_rigvm_links"
                        and observed["stage"] in ("before_rigvm_link_validation_checkpoint", "returned_rigvm_link_validation_operation")):
                    # Native link proposal diagnostics/counts/dirty/known-null prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_rigvm_link_validation_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_rigvm_link_validation_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessRigVMTopologyTools.inspect_controlrig_rigvm_topology"
                        and observed["stage"] in ("before_rigvm_topology_checkpoint", "returned_rigvm_topology_operation")):
                    # Native graph/node/pin/link/events/dirty/known-null prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_rigvm_topology_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_rigvm_topology_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessEnumTools.create_user_defined_enum"
                        and observed["stage"] in ("before_user_defined_enum_checkpoint", "returned_user_defined_enum_operation")):
                    # Native created/entries/MAX/compile/save/unknown prefix precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_user_defined_enum_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_user_defined_enum_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "write_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessEditorTraceTools.line_trace_physical_material"
                        and observed["stage"] in ("before_physical_material_trace_checkpoint", "returned_physical_material_trace_operation")):
                    # Full Native hit/material/callback/unknown receipt precedes terminal errors.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_physical_material_trace_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_physical_material_trace_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessProjectTools.replace_asset_references_mcp"
                        and observed["stage"] in ("before_asset_reference_checkpoint", "returned_asset_reference_operation")):
                    # Native scope/remap/compile/save/unknown receipt precedes terminal criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_asset_reference_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_asset_reference_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "write_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessProjectTools.audit_content_root_mcp"
                        and observed["stage"] in ("before_content_audit_checkpoint", "returned_content_audit_operation")):
                    # Preserve complete raw inventory/report/unknown prefix before criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_content_audit_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_content_audit_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessProjectTools.create_input_mapping"
                        and observed["stage"] in ("before_input_mapping_checkpoint", "returned_input_mapping_operation")):
                    # Persist complete actual RAM/config/raw/save prefix before domain criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_input_mapping_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_input_mapping_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "write_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] in ("CubelessStateTreeTools.add_state_tree_state", "CubelessStateTreeTools.compile_state_tree")
                        and observed["stage"] in ("before_state_tree_mutation_checkpoint", "returned_state_tree_mutation_operation")):
                    # Native owns the composite work. Preserve its actual prefix before criteria.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_state_tree_mutation_operation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_state_tree_mutation_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "write_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessStateTreeTools.create_state_tree"
                        and observed["stage"] == "returned_state_tree_create_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Durable actual compile/save and changed/unknown prefix precedes criteria.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] in ("CubelessStateTreeTools.list_state_trees", "CubelessStateTreeTools.inspect_state_tree")
                        and observed["stage"] in ("before_state_tree_observation_checkpoint", "returned_state_tree_observation")):
                    data = observed["data"]
                    returned = observed["stage"] == "returned_state_tree_observation" and data.get("native_result_returned") is True
                    if observed["stage"] == "before_state_tree_observation_checkpoint" or returned:
                        attempted = data.get("native_mutation_attempted")
                        row.update(phase="engine_returned" if returned else "read_intent",
                            mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                            mutation_performed=data.get("mutation_performed"),
                            mutation_state_unknown=data.get("mutation_state_unknown") is True)
                        _aggregate(result, summary)
                        persist(f"batch[{index}]:observed:{observed['stage']}")
                        return

                if (row["type"] == "CubelessAnimStateMachineInspectionTools.inspect_anim_state_machine_transitions"
                        and observed["stage"] == "returned_anim_state_machine_observation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve returned read/load/current prefix before criteria; no generic writer intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return

                if (row["type"] == "CubelessBlueprintWidgetFallbackTools.analyze_blueprint_widget_fallbacks_mcp"
                        and observed["stage"] == "returned_blueprint_widget_fallback_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Diagnostic/report/known-null prefix is retained before terminal errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessUMGTextBindingTools.set_text_block_binding"
                        and observed["stage"] == "returned_umg_text_binding_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Full source/property-path/binding/compiler/save or unknown prefix precedes errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessUMGUtilityTools.add_widget_to_viewport"
                        and observed["stage"] == "before_read_checkpoint"):
                    # Class-readiness intent must not enter generic writer intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessUMGUtilityTools.add_checkbox_to_widget", "CubelessUMGUtilityTools.add_widget_to_viewport")
                        and observed["stage"] == "returned_umg_utility_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual checkbox writer or viewport class-readiness prefix precedes errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessUMGInteractionTools.add_sliders_to_widget", "CubelessUMGInteractionTools.bind_widget_event")
                        and observed["stage"] == "returned_umg_interaction_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual Native slider/event/each compiler and save prefixes precede errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessUMGAuthoringTools.create_umg_widget_blueprint", "CubelessUMGAuthoringTools.add_text_block_to_widget", "CubelessUMGAuthoringTools.add_button_to_widget")
                        and observed["stage"] == "returned_umg_authoring_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Native leaf compiler/save and tree mutation prefixes precede errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessAnimGraphInspectionTools.inspect_anim_graph_node_settings", "CubelessAnimGraphInspectionTools.inspect_anim_graph_protected_topology")
                        and observed["stage"] == "returned_anim_graph_inspection_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Normal authored read or implicit default graph creation prefix precedes errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintComponentReferenceTools.add_blueprint_get_self_component_reference"
                        and observed["stage"] == "returned_blueprint_component_reference_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Permissive getter/default graph creation prefix is retained before errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintGraphTools.list_blueprint_nodes", "CubelessBlueprintGraphTools.delete_blueprint_node")
                        and observed["stage"] in ("before_read_checkpoint", "returned_blueprint_node_cleanup_operation")):
                    data = observed["data"]
                    returned = observed["stage"] == "returned_blueprint_node_cleanup_operation" and data.get("native_result_returned") is True
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if returned else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"), mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTopologyTools.inspect_blueprint_graph_call_topology"
                        and observed["stage"] in ("before_read_checkpoint", "returned_blueprint_topology_operation")):
                    # Read intent/returned SDK observation is not synthetic writer intent.
                    data = observed["data"]
                    returned = observed["stage"] == "returned_blueprint_topology_operation" and data.get("native_result_returned") is True
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned" if returned else "read_intent",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintQueryLayoutTools.find_blueprint_nodes", "CubelessBlueprintQueryLayoutTools.set_blueprint_node_positions")
                        and observed["stage"] == "returned_blueprint_query_layout_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Original graph creation/layout/current/compiler/save prefix precedes parent errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintNetworkingTools.inspect_blueprint_networking_settings", "CubelessBlueprintNetworkingTools.set_blueprint_networking_settings")
                        and observed["stage"] == "returned_blueprint_networking_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual CDO/current/compile/forced-save/audit prefix precedes parent errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintGraphTransferTools.import_blueprint_graph_nodes_from_text", "CubelessBlueprintGraphTransferTools.repair_blueprint_pin_links")
                        and observed["stage"] == "returned_graph_transfer_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual composite import/repair/compiler/forced-save prefix precedes parent errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintBindingTools.add_blueprint_native_delegate_bind_node", "CubelessBlueprintBindingTools.add_blueprint_create_delegate_node", "CubelessBlueprintBindingTools.add_blueprint_interface_message_node")
                        and observed["stage"] == "returned_blueprint_binding_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual property/function/links/only-new-node cleanup prefix precedes errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if ((row["type"] in ("CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_unbind_node", "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_clear_node", "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_assign_node")
                        and observed["stage"] == "returned_blueprint_dispatcher_lifecycle_operation")
                        or (row["type"] in ("CubelessBlueprintEventTools.add_blueprint_custom_event_node", "CubelessBlueprintGraphTools.add_blueprint_custom_event_node")
                        and observed["stage"] == "returned_custom_event_signatures_operation")) and observed["data"].get("native_result_returned") is True:
                    # Native main/secondary event/signature/link/void prefix precedes validation.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher", "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_call_node", "CubelessBlueprintDispatcherTools.add_blueprint_event_dispatcher_bind_node")
                        and observed["stage"] == "returned_blueprint_dispatcher_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual declaration/property/node/cleanup prefix precedes parent validation.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintAnnotationTools.add_blueprint_comment_box", "CubelessBlueprintAnnotationTools.set_blueprint_node_comment", "CubelessBlueprintAnnotationTools.set_blueprint_function_description")
                        and observed["stage"] == "returned_blueprint_annotation_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual grouping/style/text/implicit structural prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintExpressionTools.add_blueprint_input_axis_event_node", "CubelessBlueprintExpressionTools.add_blueprint_enhanced_input_action_node")
                        and observed["stage"] == "returned_blueprint_input_events_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual flags/action load/graph/lifecycle prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintExpressionTools.add_blueprint_get_editor_subsystem_node"
                        and observed["stage"] == "returned_blueprint_editor_subsystem_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual utility owner/class/node/source prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintExpressionTools.add_blueprint_input_action_node", "CubelessBlueprintExpressionTools.add_blueprint_self_reference")
                        and observed["stage"] == "returned_blueprint_input_self_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual input-name/node/owner/lifecycle/source prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintExpressionTools.add_blueprint_get_subsystem_from_pc_node", "CubelessBlueprintExpressionTools.add_blueprint_construct_object_node")
                        and observed["stage"] == "returned_blueprint_construction_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual class/default/restoration/retirement/source prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintSelectionTools.add_blueprint_enum_literal_node", "CubelessBlueprintSelectionTools.add_blueprint_is_valid_node", "CubelessBlueprintSelectionTools.add_blueprint_switch_int_node")
                        and observed["stage"] == "returned_blueprint_branching_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual enum/default/case/allocation/retirement/source prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintExpressionTools.add_blueprint_compare_node", "CubelessBlueprintExpressionTools.add_blueprint_literal_node")
                        and observed["stage"] == "returned_blueprint_value_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual allocation/default/retirement/current-state prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintExpressionTools.add_blueprint_math_node", "CubelessBlueprintExpressionTools.add_blueprint_boolean_node")
                        and observed["stage"] == "returned_blueprint_expression_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual function-core/node/alias prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.expand_material_function_calls"
                        and observed["stage"] == "returned_material_function_expansion_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual passes/composite/rollback/save prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessBlueprintSelectionTools.add_blueprint_select_node", "CubelessBlueprintSelectionTools.add_blueprint_switch_enum_node")
                        and observed["stage"] == "returned_blueprint_selection_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual call-core/enum/node/retirement prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.set_material_parameter_collection_sync"
                        and observed["stage"] == "returned_material_collection_sync_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual controller/initial-copy/stop/save point precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.refresh_material_cached_expression_data"
                        and observed["stage"] == "returned_material_cache_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual cache/compiler/dependency/save prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.set_material_texture_sample_sampler_type_mcp"
                        and observed["stage"] == "returned_material_sampler_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual sampler/compiler/direct-save prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.replace_material_collection_parameters"
                        and observed["stage"] == "returned_material_collection_parameters_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Runtime-instance/graph/compiler/save prefix precedes generic parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessMaterialTools.replace_material_collection_references", "CubelessMaterialTools.replace_material_texture_references")
                        and observed["stage"] == "returned_material_reference_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual reference/compiler/save/unknown prefix precedes parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessSkeletalPhysicsTools.create_skeletal_bone_constraint_fixture", "CubelessSkeletalPhysicsTools.inspect_skeletal_bone_constraint_fixture")
                        and observed["stage"] == "returned_skeletal_constraint_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve actual transient/point-read/unknown prefix before parent intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSkeletalPhysicsTools.create_skeletal_physics_asset"
                        and observed["stage"] == "returned_skeletal_physics_asset_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual dry/generator/source/cleanup/save prefix precedes intent; no nested journal or second save.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSkeletalTools.set_bone_translation_retargeting"
                        and observed["stage"] == "returned_skeletal_retarget_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve actual setter/load/save and known/unknown prefix before intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in ("CubelessSkeletalTools.add_skeletal_socket", "CubelessSkeletalTools.set_skeletal_socket", "CubelessSkeletalTools.remove_skeletal_socket")
                        and observed["stage"] == "returned_skeletal_socket_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Actual ordered load/Modify/array/save and unknown return prefix precedes intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessNiagaraModuleTools.set_niagara_module_inputs_batch"
                        and observed["stage"] == "returned_niagara_module_inputs_batch_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve the full ordered RI/compile/save/known/unknown
                    # native prefix before parent dispatch intent; no resubmit.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessLocalVariableTools.add_blueprint_local_variable"
                        and observed["stage"] == "returned_local_variable_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve complete real local-declaration/default/source and unknown
                    # return prefixes before generic dispatch intent; no replay.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessFunctionNodeTools.add_blueprint_function_node"
                        and observed["stage"] == "returned_function_node_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve complete real node/core/default/source and unknown
                    # return prefixes before generic dispatch intent; no replay.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTools.set_pawn_properties"
                        and observed["stage"] == "returned_pawn_properties_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve ordered partial and known/unknown returned CDO
                    # prefixes before generic dispatch intent; no second save.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintGraphTools.set_blueprint_custom_event_metadata"
                        and observed["stage"] == "returned_custom_event_metadata_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve reconstruction/compile/generated-function/save
                    # prefixes and incomplete raw returns before generic intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTools.set_blueprint_category_sorting"
                        and observed["stage"] == "returned_category_sorting_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Matching dirty order can perform actual forced save with changed=False.
                    # Keep returned known/unknown/save prefixes before generic intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTools.remove_blueprint_variable"
                        and observed["stage"] == "returned_variable_removal_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve native actual deletion/structural prefix, including
                    # known failure or incomplete raw results, before generic intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessBlueprintTools.set_blueprint_variable_metadata", "CubelessBlueprintTools.set_blueprint_variable_order")
                        and observed["stage"] == "returned_variable_lifecycle_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Metadata helper work and order's matching/changed compile-save
                    # prefix are native-authoritative. Preserve returned raw unknown
                    # or saved failure before generic intent accounting; no second journal/save.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessDataAssetTools.get_data_asset", "CubelessDataAssetTools.set_data_asset_properties",
                        "CubelessDataAssetTools.create_data_asset")
                        and observed["stage"] == "returned_data_asset_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Native owns initial loads and actual partial/matching/
                    # empty property work plus requested save0/1. Preserve raw
                    # known/unknown prefixes before any terminal error; the
                    # original performed contract remains separately nested.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintGraphTools.add_blueprint_make_struct_node"
                        and observed["stage"] == "returned_make_struct_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve only this returned native helper-call producer.
                    # Known0, partial/unknown graph edits and raw unparsed
                    # receipts must not become generic pre-dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessMaterialTools.compile_and_save_material"
                        and observed["stage"] == "returned_material_compilation_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Background pending/Function fanout can remain unknown,
                    # and a final failure can retain a real saved1. Keep this
                    # exact returned native prefix before generic intent edits.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSequencerTools.open_level_sequence_in_editor"
                        and observed["stage"] == "returned_sequencer_editor_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # UI dispatch/effects are not asset mutation proof. Preserve
                    # actual admitted call/unknown prefix before generic intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSequencerTools.add_level_sequence_camera_cut"
                        and observed["stage"] == "returned_sequencer_camera_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Native owns the optional save and retains actual camera/
                    # section/source prefix. Never rewrite known0, unknown or
                    # saved failure into generic dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSequencerTools.add_level_sequence_subsequence"
                        and observed["stage"] == "returned_sequencer_subsequence_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Native owns the optional save and retains actual parent/
                    # child/section prefix. Never rewrite known0, unknown or
                    # saved failure into generic dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessSequencerTools.set_level_sequence_playback_range",
                        "CubelessSequencerTools.remove_level_sequence_binding")
                        and observed["stage"] == "returned_sequencer_maintenance_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # These exact native producers own requested save and
                    # original terminal verification. A failed decimal/removal
                    # postcondition can retain real writes and a successful
                    # save; preserve actual known/unknown/counts/raw receipts.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessSequencerTools.add_level_sequence_float_property_keys"
                        and observed["stage"] == "returned_sequencer_float_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Native insertion is already returned, even when this
                    # checkpoint also contains a pending or returned domain
                    # exact save. Preserve actual prefix/counts/save receipts;
                    # never rewrite previous changes into dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessDataTableTools.get_datatable_schema", "CubelessDataTableTools.list_datatable_rows",
                        "CubelessDataTableTools.get_datatable_row", "CubelessDataTableTools.add_or_update_datatable_row",
                        "CubelessDataTableTools.remove_datatable_row", "CubelessDataTableTools.create_datatable")
                        and observed["stage"] == "returned_datatable_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Exact returned DataTable prefix, including loaded reads,
                    # matching writes, partial import/removal and saved results.
                    # Never rewrite actual outcomes into pre-dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTools.ensure_blueprint_rigid_constraint_chain"
                        and observed["stage"] == "returned_rigid_constraint_chain_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Only this exact chain producer. Preserve returned source
                    # loads, allocation/setter/structural/compile/save prefix,
                    # including known-zero or malformed-raw unknown, before errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessBlueprintTools.replace_blueprint_component_class"
                        and observed["stage"] == "returned_component_replacement_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Only this exact replacement producer. Retain actual
                    # preview/retirement/compile/forced-save prefix, including
                    # known-zero or malformed-raw unknown, before errors.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessBlueprintTools.set_blueprint_actor_tick_settings",
                        "CubelessBlueprintTools.set_skeletal_mesh_component_anim_defaults")
                        and observed["stage"] == "returned_blueprint_defaults_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # These two native producers own actual compile/save and
                    # defaults prefixes. Keep returned known-zero, known
                    # partial or malformed-raw unknown observations durable;
                    # never rewrite them into pre-dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessAudioTools.list_audio_assets", "CubelessAudioTools.inspect_audio_asset",
                        "CubelessAudioTools.create_sound_cue_from_wave", "CubelessAudioTools.create_basic_metasound_source")
                        and observed["stage"].startswith("returned_audio_")):
                    # Exact Audio returned prefix only. Preserve actual cache-
                    # scope/known-zero or save/readback results, including raw
                    # malformed-result unknown, before any later error. Never
                    # rewrite returned save receipts into pre-dispatch intent.
                    data = observed["data"]
                    attempted = data.get("native_mutation_attempted")
                    row.update(phase="engine_returned",
                        mutation_attempted=attempted if type(attempted) is bool else row["mutation_attempted"],
                        mutation_performed=data.get("mutation_performed"),
                        mutation_state_unknown=data.get("mutation_state_unknown") is True)
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessAITools.remove_ai_blackboard_key"
                        and observed["stage"] == "returned_ai_blackboard_remove_operation"
                        and observed["data"].get("native_result_returned") is True):
                    row.update(phase="engine_returned",
                        mutation_attempted=observed["data"]["native_mutation_attempted"],
                        mutation_performed=observed["data"]["mutation_performed"],
                        mutation_state_unknown=observed["data"]["mutation_state_unknown"])
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessAITools.create_ai_patrol_chase_behavior_assets"
                        and observed["stage"] == "returned_ai_behavior_assets_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Exact new multi-asset producer only. This is a returned
                    # observation, including known-zero rejection or partial
                    # ordered saves, so do not rewrite it as dispatch intent.
                    row.update(phase="engine_returned",
                        mutation_attempted=observed["data"]["native_mutation_attempted"],
                        mutation_performed=observed["data"]["mutation_performed"],
                        mutation_state_unknown=observed["data"]["mutation_state_unknown"])
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] == "CubelessAITools.configure_ai_controller_perception"
                        and observed["stage"] == "returned_ai_perception_operation"
                        and observed["data"].get("native_result_returned") is True):
                    # Preserve this new producer's actual compile/save/config
                    # prefix or known-zero refusal as returned observations.
                    row.update(phase="engine_returned",
                        mutation_attempted=observed["data"]["native_mutation_attempted"],
                        mutation_performed=observed["data"]["mutation_performed"],
                        mutation_state_unknown=observed["data"]["mutation_state_unknown"])
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                if (row["type"] in (
                        "CubelessSequencerTools.create_level_sequence",
                        "CubelessSequencerTools.add_level_sequence_actor_binding",
                        "CubelessSequencerTools.add_level_sequence_transform_keys")
                        and observed["stage"].startswith("returned_")
                        and observed["data"].get("native_outcomes")):
                    # Sequencer also checkpoints a returned native observation
                    # before a later exact save. Preserve its known-zero or
                    # partial/unknown prefix; this checkpoint is not dispatch.
                    row.update(phase="engine_returned",
                        mutation_attempted=observed["data"]["mutation_attempted"],
                        mutation_performed=observed["data"]["mutation_performed"],
                        mutation_state_unknown=observed["data"]["mutation_state_unknown"])
                    _aggregate(result, summary)
                    persist(f"batch[{index}]:observed:{observed['stage']}")
                    return
                row["phase"] = "mutation_intent"
                # The core prepares its legacy attempted fields immediately
                # before this callback. The durable snapshot is only intent;
                # a failed checkpoint must not claim engine dispatch happened.
                for change in observed["changes"]:
                    change["attempted"] = False
                    change["intent_prepared"] = True
                compile_state = copy.deepcopy(observed["compile"])
                save_state = copy.deepcopy(observed["save"])
                if observed["compile"]["requested"]:
                    observed["compile"].update(attempted=False, status="intent_prepared_not_dispatched")
                if observed["save"]["requested"]:
                    observed["save"].update(attempted=False, saved=False, status="intent_prepared_not_dispatched")
                _aggregate(result, summary)
                persist(f"batch[{index}]:before:{observed['stage']}")
                for change in observed["changes"]:
                    change["attempted"] = True
                observed["compile"].update(compile_state)
                observed["save"].update(save_state)
                row["mutation_attempted"] = True
                row["phase"] = "engine_call_or_readback"

            try:
                persist(f"batch[{index}]:step_intent")
                if not runtime_query_only:
                    ensure_editor_idle()
                summary["dispatched_count"] += 1
                row["phase"] = "action_preflight"
                arguments = {**command["params"], **capability["fixed"]}
                if _compile_status_owner is not None:
                    _compile_status_batch_current(_compile_status_owner)
                if _compile_status_owner is not None and _compile_status_batch_wait(command):
                    if _factory_batch_command(command):
                        from . import niagara_factory_bootstrap
                        flow = niagara_factory_bootstrap._BatchFactoryBootstrapFlow(child, arguments, before_mutation,
                            command["type"].rsplit(".", 1)[-1], lambda: _compile_status_batch_current(_compile_status_owner))
                    else:
                        from . import niagara_authoring_inspection
                        flow = niagara_authoring_inspection._BatchCompileStatusFlow(child, arguments, before_mutation)
                    _compile_status_owner["flow"] = flow  # Retain before the first SDK boundary.
                    try:
                        if not flow.start(True):
                            row["phase"] = "awaiting_owned_HN_factory_bootstrap" if _factory_batch_command(command) else "awaiting_owned_HN_compile_status"
                            persist(f"batch[{index}]:pending_compile_status")
                            paused_at = time.perf_counter()
                            try:
                                yield flow
                            finally:
                                suspended_seconds += time.perf_counter() - paused_at
                                summary["compile_status_continuation"]["HN_wait_seconds"] = round(suspended_seconds, 6)
                    except Exception as error:
                        flow.fail(error)
                        raise
                else:
                    capability["action"](child, **arguments, checkpoint_callback=before_mutation)
                if _compile_status_owner is not None:
                    _compile_status_batch_current(_compile_status_owner)
            except Exception as error:
                if _compile_status_owner is not None and _compile_status_owner.get("parent_context_lost"):
                    child["data"].update(change_state_known=False, mutation_state_unknown=True,
                        parent_receiver_or_context_lost=True, do_not_retry=True)
                fail(child, error)
            row["elapsed_seconds"] = round(time.perf_counter() - child_started, 6)
            row["phase"] = "returned"
            _observation(row)
            if _compile_status_owner is not None:
                active_flow = _compile_status_owner.get("flow")
                if (active_flow is not None and active_flow.r is child
                        and not active_flow.in_flight and not active_flow.lost):
                    # A later child may legitimately edit this target. Retain
                    # only a still-pending/unsafe HN flow, never rewrite an
                    # already-completed child on later lifecycle retirement.
                    _compile_status_owner["flow"] = None
            if not row.get("terminal_counted_by_compile_status_lifecycle", False):
                summary["succeeded" if child["success"] else "failed"] += 1
            _aggregate(result, summary)
            completed_observation = _completed_skeletal_sampling_observation(row)
            if completed_observation:
                row["completed_nonmutating_observation"] = True
                row["total_SDK_effects_unknown_preserved"] = row["mutation_state_unknown"]
                row["continuation_scope"] = "verified_completed_FC_public_getters_only; no retry or atomic-purity claim"
            forced = (row["mutation_state_unknown"] and not completed_observation) or child["error_code"].startswith("journal_") or child["error_code"] == "play_session_active"
            if forced or (not child["success"] and stop_on_error):
                reason = child["error_code"] or ("mutation_outcome_unknown" if row["mutation_state_unknown"] else "child_failed")
                summary.update(forced_stop=bool(forced), stop_reason=reason, stopped_at_index=index)
            try:
                persist(f"batch[{index}]:result")
            except Exception as error:
                summary.update(forced_stop=True, stop_reason="journal_checkpoint_failed", stopped_at_index=index)
                fail(result, error)
                break
            if forced or (not child["success"] and stop_on_error):
                break
            # Keep evidence even if the last bounded action produces a large
            # result, but do not add further actions. This is not silent truncation.
            if index + 1 < len(commands) and len(json.dumps(result, ensure_ascii=False).encode("utf-8")) > MAX_OBSERVED_RESULT_BYTES:
                summary.update(forced_stop=True, stop_reason="observed_result_size_limit", stopped_at_index=index)
                break

    def finish_steps():
        # Content-only explicit writers own native transactions for guarded cleanup.
        # Mixed writers retain the existing outer scope and native safety refusal.
        if (summary["transaction"]["scope_requested"] and not summary["transaction"]["native_owned_content_only"]
                and not summary["transaction"].get("batch_scope_skipped_for_python_execution", False)
                and not summary["transaction"].get("batch_scope_skipped_for_actor_operations", False)
                and _compile_status_owner is None):
            with engine.transaction():
                summary["transaction"]["batch_scope_entered"] = True
                yield from run_steps()
        else:
            yield from run_steps()
        summary["elapsed_seconds"] = round(time.perf_counter() - started, 6)
        summary["success"] = summary["failed"] == 0 and not summary["not_run"] and not summary["forced_stop"]
        result["success"] = summary["success"]
        result["validation"]["status"] = "all_child_postconditions_passed" if result["success"] else "batch_incomplete_or_failed"
        result["stage"] = "complete" if result["success"] else "batch_stopped" if summary["stop_reason"] else "batch_failed"
        _aggregate(result, summary)
        if not result["success"] and not result["error_code"]:
            fail(result, OperationError("batch_incomplete" if summary["not_run"] else "batch_child_failed",
                                         summary["stop_reason"] or f"{summary['failed']} child actions failed"))

    continuation = finish_steps()
    if _compile_status_owner is not None:
        return continuation
    # Original synchronous callers exhaust the same loop immediately. Only the
    # exact official compile-status route can yield an HN continuation.
    for pending in continuation:
        raise OperationError("batch_unexpected_pending", "Synchronous Batch cannot hold a pending Engine job")


def execute_batch(commands_json, stop_on_error=True, operation_id=None):
    try:
        commands = _normalize(commands_json, stop_on_error)
    except Exception as error:
        result = fail(new_result(operation_id), error)
        result["data"] = {"admission_rejected": True, "actions_executed": 0,
                          "supported_commands": sorted(_CAPABILITIES), "manifest_version": MANIFEST_VERSION}
        return result
    arguments = {"manifest_version": MANIFEST_VERSION, "commands": commands, "stop_on_error": stop_on_error}
    result = mutate("execute_batch", arguments, operation_id, lambda output: _run(output, commands, stop_on_error))
    if not result["success"] and "batch" in result["data"]:
        result["data"]["batch"]["success"] = False
    return result


def _factory_batch_command(command):
    return command["type"] in (
        "CubelessNiagaraFactoryBootstrapTools.create_empty_niagara_system",
        "CubelessNiagaraFactoryBootstrapTools.create_basic_sprite_niagara_system")


def _compile_status_batch_wait(command):
    factory = _factory_batch_command(command)
    if not factory and command["type"] != "CubelessNiagaraAuthoringInspectionTools.inspect_niagara_compile_status":
        return False
    parameters = command["params"]
    raw = parameters.get("_legacy_command_params")
    default = factory  # Original factory2 GetBoolParam defaults wait to True.
    value = raw.get("wait_for_completion", default) if type(raw) is dict else parameters.get("wait_for_completion", default)
    # UE TryGetBool accepts booleans, nonzero numbers and FString::ToBool.
    # All strings take the owned job route without replacing their exact value.
    # Unconvertible raw null/object/array retains the Native default (factory=True,
    # inspector=False); never silently replace it with Python truthiness.
    if type(value) is bool:
        return value
    if type(value) in (int, float):
        return value != 0
    if type(raw) is dict:
        return type(value) is str or default
    return False


# VD: only the existing compile-status HN workflow may suspend this parent.
# The existing compile-status ticker owns scheduling; there is no public job
# protocol, worker thread, new child journal or generic async action scheduler.
_COMPILE_STATUS_BATCH_ACTIVE = False
_COMPILE_STATUS_BATCH_INSPECTION_ACTIVE = False
_COMPILE_STATUS_BATCH_FACTORY_ACTIVE = False
_COMPILE_STATUS_BATCH_ENTRY = None
_COMPILE_STATUS_BATCH_RETAINED = None


class _CompileStatusBatchParentJournal:
    def __init__(self, commands, stop_on_error, operation_id):
        import hashlib
        import os
        from . import state
        self.result = new_result(operation_id)
        self.path = state._journal_path(self.result["operation_id"])
        self.owned = False
        self.final_entered = False
        self.replay = None
        arguments = {"manifest_version": MANIFEST_VERSION, "commands": commands, "stop_on_error": stop_on_error}
        fingerprint = hashlib.sha256(json.dumps({"name": "execute_batch", "arguments": arguments}, sort_keys=True).encode("utf-8")).hexdigest()
        if self.path.is_file():
            existing = state.get_operation(self.result["operation_id"])
            if existing.get("fingerprint") != fingerprint:
                raise OperationError("operation_id_conflict", "This operation_id already belongs to different arguments")
            if existing.get("state") != "complete":
                raise OperationError("operation_incomplete", "Previous Batch may still own changed assets or an Engine job; no resume/replay")
            self.replay = copy.deepcopy(existing["result"])
            self.replay["retry"]["replayed"] = True
            return
        if _COMPILE_STATUS_BATCH_ENTRY is not None or _COMPILE_STATUS_BATCH_RETAINED is not None:
            raise OperationError("batch_compile_status_retained", "Exact compile-status Batch ownership must be unowned before a new journal")
        if state._active_operation is not None:
            raise OperationError("mutation_busy", "Another Cubeless Python mutation is active: " + str(state._active_operation))
        record = dict(schema_version=1, operation_id=self.result["operation_id"], operation="execute_batch",
            arguments=arguments, fingerprint=fingerprint, state="started", result=self.result,
            engine_version=state.unreal.SystemLibrary.get_engine_version(), process_id=os.getpid())
        encoded = json.dumps(record, ensure_ascii=False, sort_keys=True)
        self.path.parent.mkdir(parents=True, exist_ok=True)
        with self.path.open("x", encoding="utf-8") as stream:
            stream.write(encoded); stream.flush(); os.fsync(stream.fileno())
        state._active_operation = self.result["operation_id"]
        self.owned = True

    def finish(self, recovery):
        import os
        from . import state
        if self.final_entered:
            raise OperationError("journal_finalize_repeated", "Batch finalization is not retried")
        self.final_entered = True
        with self.path.open(encoding="utf-8") as stream:
            record = json.load(stream)
        if (not self.owned or state._active_operation != self.result["operation_id"]
                or record.get("operation_id") != self.result["operation_id"]
                or record.get("state") != "started" or record.get("process_id") != os.getpid()):
            raise OperationError("journal_finalize_not_owned", "Exact parent Batch journal/slot changed; no final delivery")
        record.update(result=copy.deepcopy(self.result), state="started" if recovery else "complete")
        state._write_record(self.path, record)
        if not recovery:
            state._active_operation = None
            self.owned = False


def compile_status_batch_status(factory=False):
    def belongs(entry):
        return entry is not None and entry.get("requires_factory" if factory else "requires_inspection", not factory)
    return dict(active=_COMPILE_STATUS_BATCH_FACTORY_ACTIVE if factory else _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE,
        pending_count=int(belongs(_COMPILE_STATUS_BATCH_ENTRY)),
        quarantined_count=int(belongs(_COMPILE_STATUS_BATCH_RETAINED)), parent_journal_only=True,
        scheduling="existing_exact_inspection_or_factory_ticker", general_async_framework=False)


def activate_compile_status_batches(factory=False):
    global _COMPILE_STATUS_BATCH_ACTIVE, _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE, _COMPILE_STATUS_BATCH_FACTORY_ACTIVE
    active = _COMPILE_STATUS_BATCH_FACTORY_ACTIVE if factory else _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE
    if not active:
        if _COMPILE_STATUS_BATCH_ENTRY is not None or _COMPILE_STATUS_BATCH_RETAINED is not None:
            raise OperationError("batch_compile_status_retained", "Reconcile retained Batch before lifecycle activation")
        if factory:
            _COMPILE_STATUS_BATCH_FACTORY_ACTIVE = True
        else:
            _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE = True
        _COMPILE_STATUS_BATCH_ACTIVE = True
    return compile_status_batch_status(factory)


def _compile_status_batch_current(entry):
    from . import state
    try:
        if (entry.get("parent_context_lost") or not _COMPILE_STATUS_BATCH_ACTIVE
                or _COMPILE_STATUS_BATCH_ENTRY is not entry):
            raise OperationError("batch_compile_status_lease_lost", "Exact parent lifecycle/entry lease changed; no next Engine action")
        expected_slot = None if entry.get("journal_finalized") else entry["result"]["operation_id"]
        if state._active_operation != expected_slot:
            raise OperationError("batch_compile_status_slot_lost", "Exact parent operation slot changed; no next Engine action")
        if (entry.get("requires_inspection", True) and not _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE
                or entry.get("requires_factory", False) and not _COMPILE_STATUS_BATCH_FACTORY_ACTIVE):
            raise OperationError("batch_compile_status_domain_lost", "Required exact inspection/factory lifecycle retired; no next action")
        context = entry.get("context")
        if context is None:
            raise OperationError("batch_compile_status_context_missing", "Parent Editor/world/receiver lease was not captured")
        context.admit()  # Context and output anchor only; no source/target asset snapshot.
        if (not _COMPILE_STATUS_BATCH_ACTIVE or _COMPILE_STATUS_BATCH_ENTRY is not entry
                or state._active_operation != expected_slot
                or entry.get("requires_inspection", True) and not _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE
                or entry.get("requires_factory", False) and not _COMPILE_STATUS_BATCH_FACTORY_ACTIVE):
            raise OperationError("batch_compile_status_lease_lost", "Parent lease changed during context observation")
    except Exception as error:
        entry["parent_context_lost"] = True
        entry["result"]["data"].update(mutation_state_unknown=True, change_state_known=False,
            parent_receiver_or_context_lost=True, do_not_retry=True)
        entry["result"]["data"].setdefault("first_parent_context_failure", dict(
            stage=entry["result"]["stage"], error=str(error), error_code=getattr(error, "code", "")))
        raise


def _finish_compile_status_batch(entry):
    global _COMPILE_STATUS_BATCH_ENTRY, _COMPILE_STATUS_BATCH_RETAINED
    if entry.get("delivery_entered"):
        return
    entry["delivery_entered"] = True
    from . import state
    from ..registration.results import reflected_result
    result = entry["result"]
    summary = result["data"].get("batch", {})
    flow = entry.get("flow")
    lost = bool(flow is not None and flow.lost) or entry.get("parent_context_lost", False)
    if not lost:
        try:
            _compile_status_batch_current(entry)
        except Exception as error:
            lost = True
            fail(result, error)
            summary["success"] = False
    recovery = bool(lost or flow is not None and flow.in_flight or summary.get("mutation_state_unknown")
        or str(summary.get("stop_reason", "")).startswith("journal_") or result["error_code"].startswith("journal_"))
    result["data"]["compile_status_batch_parent"] = dict(parent_journal_only=True, child_journals_created=0,
        recovery_required=recovery, asynchronous_wait=True, outer_transaction_across_frames=False)
    try:
        entry["journal"].finish(recovery)
        entry["journal_finalized"] = not recovery
    except Exception as error:
        recovery = True
        entry["journal_error"] = str(error)
        fail(result, OperationError("journal_finalize_failed", "Batch completion was not durably finalized: " + str(error)))
        summary["success"] = False
    parent_metadata = result["data"]["compile_status_batch_parent"]
    parent_metadata.update(recovery_required=recovery, engine_completion_durable=entry.get("journal_finalized") is True,
        engine_completion_success=result["success"] if entry.get("journal_finalized") is True else None,
        delivery_failure=False)
    if lost:
        result["data"].update(change_state_known=False, mutation_state_unknown=True, do_not_retry=True)
        summary["success"] = False
        # Retain metadata and the started journal. No CU or Job getter is safe
        # after the exact source/context lease was lost.
        entry["metadata_only_quarantine"] = True
    else:
        try:
            _compile_status_batch_current(entry)
            projected = reflected_result(copy.deepcopy(result))
            _compile_status_batch_current(entry)
            entry["output"].set_editor_property("value", projected)
            _compile_status_batch_current(entry)
            entry["completion_entered"] = True
            if entry["output"].complete_value() is not True:
                raise OperationError("batch_compile_status_delivery_failed", "Official Batch completion refused")
        except Exception as error:
            recovery = True
            entry["delivery_error"] = str(error)
            parent_metadata.update(recovery_required=True, delivery_failure=True, delivery_error=str(error),
                delivery_failure_stage="complete_value" if entry.get("completion_entered") else "projection",
                delivery_retried=False)
            # The durable Engine result remains unchanged on disk. This is a
            # separate failed delivery observation, never a fabricated writer
            # failure or a reason to replay the already-completed Batch.
            summary["engine_completion_success_before_delivery"] = parent_metadata["engine_completion_success"]
            summary.update(success=False, delivery_failed=True)
            if not entry.get("completion_entered") and not entry.get("parent_context_lost"):
                try:
                    _compile_status_batch_current(entry)
                    entry["output"].set_error("Batch durable/delivery state uncertain; operation_id=" + result["operation_id"])
                except Exception:
                    pass
    parent_metadata["recovery_required"] = recovery
    if recovery:
        _COMPILE_STATUS_BATCH_RETAINED = entry
    _COMPILE_STATUS_BATCH_ENTRY = None


def _advance_compile_status_batch(entry, error=None):
    try:
        pending = entry["iterator"].throw(error) if error is not None else next(entry["iterator"])
    except StopIteration:
        entry["waiting"] = False
        _finish_compile_status_batch(entry)
        return
    except Exception as failure:
        fail(entry["result"], failure)
        entry["waiting"] = False
        _finish_compile_status_batch(entry)
        return
    if pending is not entry.get("flow"):
        fail(entry["result"], OperationError("batch_compile_status_continuation_changed", "Only the exact retained HN flow may suspend Batch"))
        _finish_compile_status_batch(entry)
        return
    entry["waiting"] = True


def tick_compile_status_batch(delta_seconds, factory=False):
    entry = _COMPILE_STATUS_BATCH_ENTRY
    if not _COMPILE_STATUS_BATCH_ACTIVE or entry is None:
        return
    import threading
    if threading.get_ident() != entry["owner_thread"]:
        flow = entry.get("flow")
        if flow is not None:
            flow.unsafe(OperationError("batch_compile_status_thread_changed", "Batch ticker left its Editor call thread"))
        fail(entry["result"], OperationError("batch_compile_status_thread_changed", "Batch ticker left its Editor call thread"))
        _finish_compile_status_batch(entry)
        return
    if entry.get("waiting"):
        flow = entry["flow"]
        if (flow.method in ("create_empty_niagara_system", "create_basic_sprite_niagara_system")) != factory:
            return  # Only this exact domain ticker may observe the retained job.
        try:
            _compile_status_batch_current(entry)
            complete = flow.poll()  # One owned HN snapshot per Engine tick; no spin/wait.
            _compile_status_batch_current(entry)
        except Exception as error:
            _advance_compile_status_batch(entry, error)
            return
        if complete:
            entry["waiting"] = False
            _advance_compile_status_batch(entry)


def shutdown_compile_status_batches(factory=False):
    global _COMPILE_STATUS_BATCH_ACTIVE, _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE, _COMPILE_STATUS_BATCH_FACTORY_ACTIVE
    global _COMPILE_STATUS_BATCH_ENTRY, _COMPILE_STATUS_BATCH_RETAINED
    if factory:
        _COMPILE_STATUS_BATCH_FACTORY_ACTIVE = False
    else:
        _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE = False
    _COMPILE_STATUS_BATCH_ACTIVE = _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE or _COMPILE_STATUS_BATCH_FACTORY_ACTIVE
    entry = _COMPILE_STATUS_BATCH_ENTRY
    if entry is not None and entry.get("requires_factory" if factory else "requires_inspection", not factory):
        entry["result"]["data"].update(lifecycle_interrupted=True, mutation_state_unknown=True, do_not_retry=True)
        entry["metadata_only_quarantine"] = True
        _COMPILE_STATUS_BATCH_RETAINED = entry
        _COMPILE_STATUS_BATCH_ENTRY = None
    # Engine shutdown: metadata only; no iterator resume/close, Job/asset/CU/save.


def deactivate_compile_status_batches(reason, factory=False):
    global _COMPILE_STATUS_BATCH_ACTIVE, _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE, _COMPILE_STATUS_BATCH_FACTORY_ACTIVE
    if factory:
        _COMPILE_STATUS_BATCH_FACTORY_ACTIVE = False
    else:
        _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE = False
    _COMPILE_STATUS_BATCH_ACTIVE = _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE or _COMPILE_STATUS_BATCH_FACTORY_ACTIVE
    entry = _COMPILE_STATUS_BATCH_ENTRY
    if entry is not None and entry.get("requires_factory" if factory else "requires_inspection", not factory):
        flow = entry.get("flow")
        if flow is not None:
            if flow.job is not None and flow.in_flight and not flow.lost and not entry.get("parent_context_lost") and not flow.cancel_entered:
                flow.cancel_entered = True
                try:
                    context = entry.get("context")
                    if context is None:
                        raise OperationError("batch_compile_status_context_missing", "No context lease for cooperative cancellation")
                    context.admit()  # Explicit lifecycle retirement; no next action is admitted.
                    flow.job.cancel_job(reason)  # Cooperative local job only; not compiler cancellation.
                    context.admit()
                except Exception as error:
                    entry["cancel_error"] = str(error)
                    entry["parent_context_lost"] = True
            flow.fail(flow.unsafe(OperationError("batch_compile_status_lifecycle_interrupted", reason)))
        result = entry["result"]
        fail(result, OperationError("batch_compile_status_lifecycle_interrupted", reason))
        summary = result["data"].get("batch")
        if summary:
            for row in summary["results"]:
                if row["status"] == "pending":
                    fail(row["response"], OperationError("batch_compile_status_lifecycle_interrupted", reason))
                    row["response"]["data"].update(mutation_state_unknown=True, change_state_known=False)
                    _observation(row)
                    summary["failed"] += 1
                    row["terminal_counted_by_compile_status_lifecycle"] = True
                    summary["stopped_at_index"] = row["index"]
            summary.update(success=False, forced_stop=True, stop_reason="batch_compile_status_lifecycle_interrupted")
            _aggregate(result, summary)
        _finish_compile_status_batch(entry)
    return compile_status_batch_status(factory)


def execute_batch_mcp(commands_json, stop_on_error=True, operation_id=None):
    """Existing official schema; exact original inspector/factory waits may suspend."""
    global _COMPILE_STATUS_BATCH_ENTRY
    import threading
    from . import niagara_authoring_inspection
    from ..registration.async_interop_toolset import CubelessPythonAsyncOperationResult
    output = CubelessPythonAsyncOperationResult()
    journal = None
    entry = None
    try:
        try:
            commands = _normalize(commands_json, stop_on_error)
        except Exception:
            # Reuse the original public admission envelope, including supported
            # commands and manifest_version; no Engine action was dispatched.
            return niagara_authoring_inspection.completed_result(execute_batch(commands_json, stop_on_error, operation_id))
        needs_wait = any(_compile_status_batch_wait(c) for c in commands)
        if not needs_wait:
            return niagara_authoring_inspection.completed_result(execute_batch(commands_json, stop_on_error, operation_id))
        requires_factory = any(_factory_batch_command(c) and _compile_status_batch_wait(c) for c in commands)
        requires_inspection = any(not _factory_batch_command(c) and _compile_status_batch_wait(c) for c in commands)
        if requires_factory:
            from . import niagara_factory_bootstrap
        if (not _COMPILE_STATUS_BATCH_ACTIVE
                or requires_inspection and (not _COMPILE_STATUS_BATCH_INSPECTION_ACTIVE or not niagara_authoring_inspection._ACTIVE)
                or requires_factory and (not _COMPILE_STATUS_BATCH_FACTORY_ACTIVE or not niagara_factory_bootstrap._ACTIVE)):
            raise OperationError("batch_compile_status_inactive_or_retained", "Required exact inspection/factory lifecycle must be active and unowned")
        journal = _CompileStatusBatchParentJournal(commands, stop_on_error, operation_id)
        if journal.replay is not None:
            return niagara_authoring_inspection.completed_result(journal.replay)
        entry = dict(output=output, journal=journal, result=journal.result, flow=None,
            owner_thread=threading.get_ident(), waiting=False,
            requires_factory=requires_factory, requires_inspection=requires_inspection)
        _COMPILE_STATUS_BATCH_ENTRY = entry
        # Reuse the existing public HF context/anchor for only parent Editor,
        # world and CU receiver identity. Assets are owned by their child APIs.
        try:
            context_receipt = journal.result["data"].setdefault("compile_status_parent_context", {})
            from ..compatibility.ue58 import pcg_utilities as parent_context_api
            entry["context"] = parent_context_api._Context(context_receipt, None)
            entry["context"].pin(output)
            _compile_status_batch_current(entry)
        except Exception:
            entry["parent_context_lost"] = True
            raise
        entry["iterator"] = _run(journal.result, commands, stop_on_error, _compile_status_owner=entry)
        _advance_compile_status_batch(entry)
    except Exception as error:
        if entry is not None:
            fail(entry["result"], error)
            _finish_compile_status_batch(entry)
        else:
            result = fail(new_result(operation_id), error)
            result["data"].update(admission_rejected=True, actions_executed=0)
            return niagara_authoring_inspection.completed_result(result)
    return output
