"""JV original Python recipe/AnimBP composite using direct existing leaf actions.

One parent journal owns all stage receipts. There is no nested journal, server
RPC, external executor, implicit cleanup, final save or automatic replay.
"""
import copy
import json
from . import state
from .state import OperationError

METHOD = "build_controlrig_recipe_with_animbp_binding"
_ACTIVE_REQUEST = False


def binding_status():
    from . import controlrig_recipe_authoring, controlrig_builders, anim_graph_controlrig, blueprint
    recipe = controlrig_recipe_authoring.api.binding_status()
    builder = controlrig_builders.api.binding_status()
    defaults = anim_graph_controlrig.api.binding_status()
    return (recipe.get("validate_controlrig_recipe") is True
            and recipe.get("build_controlrig_recipe") is True
            and builder.get("ensure_anim_graph_controlrig_node_sample") is True
            and defaults.get("set_anim_graph_controlrig_input_defaults") is True
            and callable(blueprint.compile_blueprint_action))


def _object(value, field):
    from . import controlrig_recipe_authoring
    parsed = json.loads(value) if isinstance(value, str) else value
    if not isinstance(parsed, dict):
        raise ValueError(field + " must be a JSON object")
    controlrig_recipe_authoring.reject_mcp_dependency_references(field, parsed)
    return parsed


def _sample_path(path):
    return path.startswith("/Game/_MCP_Sample/") or path.startswith("/Game/_MCP_Temp/")


def _effect(entry):
    child = entry["receipt"]
    data = child["data"]
    changes = child["changes"]
    leaf = data.get("legacy_result", {})
    sources = [data, leaf, data.get("native_outcome", {}), data.get("native_scalar_result", {})]
    flags = ("mutation_performed", "observed_changed", "observed_state_changed", "changed",
             "target_modified", "partial_target_modified", "target_created", "graph_changed",
             "defaults_changed", "visibility_changed", "links_changed")
    changed = any(source.get(key) is True for source in sources if isinstance(source, dict) for key in flags)
    changed |= any(row.get("mutation_observed") is True or row.get("changed") is True for row in changes)
    attempted = any(source.get("native_mutation_attempted") is True or source.get("mutation_attempted") is True
                    or source.get("execution_mutation_attempted") is True for source in sources if isinstance(source, dict))
    attempted |= any(row.get("attempted") is True for row in changes)
    known = not (data.get("mutation_state_unknown") is True or data.get("change_state_known") is False
                 or any(row.get("change_state_known") is False for row in changes))
    if entry["effect"] == "mutation" and not entry["returned"]:
        if (data.get("native_dispatched") is True or data.get("native_dispatch_pending") is True
                or child["compile"].get("attempted") is True):
            known = False
    if entry["effect"] == "mutation" and child["compile"].get("attempted") is True and child["success"] is not True:
        # The older compile action has no scalar change-state flag. A dispatch
        # without a decoded compiler result or its failed final readback is unknown.
        if child["compile"].get("compiled") is None or child["stage"] == "readback":
            known = False
    if entry["effect"] == "read" and not changed and not attempted:
        # A failed validator may have an unknown read outcome, not an attempted target writer.
        known = True
    return dict(change_state_known=known, mutation_attempted=attempted, changed=changed,
                mutation_state_unknown=not known)


def _tri(values):
    return True if any(value is True for value in values) else None if any(value is None for value in values) else False


def _aggregate(result):
    entries = result["data"]["stage_receipts"]
    effects = [_effect(entry) for entry in entries]
    known = all(effect["change_state_known"] for effect in effects)
    changed = any(effect["changed"] for effect in effects)
    attempted = any(effect["mutation_attempted"] for effect in effects)
    result["changes"] = [dict(copy.deepcopy(change), composite_stage=entry["name"])
                         for entry in entries for change in entry["receipt"]["changes"]]
    if (changed or attempted or not known) and not result["changes"]:
        result["changes"] = [dict(kind=METHOD, attempted=attempted, changed=changed if known else None,
                                  change_state_known=known)]
    data = result["data"]
    data.update(change_state_known=known, mutation_state_unknown=not known,
                native_mutation_attempted=attempted, execution_mutation_attempted=attempted,
                mutation_performed=changed if known else None, observed_changed=changed,
                completed_mutation_stages=[entry["name"] for entry, effect in zip(entries, effects)
                                           if entry["returned"] and entry["receipt"]["success"] is True and effect["changed"]])
    compiles = [entry["receipt"]["compile"] for entry in entries]
    saves = [entry["receipt"]["save"] for entry in entries]
    actual_compiles = [item for item in compiles if item.get("attempted") is True]
    result["compile"].update(requested=any(item.get("requested") is True for item in compiles),
        attempted=_tri([item.get("attempted", False) for item in compiles]),
        compiled=(all(item.get("compiled") is True for item in actual_compiles)
                  if actual_compiles and all(type(item.get("compiled")) is bool for item in actual_compiles) else None),
        status="per_stage_actual_history_not_final_asset_inference")
    # A later failure/unsaved defaults stage must never erase an earlier real SavedTrue.
    result["save"].update(requested=any(item.get("requested") is True for item in saves),
        attempted=_tri([item.get("attempted", False) for item in saves]),
        saved=any(item.get("saved") is True for item in saves),
        status="per_stage_saved_prefix_not_final_defaults_persistence")
    data["actual_compile_by_stage"] = [{"stage": entry["name"], **copy.deepcopy(entry["receipt"]["compile"])} for entry in entries]
    data["actual_save_by_stage"] = [{"stage": entry["name"], **copy.deepcopy(entry["receipt"]["save"])} for entry in entries]
    result["retry"].update(automatic_retry=False, atomic_rollback=False,
        duplicate_risk="unknown_leaf_outcome_no_replay" if not known else "inspect_partial_targets_before_new_operation" if changed or attempted else "none_observed")


