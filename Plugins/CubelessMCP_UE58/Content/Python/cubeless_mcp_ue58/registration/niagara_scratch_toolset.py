"""Named legacy Scratch tools on the official Registry, no transport coupling."""
import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


@unreal.uclass()
class CubelessNiagaraScratchTools(unreal.ToolsetDefinition):
    """Target-local private module, typed write, stack, HLSL and rewire tools."""

    @toolset_registry.tool_call
    @staticmethod
    def create_or_duplicate_scratch_pad_module(target_system_path: str,
            source_script_path: str | None = None, source_system_path: str | None = None,
            source_owner_kind: str = "system", source_script_index: int | None = None,
            source_scratch_pad_name: str | None = None, source_emitter_index: int | None = None,
            source_emitter_name: str | None = None, target_owner_kind: str = "system",
            target_emitter_index: int | None = None, target_emitter_name: str | None = None,
            new_script_name: str | None = None, target_supported_usage: str = "ParticleUpdateScript",
            allow_source_edit: bool = False, save: bool = True, request_compile: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create from the actual already-loaded engine default module template or duplicate an admitted Module/DynamicInput source into the target's private Scratch ownership. The primary native admission conditionally loads the exact target then selected source, holds actual returns, and checks source/version/ownership before duplication; later observers never load. Preserve all sixteen original input keys/defaults; optional reflected null strings normalize only to historical empty strings. Actual unique name, owner/version/usage, script graph and source preservation come from native observations. Unsupported owner/source/usage variants must fail before mutation rather than silently default. No external server, TCP bridge or generic executor. The compile request is nonblocking; requested save follows the original compile-then-save order and may wait through Engine PreSave. Actual attempted/returned/saved and partial/unknown outcomes are separate; never replay an uncertain creation. This primitive alone does not implement Position HLSL, source rewiring, GPU export or full legacy parity.

        Args:
            target_system_path: Exact runtime /Game Niagara System package or object path; primary native admission may conditionally load this target before the source.
            source_script_path: Optional direct Module/DynamicInput source path; nonempty takes original precedence over source_system_path. Exact primary native source load is conditional, with immediate ownership/lifetime checks. Omitted/null becomes original empty string.
            source_system_path: Optional source system for owner/index/name selection, conditionally loaded only during primary native admission. Omit both source paths for the observed already-loaded default module template.
            source_owner_kind: Original default system; emitter/parent_emitter remain native finite-admission variants and unsupported variants report known preflight failure.
            source_script_index: Optional exact int32. None/-1 means unspecified; other negative values are retained as original no-match selectors.
            source_scratch_pad_name: Optional case-insensitive source Scratch name; actual selected native identity is returned.
            source_emitter_index: Optional exact int32 joint source-emitter selector, retained without fractional casts.
            source_emitter_name: Optional source emitter name paired with its index.
            target_owner_kind: Original default system; emitter requires an admitted exact local target owner. No editor/Cubeless object is serialized as a runtime owner.
            target_emitter_index: Optional exact int32 selector when targeting emitter ownership.
            target_emitter_name: Optional target emitter name paired with its index.
            new_script_name: Optional desired private script name; actual engine unique name is authoritative, not assumed equal.
            target_supported_usage: Original ParticleUpdateScript default; native usage support is checked against the real script metadata. Blank creation adds supported usage without claiming emitter execution.
            allow_source_edit: False admits legacy generated roots and current CubelessMCPTests; true selects other valid /Game targets under all source/runtime guards.
            save: Original true request. Reports request, attempt, saved and deferred state separately. Never saves all assets or retries uncertain saving.
            request_compile: Original true requests one asynchronous compile after actual work. False defers explicit compilation to composition; implicit notification effects remain separately qualified.
            operation_id: Optional durable ID; completed identical calls replay their stored result, interrupted/uncertain mutations do not redispatch automatically.
        """
        from cubeless_mcp_ue58.operations.niagara_scratch import create_or_duplicate_scratch_pad_module
        return reflected_result(create_or_duplicate_scratch_pad_module(target_system_path,
            source_script_path, source_system_path, source_owner_kind, source_script_index,
            source_scratch_pad_name, source_emitter_index, source_emitter_name, target_owner_kind,
            target_emitter_index, target_emitter_name, new_script_name, target_supported_usage,
            allow_source_edit, save, request_compile, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_scratch_pad_parameter_write(system_path: str, parameter_name: str,
            value: str | None = None, parameter_type: str = "vec2", default_value: str | None = None,
            scratch_pad_script_path: str | None = None, scratch_pad_owner_kind: str = "system",
            scratch_pad_script_index: int | None = None, scratch_pad_name: str | None = None,
            scratch_pad_emitter_index: int | None = None, scratch_pad_emitter_name: str | None = None,
            skip_if_exists: bool = True, allow_source_edit: bool = False, save: bool = True,
            request_compile: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Add an actual typed ParameterMapSet write inside an admitted target-local private Module script and preserve its real input-map to output-map chain. Preserve the original fifteen input keys/defaults; domain value remains Any|None but this reflected optional value is JSON text, an explicit public IO representation gap. A nonempty raw default_value takes original precedence and ignored value is not decoded. Actual node/pin/type/default/metadata and native skip/partial outcomes are returned. An isolated pin or constant is not proof of the original System Position→HLSL→particle producer. HLSL/source rewiring, unsupported types and generic dynamic-pin variants remain incomplete unless native explicitly implements them. No hidden load, rollback or uncertain automatic retry; async compile/save readiness is separate.

        Args:
            system_path: Exact already-loaded target Niagara System package/object path under runtime-safe ownership guards.
            parameter_name: Original required namespaced parameter, e.g. Particles.CubelessScratchVector; actual native typed metadata must match.
            value: Optional JSON text such as "[1,2,3]", "true" or "1.25" using the original key. Omission/null maps to original None, while JSON "null" is explicitly decoded None. Raw arrays/numbers are not this reflected string schema. JSON duplicate keys/nonfinite/NUL/surrogates reject before journal/native. Domain envelope is bounded to 65536 UTF-16 units, with narrower native limits authoritative. Nonempty default_value makes this value completely ignored, including malformed encoded text.
            parameter_type: Original vec2 default and type alias spelling retained for native finite admission. Unsupported aliases/types reject before allocation; typed Position is not automatically double/LWC parity.
            default_value: Optional raw Unreal pin default string. Nonempty wins over value exactly as original Python; omit/null/empty uses value. Raw text is bounded, and actual native parsing/readback governs whether it is valid for parameter_type.
            scratch_pad_script_path: Optional exact target-local private script path; original direct-path precedence is retained. A path under the same package alone is not ownership proof.
            scratch_pad_owner_kind: Original system/emitter selector; only exact native-admitted owner variants can mutate.
            scratch_pad_script_index: Optional exact int32. Provide path, name or index; None/-1 means unspecified and other negative selectors are retained.
            scratch_pad_name: Optional name selecting the actual private module, with original case-insensitive matching.
            scratch_pad_emitter_index: Optional exact int32 selector for emitter-owned scripts.
            scratch_pad_emitter_name: Optional emitter name paired with its index unless original direct-path precedence applies.
            skip_if_exists: Original true requests zero-work success when the same parameter write already exists. Native returns actual skip identity; requested save/compile are not proof of performed work.
            allow_source_edit: Original false scoped /Game admission; true still applies all private-owner/source/runtime guards.
            save: Original true requests one exact save; compilation pending yields known saved=false/attempted=false/deferred result, not rollback or automatic retry.
            request_compile: Original true request after actual write; false lets a composed graph compile once later. No synchronous wait/flush.
            operation_id: Optional durable ID; effective raw-default precedence is included in the request fingerprint. No nested child journal in batch execution.
        """
        from cubeless_mcp_ue58.operations import niagara_scratch
        try:
            decoded = None if default_value else niagara_scratch.decode_registry_value(value)
        except Exception as error:
            return reflected_result(niagara_scratch.rejected(error, operation_id))
        return reflected_result(niagara_scratch.add_niagara_scratch_pad_parameter_write(
            system_path, parameter_name, decoded, parameter_type, default_value,
            scratch_pad_script_path, scratch_pad_owner_kind, scratch_pad_script_index,
            scratch_pad_name, scratch_pad_emitter_index, scratch_pad_emitter_name,
            skip_if_exists, allow_source_edit, save, request_compile, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_scratch_pad_module_to_stack(target_system_path: str,
            scratch_pad_script_path: str | None = None, scratch_pad_owner_kind: str = "system",
            scratch_pad_script_index: int | None = None, scratch_pad_name: str | None = None,
            scratch_pad_emitter_index: int | None = None, scratch_pad_emitter_name: str | None = None,
            target_usage: str = "ParticleUpdateScript", target_usage_id: str | None = None,
            target_emitter_index: int | None = None, target_emitter_name: str | None = None,
            target_index: int | None = None, suggested_name: str | None = None,
            skip_if_duplicate: bool = True, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Insert an actual target-local private Scratch Module into one native stack output. This is the original private-script contract, distinct from the external stock script tool's admission. Preserve all seventeen original inputs/defaults, exact usage/GUID/emitter selectors, absolute index, suggested-name semantics and duplicate policy; actual name/order/native splice comes from engine readback. Duplicate skip performs no compile/save or option reconciliation. Only already-loaded exact owned sources may enter; unsupported owner/usage/source variants fail before mutation. A write and insertion without a real consumer link does not establish a producer pipeline. No asset load, external server/bridge, runtime Cubeless helper, HLSL construction, game-thread wait or uncertain retry.

        Args:
            target_system_path: Exact already-loaded runtime /Game target System package or object path.
            scratch_pad_script_path: Optional direct private Module script path already owned by this target. No implicit load or external asset substitution.
            scratch_pad_owner_kind: Original system default or finite admitted emitter owner.
            scratch_pad_script_index: Optional exact int32 selector. None/-1 unspecified; other negatives retain original no-match semantics.
            scratch_pad_name: Optional native private-script name selecting an actual owned array entry.
            scratch_pad_emitter_index: Optional exact int32 selector for an emitter-owned Scratch script.
            scratch_pad_emitter_name: Optional emitter name paired with its index, subject to original direct-path precedence.
            target_usage: Original ParticleUpdateScript default; selected output and script usage support must match actual native observations.
            target_usage_id: Optional exact existing output GUID. SimulationStage selection requires its observed GUID; no invented output creation.
            target_emitter_index: Optional exact int32 joint selector for the selected emitter/particle stack.
            target_emitter_name: Optional name paired with target emitter index.
            target_index: Optional full int32 absolute insertion index; omit/-1 appends. Other negative indices clamp to first and large indices to last according to the original public engine splice semantics.
            suggested_name: Optional engine name suggestion; actual computed function name is authoritative and may differ.
            skip_if_duplicate: Original true performs no work when this exact private script already occupies the selected output. False permits another insertion, with actual identity/order reported.
            allow_source_edit: Original scoped false /Game admission; true does not permit engine/plugin targets or unsafe runtime dependencies.
            save: Original true request; native save while compilation pending is known deferred. Observe readiness then exact save without another insertion.
            request_compile: Original true requests asynchronous compile only after actual insertion; false retains composition for one later boundary.
            operation_id: Optional durable ID. Completed identical requests replay only their result; unknown insertion outcome requires inspection, never automatic replay.
        """
        from cubeless_mcp_ue58.operations.niagara_scratch import add_scratch_pad_module_to_stack
        return reflected_result(add_scratch_pad_module_to_stack(target_system_path,
            scratch_pad_script_path, scratch_pad_owner_kind, scratch_pad_script_index,
            scratch_pad_name, scratch_pad_emitter_index, scratch_pad_emitter_name,
            target_usage, target_usage_id, target_emitter_index, target_emitter_name,
            target_index, suggested_name, skip_if_duplicate, allow_source_edit,
            save, request_compile, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def insert_niagara_scratch_pad_custom_hlsl_for_pin(system_path: str, target_pin_name: str,
            expression: str, scratch_pad_script_path: str | None = None,
            scratch_pad_owner_kind: str = "system", scratch_pad_script_index: int | None = None,
            scratch_pad_name: str = "RenderGrid", scratch_pad_emitter_index: int | None = None,
            scratch_pad_emitter_name: str | None = None, target_node_guid: str | None = None,
            target_node_name: str | None = None, target_node_index: int | None = None,
            preserve_existing_link_as: str = "BaseValue", output_pin_name: str = "CustomHLSLOutput",
            signature_name: str = "IF_CustomPinValue", custom_hlsl_script_usage: str = "DynamicInput",
            inputs: str | None = None, user_parameter_inputs: str | None = None,
            skip_if_already_inserted: bool = True, replace_existing_custom: bool = False,
            rebuild_existing_custom: bool = False, requires_context: bool = False,
            delete_unlinked_custom_nodes: bool = False, delete_unlinked_custom_input_source_nodes: bool = False,
            allow_source_edit: bool = False, save: bool = True, request_compile: bool = True,
            parameter_inputs: str | None = None,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        'CustomHLSL User inputs additionally admit an ordered bounded list of existing User declarations using original primitive Float/Int/Bool/Color/Vec2/Vec3/Vec4/Position types, sharing existing Specs/MapGet output-default pairs, UK reuse/rebuild, cleanup and final UserState protection. Callback checks use copied full name/type and redirect identity without holding store data pointers or repeating full value/disk scans per scratch row. Real allocation/Add/pair verification prefixes are reported incrementally when a later row fails or becomes unknown. Existing single-Bool behavior remains on the shared writer; GetParameters full keys are distinguished from GetUserParameters short redirect keys. ArrayDI/UObject, wider names/source/callbacks and actual mixed/multiple input execution remain incomplete or usage pending. Existing public list carrier limitations and original partial/compile/save contracts are retained.\n\nAttach one native Custom HLSL node to a real target-local private Scratch input through the public engine graph/property APIs. Preserve all twenty-seven original input keys/defaults; parameter_inputs is an explicit additional typed-source attachment previously available only inside the old native command. The three list keys are JSON text at this reflected boundary; typed domain lists remain intact and the IO representation gap is not full public parity. Omitted/null lists differ from explicit JSON []. Native finite admission reports unsupported usage/source/replacement/context/destructive variants before mutation; options are never silently discarded. The finite primitive/Position Function or DynamicInput attachment and native ParameterMapGet output/default pairing do not establish a System Position definition, LWC precision, GPU execution or natural export. No load, private-field write, generic executor, game-thread wait, rollback or uncertain retry.\n\n        Args:\n            system_path: Exact already-loaded runtime /Game target System under owned private-script guards.\n            target_pin_name: Required actual input pin name; native direction, type and target identity must match.\n            expression: Required HLSL body, preserving multiline text; bounded to 65536 UTF-16 units at the domain envelope, with narrower native limits authoritative. The returned actual text and editable-property notification/readback are evidence, not shader execution.\n            scratch_pad_script_path: Optional exact target-owned private script path; omitted/null normalizes to original empty path.\n            scratch_pad_owner_kind: Original system owner default, with finite native owner admission.\n            scratch_pad_script_index: Optional exact int32, None/-1 unspecified; no fractional cast or hidden script creation.\n            scratch_pad_name: Original RenderGrid default; direct path precedence and actual selector matching remain native.\n            scratch_pad_emitter_index: Optional exact int32 emitter-owned script selector.\n            scratch_pad_emitter_name: Optional emitter selector; omitted/null becomes original empty name.\n            target_node_guid: Optional exact native target-node GUID filter; omitted/null becomes empty.\n            target_node_name: Optional native target-node name filter; omitted/null becomes empty.\n            target_node_index: Optional exact int32 target-node selector; no name-only equivalence assumption.\n            preserve_existing_link_as: Original BaseValue input name for an existing linked source. Prior target default is recorded/kept but no implicit BaseValue default input is created when unlinked; unsupported topology is explicit.\n            output_pin_name: Original CustomHLSLOutput output name; real output type matches the target input.\n            signature_name: Original IF_CustomPinValue signature name and returned native signature identity.\n            custom_hlsl_script_usage: Original DynamicInput default is retained; Function and DynamicInput are the finite native usage routes. Other usage reports known preflight failure; accepted shader structure is not execution proof.\n            inputs: Optional JSON list of object rows for original existing-graph sources. Omit/null means None, JSON [] remains present; finite native rows select an existing concrete output pin; input-pin-as-source or unsupported types fail explicitly.\n            user_parameter_inputs: Optional JSON object-row list for original User sources with the same null/empty presence distinction. One existing bool User map-read is supported; wider User/DI variants remain incomplete. No surrogate User declaration is invented.\n            skip_if_already_inserted: Original true; actual exact native expression/signature/topology skip rules are returned, with no compile/save when unchanged.\n            replace_existing_custom: Original false; admitted existing native Custom nodes can update in place through public reconstruction/property APIs, preserving the actual completed prefix and reporting unsupported variants.\n            rebuild_existing_custom: Original false; admitted existing Custom wrappers can rebuild with fresh pins and finite retirement checks. Native results distinguish actual removals, changed links and uncertain partial outcomes; no atomic rollback.\n            requires_context: Original false; native can wire the existing target MapSet predecessor into a context Map input. DI/context recipes outside this finite typed signature remain incomplete.\n            delete_unlinked_custom_nodes: Original false; native performs bounded orphan-Custom cleanup where supported, preserving nonselected state and stopping after an uncertain callback outcome.\n            delete_unlinked_custom_input_source_nodes: Original false; cleanup is restricted to admitted former Custom input-source candidates with no remaining uses. Wider valid source variants remain tracked as incomplete.\n            allow_source_edit: Original false scoped generated/Test roots; true still preserves all native ownership/runtime guards.\n            save: Original true request; pending compile can retain changed work with saved=false/save_deferred=true. Observe readiness and save exactly once without another attachment.\n            request_compile: Original true requests asynchronous compile after actual change; false supports composition.\n            parameter_inputs: Additional optional JSON object-row list for exact typed Module.<input_name> primitive/Position sources; System/Engine/User/DI source binding is incomplete. Omit/null and [] are distinct. It is not an implementation of the old ignored direct_parameter_name or full System Position writer.\n            operation_id: Optional single durable journal ID; batch children use their parent checkpoint and never create a nested journal.\n        '
        from cubeless_mcp_ue58.operations import niagara_scratch
        try:
            decoded_inputs = niagara_scratch.decode_registry_input_list(inputs, "inputs")
            decoded_user = niagara_scratch.decode_registry_input_list(user_parameter_inputs, "user_parameter_inputs")
            decoded_parameters = niagara_scratch.decode_registry_input_list(parameter_inputs, "parameter_inputs")
        except Exception as error:
            return reflected_result(niagara_scratch.rejected(error, operation_id))
        return reflected_result(niagara_scratch.insert_niagara_scratch_pad_custom_hlsl_for_pin(
            system_path, target_pin_name, expression, scratch_pad_script_path, scratch_pad_owner_kind,
            scratch_pad_script_index, scratch_pad_name, scratch_pad_emitter_index, scratch_pad_emitter_name,
            target_node_guid, target_node_name, target_node_index, preserve_existing_link_as,
            output_pin_name, signature_name, custom_hlsl_script_usage, decoded_inputs, decoded_user,
            skip_if_already_inserted, replace_existing_custom, rebuild_existing_custom, requires_context,
            delete_unlinked_custom_nodes, delete_unlinked_custom_input_source_nodes,
            allow_source_edit, save, request_compile, decoded_parameters, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def rewire_niagara_custom_hlsl_input_to_module_parameter(system_path: str, input_name: str,
            scratch_pad_name: str | None = None, custom_node_signature_name: str | None = None,
            module_parameter_name: str | None = None, delete_orphaned_source_node: bool = True,
            allow_source_edit: bool = False, save: bool = True, request_compile: bool = True,
            timeout_seconds: float = 60.0,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Rewire the exact selected private Custom HLSL input to a native typed Module ParameterMapGet output/default pair through the public schema attachment path. Preserve all ten original keys/defaults and native known/unknown partial outcomes. Optional null strings normalize to original empty strings. timeout_seconds is validated and retained as caller context only: official MCP owns its transport deadline and no native/game-thread wait is performed. This timing difference and initially unsupported destructive deletion/other source variants remain explicit gaps. Actual graph/type/pair/source/compile/save observations are local authoring evidence; no System definition/dominance, GPU, LWC or natural callback equivalence is claimed.

        Args:
            system_path: Exact already-loaded runtime /Game target System owning the selected private script.
            input_name: Required actual Custom HLSL input name; finite native type/signature/selector admission is required.
            scratch_pad_name: Optional original case-insensitive private-script name filter; omit/null becomes empty.
            custom_node_signature_name: Optional actual Custom signature filter. Original/native order chooses the first matching Custom signature/input within the first matching private script; no uniqueness equivalence is claimed.
            module_parameter_name: Optional Module parameter source. Omit/null/empty retains native original Module.<input_name> fallback; no target metadata fabrication replaces a real binding.
            delete_orphaned_source_node: Original true is retained. A request that would actually delete an unlinked NiagaraNodeInput returns known preflight failure; otherwise the retained true policy causes no deletion. Explicitly false requests no source deletion.
            allow_source_edit: Original false generated/Test-root admission; true still applies private-owner/runtime safety.
            save: Original true request with actual attempt/saved/deferred fields. Never replay a rewire to complete saving.
            request_compile: Original true asynchronous request after actual change; false allows one later composition boundary.
            timeout_seconds: Original60 caller-context seconds, finite positive exact numeric value. It is not forwarded to Engine API and does not alter the official transport timeout.
            operation_id: Optional single journal ID. Repeated completed IDs return the prior result; uncertain outcomes require actual reconciliation.
        """
        from cubeless_mcp_ue58.operations.niagara_scratch import rewire_niagara_custom_hlsl_input_to_module_parameter
        return reflected_result(rewire_niagara_custom_hlsl_input_to_module_parameter(system_path,
            input_name, scratch_pad_name, custom_node_signature_name, module_parameter_name,
            delete_orphaned_source_node, allow_source_edit, save, request_compile, timeout_seconds, operation_id))
