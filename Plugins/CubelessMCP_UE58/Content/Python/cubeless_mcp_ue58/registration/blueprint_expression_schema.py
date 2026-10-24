"""Pure exact public raw11 schema; each native library readiness is a separate gate."""
import copy

TOOLSET = "CubelessMCPUE58Editor.CubelessBlueprintExpressionTools"
CLASS = "CubelessBlueprintExpressionTools"
EXPRESSION_BINDINGS = ("add_blueprint_math_node", "add_blueprint_boolean_node")
BINDINGS = EXPRESSION_BINDINGS + ("add_blueprint_compare_node", "add_blueprint_literal_node", "add_blueprint_get_subsystem_from_pc_node", "add_blueprint_construct_object_node", "add_blueprint_input_action_node", "add_blueprint_self_reference", "add_blueprint_get_editor_subsystem_node", "add_blueprint_input_axis_event_node", "add_blueprint_enhanced_input_action_node")
PUBLIC_FIELDS_BY_METHOD = {method: ("blueprint_name", "operation", "node_position", "graph_name", "graph_id", "graph_type", "create_graph_if_missing") for method in EXPRESSION_BINDINGS}
DEFAULTS_BY_METHOD = {method: {"node_position": None, "graph_name": "", "graph_id": "", "graph_type": "", "create_graph_if_missing": False} for method in EXPRESSION_BINDINGS}
REQUIRED_BY_METHOD = {method: ("blueprint_name", "operation") for method in EXPRESSION_BINDINGS}
from . import blueprint_value_schema as _value_schema
PUBLIC_FIELDS_BY_METHOD.update(_value_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_value_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_value_schema.REQUIRED_BY_METHOD)
from . import blueprint_construction_schema as _construction_schema
PUBLIC_FIELDS_BY_METHOD.update(_construction_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_construction_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_construction_schema.REQUIRED_BY_METHOD)
from . import blueprint_input_self_schema as _input_self_schema
PUBLIC_FIELDS_BY_METHOD.update(_input_self_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_input_self_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_input_self_schema.REQUIRED_BY_METHOD)
from . import blueprint_editor_subsystem_schema as _editor_subsystem_schema
PUBLIC_FIELDS_BY_METHOD.update(_editor_subsystem_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_editor_subsystem_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_editor_subsystem_schema.REQUIRED_BY_METHOD)
from . import blueprint_input_events_schema as _input_events_schema
PUBLIC_FIELDS_BY_METHOD.update(_input_events_schema.PUBLIC_FIELDS_BY_METHOD)
DEFAULTS_BY_METHOD.update(_input_events_schema.DEFAULTS_BY_METHOD)
REQUIRED_BY_METHOD.update(_input_events_schema.REQUIRED_BY_METHOD)
RESULT_FIELDS = ("success", "operation_id", "stage", "error_code", "error", "changes_json",
    "compile_json", "save_json", "validation_json", "retry_json", "data_json")
MAX_ARGUMENT_BYTES = 131072
MAX_RESULT_BYTES = 16 * 1024 * 1024


