"""Legacy local Bridge facade; no listener, remote RPC or independent dispatcher.

The existing concrete Batch adapter owns normalization, lifecycle, mutation and
durable receipts. This facade owns only the local JSON envelope and metadata.
"""

import copy
import json
import unreal


def _json(value):
    return json.dumps(value, ensure_ascii=False, sort_keys=True, allow_nan=False)


def _reject(message, *, after_dispatch=False, receipt=None):
    value = {"status": "error", "error": message,
        "error_code": "local_api_result_unknown" if after_dispatch else "local_api_invalid_request",
        "result": {"success": False, "stage": "result_adapter" if after_dispatch else "preflight",
            "change_state_known": not after_dispatch, "mutation_state_unknown": after_dispatch,
            "automatic_retry": False, "command_may_still_be_in_flight": after_dispatch}}
    if receipt is not None:
        value["cubeless_dispatch_receipt"] = receipt
    return _json(value)


def _error_text(command, result):
    data = result.get("data", {})
    for payload in (result, data):
        for key in ("error", "command_result"):
            text = payload.get(key)
            if isinstance(text, str) and text:
                return text
        for row in payload.get("logs", []) if isinstance(payload.get("logs"), list) else []:
            if isinstance(row, dict) and isinstance(row.get("output"), str) and row["output"]:
                return row["output"]
    return "Command '%s' reported failure without details" % command



def _payload(command, result):
    data = result.get("data")
    if not isinstance(data, dict):
        # Read-only metadata already is its original-style JSON object.
        return copy.deepcopy(result), "metadata_object"
    if command == "batch" and isinstance(data.get("batch"), dict):
        summary = data["batch"]
        payload = {"success": result.get("success") is True, "results": []}
        for key in ("total", "executed", "succeeded", "failed", "stopped_at_index", "stop_on_error"):
            if key in summary:
                payload[key] = copy.deepcopy(summary[key])
        for row in summary.get("results", []):
            if not isinstance(row, dict) or not isinstance(row.get("response"), dict):
                raise ValueError("Batch row has no concrete child receipt; original payload projection is unavailable")
            child = row["response"]
            name = row.get("legacy_original_command", row.get("type", ""))
            response = json.loads(_envelope(name, child))
            item = {"index": row["index"], "type": name,
                "status": response["status"], "response": response}
            if response["status"] != "success":
                item["error"] = response["error"]
            payload["results"].append(item)
        if result.get("success") is not True:
            payload["error"] = _error_text(command, result)
        return payload, "existing_batch_summary_and_original_child_envelopes"
    for key in ("legacy_result", "legacy_Named_result"):
        if isinstance(data.get(key), dict):
            return copy.deepcopy(data[key]), "existing_data." + key
    # Existing concrete adapters retain fields such as actors directly in data.
    # Do not mistake native_outcome (work counters/guard receipts) for a handler.
    payload = {key: copy.deepcopy(value) for key, value in data.items() if key != "native_outcome"}
    if result.get("success") is not True:
        payload.setdefault("success", False)
        payload.setdefault("error", _error_text(command, result))
        if result.get("error_code"):
            payload.setdefault("error_code", result["error_code"])
    return payload, "existing_concrete_data_fields_full_handler_equivalence_unproven"


def _envelope(command, result, parent=None):
    # Keep source payload consumption independent from current receipt authority.
    # A verifier/parent failure must not be promoted by a native success=True.
    success = result.get("success") is True and (parent is None or parent.get("success") is True)
    payload, basis = _payload(command, result)
    out = {"status": "success" if success else "error", "result": payload,
        "cubeless_payload_projection": {"basis": basis, "all_original_handlers_equivalent": False}}
    if isinstance(result.get("data"), dict):
        out["cubeless_child_receipt"] = copy.deepcopy(result)
    if parent is not None:
        out["cubeless_dispatch_receipt"] = copy.deepcopy(parent)
    if not success:
        authority = parent if parent is not None and parent.get("success") is not True else result
        out["error"] = _error_text(command, authority)
        for key in ("error_code", "required_plugins", "disabled_plugins"):
            for source in (authority, authority.get("data", {}), result, result.get("data", {}), payload):
                if key in source:
                    out[key] = copy.deepcopy(source[key])
                    break
    return _json(out)



