"""Original ControlRig12/forced15 projected subset of fixed AuthoringTools."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('set_anim_graph_controlrig_input_defaults', 'ensure_controlrig_forced_driver_animbp')
PUBLIC_FIELDS_BY_METHOD = {'set_anim_graph_controlrig_input_defaults': ('blueprint_name', 'input_defaults', 'expose_input_pins', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_name', 'title_contains', 'control_rig_class', 'disconnect_existing_links', 'allow_non_sample'), 'ensure_controlrig_forced_driver_animbp': ('blueprint_name', 'curve_values', 'input_defaults', 'alpha', 'apply_mode', 'graph_name', 'graph_id', 'graph_type', 'node_id', 'node_name', 'title_contains', 'control_rig_class', 'replace_existing', 'disconnect_existing_links', 'allow_non_sample')}
DEFAULTS_BY_METHOD = {'set_anim_graph_controlrig_input_defaults': {'input_defaults': None, 'expose_input_pins': None, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'node_id': '', 'node_name': '', 'title_contains': '', 'control_rig_class': '', 'disconnect_existing_links': False, 'allow_non_sample': False}, 'ensure_controlrig_forced_driver_animbp': {'curve_values': None, 'input_defaults': None, 'alpha': 1.0, 'apply_mode': 'Add', 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'node_id': '', 'node_name': '', 'title_contains': '', 'control_rig_class': '', 'replace_existing': True, 'disconnect_existing_links': True, 'allow_non_sample': False}}
REQUIRED_BY_METHOD = {'set_anim_graph_controlrig_input_defaults': ('blueprint_name',), 'ensure_controlrig_forced_driver_animbp': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'set_anim_graph_controlrig_input_defaults': {'blueprint_name': 'string', 'input_defaults': 'nullable_any_map', 'expose_input_pins': 'nullable_string_list', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'node_id': 'string', 'node_name': 'string', 'title_contains': 'string', 'control_rig_class': 'string', 'disconnect_existing_links': 'boolean', 'allow_non_sample': 'boolean'}, 'ensure_controlrig_forced_driver_animbp': {'blueprint_name': 'string', 'curve_values': 'nullable_number_map', 'input_defaults': 'nullable_any_map', 'alpha': 'number', 'apply_mode': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'node_id': 'string', 'node_name': 'string', 'title_contains': 'string', 'control_rig_class': 'string', 'replace_existing': 'boolean', 'disconnect_existing_links': 'boolean', 'allow_non_sample': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.set_anim_graph_controlrig_input_defaults', 'description': 'Original ControlRig input-default12 preserves nullable Any map and string list, including None plus falsey expose conditional default2 and omitted-vs-explicit-empty collections. Native owns original candidate filters, UE58 public asset-reference class path, CustomPin visibility, unconditional Reconstruct, optional broken links and semantic/default callbacks. Reconstruct can construct/initialize/evaluate transient rig and preload modular dependencies; no CDO-only/no-evaluation assumption. Actual known/unknown mutation and full late prefix precede criteria; no consumer compiler/save, pin/name coercion, rollback/replay, runtime/cold/full-pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'input_defaults': {'type': ['object', 'null'], 'additionalProperties': True, 'default': None}, 'expose_input_pins': {'type': ['array', 'null'], 'items': {'type': 'string'}, 'default': None}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'node_id': {'type': 'string', 'default': ''}, 'node_name': {'type': 'string', 'default': ''}, 'title_contains': {'type': 'string', 'default': ''}, 'control_rig_class': {'type': 'string', 'default': ''}, 'disconnect_existing_links': {'type': 'boolean', 'default': False}, 'allow_non_sample': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_controlrig_forced_driver_animbp', 'description': 'Original forced ControlRig driver15 preserves None curve/input maps default2, explicit empty maps, Alpha1.0/Add/replaceTrue/disconnectTrue and original filters. Native owns ModifyCurve arrays/map/pins/default callbacks, ordered upstream-to-curve-to-ControlRig links and ControlRig exposed/default inputs. Earlier nodes/settings/broken links and indirect Reconstruct rig construction/evaluation/preloads retain complete current/partial prefix. UE58 public asset-reference class projection repairs old class lookup. No second compiler/save, Python alpha clamp/coercion, cleanup/rollback/replay or runtime/cold/full-pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'curve_values': {'type': ['object', 'null'], 'additionalProperties': {'type': 'number'}, 'default': None}, 'input_defaults': {'type': ['object', 'null'], 'additionalProperties': True, 'default': None}, 'alpha': {'type': 'number', 'default': 1.0}, 'apply_mode': {'type': 'string', 'default': 'Add'}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'node_id': {'type': 'string', 'default': ''}, 'node_name': {'type': 'string', 'default': ''}, 'title_contains': {'type': 'string', 'default': ''}, 'control_rig_class': {'type': 'string', 'default': ''}, 'replace_existing': {'type': 'boolean', 'default': True}, 'disconnect_existing_links': {'type': 'boolean', 'default': True}, 'allow_non_sample': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)


def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_controlrig as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 2,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_Set12_conditional_defaults_and_omission_preserved": True,
        "original_forced15_None_maps_default2_preserved": True,
        "Native_UE58_asset_reference_projection_and_indirect_Reconstruct_effects_authoritative": True,
        "actual_behavior_runtime_or_full_pipeline_verified": False}


def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 2):
        raise RuntimeError("Exact existing AuthoringTools projected two-tool ControlRig schema required")
    if [row.get("name") if type(row) is dict else None for row in schema["tools"]] != [TOOLSET + "." + m for m in BINDINGS]:
        raise RuntimeError("Original ControlRig12/forced15 ordered method names required")
    for method, row, expected in zip(BINDINGS, schema["tools"], _PUBLIC_SCHEMA["tools"]):
        if row.get("inputSchema") != expected["inputSchema"] or row.get("outputSchema") != expected["outputSchema"]:
            raise RuntimeError("Original ControlRig collections/defaults and standard11 IO differ")
        inputs = row["inputSchema"]
        output = row["outputSchema"]
        inner = output["properties"]["returnValue"]
        if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method]
                or inputs.get("additionalProperties") is not False
                or output.get("additionalProperties") is not False
                or inner.get("additionalProperties") is not False):
            raise RuntimeError("Original ControlRig ordered fields and exact object flags differ")
        for field, value in DEFAULTS_BY_METHOD[method].items():
            actual = inputs["properties"][field].get("default")
            if type(actual) is not type(value) or actual != value:
                raise RuntimeError("Original ControlRig default type/value differs: " + field)
        if inputs["properties"]["input_defaults"].get("additionalProperties") is not True:
            raise RuntimeError("Original ControlRig nullable Any map must preserve arbitrary JSON members")
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS}, "preparation_required": False}
