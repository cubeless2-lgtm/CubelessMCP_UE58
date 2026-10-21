"""Named legacy temporary Blueprint duplication, with one journal/shared batch action."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import blueprint_duplicates as api
from . import blueprint, state
from .state import OperationError


def _source_comparison_state(observation):
    value = copy.deepcopy(observation)
    if "generated_parent" in value:
        value["generated_parent"] = blueprint.parent_comparison_state(value["generated_parent"])
    return value


def duplicate_blueprint_to_temp(blueprint_name, target_path, required_root="/Game/_MCP_Temp", dry_run=True,
        overwrite_existing=False, allow_dirty_source=False, compile=False, save=True, operation_id=None):
    arguments = dict(blueprint_name=blueprint_name, target_path=target_path, required_root=required_root,
        dry_run=dry_run, overwrite_existing=overwrite_existing, allow_dirty_source=allow_dirty_source, compile=compile, save=save)
    return state.mutate("duplicate_blueprint_to_temp", arguments, operation_id,
        lambda result: duplicate_blueprint_to_temp_action(result, **arguments))


def duplicate_blueprint_to_temp_action(result, blueprint_name, target_path, required_root="/Game/_MCP_Temp",
        dry_run=True, overwrite_existing=False, allow_dirty_source=False, compile=False, save=True, checkpoint_callback=None, _replacement=None):
    for key, value in dict(dry_run=dry_run, overwrite_existing=overwrite_existing,
            allow_dirty_source=allow_dirty_source, compile=compile, save=save).items():
        if type(value) is not bool:
            raise OperationError("duplicate_option_invalid", key + " must be an actual boolean")
    result["compile"]["requested"] = compile
    result["save"]["requested"] = save
    implicit = {"attempted": False, "duplicate_call_attempted": False, "duplicate_returned": False,
                "status": "duplicate_not_called", "engine_status": None, "diagnostics_captured": False}
    result["compile"].update(scope="explicit compile only; engine PostDuplicate compiler work is separate", implicit_postduplicate_compile=implicit)
    api.blueprint.ensure_editor_idle()
    source_path = api.package_path(blueprint_name, "blueprint_name", simple_name=True)
    target = api.package_path(target_path, "target_path")
    root = api.package_path(required_root or "/Game/_MCP_Temp", "required_root")
    if not target.casefold().startswith(root.casefold() + "/"):
        raise OperationError("duplicate_target_outside_root", "Target must be an asset below required_root, not the root itself")
    if source_path.casefold() == target.casefold():
        raise OperationError("duplicate_same_asset", "Source and target must be different assets")
    if overwrite_existing:
        return _replacement_action(result, source_path, target, root, dry_run, allow_dirty_source, compile, save, checkpoint_callback)
    before_target = api.require_target_absent(target, _replacement) if _replacement is not None else api.require_target_absent(target)
    policy = api.policy()
    if _replacement is not None:
        api.replacement_current(_replacement, target=False)
    source = api.blueprint.resolve_blueprint(source_path)
    if _replacement is not None:
        api.replacement_current(_replacement, target=False)
        if source != _replacement["source_role"]["object"]:
            raise OperationError("duplicate_replacement_source_changed", "Source resolver returned a different object; no new baseline")
    before = api.capture(source)
    if _replacement is not None:
        api.replacement_current(_replacement, target=False)
    api.require_source(source, before, allow_dirty_source)
    if _replacement is not None:
        api.replacement_current(_replacement, target=False)
        if _source_comparison_state(before) != _source_comparison_state(_replacement["initial_source"]):
            result["data"]["mutation_state_unknown"] = True
            raise OperationError("duplicate_replacement_source_changed", "Original source changed during fresh duplicate preflight; no new source baseline or duplicate")
    source_audit = api.dependency_audit(source_path)
    if _replacement is not None:
        api.replacement_current(_replacement, target=False)
    data = result["data"]
    data.update(dry_run=dry_run, source_blueprint=before["blueprint"]["asset_path"], source_package=source_path,
        target_package=target, required_root=root, target_exists_before=False, overwrite_existing=False,
        deleted_existing_target=False, created=False, compile_requested=compile, source_before=before,
        target_before=before_target, policy=policy, source_dependency_audit=source_audit,
        write_scope="dry_run_no_asset_mutation" if dry_run else "new_blueprint_under_required_root",
        full_legacy_parity=False, mutation_performed=False, mutation_state_unknown=False,
        source_lineage_scope="result/journal only; no asset metadata added",
        observation_scope="exact source file/loaded snapshots; clone class/guarded runtime parent including saved Actor parent-chain source hashes, K2 topology, variable descriptors/current CDO; component template properties, comment content, arbitrary callbacks and live BlueprintGuid are not fully audited",
        editor_selection_effect="engine duplication may deselect the source; selection is not restored or verified")
    if _replacement is not None:
        # Durable fresh-action checkpoints must retain the earlier REAL deletion,
        # rather than temporarily publishing the fresh-only no-mutation defaults.
        earlier = data["replacement"]
        data.update(overwrite_existing=True, target_exists_before=_replacement["exists"],
            deleted_existing_target=earlier["deleted"], target_before=copy.deepcopy(earlier["target_before"]),
            source_before=copy.deepcopy(_replacement["initial_source"]),
            mutation_performed=earlier["deleted"] is True,
            write_scope="temp_blueprint_delete_then_duplicate")

    def verify_source(after_source=None):
        if _replacement is not None:
            api.replacement_current(_replacement, target=False)
        after_source = api.capture(source) if after_source is None else after_source
        if _replacement is not None:
            api.replacement_current(_replacement, target=False)
        data["source_after"] = after_source
        if _source_comparison_state(after_source) != _source_comparison_state(before):
            data["mutation_state_unknown"] = True
            raise OperationError("duplicate_source_changed", "Source file or observed loaded state changed; no automatic rollback or retry")
        result["validation"]["source_preserved"] = "passed"

    if dry_run:
        verify_source()
        result["success"] = True
        result["stage"] = "complete"
        result["validation"]["status"] = "dry_run_preflight_passed"
        return

    persist = checkpoint_callback or state.checkpoint
    change = {"kind": "duplicate_blueprint", "asset_path": target, "attempted": False, "created": False}

    def before_dispatch():
        # Re-observe source after policy/asset preflight, immediately before the
        # engine call. A second policy rejection leaves this request unmodified.
        if _replacement is not None:
            api.replacement_current(_replacement, target=False)
        current_source = api.capture(source)
        if _replacement is not None:
            api.replacement_current(_replacement, target=False)
        api.require_source(source, current_source, allow_dirty_source)
        verify_source(current_source)
        result["stage"] = "duplicate"
        data["pending_mutation"] = {"dispatch_state_at_checkpoint": "not_yet_dispatched", "target_package": target}
        persist(result)
        result["changes"].append(change)
        change.update(attempted=True, created=None)
        data.update(mutation_performed=True if _replacement is not None and data["replacement"]["deleted"] is True else None, mutation_state_unknown=True)
        implicit.update(attempted=None, duplicate_call_attempted=True, status="unknown_until_duplicate_returns")

    def after_return(asset):
        created = asset is not None
        change["created"] = True if created else None
        data["created"] = True if created else None
        data["mutation_performed"] = True if created or (_replacement is not None and data["replacement"]["deleted"] is True) else None
        implicit.update(attempted=True if created else None, duplicate_returned=True,
            status="duplicate_returned" if created else "duplicate_returned_without_asset")

    clone = api.duplicate(source, target, before_dispatch, after_return, _replacement) if _replacement is not None else api.duplicate(source, target, before_dispatch, after_return)
    if clone is None:
        raise OperationError("duplicate_failed", "Engine returned no duplicate; inspect target package and journal before any new request")
    if _replacement is not None:
        api.replacement_clone_current(_replacement, clone)
    data["target_blueprint"] = clone.get_path_name()

    def verify_clone():
        # A prior pass or the shared compiler's status is not this readback's
        # result. Keep its history separately; never erase compile/save scalars.
        validation = result["validation"]
        history = data.setdefault("clone_readback_history", [])
        check = {"sequence": len(history) + 1, "result": "checking", "check": "capture"}
        history.append(check)
        validation.update(status="clone_readback_checking", clone_content="not_verified")
        try:
            if _replacement is not None:
                api.replacement_clone_current(_replacement, clone)
            observed = api.capture(clone)
            if _replacement is not None:
                api.replacement_clone_current(_replacement, clone)
            data["target_after"] = observed
            basic = observed["blueprint"]
            if implicit["engine_status"] is None:
                implicit.update(engine_status=basic["status"], status="status_observed")
            check["check"] = "target_identity"
            if basic["path"] != target or basic["asset_path"] == before["blueprint"]["asset_path"]:
                raise OperationError("duplicate_identity_failed", "Duplicate asset identity does not match the requested target")
            check["check"] = "compiled_status"
            if not observed["compiled"]:
                raise OperationError("duplicate_implicit_compile_failed", "Duplicate is not up to date after engine PostDuplicate compilation; no requested explicit save follows")
            check["check"] = "semantic_content"
            if api.semantic_content(observed) != api.semantic_content(before):
                validation["clone_content"] = "failed"
                raise OperationError("duplicate_content_failed", "Observed clone graph/variable/class content differs after scoped identity normalization; raw source/target observations retained")
            check["check"] = "generated_class_identity"
            if basic["generated_class"] != basic["asset_path"] + "_C":
                raise OperationError("duplicate_generated_class_failed", "Duplicate generated class is not owned by the target asset")
        except Exception as error:
            data["mutation_state_unknown"] = True
            validation.update(status="clone_readback_failed", clone_readback_failure=getattr(error, "code", type(error).__name__))
            check.update(result="failed", error_code=getattr(error, "code", type(error).__name__))
            raise
        validation["clone_content"] = "passed"
        check.update(result="passed", check="complete")
        validation.update(status="clone_readback_passed", clone_readback_failure="")
        return observed

    result["stage"] = "readback"
    observed = verify_clone()
    verify_source()
    if observed["file"]["exists"]:
        result["save"].update(attempted=True, saved=True, status="unexpected_implicit_save_observed")
        raise OperationError("duplicate_unexpected_save", "Duplicate appeared on disk before explicit save; inspect callback/editor settings")
    data["mutation_state_unknown"] = False
    if compile:
        # Reuse the existing compiler/diagnostic/save action, not another journal.
        data["mutation_state_unknown"] = True
        if _replacement is not None:
            api.replacement_clone_current(_replacement, clone)
        blueprint.compile_blueprint_action(result, target, save, True, persist)
        if _replacement is not None:
            api.replacement_clone_current(_replacement, clone)
        data["compile_result"] = {key: data[key] for key in ("compiled", "validation_pass", "status", "diagnostics", "saved") if key in data}
        result["success"] = False  # Final clone/source checks still need to pass.
    elif save:
        result["stage"] = "save"
        api.policy()
        if _replacement is not None:
            api.replacement_clone_current(_replacement, clone)
        persist(result)
        result["save"].update(attempted=True, saved=None, status="unknown_until_save_returns")
        data["mutation_state_unknown"] = True
        if _replacement is not None:
            api.replacement_clone_current(_replacement, clone)
        saved = api.save(clone)
        result["save"].update(saved=saved, status="saved" if saved else "save_failed")
        if _replacement is not None and saved:
            api.replacement_clone_current(_replacement, clone)
        if not saved:
            raise OperationError("duplicate_save_failed", "Duplicate exists but explicit save failed; no rollback or automatic retry")
    result["stage"] = "readback"
    observed = verify_clone()
    verify_source()
    disk_saved = observed["file"]["exists"] and not observed["blueprint"]["dirty"]
    if save and not disk_saved or not save and observed["file"]["exists"]:
        data["mutation_state_unknown"] = True
        raise OperationError("duplicate_save_postcondition_failed", "Target disk/dirty state does not match the explicit save contract")
    data.update(saved=bool(save and disk_saved), mutation_performed=True, mutation_state_unknown=False)
    result["success"] = True
    result["stage"] = "complete"
    result["validation"].update(status="clone_readback_passed", save_state="passed")
    result["retry"].update(duplicate_risk="target_asset_already_created",
        policy="Replay the same operation_id for the recorded result. A new request never overwrites. Loaded readback is not a cold disk reload.")


def _replacement_action(result, source_path, target, root, dry_run, allow_dirty_source, compile, save, checkpoint_callback):
    # Preserve original Delete -> Duplicate ordering under the SAME operation and
    # Batch journal. Existing fresh action still owns clone/compiler/save logic.
    policy = api.policy()
    source = api.blueprint.resolve_blueprint(source_path)
    before = api.capture(source)
    api.require_source(source, before, allow_dirty_source)
    audit = api.dependency_audit(source_path)
    work = api.prepare_replacement(source, before, target, not dry_run)
    data = result["data"]
    target_before = copy.deepcopy(work["target_before"])
    replacement = {"delete_attempted": False, "delete_returned": False, "deleted": False,
        "delete_state_known": True, "old_target_retired": False, "target_before": target_before,
        "full_memory_editor_Undo_reference_scan": "not_available_in_public_Python",
        "known_package_referencers": list(work.get("references", [])),
        "generated_instances_other_than_existing_CDO": list(work.get("instances", [])),
        "global_force_delete_callback_effects_verified": False,
        "scope": "Scoped source/target/dirty/public-reference proof; package refs empty is not a complete memory reference audit. Public ForceDelete may affect references, Undo/editors and GC. No rollback/retry."}
    if "target_observation" in work:
        replacement["old_target_observation"] = copy.deepcopy(work["target_observation"])
    data.update(dry_run=dry_run, source_blueprint=before["blueprint"]["asset_path"], source_package=source_path,
        target_package=target, required_root=root, target_exists_before=work["exists"], overwrite_existing=True,
        deleted_existing_target=False, created=False, compile_requested=compile, source_before=copy.deepcopy(before),
        target_before=target_before, policy=policy, source_dependency_audit=audit, replacement=replacement,
        full_legacy_parity=False, mutation_performed=False, mutation_state_unknown=False,
        write_scope="dry_run_no_asset_mutation" if dry_run else "temp_blueprint_delete_then_duplicate")
    persist = checkpoint_callback or state.checkpoint

    def source_current(label):
        try:
            api.replacement_current(work, target=not work["retired"])
            actual = api.capture(source)
            api.replacement_current(work, target=not work["retired"])
        except Exception:
            data["mutation_state_unknown"] = True
            raise
        data[label] = actual
        if _source_comparison_state(actual) != _source_comparison_state(before):
            data["mutation_state_unknown"] = True
            raise OperationError("duplicate_replacement_source_changed", "Original source file/observed loaded state changed; no next writer or new baseline")

    try:
        source_current("source_after_replacement_preflight")
        if dry_run:
            result["success"] = True
            result["stage"] = "complete"
            result["validation"]["status"] = "dry_run_preflight_passed"
            replacement["destructive_admission"] = "not_run_original_dry_run"
            return
        if work["exists"]:
            change = {"kind": "delete_existing_blueprint", "asset_path": target,
                "attempted": False, "deleted": False}

            def before_delete():
                api.replacement_current(work)
                current_target = api.capture(work["target_role"]["object"])
                api.replacement_current(work)
                if _source_comparison_state(current_target) != _source_comparison_state(work["target_observation"]):
                    raise OperationError("duplicate_replacement_target_changed", "Old target changed before DeleteAsset; no destructive call")
                source_current("source_before_delete")
                result["stage"] = "delete_existing_target"
                data["pending_mutation"] = {"dispatch_state_at_checkpoint": "not_yet_dispatched",
                    "kind": "delete_existing_blueprint", "target_package": target}
                persist(result)

            def delete_started():
                result["changes"].append(change)
                change.update(attempted=True, deleted=None)
                replacement.update(delete_attempted=True, deleted=None, delete_state_known=False, old_target_retired=True)
                data.update(deleted_existing_target=None, mutation_performed=None, mutation_state_unknown=True)

            def delete_returned(value):
                replacement["delete_returned"] = True
                replacement["native_delete_result"] = value if type(value) is bool else None
                # False may follow force replacement/GC callbacks: no no-change assertion.
                replacement["deleted"] = True if value is True else None
                change["deleted"] = True if value is True else None
                data["deleted_existing_target"] = True if value is True else None

            deleted = api.delete_replacement_target(work, before_delete, delete_started, delete_returned)
            if deleted is not True:
                raise OperationError("duplicate_delete_failed", "DeleteAsset failed/returned invalid after dispatch; actual destructive prefix retained, no duplicate or automatic retry")
            source_current("source_after_delete")
            replacement["target_after_delete"] = api.replacement_absent(work)
            source_current("source_after_delete_observation")
            dirty_after = api.replacement_dirty(work)
            replacement["dirty_packages_after_delete"] = dirty_after
            if {p for p in dirty_after if p.casefold() != target.casefold()} != {p for p in work["dirty_before"] if p.casefold() != target.casefold()}:
                raise OperationError("duplicate_replacement_collateral_dirty_changed", "Nonselected dirty package inventory changed during force deletion; no duplicate follows")
            source_current("source_before_replacement_duplicate")
            replacement["delete_state_known"] = True
            data.update(mutation_performed=True, mutation_state_unknown=False)
            # The legacy bool does not mean every engine callback/UI/Undo effect was audited.
        duplicate_blueprint_to_temp_action(result, source_path, target, root, False, False,
            allow_dirty_source, compile, save, persist, _replacement=work)
        if result["success"] is True and work["retired"]:
            result["success"] = False
            dirty_final = api.replacement_dirty(work)
            replacement["dirty_packages_after_replacement"] = dirty_final
            if {p for p in dirty_final if p.casefold() != target.casefold()} != {p for p in work["dirty_before"] if p.casefold() != target.casefold()}:
                raise OperationError("duplicate_replacement_collateral_dirty_changed", "Nonselected dirty package inventory changed during replacement; actual clone/compile/save prefix retained")
            source_current("source_after_replacement")
            result["success"] = True
    except Exception:
        if replacement["delete_attempted"]:
            # Includes later duplicate/source/compile/save errors. A known deletion
            # is never used to clear uncertainty about a newer clone/SDK prefix.
            data["mutation_state_unknown"] = True
        raise
    finally:
        # COPIED metadata only. In particular unknown/retired paths perform no
        # Unreal getter, target lookup, cleanup, save or replay from this block.
        data.update(overwrite_existing=True, target_exists_before=work["exists"],
            deleted_existing_target=replacement["deleted"], target_before=target_before,
            source_before=copy.deepcopy(before), replacement=replacement,
            write_scope="dry_run_no_asset_mutation" if dry_run else "temp_blueprint_delete_then_duplicate")
        if replacement["deleted"] is True:
            data["mutation_performed"] = True
        if replacement["delete_attempted"]:
            result["retry"].update(duplicate_risk="existing_target_delete_or_replacement_attempted",
                policy="Recorded ID replay only. Old target may be deleted and replacement incomplete; inspect actual state before any new operation. No automatic retry or atomic restore.")
