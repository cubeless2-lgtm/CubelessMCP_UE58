"""Legacy named PCG contracts, with transient preflight and durable outcomes."""

import copy
import json
import math
import hashlib
import uuid

from cubeless_mcp_ue58.compatibility.ue58 import pcg as api
from cubeless_mcp_ue58.compatibility.ue58 import blueprint_duplicates
from cubeless_mcp_ue58.validators.dependencies import reject_tool_reference
from .state import OperationError, checkpoint, fail, mutate, new_result

_RESERVED = {"input", "output", "__input__", "__output__"}
_CPU_FALLOFF_DIAGNOSTIC_SHA = "768a6fd388dd3e72c54f4a7f48660efb79c08be8f3419c56eecff6e5acb8fcc7"
_CPU_FALLOFF_AUTHOR_SOURCE_SHA = "85870fc44876ebc888fa2534ca65a40f3a23305d9f853d2b24cfb8c4b7c8036a"


def _text(value, name, empty=False):
    if type(value) is not str or (not empty and not value) or len(value.encode("utf-16-le")) // 2 >= 1024 or any(ord(c) < 32 or ord(c) == 127 or 0xD800 <= ord(c) <= 0xDFFF for c in value):
        raise OperationError("pcg_name_invalid", name + " must be a bounded string without control/surrogate characters")
    return value


def _bool(value, name):
    if type(value) is not bool:
        raise OperationError("pcg_boolean_invalid", name + " must be a boolean")
    return value


def _unique(pairs):
    value = {}
    for key, item in pairs:
        if key in value:
            raise ValueError("Duplicate JSON object key: " + key)
        value[key] = item
    return value


def _json(text, name):
    if type(text) is not str or len(text.encode("utf-8")) > 1048576:
        raise OperationError("pcg_json_invalid", name + " must be a JSON string no larger than1MiB")
    try:
        return json.loads(text, object_pairs_hook=_unique,
            parse_constant=lambda value: (_ for _ in ()).throw(ValueError("Nonfinite JSON: " + value)))
    except (ValueError, TypeError) as error:
        raise OperationError("pcg_json_invalid", name + ": " + str(error)) from error


def _values(value, name):
    if type(value) is dict:
        for key, item in value.items():
            _text(key, name + " key")
            _values(item, name + "." + key)
    elif type(value) is list:
        if len(value) > 4096:
            raise OperationError("pcg_setting_bound_exceeded", name + " exceeds4096 elements")
        for item in value:
            _values(item, name + "[]")
    elif type(value) is float and not math.isfinite(value):
        raise OperationError("pcg_nonfinite_setting", name + " contains nonfinite number")
    elif type(value) is str:
        if any(ord(c) == 0 or 0xD800 <= ord(c) <= 0xDFFF for c in value):
            raise OperationError("pcg_setting_invalid", name + " contains null/surrogate characters")
        if value.startswith("/"):
            reject_tool_reference(value)


def _strings(value):
    if type(value) is str:
        yield value
    elif type(value) is dict:
        for item in value.values():
            yield from _strings(item)
    elif type(value) is list:
        for item in value:
            yield from _strings(item)


