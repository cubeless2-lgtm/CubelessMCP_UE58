"""Original catalog contracts over the existing local official Registry.

Static metadata preserves the observed UE56 433 catalog + 3 exposure-only
contracts. It cannot register, enable, disable or dispatch an Engine action.
The installed Registry remains the sole runtime catalog and execution owner.
"""
import copy
from contextvars import ContextVar
import difflib
import json
import math
from pathlib import Path
import unreal

_CALL_DEPTH = ContextVar("cubeless_discovery_call_depth", default=0)
# Original process-local policy overrides; never a tool/dispatch registry.
# All entry points require the Editor GT. Reloading this module resets these
# overrides, like restarting the old FastMCP process; no disk configuration.
_DISABLED_TOOLS = set()
_DISABLED_CATEGORIES = set()


def _json(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False)


def _error(message, code="error", **fields):
    return dict(success=False, message=message, error=message, error_code=code, **fields)


def _contracts():
    # Plugin-relative immutable source contracts, never an archived runtime path.
    return json.loads(Path(__file__).with_name("discovery_contracts.json").read_text(encoding="utf-8"))


def _schema_accepts(value, schema):
    if not isinstance(schema, dict):
        return False
    if any(key in schema for key in ("$ref", "oneOf", "allOf", "not")):
        return False  # Unsupported logical variants are not guessed.
    if "anyOf" in schema:
        return any(_schema_accepts(value, child) for child in schema["anyOf"])
    if "enum" in schema and value not in schema["enum"]:
        return False
    kind = schema.get("type")
    valid = {"null": value is None, "boolean": type(value) is bool,
        "integer": type(value) is int, "number": type(value) in (int, float) and math.isfinite(value),
        "string": isinstance(value, str), "array": isinstance(value, list),
        "object": isinstance(value, dict)}
    if kind is not None and not valid.get(kind, False):
        return False
    if isinstance(value, list) and isinstance(schema.get("items"), dict):
        return all(_schema_accepts(item, schema["items"]) for item in value)
    if isinstance(value, dict):
        if any(key not in value for key in schema.get("required", [])):
            return False
        props = schema.get("properties", {})
        for key, item in value.items():
            if key in props and not _schema_accepts(item, props[key]):
                return False
            if key not in props and schema.get("additionalProperties") is False:
                return False
    return True


def _original_arguments(record, arguments, bindings=None):
    schema = record["input_schema"]
    props = schema.get("properties", {})
    # Original pinned Pydantic argument models ignore extra fields. Keep only
    # declared values and source defaults; do not inject new current defaults.
    result = {key: copy.deepcopy(value) for key, value in arguments.items() if key in props}
    if not _schema_accepts(result, schema):
        raise ValueError("Original supplied argument shape requires exact JSON types; coercible Pydantic-only variants remain unsupported")
    # Pinned Pydantic may apply an omitted None default even when its emitted
    # supplied-value schema says array/string. Do not reject source defaults by
    # revalidating them as if the caller explicitly supplied that value.
    for key, field in props.items():
        if key not in result and "default" in field:
            result[key] = copy.deepcopy(field["default"])
    if record["name"] == "configure_anchor_field" and "field_magnitude" not in arguments:
        # ADQ observed a real partial spawn with the inherited invalid default1.
        # Native Chaos::EObjectStateType::Kinematic is2. Only this
        # repaired default may use the exact current logical schema's value2;
        # preserve historical metadata1 and every explicitly supplied value.
        toolset = "cubeless_mcp_ue58.registration.chaos_geometry_authoring_toolset.CubelessChaosGeometryAuthoringTools"
        current = [row for row in (bindings or {}).get(record["name"], []) if row["toolset"] == toolset
            and row["schema"].get("properties", {}).get("field_magnitude", {}).get("type") == "integer"
            and type(row["schema"].get("properties", {}).get("field_magnitude", {}).get("default")) is int
            and row["schema"]["properties"]["field_magnitude"]["default"] == 2]
        if len(current) != 1:
            raise ValueError("Omitted configure_anchor_field.field_magnitude requires the exact corrected current logical default2; invalid historical default1 is not dispatched")
        result["field_magnitude"] = current[0]["schema"]["properties"]["field_magnitude"]["default"]
    return result


