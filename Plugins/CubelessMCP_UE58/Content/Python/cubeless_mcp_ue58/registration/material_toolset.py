"""Named legacy Material tools on the official registry; no external bridge."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult
from cubeless_mcp_ue58.operations import material


@unreal.uclass()
class CubelessMaterialTools(unreal.ToolsetDefinition):
    """Cubeless Material inspection, bounded graph edits and exact collection values/mirror. Graph edits do not explicitly compile/save; collection mutations report their requested/actual saves. Scoped Native shader completion criteria remain distinct from full render, streaming/fanout or cold disk proof."""

    @toolset_registry.tool_call
    @staticmethod
    def resolve_material_graph(material_path: str, graph_type: str = "auto") -> CubelessPythonOperationResult:
        """Resolve a Material or Material Function; return native candidates and loaded identity in data_json.

        Args:
            material_path: Short name, package path or object path; use full paths to avoid ambiguity.
            graph_type: auto, material or function.
        """
        return reflected_result(material.resolve_material_graph(material_path, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def list_material_nodes(material_path: str, graph_type: str = "auto", node_type: str | None = None, desc_contains: str | None = None, include_pins: bool = True) -> CubelessPythonOperationResult:
        """List Material/Function expressions and optional pins, links and root-property connections; no compile/save.

        Args:
            material_path: Material or Material Function name/package/object path.
            graph_type: auto, material or function.
            node_type: Optional expression class or object-name substring filter; omitted/null means no filter.
            desc_contains: Optional expression description substring filter; omitted/null means no filter.
            include_pins: Include pin and connection metadata.
        """
        return reflected_result(material.list_material_nodes(material_path, graph_type, node_type or "", desc_contains or "", include_pins))

    @toolset_registry.tool_call
    @staticmethod
    def analyze_material_graph(material_path: str, graph_type: str = "auto", include_nodes: bool = False, include_usage: bool = True, max_referencers: int = 25) -> CubelessPythonOperationResult:
        """Inspect a Material, Function or Instance with native graph summaries and optional usage hints; no shader compilation proof.

        Args:
            material_path: Material, Material Function or Material Instance name/path.
            graph_type: auto, material or function; Instances use auto/material.
            include_nodes: Include full expression and pin data in the analysis.
            include_usage: Include Asset Registry package referencer hints.
            max_referencers: Maximum referencer packages returned with usage hints.
        """
        return reflected_result(material.analyze_material_graph(material_path, graph_type, include_nodes, include_usage, max_referencers))

    @toolset_registry.tool_call
    @staticmethod
    def list_material_collection_parameter_nodes(material_path: str, graph_type: str = "auto") -> CubelessPythonOperationResult:
        """Inspect collection-parameter expressions, parameter identity/type and mismatch diagnostics without editing.

        Args:
            material_path: Material or Material Function name/path.
            graph_type: auto, material or function.
        """
        return reflected_result(material.list_material_collection_parameter_nodes(material_path, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def get_material_parameter_collection_values(collection_path: str, parameter_names: list[str] = [], include_asset_defaults: bool = True, include_runtime: bool = True) -> CubelessPythonOperationResult:
        """Read MPC asset defaults and available editor/PIE-world values; returned runtime_resolved flags distinguish missing world values. With include_runtime=True, public World.GetParameterCollectionInstance may lazily create a world-owned instance and render resources. The bounded before/after comparison starts after the first getter and covers collection dirty/value payload only; it does not prove world instance creation was absent or GPU rendering completed. include_runtime=False bypasses that getter.

        Args:
            collection_path: Material Parameter Collection name/path.
            parameter_names: Scalar/vector names; omitted or empty array means all. Supply an array, not null. Each trimmed name is limited to 1023 UTF-16 code units; an oversized entry rejects the entire filter before native name construction.
            include_asset_defaults: Include values stored on the collection asset.
            include_runtime: Query the available editor/PIE-world collection instance, allowing the public getter's lazy setup. False bypasses the instance getter. Reported payload equality is not a world-allocation or render-side-effect observation.
        """
        # Copy the reflected array/default; never expose the shared default to
        # an operation. Optional arrays fail UE58's JSON-to-property import.
        return reflected_result(material.get_material_parameter_collection_values(collection_path, list(parameter_names), include_asset_defaults, include_runtime))

    @toolset_registry.tool_call
    @staticmethod
    def set_material_parameter_collection_values(collection_path: str, scalars: str | None = None,
            vectors: str | None = None, set_asset_defaults: bool = True, set_runtime: bool = True,
            save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Set local defaults and/or actual editor-world MPC instance values through the two distinct native paths. Preserve original six behavior keys/defaults and row continuation: bad values report errors, absent names report missing rows, valid rows still run. Invalid rows return success=False with complete_with_errors while retaining successful writes, counts and any actual exact save; postcondition_passed may remain True for that observed prefix. Asset counts count changed defaults; runtime counts count successful public setter returns, including repeated equal-value writes. Runtime-only work does not save the collection. Native reports requested/attempted/saved separately. This first scope requires already-loaded exact native base-free assets and an idle editor outside PIE/SIE; inherited writes and PIE-world selection remain incomplete. PostEditChange may cause implicit engine notifications/shader/render work; no renderer/GPU completion is claimed.

        Args:
            collection_path: Exact already-loaded /Game collection package/object path; load it separately through a named official asset tool if needed. Legacy simple-name loading is not provided by this adapter.
            scalars: Original key represented as optional JSON object string, e.g. {"Intensity":2.5}. Omission/null or JSON null means {}. The standalone domain also accepts the original dictionary. Per-row nonnumeric values reach native error/continuation semantics; whole malformed/nonfinite/oversized JSON rejects before dispatch.
            vectors: Original key represented as optional JSON object string, e.g. {"Tint":{"r":1,"g":0.5,"b":0.25,"a":1}}. Omission/null or JSON null means {}. Original r/x,g/y,b/z aliases are required; a/w is optional and the actual legacy Transparent initialization leaves alpha0 when neither alias is numeric. Unknown fields are ignored according to the legacy value contract. Domain dictionaries remain supported. This encoded reflected representation is not full original public I/O parity.
            set_asset_defaults: Original True default. Change matching local asset defaults and notify only when defaults change; effective inherited default overrides are unsupported.
            set_runtime: Original True default. Use the actual public collection instance of the editor world. GetParameterCollectionInstance may lazily create a world-owned instance; native records the attempted lookup without claiming allocation/render effects were independently measured. Missing instance is reported rather than replaced with a stand-in. False does not fetch a world instance.
            save: Original True default. Save the exact target only after changed asset defaults; no-change/runtime-only work skips save. Native errors retain actual partial counts/state and never automatically retry.
            operation_id: Optional durable request ID; one journal owns the complete named action, and identical completed requests replay the recorded result.
        """
        from cubeless_mcp_ue58.operations.material_collections import set_material_parameter_collection_values
        return reflected_result(set_material_parameter_collection_values(collection_path, scalars, vectors,
            set_asset_defaults, set_runtime, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def mirror_material_parameter_collection(source_collection_path: str, target_collection_path: str,
            parameter_names: str | None = None, copy_defaults: bool = True, preserve_ids: bool = True,
            save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Mirror same-name/same-kind MPC parameters from one already-loaded base-free source to a base-free /Game target. Keep original source order, scalar/vector counts, default/ID updates and conflict warnings. New rows copy source defaults even when copy_defaults=False; preserve_ids=False generates IDs only for new rows and retains existing target IDs. Source-ID conflicts with another same-kind target row produce a warning and skip that replacement. Missing filter names are omitted. No-change work skips notification/save. Native bounded source/target/world observations and actual save flags remain authoritative; no ticker synchronization, reference retargeting, inherited override or renderer proof is claimed.

        Args:
            source_collection_path: Exact already-loaded native /Game or /Engine base-free source package/object path. Adapter dispatches no asset load.
            target_collection_path: Exact already-loaded native base-free /Game target package/object path; Engine/plugin targets are not writable.
            parameter_names: Original optional list key represented as JSON array string, e.g. ["Intensity","Tint"]. Omission/null or JSON null/[] means all. Domain lists remain supported. Native strings trim/filter empty names and deduplicate by FName; nonstring entries reject before mutation as a bounded safety difference from the legacy AsString call. This reflected JSON-string adapter is a representation gap, not complete public I/O parity.
            copy_defaults: Original True default. Update existing target defaults when needed. Added rows retain the original unconditional source-default copy behavior.
            preserve_ids: Original True default. Preserve available source IDs for compatibility; same-kind collision warnings remain visible. False changes IDs only on added rows.
            save: Original True default. Save only the changed exact target. Requested/attempted/saved and retained partial state are returned separately; there is no automatic retry or rollback.
            operation_id: Optional durable request ID; one journal owns this named action. Batch uses the parent journal rather than a nested child operation.
        """
        from cubeless_mcp_ue58.operations.material_collections import mirror_material_parameter_collection
        return reflected_result(mirror_material_parameter_collection(source_collection_path, target_collection_path,
            parameter_names, copy_defaults, preserve_ids, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def connect_material_nodes(material_path: str, from_node_id: str, to_node_id: str, to_input: str, from_output: str | None = None, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Connect an expression output to an input, preserving before/after evidence. No explicit compile/save; editor change may schedule shaders.

        Args:
            material_path: Material or Material Function name/path.
            from_node_id: Source node_key/selector returned by list/analyze.
            to_node_id: Target node_key/selector returned by list/analyze.
            to_input: Target input pin name.
            from_output: Source output pin name; omitted/null/empty means first output.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; reuse returns completed recorded result, never retries an interrupted mutation.
        """
        return reflected_result(material.connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output or "", graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def connect_material_property(material_path: str, from_node_id: str, property: str, from_output: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Connect an expression to a root Material property and inspect the postcondition; Functions are rejected. No explicit compile/save.

        Args:
            material_path: Material name/path.
            from_node_id: Source node_key/selector from list/analyze.
            property: Root property such as BaseColor, Roughness, Normal or MP_BaseColor.
            from_output: Source output name; omitted/null/empty means first output.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.connect_material_property(material_path, from_node_id, property, from_output or "", operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def delete_material_node(material_path: str, node_id: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Delete one expression and disconnect its links, recording observed before/after; no atomic rollback or automatic compile/save.

        Args:
            material_path: Material or Material Function name/path.
            node_id: Exact node_key; native guid:, name:, path:, GUID/name/path selectors reject ambiguity.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.delete_material_node(material_path, node_id, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def layout_material_nodes(material_path: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Apply the engine's Material expression layout and verify structural/position observations; no explicit compile/save or visual-quality guarantee.

        Args:
            material_path: Material or Material Function name/path.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.layout_material_nodes(material_path, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_material_node(material_path: str, expression_class: str, graph_type: str = "auto", node_position: list[int] = [0, 0], selected_asset: str | None = None, properties_json: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one audited native Engine expression with converted property values and before/after verification; no explicit compile/save.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            expression_class: Native Engine MaterialExpression class or supported short name, such as Constant or Multiply. Plugin/generated/editor expression classes are not admitted.
            graph_type: auto, material or function.
            node_position: Two int32 editor coordinates [X,Y]; omitted means [0,0]. Supply an array, not null. Fractional coordinates are rejected.
            selected_asset: Optional asset path consumed by the selected texture/function/collection expression class; omitted/null means none. Runtime reference guards restrict accepted classes, mounts and dependencies.
            properties_json: JSON object of editable properties, e.g. {"R":0.25,"Desc":"source"}; omitted/null means {}. Replaces legacy properties dict at the reflected boundary. At most 64 properties/65536 UTF-16 code units. Property names and direct FName values are limited to 1023 UTF-16 units. Native ImportText for name-bearing, struct or unaudited values must fit 1023 units after encoding. Exact native Custom.AdditionalDefines also accepts up to 64 canonical {"DefineName":"N","DefineValue":"1"} objects with both exact string fields through typed scratch conversion, within the JSON limit; generic values are stored literally without the specialized identifier/alias parser. Unknown fields and nonstring values are rejected. Long raw UE import strings and other large struct arrays remain unsupported. Custom Code/define values permit file-independent #define/#undef/#if/#ifdef/#ifndef/#elif/#else/#endif/#line/#error/#warning directives with case-sensitive spelling and backslash-newline splicing. Dependency scanning checks both source text and the SDK physical-comment/ANSI projection; both physical/logical lines; ambiguous Unicode/SIMD forms, splice-split directive keywords and spliced quoted literals may be conservatively refused. Identifier marker guards remain. Code quoted absolute literal includes and IncludeFilePaths admit a bounded closure of existing mapped literal shader sources through RenderCore public nonfatal cached loading, including parent-relative child includes and exact file-side #pragma once. Authored strings are unchanged. Future-file, computed, generated, top-level relative, angle, user-Code pragma/import/embed and alternate directive forms remain valid incomplete scope; this is not full preprocessing, uncached disk equivalence or shader completion. Pragma/include-query operators and computed bare-hash/stringification/token-paste macro forms remain unsupported. Ordinary function-like macros without those operators remain admitted. Authoring is not HLSL syntax, shader completion or render proof. Native temporary conversion and reference audit precede creation; cleanup is not atomic package rollback.
            operation_id: Optional durable request ID. Interrupted outcomes never retry; an identical completed ID returns the historical result.
        """
        # Use FArrayProperty directly: an optional array is misread as a fixed
        # scalar array by UE58 JsonObjectConverter before optional unwrapping.
        # Copy the reflected array/default before strict operation validation.
        position = list(node_position)
        return reflected_result(material.add_material_node(material_path, expression_class, graph_type, position, selected_asset, properties_json, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_material_node_property(material_path: str, node_id: str, property_name: str, value_json: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Convert and validate a persistent editable expression property before applying it; read back the native value and record partial failure.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            node_id: Exact node_key or unambiguous native guid:/name:/path: selector returned by list/analyze.
            property_name: Editable persistent property on a native Engine expression, at most 1023 UTF-16 code units; const/transient/instanced/export properties are rejected.
            value_json: Exactly one JSON value, e.g. 0.75, true, "caption", {"R":1,"G":0,"B":0,"A":1}, or null. String/path values require JSON quotes. Replaces legacy value Any. Maximum JSON length 65536 UTF-16 units; direct FName values are limited to 1023. Native ImportText for name-bearing, struct or unaudited types must fit 1023 units after encoding. Exact native Custom.AdditionalDefines also accepts up to 64 canonical {"DefineName":"N","DefineValue":"1"} objects with both exact string fields through typed scratch conversion, within the JSON limit; generic values are literal FString values rather than the specialized identifier/alias parser. Unknown fields and nonstring values are rejected. Long raw UE import strings and other large struct arrays remain unsupported. Plain FString values retain the larger JSON limit. Custom Code/define values permit file-independent #define/#undef/#if/#ifdef/#ifndef/#elif/#else/#endif/#line/#error/#warning directives with case-sensitive spelling and backslash-newline splicing. Dependency scanning checks both source text and the SDK physical-comment/ANSI projection; both physical/logical lines; ambiguous Unicode/SIMD forms, splice-split directive keywords and spliced quoted literals may be conservatively refused. Identifier marker guards remain. Code quoted absolute literal includes and IncludeFilePaths admit a bounded closure of existing mapped literal shader sources through RenderCore public nonfatal cached loading, including parent-relative child includes and exact file-side #pragma once. Authored strings are unchanged. Future-file, computed, generated, top-level relative, angle, user-Code pragma/import/embed and alternate directive forms remain valid incomplete scope; this is not full preprocessing, uncached disk equivalence or shader completion. Pragma/include-query operators and computed bare-hash/stringification/token-paste macro forms remain unsupported. Ordinary function-like macros without those operators remain admitted. Authoring is not HLSL syntax, shader completion or render proof; unrelated fields do not rewrite existing shader text. No compile/save completion claim.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; no automatic retry or rollback of uncertain changes.
        """
        return reflected_result(material.set_material_node_property(material_path, node_id, property_name, value_json, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_custom_material_node(material_path: str, code: str, output_type: str = "CMOT_Float3", inputs_json: str = "[]", description: str | None = None, graph_type: str = "auto", node_position_json: str = "[0,0]", additional_defines_json: str = "[]", include_file_paths_json: str = "[]", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one validated Custom HLSL expression using the shared native Add core; no explicit compile/save or shader-validity claim.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            code: Nonempty HLSL source, at most 65536 UTF-16 code units. File-independent #define/#undef/#if/#ifdef/#ifndef/#elif/#else/#endif/#line/#error/#warning directives are admitted with case-sensitive spelling, comment/literal handling and logical-line continuation; quoted absolute literal includes admit a bounded closure of existing mapped literal shader files through RenderCore public nonfatal cached loading, including parent-relative child includes and exact file-side #pragma once without rewriting authored strings. Future-file, computed, generated, top-level relative, angle, user-Code pragma/import/embed and alternative %:/??= spellings remain incomplete; full preprocessing and uncached disk equivalence are not proved. The guard also checks the SDK physical-comment/ANSI projection, on both physical/logical lines, conservatively for ambiguous Unicode/SIMD cases, splice-split directive keywords and spliced quoted literals. Unclosed comments/literals, pragma/include-query operators and bare/stringify/token-paste macro hash forms are refused by the dependency guard; ordinary function-like macros remain admitted; this is not HLSL syntax validation. Creation/readback does not prove shader compilation or rendering.
            output_type: CMOT_Float1/2/3/4 or CMOT_MaterialAttributes; legacy float/number/attributes aliases are accepted. Default CMOT_Float3 is retained explicitly.
            inputs_json: JSON array of at most 64 input-name strings or objects with name/input_name, e.g. ["Tint"]. Valid nonreserved HLSL identifiers must be unique ignoring case and each fit 1023 UTF-16 units. The entire native-encoded input struct array, including field-name/quoting overhead, must also fit 1023 units; larger arrays remain unsupported even within the count cap. Replaces legacy inputs; omitted means [].
            description: Optional Custom description and inherited Desc, at most 4096 UTF-16 code units; omitted/null means empty.
            graph_type: auto, material or function.
            node_position_json: JSON array of exactly two int32 editor coordinates, e.g. [-400,200]; omitted means [0,0].
            additional_defines_json: JSON array of at most 64 objects with name/define_name and optional value/define_value (string, number or boolean; default "1"). The legacy identifier/alias/default and number/boolean text conversion is retained. Define values share the file-independent directive guard with Code; define names retain the existing identifier/marker boundary. The parsed ordered defines use exact native FCustomDefine scratch conversion, so the former aggregate 1023-unit import limit does not apply to this array. The 65536-unit per-JSON and combined-properties limits, count cap and shader/reference audits still apply; raw UE import-string parity is not implied. Replaces legacy additional_defines; omitted means [].
            include_file_paths_json: JSON path array may contain checked existing mapped absolute literal shader paths with a bounded transitive literal closure, parent-relative child includes and exact file-side #pragma once. Future-file, generated and opaque dependencies remain incomplete; this does not prove full shader preprocessing or shader completion. Replaces legacy include_file_paths; omitted means []. Each JSON string is limited to 65536 UTF-16 code units, depth 16 and 8192 values; combined native-encoded Custom properties must also fit 65536 UTF-16 units, including encoding overhead.
            operation_id: Optional durable request ID. An identical completed ID returns its recorded result; interrupted outcomes are never automatically retried. Failed creation cleanup is not atomic rollback.
        """
        return reflected_result(material.add_custom_material_node(material_path, code, output_type, inputs_json, description,
            graph_type, node_position_json, additional_defines_json, include_file_paths_json, operation_id))


    @toolset_registry.tool_call
    @staticmethod
    def compile_and_save_material(material_path: str, graph_type: str = "auto", save: bool = True) -> CubelessPythonAsyncOperationResult:
        """Compile one admitted Material through a finite Native Job and retain actual phase/save outcomes. Named waits for the original current-resource Editor criterion before the optional save; strict full render-map diagnostics are separate. Pending does not trigger a second compile or a writer replay. A Function target completes after UpdateMaterialFunction and its selected save return with known target postconditions; typed compiled remains null. The original data compiled/compilation_finished=True values are defaults, not shader completion proof. SDK-global Function fanout remains separately unverified. A real savedTrue is retained if a later callback, observation or post-save compiler state fails. Direct/raw/ordinary Batch keep their existing synchronous partial/refusal contract. Full RecompileMaterial streaming/Editor fanout, Function fanout, render/cook and cold parity remain incomplete.

        Args:
            material_path: Original short Material/Function name, package or object path. Native owns shared resolution and bounded /Game graph/source admission.
            graph_type: Original auto default; native accepts case-insensitive material, function or material_function and treats other values as auto. Text is forwarded unchanged.
            save: Original True default; one forced exact save after the original Native Editor completion criterion. Actual savedTrue precedes post-save verification. No second save, public timeout or automatic resubmit is added.
        """
        from cubeless_mcp_ue58.operations import material_compilation
        return material_compilation._named_mcp(dict(material_path=material_path,graph_type=graph_type,save=save))


    @toolset_registry.tool_call
    @staticmethod
    def replace_material_collection_references(material_path: str, target_collection_path: str,
            source_collection_path: str = "", graph_type: str = "auto", save: bool = True) -> CubelessPythonAsyncOperationResult:
        """Replace matching collection references through one owned Native Job.

        Collection updates only when references change. Material uses the current Editor criterion.
        Function completes after its UpdateMaterialFunction and selected changed-only save return with target postconditions; typed compiled remains null and legacy data True values are defaults.
        Bounded loaded-interface receipts do not verify shader/global fanout completion. Unmatched/invalid rows and actual SavedTrue/partial/unknown remain Native authority.
        Direct/raw/ordinary Batch keep their synchronous path; no second compile/save or writer replay.

        Args:
            material_path: Original Material/Function name or path.
            target_collection_path: Target Material Parameter Collection path.
            source_collection_path: Original empty default matches all admitted sources.
            graph_type: Original auto default and forwarded text.
            save: Original True; save only changed references after the Native criterion.
        """
        from cubeless_mcp_ue58.operations import material_references
        return material_references._named_mcp("replace_material_collection_references",dict(material_path=material_path,target_collection_path=target_collection_path,source_collection_path=source_collection_path,graph_type=graph_type,save=save))

    @toolset_registry.tool_call
    @staticmethod
    def replace_material_texture_references(material_path: str, replacements: dict[str, str],
            graph_type: str = "auto", save: bool = True) -> CubelessPythonAsyncOperationResult:
        """Replace texture references and resume the same Native compiler/save phases.

        Texture aliases, sampler refresh, invalid/unmatched rows and mandatory compile including replacement0 stay in the original core.
        Material Editor completion differs from strict render-map readiness. Function completes its mandatory UpdateMaterialFunction even with replacement0; only changed references request the selected save.
        Function target-known uses returned update/selected save and current target postconditions; typed compiled remains null, legacy data True values are defaults, and shader/global fanout remains unverified.
        SavedTrue survives later failure. Direct/raw/ordinary Batch remain synchronous; this Named path starts once without replay.

        Args:
            material_path: Original Material/Function name or path.
            replacements: Original string-to-string object; preserved as an object in the private carrier.
            graph_type: Original auto default and forwarded text.
            save: Original True; Native owns changed-reference save admission and actual result.
        """
        from cubeless_mcp_ue58.operations import material_references
        return material_references._named_mcp("replace_material_texture_references",dict(material_path=material_path,replacements=dict(replacements),graph_type=graph_type,save=save))

    @toolset_registry.tool_call
    @staticmethod
    def replace_material_collection_parameters(material_path: str, use_runtime_values: bool = True,
            save: bool = True) -> CubelessPythonAsyncOperationResult:
        """Bake collection expression parameters through the original Native graph core and one continuation.

        Runtime/default lookup, GUID/name mapping, rewiring and deletion retain actual partial state.
        Mandatory compile including replacement0 and optional save wait for the original Editor criterion.
        Native SavedTrue/unknown/failure stays authoritative; no replay, second save or global compiler cancellation.

        Args:
            material_path: Original Material-only name or path; no graph_type is added.
            use_runtime_values: Original True; current admitted Editor-world lookup may create lazy state.
            save: Original True; Native owns the one exact save and subsequent verification.
        """
        from cubeless_mcp_ue58.operations import material_collection_parameters
        return material_collection_parameters._named_mcp(dict(material_path=material_path,use_runtime_values=use_runtime_values,save=save))

    @toolset_registry.tool_call
    @staticmethod
    def set_material_texture_sample_sampler_type_mcp(material_path: str, texture_path: str,
            sampler_type: str = "SAMPLERTYPE_LinearColor", dry_run: bool = False,
            save: bool = True, compile: bool = True) -> CubelessPythonOperationResult:
        """Set sampler type on all TextureSample expressions referencing the original texture identity.

        Args:
            material_path: Original Material asset path, normalized and loaded by the native operation.
            texture_path: Original Texture asset path used for exact loaded-object identity filtering.
            sampler_type: Original SAMPLERTYPE_LinearColor default; native Color, LinearColor and Masks aliases retain their original case-insensitive matching.
            dry_run: Original False default. Reports nodes that would change without sampler writes or explicit compile/save.
            save: Original True default. Changed non-dry work performs the original direct save independently of compiler completion; actual attempted/saved flags remain separate from action success.
            compile: Original True default. Changed non-dry work may request the original rendering compile; request is not completed compilation.

        This finite route admits ordinary /Game Materials and matching native Engine TextureSample subclasses with authored-property readback. Custom/non-native classes and unsupported shared-package variants remain guarded.
        Matching nodes skip work, including compile/save. Native preserves would-change rows, actual write/call counters,
        nullable compiler observation and any real save result before failure. A returned savedFalse may coexist with
        original action success; it never proves save completion. No blocking wait, second save/compiler or automatic replay.
        """
        from cubeless_mcp_ue58.operations.material_sampler import set_material_texture_sample_sampler_type_mcp
        return reflected_result(set_material_texture_sample_sampler_type_mcp(material_path, texture_path,
            sampler_type, dry_run, save, compile))


    @toolset_registry.tool_call
    @staticmethod
    def refresh_material_cached_expression_data(material_path: str, save: bool = True) -> CubelessPythonAsyncOperationResult:
        """Refresh current Material cached expressions, observe Native compile completion, and retain actual save/dependency results on one Named Job.

        Args:
            material_path: Original Material path/name; Native owns material-only resolution and bounded /Game admission.
            save: Original True default; one exact save after the original current-resource Editor criterion, followed by actual Registry/dependency observations.

        Public UpdateCachedExpressionData may preserve previously loaded cached data. A stale FunctionInfos path that this public route cannot resolve remains an explicit incomplete failure; no private const_cast clear is performed. Named resumes pending completion on the same Job with no public timeout or invented deadline. SavedTrue is preserved before later unsafe, shader or dependency failure. Direct/raw/ordinary Batch retain their existing synchronous partial/refusal results. Whole RecompileMaterial streaming/Editor fanout and cold/render parity remain incomplete. No second compile/save, global compiler cancellation or automatic replay is added.
        """
        from cubeless_mcp_ue58.operations import material_cache
        return material_cache._named_mcp(dict(material_path=material_path,save=save))


    @toolset_registry.tool_call
    @staticmethod
    def set_material_parameter_collection_sync(action: str = "status", source_collection_path: str = "",
            target_collection_path: str = "", parameter_names: list[str] = [], interval_seconds: float = 0.1,
            set_asset_defaults_on_enable: bool = False, save_on_enable: bool = False) -> CubelessPythonOperationResult:
        """Observe, enable or stop the one owned Editor-world material collection sync controller.

        Args:
            action: Original public default status; lowercase-only status/enable/start/disable/stop aliases are native.
            source_collection_path: Original empty default; required only for enable/start and resolved natively.
            target_collection_path: Original empty default; enable/start accepts an isolated /Game target in the finite native scope.
            parameter_names: Original nullable string-array/default null. Omission, null and [] select all matching names.
            interval_seconds: Original 0.1 default; native applies the original finite float32 minimum-interval clamp.
            set_asset_defaults_on_enable: Original False; optional initial asset-default changes are separate from runtime copying.
            save_on_enable: Original False; native attempts one forced save only with set_asset_defaults_on_enable on start.

        A scoped field converter publishes the exact nullable array/default null while the internal direct array initializes
        empty; copied unreal.Array values preserve the whole original name list. Existing MPC getter defaults are unchanged.
        Status reads the controller receipt without a Python journal. Start/stop share one journal; native owns initial
        copy, persistent ticker and lifecycle cleanup. An enabled current point or savedFalse does not prove all future
        ticks/render completion. Stop cancels owned handles without restoring runtime values. Editor/PIE world following,
        weak-asset reload, whole-world/cache effects and full original persistence/render behavior remain incomplete.
        """
        from cubeless_mcp_ue58.operations.material_collection_sync import set_material_parameter_collection_sync
        names = None if parameter_names is None else list(parameter_names)
        return reflected_result(set_material_parameter_collection_sync(action, source_collection_path,
            target_collection_path, names, interval_seconds, set_asset_defaults_on_enable, save_on_enable))


    @toolset_registry.tool_call
    @staticmethod
    def expand_material_function_calls(material_path: str, node_id: str = "", recursive: bool = True,
            exclude_engine_functions: bool = True, max_passes: int = 8, save: bool = True,
            allow_partial_save: bool = False) -> CubelessPythonOperationResult:
        """Expand selected Material Function calls and retain the complete bounded native report.

        Args:
            material_path: Original Material name/path; native owns literal resolution and finite target admission.
            node_id: Original empty default selects all calls, or an exact original node key/ID selects one.
            recursive: Original True; process newly created calls until complete or the native pass limit.
            exclude_engine_functions: Original True; skip functions under /Engine or /Script as originally requested.
            max_passes: Original integer 8; native owns the original clamp and bounded expansion admission.
            save: Original True; native owns the direct forced save according to the actual changed/error report.
            allow_partial_save: Original False; preserve the original successful-partial save decision when errors exist.

        Returned data preserves actual passes, expanded/skipped/error rows, composite allocations/retirements,
        rollback and real save receipts. A completed report can contain expansion errors, rollback or savedFalse;
        success does not mean every call expanded or shaders/rendering completed. PostEdit/cache/compiler effects
        are implicit and separately observed; this method adds no explicit compiler, FinishCompilation, second
        save, Python rollback or automatic replay. A bounded native MaterialFunctionInstance source route resolves the actual Parent chain to a native MaterialFunction expression owner and protects both roles read-only. The nullable Base cache is observed separately from the computed base. Like the original expansion algorithm, this copies base expressions without baking instance parameter overrides; no shader/render equivalence is claimed. Wider graph/function/lifetime variants remain incomplete.
        """
        from cubeless_mcp_ue58.operations.material_function_expansion import expand_material_function_calls
        return reflected_result(expand_material_function_calls(material_path, node_id, recursive,
            exclude_engine_functions, max_passes, save, allow_partial_save))