def public_schema():
    rows = []
    for method in BINDINGS:
        if method in _input_events_schema.BINDINGS:
            rows.extend(row for row in _input_events_schema.public_rows() if row["name"] == TOOLSET + "." + method)
            continue
        properties = {}
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            schema = {} if field in ("node_position", "value") else {"type": "boolean" if field == "create_graph_if_missing" else "string"}
            if field in DEFAULTS_BY_METHOD[method]:
                schema["default"] = copy.deepcopy(DEFAULTS_BY_METHOD[method][field])
            properties[field] = schema
        rows.append({"name": TOOLSET + "." + method,
            "description": (("Original typed expression helper; finite public Engine node creation, no explicit compile/save or replay."
                if method in EXPRESSION_BINDINGS else "Original Compare/Literal helper; actual typed Engine CallFunction/default/retirement prefix, no explicit compile/save or replay. Missing MakeLiteralFloat uses Engine MathLibrary WeightedMovingAverage_Float with identical CurrentSample/PreviousSample and Weight=1; actual float pins differ, no double substitution or full parity." if method in _value_schema.BINDINGS else 'ConstructObject additionally admits up to8 already-loaded saved-clean Game generated UObject Blueprint hierarchy levels through existing per-source leases. Generated classes use actual generated parent roles while skeleton classes use the actual parent skeleton roles. Own and inherited primitive/one-level primitive Array fields, borrowed Inner descriptors, effective leaf CDO values and each parent original CDO are independently protected at existing SDK boundaries. Existing node/default/reconstruction/restore/retirement/partial writers are reused, with no source loading, source compile/save, CDO construction, runtime object creation or new transport. Wider classes/properties/owners, live runtime initialization and untested hierarchy variants remain incomplete or usage pending. Bounded saved-clean UObject Blueprint hierarchy sources additionally admit one-level primitive Array fields and exact native Vector/Rotator/Transform scalar fields. Existing generated/skeleton/CDO fields, borrowed Inner role/layout, held native value sources, authored pin types and SDK callback/retirement/partial checks remain. Exposed runtime initialization and wider complex/object/struct arrays remain incomplete or usage pending. For the detailed already-loaded saved-clean generated UObject ConstructObject path, the target Blueprint additionally admits held loaded Engine-native Actor parent chains including Pawn/Character, with existing CDO and current class/super/package/graph roles. This does not extend native requested-class or SubsystemFromPC owner support. Actual new-node/pin reconstruction/retirement and partial receipts remain unchanged; source or parent loading, CDO creation, explicit compile/save, runtime construction, custom/generated owners and full original parity remain incomplete or usage pending. Original construct-object helper7 with whole JSON Any/defaultNull position and literal class path. Alongside existing native classes, Native admits the bounded already-loaded saved-clean bounded generated Blueprint hierarchy ending at native UObject and its supported primitive ExposeOnSpawn pins. Actual class/skeleton/CDO/source, typed Return, checked Class default/reconstruction and only-new-node retirement receipts remain authoritative. No runtime construction, explicit compile/save, source loading or automatic replay; broader generated inheritance beyond the bounded hierarchy, properties, owners and cold/runtime pipeline parity remain incomplete.' if method == _construction_schema.BINDINGS[1] else "Original subsystem-from-controller/object-construction helper; specialized Engine nodes, typed class/default/restoration/retirement prefix, no runtime getter/spawn or explicit compile/save/replay. EditorSubsystem/EditorUtility remains a separate unfinished API." if method in _construction_schema.BINDINGS else ("Original legacy mapped InputAction event constructor; literal empty or unknown action names remain admitted and may produce SDK compile warnings. Actual Engine event pins and partial authoring receipt only; no EnhancedInput replacement, input mapping mutation, explicit compile/save, runtime/hardware or full pipeline claim." if method == _input_self_schema.BINDINGS[0] else "Original pure Engine Self node constructor; current-owner self pin and partial authoring receipt only. No runtime object evaluation, explicit compile/save, cleanup/replay or full owner/runtime/pipeline claim. Also admits the bounded already-loaded exact direct-UserWidget WidgetBlueprint profile with a bounded native Canvas/Button/Text authored and compiled WidgetTree and existing EventGraph, including dirty hosts. Existing mature tree observation/current-role guards are reused; authored values and ordered slots/children are preserved while compiled widget objects may be reacquired. The stock output remains PC_Object/PSC_Self with a null subtype object and actual pin name self; this is an authored self role, not a returned live Widget instance. Missing Widget graphs and broader Widget/runtime state remain incomplete.")) if method not in _editor_subsystem_schema.BINDINGS else 'Original EditorSubsystem getter constructor for a legitimate EditorUtility Blueprint; specialized pure Engine node with typed EditorSubsystem result and actual partial authoring receipt. Intentional Blutility/UnrealEd editor-only references are not game asset equivalence. No utility Run, subsystem method invocation, explicit compile/save, retry or full editor workflow claim. Additionally admits the exact native EditorUtilityWidgetBlueprint/EditorUtilityWidget owner with an already-existing empty authored WidgetTree, no root/owned widget exports and no existing preview. Current tree/class/graph and native preview slot roles are protected at SDK boundaries without creating a CDO, widget or preview. Existing UtilityObject behavior and original writer/compile-save0 are retained. Populated trees and wider generated/custom owners remain incomplete; actual new Widget usage is implemented / usage validation pending.'),
            "inputSchema": {"type": "object", "properties": properties, "required": list(REQUIRED_BY_METHOD[method]), "additionalProperties": False},
            "outputSchema": {"type": "object", "properties": {"returnValue": {"type": "object", "properties": {
                field: {"type": "boolean" if field == "success" else "string"} for field in RESULT_FIELDS},
                "required": list(RESULT_FIELDS), "additionalProperties": False}}, "required": ["returnValue"], "additionalProperties": False}})
    return {"name": TOOLSET, "version": "1.0", "tools": rows}


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import blueprint_expression as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(EXPRESSION_BINDINGS) and all(methods.values()), "library": api.LIBRARY, "methods": methods,
        "public_toolset": TOOLSET, "preparation_required": False, "reflected_Graph_class_modified": False,
        "schema_verified": False, "actual_official_raw_wire_verified": False, "actual_node_construction_verified": False,
        "full_original_runtime_or_mixed_pipeline_verified": False, "public_tool_count": 2, "public_input_counts": {BINDINGS[0]: 7, BINDINGS[1]: 7}}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 11):
        raise RuntimeError("Exact actual expression raw11 schema required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("Expression method name missing or duplicated: " + method)
        if method in _input_events_schema.BINDINGS:
            _input_events_schema.validate_row(rows[0], method)
            continue
        inputs = rows[0].get("inputSchema")
        fields = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or type(fields) is not dict or tuple(fields) != PUBLIC_FIELDS_BY_METHOD[method]
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method])):
            raise RuntimeError("Original expression input set/order changed: " + method)
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = fields[field]
            if type(item) is not dict:
                raise RuntimeError("Expression field schema missing")
            if field in ("node_position", "value"):
                if any(k in item for k in ("type", "anyOf", "oneOf", "allOf", "enum", "items", "$ref")):
                    raise RuntimeError("Original JSON Any value/position was narrowed")
            elif item.get("type") != ("boolean" if field == "create_graph_if_missing" else "string"):
                raise RuntimeError("Original expression field type changed: " + field)
            if field in DEFAULTS_BY_METHOD[method]:
                expected = DEFAULTS_BY_METHOD[method][field]
                if "default" not in item or type(item["default"]) is not type(expected) or item["default"] != expected:
                    raise RuntimeError("Original expression default changed: " + field)
            elif "default" in item:
                raise RuntimeError("Original required expression field was defaulted")
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact standard operation outer envelope required")
        returned = outer["returnValue"]
        fields = returned.get("properties") if type(returned) is dict else None
        if (type(returned) is not dict or returned.get("type") != "object" or returned.get("additionalProperties") is not False
                or returned.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)):
            raise RuntimeError("Exact reflected operation output11 required")
        if any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS):
            raise RuntimeError("Expression operation output field type changed")
    return {"schema_verified": True, "original_public_defaults_verified": True, "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method]) for method in BINDINGS},
        "public_position_Any_wire_schema_verified": True, "actual_official_raw_wire_verified": False,
        "actual_node_construction_or_runtime_verified": False, "preparation_required": False}
