"""HN original Scratch/User recipes4 with one parent and finite original leaf waits.
No RPC, whole-work replay, blocking wait or independent child journal.
"""
from __future__ import annotations
import copy,hashlib,json,logging,math,os,re,time,uuid
from pathlib import Path
from typing import Any,Dict,Literal
import unreal
from . import state
from .state import OperationError
from cubeless_mcp_ue58.compatibility.ue58 import niagara_scratch_user_recipes as api
from cubeless_mcp_ue58.registration.async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.registration.results import reflected_result
logger=logging.getLogger(__name__)
METHODS=['create_niagara_scratch_pad_vec2_user_parameter_module', 'create_niagara_scratch_pad_color_user_parameter_module', 'create_niagara_scratch_pad_bool_user_parameter_module', 'create_niagara_scratch_pad_bool_visible_toggle_module', 'create_niagara_visible_scratch_pad_snow_seed']
PUBLIC_FIELDS={'create_niagara_scratch_pad_vec2_user_parameter_module': ['target_system_path', 'scratch_pad_name', 'parameter_name', 'parameter_value', 'user_parameter_name', 'user_parameter_default_value', 'target_usage', 'target_owner_kind', 'target_emitter_index', 'target_emitter_name', 'target_stack_emitter_index', 'target_stack_emitter_name', 'stack_suggested_name', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds'], 'create_niagara_scratch_pad_color_user_parameter_module': ['target_system_path', 'scratch_pad_name', 'parameter_name', 'parameter_value', 'user_parameter_name', 'user_parameter_default_value', 'target_usage', 'target_owner_kind', 'target_emitter_index', 'target_emitter_name', 'target_stack_emitter_index', 'target_stack_emitter_name', 'stack_suggested_name', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds'], 'create_niagara_scratch_pad_bool_user_parameter_module': ['target_system_path', 'scratch_pad_name', 'parameter_name', 'parameter_value', 'module_input_name', 'module_input_default_value', 'user_parameter_name', 'user_parameter_default_value', 'target_usage', 'target_owner_kind', 'target_emitter_index', 'target_emitter_name', 'target_stack_emitter_index', 'target_stack_emitter_name', 'stack_suggested_name', 'overwrite_existing', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds'], 'create_niagara_scratch_pad_bool_visible_toggle_module': ['target_system_path', 'scratch_pad_name', 'parameter_name', 'parameter_value', 'base_module_input_name', 'base_module_input_default_value', 'user_parameter_name', 'user_parameter_default_value', 'custom_hlsl_input_name', 'custom_hlsl_expression', 'custom_hlsl_output_pin_name', 'custom_hlsl_signature_name', 'target_usage', 'target_owner_kind', 'target_emitter_index', 'target_emitter_name', 'target_stack_emitter_index', 'target_stack_emitter_name', 'stack_suggested_name', 'overwrite_existing', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds'], 'create_niagara_visible_scratch_pad_snow_seed': ['target_system_path', 'preset', 'emitter_name', 'material_path', 'use_project_weather_materials', 'scratch_pad_name', 'parameter_name', 'parameter_value', 'user_parameter_name', 'user_parameter_default_value', 'target_usage', 'spawn_rate', 'velocity', 'sprite_size', 'lifetime', 'add_spawn_box_location', 'spawn_box_size', 'spawn_box_offset', 'add_weather_detail_modules', 'allow_source_edit', 'save', 'request_compile', 'wait_for_completion', 'timeout_seconds']}
PARAM_FIELDS=PUBLIC_FIELDS
DEFAULTS={'create_niagara_scratch_pad_vec2_user_parameter_module': {'scratch_pad_name': 'MCP_BlankUpdateScratch', 'parameter_name': 'Particles.SpriteSize', 'parameter_value': None, 'user_parameter_name': 'User.MCP_BlankScratchPad_SpriteSize', 'user_parameter_default_value': None, 'target_usage': 'ParticleUpdateScript', 'target_owner_kind': 'system', 'target_emitter_index': None, 'target_emitter_name': '', 'target_stack_emitter_index': 0, 'target_stack_emitter_name': '', 'stack_suggested_name': '', 'allow_source_edit': False, 'save': True, 'request_compile': True, 'wait_for_completion': True, 'timeout_seconds': 180}, 'create_niagara_scratch_pad_color_user_parameter_module': {'scratch_pad_name': 'MCP_ColorScratch', 'parameter_name': 'Particles.Color', 'parameter_value': None, 'user_parameter_name': 'User.MCP_ScratchPad_Color', 'user_parameter_default_value': None, 'target_usage': 'ParticleUpdateScript', 'target_owner_kind': 'system', 'target_emitter_index': None, 'target_emitter_name': '', 'target_stack_emitter_index': 0, 'target_stack_emitter_name': '', 'stack_suggested_name': '', 'allow_source_edit': False, 'save': True, 'request_compile': True, 'wait_for_completion': True, 'timeout_seconds': 180}, 'create_niagara_scratch_pad_bool_user_parameter_module': {'scratch_pad_name': 'MCP_BoolUserScratch', 'parameter_name': 'Particles.MCPBoolProbe', 'parameter_value': None, 'module_input_name': 'MCPBoolInput', 'module_input_default_value': None, 'user_parameter_name': 'User.MCP_ScratchPad_Bool', 'user_parameter_default_value': None, 'target_usage': 'ParticleUpdateScript', 'target_owner_kind': 'system', 'target_emitter_index': None, 'target_emitter_name': '', 'target_stack_emitter_index': 0, 'target_stack_emitter_name': '', 'stack_suggested_name': '', 'overwrite_existing': False, 'allow_source_edit': False, 'save': True, 'request_compile': True, 'wait_for_completion': True, 'timeout_seconds': 180}, 'create_niagara_scratch_pad_bool_visible_toggle_module': {'scratch_pad_name': 'MCP_BoolVisibleScratch', 'parameter_name': 'Particles.SpriteSize', 'parameter_value': None, 'base_module_input_name': 'MCPBaseSpriteSize', 'base_module_input_default_value': None, 'user_parameter_name': 'User.MCP_VisibleToggle', 'user_parameter_default_value': None, 'custom_hlsl_input_name': 'MCPVisibleToggle', 'custom_hlsl_expression': 'BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)', 'custom_hlsl_output_pin_name': 'MCPVisibleSize', 'custom_hlsl_signature_name': 'MCP_ScratchPadBoolVisibleSize', 'target_usage': 'ParticleUpdateScript', 'target_owner_kind': 'system', 'target_emitter_index': None, 'target_emitter_name': '', 'target_stack_emitter_index': 0, 'target_stack_emitter_name': '', 'stack_suggested_name': '', 'overwrite_existing': False, 'allow_source_edit': False, 'save': True, 'request_compile': True, 'wait_for_completion': True, 'timeout_seconds': 180}, 'create_niagara_visible_scratch_pad_snow_seed': {'preset': 'snow', 'emitter_name': 'Emitter', 'material_path': '', 'use_project_weather_materials': True, 'scratch_pad_name': 'MCP_VisibleSizeScratch', 'parameter_name': 'Particles.SpriteSize', 'parameter_value': None, 'user_parameter_name': 'User.MCP_VisibleScratchPad_SpriteSize', 'user_parameter_default_value': None, 'target_usage': 'ParticleUpdateScript', 'spawn_rate': 180.0, 'velocity': None, 'sprite_size': None, 'lifetime': 10.0, 'add_spawn_box_location': True, 'spawn_box_size': None, 'spawn_box_offset': None, 'add_weather_detail_modules': True, 'allow_source_edit': False, 'save': True, 'request_compile': True, 'wait_for_completion': True, 'timeout_seconds': 180}}
REQUIRED={'create_niagara_scratch_pad_vec2_user_parameter_module': ['target_system_path'], 'create_niagara_scratch_pad_color_user_parameter_module': ['target_system_path'], 'create_niagara_scratch_pad_bool_user_parameter_module': ['target_system_path'], 'create_niagara_scratch_pad_bool_visible_toggle_module': ['target_system_path'], 'create_niagara_visible_scratch_pad_snow_seed': ['target_system_path']}
FIELD_TYPES={'create_niagara_scratch_pad_vec2_user_parameter_module': {'target_system_path': 'string', 'scratch_pad_name': 'string', 'parameter_name': 'string', 'parameter_value': 'any', 'user_parameter_name': 'string', 'user_parameter_default_value': 'any', 'target_usage': 'string', 'target_owner_kind': 'string', 'target_emitter_index': 'integer', 'target_emitter_name': 'string', 'target_stack_emitter_index': 'integer', 'target_stack_emitter_name': 'string', 'stack_suggested_name': 'string', 'allow_source_edit': 'boolean', 'save': 'boolean', 'request_compile': 'boolean', 'wait_for_completion': 'boolean', 'timeout_seconds': 'integer'}, 'create_niagara_scratch_pad_color_user_parameter_module': {'target_system_path': 'string', 'scratch_pad_name': 'string', 'parameter_name': 'string', 'parameter_value': 'any', 'user_parameter_name': 'string', 'user_parameter_default_value': 'any', 'target_usage': 'string', 'target_owner_kind': 'string', 'target_emitter_index': 'integer', 'target_emitter_name': 'string', 'target_stack_emitter_index': 'integer', 'target_stack_emitter_name': 'string', 'stack_suggested_name': 'string', 'allow_source_edit': 'boolean', 'save': 'boolean', 'request_compile': 'boolean', 'wait_for_completion': 'boolean', 'timeout_seconds': 'integer'}, 'create_niagara_scratch_pad_bool_user_parameter_module': {'target_system_path': 'string', 'scratch_pad_name': 'string', 'parameter_name': 'string', 'parameter_value': 'any', 'module_input_name': 'string', 'module_input_default_value': 'any', 'user_parameter_name': 'string', 'user_parameter_default_value': 'any', 'target_usage': 'string', 'target_owner_kind': 'string', 'target_emitter_index': 'integer', 'target_emitter_name': 'string', 'target_stack_emitter_index': 'integer', 'target_stack_emitter_name': 'string', 'stack_suggested_name': 'string', 'overwrite_existing': 'boolean', 'allow_source_edit': 'boolean', 'save': 'boolean', 'request_compile': 'boolean', 'wait_for_completion': 'boolean', 'timeout_seconds': 'integer'}, 'create_niagara_scratch_pad_bool_visible_toggle_module': {'target_system_path': 'string', 'scratch_pad_name': 'string', 'parameter_name': 'string', 'parameter_value': 'any', 'base_module_input_name': 'string', 'base_module_input_default_value': 'any', 'user_parameter_name': 'string', 'user_parameter_default_value': 'any', 'custom_hlsl_input_name': 'string', 'custom_hlsl_expression': 'string', 'custom_hlsl_output_pin_name': 'string', 'custom_hlsl_signature_name': 'string', 'target_usage': 'string', 'target_owner_kind': 'string', 'target_emitter_index': 'integer', 'target_emitter_name': 'string', 'target_stack_emitter_index': 'integer', 'target_stack_emitter_name': 'string', 'stack_suggested_name': 'string', 'overwrite_existing': 'boolean', 'allow_source_edit': 'boolean', 'save': 'boolean', 'request_compile': 'boolean', 'wait_for_completion': 'boolean', 'timeout_seconds': 'integer'}, 'create_niagara_visible_scratch_pad_snow_seed': {'target_system_path': 'string', 'preset': 'string', 'emitter_name': 'string', 'material_path': 'string', 'use_project_weather_materials': 'boolean', 'scratch_pad_name': 'string', 'parameter_name': 'string', 'parameter_value': 'any', 'user_parameter_name': 'string', 'user_parameter_default_value': 'any', 'target_usage': 'string', 'spawn_rate': 'number', 'velocity': 'array', 'sprite_size': 'array', 'lifetime': 'number', 'add_spawn_box_location': 'boolean', 'spawn_box_size': 'array', 'spawn_box_offset': 'array', 'add_weather_detail_modules': 'boolean', 'allow_source_edit': 'boolean', 'save': 'boolean', 'request_compile': 'boolean', 'wait_for_completion': 'boolean', 'timeout_seconds': 'integer'}}
NULLABLE={'create_niagara_scratch_pad_vec2_user_parameter_module': ['parameter_value', 'user_parameter_default_value', 'target_emitter_index', 'target_stack_emitter_index'], 'create_niagara_scratch_pad_color_user_parameter_module': ['parameter_value', 'user_parameter_default_value', 'target_emitter_index', 'target_stack_emitter_index'], 'create_niagara_scratch_pad_bool_user_parameter_module': ['parameter_value', 'module_input_default_value', 'user_parameter_default_value', 'target_emitter_index', 'target_stack_emitter_index'], 'create_niagara_scratch_pad_bool_visible_toggle_module': ['parameter_value', 'base_module_input_default_value', 'user_parameter_default_value', 'target_emitter_index', 'target_stack_emitter_index'], 'create_niagara_visible_scratch_pad_snow_seed': ['parameter_value', 'user_parameter_default_value', 'spawn_rate', 'velocity', 'sprite_size', 'lifetime', 'add_spawn_box_location', 'spawn_box_size', 'spawn_box_offset', 'add_weather_detail_modules']}
ITEMS={name:{} for name in METHODS}
ITEMS['create_niagara_visible_scratch_pad_snow_seed']={k:"any" for k in ['velocity', 'sprite_size', 'spawn_box_size', 'spawn_box_offset']}
_FUNCTION_LEGACY_KEYS={"create_or_duplicate_scratch_pad_module": ["success", "target_system_path", "system_path", "source_script_path", "source_script_name", "source_owner_kind", "source_owner_name", "source_emitter_index", "source_script_index", "target_owner_kind", "target_owner_name", "target_emitter_index", "new_script_name", "new_script_path", "usage", "creation_mode", "target_supported_usage", "target_scratch_pad_count", "compile_requested", "saved", "write_scope"], "add_niagara_scratch_pad_parameter_write": ["success", "skipped_existing", "system_path", "scratch_pad_script_path", "scratch_pad_script_name", "parameter_name", "parameter_type", "default_value", "parameter_map_set_node_name", "parameter_map_set_node_index", "compile_requested", "saved", "write_scope", "scratch_pad_owner_kind", "scratch_pad_owner_name", "scratch_pad_emitter_index", "scratch_pad_script_index", "requested_parameter_name", "parameter_map_set_node_guid", "input_map_source_pin", "output_map_target_pin", "previous_output_link_count", "new_output_link_count", "previous_output_links", "parameter_pin", "parameter_map_set_node"], "add_scratch_pad_module_to_stack": ["success", "skipped_duplicate", "skip_reason", "target_system_path", "system_path", "scratch_pad_script_path", "scratch_pad_script_name", "scratch_pad_owner_kind", "scratch_pad_owner_name", "scratch_pad_emitter_index", "scratch_pad_script_index", "target_usage", "target_usage_id", "stack_owner_kind", "stack_owner_name", "target_emitter_index", "target_index", "suggested_name", "existing_module_node_guid", "existing_module_node_name", "existing_module_function_name", "output_node_guid", "graph_path", "graph_node_count_before", "graph_node_count_after", "compile_requested", "saved", "write_scope", "module", "new_module_node_guid", "new_module_node_name", "new_module_function_name", "auto_bound_user_data_interface_input_count"], "insert_niagara_scratch_pad_custom_hlsl_for_pin": ["success", "already_inserted", "system_path", "scratch_pad_script_path", "target_node_name", "target_pin_name", "custom_hlsl_node_guid", "expression", "custom_hlsl_script_usage", "topology_verified", "compile_requested", "saved", "write_scope", "updated_existing_custom", "target_node_guid", "target_node_index", "scratch_pad_script_name", "scratch_pad_owner_kind", "scratch_pad_owner_name", "scratch_pad_emitter_index", "scratch_pad_script_index", "output_pin_name", "output_type", "previous_default_value", "previous_default_object", "previous_link_count", "broken_link_count", "new_link_count", "removed_unlinked_custom_node_count", "removed_unlinked_custom_input_source_node_count", "requires_context", "auto_added_context_map_input", "custom_input_count", "user_parameter_input_count", "parameter_input_count", "created_user_parameter_input_node_count", "previous_linked_sources", "new_linked_sources", "custom_inputs"], "upsert_niagara_user_parameters": ["index", "success", "error", "parameter_name", "type", "created", "data_interface_created", "data_interface_class", "data_interface_path", "value_set", "system_path", "added_count", "updated_count", "failed_count", "results", "requested_count", "processed_count", "changed", "compile_requested", "saved", "continue_on_error", "write_scope"]}
_RECIPE_NAMES={'create_niagara_scratch_pad_vec2_user_parameter_module': '_recipe_vec2', 'create_niagara_scratch_pad_color_user_parameter_module': '_recipe_color', 'create_niagara_scratch_pad_bool_user_parameter_module': '_recipe_bool_user', 'create_niagara_scratch_pad_bool_visible_toggle_module': '_recipe_visible', 'create_niagara_visible_scratch_pad_snow_seed': '_recipe_snow_seed'}
_LEAF_DOMAINS={'create_sprite_niagara_preset_system':'niagara_sprite_preset_authoring',"create_or_duplicate_scratch_pad_module": "niagara_scratch", "add_niagara_scratch_pad_parameter_write": "niagara_scratch", "link_niagara_scratch_pad_pin_to_user_parameter": "niagara_resource_bindings", "add_scratch_pad_module_to_stack": "niagara_scratch", "add_niagara_scratch_pad_module_input": "niagara_scratch_stack_authoring", "set_niagara_module_input_user_parameter": "niagara_module_input_paths", "upsert_niagara_user_parameters": "niagara_parameters", "insert_niagara_scratch_pad_custom_hlsl_for_pin": "niagara_scratch", "inspect_niagara_compile_status": "niagara_authoring_inspection"}
_LEAF_LIBRARIES={"create_or_duplicate_scratch_pad_module": "CubelessNiagaraScratchLibrary", "add_niagara_scratch_pad_parameter_write": "CubelessNiagaraScratchLibrary", "link_niagara_scratch_pad_pin_to_user_parameter": "CubelessNiagaraResourceBindingLibrary", "add_scratch_pad_module_to_stack": "CubelessNiagaraScratchLibrary", "add_niagara_scratch_pad_module_input": "CubelessNiagaraScratchStackAuthoringLibrary", "set_niagara_module_input_user_parameter": "CubelessNiagaraModuleInputPathLibrary", "upsert_niagara_user_parameters": "CubelessNiagaraParameterLibrary", "insert_niagara_scratch_pad_custom_hlsl_for_pin": "CubelessNiagaraScratchLibrary", "inspect_niagara_compile_status": "CubelessNiagaraAuthoringInspectionLibrary"}
_ACTIVE=False
_TICKER_HANDLE=None
_SHUTDOWN_HANDLE=None
_PENDING={}
_QUARANTINED={}
_LAST_TERMINAL={}
_LIFECYCLE_ERRORS=[]


