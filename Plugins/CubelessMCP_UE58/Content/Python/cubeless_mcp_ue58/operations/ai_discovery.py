"""Three original read-only AI discovery contracts, independent of transport."""

import copy

from cubeless_mcp_ue58.compatibility.ue58 import ai_discovery as api
from . import state
from .state import OperationError


SUPPORT = "inspect_ai_authoring_support"
LIST = "list_ai_assets"
SEARCH = "search_ai_behavior_tree_node_classes"
_FORBIDDEN = ("/script/unrealmcp", "/unrealmcp", "unrealmcp", "mcpunreal", "mcp_unreal",
    "/script/cubelesscontrolrigtools", "cubelesscontrolrigtools", "cubelesscontrolrigtoolseditor")


def _text(value, field):
    if type(value) is not str or "\x00" in value:
        raise OperationError("ai_discovery_input_invalid", field + " requires a string without NUL; no Engine query ran")
    return value


def _timeout(value):
    if type(value) is not int or not 1 <= value <= 2**31 - 1:
        raise OperationError("ai_discovery_timeout_invalid", "timeout_seconds requires a positive signed-int32 integer; it is caller context, not a game-thread wait/deadline")
    return value


def _maximum(value, upper, *, clamp=True):
    if type(value) is not int or not -(2**31) <= value <= 2**31 - 1:
        raise OperationError("ai_discovery_input_invalid", "max_results requires a signed-int32 integer, not a boolean or coercible value")
    return max(1, min(value, upper)) if clamp else value


def _reference_guard(arguments):
    # Preserve the original guard on literal query/root/filter strings. It is
    # historical input admission, not evidence of a dependency scan of results.
    for field, value in arguments.items():
        if type(value) is str and any(token in value.strip().strip("\"'").replace("\\", "/").lower() for token in _FORBIDDEN):
            raise OperationError("ai_discovery_reference_unsupported", "MCP-only reference token in " + field + "; no Engine query ran")


def _list_arguments(asset_root, asset_type, name_filter, max_results, timeout_seconds):
    root = _text(asset_root, "asset_root").replace("\\", "/").rstrip("/") or "/Game"
    kind = _text(asset_type, "asset_type").strip().lower()
    if not (root == "/Game" or root.startswith("/Game/")) or ".." in root or "." in root:
        raise OperationError("invalid_ai_asset_root", "asset_root must be /Game or a project package subpath, not an object/traversal path")
    if kind not in {"all", "behavior_tree", "blackboard", "eqs"}:
        raise OperationError("invalid_ai_asset_type", "asset_type must be all, behavior_tree, blackboard, or eqs")
    arguments = {"asset_root": root, "asset_type": kind,
        "name_filter": _text(name_filter, "name_filter").strip().lower(),
        "max_results": _maximum(max_results, 2000), "timeout_seconds": _timeout(timeout_seconds)}
    _reference_guard(arguments)
    return arguments


def _search_arguments(category, query, include_abstract, max_results, timeout_seconds):
    if type(include_abstract) is not bool:
        raise OperationError("ai_discovery_input_invalid", "include_abstract requires an exact boolean")
    arguments = {"category": _text(category, "category"), "query": _text(query, "query"),
        "include_abstract": include_abstract, "max_results": _maximum(max_results, 500, clamp=False),
        "timeout_seconds": _timeout(timeout_seconds)}
    # Native retains original lowercase-without-trimming category/query policy.
    _reference_guard(arguments)
    return arguments


def _context(result, identity, arguments):
    result["data"].update(operation=identity, requested_arguments=copy.deepcopy(arguments),
        caller_timeout_seconds=arguments.get("timeout_seconds", 30),
        native_game_thread_timeout_applied=False, automatic_read_retry=False,
        explicit_compile_calls=0, explicit_save_calls=0, full_legacy_ai_parity=False,
        runtime_ai_execution_verified=False, cold_persistence_verified=False,
        returned_inventory_dependency_audit_verified=False)