def _bindings():
    schemas = json.loads(unreal.ToolsetRegistry.get_all_toolset_json_schemas())
    if not isinstance(schemas, list):
        raise ValueError("Official registry did not return its schema array")
    out = {}
    for schema in schemas:
        if not isinstance(schema, dict) or not isinstance(schema.get("name"), str):
            continue
        # This compatibility surface maps migrated Cubeless tools only. A stock
        # tool with the same leaf name is not an equivalence proof.
        if "Cubeless" not in schema["name"]:
            continue
        for tool in schema.get("tools", []):
            name = tool.get("name", "")
            if not isinstance(name, str) or not name:
                continue
            leaf = name.rsplit(".", 1)[-1]
            out.setdefault(leaf, []).append({"toolset": schema["name"], "method": leaf,
                "schema": copy.deepcopy(tool.get("inputSchema", {})),
                "description": tool.get("description", "")})
    return out


# Fixed source-proven input-carrier correspondences, not another tool/dispatch
# registry. The current Registry still selects and owns the single handler.
# Each tuple names an existing facade and only its JSON-value FString inputs.
# Existing Native logical schemas take precedence and are never JSON-quoted.
_SOURCE_JSON_CARRIERS = (
    ("execute_batch", "batch_toolset.CubelessBatchTools", (("commands", "commands_json"),)),
    ("set_component_property", "component_toolset.CubelessComponentTools", (("property_value", "property_value_json"),)),
    ("set_blueprint_pin_default", "graph_toolset.CubelessBlueprintGraphTools", (("value", "value_json"),)),
    ("set_blueprint_variable_default", "toolsets.CubelessBlueprintTools", (("default_value", "default_value_json"),)),
    ("set_material_node_property", "material_toolset.CubelessMaterialTools", (("value", "value_json"),)),
    ("set_niagara_user_parameter", "niagara_parameters_toolset.CubelessNiagaraParameterTools", (("value", "value_json"),)),
    ("upsert_niagara_user_parameters", "niagara_parameters_toolset.CubelessNiagaraParameterTools", (("parameters", "parameters_json"),)),
    ("validate_niagara_effect_spec", "niagara_planning_toolset.CubelessNiagaraPlanningTools", (("effect_spec", "effect_spec_json"),)),
    ("validate_niagara_effect_patch", "niagara_planning_toolset.CubelessNiagaraPlanningTools", (("patch", "patch_json"), ("effect_spec", "effect_spec_json"))),
    ("create_niagara_refinement_ledger", "niagara_planning_toolset.CubelessNiagaraPlanningTools", (("objectives", "objectives_json"), ("baseline_state", "baseline_state_json"))),
    ("evaluate_niagara_refinement_iteration", "niagara_planning_toolset.CubelessNiagaraPlanningTools", (("ledger", "ledger_json"), ("candidate_state", "candidate_state_json"), ("objective_ids", "objective_ids_json"))),
    ("select_niagara_best_refinement_state", "niagara_planning_toolset.CubelessNiagaraPlanningTools", (("ledger", "ledger_json"),)),
    ("critique_niagara_preview_evidence", "niagara_preview_toolset.CubelessNiagaraPreviewTools", (("candidate_frame_paths", "candidate_frame_paths_json"), ("criteria", "criteria_json"), ("baseline_frame_paths", "baseline_frame_paths_json"), ("roi", "roi_json"))),
)


def _carrier_fields(record, binding):
    profile = next((row for row in _SOURCE_JSON_CARRIERS if row[0] == record["name"]
        and binding["toolset"] == "cubeless_mcp_ue58.registration." + row[1]), None)
    if profile is None:
        return None
    fields = dict(profile[2])
    props = binding["schema"].get("properties", {})
    original = record["input_schema"].get("properties", {})
    if not set(fields).issubset(original):
        return None
    # Require the real live declared FString fields. A similarly named tool,
    # changed source descriptor or attached logical JSON schema is not a match.
    for target in fields.values():
        field = props.get(target, {})
        branches = field.get("anyOf")
        string_carrier = field.get("type") == "string" or (isinstance(branches, list)
            and bool(branches) and any(branch.get("type") == "string" for branch in branches)
            and all(branch.get("type") in ("string", "null") for branch in branches))
        if not string_carrier or not _schema_accepts("", field):
            return None
    translated = {fields.get(key, key) for key in original
        if not (record["name"] == "execute_batch" and key == "timeout_seconds")}
    if not translated.issubset(props):
        return None  # Never silently discard an original declared argument.
    if any(key not in translated and "default" not in props.get(key, {})
            for key in binding["schema"].get("required", [])):
        return None
    return fields


