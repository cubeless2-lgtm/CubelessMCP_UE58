"""Bounded graph mutation tools for the first legacy mutation pipeline port."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import graph
from cubeless_mcp_ue58.operations import blueprint_node_cleanup
from cubeless_mcp_ue58.operations import blueprint_make_struct


@unreal.uclass()
class CubelessBlueprintGraphTools(unreal.ToolsetDefinition):
    'Guarded K2 graph authoring; each method retains its bounded types, source/lifetime checks and partial-change reporting. Compilation and saving are separate unless explicitly requested. Detailed contracts: CubelessDiscoveryTools.describe_toolset with this exact registered toolset name, verbosity="terse" to select, then exact method query, limit=1 and verbosity="full". Full detail may be large.'

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_map_function_node(blueprint_name: str, operation: str,
            key_type: str = "string", value_type: str = "vector", param_defaults_json: str = "{}",
            node_position_json: str = "[0,0]", graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a stock Map clear/find/add/contains/remove/keys/values/length node for the supported String-key Vector/double-value profiles. param_defaults_json is an object: only supported Key/Value literals are accepted; empty defaults retain stock wildcard pins. TargetMap and outputs are wire-only. Uses an existing admitted graph; create_graph_if_missing must be false. No wiring, runtime map execution, compile or save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_map_function_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.maps import add_blueprint_map_function_node
        return reflected_result(add_blueprint_map_function_node(blueprint_name, operation, key_type, value_type,
            param_defaults_json, node_position_json, graph_name, graph_id, graph_type,
            create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_loop_node(blueprint_name: str, loop_type: str = "for_loop",
            first_index: int | None = None, last_index: int | None = None, node_position_json: str = "[0,0]",
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None,
            element_type: str | None = None, type_object: str | None = None,
            enum_type: str | None = None) -> CubelessPythonOperationResult:
        'Create a stock ForLoop or a supported typed ForEach MacroInstance. ForEach supports bounded primitive/native object profiles; enum_type must be empty and array/object pins are wire-only. BFL support is limited to the documented PresetLoader Name/AssetData profiles. node_position_json is [x,y]; no implicit graph creation, wiring, loop execution, compile or save. Prepare the stock macro source explicitly when required. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_loop_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.loops import add_blueprint_loop_node
        return reflected_result(add_blueprint_loop_node(blueprint_name, loop_type, first_index, last_index,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id,
            element_type, type_object, enum_type))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_make_array_node(blueprint_name: str, element_type: str,
            input_count: int | None = None, values: str | None = None,
            type_object: str | None = None, enum_type: str | None = None, node_position: str | None = None,
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None,
            element_defaults: str | None = None) -> CubelessPythonOperationResult:
        'Create a typed MakeArray node using the supported primitive, native type and source-checked descriptor profiles. values, element_defaults and node_position are JSON-encoded arrays. values takes precedence over element_defaults; explicit input_count overrides inferred count. Unsupported literals and graph/type combinations are rejected. No wiring, compile, save or runtime array evaluation. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_make_array_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations import make_arrays
        from cubeless_mcp_ue58.operations.state import fail, new_result
        from cubeless_mcp_ue58.operations.variables import _json_value
        try:
            finite_values = ({"allow_finite_double": True} if str(element_type).strip().casefold().replace("_", "").replace("-", "").replace(" ", "")
                in {"int64", "integer64", "float", "single", "double", "real", "text", "object", "class", "rotator", "transform", "struct", "scriptstruct"} else {})
            typed_values = make_arrays.decode_registry_list(values, "values", **finite_values)
            typed_defaults = make_arrays.decode_registry_list(element_defaults, "element_defaults", **finite_values) if typed_values is None else None
            typed_position = _json_value(node_position, "node_position") if node_position is not None else None
        except Exception as error:
            return reflected_result(fail(new_result(operation_id), error))
        return reflected_result(make_arrays.add_blueprint_make_array_node(blueprint_name,
            element_type, input_count, typed_values, type_object, enum_type,
            typed_position, graph_name, graph_id, graph_type, create_graph_if_missing,
            operation_id, typed_defaults))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_array_function_node(blueprint_name: str, operation: str, element_type: str,
            param_defaults_json: str = "{}", type_object: str | None = None, enum_type: str | None = None,
            node_position_json: str = "[0,0]", graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, create_graph_if_missing: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a typed Array Length/Get/Set/Add/AddUnique/Insert/Contains/RemoveItem/Clear node. Supported primitive, enum, native math, object/class and UDS profiles have distinct literal and graph restrictions. param_defaults_json is an object; collection and object pins generally require typed wires. Existing source/type/graph guards and bounded graph creation remain active. No runtime evaluation or explicit compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_array_function_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.arrays import add_blueprint_array_function_node
        return reflected_result(add_blueprint_array_function_node(blueprint_name, operation, element_type,
            param_defaults_json, type_object, enum_type, node_position_json, graph_name, graph_id, graph_type,
            create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_is_valid_exec_macro_node(blueprint_name: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one installed stock IsValid exec macro in the exact member-free PresetLoader Blueprint Function Library. This is separate from the existing pure IsValid function. Prepare StandardMacros explicitly through AssetTools.load_asset or the existing stock support inspector load option; construction and connection readers do not load or repair that source. Native checks the current stock source and derives instance ports from its tunnels. Creating a node does not connect or execute it, compile or save. Unsupported context or implicit graph creation is refused; partial and uncertain state is retained without automatic retry or atomic rollback.

        Args:
            blueprint_name: Exact admitted Game Blueprint Function Library.
            node_position_json: Encoded bounded integer pair; default origin.
            graph_name: Optional exact existing PresetLoader function name.
            graph_id: Optional exact graph GUID agreeing with other selectors.
            graph_type: Optional existing function graph selector.
            create_graph_if_missing: Must remain False; graph creation is separate.
            operation_id: Optional durable operation ID; unknown mutations are never replayed.
        """
        from cubeless_mcp_ue58.operations.validity_macros import add_blueprint_is_valid_exec_macro_node
        return reflected_result(add_blueprint_is_valid_exec_macro_node(blueprint_name,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_data_table_bool_select_node(blueprint_name: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one stock K2Node_Select in the exact already-loaded member-free PresetLoader Blueprint Function Library. This dedicated tool preserves the existing Math SelectObject tool and its contract. Initial pins remain stock wildcards; connect the existing bool producer to Index first and an exact DataTable producer to an option through connect_blueprint_graph_pins. That separate API checks current pins and the stock reconstruction transition, with Option 0=False and Option 1=True. Creation alone does not establish a typed selection, execute it, compile or save. Unsupported contexts and implicit graph creation are refused; partial changes and uncertain outcomes are retained without automatic replay or atomic rollback.

        Args:
            blueprint_name: Exact admitted already-loaded Game Blueprint Function Library.
            node_position_json: Bounded integer pair as encoded JSON; default origin.
            graph_name: Optional exact existing PresetLoader function name.
            graph_id: Optional exact graph GUID agreeing with other selectors.
            graph_type: Optional selector for the existing function graph.
            create_graph_if_missing: Must remain False; graph creation is separate.
            operation_id: Optional durable request ID; uncertain mutations are never replayed automatically.
        """
        from cubeless_mcp_ue58.operations.data_table_selects import add_blueprint_data_table_bool_select_node
        return reflected_result(add_blueprint_data_table_bool_select_node(blueprint_name,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_get_data_table_row_node(blueprint_name: str, row_struct_path: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Author one dedicated DataTable Row node in the finite own PresetLoader BFL profile. Validate an already-loaded, saved-clean exact one-FString SourceString UDS with an empty FString default, or the finite compiled 1..32-field FString/Name/bool/int32/int64/float/double scalar UDS profile. The scalar branch reports every authored GUID and full converted PinType; this is not generic UDS support. Source compiled defaults are observed without reading runtime rows. Load the source explicitly with official AssetTools.load_asset before this node-only call; this call does not load it. Initial ReturnValue remains wildcard/pending: connect it to the corresponding typed UDS Break input through the checked connection API, then wire DataTable/RowName explicitly. No private type setter, implicit graph creation, execution, compile/save, rollback or automatic retry. This new editor authoring contract fills the recorded legacy pipeline gap; the old snapshot's public Row prototype/success is unconfirmed.

        Args:
            blueprint_name: Exact runtime-safe /Game Blueprint package/object path.
            row_struct_path: Exact already-loaded, saved-clean one-FString SourceString UDS object path with empty FString default, or bounded 1..32-field scalar UDS object path; a validation source, not a forced pin-type assignment.
            node_position_json: Strict JSON [x,y] of bounded integer coordinates.
            graph_name: Existing own PresetLoader function graph name.
            graph_id: Optional matching nonzero graph GUID.
            graph_type: Existing function selector; broader targets remain incomplete.
            create_graph_if_missing: False; implicit creation is explicitly unsupported.
            operation_id: Optional durable request ID; stored outcomes are replayed without repeating uncertain mutations.
        """
        from cubeless_mcp_ue58.operations.data_table_nodes import add_blueprint_get_data_table_row_node
        return reflected_result(add_blueprint_get_data_table_row_node(blueprint_name, row_struct_path,
            node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_break_struct_node(blueprint_name: str, struct_type: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, create_graph_if_missing: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a native BreakVector, BreakRotator or BreakTransform call, or a supported source-checked UDS/AssetData BreakStruct node. Math names normalize spaces, underscores and hyphens; arbitrary structs are not aliases. node_position_json is [x,y]. Conflicting graph selectors and unsupported source/type combinations are rejected. No automatic wiring, explicit compile/save or runtime evaluation. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_break_struct_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.struct_breaks import add_blueprint_break_struct_node
        return reflected_result(add_blueprint_break_struct_node(blueprint_name, struct_type, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_event_node(blueprint_name: str, event_name: str, node_position_json: str = "[0,0]",
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a supported native event node with its declared parameter types and source-checked signature. Select an event graph by agreeing name/GUID/type, or use the ordinary default EventGraph resolution/bootstrap path. node_position_json is [x,y]. Native owner, source, node and pin lifetime guards apply. Creation does not dispatch the event; implicit structural effects are reported separately from explicit compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_event_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.events import add_blueprint_event_node
        return reflected_result(add_blueprint_event_node(blueprint_name, event_name, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_custom_event_node(blueprint_name: str, custom_event_name: str, node_position_json: str = "[0,0]",
            signature_source_dispatcher_name: str | None = None, signature_source_class: str | None = None,
            signature_source_delegate_name: str | None = None, native_delegate_name: str | None = None,
            call_in_editor: bool | None = None, graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, create_graph_if_missing: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a CustomEvent, optionally using a supported dispatcher/native delegate signature. Ordinary parameterless calls resolve or bootstrap the default EventGraph; specialized signatures retain their graph/type restrictions. signature_source_delegate_name takes precedence over native_delegate_name. call_in_editor=None preserves the existing value; false is meaningful. Created graph/node prefixes survive failure; no automatic wiring or event execution. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_custom_event_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.events import add_blueprint_custom_event_node
        return reflected_result(add_blueprint_custom_event_node(blueprint_name, custom_event_name, node_position_json,
            signature_source_dispatcher_name, signature_source_class, signature_source_delegate_name,
            native_delegate_name, call_in_editor, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_input_key_node(blueprint_name: str, key: str, node_position_json: str = "[0,0]",
            control: bool = False, alt: bool = False, shift: bool = False, command: bool = False,
            consume_input: bool = True, execute_when_paused: bool = False, override_parent_binding: bool = False,
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create an InputKey event using a valid supported key and the explicit modifier/binding flags. Defaults: consume_input=true; control/alt/shift/command, execute_when_paused and override_parent_binding=false. Ordinary no-selector resolution reuses or bootstraps EventGraph. node_position_json is [x,y]. No input injection, explicit compile/save or automatic retry; a new operation_id may create another key node. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_input_key_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.events import add_blueprint_input_key_node
        return reflected_result(add_blueprint_input_key_node(blueprint_name, key, node_position_json,
            control, alt, shift, command, consume_input, execute_when_paused, override_parent_binding,
            graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_parent_function_call(blueprint_name: str, event_node_id: str, node_position_json: str = "[0,0]",
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create an explicit parent-call node for ReceiveTick in a child whose direct saved compiled Actor Blueprint parent declares its own Tick implementation and has native Actor ancestry. Other parent signatures/chains remain unsupported here. This is a new named extension, not a claimed UE56 public-tool migration. One existing exact parent-call is reused unchanged; ambiguous or mismatching nodes fail. No automatic wiring, compilation, save or fallback function. node_id/node_guid are GUIDs and object_path is the canonical path.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path or simple name under /Game/Blueprints.
            event_node_id: Existing event node object path or native nonzero GUID in the selected graph. Native rechecks the exact event/parent relation.
            node_position_json: JSON [x,y], default [0,0], with integers from -1000000 to 1000000; JSON null also means origin.
            graph_name: Optional existing event graph name.
            graph_id: Optional graph GUID agreeing with graph_name/type.
            graph_type: Optional selector; the resolved graph must be an event graph.
            create_graph_if_missing: False; True is currently unsupported.
            operation_id: Optional durable replay ID; unknown results require inspection, not automatic rerun.
        """
        from cubeless_mcp_ue58.operations.events import add_blueprint_parent_function_call
        return reflected_result(add_blueprint_parent_function_call(blueprint_name, event_node_id, node_position_json,
            graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def resolve_blueprint_graph(blueprint_name: str, graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, create_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Resolve an owned graph by agreeing name/GUID/type, or create a missing named function graph within supported Blueprint profiles when create_if_missing=true. The unnamed default EventGraph follows its legacy bootstrap rule even with false. Name collisions and ambiguity fail without silent renaming. Structural creation can regenerate the skeleton; no explicit full compile/save. Replayed operation_id returns the original observation. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="resolve_blueprint_graph", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.functions import resolve_blueprint_graph
        return reflected_result(resolve_blueprint_graph(blueprint_name, graph_name, graph_id, graph_type, create_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_function_parameter(blueprint_name: str, parameter_name: str, parameter_type: str,
            direction: str = "input", default_value_json: str | None = None, type_object: str | None = None,
            is_array: bool = False, graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str = "function", create_graph_if_missing: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Add a typed input/output parameter to a supported own function graph. parameter_type, type_object and is_array select a checked descriptor; default_value_json is encoded JSON. Primitive/native math/enum/object/class/UDS and BFL profiles have distinct limits, so consult details for the exact combination. Existing graph reuse and bounded missing-function creation retain source, signature, default and partial-change guards. No explicit full compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_function_parameter", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.functions import add_blueprint_function_parameter
        return reflected_result(add_blueprint_function_parameter(blueprint_name, parameter_name, parameter_type,
            direction, default_value_json, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_return_node(blueprint_name: str, node_position: str | None = None,
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str = "function",
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create or reuse a Return node in a supported own function graph. node_position is JSON [x,y]; omitted/null means [320,0]. Existing Return signatures must agree. The engine may connect Entry.then to Return.execute; this edge change is reported. create_graph_if_missing permits only bounded exact-name function creation. Created graph/node prefixes are retained on failure; no function execution or explicit full compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_return_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.functions import add_blueprint_return_node
        return reflected_result(add_blueprint_return_node(blueprint_name, node_position, graph_name, graph_id,
            graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_dynamic_cast_node(blueprint_name: str, target_class: str, node_position: str | None = None,
            pure: bool = False, graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, create_graph_if_missing: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create an audited native/generated-class DynamicCast node, pure or impure as requested. target_class must resolve to a supported runtime class; aliases, source loading and graph combinations have profile-specific limits. node_position is JSON [x,y]. Retains source/target/node/pin checks across callbacks and bounded graph creation. No cast execution, automatic wiring, explicit compile/save or uncertain retry. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_dynamic_cast_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.functions import add_blueprint_dynamic_cast_node
        return reflected_result(add_blueprint_dynamic_cast_node(blueprint_name, target_class, node_position,
            pure, graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_call_function_node(blueprint_name: str, function_class: str, function_name: str,
            param_defaults_json: str = "{}", node_position_json: str = "[0,0]", graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None, create_graph_if_missing: bool = False,
            allow_latent: bool = False, allow_editor_only: bool = False, allow_wildcard: bool = False,
            allow_deprecated: bool = False, allow_internal: bool = False,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create one CallFunction node for a supported audited native, generated Blueprint or CustomEvent/RPC member. function_class and function_name select the exact member; this is not arbitrary UFunction execution. param_defaults_json is an object with at most 64 supported pin defaults; collection/object/struct profiles may require typed wires. allow_* flags do not bypass owner, source, lifetime or runtime-dependency guards. Graph creation, partial changes and implicit structural effects are reported; no automatic wiring, function execution or explicit full compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_call_function_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.functions import add_blueprint_call_function_node
        return reflected_result(add_blueprint_call_function_node(blueprint_name, function_class, function_name,
            param_defaults_json, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
            allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_graph(blueprint_name: str, graph_name: str = "EventGraph") -> CubelessPythonOperationResult:
        """Read K2 node identities, pins, positions and links. Exact CustomEvent rows include the native custom_function_name, custom_event_function_flags_string and call_in_editor fields. node_id remains an object path; node_guid is a separate native GUID. Native observation caps total graph nodes at 2048 including comments; comment rows are excluded. No compile/save or forced reload.

        Args:
            blueprint_name: Full /Game Blueprint package/object path.
            graph_name: Existing graph name, defaults to EventGraph.
        """
        return reflected_result(graph.inspect_graph(blueprint_name, graph_name))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_branch_node(blueprint_name: str, node_position: list[float] = [], graph_name: str = "", graph_id: str = "", graph_type: str = "", create_graph_if_missing: bool = False) -> CubelessPythonOperationResult:
        'The original unnamed default selector additionally creates a missing empty EventGraph on an admitted idle direct-Actor Blueprint with no own UbergraphPages even when create=False. The original selector-free created=False is preserved separately from actual_event_graph_created/changed=True. Explicit event kind with empty name/no GUID requires create=True. The existing creation lease covers graph append and optional Branch/Sequence node writing, implicit skeleton regeneration and partial failures; explicit compile/save remain zero. Broader owner/topology/name selection and whole original scope remain incomplete; actual use pending.\n\nOriginal Branch6 authoring contract, separate from the retained add_branch_node x/y facade. node_position defaults to null/origin; finite two-number positions retain the original float32 conversion then safe int32 truncation toward zero, including fractions. Native preserves graph selectors and actual create/reuse/partial prefix. True first reuses an admitted existing graph and admits bounded missing-function creation on an exact normal direct-Actor host with exact function name/type and no GUID. Original string/extra-array/other Any coercions, broader graph creation and runtime branch evaluation remain incomplete. Implicit structural effects are separate from explicit full compile/save0; no edge, rollback or retry is synthesized.'
        position = [] if node_position is None else list(node_position)
        return reflected_result(graph.add_blueprint_branch_node(blueprint_name, position if position else None,
            graph_name, graph_id, graph_type, create_graph_if_missing))

    @toolset_registry.tool_call
    @staticmethod
    def add_branch_node(blueprint_name: str, graph_name: str = "EventGraph", x: int = 0, y: int = 0, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Add a Branch node and verify class/position/existence. data_json.node_id remains its object path and node_guid contains its native GUID.

        Args:
            blueprint_name: Full /Game Blueprint package/object path.
            graph_name: Existing graph name.
            x: Node horizontal position.
            y: Node vertical position.
            operation_id: Unique durable request ID; omitted/null generates an ID returned in the result.
        """
        return reflected_result(graph.add_branch_node(blueprint_name, graph_name, x, y, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_sequence_node(blueprint_name: str, node_position_json: str = "[0,0]", output_count: int = 2,
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            create_graph_if_missing: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create a stock K2 Sequence node with output_count 2..64 and JSON [x,y] position. Existing event/function/macro graphs must support execution flow. Default EventGraph bootstrap and bounded exact-name function creation retain their selector and Blueprint restrictions. Existing node/pin/link state is preserved; node_id/node_guid are GUIDs and object_path is separate. No wiring or explicit full compile/save; implicit structural changes and partial failures are reported. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_sequence_node", limit=1, verbosity="full").'
        return reflected_result(graph.add_blueprint_sequence_node(blueprint_name, node_position_json, output_count,
            graph_name, graph_id, graph_type, create_graph_if_missing, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_node_position(blueprint_name: str, node_id: str, x: int, y: int, graph_name: str = "EventGraph", expected_x: int | None = None, expected_y: int | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Move one K2 node, checking optional previous coordinates and reading back the final position; no compile/save.

        Args:
            blueprint_name: Full /Game Blueprint package/object path.
            node_id: Exact object-path node_id or nonzero node_guid returned by inspect_graph/add_branch_node. GUIDs resolve within the selected graph; comparisons use canonical object paths.
            x: New horizontal position.
            y: New vertical position.
            graph_name: Existing graph name.
            expected_x: If supplied, refuse when the prior x differs.
            expected_y: If supplied, refuse when the prior y differs.
            operation_id: Unique durable request ID; omitted/null generates an ID returned in the result.
        """
        return reflected_result(graph.set_node_position(blueprint_name, node_id, x, y, graph_name, expected_x, expected_y, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def list_blueprint_nodes(blueprint_name: str, node_type: str = "", title_contains: str = "", include_pins: bool = True, graph_name: str = "", graph_id: str = "", graph_type: str = "") -> CubelessPythonOperationResult:
        """List the selected graph through original public7, preserving node_type/title filters and include_pins=True. Native owns graph name/GUID/type resolution, case-insensitive class/title filtering and observed node/pin/link JSON. No Python filter, graph creation, deletion, compile or save is added. Getter/cache effects and broader node/graph/output coverage are limited to the actual Native current receipt. Original list fields remain in data.legacy_result with the whole typed result. Named observation has no mutation journal; Batch uses its parent journal. This is separate from inspect_graph and the existing delete_node method."""
        return reflected_result(blueprint_node_cleanup.list_blueprint_nodes(blueprint_name, node_type, title_contains, include_pins, graph_name, graph_id, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def delete_blueprint_node(blueprint_name: str, node_id: str, graph_name: str = "", graph_id: str = "", graph_type: str = "", expected_node_name: str = "", expected_node_class: str = "", expected_title_contains: str = "", allow_non_exec_linked_delete: bool = False, allow_exec_linked_delete: bool = False, allow_any_linked_delete: bool = False, allow_orphaned_event_delete: bool = False) -> CubelessPythonOperationResult:
        'Delete a supported K2 node after exact graph/identity and optional expected-name/class/title checks. Linked data/exec deletion requires the corresponding explicit allow_* opt-in; orphan events have a separate flag. Supported special nodes, including Timeline, retain source/template/reference and callback guards. Reports actual removed state and partial/unknown outcomes. No implicit compile/save or atomic rollback; inspect an uncertain result before further mutation. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="delete_blueprint_node", limit=1, verbosity="full").'
        return reflected_result(blueprint_node_cleanup.delete_blueprint_node(blueprint_name, node_id, graph_name, graph_id, graph_type, expected_node_name, expected_node_class, expected_title_contains, allow_non_exec_linked_delete, allow_exec_linked_delete, allow_any_linked_delete, allow_orphaned_event_delete))

    @toolset_registry.tool_call
    @staticmethod
    def delete_node(blueprint_name: str, node_id: str, graph_name: str = "EventGraph", allow_exec_linked_delete: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Delete a K2 node and verify absence plus no dangling links. Linked exec deletion requires explicit opt-in.

        Args:
            blueprint_name: Full /Game Blueprint package/object path.
            node_id: Exact object-path node_id or nonzero node_guid returned by this Toolset. Absence and dangling-link checks use the resolved object path.
            graph_name: Existing graph name.
            allow_exec_linked_delete: Permit removing a node with connected exec pins.
            operation_id: Unique durable request ID; omitted/null generates an ID returned in the result.
        """
        return reflected_result(graph.delete_node(blueprint_name, node_id, graph_name, allow_exec_linked_delete, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def connect_graph_pins(blueprint_name: str, source_node_id: str, source_pin_name: str, target_node_id: str, target_pin_name: str, graph_name: str = "EventGraph", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Connect output/input pins only when the public K2 schema permits a direct MAKE connection. Data-output fanout is permitted; exec-input convergence is permitted only from an empty exec output with a native MAKE decision and preservation of all previous edges. Data-input replacement and exec-output fanout are refused. Existing bidirectional links are no-ops. Verify both directions, unchanged node identities and preservation of every prior edge. Conversion/promotion nodes remain unsupported; direct float/double compatibility is decided by the engine schema.

        Args:
            blueprint_name: Full /Game Blueprint package/object path.
            source_node_id: Output node object-path node_id or native node_guid.
            source_pin_name: Exact output pin name, such as then.
            target_node_id: Input node object-path node_id or native node_guid.
            target_pin_name: Exact input pin name, such as execute.
            graph_name: Existing graph name.
            operation_id: Unique durable request ID; omitted/null generates an ID returned in the result.
        """
        return reflected_result(graph.connect_graph_pins(blueprint_name, source_node_id, source_pin_name, target_node_id, target_pin_name, graph_name, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def connect_blueprint_graph_pins(blueprint_name: str, source_node_id: str, source_pin: str,
            target_node_id: str, target_pin: str, graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, allow_pin_link_replacement: bool = False, compile: bool = False,
            save_asset: bool = False, expected_existing_source_link_count: int | None = None,
            expected_existing_source_link_target_node_id: str | None = None,
            expected_existing_source_link_target_pin: str | None = None, expected_schema_response: str | None = None,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Connect selected K2 output/input pins under the native schema and supported typed/source profiles. Existing links are preserved unless allow_pin_link_replacement and the requested preconditions explicitly permit replacement. expected_* fields guard stale observations. compile and save_asset are separate opt-ins; actual mutation, compile/save stages and partial failures are returned. No arbitrary conversion/promotion or automatic uncertain retry. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="connect_blueprint_graph_pins", limit=1, verbosity="full").'
        return reflected_result(graph.connect_blueprint_graph_pins(blueprint_name, source_node_id, source_pin,
            target_node_id, target_pin, graph_name, graph_id, graph_type, allow_pin_link_replacement,
            compile, save_asset, expected_existing_source_link_count, expected_existing_source_link_target_node_id,
            expected_existing_source_link_target_pin, expected_schema_response, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_variable_get_node(blueprint_name: str, variable_name: str, graph_name: str = "EventGraph",
            x: int = 0, y: int = 0, operation_id: str | None = None, target_class: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None, create_graph_if_missing: bool = False,
            node_position_json: str | None = None) -> CubelessPythonOperationResult:
        'Create a Get node for a supported own, inherited, local or audited native property. variable_name, target_class and graph selectors determine the exact owner; source/class/type/reference guards apply. node_position_json overrides legacy x/y only within the existing contract. Container, UDS and inherited profiles are bounded; consult details before combining them. Does not read runtime values, wire pins or explicitly compile/save. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_variable_get_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.graph_variables import add_blueprint_variable_get_node
        return reflected_result(add_blueprint_variable_get_node(blueprint_name, variable_name, graph_name, x, y, operation_id,
            target_class, graph_id, graph_type, create_graph_if_missing, node_position_json))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_variable_set_node(blueprint_name: str, variable_name: str, graph_name: str = "EventGraph",
            x: int = 0, y: int = 0, operation_id: str | None = None, target_class: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None, create_graph_if_missing: bool = False,
            node_position_json: str | None = None) -> CubelessPythonOperationResult:
        'Create a Set node for a supported own, inherited, local or audited native property. variable_name, target_class and graph selectors determine the exact owner; source/class/type/reference guards apply. node_position_json and legacy x/y retain their existing precedence. Container, UDS and native-property profiles are bounded. Does not set a live object value, wire pins or explicitly compile/save; partial node changes are reported. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="add_blueprint_variable_set_node", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.graph_variables import add_blueprint_variable_set_node
        return reflected_result(add_blueprint_variable_set_node(blueprint_name, variable_name, graph_name, x, y, operation_id,
            target_class, graph_id, graph_type, create_graph_if_missing, node_position_json))

    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_pin_default(blueprint_name: str, node_id: str, pin_name: str, value_json: str,
            direction: str | None = None, graph_name: str = "EventGraph", graph_id: str | None = None,
            graph_type: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Set an unlinked supported input pin default from value_json, preserving native type/source and surrounding graph state. direction and graph name/GUID/type must agree with the selected pin. Primitive, native math, exact enum/property and supported passive-node profiles have distinct rules; arbitrary object/container literals are not accepted. Callback changes produce actual partial/unknown receipts. No runtime evaluation, compile, save or automatic retry. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="set_blueprint_pin_default", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.pin_defaults import set_blueprint_pin_default
        return reflected_result(set_blueprint_pin_default(blueprint_name, node_id, pin_name, value_json,
            "" if direction is None else direction, graph_name, graph_id, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_make_struct_node(blueprint_name: str, struct_type: str, node_position: list[int] = [],
            graph_name: str = "", graph_id: str = "", graph_type: str = "", create_graph_if_missing: bool = False) -> CubelessPythonOperationResult:
        """Create one native KismetMath MakeVector, MakeRotator or MakeTransform call.

        Original seven inputs/defaults remain. node_position is null or a numeric
        array; the finite supported slice requires exactly two integral signed
        int32 coordinates. Existing owned event/function/macro graph selectors
        use the shared guarded resolver. True create_graph_if_missing preserves
        existing-graph reuse and admits bounded missing-function creation on a
        normal direct-Actor host with exact function name/type and no graph GUID.
        Actual created/reused and implicit structural compilation prefixes are
        Native-owned; node failure does not erase a created graph. Original
        loose string/fractional/extra-element position coercions, arbitrary
        structures and broader Blueprint classes/graphs remain incomplete.
        The exact runtime Engine member/pins and prior graph state are checked.
        No defaults parameter, explicit compile/save or runtime value execution.
        Partial native prefix is recorded before failure; never resubmit to retry.
        """
        # Direct FArray reflection avoids stock Optional-array splitting. Exact
        # converter publishes nullable/defaultNull; empty buffer from omission/null
        # and explicit[] maps to the original helper origin, proven in Utils1193-1227.
        position = [] if node_position is None else list(node_position)
        return reflected_result(blueprint_make_struct.add_blueprint_make_struct_node(blueprint_name, struct_type,
            position if position else None, graph_name, graph_id, graph_type, create_graph_if_missing))


    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_custom_event_metadata(blueprint_name: str, custom_event_name: str = "", description: str = "",
            node_id: str = "", event_name: str = "", tooltip: str = "", call_in_editor: bool | None = None,
            set_node_comment: bool = True, compile: bool = True, save: bool = False) -> CubelessPythonOperationResult:
        'Edit the selected CustomEvent description/comment and optional CallInEditor flag, preserving references, authored graph state and unrelated metadata. custom_event_name/description are primary fields; event_name/tooltip are aliases with existing omission semantics. call_in_editor=None preserves the flag; false requests clearing. compile defaults true; save defaults false. Returns actual mutation/compile/save outcomes, including post-save failures; no rollback or uncertain retry. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.graph_toolset.CubelessBlueprintGraphTools", query="set_blueprint_custom_event_metadata", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.custom_event_metadata import set_blueprint_custom_event_metadata
        return reflected_result(set_blueprint_custom_event_metadata(blueprint_name,custom_event_name,description,
            node_id,event_name,tooltip,call_in_editor,set_node_comment,compile,save))