def _leaf(result, name, action, arguments, callback, *, read=False):
    child = state.new_result(result["operation_id"])
    child["data"]["parent_journal_only"] = True
    entry = dict(name=name, effect="read" if read else "mutation", entered=True, returned=False, receipt=child)
    result["data"]["stage_receipts"].append(entry)
    checkpoint_failed = False

    def checkpoint(_):
        nonlocal checkpoint_failed
        _aggregate(result)
        result["stage"] = "composite_" + name + ":" + child["stage"]
        if callback:
            try:
                callback(result)
            except Exception as error:
                checkpoint_failed = True
                error.composite_checkpoint_failed = True
                error.composite_checkpoint_stage = name
                raise

    checkpoint(child)
    try:
        returned = action(child, **arguments, checkpoint_callback=checkpoint)
        # Existing actions either update child in place or return that same result.
        if returned is not None and returned is not child:
            raise OperationError("controlrig_binding_leaf_result_identity_changed", "Direct leaf replaced its owned stage receipt")
    except Exception as error:
        state.fail(child, error)
        if checkpoint_failed:
            _aggregate(result)
            raise
    entry["returned"] = True
    _aggregate(result)
    checkpoint(child)
    return child


def _original(child):
    leaf = child["data"].get("legacy_result")
    if type(leaf) is not dict or type(leaf.get("success")) is not bool:
        raise OperationError("controlrig_binding_leaf_report_missing", "Original leaf semantic report is unavailable; no next stage")
    return copy.deepcopy(leaf)


def _response(child, *, compile_leaf=False):
    # Preserve the legacy command response representation from direct Native data.
    body = copy.deepcopy(child["data"]) if compile_leaf else _original(child)
    effect = _effect(dict(receipt=child, returned=True, effect="mutation"))
    return dict(status="success" if child["success"] else "error", success=child["success"], result=body,
                mutation_performed=effect["changed"] if effect["change_state_known"] else None,
                mutation_state_unknown=not effect["change_state_known"], error=child.get("error", ""),
                error_code=child.get("error_code", ""))


def _failure(result, original, child, stage, message, *, read=False, response=None):
    effect = _effect(dict(receipt=child, returned=True, effect="read" if read else "mutation"))
    completed = list(result["data"]["completed_mutation_stages"])
    original.update(message=message, failed_stage=stage, failed_stage_effect="read" if read else "mutation")
    if not any(item["name"] == stage for item in original["stages"]):
        original["stages"].append(dict(name=stage, success=False, message=message))
    original["read_unreal_response" if read else "unreal_response"] = copy.deepcopy(response if response is not None else child)
    if read and not completed:
        original.update(mutation_attempted=False, mutation_performed=False, changed=False)
    if completed or effect["changed"]:
        original.update(partial_mutation=True, mutation_performed=True, do_not_retry=True)
        if completed:
            original.update(completed_mutation_count=len(completed), completed_mutation_stages=completed)
        if effect["changed"]:
            original["failed_stage_mutation_performed"] = True
    if not effect["change_state_known"]:
        original.update(mutation_state_unknown=True, do_not_retry=True)
    result["data"]["legacy_result"] = original
    result["data"]["operation_complete"] = True
    result["stage"] = "failed_" + stage
    return state.fail(result, OperationError(child.get("error_code") or "controlrig_recipe_binding_failed",
                                            child.get("error") or message))