class _BatchScratchUserRecipeFlow:
    """Original five generators; scheduling, journal and cancellation lease belong to Batch."""
    def __init__(self,result,arguments,checkpoint_callback,method,parent_current,cancel_current):
        self.r=self.result=result
        self.method,self.callback=method,checkpoint_callback
        self.parent_current,self.cancel_current=parent_current,cancel_current
        self.job=None
        self.in_flight=self.lost=self.cancel_entered=self.job_reference_verified=False
        self.checkpoint_failure_stop_permitted=False
        self.finished=False
        values=_values(method,arguments,arguments.get('_public_input',False),arguments.get('_legacy_command_params'))
        _pure_admission(method,values)
        self.current()
        if _QUARANTINED:raise OperationError('niagara_scratch_user_recipe_quarantined','Prior unknown recipe requires reconciliation; no new writer')
        if not binding_status().get(method):raise OperationError('niagara_scratch_user_recipe_leaf_unavailable','Exact existing leaf bindings are required; no first writer dispatched')
        self.entry=_initial_entry(method,values,result,lambda ignored:self.checkpoint())
        result['data'].update(Batch_async_continuation_implemented=True,parent_journal_only=True,
            child_journals_created=0,async_entry='official_Batch_owned_ScratchUser_recipe',
            native_dispatched=False,change_state_known=True,operation_complete=False,
            command_may_still_be_in_flight=False,global_compiler_cancel_attempted=False)

    def unsafe(self,error):
        self.lost=True
        self.r['data'].update(change_state_known=False,mutation_state_unknown=True,
            mutation_performed=None,do_not_retry=True,workflow_recovery_required=True)
        return error

    def current(self):
        if self.lost:raise OperationError('batch_scratch_user_recipe_observation_lost','Observation loss is sticky; no next leaf or Job SDK')
        try:
            self.parent_current()
            if not _ACTIVE:raise OperationError('niagara_scratch_user_recipe_inactive','Recipe lifecycle retired; no next leaf')
        except Exception as error:raise self.unsafe(error)

    def checkpoint(self):
        try:
            self.current()
            if self.callback:self.callback(self.r)
            self.current()
        except Exception as error:
            self.checkpoint_failure_stop_permitted=True
            raise self.unsafe(error)

    def _unadopted(self,error,native=None):
        if native is None:native=getattr(error,'native_decoded_result',None)
        raw=native.get('_raw_native_json') if type(native) is dict else None
        if raw is None:raw=getattr(error,'native_raw_result',None)
        if type(native) is dict or getattr(error,'native_snapshot_returned',False):
            self.r['data'].update(native_snapshot_returned=True,
                unadopted_returned_native_snapshot=copy.deepcopy(native),
                unadopted_returned_native_raw_result=raw,
                returned_native_snapshot_adopted=False,returned_native_current_verified=False)

    def _complete(self):
        self.current()
        unknown=self.r['data'].get('mutation_state_unknown') is True
        if unknown and self.r['success']:
            state.fail(self.r,OperationError('niagara_scratch_user_recipe_scope_unverified','Current receipt remains unknown; original legacy and actual save prefix retained'))
        self.r['data'].update(operation_complete=True,workflow_recovery_required=unknown,
            command_may_still_be_in_flight=False,automatic_retry=False,
            cancel_acknowledgement_is_not_shared_compiler_stop=True)
        self.checkpoint()
        self.finished=True
        return True

    def start(self,asynchronous):
        if asynchronous is not True:raise OperationError('batch_scratch_user_recipe_official_async_required','Original recipe waits require the existing official Batch continuation')
        self.current()
        if not _advance(self.entry,first=True):return self._complete()
        self.r['stage']='before_niagara_scratch_user_recipes_checkpoint'
        self.checkpoint()
        return False

    def _accept_leaf(self,name,legacy,error=None):
        self.current()
        more=_accept(self.entry,name,legacy,error)
        self.job=None
        self.job_reference_verified=self.in_flight=False
        self.cancel_entered=False
        if not more:return self._complete()
        if self.r['data'].get('mutation_state_unknown') is True:
            raise self.unsafe(OperationError('batch_scratch_user_recipe_child_unknown','Actual child prefix is unknown; next recipe leaf withheld without replay'))
        self.checkpoint()
        return False

    def _poll_job(self):
        self.current()
        name=self.entry['leaf'][0]
        native=None;adopted=False
        self.entry['snapshot_calls']+=1
        try:
            native=api.snapshot(self.job,name,current_callback=self.current)
            self.current()
            # _adopt_job checks exact same-job ID and monotonic revision before
            # touching receipts/compile/save. No inadmissible DTO becomes proof.
            child,legacy=_adopt_job(self.entry,native,checkpoint=False)
            adopted=True
            terminal=native['data']['native_job_terminal']
            self.r['data'].update(native_snapshot_returned=True,
                returned_native_snapshot_adopted=True,returned_native_current_verified=True,
                command_may_still_be_in_flight=not terminal)
            if terminal:self.in_flight=False
            self.checkpoint()
        except Exception as error:
            if not adopted:self._unadopted(error,native)
            else:self.r['data']['adapter_failure_after_native_adoption']=True
            raise self.unsafe(error)
        if not terminal:return False
        if self.r['data'].get('mutation_state_unknown') is True:
            raise self.unsafe(OperationError('batch_scratch_user_recipe_terminal_unknown','Terminal child has unknown change state; actual prefix retained and no next recipe stage'))
        error=None
        if not native['success'] or not native['postcondition_passed'] or not native['data']['operation_complete']:
            error=NiagaraBridgeResponseError(name,legacy or dict(success=False,error=native.get('error')),
                command_effect='read' if name=='inspect_niagara_compile_status' else 'mutation')
        return self._accept_leaf(name,legacy,error)

    def poll(self):
        self.current()
        self.checkpoint_failure_stop_permitted=False
        if self.finished:return True
        if self.job_reference_verified:return self._poll_job()
        name,params=self.entry['leaf']
        prior={key:self.r['data'].get(key) for key in ('change_state_known','mutation_state_unknown','mutation_performed')}
        self.r['stage']='before_niagara_scratch_user_recipes_checkpoint'
        self.r['data'].update(pending_recipe_leaf=dict(index=self.entry['leaf_index'],operation=name),
            recipe_leaf_dispatch_intent=True,change_state_known=False,
            mutation_state_unknown=True,mutation_performed=None)
        self.checkpoint()  # Persist uncertain dispatch intent before the next SDK.
        self.in_flight=True
        if name in api.METHODS and params.get('wait_for_completion') is True:
            try:
                self.job=api.start_job(name,params,current_callback=self.current)
                self.entry.update(job=self.job,job_reference_verified=True)
                self.job_reference_verified=True
                self.r['data'].update(native_dispatched=True,native_start_returned=True,
                    command_may_still_be_in_flight=True)
                self.current()
                self.checkpoint()
            except Exception as error:
                if getattr(error,'native_start_returned',False):
                    self.job=getattr(error,'native_returned_job',self.job)
                    self.entry['job']=self.job
                    self.r['data'].update(native_dispatched=True,native_start_returned=True)
                if getattr(error,'native_start_dispatched',None) is False:
                    self.in_flight=False
                    if not self.lost:self.r['data'].update(prior)
                else:self.unsafe(error)
                raise
            return False  # One start OR one snapshot per existing parent tick.
        self.entry['synchronous_child_entered']=True
        try:child,legacy,error=_call_sync(self.entry,name,params)
        except Exception as error:raise self.unsafe(error)
        self.entry['synchronous_child_entered']=False
        self.in_flight=False
        self.current()  # SDK callbacks may retire the parent; never resume then.
        if not child.get('success'):
            error=NiagaraBridgeResponseError(name,legacy or dict(success=False,error=child.get('error')),
                command_effect='read' if name=='inspect_niagara_compile_status' else 'mutation')
        return self._accept_leaf(name,legacy,error)

    def fail(self,error):
        if self.checkpoint_failure_stop_permitted and self.in_flight and self.job_reference_verified and not self.cancel_entered:
            try:
                job=self.job
                self.cancel_current(self,job,True)
                self.cancel_entered=True
                self.r['data']['owned_cancel_after_failure_attempts']=1
                acknowledged=job.cancel_job('Stop exact owned recipe leaf after Batch checkpoint failure: '+str(error)[:700])
                self.r['data'].update(owned_cancel_after_failure_returned=True,
                    owned_cancel_acknowledgement=acknowledged if type(acknowledged) is bool else None)
                self.cancel_current(self,job,True)
            except Exception as cancel_error:self.r['data']['owned_cancel_after_failure_error']=str(cancel_error)[:700]
        state.fail(self.r,error)
        if self.in_flight or self.lost:
            self.r['data'].update(mutation_state_unknown=True,change_state_known=False,
                mutation_performed=None,workflow_recovery_required=True,do_not_retry=True)
        self.r['data'].update(operation_complete=False,command_may_still_be_in_flight=self.in_flight)
        self.r['retry'].update(automatic_retry=False,duplicate_risk='owned_ScratchUser_recipe_Batch_no_replay')
        return self.r

def tool_error(message,*,error_code='error',details=None,**fields):
    result=dict(success=False,message=message,error=message,error_code=error_code)
    if details is not None:result['details']=details
    result.update(fields);return result

def _public_error(field,kind,message):
    error=OperationError('niagara_scratch_user_recipes_public_argument_invalid',field+': '+message)
    error.public_validation_errors=[dict(field=[field],type=kind,message=message)];return error

def _public_bool(field,value):
    if type(value) is bool:return value
    if type(value) in (int,float) and value in (0,1):return bool(value)
    if type(value) is str:
        lower=value.lower()
        if lower in ('true','1','t','yes','y','on'):return True
        if lower in ('false','0','f','no','n','off'):return False
        raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    if type(value) in (int,float):raise _public_error(field,'bool_parsing','Input should be a valid boolean, unable to interpret input')
    raise _public_error(field,'bool_type','Input should be a valid boolean')

def _public_int(field,value):
    if type(value) in (int,bool):return int(value)
    if type(value) is float:
        if not math.isfinite(value):raise _public_error(field,'finite_number','Input should be a finite number')
        if not value.is_integer():raise _public_error(field,'int_from_float','Input should be a valid integer, got a number with a fractional part')
        return int(value)
    if type(value) is str:
        stripped=value.strip()
        if stripped.isascii() and re.fullmatch(r'[+-]?[0-9](?:_?[0-9])*(?:\.0+)?',stripped):return int(stripped.replace('_','').split('.')[0])
        raise _public_error(field,'int_parsing','Input should be a valid integer, unable to parse string as an integer')
    raise _public_error(field,'int_type','Input should be a valid integer')

def _public_float(field,value):
    if type(value) in (int,float,bool):
        try:return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_type','Input should be a valid number')
    if type(value) is str:
        try:
            if not value.isascii():raise ValueError()
            return float(value)
        except (ValueError,OverflowError):raise _public_error(field,'float_parsing','Input should be a valid number, unable to parse string as a number')
    raise _public_error(field,'float_type','Input should be a valid number')

def normalize_public_mcp_arguments(method,arguments):
    """Field-specific original FastMCP preparse/Pydantic profile; no Engine/raw coercion clone."""
    if method not in PUBLIC_FIELDS or type(arguments) is not dict:raise OperationError('niagara_scratch_user_recipes_public_argument_invalid','Known method and public argument object required')
    values=copy.deepcopy(DEFAULTS[method])
    for field in PUBLIC_FIELDS[method]:
        if field not in arguments:
            if field in REQUIRED[method]:raise _public_error(field,'missing','Field required')
            continue
        value=copy.deepcopy(arguments[field])
        if type(value) is str:
            try:parsed=json.loads(value)
            except json.JSONDecodeError:parsed=value
            if not isinstance(parsed,str):value=parsed
        kind=FIELD_TYPES[method][field]
        if value is None and field in NULLABLE[method]:pass
        elif kind=='array':
            if not isinstance(value,(list,tuple)):raise _public_error(field,'list_type','Input should be a valid list')
            rows=[]
            for index,item in enumerate(value):
                try:
                    if ITEMS[method][field]=='string':
                        if type(item) is not str:raise _public_error(field,'string_type','Input should be a valid string')
                    elif ITEMS[method][field]=='integer':item=_public_int(field,item)
                    elif ITEMS[method][field]=='object':
                        if type(item) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
                    elif ITEMS[method][field]=='number':item=_public_float(field,item)
                    elif ITEMS[method][field]!='any':raise OperationError('niagara_scratch_user_recipes_public_argument_invalid','Unexpected frozen item type')
                except OperationError as error:error.public_validation_errors[0]['field'].append(index);raise
                rows.append(item)
            value=rows
        elif kind=='object':
            if type(value) is not dict:raise _public_error(field,'dict_type','Input should be a valid dictionary')
            if any(type(key) is not str for key in value):raise _public_error(field,'string_type','Dictionary keys should be valid strings')
        elif kind=='any':pass
        elif kind=='string':
            if type(value) is not str:raise _public_error(field,'string_type','Input should be a valid string')
        elif kind=='boolean':value=_public_bool(field,value)
        elif kind=='integer':value=_public_int(field,value)
        elif kind=='number':value=_public_float(field,value)
        else:raise OperationError('niagara_scratch_user_recipes_public_argument_invalid','Unexpected frozen public type')
        values[field]=value
    return values

