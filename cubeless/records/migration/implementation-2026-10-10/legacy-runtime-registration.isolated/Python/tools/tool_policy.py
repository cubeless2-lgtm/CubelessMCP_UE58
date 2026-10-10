"""Shared safety metadata for the Unreal MCP tool catalog.

The policy layer is intentionally independent from individual tool modules.
It describes risk and verification expectations, drives the additive mutation
result contract, and lets search-mode dispatchers require explicit confirmation
before a mutating high-risk call.
"""

from __future__ import annotations

from dataclasses import dataclass, replace
from inspect import Parameter, Signature
from typing import Any, Mapping


TOOL_EFFECTS = frozenset({"read", "write", "destructive", "lifecycle"})
TOOL_RISK_LEVELS = frozenset({"low", "medium", "high", "critical"})
TOOL_VERIFICATION_LEVELS = frozenset({"none", "legacy", "independent_readback"})
TOOL_CONFIRMATION_MODES = frozenset(
    {"none", "dispatcher_confirm", "dispatcher_confirm_if_replace_existing"}
)


@dataclass(frozen=True)
class ToolPolicy:
    """Stable policy metadata attached to every registered domain tool."""

    effect: str
    risk: str
    supports_dry_run: bool
    dry_run_default: bool | None
    transactional: bool
    postcondition: str
    confirmation_required: bool
    confirmation_mode: str
    result_contract: str
    verification_level: str
    strict_postcondition: bool
    rationale: str

    def __post_init__(self) -> None:
        if self.effect not in TOOL_EFFECTS:
            raise ValueError(f"Unsupported tool effect: {self.effect}")
        if self.risk not in TOOL_RISK_LEVELS:
            raise ValueError(f"Unsupported tool risk: {self.risk}")
        if self.supports_dry_run is False and self.dry_run_default is not None:
            raise ValueError("dry_run_default requires supports_dry_run")
        if (
            self.effect in {"destructive", "lifecycle"}
            or self.risk in {"high", "critical"}
        ) and not self.confirmation_required:
            raise ValueError("high-impact tools must require confirmation")
        if self.confirmation_required and self.confirmation_mode == "none":
            raise ValueError("confirmation_required tools need a confirmation mode")
        if self.confirmation_mode not in TOOL_CONFIRMATION_MODES:
            raise ValueError(f"Unsupported confirmation mode: {self.confirmation_mode}")
        if self.verification_level not in TOOL_VERIFICATION_LEVELS:
            raise ValueError(f"Unsupported tool verification level: {self.verification_level}")
        if self.effect == "read" and self.result_contract != "none":
            raise ValueError("read-only tools must not declare a mutation result contract")
        if self.effect != "read" and self.result_contract != "mutation_result_v1":
            raise ValueError("mutation-capable tools must declare mutation_result_v1")
        if self.strict_postcondition and self.verification_level != "independent_readback":
            raise ValueError("strict postconditions require independent readback")


_READ_PREFIXES = (
    "analyze_",
    "audit_",
    "check_",
    "critique_",
    "describe_",
    "diagnose_",
    "dry_run_",
    "find_",
    "get_",
    "inspect_",
    "list_",
    "make_",
    "plan_",
    "profile_",
    "search_",
    "select_",
    "validate_",
)
_DESTRUCTIVE_PREFIXES = (
    "cleanup_",
    "clear_",
    "delete_",
    "recreate_",
    "remap_",
    "remove_",
    "replace_",
)

# A read-oriented name is not enough to claim a read-only effect when the
# public contract can request compilation, saving, or a filesystem report.
# Keep this list narrow and capability-based so future wrappers do not silently
# inherit a low-risk policy from naming alone.
_MUTATION_CAPABILITY_PARAMETERS = frozenset(
    {
        "output_path",
        "request_compile",
        "save",
        "save_asset",
        "save_assets",
        "save_level",
        "write_manifest",
        "write_report",
    }
)


