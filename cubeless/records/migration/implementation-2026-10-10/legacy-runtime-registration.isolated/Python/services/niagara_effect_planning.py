"""Grounded, mutation-free planning contracts for Niagara effect authoring.

This module deliberately contains no model calls and no Unreal bridge calls.
It turns an agent-authored intent/spec/patch into deterministic, reviewable
contracts that can be consumed by the existing Niagara MCP tool surface.
"""

from __future__ import annotations

import copy
import json
import re
from collections.abc import Iterable
from functools import lru_cache
from pathlib import Path
from typing import Any

from tools.response_schema import tool_error, tool_success

EFFECT_SPEC_SCHEMA_VERSION = 1
EFFECT_PATCH_SCHEMA_VERSION = 1
MAX_EMITTERS = 16
MAX_OBJECTIVES = 24
MAX_PATCH_OPERATIONS = 100
TEMP_NIAGARA_ROOT = "/Game/_MCP_Temp/NiagaraGenerated/"

EMITTER_ID_PATTERN = re.compile(r"^[a-z][a-z0-9_]{0,63}$")
TOKEN_PATTERN = re.compile(r"[0-9a-zA-Z가-힣_]+")

ALLOWED_RENDERERS = {
    "Auto",
    "None",
    "Sprite",
    "Ribbon",
    "Mesh",
    "Light",
    "Component",
}
ALLOWED_SIM_TARGETS = {"Auto", "CPU", "GPU"}
ALLOWED_STAGES = {
    "SystemSpawn",
    "SystemUpdate",
    "EmitterSpawn",
    "EmitterUpdate",
    "ParticleSpawn",
    "ParticleUpdate",
    "EventHandler",
    "SimulationStage",
    "Renderer",
}
ALLOWED_MUTATION_POLICIES = {"temp_only", "plan_only"}
ALLOWED_ROLLBACK_STRATEGIES = {
    "restore_baseline",
    "inverse_operations",
    "discard_candidate",
}

FORBIDDEN_ASSET_REFERENCE_TOKENS = (
    "/Script/UnrealMCP",
    "/UnrealMCP",
    "MCPUnreal",
    "mcp_unreal",
)

GROUNDING_REFS = [
    "Docs/Tools/niagara_tools.md",
    "Docs/Tools/batch_tools.md",
    "codex/docs/projects/cubeless-stylized/tooling/niagara-mcp-authoring-extension.md",
    "codex/docs/projects/cubeless-stylized/systems/mcpweather.md",
    "Docs/Analysis/MCPWeatherNiagara",
]

READ_ONLY_PREFLIGHT_TOOLS = [
    "analyze_niagara_system",
    "inspect_niagara_stack",
    "inspect_niagara_module_inputs",
    "inspect_niagara_emitter_properties",
    "inspect_niagara_compile_status",
]

POSTFLIGHT_TOOLS = [
    "inspect_niagara_compile_status",
    "analyze_niagara_system",
    "audit_niagara_promotion_candidate",
]

# These are existing bridge commands. P0 validates and emits a manifest only;
# it never dispatches these commands by itself.
ALLOWED_PATCH_COMMANDS: dict[str, dict[str, Any]] = {
    "set_niagara_module_input_value": {
        "required": ("system_path", "input_name", "value"),
    },
    "create_niagara_module_input_override": {
        "required": ("system_path", "input_name", "value"),
    },
    "set_niagara_module_input_literal_override": {
        "required": ("system_path", "input_name", "value"),
    },
    "set_niagara_module_static_switch": {
        "required": ("system_path", "input_name", "value"),
    },
    "set_niagara_rapid_iteration_parameter": {
        "required": ("system_path", "parameter_name", "value"),
    },
    "set_niagara_module_input_user_parameter": {
        "required": ("system_path", "input_name", "user_parameter_name"),
    },
    "set_niagara_module_input_linked_parameter": {
        "required": ("system_path", "input_name", "linked_parameter_name"),
    },
    "set_niagara_module_inputs_batch": {
        "required": ("system_path", "edits"),
    },
    "set_niagara_renderer_material": {
        "required": ("system_path", "material_path"),
    },
    "set_niagara_sprite_renderer_settings": {
        "required": ("system_path",),
        "one_of": ("alignment", "facing_mode", "sub_image_size"),
    },
    "set_niagara_renderer_enabled": {
        "required": ("system_path", "enabled"),
    },
    "set_niagara_emitter_enabled": {
        "required": ("system_path", "enabled"),
    },
    "set_niagara_emitter_local_space": {
        "required": ("system_path", "value"),
    },
    "set_niagara_emitter_sim_target": {
        "required": ("system_path", "sim_target"),
    },
    "set_niagara_emitter_fixed_bounds": {
        "required": ("system_path",),
        "one_of": ("mode", "min", "max"),
    },
    "set_niagara_emitter_persistent_ids": {
        "required": ("system_path", "enabled"),
        "one_of": ("emitter_index", "emitter_name"),
        "supports_wait": True,
    },
    "add_niagara_event_handler": {
        "required": ("system_path", "source_event_name"),
        "one_of": ("emitter_index", "emitter_name"),
        "supports_wait": True,
    },
    "add_niagara_stock_module_to_stack": {
        "path_key": "target_system_path",
        "required": (
            "target_system_path",
            "module_script_path",
            "target_usage",
            "target_usage_id",
        ),
        "one_of": ("target_emitter_index", "target_emitter_name"),
    },
    "remove_niagara_modules": {
        "required": ("system_path",),
        "one_of": (
            "module_names",
            "module_name_prefixes",
            "module_name_contains",
            "module_name",
        ),
        "destructive_within_candidate": True,
    },
}


def _module(
    module: str,
    stage: str,
    responsibility: str,
    parameters: dict[str, Any] | None = None,
) -> dict[str, Any]:
    return {
        "module": module,
        "stage": stage,
        "responsibility": responsibility,
        "parameters": parameters or {},
    }


