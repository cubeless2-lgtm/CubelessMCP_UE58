"""UE58 public PCG authoring and exact hidden editor-only observations.

AI2 proves reflected method presence, not behavior. No ServerRPC, ExecuteTool,
private engine header, protected selector-field write or generated Cubeless call.
"""

import json
import math
import unreal

from . import blueprint, blueprint_duplicates
from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError
from cubeless_mcp_ue58.validators.dependencies import reject_tool_reference


def _error(code, message):
    raise OperationError(code, message)


def construction_available():
    """Binding presence is separate from the confirmed construction health gap.

    AI3 and AJ2 had fatal Python crashes; the latest occurred during garbage
    collection and identifies reflected pin/array traversal before AddEdge.
    Keep this closed until the native
    observation/settings path is reviewed and explicitly validated by root.
    """
    return False


def require_construction_health():
    if not construction_available():
        _error("pcg_construction_blocked_python_gc",
            "PCG construction is blocked after two fatal Python crashes; "
            "the latest occurred during garbage collection. "
            "Native bindings may exist, but construction health is not verified; "
            "no transient graph or persistent target was attempted by this request. "
            "Original interrupted operation IDs remain blocked; no fallback or retry.")


def dirty_packages():
    return sorted({item.get_path_name() for item in
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_content_packages()) +
        list(unreal.EditorLoadingAndSavingUtils.get_dirty_map_packages())})


def context():
    blueprint.ensure_editor_idle()
    version = unreal.SystemLibrary.get_engine_version()
    if not version.startswith("5.8.3-"):
        _error("pcg_engine_version_unsupported", "This compatibility module was implemented only for UE5.8.3: " + version)
    return {"engine_version": version, "dirty_packages": dirty_packages(),
        "dependency": "public EngineAPI + native finite-setting ToolsetLibrary API; Editor-only leaves, no ServerRPC",
        "server_rpc_dependency": False, "toolset_execute_tool_dependency": False}


def _native(method, *arguments):
    function = getattr(getattr(unreal, "CubelessPCGLibrary", None), method, None)
    if not callable(function):
        function = None
        _error("native_pcg_api_unavailable", "Build/load CubelessPCGLibrary." + method + "; no fallback was executed")
    try:
        raw = function(*arguments)
    finally:
        function = None  # Never retain a reflected callable in a later error/traceback cycle.
    value = json.loads(raw)
    if type(value) is not dict:
        _error("native_pcg_result_invalid", "Native PCG leaf returned a non-object JSON value; no fallback executed")
    if value.get("success") is not True:
        error = OperationError(value.get("error_code", "native_pcg_observation_failed"), value.get("error", "Native PCG leaf failed"))
        error.observation = value
        raise error
    return value


