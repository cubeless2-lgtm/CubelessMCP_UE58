"""Exact additive two-name gate for root-owned AI registration integration.

No class creation, pending result, world call or transport. Existing canonical
AI schemas remain with their existing validator; only these two return schemas
are object-valued native async completions rather than OperationResult structs.
"""
from cubeless_mcp_ue58.compatibility.ue58 import ai_live as adapter

NAMES = ("run_ai_dummy_patrol_chase_smoke", "run_ai_behavior_tree_execution_smoke")
_COMMON = {
    "pawn_asset_path": ("string", "/Game/Cubeless/Reactive/BP_Piper_ReactivePrototype"),
    "skeletal_mesh_path": ("string", "/Game/Cubeless/Character/Piper/SK_Piper"),
    "anim_blueprint_path": ("string", "/Game/Cubeless/Character/Animation/ABP_RPGChracter"),
    "report_dir": ("string", "Docs/Analysis/AIDummySmoke"),
    "setup_wait_seconds": ("number", 1.5)}
CONTRACTS = {
    NAMES[0]: {**_COMMON, "label_prefix": ("string", "MCP_AIDummySmoke"),
        "cleanup_existing": ("boolean", True), "cleanup_after": ("boolean", True),
        "patrol_wait_seconds": ("number", 2.0), "chase_wait_seconds": ("number", 2.0),
        "sight_radius": ("number", 900.0), "acceptance_radius": ("number", 55.0)},
    NAMES[1]: {**_COMMON, "label_prefix": ("string", "MCP_AIBTExecutionSmoke"),
        "behavior_tree_path": ("string", "/Game/Cubeless/AI/Smoke/BT_AIDummyPatrolChase.BT_AIDummyPatrolChase"),
        "blackboard_path": ("string", "/Game/Cubeless/AI/Smoke/BB_AIDummyPatrolChase.BB_AIDummyPatrolChase"),
        "target_actor_key": ("string", "TargetActor"), "patrol_location_key": ("string", "PatrolLocation"),
        "bt_wait_seconds": ("number", 5.0)}}


def binding_status(toolset_class):
    try:
        native_type = adapter.result_class()
        native_ready, error = True, ""
    except Exception as failure:
        native_type, native_ready, error = None, False, str(failure)
    present = {name for name in NAMES if callable(getattr(toolset_class, name, None))}
    expected = set(NAMES) if native_ready else set()
    return {"ready": native_ready and present == expected, "native_ready": native_ready,
        "definition_matches_dependency": present == expected, "present_names": sorted(present),
        "expected_names": sorted(expected), "result_type": str(native_type) if native_type else "",
        "error": error if present == expected else "Stale generated AI class differs from its native async dependency; restart after repair",
        "verification_scope": "Exact native result type and two definition names only; no world/configuration/behavior proof"}


def validate_live_schema(schema, toolset_class):
    status = binding_status(toolset_class)
    if not status["definition_matches_dependency"]:
        raise RuntimeError(status["error"])
    rows = [row for row in schema.get("tools", []) if row["name"].rsplit(".", 1)[-1] in NAMES]
    expected = set(NAMES) if status["native_ready"] else set()
    if len(rows) != len(expected) or {row["name"].rsplit(".", 1)[-1] for row in rows} != expected:
        raise RuntimeError("AI async schema does not match the exact dependency-backed two-name definition")
    for row in rows:
        name = row["name"].rsplit(".", 1)[-1]
        inputs = row["inputSchema"]
        fields = CONTRACTS[name]
        props = inputs.get("properties", {})
        if inputs.get("required", []) or set(props) != set(fields):
            raise RuntimeError(name + " changed the original optional input names")
        for key, (kind, original_default) in fields.items():
            actual = props[key]
            default = actual.get("default")
            same_type = type(default) in (int, float) if kind == "number" else type(default) is type(original_default)
            if actual.get("type") != kind or "default" not in actual or not same_type or default != original_default:
                raise RuntimeError(name + "." + key + " changed the original reflected type/default")
        output = row.get("outputSchema", {})
        value = output.get("properties", {}).get("returnValue", {})
        if output.get("type") != "object" or set(output.get("required", [])) != {"returnValue"} or set(output.get("properties", {})) != {"returnValue"}:
            raise RuntimeError(name + " changed the official single returnValue envelope")
        # Installed FJsonObjectWrapper generates an open object schema; no
        # canonical OperationResult, promise-ref or quoted string is admitted.
        if value.get("type") != "object" or value.get("properties") or value.get("required") or any(key in value for key in ("$ref", "anyOf", "oneOf")):
            raise RuntimeError(name + " must expose the native JsonObjectWrapper async Value schema")
    return True