def _path(value):
    try:
        return blueprint_duplicates.package_path(value, "graph_path")
    except OperationError as error:
        raise OperationError("pcg_asset_path_too_long" if type(value) is str and len(value.encode("utf-16-le")) // 2 >= 1024 else "pcg_invalid_asset_path", str(error)) from error


def _finite_spec_settings(settings, settings_class):
    """Preserve admitted legacy math/Name coercion without Unreal observation.

    Partial struct dictionaries keep unspecified components for the Native
    writer to merge from the actual settings value. Invalid conversions remain
    raw so the existing Native preparation receipt owns the failure boundary.
    """
    data_source = settings_class in ("PCGDataFromActorSettings", "/Script/PCG.PCGDataFromActorSettings",
        "PCGGetLandscapeSettings", "/Script/PCG.PCGGetLandscapeSettings")
    # UE56 coerce_value resolves exported PCG Python enum tokens. The finite
    # native converter consumes authored enum names from PCGDataFromActor.h17-27.
    data_source_modes = {
        "PARSE_ACTOR_COMPONENTS": "ParseActorComponents", "GET_SINGLE_POINT": "GetSinglePoint",
        "GET_DATA_FROM_PROPERTY": "GetDataFromProperty", "GET_DATA_FROM_PCG_COMPONENT": "GetDataFromPCGComponent",
        "GET_DATA_FROM_PCG_COMPONENT_OR_PARSE_COMPONENTS": "GetDataFromPCGComponentOrParseComponents",
        "GET_ACTOR_REFERENCE": "GetActorReference", "GET_COMPONENTS_REFERENCE": "GetComponentsReference",
        "GET_POINT_DATA": "GetPointData",
    }
    projection = settings_class in ("PCGProjectionSettings", "/Script/PCG.PCGProjectionSettings")
    transform_points = settings_class in ("PCGTransformPointsSettings", "/Script/PCG.PCGTransformPointsSettings")
    math_fields = (("offsetmin", "offsetmax", "rotationmin", "rotationmax", "scalemin", "scalemax")
        if transform_points else ("pointextents",))
    normalized = {}
    for name, value in settings.items():
        token = name.replace("_", "").lower()
        if projection and token == "projectionparams" and type(value) is dict:
            value = dict(value)
            # Mirrors the legacy enum_value_from_string lookup on exported
            # PCG enum tokens. Omitted members stay omitted for actual-value merge.
            enums = {
                "colorblendmode": ("SourceValue", "TargetValue", "Add", "Subtract", "Multiply"),
                "attributemode": ("ExcludeAttributes", "IncludeAttributes"),
                "attributemergeoperation": ("Min", "Max", "Sub", "Add", "Mul", "Div", "SourceValue", "TargetValue"),
                "tagmergeoperation": ("Source", "Target", "Both"),
            }
            for child, item in value.items():
                if type(item) is str:
                    token_item = item.split(".")[-1].replace("_", "").casefold()
                    for native in enums.get(child.replace("_", "").casefold(), ()):
                        if token_item == native.casefold():
                            value[child] = native
                            break
        elif token in math_fields:
            if type(value) is list and len(value) >= 3:
                try:
                    components = [float(component) for component in value[:3]]
                except (ValueError, TypeError, OverflowError):
                    pass
                else:
                    if all(math.isfinite(component) for component in components):
                        value = components
        elif transform_points and token == "attributename" and value is not None and type(value) is not dict:
            # coerce_value(Name, value) uses safe_unreal_name(str(value)); a
            # dictionary belongs to the legacy recursive-setting path instead.
            value = str(value)
        elif data_source and token == "mode" and type(value) is str:
            value = data_source_modes.get(value.split(".")[-1].upper(), value)
        normalized[name] = value
    return normalized


def _spec(text, overwrite_existing, allow_overwrite_non_temp, save, dry_run):
    document = _json(text, "graph_spec_json")
    if type(document) is not dict:
        raise OperationError("pcg_invalid_graph_spec", "graph_spec must be an object")
    unknown = set(document) - {"asset_path", "graph_path", "nodes", "edges", "forbidden_dependency_prefixes"}
    if unknown:
        raise OperationError("pcg_graph_spec_field_unsupported", "Unsupported graph_spec fields: " + ", ".join(sorted(unknown)))
    package = _path(document.get("asset_path") or document.get("graph_path"))
    nodes = [] if document.get("nodes") is None else document["nodes"]
    edges = [] if document.get("edges") is None else document["edges"]
    if type(nodes) is not list or len(nodes) > 256 or type(edges) is not list or len(edges) > 2048:
        raise OperationError("pcg_invalid_graph_spec", "nodes/edges must be arrays bounded at256/2048")
    known, normalized = set(_RESERVED), []
    inline_material_attribute_orders = []
    for index, node in enumerate(nodes):
        if type(node) is not dict:
            raise OperationError("pcg_invalid_graph_spec", "Each node must be an object")
        accepted = {"id", "settings_class", "class", "title", "position", "node_position", "settings",
            "mesh_entries", "static_mesh_entries", "spawner_mesh_entries", "mesh_selector"}
        if set(node) - accepted:
            raise OperationError("pcg_node_field_unsupported", "Unknown node fields: " + ", ".join(sorted(set(node) - accepted)))
        identifier = _text(node.get("id") or node.get("title") or "Node_" + str(index), "node.id")
        title = _text(node.get("title") or identifier, "node.title")
        identities = {identifier.casefold(), title.casefold()}
        if identities & known:
            raise OperationError("pcg_duplicate_or_reserved_node_id", "Node id/title collides with an earlier or factory input/output identifier")
        known.update(identities)
        settings_class = _text(node.get("settings_class") or node.get("class"), "settings_class")
        reject_tool_reference(settings_class)
        if settings_class in ("TransformPointsSettings", "SurfaceSamplerSettings", "DifferenceSettings"):
            # Legacy resolver also tries the native PCG-prefixed reflected name.
            settings_class = "PCG" + settings_class
        position = node.get("position") or node.get("node_position") or [index * 320, 0]
        if type(position) is not list or len(position) != 2 or any(type(item) is not int or not -(2**31) <= item < 2**31 for item in position):
            raise OperationError("pcg_position_invalid", "position must contain two int32 values without fractional truncation")
        settings = {} if node.get("settings") is None else node["settings"]
        if type(settings) is not dict:
            raise OperationError("pcg_settings_invalid", "Node settings must be an object")
        _values(settings, "settings")
        if settings_class in ("PCGTransformPointsSettings", "/Script/PCG.PCGTransformPointsSettings",
                "PCGSurfaceSamplerSettings", "/Script/PCG.PCGSurfaceSamplerSettings",
                "PCGDataFromActorSettings", "/Script/PCG.PCGDataFromActorSettings",
                "PCGGetLandscapeSettings", "/Script/PCG.PCGGetLandscapeSettings"):
            settings = _finite_spec_settings(settings, settings_class)
        inline = {key: copy.deepcopy(node[key]) for key in (
            "mesh_entries", "static_mesh_entries", "spawner_mesh_entries", "mesh_selector") if key in node}
        _values(inline, "inline mesh configuration")
        selector = inline.get("mesh_selector")
        if (settings_class in ("PCGStaticMeshSpawnerSettings", "/Script/PCG.PCGStaticMeshSpawnerSettings")
                and type(selector) is dict and type(selector.get("type")) is str
                and selector["type"].lower() in ("by_attribute", "attribute")
                and type(selector.get("material_override_attributes")) is dict):
            inline_material_attribute_orders.append({"node_id": identifier, "keys": list(selector["material_override_attributes"])})
        normalized.append({"id": identifier, "title": title, "settings_class": settings_class,
            "position": position, "settings": settings, **inline})
    normalized_edges = []
    for edge in edges:
        if type(edge) is not dict or set(edge) - {"from", "from_node", "to", "to_node", "from_pin", "to_pin"}:
            raise OperationError("pcg_edge_invalid", "Edge must contain only known endpoint/pin fields")
        source = _text(edge.get("from") or edge.get("from_node") or "", "edge.from")
        target = _text(edge.get("to") or edge.get("to_node") or "", "edge.to")
        normalized_edges.append({"from": source, "to": target, "from_pin": _text(edge.get("from_pin") or "Out", "from_pin"),
            "to_pin": _text(edge.get("to_pin") or ("Out" if target in ("output", "__output__") else "In"), "to_pin")})
    prefixes = document.get("forbidden_dependency_prefixes") or []
    if type(prefixes) is not list or len(prefixes) > 64:
        raise OperationError("pcg_dependency_policy_invalid", "forbidden_dependency_prefixes must be an array of at most64 paths")
    for prefix in prefixes:
        _text(prefix, "forbidden dependency prefix")
        if not prefix.startswith("/"):
            raise OperationError("pcg_dependency_policy_invalid", "Dependency prefixes must be mounted paths")
    flags = {key: _bool(value, key) for key, value in {"overwrite_existing": overwrite_existing,
        "allow_overwrite_non_temp": allow_overwrite_non_temp, "save": save, "dry_run": dry_run}.items()}
    if prefixes and not flags["save"]:
        raise OperationError("pcg_dependency_policy_requires_save", "save=True is required to audit forbidden dependencies")
    hits = sorted({value for value in _strings(normalized) if any(value.startswith(prefix) for prefix in prefixes)})
    if hits:
        raise OperationError("pcg_graph_spec_preflight_failed", "Spec settings/class values contain a forbidden dependency: " + ", ".join(hits))
    values = {"asset_path": package, "nodes": normalized, "edges": normalized_edges,
        "forbidden_dependency_prefixes": prefixes, **flags}
    if inline_material_attribute_orders:
        # Journal identity observes dict iteration order; the Native root whitelist omits this private salt.
        values["_inline_material_attribute_order"] = inline_material_attribute_orders
    return values


def _persist(result, callback):
    (callback or checkpoint)(result)


def _base(result):
    result["data"].update(mutation_attempted=False, mutation_performed=False, outcome_unknown=False,
        automatic_retries=0, atomic_rollback=False, full_legacy_pcg_parity=False,
        runtime_generation_verified=False, actor_property_binding_verified=False,
        unported_scope=["weighted/by-attribute mesh selector entries", "generated settings classes", "fuzzy graph-name resolution", "CPU/GPU generation and host binding"],
        api_dependency="public UE58 EngineAPI + finite-setting ToolsetAPI UToolsetLibrary::SetObjectProperties; hidden Editor-only leaves, no ServerRPC or ExecuteTool",
        staged_preview_is_asset_commit=False)
    result["compile"]["implicit_invalidation_counts"] = "not_observed; public PCG setters/edges may notify/invalidate caches"


def _progress(result, callback, stage, scope, **details):
    """Last durable boundary only; it cannot describe a later crashed call."""
    result["stage"] = stage
    result["data"]["progress"] = {"scope": scope, "boundary": "before_next_leaf_or_verified_completion",
        "later_outcome": "unknown_until_return_and_readback", **details}
    _persist(result, callback)


def _build(graph, values, settings_classes, result, callback=None, transient=False, fixed_transient_diagnostic=False):
    if fixed_transient_diagnostic:
        if not transient:
            raise OperationError("pcg_diagnostic_scope_invalid", "The fixed diagnostic never builds a persistent graph")
        _fixed_diagnostic_spec(values)
        _diagnostic_owner(graph)
        for spec in values["nodes"]:
            if settings_classes[spec["settings_class"]].static_class().get_path_name() != "/Script/PCG." + spec["settings_class"]:
                raise OperationError("pcg_diagnostic_class_mismatch", "Diagnostic class mapping differs from the exact pinned native cohort; no member created")
    scope = "transient_preflight" if transient else "persistent_target_build"
    created, connected, members = [], [], []
    built = {"created_nodes": created, "connected_edges": connected}
    if transient:
        result["data"]["preflight"] = built
    else:
        result["data"].update(created_nodes=created, connected_edges=connected)
    _progress(result, callback, scope + ":factory_io_read", scope)
    lookup = {"input": graph.get_input_node(), "__input__": graph.get_input_node(),
        "output": graph.get_output_node(), "__output__": graph.get_output_node()}
    for spec in values["nodes"]:
        _progress(result, callback, scope + ":build_node:" + spec["id"], scope,
            node_id=spec["id"], settings_class=spec["settings_class"], completed_nodes=len(created), completed_edges=len(connected))
        add = _diagnostic_add_node if fixed_transient_diagnostic else api.add_node
        node, settings = add(graph, settings_classes[spec["settings_class"]], spec["title"], spec["position"])
        row = {"id": spec["id"], "title": spec["title"], "node_name": node.get_name(), "node_path": node.get_path_name(),
            "settings_path": settings.get_path_name(), "settings_class": settings.get_class().get_path_name(),
            "position": spec["position"], "settings_applied": [], "settings_readback": []}
        created.append(row)
        members.append((node, settings, row))
        def before_property(name):
            _progress(result, callback, scope + ":setting:" + spec["id"] + ":" + name, scope,
                node_id=spec["id"], property=name,
                leaf="native_json_typed_setting_write_and_independent_readback", completed_properties=len(row["settings_readback"]))
        api.apply_settings(settings, spec["settings"], row["settings_readback"], before_property)
        row["settings_applied"] = list(spec["settings"])
        for alias in (spec["id"], spec["title"], node.get_name(), node.get_path_name()):
            if alias in lookup and lookup[alias] != node:
                raise OperationError("pcg_node_alias_collision", "Engine node alias conflicts with another spec node")
            lookup[alias] = node
    for index, edge in enumerate(values["edges"]):
        _progress(result, callback, scope + ":build_edge:" + str(index), scope,
            edge_index=index, edge=edge, completed_edges=len(connected),
            leaf="snapshot_native_connection_admission_add_edge_readback")
        if edge["from"] not in lookup or edge["to"] not in lookup:
            raise OperationError("pcg_unknown_edge_node", "Edge references an unknown source/target identifier")
        connect = _diagnostic_connect if fixed_transient_diagnostic else api.connect
        readback = connect(graph, lookup[edge["from"]], edge["from_pin"], lookup[edge["to"]], edge["to_pin"])
        connected.append({**edge, **readback})
    for node, settings, row in members:
        _progress(result, callback, scope + ":final_settings_readback:" + row["id"], scope,
            node_id=row["id"], leaf="owner_identity_and_materialized_settings_readback")
        if node.get_outer() != graph or settings.get_outer() != node or node.get_settings() != settings:
            raise OperationError("pcg_settings_owner_changed", "Node/settings identity changed during graph construction")
        row["settings_final_readback"] = api.read_settings(settings, row["settings_readback"])
        if any(item["verified"] is not True for item in row["settings_final_readback"]):
            raise OperationError("pcg_final_setting_readback_mismatch", "A requested setting changed during subsequent node/edge notifications: " + row["id"])
    _progress(result, callback, scope + ":final_snapshot", scope,
        completed_nodes=len(created), completed_edges=len(connected), leaf="native_json_public_graph_pin_and_edge_snapshot")
    actual = api.inspect_snapshot(graph) if fixed_transient_diagnostic else api.snapshot(graph)
    if actual["authored_node_count"] != len(created) or actual["edge_count"] != len(connected):
        raise OperationError("pcg_graph_inventory_mismatch", "Actual authored node/physical edge counts differ from the complete spec")
    built["snapshot"] = actual
    return built


def _fixed_diagnostic_spec(values):
    """Exact source-derived CPU cohort; this is not a configurable executor."""
    plain = {"nodes": values["nodes"], "edges": values["edges"]}
    digest = hashlib.sha256(json.dumps(plain, sort_keys=True, ensure_ascii=False,
        separators=(",", ":"), allow_nan=False).encode("utf-8")).hexdigest()
    if (digest != _CPU_FALLOFF_DIAGNOSTIC_SHA or len(values["nodes"]) != 9
            or sum(len(row["settings"]) for row in values["nodes"]) != 42
            or len(values["edges"]) != 9 or values["save"] is not False
            or values["dry_run"] is not True or values["overwrite_existing"] is not False
            or values["allow_overwrite_non_temp"] is not False):
        raise OperationError("pcg_diagnostic_spec_mismatch", "Only the exact pinned nine-class/42-field/nine-edge transient cohort is admitted")


def _diagnostic_owner(graph):
    if (graph is None or graph.get_class().get_path_name() != "/Script/PCG.PCGGraph"
            or graph.get_package().get_path_name() != "/Engine/Transient"
            or not graph.get_path_name().startswith("/Engine/Transient.")):
        raise OperationError("pcg_diagnostic_owner_invalid", "The fixed diagnostic requires an ordinary native /Engine/Transient graph; no persistent target is allowed")


def _diagnostic_add_node(graph, settings_class, title, position):
    # _build validates the fixed spec before reaching this leaf. This separate
    # private path keeps normal add_node's health gate closed without patching it.
    _diagnostic_owner(graph)
    returned = graph.add_node_of_type(settings_class)
    if isinstance(returned, tuple) and len(returned) == 2:
        node, settings = returned
    elif isinstance(returned, api.unreal.PCGNode):
        node, settings = returned, returned.get_settings()
    else:
        raise OperationError("pcg_add_node_return_unsupported", "Fixed diagnostic received an unsupported native return shape; inspect memory state, no fallback")
    if (node is None or settings is None or node.get_outer() != graph
            or settings.get_outer() != node or settings.get_class() != settings_class.static_class()):
        raise OperationError("pcg_node_identity_mismatch", "Fixed diagnostic node/settings ownership does not match")
    node.set_editor_property("node_title", api.unreal.Name(title))
    node.set_node_position(*position)
    if str(node.get_editor_property("node_title")) != title or list(node.get_node_position()) != position:
        raise OperationError("pcg_node_metadata_mismatch", "Fixed diagnostic node metadata readback differs")
    return node, settings


def _diagnostic_connect(graph, source, source_pin, target, target_pin):
    _diagnostic_owner(graph)
    before = api.inspect_snapshot(graph)
    expected = (source.get_path_name(), source_pin, target.get_path_name(), target_pin)
    if expected in api.edge_keys(before):
        raise OperationError("pcg_duplicate_edge", "Fixed diagnostic edge already exists; no dispatch")
    native = api._native("inspect_connection", graph.get_path_name(), source.get_path_name(),
        source_pin, target.get_path_name(), target_pin)
    if any(native.get(flag) is not True for flag in ("state_preserved", "direct_compatible", "can_connect")):
        raise OperationError("pcg_edge_requires_conversion_or_replacement", "Fixed diagnostic requires an ordinary direct connectable native edge")
    graph.add_edge(source, source_pin, target, target_pin)
    after = api.inspect_snapshot(graph)
    if (api.edge_keys(after) != api.edge_keys(before) | {expected}
            or after["node_count_including_factory"] != before["node_count_including_factory"]):
        raise OperationError("pcg_edge_readback_mismatch", "Fixed diagnostic native edge union differs; memory changes retained")
    return {"from_node": expected[0], "from_pin": source_pin, "to_node": expected[2],
        "to_pin": target_pin, "native_connection_preflight": native, "actual_edge_union_verified": True}


def _diagnose_cpu_falloff_transient(result, graph_spec_json, checkpoint_callback=None):
    """Root's temporary no-argument test Toolset only; never a production route.

    The bootstrap owns fresh operation-ID/PID/project checks and the journal.
    This helper rejects any changed cohort and any present target, then shares
    the domain builder with explicitly transient-only constructors. It performs
    no factory, asset load, notification, compile, save, rollback or retry.
    Success proves only this newly executed transient cohort; it does not enable
    construction_available(), resolve either historical crash or prove GC cause.
    """
    _base(result)
    result["data"].update(construction_available=api.construction_available(), diagnostic_only=True,
        transient_graph_construction_attempted=False, transient_graph_construction_performed=False,
        transient_outcome_unknown=False, persistent_target_factory_attempted=False,
        persistent_target_mutation_performed=False, asset_load_attempted=False,
        notify_attempted=False, compile_attempted=False, persistence_attempted=False,
        mutation_scope="none", original_interrupted_operations_retried=False,
        diagnostic_spec_sha256=_CPU_FALLOFF_DIAGNOSTIC_SHA,
        diagnostic_author_source_sha256=_CPU_FALLOFF_AUTHOR_SOURCE_SHA, gc_root_cause_verified=False)
    result["stage"] = "fixed_transient_diagnostic_preflight"
    values = _spec(graph_spec_json, False, False, False, True)
    _fixed_diagnostic_spec(values)
    package = values["asset_path"]
    prefix, suffix = "/Game/CubelessMCPTests/PCGFalloffTransient_", "/PCG_CPUFalloff"
    token = package[len(prefix):-len(suffix)] if package.startswith(prefix) and package.endswith(suffix) else ""
    try:
        parsed = uuid.UUID(token)
        valid = parsed.version == 4 and parsed.hex == token
    except (ValueError, AttributeError):
        valid = False
    if not valid:
        raise OperationError("pcg_diagnostic_target_invalid", "Use a fresh lowercase UUID4 PCGFalloffTransient test identity; the target is never created")
    before = api.context()
    initial = api.target_state(package)
    if api.exists(initial):
        raise OperationError("pcg_diagnostic_target_present", "Target registry/loaded/directory/file identity must be absent; no construction dispatched")
    api.require_helpers("inspect_settings_class", "inspect_graph_snapshot", "inspect_setting", "apply_setting", "inspect_connection")
    result["data"].update(asset_path=package, editor_before=before, target_before=initial,
        save_requested=False, saved=False, dry_run=True, transient_preflight_executed=False)
    classes, admissions = api.prepare_settings_classes(values["nodes"])
    result["data"]["settings_class_admissions"] = admissions
    _progress(result, checkpoint_callback, "fixed_transient_diagnostic:before_allocation", "transient_diagnostic_only")
    result["data"].update(mutation_attempted=True, mutation_performed=None, outcome_unknown=True,
        transient_graph_construction_attempted=True, transient_graph_construction_performed=None,
        transient_outcome_unknown=True, mutation_scope="transient_ram_only")
    _persist(result, checkpoint_callback)
    graph = None
    try:
        graph = api.unreal.new_object(api.unreal.PCGGraph)
        _diagnostic_owner(graph)
        result["data"].update(transient_graph=graph.get_path_name(), mutation_performed=True,
            transient_graph_construction_performed=True, transient_preflight_executed=True)
        built = _build(graph, values, classes, result, checkpoint_callback,
            transient=True, fixed_transient_diagnostic=True)
        actual_target = api.target_state(package)
        dirty = api.dirty_packages()
        result["data"].update(target_after=actual_target, dirty_packages_after=dirty)
        if actual_target != initial or dirty != before["dirty_packages"]:
            raise OperationError("pcg_diagnostic_preservation_failed", "Fixed transient run changed target identity or content/map dirty scope; no cleanup or retry")
        result["data"].update(graph_after=built["snapshot"], transient_preflight_verified=True,
            outcome_unknown=False, transient_outcome_unknown=False, target_preserved=True,
            transient_graph_lifetime_after_return="not_asserted_no_persistent_ownership",
            construction_health_reenabled=False)
        result["validation"].update(status="fixed_transient_native_json_cohort_observed",
            runtime_generation="not_run", selectors="not_run", disk_reload="not_run",
            packaged_runtime="not_run", gc_root_cause="not_verified")
        result.update(success=True, stage="complete")
        _persist(result, checkpoint_callback)
        return result
    except Exception as error:
        native = getattr(error, "observation", None)
        if type(native) is dict:
            result["data"]["native_failure"] = native
        if graph is not None:
            try:
                result["data"]["partial_transient_graph"] = api.inspect_snapshot(graph)
            except Exception as observation_error:
                result["data"]["partial_observation_error"] = str(observation_error)
        # A graph-only observation does not prove all typed setting after-state.
        # Keep the incomplete overall memory outcome unknown without retrying.
        result["data"]["outcome_unknown"] = True
        result["data"]["transient_outcome_unknown"] = True
        raise


def _save(result, graph, package, callback=None):
    result["stage"] = "save"
    result["save"].update(requested=True, attempted=True, saved=None)
    result["data"].update(mutation_attempted=True, persistence_attempted=True, outcome_unknown=True)
    try:
        returned = api.save(graph)
    except Exception:
        result["save"]["status"] = "exception_persistence_unknown"
        try:
            result["data"]["save_file_on_failure"] = api.file_state(package)
        except Exception as observe_error:
            result["data"]["save_file_observation_error"] = str(observe_error)
        raise
    result["save"].update(saved=returned, status="saved" if returned else "save_returned_false")
    result["data"].update(saved=returned, save_file=api.file_state(package))
    if returned is not True or not result["data"]["save_file"]["exists"] or not result["data"]["save_file"]["sha256"]:
        raise OperationError("pcg_graph_save_failed", "Save did not prove the exact asset on disk; inspect persisted/in-memory state before any new request")
    result["data"].update(mutation_performed=True, persistence_performed=True, outcome_unknown=False)
    # Preserve actual persistence in the existing durable journal before any
    # metadata callback. A later refresh failure never erases this saved prefix.
    _persist(result, callback)
    result["stage"] = "saved_file_registry_refresh"
    result["data"].update(outcome_unknown=True, mutation_state_unknown=True, change_state_known=False)
    result["data"]["saved_file_registry_refresh"] = {"attempted": True, "returned": False}
    _persist(result, callback)
    before_refresh = api.snapshot(graph)
    result["data"]["saved_registry_graph_before"] = before_refresh
    result["data"]["saved_file_registry_refresh"] = api.refresh_saved_file_registry(package, result["data"]["save_file"])
    after_refresh = api.snapshot(graph)
    result["data"]["saved_registry_graph_after"] = after_refresh
    if after_refresh != before_refresh:
        raise OperationError("pcg_saved_registry_graph_changed", "Registry callback changed graph topology; saved prefix and actual snapshot retained, no retry")
    result["data"].update(outcome_unknown=False, mutation_state_unknown=False, change_state_known=True)
    result["data"]["saved_file_registry_refresh"]["graph_topology_preserved"] = True
    _persist(result, callback)



def _preserved(result, package):
    actual = api.dirty_packages()
    before = set(result["data"]["editor_before"]["dirty_packages"])
    unexpected = set(actual) - before - {package}
    removed = before - set(actual) - {package}
    result["data"].update(dirty_packages_after=actual, unexpected_dirty_added=sorted(unexpected), unexpected_dirty_removed=sorted(removed))
    if unexpected or removed:
        raise OperationError("pcg_unrelated_dirty_state_changed", "Another package dirty state changed during this operation; no rollback or retry")
    result["validation"]["unrelated_dirty_state_preserved"] = True


def create_pcg_graph_from_spec(graph_spec_json, overwrite_existing=False, allow_overwrite_non_temp=False,
        save=True, dry_run=False, operation_id=None):
    try:
        values = _spec(graph_spec_json, overwrite_existing, allow_overwrite_non_temp, save, dry_run)
    except Exception as error:
        return fail(new_result(operation_id), error)
    # A dry run constructs a real transient graph; it needs the same owned
    # durable journal and incomplete-identity rejection as a persisted build.
    return mutate("create_pcg_graph_from_spec", values, operation_id, lambda result: _create(result, values))


def create_pcg_graph_from_spec_action(result, graph_spec_json, overwrite_existing=False, allow_overwrite_non_temp=False,
        save=True, dry_run=False, checkpoint_callback=None):
    _create(result, _spec(graph_spec_json, overwrite_existing, allow_overwrite_non_temp, save, dry_run), checkpoint_callback)


_FINITE_NATIVE_FRESH_CLASSES = frozenset((
    "PCGCreatePointsGridSettings", "PCGCreatePointsSettings", "PCGDistanceSettings",
    "PCGDensityRemapSettings", "PCGDensityFilterSettings", "PCGAttributeNoiseSettings",
    "PCGAttributeFilteringSettings", "PCGGetActorPropertySettings", "PCGRandomChoiceSettings",
    "PCGPointExtentsModifierSettings", "PCGBoundsModifierSettings", "PCGStaticMeshSpawnerSettings",
    "PCGTransformPointsSettings", "PCGSurfaceSamplerSettings", "PCGDifferenceSettings",
    "PCGDataFromActorSettings", "PCGGetLandscapeSettings", "PCGProjectionSettings"))


def _finite_native_fresh_profile(values):
    """Pure normalized JSON selection; never a caller-supplied gate override."""
    nodes = values["nodes"]
    if not nodes:
        return False
    for node in nodes:
        name = node["settings_class"]
        if name.startswith("/Script/PCG."):
            name = name[len("/Script/PCG."):]
        if name not in _FINITE_NATIVE_FRESH_CLASSES:
            return False
    return True


def _create_native_graph(result, values, callback=None, request_owned_general=False):
    """Adopt one native composite receipt without subsequent UObject observation."""
    route = "request_owned_native_fresh" if request_owned_general else "finite_native_fresh"
    package = values["asset_path"]
    result["data"].update(construction_route=route, global_construction_gate_changed=False,
        construction_available=False, finite_native_profile_selected=not request_owned_general, request_owned_general_selected=request_owned_general, native_result_returned=False,
        api_dependency="public UE58 native PCG/AssetTools/UFactory APIs in one Editor-only finite JSON leaf; official Registry registration only; no nested MCP/ServerRPC or generic executor",
        native_dispatched=False, automatic_retries=0, atomic_rollback=False,
        asset_path=package, requested_node_count=len(values["nodes"]), requested_edge_count=len(values["edges"]),
        overwrite_existing_requested=values["overwrite_existing"], allow_overwrite_non_temp_requested=values["allow_overwrite_non_temp"],
        save_requested=values["save"], dry_run=values["dry_run"],
        native_dispatch_intent_recorded=True, change_state_known=False, mutation_state_unknown=True,
        outcome_unknown=True, mutation_performed=None,
        progress={"scope": "finite_native_fresh", "boundary": "before_fixed_native_constructor_dispatch",
            "later_outcome": "unknown_until_native_return_and_current_receipt",
            "native_internal_stages": "returned_call_prefix_only; this checkpoint does not prove any later stage completed"})
    effective_save = values["save"] and not values["dry_run"]
    result["save"].update(requested=effective_save, attempted=None if effective_save else False,
        saved=None if effective_save else False, status="native_constructor_not_returned")
    result["stage"] = "before_finite_native_constructor"
    result["retry"].update(automatic_retry=False, duplicate_risk="native_constructor_not_returned_no_replay")
    _persist(result, callback)
    result["data"]["native_dispatched"] = True
    result["stage"] = "finite_native_constructor"
    try:
        native = api.create_finite_native_graph(values, result["operation_id"], request_owned_general=request_owned_general)
    except Exception as error:
        if getattr(error, "native_call_returned", False):
            result["data"].update(native_result_returned=True, native_raw_result=getattr(error, "native_raw_result", None))
        if getattr(error, "native_call_dispatched", None) is False:
            result["data"].update(native_dispatched=False, mutation_attempted=False, mutation_performed=False,
                change_state_known=True, mutation_state_unknown=False, outcome_unknown=False)
            result["changes"] = []
            result["retry"]["duplicate_risk"] = "none_observed"
            result["save"].update(attempted=False, saved=False, status="native_not_dispatched")
        # Uncertain return is retained. No fallback, target/disk readback or cleanup follows.
        _persist(result, callback)
        raise
    known = native["change_state_known"]
    data = copy.deepcopy(native["data"])
    result["data"].update(data)
    result["data"].update(native_result_returned=True, native_raw_result=native["_raw_native_json"],
        native_result_data=copy.deepcopy(data),
        native_outcome={key: copy.deepcopy(value) for key, value in native.items() if key not in ("data", "_raw_native_json")},
        native_mutation_attempted=native["mutation_attempted"], mutation_attempted=native["mutation_attempted"],
        change_state_known=known, mutation_state_unknown=not known, outcome_unknown=not known,
        mutation_performed=native.get("changed") if known else None,
        automatic_retries=0, atomic_rollback=False)
    compile_status = ("compiled" if native["compiled"] is True else "unknown" if native["compile_attempted"] and native["compiled"] is None
        else "failed" if native["compile_attempted"] else "not_attempted" if native["compile_requested"] else "not_requested")
    save_status = ("saved" if native["saved"] else "save_returned_false" if native["save_attempted"]
        else "not_attempted" if native["save_requested"] else "not_run_dry_run" if values["dry_run"] else "not_requested")
    result["compile"].update(requested=native["compile_requested"], attempted=native["compile_attempted"],
        compiled=native["compiled"], status=compile_status)
    result["save"].update(requested=native["save_requested"], attempted=native["save_attempted"], saved=native["saved"], status=save_status)
    result["data"]["compile_save_receipt_source"] = "finite_native_constructor"
    result["changes"] = [] if known and not native["mutation_attempted"] and native.get("changed") is False else [dict(
        kind="create_pcg_graph_from_spec", asset_path=package, attempted=native["mutation_attempted"],
        change_state_known=known, changed=native.get("changed") if known else None,
        mutation_scope=data.get("mutation_scope"), created=data.get("created_new_asset"), object_path=data.get("object_path"),
        postcondition_passed=native["postcondition_passed"])]
    native_validation = data.get("validation", {})
    if type(native_validation) is dict:
        for key in ("complete_dependency_scan", "unrelated_dirty_state_preserved", "settings_float_tolerance"):
            if key in native_validation:
                result["validation"][key] = copy.deepcopy(native_validation[key])
    result["validation"].update(status="native_finite_constructor_postconditions", native_postcondition_passed=native["postcondition_passed"],
        total_change_state_known=known, disk_reload="not_run", packaged_runtime="not_run")
    result["retry"].update(automatic_retry=False, duplicate_risk="unknown_native_constructor_no_replay" if not known
        else "observed_native_prefix_inspect_before_new_request" if native["mutation_attempted"] else "none_observed")
    result["stage"] = native.get("stage") or "returned_finite_native_constructor"
    _persist(result, callback)
    complete = data.get("operation_complete") is True and native.get("operation_complete") is not False
    if not complete:
        result["data"].update(operation_complete=False, in_flight=True, command_may_still_be_in_flight=True,
            outcome_unknown=True, mutation_state_unknown=True, change_state_known=False, mutation_performed=None)
        for row in result["changes"]:
            row.update(change_state_known=False, changed=None)
        result["retry"]["duplicate_risk"] = "native_constructor_in_flight_no_replay"
        _persist(result, callback)
        raise OperationError("pcg_native_constructor_incomplete", "Native construction remains incomplete; no follow-up SDK call or replay")
    if not native["success"]:
        raise OperationError(native.get("error_code") or "pcg_native_constructor_failed", native.get("error") or "Native finite constructor failed; actual prefix retained")
    if (data.get("construction_route") != route or data.get("global_construction_gate_changed") is not False
            or request_owned_general and (data.get("request_owned_current_proof") is not True
                or data.get("Python_PCG_UObject_wrappers_used") is not False
                or data.get("general_native_constructor_profile") is not True)
            or not known or native["postcondition_passed"] is not True
            or effective_save and result["validation"].get("complete_dependency_scan") != "passed"):
        result["data"].update(outcome_unknown=True, mutation_state_unknown=True, change_state_known=False, mutation_performed=None)
        for row in result["changes"]:
            row.update(change_state_known=False, changed=None)
        _persist(result, callback)
        raise OperationError("pcg_native_constructor_postcondition_unverified", "Native finite scope/current postconditions are incomplete; no fallback or replay")
    if (native["compile_requested"] or native["compile_attempted"] or native["compiled"] is not False
            or native["save_requested"] is not (values["save"] and not values["dry_run"])
            or (values["dry_run"] or not values["save"]) and (native["save_attempted"] or native["saved"])
            or values["save"] and not values["dry_run"] and native["saved"] is not True):
        result["data"].update(outcome_unknown=True, mutation_state_unknown=True, change_state_known=False, mutation_performed=None)
        for row in result["changes"]:
            row.update(change_state_known=False, changed=None)
        _persist(result, callback)
        raise OperationError("pcg_native_constructor_unexpected_persistence", "Actual compile/save receipt differs from the original options; returned state retained")
    result.update(success=True, stage="complete")


def _create(result, values, callback=None):
    _base(result)
    # Both routes keep PCG graph/node/pin/edge objects inside the native request.
    # Unsupported profiles never fall back to the historical Python construction.
    return _create_native_graph(result, values, callback,
        request_owned_general=not _finite_native_fresh_profile(values))


def _selector_arguments(graph_path, node_id, selector_property_name, selector_type, attribute_name,
        selected_property_name, domain_name, point_property, extra_property, extra_names_json, reset_extra_names, save, dry_run):
    request = {"selector_type": _text(selector_type, "selector_type"), "reset_extra_names": _bool(reset_extra_names, "reset_extra_names")}
    kind = selector_type.replace("_", "").replace("-", "").lower()
    required = {"attribute": "attribute_name", "property": "selected_property_name", "pointproperty": "point_property",
        "point": "point_property", "extraproperty": "extra_property", "extra": "extra_property"}.get(kind)
    if required is None:
        raise OperationError("pcg_selector_type_invalid", "Expected attribute, property, point_property or extra_property")
    for name, value in {"attribute_name": attribute_name, "selected_property_name": selected_property_name,
            "domain_name": domain_name, "point_property": point_property, "extra_property": extra_property}.items():
        _text(value, name, True)
        if value or name == required:
            request[name] = value
    if extra_names_json is not None:
        extra = _json(extra_names_json, "extra_names_json")
        if extra is not None:
            if type(extra) is not list or len(extra) > 64:
                raise OperationError("pcg_selector_request_invalid", "extra_names must be an array of at most64 strings")
            request["extra_names"] = [_text(value, "extra name") for value in extra]
    return {"graph_path": _path(graph_path), "node_id": _text(node_id, "node_id"),
        "selector_property_name": _text(selector_property_name, "selector_property_name"), "request": request,
        "save": _bool(save, "save"), "dry_run": _bool(dry_run, "dry_run")}


def set_pcg_attribute_selector(graph_path, node_id, selector_property_name, selector_type="attribute", attribute_name="",
        selected_property_name="", domain_name="", point_property="", extra_property="", extra_names_json=None,
        reset_extra_names=True, save=True, dry_run=False, operation_id=None):
    try:
        values = _selector_arguments(graph_path, node_id, selector_property_name, selector_type, attribute_name,
            selected_property_name, domain_name, point_property, extra_property, extra_names_json, reset_extra_names, save, dry_run)
    except Exception as error:
        return fail(new_result(operation_id), error)
    if dry_run:
        result = new_result(operation_id)
        try:
            _set_selector(result, values)
        except Exception as error:
            fail(result, error)
        return result
    return mutate("set_pcg_attribute_selector", values, operation_id, lambda result: _set_selector(result, values))


def set_pcg_attribute_selector_action(result, graph_path, node_id, selector_property_name, selector_type="attribute", attribute_name="",
        selected_property_name="", domain_name="", point_property="", extra_property="", extra_names_json=None,
        reset_extra_names=True, save=True, dry_run=False, checkpoint_callback=None):
    _set_selector(result, _selector_arguments(graph_path, node_id, selector_property_name, selector_type, attribute_name,
        selected_property_name, domain_name, point_property, extra_property, extra_names_json, reset_extra_names, save, dry_run), checkpoint_callback)


def _selector_failure_state(result, error, stage):
    """Retain the native prefix before deciding whether another SDK read is safe."""
    data = result["data"]
    observation = getattr(error, "observation", None)
    if type(observation) is not dict:
        observation = data.get("assignment") if stage == "commit" else None
    dispatched = getattr(error, "native_call_dispatched", None)
    unknown = getattr(error, "outcome_unknown", None)
    if type(observation) is dict:
        data["selector_native_failure"] = observation
        receipt_unknown = (observation.get("outcome_unknown") is not False
            or observation.get("change_state_known") is not True)
        unknown = receipt_unknown if unknown is None else unknown or receipt_unknown
        # Native target flags exclude the separate transient assignment prefix.
        # Preserve an unexpected actual target change even if preflight failed.
        actual_attempt = observation.get("mutation_attempted")
        data["mutation_attempted"] = actual_attempt if type(actual_attempt) is bool else True
        data["mutation_performed"] = observation.get("mutation_performed") if not unknown else None
        data["selector_native_call_dispatched"] = True
        if stage == "commit" and result["changes"]:
            result["changes"][-1]["attempted"] = data["mutation_attempted"]
            result["changes"][-1]["native_call_dispatched"] = True
    elif dispatched is False:
        unknown = False
        data["selector_native_call_dispatched"] = False
        if stage == "commit":
            data.update(mutation_attempted=False, mutation_performed=False)
            result["changes"][-1].update(attempted=False, native_call_dispatched=False)
    elif unknown is None:
        # A legacy commit exception has no authoritative native receipt. It may
        # have assigned the value; do not run more Engine calls to guess its state.
        unknown = stage == "commit" or dispatched is True
    if dispatched is True:
        data["selector_native_call_dispatched"] = True
    raw = getattr(error, "native_raw_result", None)
    if raw is not None:
        data["selector_native_raw_failure"] = raw
    data.update(outcome_unknown=unknown, change_state_known=not unknown,
        mutation_state_unknown=unknown, automatic_retries=0)
    data["selector_failure_stage"] = stage
    data["failure_observation_skipped"] = unknown
    if unknown:
        data["mutation_performed"] = None
    return unknown


def _set_selector(result, values, callback=None):
    _base(result)
    result["data"]["editor_before"] = api.context()
    api.require_helpers("inspect_attribute_selector", "preview_attribute_selector")
    package = values["graph_path"]
    graph, load = api.resolve_graph(package)
    node = api.find_node(graph, values["node_id"])
    result["data"].update(load=load, graph_before=api.snapshot(graph), file_before=api.file_state(package),
        save_requested=values["save"], saved=False, dry_run=values["dry_run"], requested=values["request"])
    result["stage"] = "selector_preflight"
    preview = api.preview_selector(graph, node, values["selector_property_name"], values["request"])
    result["data"].update(preview=preview, changed=preview["changed"], current_selector=preview["current_selector"],
        preview_selector=preview["preview_selector"], graph=preview["graph"], node_id=preview["node_id"],
        selector_property_name=preview["selector_property_name"], selector_struct=preview["selector_struct_path"])
    if values["dry_run"]:
        _preserved(result, package)
        result.update(success=True, stage="complete")
        result["validation"]["status"] = "native_copy_preview_only"
        return
    if preview["changed"]:
        try:
            result["data"]["public_assignment_preflight"] = api.preflight_selector_assignment(graph, node, preview, values["request"])
        except Exception as error:
            _selector_failure_state(result, error, "preflight")
            raise
    else:
        result["data"]["public_assignment_preflight"] = {"skipped": True, "reason": "Native copy preview already equals the current selector"}
    if api.snapshot(graph) != result["data"]["graph_before"] or api.file_state(package) != result["data"]["file_before"]:
        raise OperationError("pcg_selector_source_changed_before_dispatch", "Transient copy preflight changed source graph/file")
    _preserved(result, package)
    if preview["changed"]:
        result["stage"] = "selector_commit"
        result["changes"].append({"kind": "set_pcg_attribute_selector", "graph": graph.get_path_name(), "node_id": node.get_path_name(),
            "selector_property_name": preview["selector_property_name"], "attempted": False})
        _persist(result, callback)
        result["changes"][-1]["attempted"] = True
        result["data"].update(mutation_attempted=True, outcome_unknown=True)
        try:
            result["data"]["assignment"] = api.commit_selector(graph, node, preview, values["request"])
            # Keep the returned actual mutation prefix durably before an
            # independent topology observation can fail. No commit is replayed.
            assignment = result["data"]["assignment"]
            result["data"].update(mutation_performed=assignment.get("mutation_performed", True),
                outcome_unknown=False, change_state_known=True, mutation_state_unknown=False)
            _persist(result, callback)
            after = api.snapshot(graph)
            result["data"]["graph_after"] = after
            if after != result["data"]["graph_before"]:
                raise OperationError("pcg_selector_changed_topology", "Selector assignment changed node/pin/edge topology; inspect known partial asset")
            result["data"].update(mutation_performed=assignment.get("mutation_performed", True), outcome_unknown=False)
        except Exception as error:
            unknown = _selector_failure_state(result, error, "commit")
            if not unknown:
                try:
                    result["data"]["actual_selector_on_failure"] = api.inspect_selector(graph, node, preview["selector_property_name"])
                    result["data"]["actual_graph_on_failure"] = api.snapshot(graph)
                    result["data"]["mutation_performed"] = result["data"]["actual_selector_on_failure"]["current_selector"] != preview["current_selector"]
                except Exception as observe_error:
                    result["data"].update(failure_observation_error=str(observe_error), outcome_unknown=True,
                        change_state_known=False, mutation_state_unknown=True)
            raise
    else:
        result["data"]["graph_after"] = result["data"]["graph_before"]
    if values["save"]:
        _save(result, graph, package, callback)
        result["data"]["dependency"] = api.audit(package, [])
        result["validation"]["complete_dependency_scan"] = "passed"
    _preserved(result, package)
    result["validation"]["status"] = "native_selector_and_topology_readback_passed"
    result.update(success=True, stage="complete")


def compile_or_notify_pcg_graph(graph_path, operation_id=None):
    try:
        package = _path(graph_path)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("compile_or_notify_pcg_graph", {"graph_path": package}, operation_id,
        lambda result: compile_or_notify_pcg_graph_action(result, package))


def compile_or_notify_pcg_graph_action(result, graph_path, checkpoint_callback=None):
    _base(result)
    package = _path(graph_path)
    result["data"]["editor_before"] = api.context()
    api.require_helpers("notify_and_recompile")
    graph, load = api.resolve_graph(package)
    before = api.snapshot(graph)
    result["data"].update(load=load, graph_before=before)
    result["stage"] = "notify_and_recompile"
    result["changes"].append({"kind": "pcg_notify_recompile_cache", "graph": graph.get_path_name(), "attempted": False})
    _persist(result, checkpoint_callback)
    result["changes"][-1]["attempted"] = True
    result["data"].update(mutation_attempted=True, outcome_unknown=True)
    result["compile"].update(requested=True, attempted=True, status="unknown_until_native_return", semantic="PCG graph cache invalidation, not Blueprint/shader compiler success")
    try:
        native = api.notify(graph, True)
    except Exception as error:
        observation = getattr(error, "observation", None)
        if type(observation) is dict:
            result["data"]["native_failure"] = observation
            result["changes"][-1]["native_call_dispatched"] = True
            if (observation.get("mutation_attempted") is False and observation.get("mutation_performed") is False
                    and observation.get("outcome_unknown") is False):
                result["changes"][-1]["attempted"] = False
                result["data"].update(mutation_attempted=False, mutation_performed=False, outcome_unknown=False)
                result["compile"].update(attempted=False, status="native_preflight_rejected")
        raise
    result["data"].update(native=native, graph=native["graph"], notified=native["notified"], recompiled=native["recompiled"],
        mutation_performed=True)
    result["compile"].update(status="cache_return_observed", recompiled=native["recompiled"], exact_recompile_semantics=native["recompiled_semantics"])
    # Preserve the actual native return/flags/request counts before the next
    # observation. Failure there retains this prefix and unknown scoped state.
    _persist(result, checkpoint_callback)
    result["data"]["graph_after"] = api.snapshot(graph)
    result["data"]["outcome_unknown"] = False
    if result["data"]["graph_after"] != before:
        raise OperationError("pcg_notify_changed_topology", "Notification/cache operation changed graph topology; inspect actual state")
    _preserved(result, package)
    result["validation"]["status"] = "cache_return_and_topology_observed"
    result.update(success=True, stage="complete")


def save_pcg_graph(graph_path, operation_id=None):
    try:
        package = _path(graph_path)
    except Exception as error:
        return fail(new_result(operation_id), error)
    return mutate("save_pcg_graph", {"graph_path": package}, operation_id, lambda result: save_pcg_graph_action(result, package))


def save_pcg_graph_action(result, graph_path, checkpoint_callback=None):
    _base(result)
    package = _path(graph_path)
    result["data"]["editor_before"] = api.context()
    graph, load = api.resolve_graph(package)
    before = api.snapshot(graph)
    result["data"].update(load=load, graph=graph.get_path_name(), graph_before=before, file_before=api.file_state(package))
    result["changes"].append({"kind": "save_pcg_graph", "graph": graph.get_path_name(), "attempted": False})
    _persist(result, checkpoint_callback)
    result["changes"][-1]["attempted"] = True
    result["data"]["mutation_attempted"] = True
    _save(result, graph, package, checkpoint_callback)
    result["data"].update(mutation_performed=True, graph_after=api.snapshot(graph))
    _persist(result, checkpoint_callback)
    result["data"]["dependency"] = api.audit(package, [])
    if result["data"]["graph_after"] != before:
        raise OperationError("pcg_save_changed_topology", "Save changed graph topology; inspect current asset")
    _preserved(result, package)
    result["validation"].update(status="saved_file_and_topology_observed", complete_dependency_scan="passed")
    result.update(success=True, stage="complete")