def create_finite_native_graph(values, operation_id, request_owned_general=False):
    """One finite constructor JSON leaf; no Python PCG UObject/value wrappers or retry."""
    document = {"graph_spec": {key: values[key] for key in
        ("asset_path", "nodes", "edges", "forbidden_dependency_prefixes")},
        **{key: values[key] for key in ("overwrite_existing", "allow_overwrite_non_temp", "save", "dry_run")},
        "operation_id": operation_id}
    if request_owned_general:
        document["request_owned_general"] = True  # Native validates source/ownership; not a health override.
    try:
        text = json.dumps(document, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if len(text.encode("utf-8")) > 2 * 1048576 or len(text.encode("utf-16-le")) // 2 > 2 * 1048576 or "\x00" in text:
            raise ValueError("Normalized finite construction carrier exceeds 2MiB or contains NUL")
    except (TypeError, ValueError, OverflowError, UnicodeError, RecursionError) as cause:
        error = OperationError("pcg_native_constructor_carrier_invalid", str(cause))
        error.native_call_dispatched = False
        raise error from cause
    function = getattr(getattr(unreal, "CubelessPCGGraphAuthoringLibrary", None), "create_graph_from_spec_json", None)
    if not callable(function):
        error = OperationError("native_pcg_constructor_unavailable", "Build/load the exact finite native constructor; no Python construction fallback or replay")
        error.native_call_dispatched = False
        raise error
    try:
        try:
            raw = function(text)
        finally:
            # Release this bound Unreal callable before decode/validation/raise.
            function = None
    except Exception as error:
        error.native_call_dispatched = True
        raise
    try:
        native = _decode_result(raw)
        if native.get("operation") != "create_pcg_graph_from_spec":
            raise OperationError("pcg_native_constructor_result_invalid", "Native operation identity differs; returned receipt retained")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native


def require_helpers(*methods):
    for name in methods:
        if not callable(getattr(getattr(unreal, "CubelessPCGLibrary", None), name, None)):
            _error("native_pcg_api_unavailable", "Missing editor-only CubelessPCGLibrary." + name + "; no graph mutation dispatched")


def target_state(package):
    return blueprint_duplicates.target_state(package)


def file_state(package):
    return blueprint_duplicates.file_state(package)


def exists(state):
    return any((state["asset_registered"], state["directory_exists"], state["loaded_package_or_object"], state["file"]["exists"]))


def resolve_graph(package):
    object_path = package + "." + package.rsplit("/", 1)[-1]
    graph = unreal.find_object(None, object_path)
    loaded = graph is not None
    if graph is None:
        graph = unreal.EditorAssetLibrary.load_asset(package)
    if graph is None or graph.get_class().get_path_name() != "/Script/PCG.PCGGraph" or graph.get_path_name() != object_path:
        _error("pcg_graph_not_found", "Exact PCGGraph did not resolve: " + package + "; no fuzzy asset substitution")
    return graph, {"already_loaded": loaded, "asset_load_attempted": not loaded}


def _settings_class(name):
    requested = name
    if name.startswith("/Script/PCG."):
        name = name.split(".", 1)[1]
    elif name.startswith("/"):
        _error("pcg_settings_class_unsupported", "Only native runtime /Script/PCG settings classes are currently admitted")
    class_type = getattr(unreal, name, None)
    if class_type is None and not name.startswith("PCG"):
        class_type = getattr(unreal, "PCG" + name, None)
    if not isinstance(class_type, type) or not issubclass(class_type, unreal.PCGSettings):
        _error("pcg_settings_class_not_found", "No reflected PCGSettings subclass: " + requested)
    if not class_type.static_class().get_path_name().startswith("/Script/PCG."):
        _error("pcg_settings_class_unsupported", "Settings class must belong to the runtime PCG module")
    return class_type


def inspect_settings_class(class_type):
    path = class_type.static_class().get_path_name()
    value = _native("inspect_settings_class", path)
    if (value.get("class_path") != path or value.get("base_class_path") != "/Script/PCG.PCGSettings"
            or value.get("package_path") != "/Script/PCG" or value.get("native_runtime_settings_verified") is not True
            or type(value.get("class_flags_string")) is not str or not value["class_flags_string"].isdigit()
            or value.get("constructible") is not True or value.get("state_preserved") is not True
            or any(value.get(flag) is not False for flag in ("abstract", "deprecated", "newer_version_exists",
                "package_dirty_before", "package_dirty_after", "object_construction_attempted", "class_default_object_observed"))):
        _error("pcg_settings_class_admission_invalid", "Native settings class metadata did not prove constructibility without construction")
    return value


def prepare_settings_classes(nodes):
    """Reject invalid UClass flags before allocating even the transient graph."""
    classes, observations = {}, []
    for node in nodes:
        name = node["settings_class"]
        if name in classes:
            continue
        class_type = _settings_class(name)
        observation = inspect_settings_class(class_type)
        classes[name] = class_type
        observations.append({"requested": name, "native": observation})
    return classes, observations


def transient_graph():
    require_construction_health()
    return unreal.new_object(unreal.PCGGraph)


def creation_policy():
    is_enabled = getattr(getattr(unreal, "SourceControl", None), "is_enabled", None)
    if not callable(is_enabled) or is_enabled() is not False:
        _error("pcg_source_control_policy_unsupported", "AssetTools authoring requires observable disabled source control; no preferences changed")
    return {"source_control_enabled": False, "preferences_changed": False}


def create_graph(package, dispatch_callback=None):
    require_construction_health()
    state = target_state(package)
    if exists(state):
        _error("pcg_asset_exists", "Target became present after preflight; no factory was dispatched")
    creation_policy()
    parent, _, name = package.rpartition("/")
    # CreateAsset uses FactoryCreateNew directly; ConfigureProperties belongs to
    # the dialog path (installed AssetTools.cpp1743–1804 versus2017–2022).
    factory = unreal.PCGGraphFactory()
    if factory.get_editor_property("supported_class").get_path_name() != "/Script/PCG.PCGGraph":
        _error("pcg_factory_class_mismatch", "PCGGraphFactory does not advertise the exact native PCGGraph")
    asset_tools = unreal.AssetToolsHelpers.get_asset_tools()
    graph_class = unreal.PCGGraph.static_class()
    if dispatch_callback is not None:
        dispatch_callback()
    graph = asset_tools.create_asset(name, parent, graph_class, factory)
    if graph is None:
        _error("pcg_graph_create_failed", "PCGGraph factory returned no asset; inspect actual registry/loaded/disk state")
    if graph.get_class().get_path_name() != "/Script/PCG.PCGGraph" or graph.get_path_name() != package + "." + name:
        _error("pcg_graph_identity_mismatch", "Factory returned another asset identity; no retry or cleanup performed")
    return graph


def add_node(graph, settings_class, title, position):
    require_construction_health()
    returned = graph.add_node_of_type(settings_class)
    if isinstance(returned, tuple) and len(returned) == 2:
        node, settings = returned
    elif isinstance(returned, unreal.PCGNode):
        node, settings = returned, returned.get_settings()
    else:
        _error("pcg_add_node_return_unsupported", "Actual add_node_of_type returned an unsupported shape; inspect partial graph, no fallback dispatch")
    if node is None or settings is None or node.get_outer() != graph or settings.get_outer() != node or settings.get_class() != settings_class.static_class():
        _error("pcg_node_identity_mismatch", "Created node/settings identities do not match their exact native owner/class")
    node.set_editor_property("node_title", unreal.Name(title))
    node.set_node_position(*position)
    if str(node.get_editor_property("node_title")) != title or list(node.get_node_position()) != position:
        _error("pcg_node_metadata_mismatch", "Node title/position readback differs")
    return node, settings


def _setting_owner(settings):
    """Only UObject identities cross Python; native code reads all value fields."""
    node = settings.get_outer()
    graph = node.get_outer() if node is not None else None
    if (node is None or graph is None
            or node.get_settings() != settings
            or graph.get_class().get_path_name() != "/Script/PCG.PCGGraph"):
        _error("pcg_setting_owner_invalid", "Exact settings/node/graph identities were not observed; no field read or write dispatched")
    return graph, node


def _setting_result(value, graph, node, settings):
    if (value.get("graph") != graph.get_path_name()
            or value.get("node_id") != node.get_path_name()
            or value.get("settings_path") != settings.get_path_name()
            or value.get("settings_class") != settings.get_class().get_path_name()
            or value.get("python_nested_struct_traversal") is not False
            or type(value.get("native_property_name")) is not str
            or type(value.get("native_property_flags_string")) is not str
            or not value["native_property_flags_string"].isdigit()
            or any(value.get(flag) is not False for flag in ("asset_load_attempted", "save_attempted", "outcome_unknown"))
            or value.get("observation_complete") is not True
            or "actual" not in value):
        error = OperationError("pcg_setting_result_invalid", "Native setting identity/typed observation is incomplete; retain the returned state and do not retry")
        error.observation = value
        raise error
    return value


def inspect_setting(settings, property_name):
    """Read-only finite native observer, usable while construction is blocked."""
    graph, node = _setting_owner(settings)
    value = _setting_result(_native("inspect_setting", graph.get_path_name(),
        node.get_path_name(), property_name), graph, node, settings)
    if (value.get("read_only") is not True or value.get("state_preserved") is not True
            or any(value.get(flag) is not False for flag in ("mutation_attempted", "mutation_performed"))):
        error = OperationError("pcg_setting_observation_changed_state", "Native read-only setting observation did not preserve state")
        error.observation = value
        raise error
    return value


def apply_settings(settings, requested, records=None, before_property=None):
    # Existing domain construction gates remain closed. This adapter performs
    # one native typed leaf per field, without Python PCGPoint/Transform copies.
    # Failed native rows are retained as actual observations, never retried.
    records = [] if records is None else records
    graph, node = _setting_owner(settings)
    for name, requested_value in requested.items():
        if before_property is not None:
            before_property(name)
        try:
            native = _setting_result(_native("apply_setting", graph.get_path_name(),
                node.get_path_name(), name, json.dumps(requested_value,
                    ensure_ascii=False, allow_nan=False)), graph, node, settings)
            if (native.get("requested_value_verified") is not True
                    or native.get("unrelated_contract_fields_preserved") is not True
                    or native.get("graph_topology_preserved") is not True
                    or native.get("compile_attempted") is not False):
                error = OperationError("pcg_setting_result_invalid", "Native write did not prove its finite value/topology postconditions")
                error.observation = native
                raise error
        except Exception as error:
            observation = getattr(error, "observation", None)
            records.append({"property": name, "requested": requested_value,
                "actual": observation.get("actual") if type(observation) is dict else None,
                "verified": False, "error_code": getattr(error, "code", "pcg_setting_call_failed"),
                "error": str(error), "native": observation,
                "automatic_retry": False})
            raise
        records.append({"property": name, "requested": requested_value,
            "actual": native["actual"], "verified": True, "native": native})
    return records


def read_settings(settings, applied):
    """Recheck plain requested values through native observation after wiring."""
    records = []
    for previous in applied:
        native = inspect_setting(settings, previous["property"])
        actual = native["actual"]
        records.append({"property": previous["property"], "actual": actual,
            "verified": equivalent(actual, previous["actual"]), "native": native})
    return records


def equivalent(actual, expected):
    """Preserve strict bool/int/identity, allow only float32 storage rounding."""
    if type(expected) is float and type(actual) is float:
        return math.isclose(actual, expected, rel_tol=1e-6, abs_tol=1e-12)
    if type(expected) is list and type(actual) is list:
        return len(actual) == len(expected) and all(equivalent(a, e) for a, e in zip(actual, expected))
    if type(expected) is dict and type(actual) is dict:
        return actual.keys() == expected.keys() and all(equivalent(actual[k], expected[k]) for k in expected)
    return type(actual) is type(expected) and actual == expected


def inspect_snapshot(graph):
    """Loaded read-only native JSON observation; no Python pin/edge structs.

    This hidden adapter does not enable construction or expose a public tool.
    Root's finite startup probe may call it while all authoring gates are closed.
    Native code strongly retains the graph and compares two public observations.
    """
    path = graph.get_path_name()
    native = _native("inspect_graph_snapshot", path)
    value = native.get("snapshot")
    if (native.get("state_preserved") is not True
            or native.get("observation_complete") is not True
            or native.get("read_only") is not True
            or native.get("python_nested_pin_struct_traversal") is not False
            or any(native.get(flag) is not False for flag in ("mutation_attempted",
                "mutation_performed", "outcome_unknown", "asset_load_attempted",
                "save_attempted", "compile_attempted", "object_construction_attempted"))
            or type(value) is not dict or value.get("graph") != path
            or value.get("package") != graph.get_package().get_path_name()
            or type(value.get("nodes")) is not list or type(value.get("edges")) is not list):
        error = OperationError("pcg_snapshot_result_invalid", "Native loaded snapshot did not prove read-only identity/topology preservation")
        error.observation = native
        raise error
    # Keep the existing parsed JSON graph/node/pin/edge shape. No reflected
    # PinProperties, AllowedTypes, pin arrays or nested edge wrappers are read.
    return value


def snapshot(graph):
    # Loaded observation is the existing finite native JSON path. Construction
    # remains gated in transient_graph/create_graph/add_node and the domain.
    # No Python nested pin/edge structs, load, allocation or fallback here.
    return inspect_snapshot(graph)


def edge_keys(snapshot_value):
    return {(row["from_node"], row["from_pin"], row["to_node"], row["to_pin"]) for row in snapshot_value["edges"]}


def connect(graph, source, source_pin, target, target_pin):
    before = snapshot(graph)
    expected = (source.get_path_name(), source_pin, target.get_path_name(), target_pin)
    if expected in edge_keys(before):
        _error("pcg_duplicate_edge", "Repeated edge in a fresh spec is rejected; no dispatch")
    native = _native("inspect_connection", graph.get_path_name(), source.get_path_name(), source_pin, target.get_path_name(), target_pin)
    if native.get("state_preserved") is not True or native.get("direct_compatible") is not True or native.get("can_connect") is not True:
        _error("pcg_edge_requires_conversion_or_replacement", "This direct spec route requires compatible, connectable pins without implicit conversion or replacing an occupied input: " + json.dumps(native))
    # Return value is the To node, not a success indication. Independent physical
    # edge union proves this mutation, including preservation of prior edges.
    graph.add_edge(source, source_pin, target, target_pin)
    after = snapshot(graph)
    if edge_keys(after) != edge_keys(before) | {expected} or after["node_count_including_factory"] != before["node_count_including_factory"]:
        _error("pcg_edge_readback_mismatch", "Native AddEdge changed another edge/node or did not retain the requested direct edge")
    return {"from_node": expected[0], "from_pin": source_pin, "to_node": expected[2], "to_pin": target_pin,
        "native_connection_preflight": native, "actual_edge_union_verified": True}


def find_node(graph, identifier):
    nodes = list(graph.get_editor_property("nodes")) + [graph.get_input_node(), graph.get_output_node()]
    found = [node for node in nodes if identifier in (node.get_path_name(), node.get_name(), str(node.get_editor_property("node_title")))]
    if len(found) != 1:
        _error("pcg_node_not_found_or_ambiguous", "Use one exact member node path/name/title in this graph")
    return found[0]


def inspect_selector(graph, node, property_name):
    value = _native("inspect_attribute_selector", graph.get_path_name(), node.get_path_name(), property_name)
    if value.get("state_preserved") is not True:
        _error("pcg_selector_observation_changed_state", "Native read-only selector observation did not preserve state")
    return value


def preview_selector(graph, node, property_name, request):
    value = _native("preview_attribute_selector", graph.get_path_name(), node.get_path_name(), property_name, json.dumps(request, ensure_ascii=False, allow_nan=False))
    if value.get("native_copy_preview") is not True or value.get("state_preserved") is not True or value.get("preview_selector", {}).get("valid") is not True:
        _error("pcg_selector_preview_invalid", "Native copied selector preview was not valid or preserved")
    return value


def _selector_semantic(value):
    selection = value["selection"]
    return {"selection": selection, "name": value["name"], "domain_name": value["domain_name"],
        "extra_names": value["extra_names"], "display": value["display"], "valid": value["valid"]}


def finite_native_selector_assignment(preview):
    """Route reflected native scalar selector roles to the guarded C++ leaf.

    This untrusted preview selects transport only. Native independently proves
    the exact runtime settings class, property declaration/owner and concrete
    input/output struct before any scratch construction or target assignment.
    Generated/custom classes, custom structs and container selectors stay out.
    """
    settings_class = preview.get("settings_class")
    name = preview.get("selector_property_name")
    return (type(settings_class) is str
        and settings_class.startswith("/Script/PCG.")
        and type(name) is str and bool(name)
        and preview.get("selector_struct_path") in (
            "/Script/PCG.PCGAttributePropertyInputSelector",
            "/Script/PCG.PCGAttributePropertyOutputSelector"))


def _native_selector_assignment(method, preview, request):
    """One fixed JSON native leaf. No PCG value wrappers, fallback or retry."""
    try:
        document = {"graph_path": preview["graph"], "node_id": preview["node_id"],
            "selector_property_name": preview["selector_property_name"],
            "request": request, "expected_preview": preview}
        text = json.dumps(document, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        if "\x00" in text or len(text.encode("utf-16-le")) // 2 > 262144:
            raise ValueError("Finite selector carrier exceeds256Ki UTF16 units or contains NUL")
    except (TypeError, ValueError, KeyError, OverflowError, UnicodeError, RecursionError) as cause:
        error = OperationError("pcg_native_selector_carrier_invalid", str(cause))
        error.native_call_dispatched = False
        error.outcome_unknown = False
        raise error from cause
    function = getattr(getattr(unreal, "CubelessPCGLibrary", None), method, None)
    if not callable(function):
        function = None
        error = OperationError("native_pcg_api_unavailable", "Build/load CubelessPCGLibrary." + method + "; no fallback or retry")
        error.native_call_dispatched = False
        error.outcome_unknown = False
        raise error
    try:
        try:
            raw = function(text)
        finally:
            function = None  # Release the bound Unreal callable before decode/raise.
    except Exception as error:
        error.native_call_dispatched = True
        error.outcome_unknown = True
        raise
    native = None
    try:
        native = json.loads(raw)
        if type(native) is not dict or native.get("operation") != method:
            raise ValueError("Finite selector native operation identity differs")
        if any(type(native.get(key)) is not bool for key in
                ("success", "mutation_attempted", "change_state_known", "outcome_unknown",
                    "operation_complete", "postcondition_passed", "observation_complete")):
            raise ValueError("Finite selector native outcome flags are incomplete")
        if native["change_state_known"] != (not native["outcome_unknown"]):
            raise ValueError("Finite selector native outcome flags disagree")
        if native["change_state_known"] and type(native.get("mutation_performed")) is not bool:
            raise ValueError("Known finite selector outcome lacks actual mutation state")
        if native.get("mutation_performed") is True and native["mutation_attempted"] is False:
            raise ValueError("Finite selector native actual mutation contradicts no attempt")
        if native["success"] and any(native[key] is not True for key in
                ("operation_complete", "postcondition_passed", "observation_complete")):
            raise ValueError("Finite selector native success lacks complete postcondition/observation proof")
        native["_raw_native_json"] = raw
    except (TypeError, ValueError, RecursionError) as cause:
        error = OperationError("pcg_native_selector_result_invalid", str(cause))
        error.native_call_dispatched = True
        error.native_raw_result = raw
        if type(native) is dict:
            native["_raw_native_json"] = raw
            error.observation = native
        error.outcome_unknown = True
        raise error from cause
    if native["success"] is not True:
        error = OperationError(native.get("error_code", "pcg_native_selector_failed"),
            native.get("error", "Finite native selector leaf failed"))
        error.native_call_dispatched = True
        error.observation = native
        error.outcome_unknown = native["outcome_unknown"]
        raise error
    if native["outcome_unknown"]:
        error = OperationError("pcg_native_selector_result_invalid", "Native success lacks a known mutation outcome")
        error.native_call_dispatched = True
        error.observation = native
        error.outcome_unknown = True
        raise error
    try:
        if native.get("asset_load_attempted") is not False or native.get("save_attempted") is not False:
            raise ValueError("Finite native selector leaf did not prove no asset load/save")
        if method == "preflight_attribute_selector_assignment":
            if (native.get("transient_public_assignment_verified") is not True
                    or type(native.get("converter_dependency")) is not bool
                    or native.get("preview_state") != preview["preview_selector"]
                    or native.get("source_unchanged") is not True
                    or type(native.get("settings_class_admission")) is not dict
                    or native["mutation_attempted"] is not False or native["mutation_performed"] is not False):
                raise ValueError("Finite native preflight did not prove the public assignment and unchanged source")
        else:
            before, after = native.get("before"), native.get("after")
            if (type(preview.get("changed")) is not bool
                    or native["mutation_performed"] is not preview["changed"]
                    or native["mutation_attempted"] is not preview["changed"]
                    or type(before) is not dict or type(after) is not dict
                    or before.get("current_selector") != preview["current_selector"]
                    or before.get("settings_path") != preview["settings_path"]
                    or _selector_semantic(after["current_selector"]) != _selector_semantic(preview["preview_selector"])
                    or native.get("public_selector_copy_assignment") is not True
                    or type(native.get("toolset_native_converter_dependency")) is not bool
                    or native.get("toolset_execute_tool_dependency") is not False):
                raise ValueError("Finite native commit did not prove exact public selector assignment/readback")
    except (TypeError, ValueError, KeyError) as cause:
        error = OperationError("pcg_native_selector_result_invalid", str(cause))
        error.native_call_dispatched = True
        error.observation = native
        error.outcome_unknown = native["outcome_unknown"]
        raise error from cause
    return native


def commit_selector(graph, node, preview, request):
    if finite_native_selector_assignment(preview):
        return _native_selector_assignment("commit_attribute_selector_assignment", preview, request)
    settings = node.get_settings()
    name = preview["selector_property_name"]
    before = inspect_selector(graph, node, name)
    if before["current_selector"] != preview["current_selector"] or before["settings_path"] != preview["settings_path"]:
        _error("pcg_selector_changed_before_dispatch", "Exact selector source changed after preview; nothing dispatched")
    selector = settings.get_editor_property(name).copy()
    kind = request["selector_type"].replace("_", "").replace("-", "").lower()
    if "domain_name" in request:
        selector.set_domain_name(request["domain_name"], False)
    reset = request["reset_extra_names"]
    if kind == "attribute":
        selector.set_attribute_name(request["attribute_name"], reset)
    elif kind == "property":
        selector.set_property_name(request["selected_property_name"], reset)
    elif kind in ("point", "pointproperty"):
        selector.set_point_property(_enum(unreal.PCGPointProperties, request["point_property"]), reset)
    elif kind in ("extra", "extraproperty"):
        selector.set_extra_property(_enum(unreal.PCGExtraProperties, request["extra_property"]), reset)
    else:
        _error("pcg_selector_type_invalid", "Unexpected selector type after native preview")
    settings.modify()
    graph.modify()
    settings.set_editor_property(name, selector)
    converter = False
    if "extra_names" in request:
        # Public native converter is registered by official PCGToolset startup.
        # A complete canonical DSL assignment replaces accessors without writing
        # their protected fields. Native preview and postread remain authoritative.
        converter = True
        returned = unreal.ToolsetLibrary.set_object_properties(settings,
            json.dumps({name: preview["preview_selector"]["display"]}, ensure_ascii=False))
        if returned is not True:
            _error("pcg_selector_converter_failed", "Public selector converter returned false; previous selector assignment may have committed")
    after = inspect_selector(graph, node, name)
    if _selector_semantic(after["current_selector"]) != _selector_semantic(preview["preview_selector"]):
        _error("pcg_selector_readback_mismatch", "Native semantic readback differs from the exact copied preview")
    return {"before": before, "after": after, "public_selector_copy_assignment": True,
        "toolset_native_converter_dependency": converter, "toolset_execute_tool_dependency": False}


def preflight_selector_assignment(graph, node, preview, request):
    """Test actual public struct/canonical-converter assignment off the asset.

    This is a single transient copy inside the named selector operation, not an
    asset fixture. A semantic mismatch stops before dispatching any asset write.
    """
    if finite_native_selector_assignment(preview):
        return _native_selector_assignment("preflight_attribute_selector_assignment", preview, request)
    settings = node.get_settings()
    class_admission = inspect_settings_class(type(settings))
    scratch = transient_graph()
    member, copied_settings = add_node(scratch, type(settings), "CubelessSelectorPreview", [0, 0])
    name = preview["selector_property_name"]
    copied_settings.set_editor_property(name, settings.get_editor_property(name).copy())
    copied_preview = preview_selector(scratch, member, name, request)
    if copied_preview["current_selector"] != preview["current_selector"] or copied_preview["preview_selector"] != preview["preview_selector"]:
        _error("pcg_selector_copy_preview_mismatch", "Transient selector copy differs from native asset preview")
    observed = commit_selector(scratch, member, copied_preview, request)
    current = inspect_selector(graph, node, name)
    if current["current_selector"] != preview["current_selector"]:
        _error("pcg_selector_source_changed_in_preflight", "Transient assignment affected the asset source; do not dispatch or retry")
    return {"transient_public_assignment_verified": True,
        "converter_dependency": observed["toolset_native_converter_dependency"],
        "preview_state": observed["after"]["current_selector"], "source_unchanged": True,
        "settings_class_admission": class_admission}


def _enum(class_type, token):
    normalized = token.split(".")[-1].replace("_", "").lower()
    matches = [getattr(class_type, name) for name in dir(class_type)
        if name.replace("_", "").lower() == normalized]
    if len(matches) != 1:
        _error("pcg_selector_enum_invalid", "Enum did not resolve uniquely after native preview: " + token)
    return matches[0]


def notify(graph, allow_dirty=True):
    return _native("notify_and_recompile", graph.get_path_name(), allow_dirty)


def save(graph):
    value = unreal.EditorAssetLibrary.save_loaded_asset(graph, False)
    if type(value) is not bool:
        _error("pcg_save_return_invalid", "SaveLoadedAsset did not return a boolean; persistence outcome unknown")
    return value


def refresh_saved_file_registry(package, expected_file):
    """Synchronize the exact persisted file before the mandatory disk audit.

    IAssetRegistry::ScanModifiedAssetFiles is BlueprintCallable. Its installed
    UE58 implementation synchronously scans and processes queued loaded assets
    before returning (AssetRegistry.cpp:8959-8985). This is metadata refresh,
    never another save or mutation retry. Keep the original saved prefix.
    """
    before = file_state(package)
    if before != expected_file or not before["exists"] or not before["sha256"]:
        _error("pcg_saved_registry_file_changed", "Exact saved file changed before registry refresh; no resave or retry")
    registry = scanner = None
    try:
        registry = unreal.AssetRegistryHelpers.get_asset_registry()
        scanner = getattr(registry, "scan_modified_asset_files", None)
        if not callable(scanner):
            _error("pcg_saved_registry_refresh_unavailable", "Public AssetRegistry.scan_modified_asset_files is unavailable; saved prefix retained")
        scanner([before["path"]])
    finally:
        scanner = None
        registry = None
    after = file_state(package)
    if after != before:
        _error("pcg_saved_registry_file_changed", "Registry callback changed the saved file; original persistence remains recorded, no retry")
    return {"attempted": True, "returned": True, "file": before,
        "api": "IAssetRegistry::ScanModifiedAssetFiles", "exact_saved_file_only": True,
        "save_requests": 0, "SDK_global_purity_claimed": False}


def audit(package, forbidden_prefixes):
    value = blueprint_duplicates.dependency_audit(package)
    hits = sorted({edge["to"] for edge in value.get("edges", [])
        if any(edge["to"].startswith(prefix) for prefix in forbidden_prefixes)})
    return {"audit": value, "forbidden_dependency_hits": hits}


def sanitize_descriptions(graph):
    changes = []
    objects = [graph] + [node.get_settings() for node in graph.get_editor_property("nodes")]
    for obj in objects:
        try:
            text = str(obj.get_editor_property("description"))
        except Exception:
            continue  # Description is optional; dependency audit remains mandatory.
        if len(text.encode("utf-16-le")) // 2 <= 240:
            continue
        bounded = text.encode("utf-16-le")[:480].decode("utf-16-le", errors="ignore").rstrip()
        obj.set_editor_property("description", bounded)
        actual = str(obj.get_editor_property("description"))
        if len(actual.encode("utf-16-le")) // 2 > 240:
            _error("pcg_description_sanitize_failed", "Owned graph/settings description exceeded the safe legacy scan bound")
        changes.append({"object_path": obj.get_path_name(), "original_tchar_length": len(text.encode("utf-16-le")) // 2,
            "new_tchar_length": len(actual.encode("utf-16-le")) // 2, "sanitized": True})
    return changes