_EXPLICIT_OVERRIDES: Mapping[str, Mapping[str, Any]] = {
    "manage_tools": {
        "effect": "write",
        "risk": "medium",
        "confirmation_required": False,
        "confirmation_mode": "none",
        "rationale": "Changes only in-memory MCP enablement policy and is protected from disablement.",
    },
    "execute_batch": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "May compose up to 100 mutating bridge commands in one editor transaction.",
    },
    "configure_chaos_cloth_component": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can change live or editor-world Chaos Cloth component configuration and rebuild its simulation proxy.",
    },
    "configure_chaos_cloth_dataflow_node": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can mutate a Chaos Cloth Dataflow graph, regenerate its cloth asset, and optionally save both assets.",
    },
    "configure_chaos_cloth_weight_map": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can mutate a Chaos Cloth Dataflow weight map, regenerate its cloth asset, and optionally save both assets.",
    },
    "create_legacy_chaos_clothing_from_skeletal_sections": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Duplicates a Skeletal Mesh and binds engine Chaos Clothing assets with painted MaxDistance maps to selected sections.",
    },
    "create_skeletal_bone_constraint_fixture": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Creates a non-persisted editor-world SkeletalMeshActor fixture with a native PhysicsConstraintComponent and verifies the constrained bodies by readback.",
    },
    "create_skeletal_physics_asset": {
        "effect": "write",
        "risk": "high",
        "supports_dry_run": True,
        "dry_run_default": True,
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "postcondition": "independent_readback",
        "verification_level": "independent_readback",
        "strict_postcondition": True,
        "rationale": "Creates or explicitly rebuilds a persistent PhysicsAsset and verifies generated bodies and constraints before saving.",
    },
    "ensure_anim_graph_secondary_motion_chain": {
        "effect": "write",
        "risk": "high",
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "postcondition": "independent_readback",
        "verification_level": "independent_readback",
        "strict_postcondition": True,
        "rationale": "Mutates, compiles, and can save an Animation Blueprint graph containing AnimDynamics or optional KawaiiPhysics nodes.",
    },
    "ensure_blueprint_rigid_constraint_chain": {
        "effect": "write",
        "risk": "high",
        "supports_dry_run": True,
        "dry_run_default": True,
        "transactional": True,
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "postcondition": "independent_readback",
        "verification_level": "independent_readback",
        "strict_postcondition": True,
        "rationale": "Creates persistent StaticMesh body and PhysicsConstraint component templates, compiles the Blueprint, and can save it.",
    },
    "configure_chaos_solver_settings": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can change Chaos Solver behavior in the editor world or an active play world; editor-world changes can optionally save the level.",
    },
    "compile_controlrig_profile_recipe": {
        "effect": "read",
        "risk": "low",
        "confirmation_required": False,
        "confirmation_mode": "none",
        "rationale": "Compiles deterministic planning evidence and an in-memory recipe without building, saving, or mutating assets.",
    },
    "run_ai_dummy_patrol_chase_smoke": {
        "effect": "lifecycle",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Starts and stops SIE while creating and destroying shared editor-world AI fixtures.",
    },
    "run_ai_behavior_tree_execution_smoke": {
        "effect": "lifecycle",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Starts and stops SIE while creating and destroying shared editor-world behavior-tree fixtures.",
    },
    "create_ai_patrol_chase_behavior_assets": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm_if_replace_existing",
        "rationale": "Creates standard AI assets, but replace_existing=true deletes and recreates an existing Behavior Tree and Blackboard pair.",
    },
    "execute_unreal_python": {
        "effect": "destructive",
        "risk": "critical",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Executes arbitrary Unreal Python and can mutate assets or editor state.",
    },
    "execute_blueprint_niagara_component_template_asset_swap_canary": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can replace and mutate a temporary Blueprint canary when dry_run is false.",
    },
    "execute_blueprint_niagara_component_template_asset_swap_durable": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can compile and save a durable Blueprint component-template mutation.",
    },
    "execute_niagara_promotion_manifest": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can promote and save multiple Niagara Systems to durable project paths.",
    },
    "open_editor_level": {
        "effect": "lifecycle",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Switches editor world state and may be blocked by dirty packages.",
    },
    "safe_new_preview_map": {
        "effect": "lifecycle",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Creates or switches editor world state under guarded preview-map policy.",
    },
    "postprocess_content_folder_mcp": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can remap, compile, and save many assets in an existing content folder.",
    },
    "promote_niagara_system_candidate": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can duplicate and save a Niagara System to a durable project path.",
    },
    "promote_pcg_temp_graph": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can duplicate and save a PCG graph to a new durable project path; all live existing-target overwrite paths are intentionally unsupported.",
    },
    "recompile_niagara_scripts_in_root": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can force compile and save many Niagara scripts under a root path.",
    },
    "repair_world_actor_instances_mcp": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can remap actor/component references and save an existing map.",
    },
    "resave_pcg_assets": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can resave every matching PCG asset under a broad content root.",
    },
    "run_content_validation_pipeline_mcp": {
        "effect": "destructive",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can delete a target root, overwrite assets, repair maps, and save the result.",
    },
    "sample_anim_node_pre_post_runtime_pose": {
        "effect": "destructive",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Can duplicate, overwrite, and clean up temporary AnimBlueprint assets when dry_run is false.",
    },
    "sample_render_target2d_region_stats": {
        "effect": "read",
        "risk": "low",
        "confirmation_required": False,
        "confirmation_mode": "none",
        "rationale": "Reads render-target pixels without changing editor, runtime, or asset state.",
    },
    "validate_chaos_cache": {
        "effect": "write",
        "risk": "high",
        "confirmation_required": True,
        "confirmation_mode": "dispatcher_confirm",
        "rationale": "Changes Chaos Cache playback and component physics state while validating repeated scrubbing.",
    },
}

_STRICT_INDEPENDENT_READBACK_TOOLS = frozenset(
    {
        "add_level_sequence_actor_binding",
        "add_level_sequence_camera_cut",
        "add_or_update_datatable_row",
        "connect_blueprint_graph_pins",
        "configure_chaos_cloth_component",
        "configure_chaos_cloth_dataflow_node",
        "configure_chaos_cloth_weight_map",
        "configure_chaos_solver_settings",
        "create_legacy_chaos_clothing_from_skeletal_sections",
        "create_skeletal_bone_constraint_fixture",
        "create_data_asset",
        "create_datatable",
        "create_level_sequence",
        "delete_actor",
        "delete_blueprint_node",
        "remove_datatable_row",
        "remove_level_sequence_binding",
        "set_actor_transform",
        "set_blueprint_node_positions",
        "set_blueprint_networking_settings",
        "set_data_asset_properties",
        "set_level_sequence_playback_range",
        "spawn_actor",
    }
)