def _current_arguments(record, binding, arguments):
    if "input_adaptation" not in binding:
        return arguments  # Already-logical Native schema remains authoritative.
    fields = _carrier_fields(record, binding)
    if fields is None:
        raise ValueError("Source-proven JSON carrier no longer matches the current declared schema")
    packed = {}
    for key, value in arguments.items():
        if record["name"] == "execute_batch" and key == "timeout_seconds":
            # Original batch_tools.py used this only as a client socket timeout,
            # never as Engine cancellation. No socket exists on this local path.
            # Keep default0 usable; nonzero transport overrides need a separate
            # client policy and must not silently change an Engine deadline.
            if type(value) is not int or value != 0:
                raise ValueError("execute_batch timeout_seconds supports original default 0 only on the local path; nonzero client socket timeout overrides remain unsupported, and no Engine work was dispatched")
            continue
        if key in fields:
            # Exactly one encoding of the original semantic value. A supplied
            # string containing JSON text remains a string, not a parsed object.
            packed[fields[key]] = _json(value)
        else:
            packed[key] = value
    return packed


def _choose(record, bindings, arguments=None):
    candidates = []
    original = record["input_schema"].get("properties", {})
    fixed = any(row[0] == record["name"] for row in _SOURCE_JSON_CARRIERS)
    for row in bindings.get(record["name"], []):
        props = row["schema"].get("properties", {})
        direct = (set(arguments).issubset(props) and _schema_accepts(arguments, row["schema"])) if arguments is not None else (
            set(record["input_schema"].get("required", [])).issubset(props)
            and (not fixed or set(original).issubset(props))
            and all(key in original or "default" in props.get(key, {}) for key in row["schema"].get("required", [])))
        if direct:
            candidates.append(row)
            continue
        fields = _carrier_fields(record, row)
        if fields is None:
            continue
        adapted = copy.deepcopy(row)
        adapted["input_adaptation"] = dict(kind="fixed_source_JSON_value_to_FString",
            fields=fields, logical_schema_authority="current registered schema; logical inputs are not encoded",
            scope="Existing exact facade only; domain support limits and original output/transaction differences remain",
            batch_timeout="Original default0 only; nonzero old client socket overrides unsupported" if record["name"] == "execute_batch" else None)
        if arguments is not None:
            packed = _current_arguments(record, adapted, arguments)
            if not set(packed).issubset(props) or not _schema_accepts(packed, row["schema"]):
                continue
        candidates.append(adapted)
    return candidates[0] if len(candidates) == 1 else None, len(candidates)



def _policy_reason(record):
    if record["protected"]:
        return ""
    if record["name"] in _DISABLED_TOOLS:
        return "tool '%s' is disabled" % record["name"]
    if record["category"] in _DISABLED_CATEGORIES:
        return "category '%s' is disabled" % record["category"]
    return ""


def _refresh_policy(records):
    for row in records:
        reason = _policy_reason(row)
        row["enabled"] = row["binding_candidates"] > 0 and not reason
        row["disabled_reason"] = reason or ("" if row["binding_candidates"] > 0
            else "Current original-compatible logical-schema binding unavailable")
    return records


def _records():
    contracts = _contracts()
    bindings = _bindings()
    result = []
    for original in contracts["catalog"]:
        row = copy.deepcopy(original)
        binding, count = _choose(row, bindings)
        if row["name"] == "configure_anchor_field":
            row["default_compatibility"] = dict(field="field_magnitude", historical_default=1,
                corrected_default=2, condition="Omitted field only and exact registered logical schema confirms integer default2",
                explicit_values_preserved=True, source="ADQ Native Kinematic2 default repair; original failure/partial spawn retained")
        row.update(enabled=count > 0,
            disabled_reason="" if count > 0 else "Current original-compatible logical-schema binding unavailable",
            binding=binding, binding_candidates=count)
        result.append(row)
    return contracts, _refresh_policy(result), bindings