def _record(source):
    if not source.get_editor_property("is_complete"):
        raise RuntimeError("Batch future is not terminal; no second dispatch is permitted")
    if source.get_editor_property("error"):
        raise RuntimeError("Batch future infrastructure failure: " + source.get_editor_property("error"))
    record = json.loads(source.get_value_as_json_string())
    if not isinstance(record, dict) or type(record.get("success")) is not bool:
        raise TypeError("Batch future did not return its standard11 object")
    for key in ("changes", "compile", "save", "validation", "retry", "data"):
        # SDK FJsonObjectConverter retains the exact Python reflected property name.
        text = record.pop(key + "_json")
        record[key] = json.loads(text)
    if not isinstance(record["data"], dict):
        raise TypeError("Batch data is not an object")
    return record



def _source_receipt(source):
    # Observation only: never wait, dispatch or cancel to repair an adapter error.
    receipt = {"request_retried": False, "request_cancelled": False}
    try:
        complete = source.get_editor_property("is_complete")
        receipt.update(source_is_complete=complete, pending=not complete)
        if complete:
            receipt["terminal_source_error"] = source.get_editor_property("error")
            receipt["terminal_source_value_json"] = source.get_value_as_json_string()
            try:
                receipt["batch_receipt"] = _record(source)
            except Exception as error:
                receipt["terminal_decode_error"] = str(error)
        else:
            receipt["scope"] = "source is still pending; client/adapter failure is not Engine cancellation"
    except Exception as error:
        receipt.update(source_observation="unavailable", source_observation_error=str(error))
    return receipt


def complete_command(output, source):
    """One terminal conversion. Retain child and parent unknown/save evidence."""
    if output.get_editor_property("is_complete"):
        return
    try:
        if source != output.get_editor_property("source"):
            raise RuntimeError("Completion source identity changed")
        record = _record(source)
        command = output.get_editor_property("command_type")
        if command == "batch":
            # A real legacy batch is one existing Batch call, not a nested item.
            text = _envelope(command, record)
        else:
            rows = record["data"].get("batch", {}).get("results", [])
            if len(rows) == 1 and isinstance(rows[0].get("response"), dict):
                text = _envelope(command, rows[0]["response"], record)
            else:
                text = _envelope(command, record)
        source.on_completed.remove_function(output, "consume_source")
        output.set_value(text)
    except Exception as error:
        # No retry/cancel and no unchanged claim after dispatch/format failure.
        receipt = locals().get("record")
        if receipt is None:
            receipt = _source_receipt(source)
        output.set_value(_reject(str(error), after_dispatch=True, receipt=receipt))


def _runtime():
    value = json.loads(unreal.CubelessLocalApiLibrary.read_local_api_state())
    if value.get("success") is not True:
        raise RuntimeError(value.get("error", "Local API requires the live Editor game thread"))
    return value


