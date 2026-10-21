"""Two original transient probes; logical nullable JSON and reflected FString carriers differ."""
import json
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_direct_gate_probe as direct_api
from cubeless_mcp_ue58.compatibility.ue58 import controlrig_pre_post_runtime_pose as pose_api

TOOLSET = "cubeless_mcp_ue58.registration.controlrig_transient_probe_toolset.CubelessControlRigTransientProbeTools"
CLASS_PATH = "/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/controlrig_transient_probe_toolset_PY.CubelessControlRigTransientProbeTools"
BINDINGS = ('controlrig_direct_gate_probe', 'sample_controlrig_pre_post_runtime_pose')
PUBLIC_FIELDS_BY_METHOD = {'controlrig_direct_gate_probe': ('control_rig_path', 'control_rig_class', 'cases', 'sample_elements', 'control_values', 'execute_events', 'should_trace_property', 'interaction_location_property'), 'sample_controlrig_pre_post_runtime_pose': ('control_rig_path', 'control_rig_class', 'input_defaults', 'curve_values', 'cases', 'sample_elements', 'control_values', 'execute_events', 'should_trace_property', 'interaction_location_property')}
NULLABLE_KINDS = {'cases': 'array_object', 'sample_elements': 'array_any', 'control_values': 'object_any', 'execute_events': 'array_string', 'input_defaults': 'object_any', 'curve_values': 'object_number'}
DEFAULTS_BY_METHOD = {'controlrig_direct_gate_probe': {'control_rig_path': '', 'control_rig_class': '', 'cases': None, 'sample_elements': None, 'control_values': None, 'execute_events': None, 'should_trace_property': 'ShouldDoIKTrace', 'interaction_location_property': 'InteractionWorldLocation'}, 'sample_controlrig_pre_post_runtime_pose': {'control_rig_path': '', 'control_rig_class': '', 'input_defaults': None, 'curve_values': None, 'cases': None, 'sample_elements': None, 'control_values': None, 'execute_events': None, 'should_trace_property': 'ShouldDoIKTrace', 'interaction_location_property': 'InteractionWorldLocation'}}
REQUIRED_BY_METHOD = {'controlrig_direct_gate_probe': (), 'sample_controlrig_pre_post_runtime_pose': ()}
ATTACHMENT_LIBRARY = "CubelessControlRigTransientProbeAttachmentLibrary"
ATTACHMENT_METHODS = ("install_controlrig_transient_probe_toolset", "verify_controlrig_transient_probe_toolset", "remove_controlrig_transient_probe_toolset")
ATTACHMENT_READY_FIELDS = ('ready', 'verified', 'active', 'guard_attached', 'raw_guard_installed', 'default_verified', 'reflected_carrier_schema_verified', 'logical_schema_verified', 'ordinary_string4_schema_verified', 'nullable4_schema_verified', 'nullable6_schema_verified', 'direct_probe8_schema_verified', 'pre_post10_schema_verified', 'empty_path_defaults_verified', 'property_defaults_verified', 'output11_schema_preserved')
RESULT_FIELDS = ("success","operation_id","stage","error_code","error","changes_json","compile_json","save_json","validation_json","retry_json","data_json")

def binding_status(toolset_class):
    APIs = (direct_api,pose_api)
    methods = {m: type(s) is dict and set(s)=={m} and s.get(m) is True for m,api in zip(BINDINGS,APIs) for s in (api.binding_status(),)}
    defined = {m: callable(getattr(toolset_class,m,None)) for m in BINDINGS}
    return dict(ready=False,bindings_ready=all(methods.values()) and all(defined.values()),
        library="CubelessControlRigTransientProbeLibrary",methods=methods,definition_methods=defined,public_toolset=TOOLSET,
        public_tool_count=2,public_input_counts={BINDINGS[0]:8,BINDINGS[1]:10},hidden_input_counts={BINDINGS[0]:8,BINDINGS[1]:10},
        nullable_json_carrier_counts={BINDINGS[0]:4,BINDINGS[1]:6},ordinary_string_counts={m:4 for m in BINDINGS},
        schema_verified=False,default_verified=False,raw_guard_required=True,raw_guard_ready=False,nullable_raw_guard_ready=False,
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False,
        unique_external_pipeline_complete=False)

def _required(expected,value):
    return type(value) is list and len(value)==len(expected) and all(type(v) is str for v in value) and set(value)==set(expected)

def _child(kind):
    return {"type":"object"} if kind=="array_object" else {"type":"string"} if kind=="array_string" else {"type":"number"} if kind=="object_number" else {}