def _run(result, values, original_arguments, legacy_arguments, public_input, callback):
    global _ACTIVE_REQUEST
    if _ACTIVE_REQUEST:
        raise OperationError("controlrig_recipe_binding_reentry", "Composite reentry rejected before work")
    _ACTIVE_REQUEST = True
    report = dict(success=False, command=METHOD, adapter="python_reflection_composite", sample_only=True,
                  stages=[], target_control_rig_path="")
    result["data"].update(public_method=METHOD, requested_arguments=copy.deepcopy(original_arguments),
        lower_name_Batch_arguments=copy.deepcopy(legacy_arguments), public_argument_validation_applied=public_input,
        journal_created=result["data"].get("parent_journal_only") is not True, composite_parent_journal_only=True,
        stage_receipts=[], legacy_result=report, operation_complete=False, automatic_action_retry=False,
        actual_backend_route="direct_existing_plugin_leaf_actions", server_RPC_dependency=False,
        registration_dependency="ToolsetAPI", operation_dependency="direct_public_Engine_API_via_existing_leafs",
        options_dry_run_does_not_reclassify_composite=True, final_save_added=False,
        final_defaults_saved_or_runtime_cold_verified=False)
    try:
        from . import controlrig_recipe_authoring as recipe_api, controlrig_builders, anim_graph_controlrig, blueprint
        recipe = _object(values["recipe"], "recipe")
        options = _object(values["options"], "options") if values.get("options") is not None else None
        options = options or {}
        binding = recipe.get("animbp_binding")
        verification = recipe.get("verification") if isinstance(recipe.get("verification"), dict) else {}
        report["target_control_rig_path"] = recipe.get("target_control_rig_path", "")
        message = None
        if not isinstance(binding, dict) or not binding.get("enabled"):
            message = "animbp_binding.enabled=true is required for binding orchestration."
        else:
            source_path = str(binding.get("source_anim_bp_path") or "")
            target_path = str(binding.get("target_anim_bp_path") or "")
            rig_path = str(recipe.get("target_control_rig_path") or "")
            report.update(source_anim_bp_path=source_path, target_anim_bp_path=target_path, target_control_rig_path=rig_path)
            if not _sample_path(rig_path):
                message = "target_control_rig_path must be under /Game/_MCP_Sample or /Game/_MCP_Temp."
            elif not _sample_path(target_path):
                message = "animbp_binding.target_anim_bp_path must be under /Game/_MCP_Sample or /Game/_MCP_Temp."
        if message:
            report.update(message=message, mutation_attempted=False, mutation_performed=False, changed=False)
            report["stages"].append(dict(name="local_contract", success=False, message=message))
            _aggregate(result)
            result["data"]["operation_complete"] = True
            return state.fail(result, OperationError("controlrig_recipe_binding_local_contract", message))
        child = _leaf(result, "validate_recipe", recipe_api.validate_controlrig_recipe_action, dict(recipe=recipe), callback, read=True)
        leaf = _original(child)
        report["validation_report"] = leaf
        report["stages"].append(dict(name="validate_recipe", success=child["success"] is True and bool(leaf.get("success"))))
        if not report["stages"][-1]["success"]:
            return _failure(result, report, child, "validate_recipe", "Recipe validation failed before orchestration.", read=True, response=leaf)
        rig_recipe = copy.deepcopy(recipe)
        rig_recipe.setdefault("animbp_binding", {})["enabled"] = False
        child = _leaf(result, "build_controlrig_without_binding", recipe_api.build_controlrig_recipe_action,
                      dict(recipe=rig_recipe, options=options if options else None), callback)
        leaf = _original(child)
        report["controlrig_build_report"] = leaf
        report["stages"].append(dict(name="build_controlrig_without_binding", success=child["success"] is True and bool(leaf.get("success"))))
        if not report["stages"][-1]["success"]:
            return _failure(result, report, child, "build_controlrig_without_binding", "ControlRig build failed; AnimBP binding was not attempted.", response=leaf)
        save_requested = bool(verification.get("save")) and bool(options.get("allow_save"))
        node_args = dict(source_anim_bp_path=source_path, target_anim_bp_path=target_path,
            control_rig_asset_path=leaf.get("target_control_rig_path") or rig_path,
            allow_existing_target_reuse=bool(binding.get("allow_existing_target_reuse", True)),
            overwrite_existing=bool(binding.get("overwrite_existing", False)),
            replace_existing_links=bool(binding.get("replace_existing_links", True)),
            compile=bool(verification.get("compile", True)), save_asset=save_requested)
        child = _leaf(result, "ensure_anim_graph_controlrig_node_sample", controlrig_builders.ensure_anim_graph_controlrig_node_sample_action, node_args, callback)
        response = _response(child)
        report["animbp_node_response"] = response
        report["stages"].append(dict(name="ensure_anim_graph_controlrig_node_sample", success=child["success"] is True))
        if not child["success"]:
            return _failure(result, report, child, "ensure_anim_graph_controlrig_node_sample", "ControlRig node sample creation failed; input defaults were not attempted.", response=response)
        defaults = binding.get("input_defaults") if isinstance(binding.get("input_defaults"), dict) else {}
        raw_expose = binding.get("expose_input_pins") if isinstance(binding.get("expose_input_pins"), list) else []
        expose = [str(pin).strip() for pin in raw_expose if isinstance(pin, str) and str(pin).strip()]
        defaults_response = None
        if defaults or expose:
            node_result = response["result"]
            args = dict(blueprint_name=target_path, graph_name="AnimGraph", graph_type="function",
                        disconnect_existing_links=bool(binding.get("disconnect_existing_input_links", False)),
                        allow_non_sample=target_path.startswith("/Game/_MCP_Temp/"))
            if defaults:
                args["input_defaults"] = defaults
            if expose:
                args["expose_input_pins"] = expose
            if node_result.get("created_node_id"):
                args["node_id"] = node_result["created_node_id"]
            if node_result.get("control_rig_class_path"):
                args["control_rig_class"] = node_result["control_rig_class_path"]
            child = _leaf(result, "set_anim_graph_controlrig_input_defaults", anim_graph_controlrig.set_anim_graph_controlrig_input_defaults_action, args, callback)
            defaults_response = _response(child)
            report["input_defaults_response"] = defaults_response
            report["stages"].append(dict(name="set_anim_graph_controlrig_input_defaults", success=child["success"] is True))
            if not child["success"]:
                return _failure(result, report, child, "set_anim_graph_controlrig_input_defaults", "AnimBP ControlRig node was created, but input pin exposure/default application failed.", response=defaults_response)
            if verification.get("compile", True):
                child = _leaf(result, "compile_bound_anim_bp", blueprint.compile_blueprint_action,
                              dict(blueprint_name=target_path, save=False, refresh_nodes=False), callback)
                response = _response(child, compile_leaf=True)
                report["animbp_compile_response"] = response
                report["stages"].append(dict(name="compile_bound_anim_bp", success=child["success"] is True))
                if not child["success"]:
                    return _failure(result, report, child, "compile_bound_anim_bp", "AnimBP input pins/defaults were applied, but post-change compile failed.", response=response)
        else:
            report["stages"].append(dict(name="set_anim_graph_controlrig_input_defaults", success=True, skipped=True,
                message="No animbp_binding.input_defaults or expose_input_pins were provided; input command was skipped."))
        report.update(success=True, message="ControlRig recipe and sample AnimBP binding orchestration completed.",
                      save_requested=save_requested, input_defaults_applied=bool(defaults and defaults_response),
                      input_pins_exposed=bool((defaults or expose) and defaults_response))
        _aggregate(result)
        if not result["data"]["change_state_known"]:
            raise OperationError("controlrig_recipe_binding_outcome_unknown", "Composite has an unknown leaf outcome; no completion or replay")
        result["data"]["operation_complete"] = True
        result["validation"].update(status="original_composite_order_and_direct_leaf_postconditions",
            disk_reload="not_run", runtime="not_run", unique_external_pipeline="not_run",
            final_defaults_persistence="not_inferred_from_earlier_sample_save")
        result.update(success=True, stage="complete")
        if callback:
            try:
                callback(result)
            except Exception as error:
                error.composite_checkpoint_failed = True
                error.composite_checkpoint_stage = "complete"
                raise
        return result
    except Exception as error:
        _aggregate(result)
        report.update(success=False, message=str(error))
        entries = result["data"]["stage_receipts"]
        if getattr(error, "composite_checkpoint_failed", False):
            # A journal failure is not a fabricated Engine-stage failure. Keep
            # its actual Native/compile/SavedTrue receipts and stop all work.
            report.update(failed_stage="parent_checkpoint", failed_stage_effect="journal",
                          checkpoint_source_stage=getattr(error, "composite_checkpoint_stage", result["stage"]),
                          do_not_retry=True)
            report["stages"].append(dict(name="parent_checkpoint", success=False, message=str(error)))
            completed = list(result["data"]["completed_mutation_stages"])
            if completed or result["data"]["observed_changed"]:
                report.update(partial_mutation=True, mutation_performed=True)
                if completed:
                    report.update(completed_mutation_count=len(completed), completed_mutation_stages=completed)
            if result["data"]["mutation_state_unknown"]:
                report["mutation_state_unknown"] = True
            result["stage"] = "composite_parent_checkpoint_failed"
        elif entries and "failed_stage" not in report:
            _failure(result, report, entries[-1]["receipt"], entries[-1]["name"], str(error), read=entries[-1]["effect"] == "read")
        state.fail(result, error)
        result["data"]["operation_complete"] = False
        return result
    finally:
        _ACTIVE_REQUEST = False