LEGACY_MESSAGES={"inspect_niagara_stack": "Error inspecting Niagara stack: ", "inspect_niagara_graph": "Error inspecting Niagara graph: ", "inspect_niagara_graph_actions": "Error inspecting Niagara graph actions: ", "inspect_niagara_custom_hlsl_nodes": "Error inspecting Niagara Custom HLSL nodes: ", "inspect_niagara_scratch_pad_interface": "Error inspecting Niagara scratch pad interface: ", "inspect_niagara_compile_status": "Error inspecting Niagara compile status: "}

def _reject_legacy_command_dependency_refs(params):
    from .niagara_authoring_inspection import _reject_legacy_command_dependency_refs as reject
    reject(params)

def _values(method,arguments,public=False,legacy=None):
    if method not in METHODS:raise OperationError('niagara_scratch_user_recipe_method_invalid','Only the four original recipe contracts are admitted')
    source=legacy if legacy is not None else arguments
    if type(source) is not dict:raise OperationError('niagara_scratch_user_recipe_input_invalid','Original recipe arguments must be an object')
    values=normalize_public_mcp_arguments(method,source) if public else dict(copy.deepcopy(DEFAULTS[method]),**{k:copy.deepcopy(v) for k,v in source.items() if k in PUBLIC_FIELDS[method]})
    for key in REQUIRED[method]:
        if key not in values:raise OperationError('niagara_scratch_user_recipe_input_invalid','Missing '+key)
    api._carrier(values)
    return values

def _context(result,method,values,entry):
    result['data'].update(operation=method,requested_arguments=copy.deepcopy(values),entry_route=entry,parent_journal_only=entry=='ordinary_Batch',original_recipe_order_preserved=True,original_public_default_wait_preserved=True,original_completed_command_list_is_not_actual_mutation_count=True,automatic_action_retry=False,atomic_rollback=False,server_RPC_dependency=False,operation_dependency='existing dedicated Editor EngineAPI leaves',unique_external_pipeline_complete=False,actual_official_async_wire_verified=False,legacy_Named_result_reference='data.legacy_result')

def _rejected(method,error):
    result=state.fail(state.new_result(),error);result['stage']='niagara_scratch_user_recipe_admission'
    result['data'].update(operation=method,native_dispatched=False,native_mutation_attempted=False,execution_mutation_attempted=False,change_state_known=True,changed=False,mutation_state_unknown=False,mutation_performed=False,journal_created=False)
    result['compile'].update(requested=False,attempted=False,compiled=False);result['save'].update(requested=False,attempted=False,saved=False)
    if hasattr(error,'public_validation_errors'):result['data']['public_validation_errors']=copy.deepcopy(error.public_validation_errors)
    if hasattr(error,'legacy_result'):result['data']['legacy_result']=copy.deepcopy(error.legacy_result)
    return result

def _requires_continuation(method,values):
    # GH continuation is required only when both flags are true. BoolUser
    # request_compile=False keeps GL's own pending/zero-work wait admission.
    return method=='create_niagara_visible_scratch_pad_snow_seed' or bool(values.get('wait_for_completion') and values.get('request_compile'))

def _pure_admission(method,values):
    generator=globals()[_RECIPE_NAMES[method]](**values)
    try:
        try:next(generator)
        except StopIteration as finished:
            legacy=finished.value
            if type(legacy) is dict and legacy.get('success') is False:
                error=OperationError(legacy.get('error_code','niagara_scratch_user_recipe_preflight_failed'),legacy.get('error','Original recipe preflight failed'));error.legacy_result=legacy;raise error
    finally:generator.close()

def _sync_admission(method,values):
    if _requires_continuation(method,values):
        raise OperationError('niagara_scratch_user_recipe_sync_continuation_unsupported','This direct/ordinary Batch recipe has a required GL/GH wait phase and no final continuation. Rejected before the first Engine writer; use the exact Named workflow. No wait=False substitution or replay.')

def _child_module(name):
    import importlib
    return importlib.import_module('cubeless_mcp_ue58.operations.'+_LEAF_DOMAINS[name])

def _leaf_legacy(name,child):
    data=child.get('data',{})
    if type(data.get('legacy_result')) is dict:return copy.deepcopy(data['legacy_result'])
    if type(data.get('legacy_Named_result')) is dict:return copy.deepcopy(data['legacy_Named_result'])
    keys=_FUNCTION_LEGACY_KEYS.get(name,())
    legacy={k:copy.deepcopy(data[k]) for k in keys if k in data}
    if keys:
        legacy['success']=child.get('success') is True
        legacy['saved']=child.get('save',{}).get('saved') is True
        legacy['compile_requested']=child.get('compile',{}).get('requested') is True
    # Exact renames of authoritative existing metadata, never an invented count.
    if name=='insert_niagara_scratch_pad_custom_hlsl_for_pin':
        if 'custom_node_guid' in data:legacy['custom_hlsl_node_guid']=data['custom_node_guid']
    if name=='add_scratch_pad_module_to_stack' and data.get('skipped_duplicate') is True:
        module=data.get('module',{})
        for key,actual in (('existing_module_node_name','node_name'),('existing_module_function_name','function_name')):
            if key not in legacy and actual in module:legacy[key]=module[actual]
    return legacy

def _prefix(result,child):
    data=child.get('data',{});outcome=data.get('native_outcome',{})
    # Actual histories are monotonic across all children and all same-job revisions.
    for group,fields in (('compile',('requested','attempted')),('save',('requested','attempted','saved'))):
        for field in fields:result[group][field]=(result[group].get(field) is True or child.get(group,{}).get(field) is True)
    if result['compile'].get('requested') is True or result['compile'].get('attempted') is True:
        result['compile']['status']='recipe_child_compile_request_or_attempt_recorded'
    current=child.get('compile',{}).get('compiled')
    if child.get('compile',{}).get('attempted') is True:
        if current is not True:result['compile']['compiled']=current
        elif result['compile'].get('compiled') is not None:result['compile']['compiled']=True
    if 'SDK_global_purity_unverified' in data:result['data']['SDK_global_purity_unverified']=result['data'].get('SDK_global_purity_unverified',False) or data['SDK_global_purity_unverified'] is True
    known=data.get('change_state_known',not data.get('mutation_state_unknown',False))
    explicit=data.get('native_mutation_attempted',outcome.get('mutation_attempted'))
    changed=data.get('mutation_performed',outcome.get('changed'))
    result['data']['actual_mutation_attempted']=result['data'].get('actual_mutation_attempted',False) or explicit is True
    result['data']['execution_mutation_attempted']=result['data']['actual_mutation_attempted'];result['data']['native_mutation_attempted']=result['data']['actual_mutation_attempted']
    result['data']['actual_changed_observed']=result['data'].get('actual_changed_observed',False) or changed is True
    if known is False:result['data']['mutation_state_unknown']=True
    result['changes']=[dict(kind=result['data']['operation'],attempted=result['data']['actual_mutation_attempted'],changed=None if result['data'].get('mutation_state_unknown') else result['data']['actual_changed_observed'],change_state_known=not result['data'].get('mutation_state_unknown',False))] if result['data']['actual_mutation_attempted'] or result['data'].get('mutation_state_unknown') else []

def _record_child(entry,name,child,legacy=None,pending=False):
    result=entry['result'];_prefix(result,child)
    index=entry['leaf_index'];receipts=result['data'].setdefault('native_receipts',[])
    data=child.get('data',{});native=data.get('native_outcome',{})
    original_keys=set(_FUNCTION_LEGACY_KEYS.get(name,()))
    skip=original_keys|{'legacy_result','legacy_Named_result','native_raw_result','native_result_data','native_outcome','requested_arguments','native_params','legacy_command_params'}
    diagnostic={k:copy.deepcopy(v) for k,v in data.items() if k not in skip}
    receipt=dict(index=index,operation=name,success=child.get('success'),stage=child.get('stage'),error_code=child.get('error_code'),error=child.get('error'),compile=copy.deepcopy(child.get('compile',{})),save=copy.deepcopy(child.get('save',{})),validation=copy.deepcopy(child.get('validation',{})),changes=copy.deepcopy(child.get('changes',[])),diagnostics=diagnostic,pending=pending,canonical_legacy_reference=None)
    # The original Native raw string is not rewritten and labelled raw. Only
    # its digest/length is retained alongside one typed diagnostic projection.
    raw=data.get('native_raw_result')
    if type(raw) is str:receipt['original_native_raw_sha256']=hashlib.sha256(raw.encode('utf-8')).hexdigest();receipt['original_native_raw_characters']=len(raw)
    if len(receipts)==index:receipts.append(receipt)
    elif len(receipts)>index:receipts[index]=receipt
    else:raise OperationError('niagara_scratch_user_recipe_receipt_order','Child receipt order changed')
    # Revisions replace only the same leaf's uncertainty; actual histories stay ORed.
    known=data.get('change_state_known',not data.get('mutation_state_unknown',False)) is True
    observer=(name=='inspect_niagara_compile_status' and child.get('success') is True and child.get('validation',{}).get('native_postcondition_passed') is True and not child.get('compile',{}).get('attempted') and not child.get('save',{}).get('attempted') and data.get('native_mutation_attempted') is False)
    receipt['target_change_state_known']=known or observer
    result['data']['mutation_state_unknown']=any(not row.get('target_change_state_known',False) for row in receipts)
    result['data']['mutation_performed']=None if result['data']['mutation_state_unknown'] else result['data']['actual_changed_observed']
    result['changes']=[dict(kind=entry['method'],attempted=result['data']['actual_mutation_attempted'],changed=result['data']['mutation_performed'],change_state_known=not result['data']['mutation_state_unknown'])] if result['data']['actual_mutation_attempted'] or result['data']['mutation_state_unknown'] else []
    result['data'].update(change_state_known=not result['data']['mutation_state_unknown'],native_result_returned=True)
    result['stage']='returned_niagara_scratch_user_recipes_operation'
    if legacy is not None:entry['current_legacy']=legacy

def _from_native(native):
    child=state.new_result();data=native['data'];child['data'].update(copy.deepcopy(data));child['data'].update(native_outcome={k:v for k,v in native.items() if k not in ('data','_raw_native_json')},native_mutation_attempted=native['mutation_attempted'],change_state_known=native['change_state_known'],mutation_state_unknown=not native['change_state_known'],mutation_performed=native['changed'] if native['change_state_known'] else None)
    child.update(success=native['success'],stage=native['stage'],error_code=native.get('error_code',''),error=native.get('error',''))
    child['compile'].update(requested=native['compile_requested'],attempted=native['compile_attempted'],compiled=native['compiled']);child['save'].update(requested=native['save_requested'],attempted=native['save_attempted'],saved=native['saved'])
    child['validation'].update(native_postcondition_passed=native['postcondition_passed']);child['changes']=[] if native['change_state_known'] and native['changed'] is False and not native['mutation_attempted'] else [dict(attempted=native['mutation_attempted'],changed=native['changed'],change_state_known=native['change_state_known'])]
    return child

def _call_sync(entry,name,params):
    child=state.new_result();module=_child_module(name);values=copy.deepcopy(params)
    # These current action boundaries decode JSON text once; other Any defaults stay typed.
    if name=='add_niagara_scratch_pad_parameter_write' and not values.get('default_value') and values.get('value') is not None:
        values['value']=json.dumps(values['value'],ensure_ascii=False,allow_nan=False,separators=(',',':'))
    elif name=='insert_niagara_scratch_pad_custom_hlsl_for_pin':
        for field in ('inputs','user_parameter_inputs','parameter_inputs'):
            if field in values and values[field] is not None:
                values[field]=json.dumps(values[field],ensure_ascii=False,allow_nan=False,separators=(',',':'))
    elif name=='upsert_niagara_user_parameters':values['parameters_json']=json.dumps(values.pop('parameters'),ensure_ascii=False,allow_nan=False)
    error=None
    def child_checkpoint(current):
        data=current.get('data',{})
        if type(data.get('native_outcome')) is dict or data.get('native_result_returned') is True:
            _record_child(entry,name,current,_leaf_legacy(name,current),pending=True)
        _checkpoint(entry)
    try:getattr(module,name+'_action')(child,**values,checkpoint_callback=child_checkpoint)
    except Exception as caught:error=caught;state.fail(child,caught)
    legacy=_leaf_legacy(name,child);_record_child(entry,name,child,legacy)
    # Adopt actual failure/save prefix before the parent checkpoint or error.
    _checkpoint(entry)
    return child,legacy,error

def _checkpoint(entry):
    if entry.get('finalization_attempted'):raise OperationError('niagara_scratch_user_recipe_sealed','A sealed receipt cannot be changed or checkpointed')
    callback=entry.get('callback')
    if callback:callback(entry['result'])

def _initial_entry(method,values,result,callback=None):
    _context(result,method,values,'Named_official_MCP_only' if callback is None else 'ordinary_Batch')
    result['data'].update(recipe_stages=[],native_receipts=[],actual_mutation_attempted=False,actual_changed_observed=False,mutation_state_unknown=False,mutation_performed=False)
    result['compile'].update(requested=False,attempted=False,compiled=False);result['save'].update(requested=False,attempted=False,saved=False)
    return dict(method=method,values=values,result=result,generator=globals()[_RECIPE_NAMES[method]](**values),leaf_index=0,leaf=None,job=None,callback=callback,executing=False,job_reference_verified=False,snapshot_calls=0)

def _advance(entry,value=None,error=None,first=False):
    try:
        if first:step=next(entry['generator'])
        elif error is not None:step=entry['generator'].throw(error)
        else:step=entry['generator'].send(value)
        entry['leaf']=step
    except StopIteration as stopped:
        entry['leaf']=None;legacy=stopped.value
        if type(legacy) is not dict:raise OperationError('niagara_scratch_user_recipe_result_invalid','Original recipe must return one legacy object')
        entry['result']['data']['legacy_result']=legacy
        if legacy.get('success') is True and entry['method']!='create_niagara_visible_scratch_pad_snow_seed':
            entry['result']['data'].pop('recipe_stages',None)
            for receipt in entry['result']['data']['native_receipts']:
                reference=receipt.get('canonical_legacy_reference')
                if type(reference) is str:receipt['canonical_legacy_reference']=reference.replace('data.recipe_stages','data.legacy_result.stages')
        elif legacy.get('success') is False:
            receipts=entry['result']['data']['native_receipts'];response=legacy.get('unreal_response')
            current=entry.get('current_legacy')
            if receipts and type(response) is dict and type(current) is dict and json.dumps(response,sort_keys=True,allow_nan=False)==json.dumps(current,sort_keys=True,allow_nan=False):
                receipt=receipts[-1]
                if receipt.get('index')==entry['leaf_index'] and receipt.get('operation')==legacy.get('failed_bridge_command'):
                    receipt['canonical_legacy_reference']='data.legacy_result.unreal_response'
        entry['result'].update(success=legacy.get('success') is True,stage='complete' if legacy.get('success') is True else 'recipe_failed',error_code=legacy.get('error_code',''),error=legacy.get('error',''))
        return False
    return True