def execute_command_json(command_type, params_json=""):
    """GT-only local entry; direct Python returns the SDK string future.

    Official Toolset waits for the future and returns its final JSON string.
    Exactly one existing Batch dispatch owns the work, including async waits.
    """
    from ..registration.local_api_result import CubelessLocalCommandAsyncResult
    _runtime()  # Reject foreign threads before allocating any UObject receiver.
    output = CubelessLocalCommandAsyncResult()
    dispatched = False
    try:
        if not isinstance(command_type, str) or not command_type.strip():
            output.set_value(_reject("CommandType must not be empty"))
            return output
        command = command_type.strip()
        if not isinstance(params_json, str):
            raise ValueError("ParamsJson must be a valid JSON object")
        try:
            params = json.loads(params_json.strip() or "{}", parse_constant=lambda _: (_ for _ in ()).throw(ValueError("Nonfinite JSON")))
        except (ValueError, TypeError):
            raise ValueError("ParamsJson must be a valid JSON object") from None
        if not isinstance(params, dict):
            raise ValueError("ParamsJson must be a valid JSON object")
        params.pop("__mcp_ticker_dispatch_effective", None)
        _runtime()  # Check context before any domain dispatch; no off-thread wait.
        if command == "get_mcp_capabilities":
            output.set_value(_envelope(command, capabilities()))
            return output
        if command == "batch":
            commands = params.get("commands")
            if not isinstance(commands, list):
                raise ValueError("Batch requires a 'commands' array of {type, params} objects")
            if not 1 <= len(commands) <= 100:
                raise ValueError("Batch 'commands' must contain 1..100 entries, got %d" % len(commands))
            stop = params.get("stop_on_error") if type(params.get("stop_on_error")) is bool else True
            # Native ignored non-object params and stripped the private ticker key.
            commands = copy.deepcopy(commands)
            for row in commands:
                if isinstance(row, dict):
                    row_params = row.get("params")
                    row["params"] = row_params if isinstance(row_params, dict) else {}
                    row["params"].pop("__mcp_ticker_dispatch_effective", None)
        else:
            commands, stop = [{"type": command, "params": params}], True
        from .batch import execute_batch_mcp
        # Allocate/configure receiver before the only dispatch; it holds the
        # child future through a reflected UObject property, not a global map.
        output.set_editor_property("command_type", command)
        command_payload = _json(commands)
        dispatched = True
        source = execute_batch_mcp(command_payload, stop)
        output.set_editor_property("source", source)
        source.on_completed.add_function(output, "consume_source")
        source.broadcast_on_completed_if_complete()
    except Exception as error:
        # A returned/raised dispatch may already have changed Engine state.
        receipt = None
        if "source" in locals():
            receipt = _source_receipt(source)
            try:
                receipt["source_retained_by_receiver"] = output.get_editor_property("source") == source
            except Exception as observation_error:
                receipt["source_hold_observation_error"] = str(observation_error)
        output.set_value(_reject(str(error), after_dispatch=dispatched, receipt=receipt))
    return output


_OPTIONAL_ROUTES = (
    ("gas_inspect_blueprint", "gas", "CubelessGASAuthoringTools.inspect_gas_blueprint", ("GameplayAbilities",), False),
    ("gas_create_gameplay_ability_blueprint", "gas", "CubelessGASAuthoringTools.create_gameplay_ability_blueprint", ("GameplayAbilities",), False),
    ("gas_create_gameplay_effect_blueprint", "gas", "CubelessGASAuthoringTools.create_gameplay_effect_blueprint", ("GameplayAbilities",), False),
    ("gas_create_attribute_set_blueprint", "gas", "CubelessGASAuthoringTools.create_attribute_set_blueprint", ("GameplayAbilities",), False),
    ("gas_add_ability_system_component_to_blueprint", "gas", "CubelessGASAuthoringTools.add_ability_system_component_to_blueprint", ("GameplayAbilities",), False),
    ("gas_add_gameplay_attribute_to_attribute_set", "gas", "CubelessGASAuthoringTools.add_gameplay_attribute_to_attribute_set", ("GameplayAbilities",), False),
    ("state_tree_list_assets", "state_tree", "CubelessStateTreeTools.list_state_trees", ("StateTree", "GameplayStateTree"), True),
    ("state_tree_inspect_asset", "state_tree", "CubelessStateTreeTools.inspect_state_tree", ("StateTree", "GameplayStateTree"), True),
    ("state_tree_create_asset", "state_tree", "CubelessStateTreeTools.create_state_tree", ("StateTree", "GameplayStateTree"), True),
    ("state_tree_add_state", "state_tree", "CubelessStateTreeTools.add_state_tree_state", ("StateTree", "GameplayStateTree"), True),
    ("state_tree_compile_asset", "state_tree", "CubelessStateTreeTools.compile_state_tree", ("StateTree", "GameplayStateTree"), True),
    ("audio_create_basic_metasound_source", "metasound", "CubelessAudioTools.create_basic_metasound_source", ("Metasound",), True),
)


