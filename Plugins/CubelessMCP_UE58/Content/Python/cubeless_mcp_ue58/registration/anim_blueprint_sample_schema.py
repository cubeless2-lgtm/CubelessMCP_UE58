"""Original AnimBlueprintSample10 projected singleton of existing AuthoringTools Raw11."""
import copy
TOOLSET = 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools'
BINDINGS = ('create_anim_blueprint_sample',)
PUBLIC_FIELDS_BY_METHOD = {'create_anim_blueprint_sample': ('target_anim_bp_path', 'skeleton_path', 'skeletal_mesh_path', 'preview_skeletal_mesh_path', 'parent_class_path', 'overwrite_existing', 'compile', 'save_asset', 'dry_run', 'validate_only')}
DEFAULTS_BY_METHOD = {'create_anim_blueprint_sample': {'skeleton_path': '', 'skeletal_mesh_path': '', 'preview_skeletal_mesh_path': '', 'parent_class_path': '/Script/Engine.AnimInstance', 'overwrite_existing': False, 'compile': True, 'save_asset': False, 'dry_run': False, 'validate_only': False}}
REQUIRED_BY_METHOD = {'create_anim_blueprint_sample': ('target_anim_bp_path',)}
FIELD_KINDS_BY_METHOD = {'create_anim_blueprint_sample': {'target_anim_bp_path': 'string', 'skeleton_path': 'string', 'skeletal_mesh_path': 'string', 'preview_skeletal_mesh_path': 'string', 'parent_class_path': 'string', 'overwrite_existing': 'boolean', 'compile': 'boolean', 'save_asset': 'boolean', 'dry_run': 'boolean', 'validate_only': 'boolean'}}
RESULT_FIELDS = ('success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json')
_PUBLIC_SCHEMA = {'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools', 'version': '1.0', 'tools': [{'name': 'CubelessMCPUE58Editor.CubelessAnimGraphAuthoringTools.create_anim_blueprint_sample', 'description': 'Original Animation Blueprint sample creator10 preserves target/source paths and parent default, overwriteFalse/compileTrue/saveFalse/dryFalse/validateFalse. Native owns sample/temp scope, Skeleton/Mesh/Preview/Parent identity/source guards, explicit overwrite delete/create prefix, configured Animation Blueprint factory, requested full compile and original forced save even after compile failure. Factory implicit Blueprint compile is separate SDK work; dry/validate can load/validate sources before target mutation. Full target/current/partial/compiler/save result retained before callback/errors; no wrapper second calls, path normalization/coercion, retry/rollback or runtime/cold/full-pipeline claim.', 'inputSchema': {'type': 'object', 'properties': {'target_anim_bp_path': {'type': 'string'}, 'skeleton_path': {'type': 'string', 'default': ''}, 'skeletal_mesh_path': {'type': 'string', 'default': ''}, 'preview_skeletal_mesh_path': {'type': 'string', 'default': ''}, 'parent_class_path': {'type': 'string', 'default': '/Script/Engine.AnimInstance'}, 'overwrite_existing': {'type': 'boolean', 'default': False}, 'compile': {'type': 'boolean', 'default': True}, 'save_asset': {'type': 'boolean', 'default': False}, 'dry_run': {'type': 'boolean', 'default': False}, 'validate_only': {'type': 'boolean', 'default': False}}, 'required': ['target_anim_bp_path'], 'additionalProperties': False}, 'outputSchema': {'type': 'object', 'properties': {'returnValue': {'type': 'object', 'properties': {'success': {'type': 'boolean'}, 'operation_id': {'type': 'string'}, 'stage': {'type': 'string'}, 'error_code': {'type': 'string'}, 'error': {'type': 'string'}, 'changes_json': {'type': 'string'}, 'compile_json': {'type': 'string'}, 'save_json': {'type': 'string'}, 'validation_json': {'type': 'string'}, 'retry_json': {'type': 'string'}, 'data_json': {'type': 'string'}}, 'required': ['success', 'operation_id', 'stage', 'error_code', 'error', 'changes_json', 'compile_json', 'save_json', 'validation_json', 'retry_json', 'data_json'], 'additionalProperties': False}}, 'required': ['returnValue'], 'additionalProperties': False}}]}

def public_schema():
    return copy.deepcopy(_PUBLIC_SCHEMA)

def binding_status():
    from cubeless_mcp_ue58.compatibility.ue58 import anim_blueprint_sample as api
    methods = api.binding_status()
    return {"ready": set(methods) == set(BINDINGS) and all(methods.values()), "library": api.LIBRARY,
        "methods": methods, "public_toolset": TOOLSET, "public_tool_count": 1,
        "public_input_counts": {m: len(PUBLIC_FIELDS_BY_METHOD[m]) for m in BINDINGS},
        "preparation_required": False, "schema_verified": False, "actual_official_raw_wire_verified": False,
        "original_creator10_paths_default_parent_and_compile_save_flags_preserved": True,
        "Native_factory_implicit_compile_original_explicit_compile_then_save_order_authoritative": True,
        "actual_behavior_runtime_or_full_pipeline_verified": False}

def validate_schema(schema):
    if (type(schema) is not dict or schema.get("name") != TOOLSET
            or type(schema.get("tools")) is not list or len(schema["tools"]) != 1):
        raise RuntimeError("Exact existing AuthoringTools projected singleton AnimBlueprint creator schema required")
    row = schema["tools"][0];method = BINDINGS[0];expected = _PUBLIC_SCHEMA["tools"][0]
    if type(row) is not dict or row.get("name") != TOOLSET + "." + method:
        raise RuntimeError("Exact original create_anim_blueprint_sample name required")
    if row.get("inputSchema") != expected["inputSchema"] or row.get("outputSchema") != expected["outputSchema"]:
        raise RuntimeError("Original creator paths/defaults and standard11 IO differ")
    inputs = row["inputSchema"];output = row["outputSchema"];inner = output["properties"]["returnValue"]
    if (tuple(inputs["properties"]) != PUBLIC_FIELDS_BY_METHOD[method] or inputs.get("additionalProperties") is not False
            or output.get("additionalProperties") is not False or inner.get("additionalProperties") is not False):
        raise RuntimeError("Original creator ordered fields and object flags differ")
    for field, value in DEFAULTS_BY_METHOD[method].items():
        actual = inputs["properties"][field].get("default")
        if type(actual) is not type(value) or actual != value:
            raise RuntimeError("Original creator default type/value differs: " + field)
    return {"schema_verified": True, "original_public_defaults_verified": True,
        "public_input_counts": {method: len(PUBLIC_FIELDS_BY_METHOD[method])}, "preparation_required": False}