def _read_action(result, identity, arguments, checkpoint_callback=None):
    _context(result, identity, arguments)
    result["stage"] = "engine_read"
    result["data"].update(engine_api_dispatched=True, engine_read_returned=False,
        read_outcome_unknown=True, native_dispatched=identity == SEARCH)
    try:
        if identity == SUPPORT:
            payload = api.inspect_ai_authoring_support()
        elif identity == LIST:
            payload = api.list_ai_assets(arguments["asset_root"], arguments["asset_type"],
                arguments["name_filter"], arguments["max_results"])
        else:
            native = api.search_ai_behavior_tree_node_classes(arguments["category"], arguments["query"],
                arguments["include_abstract"], arguments["max_results"])
            # Retain every actual native field and prefix before error/proof checks.
            result["data"].update(copy.deepcopy(native["data"]))
            result["data"]["native_outcome"] = {key: copy.deepcopy(value) for key, value in native.items() if key != "data"}
            result["stage"] = native.get("stage", "native_ai_discovery_read")
            result["data"].update(engine_read_returned=True,
                read_outcome_unknown=not native["change_state_known"],
                native_mutation_attempted=native["mutation_attempted"])
            result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"], compiled=native["compiled"])
            result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"])
            no_work = (native["change_state_known"] and native["changed"] is False
                and not native["mutation_attempted"] and not native["compile_attempted"] and not native["save_attempted"])
            result["changes"] = [] if no_work else [{"kind": identity,
                "attempted": native["mutation_attempted"], "change_state_known": native["change_state_known"],
                "changed": native.get("changed") if native["change_state_known"] else None,
                "outcome": "unexpected_native_read_effect_or_unknown"}]
            if checkpoint_callback:
                checkpoint_callback(result, "returned_ai_discovery_read")
            if native["success"] is not True:
                raise OperationError(native.get("error_code") or "search_ai_behavior_tree_node_classes_failed", native.get("error") or "Native loaded-class discovery failed; inspect retained observations")
            if (native["mutation_attempted"] or not native["change_state_known"] or native["changed"]
                    or not native["postcondition_passed"] or native["compile_requested"] or native["compile_attempted"]
                    or native["compiled"] is not None or native["save_requested"] or native["save_attempted"] or native["saved"]):
                raise OperationError("ai_discovery_read_contract_failed", "Native Search did not return its finite no-writer read contract; raw result is retained")
            payload = {}
        result["data"].update(copy.deepcopy(payload))
        _context(result, identity, arguments)
        result["data"].update(engine_read_returned=True, read_outcome_unknown=False)
        result["validation"].update(status="current_editor_discovery", native_read_postcondition=identity == SEARCH)
        result["success"] = True
        result["stage"] = "complete"
    except Exception as error:
        prefix = getattr(error, "observation_prefix", None)
        if type(prefix) is dict:
            result["data"].update(copy.deepcopy(prefix))
        if getattr(error, "code", "") in {"ai_asset_registry_unavailable", "native_ai_discovery_api_unavailable"}:
            result["data"].update(engine_api_dispatched=False, native_dispatched=False,
                engine_read_returned=False, read_outcome_unknown=False)
        raise


def _read(identity, arguments):
    result = state.new_result()
    try:
        _read_action(result, identity, arguments)
    except Exception as error:
        state.fail(result, error)
    return result


def _rejected(identity, error):
    result = state.fail(state.new_result(), error)
    result["data"].update(operation=identity, admission_rejected=True,
        engine_api_dispatched=False, native_dispatched=False, read_outcome_unknown=False,
        mutation_attempted=False, mutation_performed=False, changed=False)
    return result


def inspect_ai_authoring_support():
    return _read(SUPPORT, {})


def inspect_ai_authoring_support_action(result, checkpoint_callback=None):
    _read_action(result, SUPPORT, {}, checkpoint_callback)


def list_ai_assets(asset_root="/Game", asset_type="all", name_filter="", max_results=500, timeout_seconds=60):
    try:
        arguments = _list_arguments(asset_root, asset_type, name_filter, max_results, timeout_seconds)
    except Exception as error:
        return _rejected(LIST, error)
    return _read(LIST, arguments)


def list_ai_assets_action(result, asset_root="/Game", asset_type="all", name_filter="", max_results=500,
        timeout_seconds=60, checkpoint_callback=None):
    _read_action(result, LIST, _list_arguments(asset_root, asset_type, name_filter, max_results, timeout_seconds), checkpoint_callback)


def search_ai_behavior_tree_node_classes(category="all", query="", include_abstract=False, max_results=200, timeout_seconds=60):
    try:
        arguments = _search_arguments(category, query, include_abstract, max_results, timeout_seconds)
    except Exception as error:
        return _rejected(SEARCH, error)
    return _read(SEARCH, arguments)


def search_ai_behavior_tree_node_classes_action(result, category="all", query="", include_abstract=False,
        max_results=200, timeout_seconds=60, checkpoint_callback=None):
    _read_action(result, SEARCH, _search_arguments(category, query, include_abstract, max_results, timeout_seconds), checkpoint_callback)
