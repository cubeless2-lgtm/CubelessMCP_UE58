"""Original AnimGraph authoring10/7 IO; Native owns all partial mutations."""
import copy
from . import anim_graph_bone_demo_schema as _bone_demo_schema
from . import anim_graph_modify_curve_demo_schema as _curve_demo_schema
from . import anim_graph_trail_demo_schema as _trail_demo_schema
from . import anim_graph_secondary_motion_schema as _motion_schema
from . import anim_graph_controlrig_schema as _rig_schema
from . import controlrig_builders_schema as _builders_schema
from . import anim_blueprint_sample_schema as _sample_schema

TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('set_anim_graph_rigidbody_settings', 'ensure_anim_graph_input_pose_passthrough')
PUBLIC_FIELDS_BY_METHOD = {'set_anim_graph_rigidbody_settings': ('blueprint_name', 'node_id', 'alpha', 'external_force', 'simulation_space', 'enable_world_geometry', 'allow_non_sample', 'graph_name', 'graph_id', 'graph_type'), 'ensure_anim_graph_input_pose_passthrough': ('blueprint_name', 'graph_name', 'graph_id', 'graph_type', 'replace_existing', 'input_node_position', 'allow_non_sample')}
DEFAULTS_BY_METHOD = {'set_anim_graph_rigidbody_settings': {'node_id': '', 'alpha': None, 'external_force': None, 'simulation_space': '', 'enable_world_geometry': None, 'allow_non_sample': False, 'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function'}, 'ensure_anim_graph_input_pose_passthrough': {'graph_name': 'AnimGraph', 'graph_id': '', 'graph_type': 'function', 'replace_existing': False, 'input_node_position': None, 'allow_non_sample': False}}
REQUIRED_BY_METHOD = {'set_anim_graph_rigidbody_settings': ('blueprint_name',), 'ensure_anim_graph_input_pose_passthrough': ('blueprint_name',)}
FIELD_KINDS_BY_METHOD = {'set_anim_graph_rigidbody_settings': {'blueprint_name': 'string', 'node_id': 'string', 'alpha': 'any', 'external_force': 'any', 'simulation_space': 'string', 'enable_world_geometry': 'any', 'allow_non_sample': 'boolean', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string'}, 'ensure_anim_graph_input_pose_passthrough': {'blueprint_name': 'string', 'graph_name': 'string', 'graph_id': 'string', 'graph_type': 'string', 'replace_existing': 'boolean', 'input_node_position': 'any', 'allow_non_sample': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.set_anim_graph_rigidbody_settings', 'description': 'Original RigidBody10 preserves untyped optional alpha/external_force/enable_world_geometry; public None omits those fields, supplied values are whole JSON. Native preserves literal GUID comparison and original setting/default callback order; unsupported simulation_space may fail after earlier writes. Full actual current/partial/known-null prefix is authoritative. No wrapper setting conversion, rollback, replay, compiler/save or runtime/cold/full pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'node_id': {'type': 'string', 'default': ''}, 'alpha': {'default': None}, 'external_force': {'default': None}, 'simulation_space': {'type': 'string', 'default': ''}, 'enable_world_geometry': {'default': None}, 'allow_non_sample': {'type': 'boolean', 'default': False}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}, {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.ensure_anim_graph_input_pose_passthrough', 'description': 'Original InputPose7 preserves selectors/replace_existing and untyped input_node_position; public None becomes [-320,0], other whole JSON remains untouched and Native projects position only when creating a node. Reuse/create, late occupied-root refusal and break-before-schema-admission retain actual node/link prefix without cleanup or rollback. Full reciprocal/current/source proof is Native-owned. No wrapper compiler/save, replay or runtime/cold/full pipeline proof.', 'inputSchema': {'type': 'object', 'properties': {'blueprint_name': {'type': 'string'}, 'graph_name': {'type': 'string', 'default': 'AnimGraph'}, 'graph_id': {'type': 'string', 'default': ''}, 'graph_type': {'type': 'string', 'default': 'function'}, 'replace_existing': {'type': 'boolean', 'default': False}, 'input_node_position': {'default': None}, 'allow_non_sample': {'type': 'boolean', 'default': False}}, 'required': ['blueprint_name'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    schema = copy.deepcopy(_PUBLIC_SCHEMA)
    schema["tools"].extend(_bone_demo_schema.public_schema()["tools"])
    schema["tools"].extend(_curve_demo_schema.public_schema()["tools"])
    schema["tools"].extend(_trail_demo_schema.public_schema()["tools"])
    schema["tools"].extend(_motion_schema.public_schema()["tools"])
    schema["tools"].extend(_rig_schema.public_schema()["tools"])
    schema["tools"].extend(_builders_schema.public_schema()["tools"])
    schema["tools"].extend(_sample_schema.public_schema()["tools"])
    return schema



def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_graph_authoring as api
    methods = api.binding_status()
    original_ready = set(methods) == set(BINDINGS) and all(methods.values())
    bone = _bone_demo_schema.binding_status()
    methods.update(bone["methods"])
    curve = _curve_demo_schema.binding_status()
    methods.update(curve["methods"])
    trail = _trail_demo_schema.binding_status()
    methods.update(trail["methods"])
    motion = _motion_schema.binding_status()
    methods.update(motion["methods"])
    rig = _rig_schema.binding_status()
    methods.update(rig["methods"])
    builders = _builders_schema.binding_status()
    methods.update(builders["methods"])
    sample = _sample_schema.binding_status()
    methods.update(sample["methods"])
    return {"ready": original_ready and bone["ready"] and curve["ready"] and trail["ready"] and motion["ready"] and rig["ready"] and builders["ready"] and sample["ready"], "library": api.LIBRARY, "libraries": [api.LIBRARY, bone["library"], curve["library"], trail["library"], motion["library"], rig["library"], builders["library"], sample["library"]], "methods": methods,
        "public_toolset": TOOLSET, "public_tool_count": 11,
        "public_input_counts": {**{m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS}, **bone["public_input_counts"], **curve["public_input_counts"], **trail["public_input_counts"], **motion["public_input_counts"], **rig["public_input_counts"], **builders["public_input_counts"], **sample["public_input_counts"]},
        "bone_demo": bone, "modify_curve_demo": curve, "trail_demo": trail, "secondary_motion": motion, "controlrig": rig, "controlrig_builders": builders, "anim_blueprint_sample": sample, "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_late_setting_node_and_pose_link_failures_native_owned": True,
        "public_None_omission_and_private_supplied_null_distinct": True,
        "actual_anim_graph_authoring_behavior_verified": False, "full_original_anim_graph_authoring_runtime_or_pipeline_verified": False}



def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 11):
        raise RuntimeError("Exact anim_graph_authoring raw11 schema required")
    expected_names = {TOOLSET + "." + m for m in BINDINGS + _bone_demo_schema.BINDINGS + _curve_demo_schema.BINDINGS + _trail_demo_schema.BINDINGS + _motion_schema.BINDINGS + _rig_schema.BINDINGS + _builders_schema.BINDINGS + _sample_schema.BINDINGS}
    if any(type(row) is not dict for row in schema["tools"]) or {row.get("name") for row in schema["tools"]} != expected_names:
        raise RuntimeError("Exact original AnimGraphAuthoring10/7/8/9/19/19/12/15/13/20/10 names required")
    for method in BINDINGS:
        rows = [row for row in schema["tools"] if type(row) is dict and row.get("name") == TOOLSET + "." + method]
        if len(rows) != 1:
            raise RuntimeError("AnimGraphAuthoring method missing or duplicated: " + method)
        expected = next(row for row in _PUBLIC_SCHEMA["tools"] if row["name"] == TOOLSET + "." + method)
        inputs = rows[0].get("inputSchema")
        properties = inputs.get("properties") if type(inputs) is dict else None
        if (type(inputs) is not dict or inputs.get("type") != "object" or inputs.get("additionalProperties") is not False
                or inputs.get("required") != list(REQUIRED_BY_METHOD[method]) or type(properties) is not dict
                or tuple(properties) != PUBLIC_FIELDS_BY_METHOD[method]):
            raise RuntimeError("Original anim_graph_authoring input order/envelope/required changed")
        for field in PUBLIC_FIELDS_BY_METHOD[method]:
            item = properties[field]
            if type(item) is not dict or item != expected["inputSchema"]["properties"][field]:
                raise RuntimeError("Original anim_graph_authoring scalar type/default changed: " + field)
            if field in DEFAULTS_BY_METHOD[method] and type(item.get("default")) is not type(DEFAULTS_BY_METHOD[method][field]):
                raise RuntimeError("Original anim_graph_authoring default type changed: " + field)
        output = rows[0].get("outputSchema")
        outer = output.get("properties") if type(output) is dict else None
        if (type(output) is not dict or output.get("type") != "object" or output.get("additionalProperties") is not False
                or output.get("required") != ["returnValue"] or type(outer) is not dict or set(outer) != {"returnValue"}):
            raise RuntimeError("Exact anim_graph_authoring operation outer envelope required")
        inner = outer["returnValue"]
        fields = inner.get("properties") if type(inner) is dict else None
        if (type(inner) is not dict or inner.get("type") != "object" or inner.get("additionalProperties") is not False
                or inner.get("required") != list(RESULT_FIELDS) or type(fields) is not dict or set(fields) != set(RESULT_FIELDS)
                or any(type(fields[k]) is not dict or fields[k].get("type") != ("boolean" if k == "success" else "string") for k in RESULT_FIELDS)):
            raise RuntimeError("Exact standard anim_graph_authoring operation output11 required")
    bone_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _bone_demo_schema.BINDINGS}]}
    bone_validation = _bone_demo_schema.validate_schema(bone_schema)
    curve_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _curve_demo_schema.BINDINGS}]}
    curve_validation = _curve_demo_schema.validate_schema(curve_schema)
    trail_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _trail_demo_schema.BINDINGS}]}
    trail_validation = _trail_demo_schema.validate_schema(trail_schema)
    motion_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _motion_schema.BINDINGS}]}
    motion_validation = _motion_schema.validate_schema(motion_schema)
    rig_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _rig_schema.BINDINGS}]}
    rig_validation = _rig_schema.validate_schema(rig_schema)
    builders_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _builders_schema.BINDINGS}]}
    builders_validation = _builders_schema.validate_schema(builders_schema)
    sample_schema = {**schema, "tools": [row for row in schema["tools"] if row["name"] in {TOOLSET + "." + m for m in _sample_schema.BINDINGS}]}
    sample_validation = _sample_schema.validate_schema(sample_schema)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {**{m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS}, **bone_validation["public_input_counts"], **curve_validation["public_input_counts"], **trail_validation["public_input_counts"], **motion_validation["public_input_counts"], **rig_validation["public_input_counts"], **builders_validation["public_input_counts"], **sample_validation["public_input_counts"]},
        "original_ModifyBone8_defaults_and_standard11_verified": True,
        "original_ModifyCurve9_nullable_number_map_defaults_and_standard11_verified": True,
        "original_Trail19_Any_vector_float_defaults_and_standard11_verified": True,
        "original_SecondaryMotion19_nullable_lists_compile_save_and_standard11_verified": True,
        "original_ControlRig12_forced15_nullable_collections_defaults_and_standard11_verified": True,
        "original_ControlRigBuilder13_20_nullable_lists_limits_compile_save_and_standard11_verified": True,
        "original_AnimBlueprintSample10_paths_compile_save_and_standard11_verified": True,
        "original10_7_optional_Any_and_input_pose_defaults_verified": True, "preparation_required": False,
        "actual_official_raw_wire_verified": False, "actual_anim_graph_authoring_behavior_or_pipeline_verified": False}