def capabilities():
    runtime = _runtime()
    from . import batch
    routes = []
    for command, domain, target, required, legacy_provider in sorted(_OPTIONAL_ROUTES):
        required = list(required) + ["CubelessMCP_UE58"]
        disabled = [p for p in required if runtime["plugins"].get(p) is not True]
        binding = batch._CAPABILITIES.get(target)
        provider = runtime["module_loaded"] and isinstance(binding, dict) and callable(binding.get("action"))
        available = not disabled and provider
        row = {"command": command, "domain": domain, "available": available,
            "provider_registered": bool(provider), "required_plugins": required,
            "disabled_plugins": disabled, "current_tool": target,
            "provider_basis": "existing_local_batch_action_and_loaded_editor_module",
            "legacy_requires_provider": legacy_provider, "behavior_verified_by_metadata": False}
        if not available:
            row["reason"] = "dependency_plugin_disabled" if disabled else "provider_not_registered"
        routes.append(row)
    return {"success": True, "schema_version": 1, "editor_session_id": runtime["editor_session_id"],
        "engine_version": runtime["engine_version"], "engine_target": "5.8.3", "routes": routes,
        "provider_model": "Cubeless local adapters replace UE56 optional provider modules; no new registry",
        "official_toolset_registration_proven": False, "asset_mutation": False}


def get_mcp_capabilities():
    try:
        return _json(capabilities())
    except Exception as error:
        return _json({"success": False, "error": str(error), "asset_mutation": False})


def get_python_api_info_json():
    try:
        runtime = _runtime()
        return _json({"schema_version": 1, "api": "CubelessMCP_UE58 local Editor API",
            "engine_version": runtime["engine_version"], "editor_only": True,
            "server_running": None, "server_port": None,
            "legacy_tcp_state": "removed; these fields do not describe the official HTTP server",
            "command_entry_point": "CubelessPythonDiagnostics.execute_command_json(command_type, params_json)",
            "parameter_contract": "params_json must be one JSON object; empty string means {}",
            "result_contract": "Official MCP waits for JSON string status/result/error; result projects existing legacy_result or concrete data fields and original-shaped Batch rows; full child and parent standard11 receipts remain separate",
            "direct_python_result": "ToolCallAsyncResultString future; inspect completion, not an off-GT blocking string",
            "dispatch_dependency": "existing local Batch action mapping and Engine APIs; no server RPC",
            "compatibility_differences": ["Current domain admission/support limits remain", "Existing legacy payload/concrete data projection restores basic original field paths; complete original handler payload equivalence remains unproven", "Single calls reuse existing Batch transaction/journal semantics", "Batch prevalidates its bounded input before dispatch; UE56 malformed rows could fail after earlier rows ran", "No off-GT blocking dispatcher or legacy TCP state"],
            "dependency_scope": "Editor authoring only; no generated-game Cubeless/MCP runtime references",
            "recommended_ue_5_8_routes": ["create_geometry_collection_from_mesh", "inspect_geometry_collection_performance_guarded", "inspect_geometry_collection_runtime_state", "inject_pie_input_key", "configure_building_fracture", "configure_anchor_field", "trigger_trace_external_strain", "record_chaos_cache", "validate_chaos_cache", "create_chaos_level_sequence", "bind_chaos_cache_to_sequence", "configure_chaos_sequence_timing", "inspect_chaos_cinematic_pipeline", "inspect_chaos_solver_settings", "configure_chaos_solver_settings", "inspect_chaos_cloth_asset", "inspect_chaos_cloth_simulation_mesh", "inspect_chaos_cloth_component", "configure_chaos_cloth_component", "configure_chaos_cloth_dataflow_node", "configure_chaos_cloth_weight_map", "get_actor_properties", "set_actor_property", "get_component_property", "set_component_property", "batch"],
            "recommended_routes_basis": "retained UE56 local route identities; individual support and evidence remain per API",
            "module_loaded": runtime["module_loaded"], "asset_mutation": False})
    except Exception as error:
        return _json({"success": False, "error": str(error), "asset_mutation": False})
