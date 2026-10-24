"""Three original named module-input behaviors on the official Registry."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


@unreal.uclass()
class CubelessNiagaraModuleInputTools(unreal.ToolsetDefinition):
    """Typed literal defaults, finite local Export DI and typed parameter links. Loaded system-owned inherited emitter paths additionally preserve the actual Parent and nullable LastMerge version/source/graph roles while changing only the selected local override graph. Shared parent package dirtiness is protected; a child-owned LastMerge graph can share the target package dirty lifecycle while retaining authored content. No loader, detach, parent merge or parent write is added. Unsupported source roles remain rejected, and broader inherited behavior awaits usage validation."""

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_input_literal_override(system_path: str, input_name: str,
            value: str, emitter_index: int | None = None, emitter_name: str | None = None,
            module_index: int | None = None, module_name: str | None = None,
            module_node_guid: str | None = None, overwrite_existing: bool = False,
            allow_source_edit: bool = False, save: bool = True, request_compile: bool = True,
            wait_for_completion: bool = False, timeout_seconds: float = 10.0,
            poll_interval_seconds: float = 0.1,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Write one typed native Niagara override-pin default without changing RapidIteration storage. Preserve the original named behavior and fifteen input keys/defaults, with explicit JSON-string representation for value at the reflected boundary; the Python domain still accepts Any. Input type/enum and actual graph topology come from the selected loaded native module. Existing linked overrides always reject, including overwrite_existing=True; existing unlinked default text can be overwritten only when explicitly allowed. Int values require exact int32 rather than the legacy fractional cast. No automatic asset load, rollback, wait/flush or uncertain retry. Native compile/save requests and known deferred work are reported separately. This finite operation does not complete every original module-input/DI pipeline or prove GPU execution, persistence reload or full public IO parity.

        Args:
            system_path: Exact already-loaded native Niagara System package/object path under /Game. Default writes permit isolated generated/test roots; no live world NiagaraComponent target.
            input_name: Actual module input name, full or Module.-stripped, matched case-insensitively.
            value: Required JSON string containing exactly one value: true, 1.25, [1,2,3], a quoted enum name, or null. This retains the original key but replaces its Any representation; raw JSON numbers/arrays/objects/null do not match this reflected string schema. JSON null remains a supplied literal and native rejects an unsupported null type. The domain JSON envelope rejects above 65536 UTF-16 units; the finite native literal accepts at most 16384 UTF-16 units. Duplicate JSON keys/nonfinite/NUL/unpaired-surrogate values reject before native dispatch. Float/int/bool/Color/Vec2/3/4/Position/enum admission is native-type-specific; DI/UObject literal values reject.
            emitter_index: Optional exact int32. Null or -1 leaves index unspecified. Other negative values remain supplied and produce native no-match. Booleans/fractions reject.
            emitter_name: Optional joint case-insensitive emitter name filter; null/omission means original empty string.
            module_index: Optional index in all emitter function-call nodes sorted by NodePosY then NodePosX; this is not stack/insertion order. Null/-1 unspecified, other int32 values retained.
            module_name: Optional joint case-insensitive native function name filter; null/omission means empty.
            module_node_guid: Optional exact module GUID filter; null/omission means empty. The original first matching sorted call is selected with all supplied selectors together.
            overwrite_existing: Allow replacement of an existing unlinked pin default only. Linked graph replacement is always rejected by this literal route.
            allow_source_edit: Default false limits writes to generated/test assets. True opts into another exact /Game target without relaxing native identity/reference guards.
            save: Request at most one exact target save. Outstanding/needed compilation defers saving with retained work; inspect readiness and save the exact asset rather than replaying this setter.
            request_compile: Request one nonblocking native compile after the verified change; completion is separately observed.
            wait_for_completion: Default false. True is incomplete and rejects known0 before journal/native dispatch to avoid blocking the official game thread.
            timeout_seconds: Original positive finite default10, retained caller/readiness context only; no internal game-thread wait.
            poll_interval_seconds: Original positive finite default0.1, retained caller context only; no internal polling.
            operation_id: Optional durable request ID; identical completed requests replay recorded results, interrupted requests do not dispatch again.
        """
        from cubeless_mcp_ue58.operations import niagara_module_inputs as operations
        try:
            typed_value = operations.decode_registry_json(value, "value")
        except Exception as error:
            return reflected_result(operations.registry_json_failure(error, operation_id))
        return reflected_result(operations.set_niagara_module_input_literal_override(
            system_path, input_name, typed_value, emitter_index, emitter_name, module_index,
            module_name, module_node_guid, overwrite_existing, allow_source_edit, save,
            request_compile, wait_for_completion, timeout_seconds, poll_interval_seconds,
            operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_data_interface_module_input(system_path: str, input_name: str,
            properties: str | None = None, emitter_index: int | None = None,
            emitter_name: str | None = None, module_index: int | None = None,
            module_name: str | None = None, module_node_guid: str | None = None,
            overwrite_existing: bool = False, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True, wait_for_completion: bool = False,
            timeout_seconds: float = 10.0, poll_interval_seconds: float = 0.1,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one local DI override from the actual selected input type, using the exported Engine creator's returned node-owned instance. The finite implemented class is exact native NiagaraDataInterfaceExport, with only its four public export settings. Preserve the original named contract and property-key omission semantics through a reflected JSON-string envelope; the Python domain still accepts dict|None. Other DI classes/property fields and occupied DI/linked override replacement remain explicitly incomplete before mutation, even when overwrite_existing=True. There is no arbitrary class/property executor, extra DI allocation, hidden load or atomic rollback. Native before/after, actual DI identity/fields, compile/save/deferred state and uncertain failure are retained. A static callback User binding does not assign a live receiver or prove Niagara/GPU callback transport; cold private Input-node-to-DI reference proof remains separate.

        Args:
            system_path: Exact already-loaded native /Game Niagara System package/object path admitted by native write guards.
            input_name: Actual DI-typed module input name; full or Module.-stripped case-insensitive form. Class is inferred from this input, never supplied by the caller.
            properties: Optional JSON object string using original key properties, not a raw object: e.g. {"GPUAllocationFixedSize":1,"CallbackHandlerParameter":"(Parameter=(Name=\"User.CFNL_OutputPositionReceiver\"))"}. Omission/null, JSON "null" or "{}" request no fields, matching the original omitted None/empty map. Supported exact fields are CallbackHandlerParameter, GPUAllocationMode, GPUAllocationFixedSize, GPUAllocationPerParticleSize; each value uses the original scalar string/bool/number envelope with finite native type/range validation. Callback requires a qualified native UObject User binding name; the declaration may be absent so a producer can declare it afterwards. If that User declaration already exists it must have exact native UObject type, otherwise known preflight niagara_export_callback_user_type_mismatch rejects without allocation. The return reports callback_user_parameter_exists, callback_user_parameter_type_matches and callback_declaration_created=False; this action creates no User declaration. Unknown fields, duplicate keys, malformed binding/JSON or unsupported property values reject before native mutation. The domain JSON envelope rejects above 65536 UTF-16 units; the finite native property object accepts at most 16384 UTF-16 units and these four fields only. This representation difference means full original public IO parity is not claimed.
            emitter_index: Optional exact int32, null/-1 unspecified; other negative values remain native no-match.
            emitter_name: Optional joint case-insensitive name filter; null/omission normalizes to original empty string.
            module_index: Optional sorted emitter function-call index (NodePosY/X), not stack insertion index; null/-1 unspecified.
            module_name: Optional joint native function-name filter; null/omission means empty.
            module_node_guid: Optional joint exact GUID filter; null/omission means empty. First matching original sorted call selected.
            overwrite_existing: Retained original flag. Finite occupied override replacement is incomplete and rejects before mutation, rather than removing an old graph.
            allow_source_edit: Default false permits isolated generated/test targets; true permits another exact /Game target without relaxing native runtime guards.
            save: Request one exact save; outstanding/needed compile defers it. Continue by readiness plus exact save, never by recreating this DI.
            request_compile: Request one nonblocking native compile after the verified input change; completion remains separately observed.
            wait_for_completion: True rejects known0 before journal/native dispatch; no game-thread blocking or flush.
            timeout_seconds: Positive finite default10, preserved caller/readiness context only.
            poll_interval_seconds: Positive finite default0.1, preserved caller context; no internal polling.
            operation_id: Durable ID; interrupted/uncertain operations never retry automatically.
        """
        from cubeless_mcp_ue58.operations import niagara_module_inputs as operations
        try:
            typed_properties = operations.decode_registry_json(properties, "properties", optional_object=True)
        except Exception as error:
            return reflected_result(operations.registry_json_failure(error, operation_id))
        return reflected_result(operations.set_niagara_data_interface_module_input(
            system_path, input_name, typed_properties, emitter_index, emitter_name,
            module_index, module_name, module_node_guid, overwrite_existing,
            allow_source_edit, save, request_compile, wait_for_completion,
            timeout_seconds, poll_interval_seconds, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_input_linked_parameter(system_path: str, input_name: str,
            linked_parameter_name: str, emitter_index: int | None = None,
            emitter_name: str | None = None, module_index: int | None = None,
            module_name: str | None = None, module_node_guid: str | None = None,
            overwrite_existing: bool = False, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True, wait_for_completion: bool = False,
            timeout_seconds: float = 10.0, poll_interval_seconds: float = 0.1,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Wire one selected native module input to a typed namespaced Niagara parameter through the exported graph helper. Preserve original named selector/overwrite/compile/save/timing inputs, actual aliased handle and linked name; this does not set User defaults or RapidIteration data. The helper may create a ParameterMapGet and graph parameter metadata. Original source did not establish upstream existence despite its description; native finite admission requires actual matching typed User/target/source graph metadata and reports actual type/reference rather than producer execution. If resolver enumeration omits the input, fallback is bounded to exact selected already-loaded module-source metadata. Scratch producer construction/functions and occupied override replacement remain incomplete; occupied replacement fails before mutation. No automatic load, server bridge, rollback or unknown mutation retry. Save may be known deferred during nonblocking compilation; separate readiness and exact saving do not replay this link. Full producer/GPU transport, cold reference proof and full legacy parity remain unverified.

        Args:
            system_path: Exact already-loaded native /Game Niagara System package/object path under native write/runtime guards.
            input_name: Actual native module input name, full or Module.-stripped case-insensitive form.
            linked_parameter_name: Nonempty qualified namespaced Niagara parameter name, e.g. User.CubelessExportVector. Native type/reference admission is authoritative; no guessed Position/Vec3 equivalence or producer readiness claim.
            emitter_index: Optional exact int32; null/-1 unspecified, other negative values remain supplied and native no-match.
            emitter_name: Optional joint case-insensitive emitter filter; null/omission means original empty string.
            module_index: Optional index in all emitter call nodes sorted NodePosY then NodePosX, not stack/insertion position; null/-1 unspecified.
            module_name: Optional joint native function-name filter; null/omission means empty.
            module_node_guid: Optional joint exact GUID filter; original first matching sorted call selected. Null/omission means empty.
            overwrite_existing: Retained original flag; occupied DI/linked replacement is incomplete and rejects before mutation.
            allow_source_edit: Default false permits isolated generated/test roots; true opts into another exact /Game target without weakening guards.
            save: Request one exact target save. Pending/needed compile defers it; inspect readiness, then save without replaying link creation.
            request_compile: Request one nonblocking compile after verified change; actual completion remains separately observed.
            wait_for_completion: Default false. True rejects known0 before journal/native dispatch because the official game thread must not block.
            timeout_seconds: Positive finite default10, retained caller/readiness context only.
            poll_interval_seconds: Positive finite default0.1, retained caller context only; no internal polling.
            operation_id: Optional durable ID; completed identical requests replay their result, incomplete/uncertain requests never dispatch automatically.
        """
        from cubeless_mcp_ue58.operations import niagara_module_inputs as operations
        return reflected_result(operations.set_niagara_module_input_linked_parameter(
            system_path, input_name, linked_parameter_name, emitter_index, emitter_name,
            module_index, module_name, module_node_guid, overwrite_existing,
            allow_source_edit, save, request_compile, wait_for_completion,
            timeout_seconds, poll_interval_seconds, operation_id))