def _accept(entry,name,legacy,error=None):
    if error is None and legacy.get('success') is not False:
        stages=entry['result']['data']['recipe_stages'];stages.append(dict(stage=name,result=legacy))
        receipts=entry['result']['data']['native_receipts'];index=entry['leaf_index']
        if index<len(receipts):receipts[index]['canonical_legacy_reference']='data.recipe_stages['+str(len(stages)-1)+'].result'
    entry['job']=None;entry['job_reference_verified']=False;entry.pop('native_job_id',None);entry.pop('last_revision',None);entry.pop('cancel_attempted',None)
    more=_advance(entry,legacy,error)
    entry['leaf_index']+=1
    return more

def _direct_action(result,method,arguments,checkpoint_callback=None,legacy=None,public=False):
    try:values=_values(method,arguments,public,legacy);_pure_admission(method,values);_sync_admission(method,values)
    except Exception as error:
        operation_id=result['operation_id'];rejected=_rejected(method,error);rejected['operation_id']=operation_id
        if hasattr(error,'legacy_result'):rejected['data']['legacy_result']=copy.deepcopy(error.legacy_result)
        rejected['data']['parent_journal_only']=True;rejected['stage']='returned_niagara_scratch_user_recipes_operation';result.clear();result.update(rejected)
        if checkpoint_callback:checkpoint_callback(result)
        raise
    entry=_initial_entry(method,values,result,checkpoint_callback or (lambda ignored:None));_context(result,method,values,'direct_python' if checkpoint_callback is state.checkpoint else 'ordinary_Batch');more=_advance(entry,first=True)
    while more:
        name,params=entry['leaf'];result['stage']='before_niagara_scratch_user_recipes_checkpoint';result['data']['pending_recipe_leaf']=dict(index=entry['leaf_index'],operation=name);_checkpoint(entry)
        child,payload,error=_call_sync(entry,name,params)
        if not child.get('success'):error=NiagaraBridgeResponseError(name,payload or dict(success=False,error=child.get('error')),command_effect='read' if name=='inspect_niagara_compile_status' else 'mutation')
        more=_accept(entry,name,payload,error)
    result['data'].pop('pending_recipe_leaf',None)
    if not result['success']:raise OperationError(result['error_code'] or 'niagara_scratch_user_recipe_failed',result['error'] or 'Recipe failed; actual prefix retained')
    return result

def _direct(method,arguments):
    try:values=_values(method,arguments);_pure_admission(method,values);_sync_admission(method,values)
    except Exception as error:return _rejected(method,error)
    return state.mutate(method,values,None,lambda result:_direct_action(result,method,values,state.checkpoint))

class NiagaraBridgeResponseError(RuntimeError):
    """Carry a rejected bridge response through composite Niagara recipes."""

    def __init__(self, command_name: str, response: Dict[str, Any], *, command_effect: Literal['read', 'mutation', 'unknown']) -> None:
        self.command_name = command_name
        self.response = response
        self.command_effect = command_effect
        super().__init__(f'Rejected Unreal bridge response for {command_name}')

def _niagara_bridge_failure_fields(exc: BaseException) -> Dict[str, Any]:
    if not isinstance(exc, NiagaraBridgeResponseError):
        return {}
    response_field = 'read_unreal_response' if exc.command_effect == 'read' else 'unreal_response'
    return {response_field: exc.response, 'failed_bridge_command': exc.command_name, 'failed_bridge_command_effect': exc.command_effect}

def _niagara_partial_mutation_fields(completed_mutation_commands: list[str]) -> Dict[str, Any]:
    if not completed_mutation_commands:
        return {}
    return {'partial_mutation': True, 'mutation_performed': True, 'completed_mutation_count': len(completed_mutation_commands), 'completed_mutation_commands': list(completed_mutation_commands), 'do_not_retry': True, 'recovery_policy': 'inspect_current_niagara_graph_and_stack_before_retry', 'recovery_steps': ['inspect_current_niagara_graph_and_stack', 'reconcile_completed_recipe_stages', 'retry_only_the_verified_missing_stage'], 'verification': {'status': 'unverified', 'method': 'composite_recipe_postcondition_not_completed'}}

def _niagara_error(message: str, *, error_code: str='niagara_tool_failed', cause: BaseException | None=None, **fields: Any) -> Dict[str, Any]:
    if cause is not None:
        for key, value in _niagara_bridge_failure_fields(cause).items():
            fields.setdefault(key, value)
    return tool_error(message, error_code=error_code, **fields)

