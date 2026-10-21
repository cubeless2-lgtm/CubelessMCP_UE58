"""PCG7 raw Registry contract; registration and construction are separate.

Never imports/decorates the excluded CubelessPCGTools UClass. Callable native
presence is source readiness, not active registration, GC repair or execution.
"""
import unreal
from cubeless_mcp_ue58.compatibility.ue58 import pcg as api
from . import raw_pcg

TOOLSET_NAME="cubeless_mcp_ue58.registration.pcg_toolset.CubelessPCGTools"
BINDINGS=raw_pcg.METHODS
DEFAULTS=raw_pcg.DEFAULTS
FIELDS=raw_pcg.FIELDS
NATIVE_METHODS=("inspect_settings_class","inspect_graph_snapshot","inspect_setting","apply_setting",
    "inspect_attribute_selector","preview_attribute_selector","inspect_connection","notify_and_recompile")


def binding_status():
    library=getattr(unreal,"CubelessPCGLibrary",None)
    methods={name:callable(getattr(library,name,None)) for name in NATIVE_METHODS}
    available=library is not None and all(methods.values())
    construction=callable(getattr(api,"construction_available",None)) and api.construction_available()
    fresh=getattr(unreal,"CubelessPCGGraphAuthoringLibrary",None)
    finite_fresh_binding=callable(getattr(fresh,"create_graph_from_spec_json",None))
    return {"ready":available,"bindings_ready":available,"library":"CubelessPCGLibrary","methods":methods,
        "finite_fresh_native_binding_available":finite_fresh_binding,
        "finite_fresh_behavior_verified":False,"general_construction_gate_changed":False,
        "preparation_required":False,"raw_toolset_registration_required":True,"public_toolset_name":TOOLSET_NAME,
        "construction_available":construction,"execution_ready":False,"registration_verified":False,"schema_verified":False,
        "behavior_verified":False,"actual_call_packing_verified":False,"gc_root_cause_verified":False,
        "known_construction_blocker":"" if construction else "pcg_construction_blocked_python_gc",
        "verification_scope":"Native callable presence only; exact finite fresh graph-spec construction has a separate JSON binding and current-ownership checks. General Python and unrestricted loaded-graph construction remain blocked. Dedicated native Add/Connect/SetSetting have a separate request-local exact Grid/DensityRemap/Distance/RandomChoice/DensityFilter/AttributeNoise/AttributeFiltering/CreatePoints/GetActorProperty target-owned profile; callable presence does not verify that profile or repair general GC health. Active registration/schema, representative behavior and execution/GC health require separate evidence"}


def validate_schema(schema):
    if type(schema) is not dict or schema.get("name")!=TOOLSET_NAME or type(schema.get("tools")) is not list:
        raise RuntimeError("Exact PCG raw Toolset schema required")
    tools=schema["tools"]
    if len(tools)!=7 or [row.get("name") for row in tools]!=[TOOLSET_NAME+"."+name for name in BINDINGS]:
        raise RuntimeError("Only seven exact PCG capabilities may be registered")
    for method,row in zip(BINDINGS,tools):
        inputs=row.get("inputSchema",{});fields=inputs.get("properties",{})
        if (set(fields)!=set(FIELDS[method]) or inputs.get("required")!=[key for key in FIELDS[method] if key not in DEFAULTS[method]]
                or inputs.get("additionalProperties") is not False):
            raise RuntimeError("Original PCG required/default fields changed: "+method)
        for name in FIELDS[method]:
            field=fields[name]
            expected_type=("object" if name=="graph_spec" else ["array","null"] if name in ("extra_names","forbidden_dependency_prefixes","allowed_dependency_prefixes")
                else ["string","null"] if name=="operation_id" else "boolean" if name in
                ("overwrite_existing","allow_overwrite_non_temp","save","dry_run","reset_extra_names","allow_non_temp_source","selected_only","cleanup","generate") else "string")
            if field.get("type")!=expected_type:
                raise RuntimeError("Original PCG field type changed: "+name)
            if name in ("extra_names","forbidden_dependency_prefixes","allowed_dependency_prefixes") and field.get("items")!={"type":"string"}:
                raise RuntimeError("Original extra_names string array/null required")
            if name in DEFAULTS[method]:
                value=DEFAULTS[method][name]
                if "default" not in field or type(field["default"]) is not type(value) or field["default"]!=value:
                    raise RuntimeError("Exact PCG default changed: "+name)
            elif "default" in field:
                raise RuntimeError("Required PCG field cannot be defaulted")
        output=row.get("outputSchema",{});envelope=output.get("properties",{}).get("returnValue",{})
        result_fields=envelope.get("properties",{})
        if (output.get("required")!=["returnValue"] or output.get("additionalProperties") is not False
                or set(result_fields)!=set(raw_pcg.RESULT_FIELDS) or envelope.get("required")!=list(raw_pcg.RESULT_FIELDS)
                or envelope.get("additionalProperties") is not False):
            raise RuntimeError("Existing exact typed operation envelope changed")
        for name in raw_pcg.RESULT_FIELDS:
            if result_fields[name].get("type")!=("boolean" if name=="success" else "string"):
                raise RuntimeError("Existing reflected result type changed")
    return {"schema_verified":True,"validated_tool_names":list(BINDINGS),
        "validated_input_counts":{method:len(FIELDS[method]) for method in BINDINGS},
        "original_object_array_contracts_verified":True,"preparation_required":False,
        "construction_gate_changed":False,"execution_ready":False,"behavior_verified":False,
        "actual_call_packing_verified":False,"gc_root_cause_verified":False}