def _matching(records, toolset="", query=""):
    category = toolset.strip().lower()
    query = query.strip().casefold()
    return [row for row in records
        if category in ("", "all", "*", row["category"].casefold())
        and (not query or query in " ".join((row["name"], row["category"], row["description"], row["module"],
            (row.get("binding") or {}).get("description", ""))).casefold())]


def _context():
    return {"catalog_source": "UE56 actual433 immutable contracts + current official Registry",
        "exposure_only_count": 3, "legacy_union_count": 436,
        "disabled_state": "Current compatible registered candidates plus this module-local original policy overrides; old process overrides are not imported and ambiguous inputs remain unresolved",
        "unsupported": ["Ambiguous/renamed or incompatible logical-schema bindings", "Original Pydantic coercion-only variants", "Every-handler output equivalence not proven"],
        "server_rpc": False, "engine_behavior_proven_by_metadata": False}


def list_toolsets(query=""):
    try:
        from .local_api import _runtime
        _runtime()
        contracts, records, _ = _records()
        matched = _matching(records, query=query)
        rows = []
        for category in sorted({row["category"] for row in records}):
            all_rows = [r for r in records if r["category"] == category]
            matches = [r for r in matched if r["category"] == category]
            if query.strip() and not matches:
                continue
            effects = {}
            for row in all_rows:
                effect = row["policy"]["effect"]
                effects[effect] = effects.get(effect, 0) + 1
            rows.append(dict(name=category, description=contracts["categories"].get(category, ""),
                total_tools=len(all_rows), enabled_tools=sum(r["enabled"] for r in all_rows),
                matching_tools=len(matches), effects=effects))
        return dict(success=True, exposure_mode="search", query=query, total_toolsets=len(rows),
            total_catalog_tools=len(records), matching_tools=len(matched), toolsets=rows,
            next_step="Call describe_toolset with a toolset name, or use toolset='all' with a query.",
            migration_context=_context())
    except Exception as error:
        return _error(str(error), "catalog_unavailable")


def _terse(description):
    # Same source first-sentence rule, including the original abbreviation list.
    text = " ".join(str(description or "").split()).strip()
    end = len(text)
    for index, ch in enumerate(text):
        if ch not in ".!?" or index + 1 < len(text) and not text[index + 1].isspace():
            continue
        start = text.rfind(" ", 0, index) + 1
        token = text[start:index + 1].casefold().lstrip("([{\"'")
        if token in {"e.g.", "i.e.", "etc.", "vs.", "mr.", "mrs.", "dr.", "u.s."}:
            continue
        end = index + 1
        break
    text = text[:end].strip()
    if len(text) <= 120:
        return text
    cut = text.rfind(" ", 0, 118)
    return text[:cut if cut > 0 else 117].rstrip() + "..."


def describe_toolset(toolset, query="", offset=0, limit=20, include_schema=True, verbosity="full"):
    try:
        from .local_api import _runtime
        _runtime()
        _, records, _ = _records()
        category = toolset.strip().lower()
        categories = sorted({r["category"] for r in records})
        if category not in ("", "all", "*") and category not in categories:
            return _error("Unknown toolset: " + toolset, "unknown_toolset", toolset=toolset, available_toolsets=categories)
        if offset < 0:
            return _error("offset must be at least 0", "invalid_offset", offset=offset)
        if not 1 <= limit <= 50:
            return _error("limit must be between 1 and 50", "invalid_limit", limit=limit)
        mode = verbosity.strip().lower()
        if mode not in ("full", "terse"):
            return _error("verbosity must be either 'full' or 'terse'", "invalid_verbosity", verbosity=verbosity)
        matches = _matching(records, toolset, query)
        rows = []
        for row in matches[offset:offset + limit]:
            if mode == "terse":
                item = dict(name=row["name"], summary=_terse(row["description"]),
                    effect=row["policy"]["effect"], confirm_required=row["policy"]["confirmation_required"])
            else:
                item = dict(name=row["name"], toolset=row["category"], description=row["description"],
                    enabled=row["enabled"], policy=copy.deepcopy(row["policy"]))
                if include_schema:
                    item["input_schema"] = copy.deepcopy(row["input_schema"])
            item["current_binding"] = copy.deepcopy(row["binding"])
            item["disabled_reason"] = row["disabled_reason"]
            rows.append(item)
        next_offset = offset + len(rows)
        return dict(success=True, exposure_mode="search", toolset=toolset, query=query, offset=offset,
            limit=limit, total_matches=len(matches), returned_tools=len(rows),
            next_offset=next_offset if next_offset < len(matches) else None, tools=rows,
            next_step="Call call_tool with the exact toolset, tool_name, and arguments.", migration_context=_context())
    except Exception as error:
        return _error(str(error), "catalog_unavailable")