def _validate_schema(schema,toolset_class,reflected_carriers=False):
    if type(schema) is not dict or schema.get("name")!=TOOLSET or type(schema.get("tools")) is not list or len(schema["tools"])!=2:
        raise RuntimeError("Exact canonical transient probe Toolset2 required")
    remaining=set(BINDINGS)
    for row in schema["tools"]:
        if type(row) is not dict or type(row.get("name")) is not str or not row["name"].startswith(TOOLSET+"."):
            raise RuntimeError("Exact original probe method name required")
        method=row["name"][len(TOOLSET)+1:]
        if method not in remaining or not callable(getattr(toolset_class,method,None)):
            raise RuntimeError("Only both original probe definitions are admitted")
        remaining.remove(method);inputs=row.get("inputSchema");props=inputs.get("properties") if type(inputs) is dict else None
        if type(inputs) is not dict or inputs.get("type")!="object" or type(props) is not dict or set(props)!=set(PUBLIC_FIELDS_BY_METHOD[method]) or not _required(("control_rig_path","control_rig_class") if reflected_carriers else (),inputs.get("required")):
            raise RuntimeError("Exact public8/10 fields and physical-required/logical-optional paths required")
        for field,default in DEFAULTS_BY_METHOD[method].items():
            item=props[field];kind=NULLABLE_KINDS.get(field)
            expected_type="string" if reflected_carriers or kind is None else ["array" if kind.startswith("array_") else "object","null"]
            if type(item) is not dict or item.get("type")!=expected_type:
                raise RuntimeError("Probe logical shape/private carrier differs: "+field)
            if reflected_carriers and field in ("control_rig_path","control_rig_class"):
                if "default" in item: raise RuntimeError("SDK physical empty path defaults must remain absent")
            else:
                expected="null" if reflected_carriers and kind else default
                if "default" not in item or type(item["default"]) is not type(expected) or item["default"]!=expected:
                    raise RuntimeError("Original probe nullable/property default differs: "+field)
            if kind and not reflected_carriers:
                childkey="items" if kind.startswith("array_") else "additionalProperties"
                if item.get(childkey)!=_child(kind): raise RuntimeError("Original public probe child shape differs: "+field)
        output=row.get("outputSchema");outer=output.get("properties") if type(output) is dict else None
        if type(output) is not dict or output.get("type")!="object" or type(outer) is not dict or set(outer)!={"returnValue"} or not _required(("returnValue",),output.get("required")):
            raise RuntimeError("Exact returnValue output envelope required")
        value=outer["returnValue"];fields=value.get("properties") if type(value) is dict else None
        if type(value) is not dict or value.get("type")!="object" or type(fields) is not dict or set(fields)!=set(RESULT_FIELDS) or not _required(RESULT_FIELDS,value.get("required")) or any(type(fields[k]) is not dict or fields[k].get("type")!=("boolean" if k=="success" else "string")for k in RESULT_FIELDS):
            raise RuntimeError("Exact synchronous standard11 result required")
    return dict(schema_verified=not reflected_carriers,default_verified=not reflected_carriers,
        reflected_carrier_schema_verified=reflected_carriers,logical_adapter_schema_verified=not reflected_carriers,
        public_input_counts={BINDINGS[0]:8,BINDINGS[1]:10},nullable_json_carrier_counts={BINDINGS[0]:4,BINDINGS[1]:6},
        actual_official_wire_verified=False,nullable_Named_wire_verified=False,metadata_schema_validation_is_wire_packing_proof=False)

def validate_schema(schema,toolset_class):
    return _validate_schema(schema,toolset_class)

def validate_reflected_carrier_schema(schema,toolset_class):
    return _validate_schema(schema,toolset_class,True)

def validate_registered_metadata(class_schema,attachment_report,toolset_class):
    reflected=validate_reflected_carrier_schema(class_schema,toolset_class)
    if type(attachment_report) is not dict or attachment_report.get("operation") not in ATTACHMENT_METHODS[:2] or not all(attachment_report.get(k) is True for k in ATTACHMENT_READY_FIELDS) or attachment_report.get("asset_mutation") is not False or attachment_report.get("metadata_mutation") is not False or attachment_report.get("logical_schema_source")!="actual_registered_wrapper_GetJsonSchema" or attachment_report.get("logical_schema_registry_identity_verified") is not True:
        raise RuntimeError("Exact ready owner receipt with actual registered probe wrapper schema required")
    text=attachment_report.get("logical_schema_json")
    if type(text) is not str or not text: raise RuntimeError("Actual registered probe wrapper schema missing")
    logical_schema=json.loads(text);logical=validate_schema(logical_schema,toolset_class)
    return dict(logical,reflected_carrier_schema_verified=True,class_reflected_schema_verified=True,
        class_schema_getter_scope="fresh_FFunctionLibraryToolset_not_registered_handler",
        logical_schema_source="actual_registered_wrapper_GetJsonSchema",logical_schema_registry_identity_verified=True,
        actual_registry_wrapper_schema_verified=True,logical_schema_json=text,logical_schema=logical_schema,
        reflected_schema_validation=reflected,logical_schema_validation=logical,actual_official_wire_verified=False,nullable_Named_wire_verified=False)