PLANNING_PROFILES: list[dict[str, Any]] = [
    {
        "profile_id": "rain",
        "terms": ("rain", "raindrop", "rainfall", "비", "빗방울", "강수", "폭우"),
        "emitters": [
            {
                "emitter_id": "rain",
                "display_name": "Rain",
                "responsibility": "Spawn and move visible rain particles through a bounded volume.",
                "renderer": "Sprite",
                "simulation_target": "GPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Rate", "EmitterUpdate", "Control rain density."),
                    _module("Box Location", "ParticleSpawn", "Distribute rain over a bounded volume."),
                    _module("Initialize Particle", "ParticleSpawn", "Set lifetime, color, and non-uniform sprite size."),
                    _module("Add Velocity", "ParticleSpawn", "Set downward rain motion."),
                    _module("Gravity Force", "ParticleUpdate", "Maintain downward acceleration."),
                    _module("Scale Color", "ParticleUpdate", "Fade particles over lifetime."),
                ],
                "material": {
                    "query": "project-owned thin translucent rain streak material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": [
            "Rain spawns continuously across the intended volume.",
            "Rain motion and sprite orientation read as falling streaks.",
        ],
        "knowledge_topics": ["camera_follow_world_space_lag", "rain_streak_orientation", "gpu_bounds_culling"],
        "suggested_seed_tool": "create_sprite_niagara_preset_system",
    },
    {
        "profile_id": "snow",
        "terms": ("snow", "snowflake", "snowfall", "눈", "눈송이", "강설"),
        "emitters": [
            {
                "emitter_id": "snow",
                "display_name": "Snow",
                "responsibility": "Spawn slow, drifting snow particles over a broad volume.",
                "renderer": "Sprite",
                "simulation_target": "GPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Rate", "EmitterUpdate", "Control snow density."),
                    _module("Box Location", "ParticleSpawn", "Distribute snow over a bounded volume."),
                    _module("Initialize Particle", "ParticleSpawn", "Set lifetime, random size, and color."),
                    _module("Add Velocity", "ParticleSpawn", "Set slow downward motion."),
                    _module("Curl Noise Force", "ParticleUpdate", "Add wind-driven flutter."),
                    _module("Drag", "ParticleUpdate", "Damp motion into a soft drift."),
                    _module("Scale Color", "ParticleUpdate", "Fade snow over lifetime."),
                ],
                "material": {
                    "query": "project-owned soft snowflake sprite material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": [
            "Snow falls with visible random drift rather than uniform rain-like motion.",
            "Snow remains visible without source-project runtime controllers.",
        ],
        "knowledge_topics": ["camera_follow_world_space_lag", "standalone_visibility_dependency", "gpu_bounds_culling"],
        "suggested_seed_tool": "create_sprite_niagara_preset_system",
    },
    {
        "profile_id": "projectile",
        "terms": ("projectile", "orb", "missile", "blob", "투사체", "구체", "오브", "탄환"),
        "emitters": [
            {
                "emitter_id": "projectile",
                "display_name": "Projectile",
                "responsibility": "Render the primary moving projectile body.",
                "renderer": "Sprite",
                "simulation_target": "CPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Burst Instantaneous", "EmitterSpawn", "Spawn the projectile body once."),
                    _module("Initialize Particle", "ParticleSpawn", "Set projectile lifetime, size, and color."),
                    _module("Add Velocity", "ParticleSpawn", "Define projectile trajectory."),
                    _module("Scale Color", "ParticleUpdate", "Control projectile fade."),
                ],
                "material": {
                    "query": "project-owned glowing projectile material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": ["The primary projectile has a clear trajectory and readable silhouette."],
        "knowledge_topics": ["event_location_overwritten"],
        "suggested_seed_tool": "create_basic_sprite_niagara_system",
    },
    {
        "profile_id": "trail",
        "terms": ("trail", "ribbon", "tail", "streak", "트레일", "리본", "꼬리", "잔상"),
        "emitters": [
            {
                "emitter_id": "trail",
                "display_name": "Trail",
                "responsibility": "Render a persistent trail following a source particle or event.",
                "renderer": "Ribbon",
                "simulation_target": "CPU",
                "depends_on": [],
                "modules": [
                    _module("Receive Location Event", "EventHandler", "Receive source positions for the trail."),
                    _module("Initialize Ribbon", "ParticleSpawn", "Initialize ribbon width and linkage."),
                    _module("Scale Ribbon Width", "ParticleUpdate", "Taper the trail over lifetime."),
                    _module("Scale Color", "ParticleUpdate", "Fade the trail over lifetime."),
                ],
                "material": {
                    "query": "project-owned additive or translucent ribbon trail material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": ["The trail follows the intended source and tapers without discontinuities."],
        "knowledge_topics": ["event_location_overwritten"],
        "suggested_seed_tool": "create_empty_niagara_system",
    },
    {
        "profile_id": "impact_ripple",
        "terms": ("collision", "impact", "hit", "ripple", "충돌", "타격", "피격", "파문", "물결"),
        "emitters": [
            {
                "emitter_id": "impact",
                "display_name": "Impact",
                "responsibility": "Render a short secondary response at an impact or collision location.",
                "renderer": "Sprite",
                "simulation_target": "CPU",
                "depends_on": [],
                "modules": [
                    _module("Receive Collision Event", "EventHandler", "Receive collision payload from the source emitter."),
                    _module("Initialize Particle", "ParticleSpawn", "Set short lifetime and initial size."),
                    _module("Scale Sprite Size", "ParticleUpdate", "Expand the impact or ripple over time."),
                    _module("Scale Color", "ParticleUpdate", "Fade the impact or ripple."),
                ],
                "material": {
                    "query": "project-owned impact or horizontal ripple material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": [
            "A secondary effect appears at the source collision location.",
            "The impact response expands and fades without persistent scale drift.",
        ],
        "knowledge_topics": ["collision_secondary_effect_missing", "event_location_overwritten", "iterative_parameter_drift"],
        "suggested_seed_tool": "create_empty_niagara_system",
    },
    {
        "profile_id": "burst",
        "terms": ("burst", "explosion", "firework", "explode", "폭발", "폭죽", "버스트", "터짐"),
        "emitters": [
            {
                "emitter_id": "burst",
                "display_name": "Burst",
                "responsibility": "Spawn a dense radial burst with controlled expansion and fade.",
                "renderer": "Sprite",
                "simulation_target": "GPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Burst Instantaneous", "EmitterSpawn", "Create the burst at a specific time."),
                    _module("Sphere Location", "ParticleSpawn", "Distribute particles around the burst center."),
                    _module("Initialize Particle", "ParticleSpawn", "Set lifetime, size, and color variation."),
                    _module("Add Velocity", "ParticleSpawn", "Set radial expansion."),
                    _module("Drag", "ParticleUpdate", "Slow the expansion over time."),
                    _module("Scale Color", "ParticleUpdate", "Fade the burst."),
                ],
                "material": {
                    "query": "project-owned glowing spark or burst material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": ["The burst expands radially, slows, and fades with a readable temporal arc."],
        "knowledge_topics": ["spawn_distribution_axis", "gpu_bounds_culling", "iterative_parameter_drift"],
        "suggested_seed_tool": "create_sprite_niagara_preset_system",
    },
    {
        "profile_id": "smoke_mist",
        "terms": ("smoke", "mist", "fog", "cloud", "연기", "안개", "미스트", "구름"),
        "emitters": [
            {
                "emitter_id": "mist",
                "display_name": "Mist",
                "responsibility": "Render a soft expanding volume of translucent particles.",
                "renderer": "Sprite",
                "simulation_target": "GPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Rate", "EmitterUpdate", "Control mist density."),
                    _module("Shape Location", "ParticleSpawn", "Define the emission volume."),
                    _module("Initialize Particle", "ParticleSpawn", "Set long lifetime, soft color, and size variation."),
                    _module("Curl Noise Force", "ParticleUpdate", "Create slow turbulent motion."),
                    _module("Drag", "ParticleUpdate", "Damp motion."),
                    _module("Scale Sprite Size", "ParticleUpdate", "Expand the mist over lifetime."),
                    _module("Scale Color", "ParticleUpdate", "Fade opacity over lifetime."),
                ],
                "material": {
                    "query": "project-owned soft translucent smoke or mist material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": ["Mist expands softly with controlled density and no abrupt opacity transitions."],
        "knowledge_topics": ["particles_invisible_generic", "iterative_parameter_drift"],
        "suggested_seed_tool": "create_sprite_niagara_preset_system",
    },
    {
        "profile_id": "sparks",
        "terms": ("spark", "sparks", "glitter", "ember", "불꽃", "스파크", "반짝", "불티"),
        "emitters": [
            {
                "emitter_id": "sparks",
                "display_name": "Sparks",
                "responsibility": "Render small bright particles with fast motion and short lifetime.",
                "renderer": "Sprite",
                "simulation_target": "GPU",
                "depends_on": [],
                "modules": [
                    _module("Spawn Burst Instantaneous", "EmitterSpawn", "Spawn a compact spark burst."),
                    _module("Initialize Particle", "ParticleSpawn", "Set small size, bright color, and short lifetime."),
                    _module("Add Velocity", "ParticleSpawn", "Set initial spark motion."),
                    _module("Gravity Force", "ParticleUpdate", "Arc sparks downward."),
                    _module("Drag", "ParticleUpdate", "Reduce velocity over time."),
                    _module("Scale Color", "ParticleUpdate", "Fade sparks."),
                ],
                "material": {
                    "query": "project-owned additive spark material",
                    "asset_path": "",
                    "source": "unresolved",
                },
            }
        ],
        "objectives": ["Sparks move quickly, arc under gravity, and fade before they become visual noise."],
        "knowledge_topics": ["gpu_bounds_culling", "particles_invisible_generic"],
        "suggested_seed_tool": "create_sprite_niagara_preset_system",
    },
]


def _tokens(value: str) -> set[str]:
    return {token.casefold() for token in TOKEN_PATTERN.findall(value or "") if token}


def _string(value: Any) -> str:
    return value.strip() if isinstance(value, str) else ""


def _path_is_temp(path: str) -> bool:
    return path.startswith(TEMP_NIAGARA_ROOT)


def _path_is_game_asset(path: str) -> bool:
    return path.startswith("/Game/") and ".." not in path and "\\" not in path


def _asset_path_has_forbidden_reference(path: str) -> bool:
    lowered = path.casefold()
    return any(token.casefold() in lowered for token in FORBIDDEN_ASSET_REFERENCE_TOKENS)


def _intent_term_matches(
    term: str,
    *,
    lowered_intent: str,
    intent_tokens: set[str],
) -> bool:
    """Match ASCII terms on token boundaries while retaining Korean phrase matching."""

    normalized = term.casefold().strip()
    if not normalized:
        return False
    if re.search(r"[a-z0-9_]", normalized):
        if normalized in intent_tokens:
            return True
        boundary_pattern = rf"(?<![0-9a-zA-Z_]){re.escape(normalized)}(?![0-9a-zA-Z_])"
        return re.search(boundary_pattern, lowered_intent, flags=re.IGNORECASE) is not None
    return normalized in lowered_intent or normalized in intent_tokens


def _dedupe_strings(values: Iterable[str]) -> list[str]:
    seen: set[str] = set()
    output: list[str] = []
    for value in values:
        normalized = _string(value)
        key = normalized.casefold()
        if normalized and key not in seen:
            seen.add(key)
            output.append(normalized)
    return output


def _default_constraints() -> dict[str, Any]:
    return {
        "mutation_policy": "temp_only",
        "max_refinement_iterations": 4,
        "max_emitters": 8,
        "promotion_required": True,
        "plugin_independence_required": True,
        "material_policy": "project_owned_or_engine",
        "preserve_editability": True,
        "selective_rollback_required": True,
        "best_state_selection_required": True,
    }


def _default_objective(objective_id: str, description: str, weight: float = 1.0) -> dict[str, Any]:
    return {
        "objective_id": objective_id,
        "description": description,
        "evidence": ["visual", "compile", "inspection"],
        "weight": weight,
    }


def _safe_identifier(value: str, fallback: str) -> str:
    tokens = TOKEN_PATTERN.findall(value.casefold())
    ascii_tokens = [re.sub(r"[^a-z0-9_]", "", token) for token in tokens]
    candidate = "_".join(token for token in ascii_tokens if token)
    if not candidate or not candidate[0].isalpha():
        candidate = fallback
    return candidate[:64]


def _merge_profile_emitters(selected_profiles: list[dict[str, Any]]) -> list[dict[str, Any]]:
    emitters: list[dict[str, Any]] = []
    used_ids: set[str] = set()
    for profile in selected_profiles:
        for raw_emitter in profile["emitters"]:
            emitter = copy.deepcopy(raw_emitter)
            base_id = emitter["emitter_id"]
            candidate_id = base_id
            suffix = 2
            while candidate_id in used_ids:
                candidate_id = f"{base_id}_{suffix}"
                suffix += 1
            emitter["emitter_id"] = candidate_id
            used_ids.add(candidate_id)
            emitters.append(emitter)

    emitter_ids = {emitter["emitter_id"] for emitter in emitters}
    source_candidates = [
        emitter["emitter_id"]
        for emitter in emitters
        if emitter["emitter_id"] not in {"impact", "trail"}
    ]
    source_id = source_candidates[0] if source_candidates else ""
    if source_id:
        for emitter in emitters:
            if emitter["emitter_id"] in {"impact", "trail"} and source_id in emitter_ids:
                emitter["depends_on"] = [source_id]
    return emitters[:MAX_EMITTERS]


def _fallback_emitter() -> dict[str, Any]:
    return {
        "emitter_id": "primary",
        "display_name": "Primary",
        "responsibility": "Render the primary visual behavior described by the intent.",
        "renderer": "Sprite",
        "simulation_target": "Auto",
        "depends_on": [],
        "modules": [
            _module("Spawn Rate", "EmitterUpdate", "Control particle density."),
            _module("Shape Location", "ParticleSpawn", "Define the spawn distribution."),
            _module("Initialize Particle", "ParticleSpawn", "Set lifetime, size, and color."),
            _module("Add Velocity", "ParticleSpawn", "Define initial motion."),
            _module("Scale Color", "ParticleUpdate", "Fade particles over lifetime."),
        ],
        "material": {
            "query": "project-owned material matching the requested silhouette and temporal behavior",
            "asset_path": "",
            "source": "unresolved",
        },
    }


def _normalize_draft_spec(intent: str, draft_spec: dict[str, Any]) -> dict[str, Any]:
    normalized = copy.deepcopy(draft_spec)
    normalized.setdefault("schema_version", EFFECT_SPEC_SCHEMA_VERSION)
    normalized["intent"] = _string(normalized.get("intent")) or intent
    normalized.setdefault("system", {})
    normalized.setdefault("emitters", [])
    normalized.setdefault("objectives", [])
    constraints = _default_constraints()
    supplied_constraints = normalized.get("constraints")
    if isinstance(supplied_constraints, dict):
        constraints.update(supplied_constraints)
    normalized["constraints"] = constraints
    grounding = normalized.get("grounding")
    if not isinstance(grounding, dict):
        grounding = {}
    grounding["repository_refs"] = _dedupe_strings(
        list(grounding.get("repository_refs") or []) + GROUNDING_REFS
    )
    grounding.setdefault("knowledge_topics", [])
    normalized["grounding"] = grounding
    return normalized


def validate_effect_spec(effect_spec: Any) -> dict[str, Any]:
    """Validate a ParticleEffectSpec without touching Unreal or the filesystem."""
    errors: list[str] = []
    warnings: list[str] = []
    if not isinstance(effect_spec, dict):
        return tool_error(
            "effect_spec must be an object",
            error_code="niagara_effect_spec_not_object",
            validation_errors=["effect_spec must be an object"],
            validation_warnings=[],
            dry_run=True,
            mutation_performed=False,
        )

    normalized = copy.deepcopy(effect_spec)
    version = normalized.get("schema_version")
    if version != EFFECT_SPEC_SCHEMA_VERSION:
        errors.append(
            f"schema_version must be {EFFECT_SPEC_SCHEMA_VERSION}, got {version!r}"
        )

    intent = _string(normalized.get("intent"))
    normalized["intent"] = intent
    if not intent:
        errors.append("intent must be a non-empty string")

    system = normalized.get("system")
    if not isinstance(system, dict):
        errors.append("system must be an object")
        system = {}
    normalized["system"] = system
    working_path = _string(system.get("working_path"))
    source_path = _string(system.get("source_system_path"))
    for field_name, path in (
        ("system.working_path", working_path),
        ("system.source_system_path", source_path),
    ):
        if path and not _path_is_game_asset(path):
            errors.append(f"{field_name} must be a normalized /Game asset path")
        if path and _asset_path_has_forbidden_reference(path):
            errors.append(f"{field_name} contains an MCP-only dependency marker")
    if working_path and not _path_is_temp(working_path):
        warnings.append(
            "system.working_path is outside the generated Niagara temp root; planning is valid but P0 patch execution stays blocked"
        )

    emitters = normalized.get("emitters")
    if not isinstance(emitters, list):
        errors.append("emitters must be an array")
        emitters = []
    normalized["emitters"] = emitters
    if not emitters:
        errors.append("emitters must contain at least one emitter")
    if len(emitters) > MAX_EMITTERS:
        errors.append(f"emitters exceeds the {MAX_EMITTERS}-emitter schema limit")

    emitter_ids: list[str] = []
    dependency_map: dict[str, list[str]] = {}
    for index, emitter in enumerate(emitters):
        prefix = f"emitters[{index}]"
        if not isinstance(emitter, dict):
            errors.append(f"{prefix} must be an object")
            continue
        emitter_id = _string(emitter.get("emitter_id"))
        if not EMITTER_ID_PATTERN.fullmatch(emitter_id):
            errors.append(
                f"{prefix}.emitter_id must match {EMITTER_ID_PATTERN.pattern}"
            )
        emitter_ids.append(emitter_id)
        if not _string(emitter.get("responsibility")):
            errors.append(f"{prefix}.responsibility must be a non-empty string")
        renderer = _string(emitter.get("renderer")) or "Auto"
        emitter["renderer"] = renderer
        if renderer not in ALLOWED_RENDERERS:
            errors.append(
                f"{prefix}.renderer must be one of {sorted(ALLOWED_RENDERERS)}"
            )
        sim_target = _string(emitter.get("simulation_target")) or "Auto"
        emitter["simulation_target"] = sim_target
        if sim_target not in ALLOWED_SIM_TARGETS:
            errors.append(
                f"{prefix}.simulation_target must be one of {sorted(ALLOWED_SIM_TARGETS)}"
            )

        depends_on = emitter.get("depends_on", [])
        if not isinstance(depends_on, list) or not all(
            isinstance(item, str) and item for item in depends_on
        ):
            errors.append(f"{prefix}.depends_on must be an array of emitter IDs")
            depends_on = []
        emitter["depends_on"] = _dedupe_strings(depends_on)
        dependency_map[emitter_id] = emitter["depends_on"]

        modules = emitter.get("modules", [])
        if not isinstance(modules, list):
            errors.append(f"{prefix}.modules must be an array")
            modules = []
        emitter["modules"] = modules
        for module_index, module in enumerate(modules):
            module_prefix = f"{prefix}.modules[{module_index}]"
            if not isinstance(module, dict):
                errors.append(f"{module_prefix} must be an object")
                continue
            if not _string(module.get("module")):
                errors.append(f"{module_prefix}.module must be a non-empty string")
            stage = _string(module.get("stage"))
            if stage not in ALLOWED_STAGES:
                errors.append(
                    f"{module_prefix}.stage must be one of {sorted(ALLOWED_STAGES)}"
                )
            if not _string(module.get("responsibility")):
                errors.append(
                    f"{module_prefix}.responsibility must be a non-empty string"
                )
            parameters = module.get("parameters", {})
            if not isinstance(parameters, dict):
                errors.append(f"{module_prefix}.parameters must be an object")

        material = emitter.get("material", {})
        if not isinstance(material, dict):
            errors.append(f"{prefix}.material must be an object")
            material = {}
        emitter["material"] = material
        material_path = _string(material.get("asset_path"))
        if material_path:
            valid_material_root = material_path.startswith(
                ("/Game/", "/Engine/", "/Niagara/")
            )
            if not valid_material_root or ".." in material_path or "\\" in material_path:
                errors.append(
                    f"{prefix}.material.asset_path must use a normalized project or engine asset path"
                )
            if _asset_path_has_forbidden_reference(material_path):
                errors.append(
                    f"{prefix}.material.asset_path contains an MCP-only dependency marker"
                )

    duplicate_emitter_ids = sorted(
        emitter_id for emitter_id in set(emitter_ids) if emitter_ids.count(emitter_id) > 1
    )
    if duplicate_emitter_ids:
        errors.append(f"duplicate emitter IDs: {duplicate_emitter_ids}")

    known_ids = set(emitter_ids)
    for emitter_id, dependencies in dependency_map.items():
        for dependency in dependencies:
            if dependency not in known_ids:
                errors.append(
                    f"emitter {emitter_id!r} depends on unknown emitter {dependency!r}"
                )
            if dependency == emitter_id:
                errors.append(f"emitter {emitter_id!r} cannot depend on itself")

    visiting: set[str] = set()
    visited: set[str] = set()

    def visit(emitter_id: str) -> bool:
        if emitter_id in visiting:
            return True
        if emitter_id in visited:
            return False
        visiting.add(emitter_id)
        for dependency in dependency_map.get(emitter_id, []):
            if dependency in dependency_map and visit(dependency):
                return True
        visiting.remove(emitter_id)
        visited.add(emitter_id)
        return False

    if any(visit(emitter_id) for emitter_id in dependency_map):
        errors.append("emitter dependency graph contains a cycle")

    objectives = normalized.get("objectives")
    if not isinstance(objectives, list):
        errors.append("objectives must be an array")
        objectives = []
    normalized["objectives"] = objectives
    if not objectives:
        errors.append("objectives must contain at least one measurable objective")
    if len(objectives) > MAX_OBJECTIVES:
        errors.append(f"objectives exceeds the {MAX_OBJECTIVES}-objective schema limit")
    objective_ids: list[str] = []
    for index, objective in enumerate(objectives):
        prefix = f"objectives[{index}]"
        if not isinstance(objective, dict):
            errors.append(f"{prefix} must be an object")
            continue
        objective_id = _string(objective.get("objective_id"))
        objective_ids.append(objective_id)
        if not EMITTER_ID_PATTERN.fullmatch(objective_id):
            errors.append(
                f"{prefix}.objective_id must match {EMITTER_ID_PATTERN.pattern}"
            )
        if not _string(objective.get("description")):
            errors.append(f"{prefix}.description must be a non-empty string")
        evidence = objective.get("evidence", [])
        if not isinstance(evidence, list) or not all(
            isinstance(item, str) and item for item in evidence
        ):
            errors.append(f"{prefix}.evidence must be an array of evidence labels")
        weight = objective.get("weight", 1.0)
        if not isinstance(weight, (int, float)) or isinstance(weight, bool) or weight <= 0:
            errors.append(f"{prefix}.weight must be a positive number")
    duplicate_objective_ids = sorted(
        objective_id
        for objective_id in set(objective_ids)
        if objective_ids.count(objective_id) > 1
    )
    if duplicate_objective_ids:
        errors.append(f"duplicate objective IDs: {duplicate_objective_ids}")

    constraints = normalized.get("constraints")
    if not isinstance(constraints, dict):
        errors.append("constraints must be an object")
        constraints = {}
    normalized["constraints"] = constraints
    mutation_policy = constraints.get("mutation_policy")
    if mutation_policy not in ALLOWED_MUTATION_POLICIES:
        errors.append(
            f"constraints.mutation_policy must be one of {sorted(ALLOWED_MUTATION_POLICIES)}"
        )
    max_iterations = constraints.get("max_refinement_iterations")
    if not isinstance(max_iterations, int) or isinstance(max_iterations, bool) or not 1 <= max_iterations <= 8:
        errors.append("constraints.max_refinement_iterations must be an integer in 1..8")
    if constraints.get("promotion_required") is not True:
        errors.append("constraints.promotion_required must be true")
    if constraints.get("plugin_independence_required") is not True:
        errors.append("constraints.plugin_independence_required must be true")
    if constraints.get("selective_rollback_required") is not True:
        errors.append("constraints.selective_rollback_required must be true")
    if constraints.get("best_state_selection_required") is not True:
        errors.append("constraints.best_state_selection_required must be true")

    grounding = normalized.get("grounding")
    if not isinstance(grounding, dict):
        errors.append("grounding must be an object")
        grounding = {}
    normalized["grounding"] = grounding

    valid = not errors
    execution_ready = bool(
        valid
        and working_path
        and _path_is_temp(working_path)
        and constraints.get("mutation_policy") == "temp_only"
    )
    response_fields = {
        "valid": valid,
        "execution_ready": execution_ready,
        "dry_run": True,
        "mutation_performed": False,
        "schema_version": EFFECT_SPEC_SCHEMA_VERSION,
        "validation_errors": errors,
        "validation_warnings": warnings,
        "normalized_spec": normalized,
        "emitter_count": len(emitters),
        "objective_count": len(objectives),
    }
    if errors:
        return tool_error(
            "ParticleEffectSpec validation failed",
            error_code="niagara_effect_spec_invalid",
            **response_fields,
        )
    return tool_success(**response_fields)


def plan_effect(
    intent: str,
    *,
    working_path: str = "",
    source_system_path: str = "",
    draft_spec: dict[str, Any] | None = None,
    material_candidates: list[dict[str, Any]] | None = None,
    constraints: dict[str, Any] | None = None,
) -> dict[str, Any]:
    """Create or normalize a dry-run ParticleEffectSpec from an artistic intent."""
    intent = _string(intent)
    if not intent:
        return tool_error(
            "intent must be a non-empty string",
            error_code="niagara_effect_intent_required",
            dry_run=True,
            mutation_performed=False,
        )
    if draft_spec is not None and not isinstance(draft_spec, dict):
        return tool_error(
            "draft_spec must be an object when provided",
            error_code="niagara_effect_draft_spec_invalid",
            dry_run=True,
            mutation_performed=False,
        )
    if material_candidates is not None and not isinstance(material_candidates, list):
        return tool_error(
            "material_candidates must be an array when provided",
            error_code="niagara_effect_material_candidates_invalid",
            dry_run=True,
            mutation_performed=False,
        )
    material_candidate_errors: list[str] = []
    for index, candidate in enumerate(material_candidates or []):
        if not isinstance(candidate, dict):
            material_candidate_errors.append(
                f"material_candidates[{index}] must be an object"
            )
            continue
        candidate_path = _string(candidate.get("asset_path"))
        if candidate_path:
            valid_root = candidate_path.startswith(("/Game/", "/Engine/", "/Niagara/"))
            if not valid_root or ".." in candidate_path or "\\" in candidate_path:
                material_candidate_errors.append(
                    f"material_candidates[{index}].asset_path must be a normalized project or engine asset path"
                )
            if _asset_path_has_forbidden_reference(candidate_path):
                material_candidate_errors.append(
                    f"material_candidates[{index}].asset_path contains an MCP-only dependency marker"
                )
    if material_candidate_errors:
        return tool_error(
            "material_candidates validation failed",
            error_code="niagara_effect_material_candidates_invalid",
            validation_errors=material_candidate_errors,
            dry_run=True,
            mutation_performed=False,
        )
    if constraints is not None and not isinstance(constraints, dict):
        return tool_error(
            "constraints must be an object when provided",
            error_code="niagara_effect_constraints_invalid",
            dry_run=True,
            mutation_performed=False,
        )

    selected_profiles: list[dict[str, Any]] = []
    lowered_intent = intent.casefold()
    intent_tokens = _tokens(intent)
    for profile in PLANNING_PROFILES:
        terms = tuple(str(term).casefold() for term in profile["terms"])
        if any(
            _intent_term_matches(
                term,
                lowered_intent=lowered_intent,
                intent_tokens=intent_tokens,
            )
            for term in terms
        ):
            selected_profiles.append(profile)

    profile_ids = [profile["profile_id"] for profile in selected_profiles]
    profile_knowledge_topics = _dedupe_strings(
        topic
        for profile in selected_profiles
        for topic in profile["knowledge_topics"]
    )
    profile_seed_tools = _dedupe_strings(
        profile["suggested_seed_tool"] for profile in selected_profiles
    )

    if draft_spec is not None:
        spec = _normalize_draft_spec(intent, draft_spec)
    else:
        emitters = _merge_profile_emitters(selected_profiles)
        if not emitters:
            emitters = [_fallback_emitter()]
        objective_descriptions = _dedupe_strings(
            objective
            for profile in selected_profiles
            for objective in profile["objectives"]
        )
        if not objective_descriptions:
            objective_descriptions = [
                "The rendered effect visibly matches the requested motion, silhouette, and temporal behavior."
            ]
        objectives = [
            _default_objective(
                _safe_identifier(description, f"objective_{index + 1}"),
                description,
            )
            for index, description in enumerate(objective_descriptions)
        ]
        objectives.extend(
            [
                _default_objective(
                    "compile_integrity",
                    "The Niagara system compiles without errors and preserves resolved module bindings.",
                    1.25,
                ),
                _default_objective(
                    "dependency_independence",
                    "The authored result has no UnrealMCP or unapproved source-content dependencies.",
                    1.5,
                ),
            ]
        )
        merged_constraints = _default_constraints()
        if constraints:
            merged_constraints.update(copy.deepcopy(constraints))
        spec = {
            "schema_version": EFFECT_SPEC_SCHEMA_VERSION,
            "intent": intent,
            "system": {
                "working_path": _string(working_path),
                "source_system_path": _string(source_system_path),
            },
            "emitters": emitters,
            "objectives": objectives[:MAX_OBJECTIVES],
            "constraints": merged_constraints,
            "grounding": {
                "repository_refs": list(GROUNDING_REFS),
                "knowledge_topics": profile_knowledge_topics,
                "material_candidates": copy.deepcopy(material_candidates or []),
                "suggested_seed_tools": profile_seed_tools
                or ["create_basic_sprite_niagara_system"],
            },
        }

    if working_path:
        spec.setdefault("system", {})["working_path"] = working_path
    if source_system_path:
        spec.setdefault("system", {})["source_system_path"] = source_system_path
    if constraints:
        merged_constraints = _default_constraints()
        if isinstance(spec.get("constraints"), dict):
            merged_constraints.update(spec["constraints"])
        merged_constraints.update(copy.deepcopy(constraints))
        spec["constraints"] = merged_constraints
    if material_candidates is not None:
        grounding = spec.setdefault("grounding", {})
        if isinstance(grounding, dict):
            grounding["material_candidates"] = copy.deepcopy(material_candidates)
    grounding = spec.setdefault("grounding", {})
    if isinstance(grounding, dict):
        grounding["repository_refs"] = _dedupe_strings(
            list(grounding.get("repository_refs") or []) + GROUNDING_REFS
        )
        grounding["knowledge_topics"] = _dedupe_strings(
            list(grounding.get("knowledge_topics") or [])
            + profile_knowledge_topics
        )
        grounding["suggested_seed_tools"] = _dedupe_strings(
            list(grounding.get("suggested_seed_tools") or [])
            + profile_seed_tools
            + ["create_basic_sprite_niagara_system"]
        )

    validation = validate_effect_spec(spec)
    if not validation.get("success"):
        validation["planner_profiles"] = profile_ids
        validation["planner_mode"] = "normalize_draft" if draft_spec is not None else "grounded_seed"
        return validation

    normalized_spec = validation["normalized_spec"]
    return tool_success(
        dry_run=True,
        mutation_performed=False,
        writes_assets=False,
        planner_mode="normalize_draft" if draft_spec is not None else "grounded_seed",
        planner_profiles=profile_ids,
        effect_spec=normalized_spec,
        validation={
            "valid": validation["valid"],
            "execution_ready": validation["execution_ready"],
            "warnings": validation["validation_warnings"],
        },
        recommended_workflow=[
            {
                "phase": "inspect",
                "tools": list(READ_ONLY_PREFLIGHT_TOOLS),
                "mutation": False,
            },
            {
                "phase": "parameterize",
                "tools": [
                    "inspect_niagara_module_inputs",
                    "validate_niagara_effect_patch",
                ],
                "mutation": False,
            },
            {
                "phase": "apply_candidate",
                "tools": ["execute_batch"],
                "mutation": True,
                "guard": "explicit follow-up call; generated Niagara temp path only",
            },
            {
                "phase": "review_and_promote",
                "tools": list(POSTFLIGHT_TOOLS),
                "mutation": False,
            },
        ],
        next_action=(
            "Inspect or create the generated temp working asset, then author an objective-based patch."
            if validation["execution_ready"]
            else "Choose an unused working_path under /Game/_MCP_Temp/NiagaraGenerated before patch execution."
        ),
    )


@lru_cache(maxsize=1)
def load_diagnostic_knowledge() -> dict[str, Any]:
    knowledge_path = Path(__file__).with_name("niagara_diagnostic_knowledge_v1.json")
    with knowledge_path.open("r", encoding="utf-8") as knowledge_file:
        knowledge = json.load(knowledge_file)
    if knowledge.get("schema_version") != 1:
        raise ValueError(
            f"Unsupported Niagara diagnostic knowledge schema: {knowledge.get('schema_version')!r}"
        )
    entries = knowledge.get("entries")
    if not isinstance(entries, list) or not entries:
        raise ValueError("Niagara diagnostic knowledge has no entries")
    return knowledge


def diagnose_effect_issue(
    observation: str,
    *,
    compile_feedback: str = "",
    system_summary: dict[str, Any] | None = None,
    previous_patch_history: list[dict[str, Any]] | None = None,
    max_results: int = 5,
) -> dict[str, Any]:
    """Retrieve symptom-cause-fix records using deterministic phrase/token scoring."""
    observation = _string(observation)
    if not observation:
        return tool_error(
            "observation must be a non-empty string",
            error_code="niagara_diagnostic_observation_required",
            dry_run=True,
            mutation_performed=False,
        )
    if system_summary is not None and not isinstance(system_summary, dict):
        return tool_error(
            "system_summary must be an object when provided",
            error_code="niagara_diagnostic_system_summary_invalid",
            dry_run=True,
            mutation_performed=False,
        )
    if previous_patch_history is not None and not isinstance(previous_patch_history, list):
        return tool_error(
            "previous_patch_history must be an array when provided",
            error_code="niagara_diagnostic_patch_history_invalid",
            dry_run=True,
            mutation_performed=False,
        )
    if not isinstance(max_results, int) or isinstance(max_results, bool) or not 1 <= max_results <= 10:
        return tool_error(
            "max_results must be an integer in 1..10",
            error_code="niagara_diagnostic_max_results_invalid",
            dry_run=True,
            mutation_performed=False,
        )

    summary_text = json.dumps(system_summary or {}, ensure_ascii=False, default=str)
    history_text = json.dumps(previous_patch_history or [], ensure_ascii=False, default=str)
    haystack = " ".join(
        value for value in (observation, _string(compile_feedback), summary_text, history_text) if value
    ).casefold()
    haystack_tokens = _tokens(haystack)
    knowledge = load_diagnostic_knowledge()
    scored: list[dict[str, Any]] = []
    for entry in knowledge["entries"]:
        score = 0.0
        matched_terms: list[str] = []
        for pattern in entry.get("patterns", []):
            normalized_pattern = str(pattern).casefold()
            if normalized_pattern and normalized_pattern in haystack:
                score += 6.0
                matched_terms.append(str(pattern))
        for keyword in entry.get("keywords", []):
            normalized_keyword = str(keyword).casefold()
            if normalized_keyword in haystack_tokens or normalized_keyword in haystack:
                score += 1.0
                matched_terms.append(str(keyword))
        for module_name in entry.get("related_modules", []):
            normalized_module = str(module_name).casefold()
            if normalized_module and normalized_module in haystack:
                score += 2.0
                matched_terms.append(str(module_name))
        if entry.get("diagnosis_id") == "compile_or_static_switch_failure" and _string(compile_feedback):
            score += 1.5
        if entry.get("diagnosis_id") == "iterative_parameter_drift" and previous_patch_history:
            score += min(3.0, len(previous_patch_history) * 0.5)
        scored.append(
            {
                "score": round(score, 3),
                "entry": entry,
                "matched_terms": _dedupe_strings(matched_terms),
            }
        )

    scored.sort(
        key=lambda item: (
            -item["score"],
            str(item["entry"].get("diagnosis_id", "")),
        )
    )
    selected = [item for item in scored if item["score"] > 0][:max_results]
    if not selected:
        generic = next(
            item
            for item in scored
            if item["entry"].get("diagnosis_id") == "particles_invisible_generic"
        )
        selected = [generic]

    top_score = selected[0]["score"]
    if top_score >= 10:
        confidence = "high"
    elif top_score >= 4:
        confidence = "medium"
    else:
        confidence = "low"

    matches: list[dict[str, Any]] = []
    evidence_tools: list[str] = []
    for item in selected:
        entry = item["entry"]
        entry_tools = list(entry.get("evidence_tools") or [])
        evidence_tools.extend(entry_tools)
        matches.append(
            {
                "diagnosis_id": entry["diagnosis_id"],
                "title": entry["title"],
                "score": item["score"],
                "matched_terms": item["matched_terms"],
                "causes": copy.deepcopy(entry.get("causes") or []),
                "fixes": copy.deepcopy(entry.get("fixes") or []),
                "evidence_tools": entry_tools,
                "related_modules": copy.deepcopy(entry.get("related_modules") or []),
                "source_refs": copy.deepcopy(entry.get("source_refs") or []),
            }
        )

    top_match = matches[0]
    objective_template = {
        "objective_id": f"diagnose_{top_match['diagnosis_id']}",
        "description": top_match["title"],
        "expected_observation": "Visible or inspection evidence confirms that the diagnosed symptom improves without regressing unrelated objectives.",
        "rollback": {
            "strategy": "restore_baseline",
            "criteria": "The expected observation does not improve, compile health regresses, or an unrelated objective deteriorates.",
        },
        "operations": [],
    }
    return tool_success(
        dry_run=True,
        mutation_performed=False,
        retrieval_mode="deterministic_phrase_token_v1",
        embedding_dependency=False,
        observation=observation,
        confidence=confidence,
        matches=matches,
        evidence_plan=_dedupe_strings(evidence_tools),
        objective_template=objective_template,
        next_action="Collect the listed evidence, then fill objective_template.operations and validate the patch before any mutation.",
    )


def _validate_patch_operation(
    operation: Any,
    *,
    operation_path: str,
    system_path: str,
) -> tuple[dict[str, Any] | None, list[str], list[str]]:
    errors: list[str] = []
    warnings: list[str] = []
    if not isinstance(operation, dict):
        return None, [f"{operation_path} must be an object"], warnings
    command = _string(operation.get("command"))
    if command not in ALLOWED_PATCH_COMMANDS:
        return None, [f"{operation_path}.command is not allowed: {command!r}"], warnings
    params = operation.get("params")
    if not isinstance(params, dict):
        return None, [f"{operation_path}.params must be an object"], warnings
    normalized = copy.deepcopy(operation)
    normalized["command"] = command
    normalized_params = copy.deepcopy(params)
    rule = ALLOWED_PATCH_COMMANDS[command]
    path_key = str(rule.get("path_key") or "system_path")
    normalized_params.setdefault(path_key, system_path)
    normalized_params.setdefault("allow_source_edit", False)
    normalized_params.setdefault("save", False)
    normalized_params.setdefault("request_compile", True)
    if rule.get("supports_wait"):
        normalized_params.setdefault("wait_for_completion", True)
    normalized["params"] = normalized_params

    if normalized_params.get(path_key) != system_path:
        errors.append(
            f"{operation_path}.params.{path_key} must exactly match patch.system_path"
        )
    alternate_path_key = "target_system_path" if path_key == "system_path" else "system_path"
    if alternate_path_key in normalized_params:
        errors.append(
            f"{operation_path}.params.{alternate_path_key} is not valid for {command}; use {path_key}"
        )
    if normalized_params.get("allow_source_edit") is not False:
        errors.append(
            f"{operation_path}.params.allow_source_edit must remain false in P0 objective patches"
        )
    if normalized_params.get("save") is not False:
        errors.append(
            f"{operation_path}.params.save must remain false; reviewed patches mutate only the temp in-memory candidate"
        )
    if not isinstance(normalized_params.get("request_compile"), bool):
        errors.append(f"{operation_path}.params.request_compile must be a boolean")
    if rule.get("supports_wait") and not isinstance(
        normalized_params.get("wait_for_completion"), bool
    ):
        errors.append(f"{operation_path}.params.wait_for_completion must be a boolean")

    def validate_parameter_value(value: Any, value_path: str) -> None:
        if isinstance(value, str):
            if _asset_path_has_forbidden_reference(value):
                errors.append(
                    f"{value_path} contains an MCP-only dependency marker"
                )
            return
        if isinstance(value, dict):
            for child_key, child_value in value.items():
                validate_parameter_value(child_value, f"{value_path}.{child_key}")
            return
        if isinstance(value, list):
            for child_index, child_value in enumerate(value):
                validate_parameter_value(child_value, f"{value_path}[{child_index}]")

    validate_parameter_value(normalized_params, f"{operation_path}.params")

    if command == "set_niagara_renderer_material":
        material_path = _string(normalized_params.get("material_path"))
        if not material_path.startswith(("/Game/", "/Engine/", "/Niagara/")) or ".." in material_path or "\\" in material_path:
            errors.append(
                f"{operation_path}.params.material_path must be a normalized project or engine asset path"
            )
    if command == "add_niagara_stock_module_to_stack":
        module_path = _string(normalized_params.get("module_script_path"))
        if not module_path.startswith(("/Game/", "/Engine/", "/Niagara/")) or ".." in module_path or "\\" in module_path:
            errors.append(
                f"{operation_path}.params.module_script_path must be a normalized project or engine asset path"
            )
    for required_key in rule.get("required", ()):
        if required_key not in normalized_params or normalized_params[required_key] in (None, ""):
            errors.append(
                f"{operation_path}.params.{required_key} is required for {command}"
            )
    one_of = rule.get("one_of", ())
    if one_of and not any(
        key in normalized_params and normalized_params[key] not in (None, "", [], {})
        for key in one_of
    ):
        errors.append(
            f"{operation_path}.params must provide at least one of {list(one_of)} for {command}"
        )
    if command == "set_niagara_emitter_fixed_bounds":
        has_min = "min" in normalized_params and normalized_params.get("min") is not None
        has_max = "max" in normalized_params and normalized_params.get("max") is not None
        if has_min != has_max:
            errors.append(
                f"{operation_path}.params.min and max must be provided together"
            )
    if command == "set_niagara_module_inputs_batch":
        edits = normalized_params.get("edits")
        if not isinstance(edits, list) or not edits:
            errors.append(f"{operation_path}.params.edits must be a non-empty array")
        elif len(edits) > MAX_PATCH_OPERATIONS:
            errors.append(
                f"{operation_path}.params.edits exceeds the {MAX_PATCH_OPERATIONS}-edit limit"
            )
    if rule.get("destructive_within_candidate"):
        warnings.append(
            f"{operation_path} removes modules from the temp candidate and requires baseline restoration evidence"
        )
    if normalized_params.get("request_compile") is False:
        warnings.append(
            f"{operation_path} disables compile request; postflight compile inspection remains required"
        )
    return normalized, errors, warnings


def validate_effect_patch(
    patch: Any,
    *,
    effect_spec: dict[str, Any] | None = None,
    allow_non_temp_target: bool = False,
) -> dict[str, Any]:
    """Validate an objective-based Niagara patch and emit a non-executed manifest."""
    errors: list[str] = []
    warnings: list[str] = []
    if not isinstance(patch, dict):
        return tool_error(
            "patch must be an object",
            error_code="niagara_effect_patch_not_object",
            validation_errors=["patch must be an object"],
            validation_warnings=[],
            dry_run=True,
            mutation_performed=False,
        )
    if effect_spec is not None and not isinstance(effect_spec, dict):
        return tool_error(
            "effect_spec must be an object when provided",
            error_code="niagara_effect_patch_spec_invalid",
            dry_run=True,
            mutation_performed=False,
        )

    normalized = copy.deepcopy(patch)
    version = normalized.get("schema_version")
    if version != EFFECT_PATCH_SCHEMA_VERSION:
        errors.append(
            f"schema_version must be {EFFECT_PATCH_SCHEMA_VERSION}, got {version!r}"
        )
    patch_id = _string(normalized.get("patch_id"))
    normalized["patch_id"] = patch_id
    if not EMITTER_ID_PATTERN.fullmatch(patch_id):
        errors.append(f"patch_id must match {EMITTER_ID_PATTERN.pattern}")
    baseline_state_id = _string(normalized.get("baseline_state_id"))
    normalized["baseline_state_id"] = baseline_state_id
    if not baseline_state_id:
        errors.append("baseline_state_id must be a non-empty string")

    system_path = _string(normalized.get("system_path"))
    normalized["system_path"] = system_path
    if not _path_is_game_asset(system_path):
        errors.append("system_path must be a normalized /Game asset path")
    if _asset_path_has_forbidden_reference(system_path):
        errors.append("system_path contains an MCP-only dependency marker")
    if system_path and not _path_is_temp(system_path):
        if not allow_non_temp_target:
            errors.append(
                f"system_path must remain under the generated Niagara temp root {TEMP_NIAGARA_ROOT} for P0 patch validation"
            )
        else:
            warnings.append(
                "non-temp patch target explicitly allowed for validation only; this does not approve mutation or bypass promotion"
            )

    if effect_spec is not None:
        spec_validation = validate_effect_spec(effect_spec)
        if not spec_validation.get("success"):
            errors.append("effect_spec is invalid")
        else:
            spec_working_path = _string(
                spec_validation["normalized_spec"].get("system", {}).get("working_path")
            )
            if spec_working_path and spec_working_path != system_path:
                errors.append(
                    "patch.system_path must match effect_spec.system.working_path"
                )

    objectives = normalized.get("objectives")
    if not isinstance(objectives, list):
        errors.append("objectives must be an array")
        objectives = []
    normalized["objectives"] = objectives
    if not objectives:
        errors.append("objectives must contain at least one objective")
    if len(objectives) > MAX_OBJECTIVES:
        errors.append(f"objectives exceeds the {MAX_OBJECTIVES}-objective limit")

    objective_ids: list[str] = []
    bridge_commands: list[dict[str, Any]] = []
    normalized_objectives: list[dict[str, Any]] = []
    for objective_index, objective in enumerate(objectives):
        prefix = f"objectives[{objective_index}]"
        if not isinstance(objective, dict):
            errors.append(f"{prefix} must be an object")
            continue
        normalized_objective = copy.deepcopy(objective)
        objective_id = _string(normalized_objective.get("objective_id"))
        normalized_objective["objective_id"] = objective_id
        objective_ids.append(objective_id)
        if not EMITTER_ID_PATTERN.fullmatch(objective_id):
            errors.append(f"{prefix}.objective_id must match {EMITTER_ID_PATTERN.pattern}")
        if not _string(normalized_objective.get("description")):
            errors.append(f"{prefix}.description must be a non-empty string")
        if not _string(normalized_objective.get("expected_observation")):
            errors.append(
                f"{prefix}.expected_observation must be a non-empty string"
            )

        rollback = normalized_objective.get("rollback")
        if not isinstance(rollback, dict):
            errors.append(f"{prefix}.rollback must be an object")
            rollback = {}
        normalized_objective["rollback"] = rollback
        rollback_strategy = _string(rollback.get("strategy"))
        if rollback_strategy not in ALLOWED_ROLLBACK_STRATEGIES:
            errors.append(
                f"{prefix}.rollback.strategy must be one of {sorted(ALLOWED_ROLLBACK_STRATEGIES)}"
            )
        if not _string(rollback.get("criteria")):
            errors.append(f"{prefix}.rollback.criteria must be a non-empty string")
        if rollback_strategy == "inverse_operations":
            inverse_operations = rollback.get("inverse_operations")
            if not isinstance(inverse_operations, list) or not inverse_operations:
                errors.append(
                    f"{prefix}.rollback.inverse_operations must be a non-empty array for inverse_operations strategy"
                )

        operations = normalized_objective.get("operations")
        if not isinstance(operations, list):
            errors.append(f"{prefix}.operations must be an array")
            operations = []
        normalized_operations: list[dict[str, Any]] = []
        if not operations:
            warnings.append(
                f"{prefix}.operations is empty; the objective is diagnostic-only and not execution-ready"
            )
        for operation_index, operation in enumerate(operations):
            normalized_operation, operation_errors, operation_warnings = _validate_patch_operation(
                operation,
                operation_path=f"{prefix}.operations[{operation_index}]",
                system_path=system_path,
            )
            errors.extend(operation_errors)
            warnings.extend(operation_warnings)
            if normalized_operation is not None:
                normalized_operations.append(normalized_operation)
                bridge_commands.append(
                    {
                        "type": normalized_operation["command"],
                        "params": normalized_operation["params"],
                    }
                )
        normalized_objective["operations"] = normalized_operations
        normalized_objectives.append(normalized_objective)

    normalized["objectives"] = normalized_objectives
    duplicate_objective_ids = sorted(
        objective_id
        for objective_id in set(objective_ids)
        if objective_ids.count(objective_id) > 1
    )
    if duplicate_objective_ids:
        errors.append(f"duplicate objective IDs: {duplicate_objective_ids}")
    if len(bridge_commands) > MAX_PATCH_OPERATIONS:
        errors.append(
            f"patch contains {len(bridge_commands)} operations; execute_batch supports at most {MAX_PATCH_OPERATIONS}"
        )
    if bridge_commands and bridge_commands[-1]["params"].get("request_compile") is not True:
        errors.append(
            "the final patch operation must keep request_compile=true so postflight can observe a compiled candidate"
        )

    valid = not errors
    execution_ready = bool(
        valid
        and bridge_commands
        and system_path
        and _path_is_temp(system_path)
        and all(objective.get("operations") for objective in normalized_objectives)
    )
    manifest_commands = bridge_commands if execution_ready else []
    blocked_commands = [] if execution_ready else bridge_commands
    response_fields = {
        "valid": valid,
        "execution_ready": execution_ready,
        "dry_run": True,
        "mutation_performed": False,
        "writes_assets": False,
        "schema_version": EFFECT_PATCH_SCHEMA_VERSION,
        "validation_errors": errors,
        "validation_warnings": warnings,
        "normalized_patch": normalized,
        "operation_count": len(bridge_commands),
        "execution_manifest": {
            "tool": "execute_batch",
            "commands": manifest_commands,
            "blocked_commands": blocked_commands,
            "stop_on_error": True,
            "dispatch_performed": False,
            "requires_explicit_follow_up_call": True,
            "fail_closed": True,
        },
        "preflight_tools": list(READ_ONLY_PREFLIGHT_TOOLS),
        "postflight_tools": list(POSTFLIGHT_TOOLS),
        "rollback_policy": {
            "baseline_state_id": baseline_state_id,
            "selective_by_objective": True,
            "best_state_selection_required": True,
        },
    }
    if errors:
        return tool_error(
            "Niagara objective patch validation failed",
            error_code="niagara_effect_patch_invalid",
            **response_fields,
        )
    return tool_success(**response_fields)