def _confirmation_required(policy, arguments, confirmed):
    if confirmed or not policy["confirmation_required"]:
        return False
    if policy["confirmation_mode"] == "dispatcher_confirm_if_replace_existing" and arguments.get("replace_existing", False) is False:
        return False
    if policy["supports_dry_run"] and arguments.get("dry_run", policy["dry_run_default"]) is True:
        return False
    return True



def _held_source_receipt(output, source):
    from .local_api import _source_receipt
    receipt = _source_receipt(source)
    try:
        receipt["source_retained_by_receiver"] = output.get_editor_property("source") == source
    except Exception as error:
        receipt["source_hold_observation_error"] = str(error)
    return receipt


def complete_call(output, source):
    if output.get_editor_property("is_complete"):
        return
    try:
        if output.get_editor_property("source") != source or not source.get_editor_property("is_complete"):
            raise ValueError("Discovery completion source identity or terminal state changed")
        if source.get_editor_property("error"):
            raise ValueError(source.get_editor_property("error"))
        # ExecuteTool's Value is already a JSON string, not JSON-of-JSON input.
        raw = source.get_editor_property("value")
        value = json.loads(raw)
        result = value.get("returnValue", value) if isinstance(value, dict) else value
        if isinstance(result, str):
            result = json.loads(result)
        if isinstance(result, dict) and all(k in result for k in ("data_json", "changes_json", "save_json", "compile_json")):
            record = copy.deepcopy(result)
            for key in ("changes", "compile", "save", "validation", "retry", "data"):
                record[key] = json.loads(record.pop(key + "_json"))
            from .local_api import _payload
            payload, basis = _payload(output.get_editor_property("legacy_tool_name"), record)
            result = copy.deepcopy(payload)
            result["cubeless_operation_receipt"] = record
            result["cubeless_payload_projection"] = basis
            # Preserve actual postwrite/unknown failure even if original native
            # legacy payload itself says success=True.
            if record.get("success") is not True:
                result.update(success=False, error=record.get("error", ""), error_code=record.get("error_code", ""))
        if not isinstance(result, (dict, list)):
            raise ValueError("Original catalog result projection requires a JSON object/list; actual raw result retained")
        source.on_completed.remove_function(output, "consume_source")
        output.set_value(_json(result))
    except Exception as error:
        from .local_api import _source_receipt
        output.set_value(_json(_error(str(error), "tool_validation_or_execution_error",
            tool=output.get_editor_property("legacy_tool_name"), mutation_state_unknown=True,
            automatic_retry=False, source_receipt=_held_source_receipt(output, source))))