_UE56_NATIVE_TRANSACTIONAL_READBACK_TOOLS = frozenset(
    {
        "add_ability_system_component_to_blueprint",
        "add_ai_blackboard_key",
        "add_gameplay_attribute_to_attribute_set",
        "add_state_tree_state",
        "compile_state_tree",
        "configure_ai_controller_perception",
        "create_ai_behavior_tree_asset",
        "create_ai_blackboard_asset",
        "create_ai_eqs_query",
        "create_ai_patrol_chase_behavior_assets",
        "create_attribute_set_blueprint",
        "create_basic_metasound_source",
        "create_gameplay_ability_blueprint",
        "create_gameplay_effect_blueprint",
        "create_sound_cue_from_wave",
        "create_state_tree",
        "remove_ai_blackboard_key",
        "set_ai_behavior_tree_blackboard",
    }
)


def _dry_run_default(signature: Signature) -> tuple[bool, bool | None]:
    parameter = signature.parameters.get("dry_run")
    if parameter is None:
        return False, None
    if parameter.default is Parameter.empty:
        return True, None
    return True, parameter.default if isinstance(parameter.default, bool) else None


def infer_tool_policy(
    name: str,
    category: str,
    signature: Signature,
    description: str = "",
) -> ToolPolicy:
    """Infer conservative policy metadata from one public tool contract."""

    normalized_name = name.strip().lower()
    supports_dry_run, dry_run_default = _dry_run_default(signature)
    parameter_names = set(signature.parameters)
    mutation_capabilities = parameter_names & _MUTATION_CAPABILITY_PARAMETERS

    if normalized_name.startswith(_DESTRUCTIVE_PREFIXES):
        effect = "destructive"
    elif normalized_name.startswith(_READ_PREFIXES) and not mutation_capabilities:
        effect = "read"
    else:
        effect = "write"

    risk = {
        "read": "low",
        "write": "medium",
        "destructive": "high",
        "lifecycle": "high",
    }[effect]
    confirmation_required = effect in {"destructive", "lifecycle"}
    transactional = "transaction" in description.casefold() or normalized_name == "execute_batch"

    if {"request_compile", "wait_for_completion"} & parameter_names:
        postcondition = "compile_and_structured_readback"
    elif {"save", "save_asset", "save_assets", "save_level"} & parameter_names:
        postcondition = "structured_result_and_save_status"
    else:
        postcondition = "structured_result"

    policy = ToolPolicy(
        effect=effect,
        risk=risk,
        supports_dry_run=supports_dry_run,
        dry_run_default=dry_run_default,
        transactional=transactional,
        postcondition=postcondition,
        confirmation_required=confirmation_required,
        confirmation_mode="dispatcher_confirm" if confirmation_required else "none",
        result_contract="none" if effect == "read" else "mutation_result_v1",
        verification_level="none" if effect == "read" else "legacy",
        strict_postcondition=False,
        rationale=f"Conservative policy inferred from {category}:{name} public contract.",
    )

    override = _EXPLICIT_OVERRIDES.get(normalized_name)
    if override:
        resolved_override = dict(override)
        resolved_effect = str(resolved_override.get("effect", policy.effect))
        if resolved_effect == "read":
            resolved_override.setdefault("result_contract", "none")
            resolved_override.setdefault("verification_level", "none")
            resolved_override.setdefault("strict_postcondition", False)
        elif policy.effect == "read":
            resolved_override.setdefault("result_contract", "mutation_result_v1")
            resolved_override.setdefault("verification_level", "legacy")
            resolved_override.setdefault("strict_postcondition", False)
        policy = replace(policy, **resolved_override)

    if normalized_name in _STRICT_INDEPENDENT_READBACK_TOOLS:
        policy = replace(
            policy,
            postcondition="independent_readback",
            verification_level="independent_readback",
            strict_postcondition=True,
        )
    elif normalized_name in _UE56_NATIVE_TRANSACTIONAL_READBACK_TOOLS:
        policy = replace(
            policy,
            transactional=True,
            postcondition="independent_readback",
            verification_level="independent_readback",
        )
    return policy


def requires_dispatch_confirmation(
    policy: ToolPolicy,
    arguments: Mapping[str, Any] | None,
    *,
    confirmed: bool,
) -> bool:
    """Return whether search-mode dispatch must block this invocation."""

    if confirmed or not policy.confirmation_required:
        return False
    if (
        policy.confirmation_mode == "dispatcher_confirm_if_replace_existing"
        and (arguments or {}).get("replace_existing", False) is False
    ):
        return False
    if policy.supports_dry_run:
        requested_dry_run = (arguments or {}).get("dry_run", policy.dry_run_default)
        if requested_dry_run is True:
            return False
    return True