def _recipe_snow_seed(
    target_system_path: str,
    preset: str = "snow",
    emitter_name: str = "Emitter",
    material_path: str = "",
    use_project_weather_materials: bool = True,
    scratch_pad_name: str = "MCP_VisibleSizeScratch",
    parameter_name: str = "Particles.SpriteSize",
    parameter_value: Any | None = None,
    user_parameter_name: str = "User.MCP_VisibleScratchPad_SpriteSize",
    user_parameter_default_value: Any | None = None,
    target_usage: str = "ParticleUpdateScript",
    spawn_rate: float | None = 180.0,
    velocity: list | None = None,
    sprite_size: list | None = None,
    lifetime: float | None = 10.0,
    add_spawn_box_location: bool | None = True,
    spawn_box_size: list | None = None,
    spawn_box_offset: list | None = None,
    add_weather_detail_modules: bool | None = True,
    allow_source_edit: bool = False,
    save: bool = True,
    request_compile: bool = True,
    wait_for_completion: bool = True,
    timeout_seconds: int = 180,
) -> Dict[str, Any]:
    """Create a visible snow-like temp seed and add a Scratch Pad User sprite-size control."""

    completed_mutation_commands: list[str] = []

    def command(command_name, params):
        result = (yield (command_name, params))
        if not isinstance(result, dict):
            raise RuntimeError('Invalid canonical leaf result for ' + command_name)
        if result.get('success') is False:
            raise NiagaraBridgeResponseError(command_name, result, command_effect='mutation')
        completed_mutation_commands.append(command_name)
        return result

    if not target_system_path:
        return _niagara_error("target_system_path is required", error_code="niagara_missing_required_parameter")
    if parameter_value is None:
        parameter_value = [36.0, 36.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [56.0, 56.0]
    if velocity is None:
        velocity = [0.0, 0.0, -90.0]
    if sprite_size is None:
        sprite_size = [32.0, 32.0]
    if spawn_box_size is None:
        spawn_box_size = [1200.0, 1200.0, 700.0]
    if spawn_box_offset is None:
        spawn_box_offset = [0.0, 0.0, 300.0]

    try:
        preset_params: Dict[str, Any] = {
            "target_system_path": target_system_path,
            "preset": preset,
            "emitter_name": emitter_name,
            "material_path": material_path,
            "use_project_weather_materials": use_project_weather_materials,
            "add_spawn_box_location": add_spawn_box_location,
            "spawn_box_size": spawn_box_size,
            "spawn_box_offset": spawn_box_offset,
            "add_weather_detail_modules": add_weather_detail_modules,
            "allow_non_temp_path": allow_source_edit,
            "overwrite_existing": False,
            "save": True,
            "wait_for_completion": True,
            "timeout_seconds": timeout_seconds,
        }
        if spawn_rate is not None:
            preset_params["spawn_rate"] = spawn_rate
        if velocity is not None:
            preset_params["velocity"] = velocity
        if sprite_size is not None:
            preset_params["sprite_size"] = sprite_size
        if lifetime is not None:
            preset_params["lifetime"] = lifetime

        created_system = (yield from command("create_sprite_niagara_preset_system", preset_params))
        created_system_path = str(
            created_system.get("system_path")
            or created_system.get("target_system_path")
            or target_system_path
        )

        scratch_pad_recipe = yield from _recipe_vec2(
            target_system_path=created_system_path,
            scratch_pad_name=scratch_pad_name,
            parameter_name=parameter_name,
            parameter_value=parameter_value,
            user_parameter_name=user_parameter_name,
            user_parameter_default_value=user_parameter_default_value,
            target_usage=target_usage,
            target_stack_emitter_index=0,
            stack_suggested_name=scratch_pad_name,
            allow_source_edit=allow_source_edit,
            save=save,
            request_compile=request_compile,
            wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,
        )
        if scratch_pad_recipe.get("success") is False:
            inner_completed = scratch_pad_recipe.get("completed_mutation_commands")
            combined_completed = list(completed_mutation_commands)
            if isinstance(inner_completed, list):
                combined_completed.extend(
                    str(command_name)
                    for command_name in inner_completed
                    if isinstance(command_name, str) and command_name
                )
            bridge_failure_fields = {
                key: scratch_pad_recipe[key]
                for key in (
                    "unreal_response",
                    "read_unreal_response",
                    "failed_bridge_command",
                    "failed_bridge_command_effect",
                )
                if key in scratch_pad_recipe
            }
            return _niagara_error(
                "Scratch Pad recipe failed during visible snow seed creation",
                error_code="niagara_visible_seed_scratch_pad_failed",
                created_system=created_system,
                scratch_pad_recipe=scratch_pad_recipe,
                **bridge_failure_fields,
                **_niagara_partial_mutation_fields(combined_completed),
            )

        return {
            "success": True,
            "system_path": created_system_path,
            "preset": preset,
            "material_path": material_path,
            "use_project_weather_materials": use_project_weather_materials,
            "scratch_pad_script_path": scratch_pad_recipe.get("scratch_pad_script_path"),
            "scratch_pad_script_name": scratch_pad_recipe.get("scratch_pad_script_name"),
            "parameter_name": parameter_name,
            "parameter_type": "Vector 2D",
            "parameter_value": parameter_value,
            "user_parameter_name": scratch_pad_recipe.get("user_parameter_name", user_parameter_name),
            "user_parameter_default_value": user_parameter_default_value,
            "created_system": created_system,
            "scratch_pad_recipe": scratch_pad_recipe,
            "compile_status": scratch_pad_recipe.get("compile_status"),
            "write_scope": "visible_scratch_pad_snow_seed_recipe",
            "saved": bool(save),
            "compile_requested": request_compile,
        }
    except Exception as exc:
        error_msg = f"Error creating visible Scratch Pad snow seed: {exc}"
        logger.error(error_msg)
        return _niagara_error(
            error_msg,
            cause=exc,
            **_niagara_partial_mutation_fields(completed_mutation_commands),
        )

def _recipe_vec2(target_system_path: str, scratch_pad_name: str='MCP_BlankUpdateScratch', parameter_name: str='Particles.SpriteSize', parameter_value: Any | None=None, user_parameter_name: str='User.MCP_BlankScratchPad_SpriteSize', user_parameter_default_value: Any | None=None, target_usage: str='ParticleUpdateScript', target_owner_kind: str='system', target_emitter_index: int | None=None, target_emitter_name: str='', target_stack_emitter_index: int | None=0, target_stack_emitter_name: str='', stack_suggested_name: str='', allow_source_edit: bool=False, save: bool=True, request_compile: bool=True, wait_for_completion: bool=True, timeout_seconds: int=180) -> Dict[str, Any]:
    """Create a blank Scratch Pad module that writes a Vector2D User parameter."""
    completed_mutation_commands: list[str] = []

    def command(command_name, params):
        result = (yield (command_name, params))
        if not isinstance(result, dict):
            raise RuntimeError('Invalid canonical leaf result for ' + command_name)
        effect = 'read' if command_name == 'inspect_niagara_compile_status' else 'mutation'
        if result.get('success') is False:
            raise NiagaraBridgeResponseError(command_name, result, command_effect=effect)
        if effect == 'mutation':
            completed_mutation_commands.append(command_name)
        return result
    if not target_system_path:
        return _niagara_error('target_system_path is required', error_code='niagara_missing_required_parameter')
    if parameter_value is None:
        parameter_value = [24.0, 24.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [42.0, 42.0]
    if not user_parameter_name:
        return _niagara_error('user_parameter_name is required', error_code='niagara_missing_required_parameter')
    if not parameter_name or '.' not in parameter_name:
        return _niagara_error('parameter_name must include a Niagara namespace, for example Particles.SpriteSize', error_code='niagara_invalid_parameter_name')
    try:
        stages: list[Dict[str, Any]] = []
        create_params: Dict[str, Any] = {'target_system_path': target_system_path, 'new_script_name': scratch_pad_name, 'target_supported_usage': target_usage, 'target_owner_kind': target_owner_kind, 'target_emitter_name': target_emitter_name, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}
        if target_emitter_index is not None:
            create_params['target_emitter_index'] = target_emitter_index
        created_scratch = (yield from command('create_or_duplicate_scratch_pad_module', create_params))
        scratch_pad_path = str(created_scratch.get('new_script_path') or '')
        scratch_pad_script_name = str(created_scratch.get('new_script_name') or '')
        stages.append({'stage': 'create_or_duplicate_scratch_pad_module', 'result': created_scratch})
        parameter_write = (yield from command('add_niagara_scratch_pad_parameter_write', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'parameter_name': parameter_name, 'parameter_type': 'vec2', 'value': parameter_value, 'skip_if_exists': True, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_parameter_write', 'result': parameter_write})
        user_link = (yield from command('link_niagara_scratch_pad_pin_to_user_parameter', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_pin_name': parameter_name, 'user_parameter_name': user_parameter_name, 'default_value': user_parameter_default_value, 'overwrite_existing': False, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'link_niagara_scratch_pad_pin_to_user_parameter', 'result': user_link})
        stack_params: Dict[str, Any] = {'target_system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_usage': target_usage, 'target_emitter_name': target_stack_emitter_name, 'suggested_name': stack_suggested_name or scratch_pad_name, 'skip_if_duplicate': True, 'allow_source_edit': allow_source_edit, 'save': save, 'request_compile': request_compile}
        if target_stack_emitter_index is not None:
            stack_params['target_emitter_index'] = target_stack_emitter_index
        stack_insert = (yield from command('add_scratch_pad_module_to_stack', stack_params))
        stages.append({'stage': 'add_scratch_pad_module_to_stack', 'result': stack_insert})
        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = (yield from command('inspect_niagara_compile_status', {'system_path': target_system_path, 'request_compile': False, 'wait_for_completion': True, 'timeout_seconds': timeout_seconds}))
            stages.append({'stage': 'inspect_niagara_compile_status', 'result': compile_status})
        return {'success': True, 'system_path': stack_insert.get('system_path', target_system_path), 'scratch_pad_script_path': scratch_pad_path, 'scratch_pad_script_name': scratch_pad_script_name or user_link.get('scratch_pad_script_name'), 'target_usage': target_usage, 'parameter_name': parameter_name, 'parameter_type': 'Vector 2D', 'parameter_value': parameter_value, 'user_parameter_name': user_link.get('user_parameter_name', user_parameter_name), 'linked_parameter_name': user_link.get('linked_parameter_name', user_parameter_name), 'user_parameter_default_value': user_parameter_default_value, 'user_parameter_created': user_link.get('user_parameter_created'), 'parameter_map_set_node_name': user_link.get('parameter_map_set_node_name'), 'parameter_map_set_node_guid': user_link.get('parameter_map_set_node_guid'), 'new_link_count': user_link.get('new_link_count'), 'new_module_node_name': stack_insert.get('new_module_node_name'), 'new_module_function_name': stack_insert.get('new_module_function_name'), 'compile_status': compile_status, 'stages': stages, 'write_scope': 'scratch_pad_vec2_user_parameter_module_recipe', 'saved': stack_insert.get('saved', False), 'compile_requested': request_compile}
    except Exception as exc:
        error_msg = f'Error creating Scratch Pad Vector2D User-parameter module: {exc}'
        logger.error(error_msg)
        return _niagara_error(error_msg, cause=exc, **_niagara_partial_mutation_fields(completed_mutation_commands))

def _recipe_color(target_system_path: str, scratch_pad_name: str='MCP_ColorScratch', parameter_name: str='Particles.Color', parameter_value: Any | None=None, user_parameter_name: str='User.MCP_ScratchPad_Color', user_parameter_default_value: Any | None=None, target_usage: str='ParticleUpdateScript', target_owner_kind: str='system', target_emitter_index: int | None=None, target_emitter_name: str='', target_stack_emitter_index: int | None=0, target_stack_emitter_name: str='', stack_suggested_name: str='', allow_source_edit: bool=False, save: bool=True, request_compile: bool=True, wait_for_completion: bool=True, timeout_seconds: int=180) -> Dict[str, Any]:
    """Create a blank Scratch Pad module that writes a Linear Color User parameter."""
    completed_mutation_commands: list[str] = []

    def command(command_name, params):
        result = (yield (command_name, params))
        if not isinstance(result, dict):
            raise RuntimeError('Invalid canonical leaf result for ' + command_name)
        effect = 'read' if command_name == 'inspect_niagara_compile_status' else 'mutation'
        if result.get('success') is False:
            raise NiagaraBridgeResponseError(command_name, result, command_effect=effect)
        if effect == 'mutation':
            completed_mutation_commands.append(command_name)
        return result
    if not target_system_path:
        return _niagara_error('target_system_path is required', error_code='niagara_missing_required_parameter')
    if parameter_value is None:
        parameter_value = [0.35, 0.65, 1.0, 1.0]
    if user_parameter_default_value is None:
        user_parameter_default_value = [1.0, 0.45, 0.18, 1.0]
    if not user_parameter_name:
        return _niagara_error('user_parameter_name is required', error_code='niagara_missing_required_parameter')
    if not parameter_name or '.' not in parameter_name:
        return _niagara_error('parameter_name must include a Niagara namespace, for example Particles.Color', error_code='niagara_invalid_parameter_name')
    try:
        stages: list[Dict[str, Any]] = []
        create_params: Dict[str, Any] = {'target_system_path': target_system_path, 'new_script_name': scratch_pad_name, 'target_supported_usage': target_usage, 'target_owner_kind': target_owner_kind, 'target_emitter_name': target_emitter_name, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}
        if target_emitter_index is not None:
            create_params['target_emitter_index'] = target_emitter_index
        created_scratch = (yield from command('create_or_duplicate_scratch_pad_module', create_params))
        scratch_pad_path = str(created_scratch.get('new_script_path') or '')
        scratch_pad_script_name = str(created_scratch.get('new_script_name') or '')
        stages.append({'stage': 'create_or_duplicate_scratch_pad_module', 'result': created_scratch})
        parameter_write = (yield from command('add_niagara_scratch_pad_parameter_write', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'parameter_name': parameter_name, 'parameter_type': 'color', 'value': parameter_value, 'skip_if_exists': True, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_parameter_write', 'result': parameter_write})
        user_link = (yield from command('link_niagara_scratch_pad_pin_to_user_parameter', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_pin_name': parameter_name, 'user_parameter_name': user_parameter_name, 'default_value': user_parameter_default_value, 'overwrite_existing': False, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'link_niagara_scratch_pad_pin_to_user_parameter', 'result': user_link})
        stack_params: Dict[str, Any] = {'target_system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_usage': target_usage, 'target_emitter_name': target_stack_emitter_name, 'suggested_name': stack_suggested_name or scratch_pad_name, 'skip_if_duplicate': True, 'allow_source_edit': allow_source_edit, 'save': save, 'request_compile': request_compile}
        if target_stack_emitter_index is not None:
            stack_params['target_emitter_index'] = target_stack_emitter_index
        stack_insert = (yield from command('add_scratch_pad_module_to_stack', stack_params))
        stages.append({'stage': 'add_scratch_pad_module_to_stack', 'result': stack_insert})
        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = (yield from command('inspect_niagara_compile_status', {'system_path': target_system_path, 'request_compile': False, 'wait_for_completion': True, 'timeout_seconds': timeout_seconds}))
            stages.append({'stage': 'inspect_niagara_compile_status', 'result': compile_status})
        return {'success': True, 'system_path': stack_insert.get('system_path', target_system_path), 'scratch_pad_script_path': scratch_pad_path, 'scratch_pad_script_name': scratch_pad_script_name or user_link.get('scratch_pad_script_name'), 'target_usage': target_usage, 'parameter_name': parameter_name, 'parameter_type': 'Linear Color', 'parameter_value': parameter_value, 'user_parameter_name': user_link.get('user_parameter_name', user_parameter_name), 'linked_parameter_name': user_link.get('linked_parameter_name', user_parameter_name), 'user_parameter_default_value': user_parameter_default_value, 'user_parameter_created': user_link.get('user_parameter_created'), 'parameter_map_set_node_name': user_link.get('parameter_map_set_node_name'), 'parameter_map_set_node_guid': user_link.get('parameter_map_set_node_guid'), 'new_link_count': user_link.get('new_link_count'), 'new_module_node_name': stack_insert.get('new_module_node_name'), 'new_module_function_name': stack_insert.get('new_module_function_name'), 'compile_status': compile_status, 'stages': stages, 'write_scope': 'scratch_pad_color_user_parameter_module_recipe', 'saved': stack_insert.get('saved', False), 'compile_requested': request_compile}
    except Exception as exc:
        error_msg = f'Error creating Scratch Pad Linear Color User-parameter module: {exc}'
        logger.error(error_msg)
        return _niagara_error(error_msg, cause=exc, **_niagara_partial_mutation_fields(completed_mutation_commands))

def _recipe_bool_user(target_system_path: str, scratch_pad_name: str='MCP_BoolUserScratch', parameter_name: str='Particles.MCPBoolProbe', parameter_value: Any | None=None, module_input_name: str='MCPBoolInput', module_input_default_value: Any | None=None, user_parameter_name: str='User.MCP_ScratchPad_Bool', user_parameter_default_value: Any | None=None, target_usage: str='ParticleUpdateScript', target_owner_kind: str='system', target_emitter_index: int | None=None, target_emitter_name: str='', target_stack_emitter_index: int | None=0, target_stack_emitter_name: str='', stack_suggested_name: str='', overwrite_existing: bool=False, allow_source_edit: bool=False, save: bool=True, request_compile: bool=True, wait_for_completion: bool=True, timeout_seconds: int=180) -> Dict[str, Any]:
    """Create a blank Scratch Pad bool module input bound to a User parameter."""
    completed_mutation_commands: list[str] = []

    def command(command_name, params):
        result = (yield (command_name, params))
        if not isinstance(result, dict):
            raise RuntimeError('Invalid canonical leaf result for ' + command_name)
        effect = 'read' if command_name == 'inspect_niagara_compile_status' else 'mutation'
        if result.get('success') is False:
            raise NiagaraBridgeResponseError(command_name, result, command_effect=effect)
        if effect == 'mutation':
            completed_mutation_commands.append(command_name)
        return result
    if not target_system_path:
        return _niagara_error('target_system_path is required', error_code='niagara_missing_required_parameter')
    if parameter_value is None:
        parameter_value = True
    if module_input_default_value is None:
        module_input_default_value = True
    if user_parameter_default_value is None:
        user_parameter_default_value = False
    if not user_parameter_name:
        return _niagara_error('user_parameter_name is required', error_code='niagara_missing_required_parameter')
    if not module_input_name:
        return _niagara_error('module_input_name is required', error_code='niagara_missing_required_parameter')
    if not parameter_name or '.' not in parameter_name:
        return _niagara_error('parameter_name must include a Niagara namespace, for example Particles.MCPBoolProbe', error_code='niagara_invalid_parameter_name')
    try:
        stages: list[Dict[str, Any]] = []
        create_params: Dict[str, Any] = {'target_system_path': target_system_path, 'new_script_name': scratch_pad_name, 'target_supported_usage': target_usage, 'target_owner_kind': target_owner_kind, 'target_emitter_name': target_emitter_name, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}
        if target_emitter_index is not None:
            create_params['target_emitter_index'] = target_emitter_index
        created_scratch = (yield from command('create_or_duplicate_scratch_pad_module', create_params))
        scratch_pad_path = str(created_scratch.get('new_script_path') or '')
        scratch_pad_script_name = str(created_scratch.get('new_script_name') or '')
        stages.append({'stage': 'create_or_duplicate_scratch_pad_module', 'result': created_scratch})
        parameter_write = (yield from command('add_niagara_scratch_pad_parameter_write', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'parameter_name': parameter_name, 'parameter_type': 'bool', 'value': parameter_value, 'skip_if_exists': True, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_parameter_write', 'result': parameter_write})
        module_input = (yield from command('add_niagara_scratch_pad_module_input', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'input_name': module_input_name, 'input_type': 'bool', 'default_value': module_input_default_value, 'consumer_pin_name': parameter_name, 'require_consumer_link': True, 'overwrite_existing_link': False, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_module_input', 'result': module_input})
        stack_params: Dict[str, Any] = {'target_system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_usage': target_usage, 'target_emitter_name': target_stack_emitter_name, 'suggested_name': stack_suggested_name or scratch_pad_name, 'skip_if_duplicate': True, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}
        if target_stack_emitter_index is not None:
            stack_params['target_emitter_index'] = target_stack_emitter_index
        stack_insert = (yield from command('add_scratch_pad_module_to_stack', stack_params))
        module_node_guid = str(stack_insert.get('new_module_node_guid') or stack_insert.get('existing_module_node_guid') or '')
        if not module_node_guid:
            raise RuntimeError(f'Scratch Pad stack insertion did not return a module node GUID: {stack_insert}')
        stages.append({'stage': 'add_scratch_pad_module_to_stack', 'result': stack_insert})
        user_link_params: Dict[str, Any] = {'system_path': target_system_path, 'input_name': module_input_name, 'user_parameter_name': user_parameter_name, 'default_value': user_parameter_default_value, 'emitter_name': target_stack_emitter_name, 'module_node_guid': module_node_guid, 'overwrite_existing': overwrite_existing, 'allow_source_edit': allow_source_edit, 'save': save, 'request_compile': request_compile, 'wait_for_completion': wait_for_completion, 'timeout_seconds': timeout_seconds, 'poll_interval_seconds': 0.1}
        if target_stack_emitter_index is not None:
            user_link_params['emitter_index'] = target_stack_emitter_index
        user_link = (yield from command('set_niagara_module_input_user_parameter', user_link_params))
        stages.append({'stage': 'set_niagara_module_input_user_parameter', 'result': user_link})
        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = (yield from command('inspect_niagara_compile_status', {'system_path': target_system_path, 'request_compile': False, 'wait_for_completion': True, 'timeout_seconds': timeout_seconds}))
            stages.append({'stage': 'inspect_niagara_compile_status', 'result': compile_status})
        return {'success': True, 'system_path': user_link.get('system_path', stack_insert.get('system_path', target_system_path)), 'scratch_pad_script_path': scratch_pad_path, 'scratch_pad_script_name': scratch_pad_script_name or module_input.get('scratch_pad_script_name'), 'target_usage': target_usage, 'parameter_name': parameter_name, 'parameter_type': 'bool', 'parameter_value': parameter_value, 'module_input_name': user_link.get('input_name', module_input.get('input_name', module_input_name)), 'module_input_default_value': module_input_default_value, 'module_input_type': user_link.get('input_type', module_input.get('input_type', 'bool')), 'consumer_pin_name': module_input.get('consumer_pin_name', parameter_name), 'new_consumer_link_count': module_input.get('new_consumer_link_count'), 'user_parameter_name': user_link.get('user_parameter_name', user_parameter_name), 'linked_parameter_name': user_link.get('linked_parameter_name', user_parameter_name), 'user_parameter_default_value': user_parameter_default_value, 'user_parameter_created': user_link.get('user_parameter_created'), 'default_value_set': user_link.get('default_value_set'), 'default_value': user_link.get('default_value'), 'previous_override_link_count': user_link.get('previous_override_link_count'), 'overwrote_existing': user_link.get('overwrote_existing'), 'new_module_node_guid': module_node_guid, 'new_module_node_name': stack_insert.get('new_module_node_name') or stack_insert.get('existing_module_node_name'), 'new_module_function_name': stack_insert.get('new_module_function_name') or stack_insert.get('existing_module_function_name'), 'compile_status': compile_status, 'stages': stages, 'write_scope': 'scratch_pad_bool_user_parameter_module_recipe', 'saved': user_link.get('saved', False), 'compile_requested': request_compile}
    except Exception as exc:
        error_msg = f'Error creating Scratch Pad bool User-parameter module: {exc}'
        logger.error(error_msg)
        return _niagara_error(error_msg, cause=exc, **_niagara_partial_mutation_fields(completed_mutation_commands))

def _recipe_visible(target_system_path: str, scratch_pad_name: str='MCP_BoolVisibleScratch', parameter_name: str='Particles.SpriteSize', parameter_value: Any | None=None, base_module_input_name: str='MCPBaseSpriteSize', base_module_input_default_value: Any | None=None, user_parameter_name: str='User.MCP_VisibleToggle', user_parameter_default_value: Any | None=None, custom_hlsl_input_name: str='MCPVisibleToggle', custom_hlsl_expression: str='BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)', custom_hlsl_output_pin_name: str='MCPVisibleSize', custom_hlsl_signature_name: str='MCP_ScratchPadBoolVisibleSize', target_usage: str='ParticleUpdateScript', target_owner_kind: str='system', target_emitter_index: int | None=None, target_emitter_name: str='', target_stack_emitter_index: int | None=0, target_stack_emitter_name: str='', stack_suggested_name: str='', overwrite_existing: bool=False, allow_source_edit: bool=False, save: bool=True, request_compile: bool=True, wait_for_completion: bool=True, timeout_seconds: int=180) -> Dict[str, Any]:
    """Create a temp Scratch Pad module where a bool User parameter toggles sprite visibility."""
    completed_mutation_commands: list[str] = []

    def command(command_name, params):
        result = (yield (command_name, params))
        if not isinstance(result, dict):
            raise RuntimeError('Invalid canonical leaf result for ' + command_name)
        effect = 'read' if command_name == 'inspect_niagara_compile_status' else 'mutation'
        if result.get('success') is False:
            raise NiagaraBridgeResponseError(command_name, result, command_effect=effect)
        if effect == 'mutation':
            completed_mutation_commands.append(command_name)
        return result
    if not target_system_path:
        return _niagara_error('target_system_path is required', error_code='niagara_missing_required_parameter')
    if parameter_value is None:
        parameter_value = [64.0, 64.0]
    if base_module_input_default_value is None:
        base_module_input_default_value = parameter_value
    if user_parameter_default_value is None:
        user_parameter_default_value = True
    if not user_parameter_name:
        return _niagara_error('user_parameter_name is required', error_code='niagara_missing_required_parameter')
    if not base_module_input_name:
        return _niagara_error('base_module_input_name is required', error_code='niagara_missing_required_parameter')
    if not custom_hlsl_input_name:
        return _niagara_error('custom_hlsl_input_name is required', error_code='niagara_missing_required_parameter')
    if not parameter_name or '.' not in parameter_name:
        return _niagara_error('parameter_name must include a Niagara namespace, for example Particles.SpriteSize', error_code='niagara_invalid_parameter_name')
    if overwrite_existing:
        return _niagara_error('overwrite_existing is not admitted for bool visible Scratch Pad toggle creation', error_code='niagara_overwrite_not_allowed')
    try:
        stages: list[Dict[str, Any]] = []
        create_params: Dict[str, Any] = {'target_system_path': target_system_path, 'new_script_name': scratch_pad_name, 'target_supported_usage': target_usage, 'target_owner_kind': target_owner_kind, 'target_emitter_name': target_emitter_name, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}
        if target_emitter_index is not None:
            create_params['target_emitter_index'] = target_emitter_index
        created_scratch = (yield from command('create_or_duplicate_scratch_pad_module', create_params))
        scratch_pad_path = str(created_scratch.get('new_script_path') or '')
        scratch_pad_script_name = str(created_scratch.get('new_script_name') or '')
        stages.append({'stage': 'create_or_duplicate_scratch_pad_module', 'result': created_scratch})
        parameter_write = (yield from command('add_niagara_scratch_pad_parameter_write', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'parameter_name': parameter_name, 'parameter_type': 'vec2', 'value': parameter_value, 'skip_if_exists': True, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_parameter_write', 'result': parameter_write})
        base_module_input = (yield from command('add_niagara_scratch_pad_module_input', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'input_name': base_module_input_name, 'input_type': 'vec2', 'default_value': base_module_input_default_value, 'consumer_pin_name': parameter_name, 'require_consumer_link': True, 'overwrite_existing_link': False, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'add_niagara_scratch_pad_module_input', 'result': base_module_input})
        user_upsert = (yield from command('upsert_niagara_user_parameters', {'system_path': target_system_path, 'parameters': [{'name': user_parameter_name, 'type': 'bool', 'default_value': user_parameter_default_value}], 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False, 'continue_on_error': False}))
        stages.append({'stage': 'upsert_niagara_user_parameters', 'result': user_upsert})
        custom_hlsl = (yield from command('insert_niagara_scratch_pad_custom_hlsl_for_pin', {'system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'scratch_pad_owner_kind': target_owner_kind, 'scratch_pad_name': scratch_pad_script_name or scratch_pad_name, 'target_pin_name': parameter_name, 'expression': custom_hlsl_expression, 'preserve_existing_link_as': 'BaseValue', 'output_pin_name': custom_hlsl_output_pin_name, 'signature_name': custom_hlsl_signature_name, 'user_parameter_inputs': [{'input_name': custom_hlsl_input_name, 'user_parameter_name': user_parameter_name}], 'skip_if_already_inserted': True, 'replace_existing_custom': False, 'rebuild_existing_custom': False, 'requires_context': True, 'delete_unlinked_custom_nodes': False, 'delete_unlinked_custom_input_source_nodes': False, 'allow_source_edit': allow_source_edit, 'save': False, 'request_compile': False}))
        stages.append({'stage': 'insert_niagara_scratch_pad_custom_hlsl_for_pin', 'result': custom_hlsl})
        stack_params: Dict[str, Any] = {'target_system_path': target_system_path, 'scratch_pad_script_path': scratch_pad_path, 'target_usage': target_usage, 'target_emitter_name': target_stack_emitter_name, 'suggested_name': stack_suggested_name or scratch_pad_name, 'skip_if_duplicate': True, 'allow_source_edit': allow_source_edit, 'save': save, 'request_compile': request_compile}
        if target_stack_emitter_index is not None:
            stack_params['target_emitter_index'] = target_stack_emitter_index
        stack_insert = (yield from command('add_scratch_pad_module_to_stack', stack_params))
        module_node_guid = str(stack_insert.get('new_module_node_guid') or stack_insert.get('existing_module_node_guid') or '')
        if not module_node_guid:
            raise RuntimeError(f'Scratch Pad stack insertion did not return a module node GUID: {stack_insert}')
        stages.append({'stage': 'add_scratch_pad_module_to_stack', 'result': stack_insert})
        compile_status: Dict[str, Any] | None = None
        if request_compile and wait_for_completion:
            compile_status = (yield from command('inspect_niagara_compile_status', {'system_path': target_system_path, 'request_compile': False, 'wait_for_completion': True, 'timeout_seconds': timeout_seconds}))
            stages.append({'stage': 'inspect_niagara_compile_status', 'result': compile_status})
        return {'success': True, 'system_path': stack_insert.get('system_path', target_system_path), 'scratch_pad_script_path': scratch_pad_path, 'scratch_pad_script_name': scratch_pad_script_name or base_module_input.get('scratch_pad_script_name') or custom_hlsl.get('scratch_pad_script_name'), 'target_usage': target_usage, 'parameter_name': parameter_name, 'parameter_type': 'vec2', 'parameter_value': parameter_value, 'base_module_input_name': base_module_input.get('input_name', base_module_input_name), 'base_module_input_type': base_module_input.get('input_type', 'vec2'), 'base_module_input_default_value': base_module_input_default_value, 'base_consumer_pin_name': base_module_input.get('consumer_pin_name', parameter_name), 'base_new_consumer_link_count': base_module_input.get('new_consumer_link_count'), 'user_parameter_name': user_parameter_name if user_parameter_name.startswith('User.') else f'User.{user_parameter_name}', 'user_parameter_default_value': user_parameter_default_value, 'user_parameter_created': bool(user_upsert.get('added_count')), 'custom_hlsl_input_name': custom_hlsl_input_name, 'custom_hlsl_expression': custom_hlsl.get('expression', custom_hlsl_expression), 'custom_hlsl_output_pin_name': custom_hlsl.get('output_pin_name', custom_hlsl_output_pin_name), 'custom_hlsl_signature_name': custom_hlsl_signature_name, 'custom_hlsl_node_guid': custom_hlsl.get('custom_hlsl_node_guid'), 'custom_hlsl_previous_link_count': custom_hlsl.get('previous_link_count'), 'custom_hlsl_user_parameter_input_count': custom_hlsl.get('user_parameter_input_count'), 'custom_hlsl_created_user_parameter_input_node_count': custom_hlsl.get('created_user_parameter_input_node_count'), 'custom_hlsl_auto_added_context_map_input': custom_hlsl.get('auto_added_context_map_input'), 'custom_hlsl_custom_inputs': custom_hlsl.get('custom_inputs'), 'new_module_node_guid': module_node_guid, 'new_module_node_name': stack_insert.get('new_module_node_name') or stack_insert.get('existing_module_node_name'), 'new_module_function_name': stack_insert.get('new_module_function_name') or stack_insert.get('existing_module_function_name'), 'compile_status': compile_status, 'stages': stages, 'write_scope': 'scratch_pad_bool_visible_toggle_module_recipe', 'saved': stack_insert.get('saved', False), 'compile_requested': request_compile}
    except Exception as exc:
        error_msg = f'Error creating Scratch Pad bool visible toggle module: {exc}'
        logger.error(error_msg)
        return _niagara_error(error_msg, cause=exc, **_niagara_partial_mutation_fields(completed_mutation_commands))


_MODULE_INPUT_BINDINGS=('cleanup_niagara_renderers','remap_niagara_script_parameter_collection_references','remove_niagara_unused_module_input_path','set_niagara_module_input_user_parameter','set_niagara_array_data_interface_module_input_user_parameter')

def _single_path_domain(method):
    if method=='remap_niagara_script_parameter_collection_references':
        from . import niagara_parameter_collections as binding
    elif method=='cleanup_niagara_renderers':
        from . import niagara_renderers as binding
    else:
        from . import niagara_module_input_paths as binding
    return binding

class _RecipeJournal:
    def __init__(self,method,values,run_id):
        self.result=state.new_result(run_id);self.path=state._journal_path(run_id);self.started=time.perf_counter();self.owned=False
        if method in _MODULE_INPUT_BINDINGS:
            binding=_single_path_domain(method)
            binding._context(self.result,method,values['args'],values['original'],None)
        else:_context(self.result,method,values,'Named_official_MCP_only')
        if self.path.exists():raise OperationError('operation_incomplete','Recipe operation IDs are never replayed')
        if state._active_operation is not None:raise OperationError('mutation_busy','Another operation owns the parent slot')
        record=dict(schema_version=1,operation_id=run_id,operation=method,arguments=copy.deepcopy(values),fingerprint=hashlib.sha256(json.dumps(dict(name=method,arguments=values),sort_keys=True,allow_nan=False).encode()).hexdigest(),state='started',result=self.result,engine_version=unreal.SystemLibrary.get_engine_version(),process_id=os.getpid())
        encoded=json.dumps(record,ensure_ascii=False,allow_nan=False);self.path.parent.mkdir(parents=True,exist_ok=True)
        with self.path.open('x',encoding='utf-8') as stream:stream.write(encoded);stream.flush();os.fsync(stream.fileno())
        state._active_operation=run_id;self.owned=True
    def checkpoint(self):
        if not self.owned or state._active_operation!=self.result['operation_id']:raise OperationError('journal_checkpoint_not_owned','Recipe lease changed; no later writer or replay')
        state.checkpoint(self.result)
    def finish(self,recovery):
        if not self.owned or state._active_operation!=self.result['operation_id']:raise OperationError('journal_finalize_not_owned','Recipe lease changed before finalization')
        with self.path.open('r',encoding='utf-8') as stream:record=json.load(stream)
        if record.get('state')!='started' or record.get('operation_id')!=self.result['operation_id'] or record.get('process_id')!=os.getpid():raise OperationError('journal_finalize_not_owned','Started parent journal differs')
        record['result']=copy.deepcopy(self.result);record['state']='started' if recovery else 'complete';state._write_record(self.path,record)
        if not recovery:state._active_operation=None;self.owned=False

def _seal(entry,recovery):
    if entry.get('finalization_attempted'):raise OperationError('niagara_scratch_user_recipe_final_replay','Finalization is not repeated')
    result=entry['result'];result['data']['elapsed_seconds']=round(time.perf_counter()-entry['journal'].started,6)
    entry['sealed_result']=copy.deepcopy(result);entry['finalization_attempted']=True
    entry['journal'].finish(recovery);entry['journal_finalized']=True

def _delivery_failed(entry,error):
    entry['delivery_error']=str(error)[:1024];_QUARANTINED[entry['run_id']]=entry;_PENDING.pop(entry['run_id'],None)
    if not entry.get('cu_completion_call_entered') and not entry.get('cu_error_attempted'):
        entry['cu_error_attempted']=True
        try:entry['result_object'].set_error('Recipe delivery uncertain; no replay; operation_id='+entry['run_id']+'; '+str(error)[:700])
        except Exception as failure:entry['delivery_error']=str(failure)[:1024]

def _delivery_emission(entry):
    """Reference exact failure or successful Snow debug aliases; never edit the sealed result.

    The full same-process journal and inline public legacy payload remain the
    canonical values. Any failed/changed proof leaves the original copy intact.
    This performs file/Python work only, including an unknown retained lease.
    """
    sealed=entry['sealed_result'];emitted=copy.deepcopy(sealed)
    try:
        data=emitted.get('data');method=entry.get('method');operation_id=entry.get('run_id')
        if (entry.get('journal_finalized') is not True or entry.get('finalization_attempted') is not True
                or method not in METHODS or type(data) is not dict or emitted.get('operation_id')!=operation_id
                or data.get('operation')!=method or data.get('operation_complete') is not True
                or data.get('final_receipt_sealed_before_delivery') is not True
                or type(operation_id) is not str or re.fullmatch(r'[A-Za-z0-9][A-Za-z0-9_.-]{0,95}',operation_id) is None):
            return emitted
        legacy=data.get('legacy_result');response=legacy.get('unreal_response') if type(legacy) is dict else None
        command=legacy.get('failed_bridge_command') if type(legacy) is dict else None
        successful_snow=(emitted.get('success') is True and method=='create_niagara_visible_scratch_pad_snow_seed'
            and type(legacy) is dict and type(legacy.get('created_system')) is dict
            and data.get('mutation_state_unknown') is False and data.get('workflow_recovery_required') is False)
        extra_alias=None
        canonical_pointer='/data/legacy_result/unreal_response/stage_results'
        if successful_snow:
            response=legacy['created_system'];command='create_sprite_niagara_preset_system'
            stages=data.get('recipe_stages')
            if (response.get('success') is not True or response.get('command')!=command or response.get('error_code')
                    or type(stages) is not list or not stages or type(stages[0]) is not dict
                    or stages[0].get('stage')!=command or type(stages[0].get('result')) is not dict):return emitted
            extra_alias=stages[0]['result']
            canonical_pointer='/data/legacy_result/created_system/stage_results'
        elif (emitted.get('success') is not False or type(response) is not dict or response.get('success') is not False
                or type(command) is not str or response.get('command')!=command):
            return emitted
        if type(response.get('stage_results')) is not list:return emitted
        compact=lambda value:json.dumps(value,ensure_ascii=False,sort_keys=True,separators=(',',':'),allow_nan=False).encode('utf-8')
        canonical=response['stage_results'];canonical_bytes=compact(canonical);matches=[]
        if successful_snow and compact(extra_alias)!=compact(response):return emitted
        for receipt in data.get('native_receipts',[]):
            if (type(receipt) is not dict or receipt.get('operation')!=command or receipt.get('success') is not response.get('success')
                    or receipt.get('pending') is not False or (bool(receipt.get('error_code')) if successful_snow else receipt.get('error_code')!=response.get('error_code'))
                    or (successful_snow and receipt.get('index')!=0)):
                continue
            diagnostic=receipt.get('diagnostics');session=diagnostic.get('session_execution') if type(diagnostic) is dict else None
            if (type(session) is dict and type(diagnostic.get('stage_results')) is list and type(session.get('stage_results')) is list
                    and compact(diagnostic['stage_results'])==canonical_bytes and compact(session['stage_results'])==canonical_bytes):
                matches.append((receipt,diagnostic,session))
        if len(matches)!=1:return emitted
        recovery=data.get('mutation_state_unknown') is True
        if data.get('workflow_recovery_required') is not recovery:return emitted
        journal_owner=entry['journal']
        def lease_current():
            return (state._active_operation==operation_id and journal_owner.owned is True) if recovery else (state._active_operation is None and journal_owner.owned is False)
        if not lease_current():return emitted
        # Use the already-owned path, never state._journal_path / Unreal.Paths.
        path=journal_owner.path
        if (not isinstance(path,Path) or not path.is_absolute() or path.name!=operation_id+'.json'
                or path.is_symlink() or path.resolve(strict=True)!=path):return emitted
        values=entry['values']
        argument_identity=json.dumps(dict(name=method,arguments=values),sort_keys=True,allow_nan=False)
        expected_fingerprint=hashlib.sha256(argument_identity.encode('utf-8')).hexdigest()
        identity_text=json.dumps(sealed,ensure_ascii=False,sort_keys=True,allow_nan=False)
        # Existing in-memory result/args plus bounded journal header; not a wire
        # limit increase and not a general large-result transport mechanism.
        bound=len(identity_text.encode('utf-8'))+len(argument_identity.encode('utf-8'))+65536
        before=path.stat()
        if not 0<before.st_size<=bound:return emitted
        with path.open('rb') as stream:
            raw=stream.read(bound+1);held=os.fstat(stream.fileno())
        after=path.stat()
        file_identity=lambda item:(item.st_dev,item.st_ino,item.st_size,item.st_mtime_ns)
        if len(raw)!=before.st_size or file_identity(before)!=file_identity(held) or file_identity(before)!=file_identity(after):return emitted
        def unique(pairs):
            value={}
            for key,item in pairs:
                if key in value:raise ValueError('Duplicate journal JSON key')
                value[key]=item
            return value
        def finite(value):raise ValueError('Nonfinite journal JSON literal: '+value)
        journal=json.loads(raw.decode('utf-8'),object_pairs_hook=unique,parse_constant=finite)
        if (type(journal) is not dict or journal.get('schema_version')!=1
                or journal.get('state')!=('started' if recovery else 'complete')
                or journal.get('operation_id')!=operation_id or journal.get('operation')!=method
                or type(journal.get('process_id')) is not int or journal['process_id']!=os.getpid()
                or json.dumps(dict(name=method,arguments=journal.get('arguments')),sort_keys=True,allow_nan=False)!=argument_identity
                or journal.get('fingerprint')!=expected_fingerprint
                or json.dumps(journal.get('result'),ensure_ascii=False,sort_keys=True,allow_nan=False)!=identity_text
                or json.dumps(entry.get('result'),ensure_ascii=False,sort_keys=True,allow_nan=False)!=identity_text
                or json.dumps(sealed,ensure_ascii=False,sort_keys=True,allow_nan=False)!=identity_text
                or not lease_current() or file_identity(path.stat())!=file_identity(before)):
            return emitted
        reference=dict(representation='sealed_recipe_journal_reference',version=1,operation_id=operation_id,
            journal_path=str(path),journal_sha256=hashlib.sha256(raw).hexdigest(),journal_bytes=len(raw),
            json_pointer='/result'+canonical_pointer,
            inline_json_pointer=canonical_pointer,
            value_encoding='UTF-8 JSON; ensure_ascii=False, sort_keys=True, separators=(comma,colon), allow_nan=False',
            value_sha256=hashlib.sha256(canonical_bytes).hexdigest(),value_bytes=len(canonical_bytes),
            proof='Same-process operation/arguments/fingerprint/full sealed result and stable journal bytes matched. Public inline legacy and full journal are unchanged.')
        if len(compact(reference))>=len(canonical_bytes):return emitted
        receipt,diagnostic,session=matches[0]
        diagnostic['stage_results']=copy.deepcopy(reference);session['stage_results']=copy.deepcopy(reference)
        if successful_snow:stages[0]['result']=dict(extra_alias,stage_results=copy.deepcopy(reference))
        index=receipt.get('index');stages=data.get('recipe_stages')
        if (type(index) is int and index>=0 and type(stages) is list and index>=len(stages)
                and receipt.get('canonical_legacy_reference')=='data.recipe_stages['+str(index)+'].result'):
            receipt['canonical_legacy_reference']='data.legacy_result.unreal_response'
        return emitted
    except Exception:
        # Optional exact debug references must not fabricate a successful proof,
        # release ownership, rewrite durable evidence, or touch Unreal objects.
        return copy.deepcopy(sealed)


def _deliver(entry):
    if entry.get('cu_delivery_attempted'):raise OperationError('niagara_scratch_user_recipe_delivery_replay','One final delivery only')
    if not entry.get('journal_finalized'):raise OperationError('niagara_scratch_user_recipe_unsealed','Durable final receipt required')
    entry['cu_delivery_attempted']=True
    entry['result_object'].set_editor_property('value',reflected_result(_delivery_emission(entry)))
    entry['cu_completion_call_entered']=True
    if entry['result_object'].complete_value() is not True:raise OperationError('niagara_scratch_user_recipe_delivery_failed','CU completion refused; no fallback completion')
    entry['cu_completed']=True

def _finish_entry(entry):
    global _LAST_TERMINAL
    if entry.get('finalization_attempted'):return
    recovery=entry['result']['data'].get('mutation_state_unknown') is True
    entry['result']['data'].update(operation_complete=True,workflow_recovery_required=recovery,final_receipt_sealed_before_delivery=True)
    if recovery and entry['result']['success']:state.fail(entry['result'],OperationError('niagara_scratch_user_recipe_scope_unverified','Current receipt remains unknown; original legacy and actual save prefix retained'))
    try:
        _checkpoint(entry);_seal(entry,recovery)
        if recovery:_QUARANTINED[entry['run_id']]=entry
        _deliver(entry)
    except Exception as error:_delivery_failed(entry,error)
    _PENDING.pop(entry['run_id'],None);_LAST_TERMINAL=dict(operation_id=entry['run_id'],success=entry['result']['success'],recovery_required=recovery,delivery_error=entry.get('delivery_error',''))

def _adopt_module_input_binding(entry,native):
    binding=_single_path_domain(entry['method'])
    result=entry['result'];prior_compile=copy.deepcopy(result['compile']);prior_save=copy.deepcopy(result['save'])
    attempted=result['data'].get('actual_mutation_attempted') is True;changed=result['data'].get('actual_changed_observed') is True
    binding._adopt(result,entry['method'],entry['binding_args'],entry['binding_original'],None,native)
    for group,prior,fields in (('compile',prior_compile,('requested','attempted')),('save',prior_save,('requested','attempted','saved'))):
        for field in fields:result[group][field]=prior.get(field) is True or result[group].get(field) is True
    attempted=attempted or native['mutation_attempted'];changed=changed or native['changed'] is True;known=native['change_state_known']
    result['data'].update(operation=entry['method'],native_dispatched=True,actual_mutation_attempted=attempted,native_mutation_attempted=attempted,execution_mutation_attempted=attempted,actual_changed_observed=changed,change_state_known=known,mutation_state_unknown=not known,mutation_performed=changed if known else None,parent_journal_only=True,shared_HN_lifecycle=True,deferred_terminal_wait_completion_unverified=not native['data']['native_job_terminal'],operation_complete=native['data']['operation_complete'])
    result['changes']=[] if known and not attempted and not changed else [dict(kind=entry['method'],attempted=attempted,changed=changed if known else None,change_state_known=known,postcondition_passed=native['postcondition_passed'])]
    result.update(success=False,stage='returned_niagara_module_input_paths_operation')
    return result,copy.deepcopy(native['data'].get('legacy_result',{}))

def _adopt_job(entry,native,checkpoint=True):
    data=native['data'];job_id=data['native_job_id'];prior=entry.get('native_job_id')
    if prior is not None and job_id!=prior:raise OperationError('niagara_scratch_user_recipe_job_changed','Another Job snapshot was returned; no restart')
    revision=data['native_checkpoint_revision']
    if entry.get('last_revision') is not None and revision<entry['last_revision']:raise OperationError('niagara_scratch_user_recipe_revision_regressed','Same-job revision regressed')
    entry['native_job_id']=job_id;entry['last_revision']=revision
    name=entry['leaf'][0]
    if entry.get('module_input_binding'):child,legacy=_adopt_module_input_binding(entry,native)
    else:
        child=_from_native(native)
        legacy=data.get('legacy_result');legacy=copy.deepcopy(legacy) if type(legacy) is dict else {}
        _record_child(entry,name,child,legacy,pending=not data['native_job_terminal'])
    entry['result']['data'].update(active_native_job_id=job_id,active_native_job_terminal=data['native_job_terminal'],active_native_checkpoint_revision=revision,active_native_operation=name)
    if checkpoint:_checkpoint(entry)
    return child,legacy

def _job_terminal(entry,native):
    child,legacy=_adopt_job(entry,native)
    if not native['data']['native_job_terminal']:return
    if entry.get('module_input_binding'):
        if not native['success']:state.fail(entry['result'],OperationError(native.get('error_code') or 'niagara_module_input_paths_native_failed',native.get('error') or 'Native binding failed; actual prefix retained'))
        elif not native['postcondition_passed'] or not native['change_state_known']:state.fail(entry['result'],OperationError('niagara_module_input_paths_postcondition_unverified','Native final binding health is incomplete; no replay'))
        else:entry['result'].update(success=True,stage='complete',error_code='',error='')
        _finish_entry(entry);return
    error=None
    if not native['success'] or not native['postcondition_passed'] or (entry['method']=='create_niagara_visible_scratch_pad_snow_seed' and not native['change_state_known']):
        error=NiagaraBridgeResponseError(entry['leaf'][0],legacy or dict(success=False,error=native.get('error')),command_effect='read' if entry['leaf'][0]=='inspect_niagara_compile_status' else 'mutation')
    more=_accept(entry,entry['leaf'][0],legacy,error)
    if not more:_finish_entry(entry)

def _run_step(entry):
    if entry.get('finalization_attempted') or entry.get('lifecycle_interrupted'):return
    name,params=entry['leaf'];result=entry['result']
    if entry.get('module_input_binding'):
        result['stage']='before_niagara_module_input_paths_checkpoint';result['data'].update(native_binding_dispatch_intent=True,mutation_state_unknown=True,mutation_performed=None)
    else:
        result['stage']='before_niagara_scratch_user_recipes_checkpoint';result['data']['pending_recipe_leaf']=dict(index=entry['leaf_index'],operation=name)
        result['data'].update(recipe_leaf_dispatch_intent=True,mutation_state_unknown=True,mutation_performed=None)
    _checkpoint(entry)
    if name in api.METHODS and (params.get('wait_for_completion') is True or entry.get('module_input_binding')):
        entry['native_start_call_entered']=True
        try:entry['job']=api.start_job(name,params)
        except Exception as error:
            if entry['method']=='create_niagara_visible_scratch_pad_snow_seed':
                if getattr(error,'native_start_returned',False):
                    entry['unverified_returned_job']=getattr(error,'native_returned_job',None)
                    result['data']['native_start_returned']=True
                if getattr(error,'native_start_dispatched',None) is False:
                    entry['native_start_call_entered']=False
            if entry.get('module_input_binding') and getattr(error,'native_start_dispatched',None) is False:
                entry['native_start_call_entered']=False;result['data'].update(change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
            raise
        entry['job_reference_verified']=True
        if not _ACTIVE or entry.get('lifecycle_interrupted') or _PENDING.get(entry['run_id']) is not entry:raise OperationError('niagara_scratch_user_recipe_start_interrupted','Native start returned after lifecycle change; no future recipe stage')
        entry['snapshot_calls']+=1;native=api.snapshot(entry['job'],name)
        if native['data']['native_job_terminal']:_job_terminal(entry,native)
        else:_adopt_job(entry,native)
        return
    entry['synchronous_child_entered']=True
    child,legacy,error=_call_sync(entry,name,params)
    entry['synchronous_child_entered']=False
    # A synchronous SDK callback can deactivate this workflow before return.
    if not _ACTIVE or entry.get('lifecycle_interrupted') or _PENDING.get(entry['run_id']) is not entry:raise OperationError('niagara_scratch_user_recipe_child_interrupted','Child returned after lifecycle interruption; actual prefix retained and no next child')
    if not child.get('success'):error=NiagaraBridgeResponseError(name,legacy or dict(success=False,error=child.get('error')),command_effect='read' if name=='inspect_niagara_compile_status' else 'mutation')
    if not _accept(entry,name,legacy,error):_finish_entry(entry)

def _cancel_owned(entry,reason):
    if entry.get('finalization_attempted') or not entry.get('job_reference_verified') or entry.get('native_publication_quarantined'):return None
    if not entry.get('cancel_attempted'):
        entry['cancel_attempted']=True
        try:entry['cancel_acknowledgement']=entry['job'].cancel_job(str(reason)[:1024])
        except Exception as error:entry['cancel_error']=str(error)[:700]
    try:
        entry['snapshot_calls']+=1;native=api.snapshot(entry['job'],entry['leaf'][0]);_adopt_job(entry,native);return native
    except Exception as error:_retain_snapshot_error(entry,error);entry['cancel_snapshot_error']=str(error)[:700];return None

def _retain_snapshot_error(entry,error):
    if entry.get('finalization_attempted'):return
    if getattr(error,'native_unsafe_publication_quarantined',False):
        entry['native_publication_quarantined']=True;entry['result']['data'].update(unsafe_job_publication_quarantined=True,native_writers_stopped=True)
    native=getattr(error,'native_decoded_result',None)
    if type(native) is dict and entry.get('job_reference_verified') and native.get('operation')==entry['leaf'][0]:
        data=native.get('data',{});prior=entry.get('native_job_id')
        if type(data.get('native_job_id')) is str and data['native_job_id'] and (prior is None or prior==data['native_job_id']):
            # Prefix first, but unsafe/invalid public terminal never becomes final.
            if entry.get('module_input_binding'):
                retained=copy.deepcopy(native);retained['_raw_native_json']=getattr(error,'native_raw_result','')
                try:_adopt_job(entry,retained,checkpoint=False)
                except Exception as retention_error:entry['prefix_retention_error']=str(retention_error)[:700]
            else:_record_child(entry,entry['leaf'][0],_from_native(native),pending=True)

def _uncertain(entry,error):
    if entry.get('finalization_attempted'):_delivery_failed(entry,error);return
    _retain_snapshot_error(entry,error)
    native=None
    if entry.get('job_reference_verified') and not entry.get('native_publication_quarantined') and not entry.get('shutdown_interrupted'):native=_cancel_owned(entry,'Stop only owned later leaf phases: '+str(error))
    terminal=native is not None and native['data']['native_job_terminal']
    state.fail(entry['result'],error);entry['result']['data'].update(workflow_recovery_required=True,operation_complete=False,automatic_retry=False,adapter_failure_requires_inspection=True)
    no_dispatch=not entry.get('native_start_call_entered') and not entry.get('synchronous_child_entered') and not entry['result']['data'].get('actual_mutation_attempted')
    recovery=not no_dispatch and (not terminal or entry['result']['data'].get('mutation_state_unknown',False))
    if recovery:entry['result']['data'].update(mutation_state_unknown=True,mutation_performed=None)
    try:
        _checkpoint(entry)
        if no_dispatch or terminal:_seal(entry,recovery)
    except Exception as failure:
        if entry.get('finalization_attempted'):_delivery_failed(entry,failure);return
        entry['checkpoint_error']=str(failure)[:700];recovery=True
    _PENDING.pop(entry['run_id'],None)
    if recovery:_QUARANTINED[entry['run_id']]=entry
    if entry.get('shutdown_interrupted'):entry['delivery_deferred_during_shutdown']=True;return
    if entry.get('cu_delivery_attempted'):entry['late_observation_retained_metadata_only']=True;return
    try:
        if entry.get('journal_finalized'):_deliver(entry)
        else:entry['cu_delivery_attempted']=True;entry['cu_error_attempted']=True;entry['result_object'].set_error('Recipe terminal unverified; started parent journal/lease retained; no replay; operation_id='+entry['run_id'])
    except Exception as failure:_delivery_failed(entry,failure)

def _tick_all(delta):
    if not _ACTIVE:return False
    for entry in tuple(_PENDING.values()):
        if entry.get('finalization_attempted') or entry.get('executing'):continue
        entry['executing']=True
        try:
            if entry.get('job_reference_verified'):
                entry['snapshot_calls']+=1;native=api.snapshot(entry['job'],entry['leaf'][0])
                if native['data']['native_job_terminal']:_job_terminal(entry,native)
                else:_adopt_job(entry,native)
            elif entry['leaf'] is not None:_run_step(entry)
            else:_finish_entry(entry)
        except Exception as error:_uncertain(entry,error)
        finally:entry['executing']=False
    return True

def _shutdown():
    global _ACTIVE
    _ACTIVE=False
    for entry in tuple(_PENDING.values()):
        if entry.get('finalization_attempted'):continue
        entry['lifecycle_interrupted']=True;entry['shutdown_interrupted']=True
        state.fail(entry['result'],OperationError('niagara_scratch_user_recipe_shutdown_unknown','Shutdown before safe final receipt; no late UObject work'))
        entry['result']['data'].update(workflow_recovery_required=True,mutation_state_unknown=True,mutation_performed=None)
        if not entry.get('executing'):
            try:_checkpoint(entry)
            except Exception:pass
        _QUARANTINED[entry['run_id']]=entry;_PENDING.pop(entry['run_id'],None)

def binding_status():
    status=api.binding_status();leaf_status={name:callable(getattr(getattr(unreal,library,None),name,None)) for name,library in _LEAF_LIBRARIES.items()};native_ready=all(value is True for value in status.values()) and all(leaf_status.values())
    recipe_status={name:native_ready for name in METHODS}
    recipe_status['create_niagara_visible_scratch_pad_snow_seed']=native_ready and api.sprite_binding_ready()
    return dict(recipe_status,active=_ACTIVE,pending_count=len(_PENDING),quarantined_count=len(_QUARANTINED),ticker_handle_present=_TICKER_HANDLE is not None,shutdown_handle_present=_SHUTDOWN_HANDLE is not None,Native_bindings=status,leaf_bindings=leaf_status,last_terminal=copy.deepcopy(_LAST_TERMINAL),lifecycle_errors=list(_LIFECYCLE_ERRORS))

def activate():
    global _ACTIVE,_TICKER_HANDLE,_SHUTDOWN_HANDLE
    if not all(value is True for value in api.binding_status().values()):raise OperationError('niagara_scratch_user_recipe_native_unavailable','Exact compile-status/removal/scalar-binding/Array-binding/collection-remap/renderer-cleanup Native Job starts are required')
    if not _ACTIVE:
        if _PENDING or _QUARANTINED or _TICKER_HANDLE is not None or _SHUTDOWN_HANDLE is not None:raise OperationError('niagara_scratch_user_recipe_ownership_retained','Prior workflow ownership must be reconciled')
        _SHUTDOWN_HANDLE=unreal.register_python_shutdown_callback(_shutdown)
        try:_TICKER_HANDLE=unreal.register_ticker_callback(_tick_all,delay=0.0)
        except Exception:unreal.unregister_python_shutdown_callback(_SHUTDOWN_HANDLE);_SHUTDOWN_HANDLE=None;raise
        _ACTIVE=True
    return binding_status()

def deactivate(reason='Scratch/User recipe Toolset deactivated'):
    global _ACTIVE,_TICKER_HANDLE,_SHUTDOWN_HANDLE
    _ACTIVE=False
    for name,handle,remover in (('ticker',_TICKER_HANDLE,unreal.unregister_ticker_callback),('shutdown',_SHUTDOWN_HANDLE,unreal.unregister_python_shutdown_callback)):
        if handle is not None:
            try:
                remover(handle)
                if name=='ticker':_TICKER_HANDLE=None
                else:_SHUTDOWN_HANDLE=None
            except Exception as error:_LIFECYCLE_ERRORS.append(dict(handle=name,error=str(error)[:700]))
    for entry in tuple(_PENDING.values()):
        if entry.get('finalization_attempted'):continue
        entry['lifecycle_interrupted']=True
        # Never seal or mutate a result while its synchronous SDK/start call is
        # still on the stack. The returned prefix is adopted by that caller.
        if entry.get('executing'):continue
        _uncertain(entry,OperationError('niagara_scratch_user_recipe_deactivated',reason))
    return binding_status()

def completed_result(record):
    value=CubelessPythonAsyncOperationResult();value.set_editor_property('value',reflected_result(record));value.complete_value();return value

def _named_module_input_binding(method,arguments):
    """GL removal/binding3, NPC script remap1 and Renderer cleanup1 share the existing HN journal, ticker, lease and final delivery."""
    binding=_single_path_domain(method)
    result_object=CubelessPythonAsyncOperationResult();entry=None
    try:
        if method not in _MODULE_INPUT_BINDINGS:raise OperationError('niagara_module_input_paths_method_invalid','Only existing GL paths3, NPC script remap1 and Renderer cleanup1 use this concrete entry')
        args,original,legacy=binding._arguments(method,arguments,None,True)
        if method in ('remap_niagara_script_parameter_collection_references','cleanup_niagara_renderers'):args['concrete_native_job']=True
        api._carrier(args['params'])
        status=api.binding_status()
        if not status.get(method) or not all(status.get(key) is True for key in ('job_class_available','job_snapshot_callable','job_cancel_callable')):raise OperationError('niagara_module_input_paths_native_unavailable','Exact existing HN removal/binding Job is required; no start dispatched')
        if not _ACTIVE:raise OperationError('niagara_module_input_paths_inactive','Shared HN lifecycle is inactive')
        if _QUARANTINED:raise OperationError('niagara_module_input_paths_quarantined','Prior shared HN ownership requires reconciliation')
        run_id=uuid.uuid4().hex;journal=_RecipeJournal(method,dict(args=args,original=original),run_id)
        result=journal.result;result['data'].update(operation=method,parent_journal_only=True,shared_HN_lifecycle=True,native_dispatched=False,actual_mutation_attempted=False,actual_changed_observed=False,change_state_known=True,mutation_state_unknown=False,mutation_performed=False)
        entry=dict(method=method,run_id=run_id,journal=journal,result=result,result_object=result_object,callback=lambda ignored:journal.checkpoint(),module_input_binding=True,binding_args=args,binding_original=original,leaf=(method,args['params']),leaf_index=0,job=None,executing=False,job_reference_verified=False,snapshot_calls=0)
        _PENDING[run_id]=entry;_checkpoint(entry)
    except Exception as error:
        if entry is not None:_uncertain(entry,error)
        else:
            record=binding._rejected(method,error);record['data']['legacy_Named_result']=copy.deepcopy(getattr(error,'legacy_result',None if hasattr(error,'public_validation_errors') else binding._legacy_exception(method,error)))
            result_object.set_editor_property('value',reflected_result(record));result_object.complete_value()
    return result_object

def _named_mcp(method,arguments):
    result_object=CubelessPythonAsyncOperationResult();entry=None
    try:
        values=_values(method,arguments,True);_pure_admission(method,values)
        if not binding_status().get(method):raise OperationError('niagara_scratch_user_recipe_leaf_unavailable','Exact existing leaf bindings are required; no first writer dispatched')
        if not _ACTIVE:raise OperationError('niagara_scratch_user_recipe_inactive','Root has not activated the exact recipe lifecycle')
        if _QUARANTINED:raise OperationError('niagara_scratch_user_recipe_quarantined','Prior unknown recipe requires reconciliation')
        run_id=uuid.uuid4().hex;journal=_RecipeJournal(method,values,run_id)
        entry=_initial_entry(method,values,journal.result,journal.checkpoint);entry.update(run_id=run_id,journal=journal,result_object=result_object)
        entry['callback']=lambda ignored:journal.checkpoint();_context(entry['result'],method,values,'Named_official_MCP_only')
        _PENDING[run_id]=entry
        if not _advance(entry,first=True):_finish_entry(entry)
        else:_checkpoint(entry)
    except Exception as error:
        if entry is not None:_uncertain(entry,error)
        else:
            record=_rejected(method,error);result_object.set_editor_property('value',reflected_result(record));result_object.complete_value()
    return result_object

def create_niagara_scratch_pad_vec2_user_parameter_module(target_system_path,scratch_pad_name="MCP_BlankUpdateScratch",parameter_name="Particles.SpriteSize",parameter_value=None,user_parameter_name="User.MCP_BlankScratchPad_SpriteSize",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    return _direct("create_niagara_scratch_pad_vec2_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds})

def create_niagara_scratch_pad_vec2_user_parameter_module_action(result,target_system_path,scratch_pad_name="MCP_BlankUpdateScratch",parameter_name="Particles.SpriteSize",parameter_value=None,user_parameter_name="User.MCP_BlankScratchPad_SpriteSize",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _direct_action(result,"create_niagara_scratch_pad_vec2_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds},checkpoint_callback,_legacy_command_params,_public_input)

def create_niagara_scratch_pad_color_user_parameter_module(target_system_path,scratch_pad_name="MCP_ColorScratch",parameter_name="Particles.Color",parameter_value=None,user_parameter_name="User.MCP_ScratchPad_Color",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    return _direct("create_niagara_scratch_pad_color_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds})

def create_niagara_scratch_pad_color_user_parameter_module_action(result,target_system_path,scratch_pad_name="MCP_ColorScratch",parameter_name="Particles.Color",parameter_value=None,user_parameter_name="User.MCP_ScratchPad_Color",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _direct_action(result,"create_niagara_scratch_pad_color_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds},checkpoint_callback,_legacy_command_params,_public_input)

def create_niagara_scratch_pad_bool_user_parameter_module(target_system_path,scratch_pad_name="MCP_BoolUserScratch",parameter_name="Particles.MCPBoolProbe",parameter_value=None,module_input_name="MCPBoolInput",module_input_default_value=None,user_parameter_name="User.MCP_ScratchPad_Bool",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",overwrite_existing=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    return _direct("create_niagara_scratch_pad_bool_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"module_input_name":module_input_name,"module_input_default_value":module_input_default_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"overwrite_existing":overwrite_existing,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds})

def create_niagara_scratch_pad_bool_user_parameter_module_action(result,target_system_path,scratch_pad_name="MCP_BoolUserScratch",parameter_name="Particles.MCPBoolProbe",parameter_value=None,module_input_name="MCPBoolInput",module_input_default_value=None,user_parameter_name="User.MCP_ScratchPad_Bool",user_parameter_default_value=None,target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",overwrite_existing=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _direct_action(result,"create_niagara_scratch_pad_bool_user_parameter_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"module_input_name":module_input_name,"module_input_default_value":module_input_default_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"overwrite_existing":overwrite_existing,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds},checkpoint_callback,_legacy_command_params,_public_input)

def create_niagara_scratch_pad_bool_visible_toggle_module(target_system_path,scratch_pad_name="MCP_BoolVisibleScratch",parameter_name="Particles.SpriteSize",parameter_value=None,base_module_input_name="MCPBaseSpriteSize",base_module_input_default_value=None,user_parameter_name="User.MCP_VisibleToggle",user_parameter_default_value=None,custom_hlsl_input_name="MCPVisibleToggle",custom_hlsl_expression="BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)",custom_hlsl_output_pin_name="MCPVisibleSize",custom_hlsl_signature_name="MCP_ScratchPadBoolVisibleSize",target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",overwrite_existing=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    return _direct("create_niagara_scratch_pad_bool_visible_toggle_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"base_module_input_name":base_module_input_name,"base_module_input_default_value":base_module_input_default_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"custom_hlsl_input_name":custom_hlsl_input_name,"custom_hlsl_expression":custom_hlsl_expression,"custom_hlsl_output_pin_name":custom_hlsl_output_pin_name,"custom_hlsl_signature_name":custom_hlsl_signature_name,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"overwrite_existing":overwrite_existing,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds})

def create_niagara_scratch_pad_bool_visible_toggle_module_action(result,target_system_path,scratch_pad_name="MCP_BoolVisibleScratch",parameter_name="Particles.SpriteSize",parameter_value=None,base_module_input_name="MCPBaseSpriteSize",base_module_input_default_value=None,user_parameter_name="User.MCP_VisibleToggle",user_parameter_default_value=None,custom_hlsl_input_name="MCPVisibleToggle",custom_hlsl_expression="BaseValue * (MCPVisibleToggle ? 1.0 : 0.0)",custom_hlsl_output_pin_name="MCPVisibleSize",custom_hlsl_signature_name="MCP_ScratchPadBoolVisibleSize",target_usage="ParticleUpdateScript",target_owner_kind="system",target_emitter_index=None,target_emitter_name="",target_stack_emitter_index=0,target_stack_emitter_name="",stack_suggested_name="",overwrite_existing=False,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _direct_action(result,"create_niagara_scratch_pad_bool_visible_toggle_module",{"target_system_path":target_system_path,"scratch_pad_name":scratch_pad_name,"parameter_name":parameter_name,"parameter_value":parameter_value,"base_module_input_name":base_module_input_name,"base_module_input_default_value":base_module_input_default_value,"user_parameter_name":user_parameter_name,"user_parameter_default_value":user_parameter_default_value,"custom_hlsl_input_name":custom_hlsl_input_name,"custom_hlsl_expression":custom_hlsl_expression,"custom_hlsl_output_pin_name":custom_hlsl_output_pin_name,"custom_hlsl_signature_name":custom_hlsl_signature_name,"target_usage":target_usage,"target_owner_kind":target_owner_kind,"target_emitter_index":target_emitter_index,"target_emitter_name":target_emitter_name,"target_stack_emitter_index":target_stack_emitter_index,"target_stack_emitter_name":target_stack_emitter_name,"stack_suggested_name":stack_suggested_name,"overwrite_existing":overwrite_existing,"allow_source_edit":allow_source_edit,"save":save,"request_compile":request_compile,"wait_for_completion":wait_for_completion,"timeout_seconds":timeout_seconds},checkpoint_callback,_legacy_command_params,_public_input)


def create_niagara_visible_scratch_pad_snow_seed(target_system_path,preset='snow',emitter_name='Emitter',material_path='',use_project_weather_materials=True,scratch_pad_name='MCP_VisibleSizeScratch',parameter_name='Particles.SpriteSize',parameter_value=None,user_parameter_name='User.MCP_VisibleScratchPad_SpriteSize',user_parameter_default_value=None,target_usage='ParticleUpdateScript',spawn_rate=180.0,velocity=None,sprite_size=None,lifetime=10.0,add_spawn_box_location=True,spawn_box_size=None,spawn_box_offset=None,add_weather_detail_modules=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180):
    return _direct('create_niagara_visible_scratch_pad_snow_seed',dict(target_system_path=target_system_path,preset=preset,emitter_name=emitter_name,material_path=material_path,use_project_weather_materials=use_project_weather_materials,scratch_pad_name=scratch_pad_name,parameter_name=parameter_name,parameter_value=parameter_value,user_parameter_name=user_parameter_name,user_parameter_default_value=user_parameter_default_value,target_usage=target_usage,spawn_rate=spawn_rate,velocity=velocity,sprite_size=sprite_size,lifetime=lifetime,add_spawn_box_location=add_spawn_box_location,spawn_box_size=spawn_box_size,spawn_box_offset=spawn_box_offset,add_weather_detail_modules=add_weather_detail_modules,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds))

def create_niagara_visible_scratch_pad_snow_seed_action(result,target_system_path,preset='snow',emitter_name='Emitter',material_path='',use_project_weather_materials=True,scratch_pad_name='MCP_VisibleSizeScratch',parameter_name='Particles.SpriteSize',parameter_value=None,user_parameter_name='User.MCP_VisibleScratchPad_SpriteSize',user_parameter_default_value=None,target_usage='ParticleUpdateScript',spawn_rate=180.0,velocity=None,sprite_size=None,lifetime=10.0,add_spawn_box_location=True,spawn_box_size=None,spawn_box_offset=None,add_weather_detail_modules=True,allow_source_edit=False,save=True,request_compile=True,wait_for_completion=True,timeout_seconds=180,checkpoint_callback=None,*,_legacy_command_params=None,_public_input=False):
    return _direct_action(result,'create_niagara_visible_scratch_pad_snow_seed',dict(target_system_path=target_system_path,preset=preset,emitter_name=emitter_name,material_path=material_path,use_project_weather_materials=use_project_weather_materials,scratch_pad_name=scratch_pad_name,parameter_name=parameter_name,parameter_value=parameter_value,user_parameter_name=user_parameter_name,user_parameter_default_value=user_parameter_default_value,target_usage=target_usage,spawn_rate=spawn_rate,velocity=velocity,sprite_size=sprite_size,lifetime=lifetime,add_spawn_box_location=add_spawn_box_location,spawn_box_size=spawn_box_size,spawn_box_offset=spawn_box_offset,add_weather_detail_modules=add_weather_detail_modules,allow_source_edit=allow_source_edit,save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,timeout_seconds=timeout_seconds),checkpoint_callback,_legacy_command_params,_public_input)