def call_tool(toolset, tool_name, arguments=None, confirm=False):
    from .local_api import _runtime, _source_receipt
    from ..registration.discovery_result import CubelessDiscoveryAsyncResult
    _runtime()  # GT before any UObject allocation or Registry execution.
    output = CubelessDiscoveryAsyncResult()
    dispatched = False
    try:
        if _CALL_DEPTH.get():
            output.set_value(_json(_error("Recursive discovery dispatch is not permitted", "recursive_tool_call")))
            return output
        contracts, records, bindings = _records()
        name = tool_name.strip()
        record = next((r for r in records if r["name"] == name), None)
        if record is None:
            candidates = [r["name"] for r in _matching(records, toolset)]
            output.set_value(_json(_error("Unknown tool: " + tool_name, "unknown_tool", tool=tool_name,
                hint="Use list_toolsets and describe_toolset before dispatch.",
                did_you_mean=difflib.get_close_matches(name, sorted(candidates), n=3, cutoff=0.0) if name and candidates else [],
                contract_hint="Call describe_toolset to inspect the exact contract.")))
            return output
        category = toolset.strip().lower()
        if category not in (record["category"].casefold(), "all", "*"):
            output.set_value(_json(_error("Tool '%s' belongs to toolset '%s', not '%s'" % (name, record["category"], toolset),
                "toolset_mismatch", tool=name, requested_toolset=toolset, actual_toolset=record["category"])))
            return output
        if arguments is None:
            arguments = {}
        if not isinstance(arguments, dict):
            output.set_value(_json(_error("arguments must be a JSON object", "invalid_arguments", tool=name)))
            return output
        if not record["enabled"]:
            output.set_value(_json(_error(record["disabled_reason"], "disabled", tool=name, category=record["category"],
                migration_scope="Compatibility frontdoor policy or current binding availability only; official Named/Engine direct calls are unaffected")))
            return output
        policy = record["policy"]
        if _confirmation_required(policy, arguments, confirm):
            output.set_value(_json(_error("Explicit confirmation is required before calling high-risk tool '%s'" % name,
                "confirmation_required", tool=name, toolset=record["category"], policy=policy,
                confirmation={"required": True, "retry_with": {"confirm": True}, "dry_run_bypass_available": policy["supports_dry_run"]})))
            return output
        args = _original_arguments(record, arguments, bindings)
        binding, count = _choose(record, bindings, args)
        if binding is None:
            output.set_value(_json(_error("Original arguments do not have one exact current logical-schema binding", "tool_validation_or_execution_error",
                tool=name, toolset=record["category"], binding_candidates=count, mutation_attempted=False,
                mutation_performed=False, mutation_state_unknown=False)))
            return output
        output.set_editor_property("legacy_tool_name", name)
        token = _CALL_DEPTH.set(_CALL_DEPTH.get() + 1)
        try:
            payload = _json(_current_arguments(record, binding, args))
            dispatched = True
            source = unreal.ToolsetRegistry.execute_tool(binding["toolset"], binding["method"], payload)
        finally:
            _CALL_DEPTH.reset(token)
        output.set_editor_property("source", source)
        source.on_completed.add_function(output, "consume_source")
        source.broadcast_on_completed_if_complete()
    except Exception as error:
        receipt = _held_source_receipt(output, source) if "source" in locals() else None
        output.set_value(_json(_error(str(error), "tool_validation_or_execution_error", tool=tool_name,
            toolset=toolset, mutation_state_unknown=dispatched, automatic_retry=False, source_receipt=receipt)))
    return output


_POLICY_SCOPE = "compatibility catalog/discovery frontdoor only; official Named tools and direct Engine API calls remain outside this policy"
_MANAGE_ACTIONS = ["status", "list", "list_tools", "health", "heartbeat", "enable_tool", "disable_tool", "set_tool", "enable_category", "disable_category", "set_category", "reset"]


def _registry_report(records, include_tools):
    records = _refresh_policy(records)
    categories = {}
    for row in records:
        summary = categories.setdefault(row["category"], dict(total=0, enabled=0, disabled=0,
            effects={}, disabled_by_category=row["category"] in _DISABLED_CATEGORIES))
        summary["total"] += 1
        summary["enabled" if row["enabled"] else "disabled"] += 1
        effect = row["policy"]["effect"]
        summary["effects"][effect] = summary["effects"].get(effect, 0) + 1
    policy_summary = {}
    for key, field in (("effects", "effect"), ("risks", "risk"),
            ("verification_levels", "verification_level"), ("result_contracts", "result_contract")):
        counts = {}
        for row in records:
            value = row["policy"][field]
            counts[value] = counts.get(value, 0) + 1
        policy_summary[key] = dict(sorted(counts.items()))
    for field in ("strict_postconditions", "confirmation_required", "supports_dry_run"):
        source_field = "strict_postcondition" if field == "strict_postconditions" else field
        policy_summary[field] = sum(bool(row["policy"][source_field]) for row in records)
    return dict(success=True, total_tools=len(records), enabled_tools=sum(bool(r["enabled"]) for r in records),
        disabled_tools=sorted(_DISABLED_TOOLS), disabled_categories=sorted(_DISABLED_CATEGORIES),
        protected_tools=sorted(r["name"] for r in records if r["protected"]), policy_summary=policy_summary,
        categories=categories, tools=copy.deepcopy(records) if include_tools else [],
        policy_scope=_POLICY_SCOPE, migration_context=_context())


def manage_tools(action="status", tool_name="", category="", enabled=None,
        include_tools=True, include_recent=True, ping_unreal=False):
    """Preserve original local policy operations; retire only old transport facts."""
    change = None
    try:
        from .local_api import _runtime
        runtime = _runtime()
        # Finish all SDK/current-schema reads before any local policy write.
        _, records, _ = _records()
        normalized = action.lower().strip()
        by_name = {r["name"]: r for r in records}
        if normalized in ("status", "list", "list_tools"):
            return _registry_report(records, include_tools)
        if normalized in ("health", "heartbeat"):
            ping = ping_unreal or normalized == "heartbeat"
            return dict(success=True, engine_version=runtime.get("engine_version"),
                editor_session_id=runtime.get("editor_session_id"), module_loaded=runtime.get("module_loaded"),
                registry=_registry_report(records, include_tools), telemetry=None, guard=None,
                retired_transport_fields={"telemetry": "Old FastMCP/TCP request counters are not current Registry health",
                    "guard": "Old external server target/rate-limit guard config is not recreated",
                    "ping_unreal": {"requested": bool(ping), "performed": False,
                        "reason": "Old TCP ping retired; current local Editor/Registry metadata is read directly"}},
                include_recent_requested=include_recent, policy_scope=_POLICY_SCOPE,
                asset_mutation=False, migration_context=_context())
        if normalized == "reset":
            before = dict(disabled_tools=sorted(_DISABLED_TOOLS), disabled_categories=sorted(_DISABLED_CATEGORIES))
            _DISABLED_TOOLS.clear()
            _DISABLED_CATEGORIES.clear()
            change = dict(action=normalized, before=before,
                after={"disabled_tools": [], "disabled_categories": []}, policy_scope=_POLICY_SCOPE)
            result = dict(success=True, message="All runtime tool policy overrides were cleared")
        elif normalized in ("enable_tool", "disable_tool", "set_tool"):
            if not tool_name:
                return _error("tool_name is required", "missing_tool_name")
            if normalized == "set_tool" and enabled is None:
                return _error("enabled is required for set_tool", "missing_enabled")
            desired = bool(enabled) if normalized == "set_tool" else normalized == "enable_tool"
            record = by_name.get(tool_name)
            if record is None:
                result = _error("Unknown tool: " + tool_name, "unknown_tool", tool=tool_name)
            elif record["protected"] and not desired:
                result = _error("Protected tool cannot be disabled: " + tool_name, "protected_tool", tool=tool_name)
            else:
                before = tool_name in _DISABLED_TOOLS
                if desired:
                    _DISABLED_TOOLS.discard(tool_name)
                else:
                    _DISABLED_TOOLS.add(tool_name)
                change = dict(action=normalized, tool=tool_name, disabled_before=before,
                    disabled_after=tool_name in _DISABLED_TOOLS, policy_scope=_POLICY_SCOPE)
                _refresh_policy(records)
                result = dict(success=True, tool=tool_name, enabled=record["enabled"])
        elif normalized in ("enable_category", "disable_category", "set_category"):
            if not category:
                return _error("category is required", "missing_category")
            if normalized == "set_category" and enabled is None:
                return _error("enabled is required for set_category", "missing_enabled")
            desired = bool(enabled) if normalized == "set_category" else normalized == "enable_category"
            if category not in {r["category"] for r in records}:
                result = _error("Unknown category: " + category, "unknown_category", category=category)
            else:
                before = category in _DISABLED_CATEGORIES
                if desired:
                    _DISABLED_CATEGORIES.discard(category)
                else:
                    _DISABLED_CATEGORIES.add(category)
                change = dict(action=normalized, category=category, disabled_before=before,
                    disabled_after=category in _DISABLED_CATEGORIES, policy_scope=_POLICY_SCOPE)
                result = dict(success=True, category=category, enabled=category not in _DISABLED_CATEGORIES)
        else:
            return _error("Unknown action: " + action, "unknown_action", supported_actions=list(_MANAGE_ACTIONS))
        result["registry"] = _registry_report(records, include_tools)
        result["policy_scope"] = _POLICY_SCOPE
        if change is not None:
            result["policy_change"] = change
        return result
    except Exception as error:
        return _error(str(error), "management_failed", policy_change=change,
            policy_scope=_POLICY_SCOPE, automatic_retry=False,
            change_state_known=change is not None, observation_scope="process-local policy state only; no Engine authoring request")
