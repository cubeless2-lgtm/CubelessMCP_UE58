"""Official Registry schemas. Work logic belongs to operations, not these wrappers."""

import json
import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from .local_api_result import CubelessLocalCommandAsyncResult
from cubeless_mcp_ue58.operations import variable_lifecycle, state


@unreal.uclass()
class CubelessPythonDiagnostics(unreal.ToolsetDefinition):
    """Cubeless editor Python startup and operation journal diagnostics."""

    @toolset_registry.tool_call
    @staticmethod
    def health() -> str:
        """Return JSON with engine version and each Cubeless Python registry/schema check; makes no asset changes."""
        from cubeless_mcp_ue58.registration import registration_status
        from cubeless_mcp_ue58 import __version__
        return json.dumps({"plugin": "CubelessMCP_UE58", "plugin_version": __version__,
                           "engine_version": unreal.SystemLibrary.get_engine_version(),
                           "registration": registration_status(), "asset_mutation": False})

    @toolset_registry.tool_call
    @staticmethod
    def get_operation(operation_id: str) -> str:
        """Return a persisted operation record as JSON. Unknown or interrupted records never authorize automatic retry.

        Args:
            operation_id: Request ID passed to a mutation tool, or generated ID returned by it.
        """
        from cubeless_mcp_ue58.operations.state import get_operation
        return json.dumps(get_operation(operation_id), ensure_ascii=False, sort_keys=True)


    @toolset_registry.tool_call
    @staticmethod
    def execute_command_json(command_type: str, params_json: str = "") -> CubelessLocalCommandAsyncResult:
        """Execute one original command or original batch using current local actions, without TCP or server RPC. Official MCP waits for the SDK future and returns the JSON string status/result/error envelope. result projects an existing legacy_result object or concrete data fields such as actors. Original-shaped Batch counts/results/child response envelopes are projected from the existing summary. Entire child and parent standard11 receipts remain separate; exact every-handler payload equivalence is not claimed. Existing command support bounds, Batch preflight, transactions, unknown-stop, compile/save and asynchronous waits remain authoritative. No automatic retry or cancellation. Direct Python callers receive a future; off-game-thread blocking and legacy TCP lifecycle are not implemented.

        Args:
            command_type: Original command alias or current concrete Batch action. Whitespace is trimmed. batch consumes params.commands and stop_on_error.
            params_json: One JSON object; empty means {}. Removes the old hidden ticker-dispatch key. Current bounded Batch preflight applies.
        """
        from cubeless_mcp_ue58.operations.local_api import execute_command_json
        return execute_command_json(command_type, params_json)

    @toolset_registry.tool_call
    @staticmethod
    def get_python_api_info_json() -> str:
        """Read local Editor API entry/result/dependency contracts. Old TCP server_running/server_port are null with an explicit removed explanation; these are not official HTTP server status. No assets, server startup or domain command dispatch."""
        from cubeless_mcp_ue58.operations.local_api import get_python_api_info_json
        return get_python_api_info_json()

    @toolset_registry.tool_call
    @staticmethod
    def get_mcp_capabilities() -> str:
        """Read the original 12 optional route identities using actual current Cubeless action bindings, loaded Editor module and enabled Engine plugins. Replaces UE56 optional-provider modules with existing local adapters. Availability is metadata only, not official registration, schema or behavior proof. Does not report TCP state or modify assets."""
        from cubeless_mcp_ue58.operations.local_api import get_mcp_capabilities
        return get_mcp_capabilities()


@unreal.uclass()
class CubelessBlueprintTools(unreal.ToolsetDefinition):
    """Cubeless Blueprint lifecycle tools with explicit compile, save, partial-failure and retry results."""

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_interface(blueprint_name: str, interface_class: str,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Preserve the original two-input interface operation, with the existing optional durable operation_id on this named surface. The exact NiagaraParticleCallbackHandler route on its supported ActorComponent Blueprint remains unchanged. A separate Native route admits the loaded exact /Script/UMG.UserObjectListEntry interface on a bounded saved-clean fresh WidgetBlueprint directly derived from UserWidget. Native uses public ImplementNewInterface and actual interface metadata/required-parent compatibility, source/current and target readback; Python does not resolve a receiver graph for this route. SDK structural graph or skeleton effects and actual partial/unknown counters remain distinct from explicit full compile/save0. Existing Niagara SaveOnCompile=Never and unchanged-reuse rules remain intact. The original class strings are passed to Native admission, including its original dependency guard, without a new Python interface whitelist. No separate receiver callback node, rollback, replay or settings mutation is added. A separate loaded saved-clean Game Blueprint Interface profile admits a direct-UInterface BPTYPE_Interface source with finite 0..32 own compiled/skeleton functions and matching source signature graphs. Each function preserves its actual by-value primitive inputs, outputs and source declaration defaults; fields remain within the existing bool/int/int64/float/double/string/name/text scalar classes and 16 parameters per function. Native derives each function override/event/Anim predicate from the installed SDK. Ordinary event signatures require no implementation graph or synthetic event node; non-event callable functions require their own interface graph. Each native graph has Entry1 and Result1 only when its signature has an output. Type-based stock terminator defaults are distinct from source declaration defaults. Source own variables, implemented interfaces, ubergraphs, macros and delegates remain outside this profile. The target is the bounded saved-clean direct-Actor Blueprint with its captured stock initialization, own Scene SCS root and unlinked default EventGraph events, without live/derived consumers or additional authored variables/functions/macros/delegates/timelines. The same Game BPI source/signature path also admits a bounded loaded saved-clean fresh WidgetBlueprint directly derived from UserWidget: own WidgetTree, an empty native Canvas root with no slot, no Bindings or Animations, and zero or one EventGraph with stock unlinked default events. Widget compiler class roles are reacquired; existing WidgetTree, Bindings, Animations and WidgetVariableNameToGuidMap are preserved. Interface function graphs and partial/unknown results use the same Game BPI implementation; no separate synthetic receiver node or explicit full compile/save is added. Game BPI implementation additionally admits a saved-clean direct UserWidget with an existing bounded native Canvas/Button/Text tree and native Canvas/Button slots, preserving authored values/FText, widget descriptors/GUIDs, existing graph contracts and separate compiled-tree roles. Bindings/animations, arbitrary variables/functions, live/derived UI and wider/custom trees remain incomplete; populated-tree use awaits validation. This new Widget owner route is implemented / usage validation pending. Native resolves the original already-loaded canonical generated-class path or BPI asset object/package path without a source loader or short-name search. Public ImplementNewInterface creates the derived 0..N implementation graphs under ImplementedInterfaces[0].Graphs, rather than appending them to target FunctionGraphs. Implicit compilation may reconstruct target classes and pins; Native reacquires these roles and preserves captured graph identity, authored defaults, existing links and the Scene root. Already-present interfaces retain the original SDK false return plus explicit structural mark. Partial new graph prefixes are reported without cleanup, retry or success promotion. Source BPI/class/function/graph/file observations and the actual target signature/current/partial prefix remain Native authority; nullable existing source CDOs are not warmed. Original SDK return and explicit structural modification remain distinct from implicit skeleton effects and explicit full compile/save0. Broader/inherited BPI parents, multiple selected interfaces, reference/container/custom/Anim signatures, authored target bodies or broader owners, source loading, runtime invocation and full interface parity remain incomplete. Other Game Blueprint Interface variants, broader native interfaces/parents/signatures and full generic interface/runtime/receiver pipeline parity remain incomplete.

        Args:
            blueprint_name: Exact /Game path or existing simple-name normalization under /Game/Blueprints. The existing Niagara route requires its directly native ActorComponent Blueprint/EventGraph; the separate UserObjectListEntry route requires the bounded loaded saved-clean fresh WidgetBlueprint/direct UserWidget profile. The Game BPI profile uses the bounded direct-Actor or direct-UserWidget target described above, including the separately admitted populated native Canvas/Button/Text tree. Broader hosts remain incomplete.
            interface_class: Existing exact /Script/Niagara.NiagaraParticleCallbackHandler or loaded exact /Script/UMG.UserObjectListEntry in its separate Native profile. The separate Game BPI profile accepts its already-loaded canonical generated-class path or BPI asset object/package path. Other original interface requests still reach Native admission rather than a Python whitelist; aliases, broader loading/signatures and other Game BPI variants remain incomplete.
            operation_id: Optional durable request ID. Identical normalized input replays historical results; interrupted/uncertain requests never automatically retry.
        """
        from cubeless_mcp_ue58.operations.particle_receiver import add_blueprint_interface
        return reflected_result(add_blueprint_interface(blueprint_name, interface_class, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def duplicate_blueprint_to_temp(blueprint_name: str, target_path: str, required_root: str = "/Game/_MCP_Temp",
            dry_run: bool = True, overwrite_existing: bool = False, allow_dirty_source: bool = False,
            compile: bool = False, save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Clone an admitted saved ordinary Blueprint below an explicit root, defaulting to dry-run. Preserves the legacy defaults and named command. Source file/loaded state and clone class, K2 graph topology/native pin types, variable descriptors/current CDO are checked; GUIDs can legitimately change in the clone. Requires SaveOnCompile=Never and source control disabled because public duplication implicitly compiles and has no SkipSave option. Settings are never changed. Loading and engine duplication may have callback/selection effects; full asset parity, cold reload and packaged execution are not claimed by this call.\n\n        Args:\n            blueprint_name: /Game Blueprint package/object path, export-text or .uasset spelling, or simple name under /Game/Blueprints. Requires exact UBlueprint asset class, a native runtime parent or an inspected saved clean compiled Actor/ActorComponent/SceneComponent Blueprint parent chain, current generated class/CDO and clean saved source dependencies. Parent source hashes/loaded state are independently observed. Observation is bounded to 64 graphs, 2048 nodes per graph/4096 total and the existing 4096-variable/package-audit limits. Derived asset classes remain unsupported; component-template/comment content is not fully audited.\n            target_path: /Game package/object path strictly below required_root, at most 1023 UTF-16 units without controls. Source-equals-target always fails. Without overwrite, an existing registry/disk/loaded package or directory fails. With overwrite, an existing saved clean ordinary Blueprint may be deleted before duplication through the bounded native/runtime-parent and loaded-role checks. Wider target asset classes and reference closures remain valid original scopes not yet migrated.\n            required_root: Defaults /Game/_MCP_Temp; explicit empty preserves that legacy default. Target must be below this directory, not equal to it. Use a dedicated owned folder for verification.\n            dry_run: Defaults True. True observes source and target presence with no delete, duplicate, compile or save; it does not perform destructive target admission or certify that replacement can execute. Requested compile/save flags remain visible with attempted False. Loading may run PostLoad callbacks.\n            overwrite_existing: Defaults False. True preserves the original DeleteAsset then DuplicateAsset ordering for an admitted existing ordinary Blueprint, under the same durable operation and Batch action. Requires a saved clean compiled target, no known external package referencers and no target-class instances or derived CDOs other than its existing CDO; public package queries are not a complete memory/editor/Undo reference audit. The public force-delete may clear references, editors or Undo and run GC. Old selected handles are retired before dispatch; actual delete/clone/compile/save prefixes and source/current checks survive failure. A False or invalid delete return stops without further Engine observations. A successful delete followed by a failed clone leaves a partial replacement with no automatic restore or retry. An absent target follows the existing fresh path. Wider dirty/reference/instance/asset-type closures remain incomplete; no atomic rollback or total engine callback preservation is claimed.\n            allow_dirty_source: Defaults False. True admits an already-saved ordinary source with current compiled/generated-class/CDO observations, duplicating its held RAM object without source compile/save. Source identity, observed K2/variable/CDO values, dirty/status, file bytes and authored metadata are guarded; clone Namespace is checked, while DuplicateTransient DisplayName/Description are source-only. First-save, uncompiled and full unobserved RAM authored parity remain incomplete. Clean sources may pass either value.\n            compile: Defaults False. Requests one additional explicit diagnostic compile with node refresh; engine PostDuplicate implicit compilation is reported separately even when False.\n            save: Defaults True. Requests one explicit save after clone readback and any requested compile. False keeps the new asset unsaved and verifies no target file appeared. Save failure fails the whole result and preserves known creation state.\n            operation_id: Optional durable replay ID; omitted/null generates a returned ID. Identical ID/arguments replay historical results; uncertain clone/save outcomes never automatically retry.\n        \n\nThe shared component-parent inspector retains actual nullable SCS roles and distinguishes failed superclass lookup from normal ancestry termination. New component-parent duplication is implemented / usage validation pending. Existing overwrite, partial replacement and no-retry contracts remain unchanged.'
        from cubeless_mcp_ue58.operations.blueprint_duplicates import duplicate_blueprint_to_temp
        return reflected_result(duplicate_blueprint_to_temp(blueprint_name, target_path, required_root, dry_run,
            overwrite_existing, allow_dirty_source, compile, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_variable(blueprint_name: str, variable_name: str, variable_type: str, is_exposed: bool = False,
            default_value_json: str | None = None, category: str | None = None, tooltip: str | None = None,
            friendly_name: str | None = None, type_object: str | None = None, is_array: bool = False,
            metadata_json: str = "{}", operation_id: str | None = None, variable_guid: str | None = None,
            is_editable: bool | None = None, is_transient: bool = False) -> CubelessPythonOperationResult:
        """Exact already-loaded native SkeletalMeshComponent arrays require is_array=True and is_transient=True with omitted/empty-array defaults only through a separate native route. Scalar/nonempty defaults, default setter and global RuntimeObject support remain unchanged. Add one exactly named declared member variable with type-checked default and metadata. Conflicts fail rather than silently renaming. No full compile or save is requested; implicit skeleton compilation is reported separately. Unsupported types/defaults/dependencies fail before editing. Full legacy type coverage remains separately tracked. A separate scalar resource declaration admits only exact native Engine.MaterialInterface, Engine.MaterialInstanceDynamic, Engine.Texture, Engine.TextureRenderTarget2D and Niagara.NiagaraSystem. TextureRenderTarget2D is an exact scalar declaration with its own native marker, typed current property and default audit; it does not create a render resource, accept a subclass or authorize render-target arrays. The NiagaraSystem branch admits only a saved clean runtime asset reference with its explicit dependency audit; it does not invoke Niagara, load a populated component template, widen general RuntimeObject or authorize other Niagara asset types. This does not widen RuntimeObject, resource arrays, subclasses, inherited/external members or graph pin literals. Add omission and explicit JSON null both retain the existing no-supplied-default adapter; set_blueprint_variable_default keeps explicit null as a typed null assignment. Non-null resource defaults use the existing class-compatible persistent asset/dependency audit and typed text roundtrip, including actual load effects and partial receipts; transient runtime MIDs are not constructed or made into persistent default assets. Existing is_transient and metadata/default verification remain in force. The original SmokeVolume per-variable RT subclass, flags and defaults are not recovered by this type admission.

        Args:
            blueprint_name: Exact runtime-safe /Game Blueprint package/object path, or simple name under /Game/Blueprints. No registry-wide ambiguous name search.
            variable_name: Exact new ASCII identifier, 1..100 characters, not None. Existing names and identity conflicts fail.
            variable_type: bool/boolean, int/integer, int64/integer64, float/single, double/real, string, name, text, Vector, Rotator, Transform, or audited struct/scriptstruct, enum/byteenum, object/class with type_object. Finite map_string_vector and map_string_double tokens additionally admit exact native String-key/Vector-value and String-key/double-value Maps only with is_transient=True, is_array=False, empty type_object and omitted/empty-object default. It preserves the existing signature and differs from the old unsupported container_type/map_value_type recipe spelling. Other maps, delegates, interfaces, sets and soft-reference descriptors remain unsupported. Native typed defaults must survive an exact engine text export/import roundtrip; values that cannot are rejected before editing.
            is_exposed: Defaults False. True enables instance editing; combined with is_editable using the legacy logical OR.
            default_value_json: Explicit JSON string adapter for legacy Any. Omitted/null, or the JSON text null, means no supplied add default, matching legacy default_value=None. Other values retain their JSON types. Text at most 65536 UTF-16 units, depth 8 and 1024 values; duplicate keys, nonfinite values, magnitudes above 2^53-1, NUL and unpaired surrogates fail.
            category: Optional category, at most 1023 UTF-16 units. Omitted/null/empty means no override.
            tooltip: Optional tooltip, at most 4096 UTF-16 units; line breaks permitted. Omitted/null/empty means no override.
            friendly_name: Optional display name, at most 1023 UTF-16 units. Omitted/null/empty means no override.
            type_object: Optional exact runtime-safe type path when required by the descriptor. Object/class without a type defaults to Object. The finite native object catalog includes exact Engine Actor, Pawn, Controller and PlayerController; this does not admit arbitrary subclasses. The separate resource slice additionally admits scalar MaterialInterface, MaterialInstanceDynamic and Texture only; LinearColor declaration/defaults continue through the existing audited struct path. Referenced assets require a complete saved dependency audit; unaudited plugin/native-module references fail.
            is_array: Legacy array request, default False. Explicit array defaults have at most 256 elements with recursively typed validation; unsupported combinations fail. Arbitrary raw Unreal import text is not accepted for aggregates.
            metadata_json: JSON object, default {}, at most 64 string/bool/finite-number values. Keys at most 1023 UTF-16 units and case-insensitively unique; string values at most 4096 units. Unsupported metadata effects fail explicitly.
            operation_id: Optional durable replay ID. The same ID and normalized arguments return the historical result; uncertain operations never automatically retry.
            variable_guid: Optional legacy internal-command GUID. Must be nonzero, valid and unique; omitted/null/empty lets the engine create one.
            is_editable: Optional legacy internal-command alias. Effective instance editing is is_exposed OR is_editable; False does not override True is_exposed.
            is_transient: Defaults False. True requests CPF_Transient on the declared variable via public UE5.8 API. Stored and current generated-property flags are observed separately; current compile may be absent/stale until a named full compile. This recovers an ignored UE5.6 pipeline request, not a claim of working legacy transient support or complete serializer equivalence.
        """
        from cubeless_mcp_ue58.operations.variables import add_blueprint_variable
        return reflected_result(add_blueprint_variable(blueprint_name, variable_name, variable_type, is_exposed,
            default_value_json, category, tooltip, friendly_name, type_object, is_array, metadata_json,
            operation_id, variable_guid, is_editable, is_transient))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_blueprint_variables(blueprint_name: str) -> CubelessPythonOperationResult:
        """Read up to 4096 declared/generated variable rows, GUIDs, pin types, flags, metadata and category order. Reports stored defaults separately from an existing generated CDO, which may be stale. Does not create a CDO, compile or save; loading can perform PostLoad work and is not a side-effect-free guarantee. Unsupported CDO values are marked unavailable.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path, or simple name under /Game/Blueprints. Complete object path at most 1023 UTF-16 units without controls; Engine/plugin paths are unsupported in this slice.
        """
        from cubeless_mcp_ue58.operations.variables import inspect_blueprint_variables
        return reflected_result(inspect_blueprint_variables(blueprint_name))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_blueprint_variable_metadata(blueprint_name: str, variable_name: str) -> CubelessPythonOperationResult:
        """Read the exact stored ordered metadata and convenience map for one unambiguous local declared/generated variable. Includes scope, index, GUID and dirty/status observations. Missing or ambiguous variables fail. Does not compile or save; inherited member completeness is not claimed.

        Args:
            blueprint_name: Exact /Game Blueprint package/object path, or simple name under /Game/Blueprints.
            variable_name: Nonempty member variable selector, at most 1023 UTF-16 units without controls; native FName matching applies.
        """
        from cubeless_mcp_ue58.operations.variables import inspect_blueprint_variable_metadata
        return reflected_result(inspect_blueprint_variable_metadata(blueprint_name, variable_name))

    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_variable_default(blueprint_name: str, variable_name: str, default_value_json: str,
            compile: bool = True, save: bool = True, dry_run: bool = False, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Set the type-checked default of one locally declared member variable. Preserves legacy public compile/save defaults True. Native snapshots, compiler/save outcomes and partial failures are retained. Full compilation may consume the stored default into the CDO, so an empty stored default afterwards is not proof of failure. For exact MaterialInterface/MID/Texture/TextureRenderTarget2D/NiagaraSystem SetDefault with compile=True, other own or generated descriptors with nonempty pending defaults are refused before mutation, compilation or saving; first perform a separate explicit compile/save, then make a new explicit request, without automatic retry. For the finite resource declarations, explicit null and compatible audited persistent asset paths keep this existing typed default path; no runtime MID/resource is created. The add tool separately normalizes JSON null to no supplied add default. Default validation may load referenced assets, so dry_run is not a claim that source loads or callbacks never occur. Stored/default/generated-property observations, requested compile/save and any failed or unknown partial prefix remain distinct.

        Args:
            blueprint_name: Runtime-safe /Game Blueprint package/object path, or simple name under /Game/Blueprints.
            variable_name: Existing locally declared member; inherited/default-only members are not silently modified.
            default_value_json: Required JSON string adapter for legacy Any. Null text remains an actual JSON null for native type validation. Uses the add tool's JSON/type limits. Clearing an existing nonempty string to an empty default is currently unsupported because the compiler treats empty stored text as no pending change; it fails explicitly. New empty string defaults remain supported.
            compile: Defaults True. Full compile uses SkipSave; False defers it for a final named batch compile. Generated CDO may remain stale until compiled.
            save: Defaults True. Explicit save occurs after successful requested compilation; False never requests saving. Save without compile retains the stored declaration state rather than proving current CDO value.
            dry_run: Defaults False. True validates and reports without mutation, full compile or save; requested compile/save flags remain visible with attempted False.
            operation_id: Optional durable ID. Same normalized request replays its recorded result. Unknown or interrupted outcomes never automatically retry.
        """
        from cubeless_mcp_ue58.operations.variables import set_blueprint_variable_default
        return reflected_result(set_blueprint_variable_default(blueprint_name, variable_name, default_value_json, compile, save, dry_run, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def create_blueprint(name: str, parent_class: str, package_path: str = "/Game/Blueprints", operation_id: str | None = None) -> CubelessPythonOperationResult:
        'Create an unsaved Blueprint without overwriting. Requires SaveOnCompile=Never because the engine factory implicitly compiles without a public SkipSave option. Settings are never changed. Parent must resolve; known MCP/editor-tool parent chains are rejected.\n\n        Args:\n            name: New asset identifier, not a path. Complete new object path must be shorter than 1024 UTF-16 units.\n            parent_class: Native class name (Actor/Pawn etc.) or native class path; empty means Actor. Also accepts an exact /Game/Package.Package_C Actor/ActorComponent/SceneComponent Blueprint class only when it and every generated parent are saved, clean, compiled and pass the runtime dependency/class guard. Parent source SHA/dirty/status are observed before/after factory work. No fallback class or automatic parent compile/save.\n            package_path: Directory under /Game; defaults to /Game/Blueprints (explicit empty also uses that legacy default). No object suffix.\n            operation_id: Unique request ID for durable replay protection. Omitted/null generates an ID returned in the result. Reuse only with identical arguments.\n\n        Returns:\n            Structured result. data_json includes name, path, asset_path and actual parent; creation remains unsaved.\n        \n\n        Exact parent_class=/Script/Engine.BlueprintFunctionLibrary selects the stock BlueprintFunctionLibraryFactory and requires BPTYPE_FunctionLibrary, exact native parent, no SCS and no own members. Public readback guards returned identity/type and optional generated/skeleton classes. Actual factory/creation prefixes and unknown failures remain without cleanup/retry; creation stays unsaved. Implicit factory compilation is separate from explicit compile/save. Other BFL aliases and complete library/runtime behavior remain incomplete.\n        \n\nComponent-parent admission observes the existing nullable SCS and resolved generated/skeleton/parent/CDO roles without assuming Actor construction-script ownership. New component-parent creation is implemented / usage validation pending; source compilation and saving are never added.'
        from cubeless_mcp_ue58.operations.blueprint import create_blueprint
        return reflected_result(create_blueprint(name, parent_class, package_path, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_blueprint(blueprint_name: str) -> CubelessPythonOperationResult:
        """Read Blueprint identity, status, graph/variable names and direct parent chain. Does not compile or save.

        Args:
            blueprint_name: Full /Game package or object path; a bare name means /Game/Blueprints/name.

        Returns:
            data_json describes loaded editor state. validation_json explicitly does not claim disk reload or complete dependency verification.
        """
        from cubeless_mcp_ue58.operations.blueprint import inspect_blueprint
        return reflected_result(inspect_blueprint(blueprint_name))

    @toolset_registry.tool_call
    @staticmethod
    def compile_blueprint(blueprint_name: str, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Compile one Blueprint with native compiler diagnostics; does not save or refresh nodes.

        Args:
            blueprint_name: Full /Game package/object path, or bare name under /Game/Blueprints.
            operation_id: Durable request ID; omitted/null generates a returned ID. Never automatically retry an uncertain result.
        """
        from cubeless_mcp_ue58.operations.blueprint import compile_blueprint
        return reflected_result(compile_blueprint(blueprint_name, False, False, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def compile_and_save_blueprint(blueprint_name: str, save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Compile once and optionally save once after successful compilation. Save failure fails the overall result.

        Args:
            blueprint_name: Full /Game package/object path, or bare name under /Game/Blueprints.
            save: Save after successful compilation; defaults to legacy True.
            operation_id: Durable request ID for identical-request replay protection; omitted/null generates a returned ID.
        """
        from cubeless_mcp_ue58.operations.blueprint import compile_blueprint
        return reflected_result(compile_blueprint(blueprint_name, save, False, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def compile_and_validate_blueprint(blueprint_name: str, save: bool = False, refresh_nodes: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Refresh nodes if requested, compile and collect native diagnostics; save only after successful compilation.

        Args:
            blueprint_name: Full /Game package/object path, or bare name under /Game/Blueprints.
            save: Save on successful compile; defaults to legacy False.
            refresh_nodes: Refresh all nodes before compile; defaults to legacy True and may change graph serialization.
            operation_id: Durable request ID; omitted/null generates a returned ID. Inspect a prior interrupted mutation before retry.
        """
        from cubeless_mcp_ue58.operations.blueprint import compile_blueprint
        return reflected_result(compile_blueprint(blueprint_name, save, refresh_nodes, operation_id))


    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_actor_tick_settings(blueprint_name: str, tick_hz: float = 60.0,
            tick_interval: float = -1.0, can_ever_tick: bool = True,
            start_with_tick_enabled: bool = True, allow_tick_on_dedicated_server: bool = True,
            tick_even_when_paused: bool = False, dry_run: bool = True,
            compile: bool = True, save: bool = True) -> CubelessPythonOperationResult:
        "Raw Tick additionally implements actual refresh+compile for exact resident K2 stock graphs with native AActor default events, the stock UserConstructionScript entry and linked stock Engine native BlueprintFunctionLibrary public-static fixed-signature CallFunction nodes, and compiler parent-reset of only bCanEverTick when parent is true or no active/ambiguous own Tick exists. Current schema/native signatures and authored logical pin roles are preserved across the public RefreshAllNodes callback; IDs and physical pin order may be reconstructed. Original after remains requested values; actual_after_compile and compile_result expose the independent compiler outcome. Named10/Anim branches and defaults are unchanged. Active/ambiguous Tick, generated-parent refresh, custom/latent/member/interface or wildcard/dynamic-signature calls and non-admitted graph nodes and full original runtime scope remain incomplete or usage pending.\n\nOriginal raw Batch Tick additionally preserves omitted/unreadable fields from the current Native CDO, installed bool/number coercions, positive Hz then explicit interval precedence and float32 negative interval clamp0 through a private Raw2 carrier. Named10 defaults and readiness remain unchanged. Default raw dry/compile/save=True and refresh=False are preserved. Actual compile+refresh, parent/compiler CanEverTick eligibility changes, numeric/Bool Blueprint-name conversion and wider float/source/runtime scope remain incomplete or usage pending.\n\nSet original Actor CDO tick defaults through one finite native helper. Dry-run retains actual before/after observations separately from desired tick values and performs no explicit write/compile/save. Negative interval retains the original wrapper's omission policy: use positive tick_hz, otherwise preserve current interval; explicit nonnegative interval takes precedence. Native float arithmetic is authoritative. Non-dry writes mark the Blueprint modified; compile=True requests original validation/optional exact save, while compile=False deliberately never saves. A successful full compile may reset bCanEverTick; the helper retains the raw postcompile snapshot, explicitly restores only that public owned CDO field when needed, notifies the property and checks all tick defaults before optional save. Actual restoration/notification/dirty counters are returned; there is no second compile. This is CDO configuration, not Tick execution, scheduling or gameplay verification. One internal generated journal adds no public argument; actual mutation/compiler/save prefixes survive errors without retry.\n\n        Args:\n            blueprint_name: Original required target name/path; existing pure component path normalization and loaded current clean Actor Blueprint parent chains apply. Parent objects stay read-only and the existing immediate-parent CDO tick policy remains enforced.\n            tick_hz: Original 60.0 default, finite requested double; original native positive-frequency calculation and float clamp retained.\n            tick_interval: Original -1.0 sentinel; negative values act as omitted, zero requests every-frame interval.\n            can_ever_tick: Original True PrimaryActorTick flag default.\n            start_with_tick_enabled: Original True PrimaryActorTick flag default.\n            allow_tick_on_dedicated_server: Original True flag default; this does not prove dedicated-server ticking.\n            tick_even_when_paused: Original False flag default.\n            dry_run: Original True preview default. Native desired fields are not actual mutated or compiled defaults.\n            compile: Original True intent, effective only when dry_run=False; actual attempts/errors are retained.\n            save: Original True intent; Tick saves only after its requested successful compile, never when compile=False.\n        "
        from cubeless_mcp_ue58.operations import blueprint_defaults
        return reflected_result(blueprint_defaults.set_blueprint_actor_tick_settings(blueprint_name,
            tick_hz, tick_interval, can_ever_tick, start_with_tick_enabled,
            allow_tick_on_dedicated_server, tick_even_when_paused, dry_run, compile, save))

    @toolset_registry.tool_call
    @staticmethod
    def set_skeletal_mesh_component_anim_defaults(blueprint_name: str, component_name: str,
            skeletal_mesh: str = "", anim_class: str = "", physics_asset: str | None = None,
            simulate_physics: bool | None = None, collision_enabled: str | None = None,
            collision_response: str | None = None, enable_gravity: bool | None = None,
            visibility_based_anim_tick_option: str | None = None,
            compile: bool = True, save: bool = True) -> CubelessPythonOperationResult:
        """Set original SkeletalMeshComponent template defaults in original setter order through a finite native helper. Empty mesh/AnimClass strings omit those writes; PhysicsAsset None omits and explicit empty clears. Nullable simulation/collision/gravity/visibility preserve original absence. Native calls SetSimulatePhysics only, preserving PhysicsAsset body types and never SetAllBodiesSimulatePhysics. PostEditChange/modified/compiler/save work and final template readback are retained independently. After a successful full compile, the raw postcompile snapshot is retained and any compiler reset of the unselected owned CDO bCanEverTick is explicitly restored through the public field and property notification; all tick defaults are checked before save, with actual counters and no second compile. The original compile=False/save=True branch performs an explicit unvalidated save; it is not a compile pass. Loaded current clean Actor Blueprint parent chains are supported while parents stay read-only. The selected SkeletalMeshComponent must be the child's own exact native local template. Inherited components, custom component classes, external instances and loaded child-derived classes remain incomplete. AnimClass resolution tries the normalized original class path first and adds _C only when that class load returned null. Existing admitted generated AnimBP sources and exact compiled-in Engine native AnimInstance subclasses are supported; the native source requires its existing CDO and a read-only class/CDO proof, not a source uasset. CLASS_Transient describes runtime instances and does not alone reject the UClass reference. Source loading can run initial PostLoad/implicit compile work; no explicit source compile, CDO/animation-instance creation, runtime animation/physics/packaging proof or uncertain retry is added.

        Args:
            blueprint_name: Original required target name/path; pure shared component path parsing, no Python preload.
            component_name: Original required local SkeletalMeshComponent selector.
            skeletal_mesh: Original empty default means no mesh write. Native resolves/admit its finite source; root schema preparation must preserve the empty default.
            anim_class: Empty omits the write. Original normalized class path is loaded first, with _C fallback only after a null result; admits the existing generated path or exact compiled-in Engine native AnimInstance with existing CDO. Nonempty sets AnimationBlueprint mode then class; there is no anim_mode input.
            physics_asset: Original None omission; explicit empty clears, nonempty requests the native admitted reference.
            simulate_physics: Original None omission; optional exact bool changes the template's simulation default without overriding individual PhysicsAsset body types.
            collision_enabled: Original None omission; NoCollision, QueryOnly, PhysicsOnly or QueryAndPhysics parser is native-authoritative.
            collision_response: Original None omission; Ignore, Overlap or Block uniform response parser is native-authoritative.
            enable_gravity: Original None omission; optional exact bool.
            visibility_based_anim_tick_option: Original None omission; original native enum parser/actual policy readback retained.
            compile: Original True; native returns actual compile attempt/result independently of public success.
            save: Original True; exact save after successful requested compile, or original unvalidated save when compile=False.
        """
        from cubeless_mcp_ue58.operations import blueprint_defaults
        return reflected_result(blueprint_defaults.set_skeletal_mesh_component_anim_defaults(blueprint_name,
            component_name, skeletal_mesh, anim_class, physics_asset, simulate_physics,
            collision_enabled, collision_response, enable_gravity, visibility_based_anim_tick_option, compile, save))


    @toolset_registry.tool_call
    @staticmethod
    def replace_blueprint_component_class(blueprint_name: str, component_name: str, new_component_class: str,
            sphere_radius: float = -1.0, dry_run: bool = True, compile: bool = True,
            save: bool = True, refresh_nodes: bool = True) -> CubelessPythonOperationResult:
        "AH additionally admits an exact own inactive native StaticMeshComponent source in the existing native scene replacement profiles, with already-loaded saved-clean runtime StaticMesh/Material/MaterialInstanceConstant references. Ordered mesh/material slots including nulls, held source asset identities/files and original common defaults are preserved through retirement and compiler return. Sphere omitted radius uses original fallback100; Box/Capsule retain new native CDO dimensions. No source reference loader or renderer/physics cache invariant is added. This wider source scope is implemented / usage validation pending; broader cold/custom/dirty references and runtime propagation remain incomplete. Original command-only require_scene_compatibility is additionally preserved by one fixed private Raw9 helper through Batch, without changing Named8. False admits an own native Box/Sphere/Capsule nonroot leaf beneath an existing own non-default Scene root into a direct generated ActorComponent source. Existing inactive default-root cache, source hierarchy/CDOs, tags, SCS GUID/topology, actual partial/unknown, refresh/compile/save/retirement guards remain. Root, child-bearing and reverse cross-category cases remain incomplete; representative runtime/cold variants are usage pending.\n\nThe native scene branch replaces one admitted own local exact Box/Sphere/Capsule template by native Box/Sphere/Capsule, retaining its SCS node, variable name, GUID and parent. A finite own parentless native Box/Sphere/Capsule SCS scene-root profile additionally preserves ordered descendants, socket, variable GUID and relative defaults when the observed CDO RootComponent is null. It separately validates the DefaultSceneRoot cache and reacquires returned compiler class/CDO roles. Plain Scene/default-selected/inherited/native-bound/multiple roots and nonnull-CDO-root variants remain incomplete. Existing Box-to-Sphere compatible Get scope and compile-success-only requested save remain unchanged. The finite subtree profile also admits an existing SCS child subtree: ordered SCS child arrays, descendant node identities and captured non-selected template values are preserved. SCS children are distinct from physical Scene-template attachments: selected old/new templates must remain unregistered, owner/world-free and physically unattached with no attachment children. Existing common-copy transform/Mobility semantics remain; this does not certify every unobserved component property. Preview defaults True and separates desired summary from actual state. sphere_radius is consumed only by a Sphere target: negative uses old Sphere radius, old Box max absolute extent, or the original 100 fallback for another admitted source such as Capsule; explicit nonnegative radius follows the original native-float clamp. Box and Capsule ignore that finite argument. Box-to-Box copies unscaled extent; other Box targets retain new defaults, and Capsule-specific radius/half-height retain new class defaults because the original common-copy helper does not copy those fields. Original Sphere aliases remain supported and Box/Capsule use the same short/Component/UComponent/exact Engine path alias forms. Apply replaces even the same class and copies only original common defaults, not every class-specific property. A separate Box-to-Sphere profile on an exact direct-AActor Blueprint host admits exact own pure self-context selected component Get nodes and their admitted directly compatible whole-object SceneComponent/PrimitiveComponent input peers. The selected Get output is retargeted to the actual new component type before reconstruction; Native checks current semantic pin roles and reciprocal direct links, retaining graph node GUIDs and captured non-selected state. Compiler pin reconstruction and selected template retirement remain actual SDK transitions, not blanket physical pin/template identity preservation. A further exact Box-to-Sphere profile preserves one selected component OnComponentBeginOverlap bound-event node together with the existing admitted own pure Get profile. Native verifies the shared UPrimitiveComponent delegate and signature, selected component reference, EventReference, CustomFunctionName/flags, captured parameter pin types/defaults and reciprocal links through reconstruction and compiler return. Generated dynamic-binding objects are compared by their captured logical tuples after SDK return, not required to keep one physical object identity across compilation. This authors a Blueprint and does not trigger overlap callbacks or prove runtime collision/event delivery. Existing preview, refresh_nodes and compile options retain their original behavior: compile=False permits the existing structural stage but suppresses requested save, and forced save follows a requested successful compile only. Other selected bound-event/delegate signatures or type migration, selected setters, incompatible old-class-specific consumers and broader graph-reference repair remain incomplete. Target is already-loaded normal /Game direct-native Actor Blueprint; inherited/custom selected templates, selected roots outside the finite own shape-root profile, live/derived/archetype instances, graph references outside the admitted profile and occupied slots reject before mutation. Child subtrees outside the admitted native profile remain incomplete. Structural refresh and optional full compile are distinct. Save runs only after a requested successful compile; compile=False suppresses caller save=True. Raw compiler.saved=False is the SkipSave receipt, while top save/ordered native receipts record the separate exact forced save. Public callbacks, retirement and compiler/reinstance work may have effects; no rollback, uncertain retry, runtime/cold or broad replacement parity claim.\n\n        The generated non-scene branch additionally accepts an exact already-loaded /Game/Package.Package_C saved-clean Blueprint ActorComponent class, replacing one own flat root leaf non-scene generated BPC template on a saved-clean direct-native Actor host. Both source classes/CDOs and runtime dependencies must be current and saved clean. SCS variable name/GUID are retained; new class CDO defaults are used and only old ComponentTags are copied, not old custom variable overrides. sphere_radius must remain finite but is not consumed in this branch. Exact pure own self-context selected generated BPC getters may be repaired when every existing direct link accepts the new complete output type. Selected output type is explicitly retargeted before public ReconstructNode, then current pin identity and reciprocal direct links are verified; optional compiler pin replacement uses current semantic mapping. Selected setters, bound events, template literals, incompatible old-class-specific consumers, child/inherited arrangements and loaded derived/live/archetype consumers are refused before Modify. Structural refresh, optional compile and exact save have current/source/partial-prefix observations; compiler and retirement callbacks are not atomic rollback. Full Touch/Offset graph-reference repair and original pipeline/runtime behavior remain incomplete.\n\n        Args:\n            blueprint_name: Exact /Game package/object path or simple /Game/Blueprints name; native target must already be loaded.\n            component_name: Exact existing reachable own local variable name; native scene targets may retain an admitted SCS child subtree or be an admitted own parentless shape root. The generated non-scene branch remains a flat root leaf.\n            new_component_class: Sphere/Box/Capsule, each Component or UComponent name, or exact /Script/Engine.SphereComponent, /Script/Engine.BoxComponent or /Script/Engine.CapsuleComponent for the finite native scene branch. Exact /Game/Package.Package_C retains the generated non-scene branch above; other classes remain incomplete.\n            sphere_radius: Default -1 adapts original public omission for a Sphere target; finite explicit nonnegative value overrides that radius. Native Box/Capsule and generated non-scene targets require the current finite value but do not consume it. This wrapper retains exact boolean flags and finite numeric radius rather than reproducing every original public Python coercion.\n            dry_run: Default True; preview makes no target allocation/write/full compile/save.\n            compile: Default True; apply uses one public full compiler call after structural refresh.\n            save: Default True; one forced exact target save only after successful requested compilation.\n            refresh_nodes: Default True; forwarded to the compiler's existing node refresh option, with actual raw compiler outcome retained.\n         ADN additionally admits an exact native /Script/Engine.StaticMeshComponent target (StaticMeshComponent or UStaticMeshComponent names) from the existing own native Box/Sphere/Capsule source profiles. It reuses original Scene/Primitive common copying, SCS root/ordered children, retirement, partial failure, optional structural refresh, compile and save. The actual existing native target CDO and returned templates must keep null StaticMesh and empty OverrideMaterials; native descriptors and nullable slots are guarded through public reflection without a mesh/material assignment or load. Physical pin reconstruction and derived renderer/physics caches are not new invariants. Compiler-returned own StaticMesh target roles are reacquired without widening initial source admission or replacing the native default baseline. sphere_radius is ignored for this target. This scope is implemented / usage validation pending; populated target mesh/material defaults, cold/custom/dirty StaticMesh source references, other original component classes, unsupported graph references, live instances and runtime propagation remain incomplete. Actual partial/unknown state is retained without automatic replay or rollback."
        from cubeless_mcp_ue58.operations.component_replacement import replace_blueprint_component_class
        return reflected_result(replace_blueprint_component_class(blueprint_name, component_name,
            new_component_class, sphere_radius, dry_run, compile, save, refresh_nodes))



    @toolset_registry.tool_call
    @staticmethod
    def ensure_blueprint_rigid_constraint_chain(blueprint_name: str, body_components: list[unreal.JsonObjectWrapper],
            root_component_name: str = "SceneRoot", constraint_prefix: str = "PC", swing1_limit_degrees: float = 50.0,
            swing2_limit_degrees: float = 50.0, twist_limit_degrees: float = 10.0, linear_damping: float = 0.05,
            angular_damping: float = 0.15, projection_linear_alpha: float = 0.25, projection_angular_alpha: float = 0.08,
            projection_linear_tolerance: float = 5.0, projection_angular_tolerance: float = 15.0,
            required_root: str = "/Game/Study/ChaosSkeletalMesh", save: bool = True, dry_run: bool = True) -> CubelessPythonOperationResult:
        """Author one ordered anchored native StaticMesh/PhysicsConstraint SCS chain in an already loaded Actor Blueprint. Original body_components is an array of dictionaries, represented by stock Array(JsonObjectWrapper) on this reflected surface, preserving optional/unknown fields. The domain performs the original list normalization; there is no public JSON-string substitute. Every body needs component_name, static_mesh and relative_location=[X,Y,Z]; present rotation must be three numbers in Pitch/Yaw/Roll order, and present mass is converted to float. Optional simulation retains native index0=False/later=True validation. Default masses are .35 except the last2.0. Native source mesh loads/PostLoad and public SCS/setter/compiler callbacks may have effects even for a dry preview; no whole-call zero-effect claim. The original leaf native Scene-root route and replacement of a lone DefaultSceneRoot remain available. A separate finite append profile admits the requested exact native Scene root with a Native-admitted local SCS subtree of native Engine, own, unregistered and owner/world-free nodes/templates. Captured existing GUIDs, names, types, SCS hierarchy/child order and observed template fields are preserved while new bodies then constraints form the suffix. A separate finite append profile also admits a named own exact native SceneComponent pivot below the actual SCS root. Actual roots, ancestors, siblings and the pivot parent/socket/local defaults remain protected; new bodies and constraints append only to that selected pivot. Selected-pivot Mobility may follow the planned write; its physical Scene-template attachment children must be empty, which is separate from SCS child membership. Exact native Game/Engine StaticMeshes with simple collision remain required. This does not generalize preservation to unobserved component properties or arbitrary existing templates. Existing body/constraint names never overwrite. Apply performs structural refresh plus one full compile; requested save runs once only after verified readback. Partial allocation/setter/compile/save and source/load snapshots remain in the native result before errors, with no retry/rollback. Live instances, derived/inherited/custom roots/templates and broader chain migration/runtime Chaos stability remain incomplete.

        Args:
            blueprint_name: Exact already-loaded /Game Blueprint package/object path or simple /Game/Blueprints name.
            body_components: Required JSON array of 2..16 body objects; component_name/static_mesh/relative_location required in each. Optional relative_rotation/mass_kg/simulate_physics and unknown JSON fields retained.
            root_component_name: Default SceneRoot; an admitted own exact native Scene root or nonroot SceneComponent pivot with the bounded existing child prefix, original leaf root or fresh replacement of a lone DefaultSceneRoot.
            constraint_prefix: Default PC; adjacent constraint names are derived in original order.
            swing1_limit_degrees: Default 50; native finite range0<value<=179.
            swing2_limit_degrees: Default 50; native finite range0<value<=179.
            twist_limit_degrees: Default10; native finite range0<value<=179.
            linear_damping: Default.05; native nonnegative finite float.
            angular_damping: Default.15; native nonnegative finite float.
            projection_linear_alpha: Default.25; native range0..1.
            projection_angular_alpha: Default.08; native range0..1.
            projection_linear_tolerance: Default5; native positive finite float.
            projection_angular_tolerance: Default15; native positive finite float.
            required_root: Default /Game/Study/ChaosSkeletalMesh; normalized required package root retained.
            save: DefaultTrue; actual exact forced save only on non-dry verified apply, separate from raw compiler SkipSave receipt.
            dry_run: DefaultTrue; separates planned topology from repeated actual state, no target allocation/compile/save.
        """
        from cubeless_mcp_ue58.operations.rigid_constraint_chain import ensure_blueprint_rigid_constraint_chain
        from cubeless_mcp_ue58.operations import state
        try:
            rows = [json.loads(row.export_text()) for row in body_components]
        except Exception as error:
            result = state.fail(state.new_result(), state.OperationError("blueprint_rigid_chain_invalid_bodies", "Stock JsonObjectWrapper body conversion failed: " + str(error)))
            result["data"].update(operation="ensure_blueprint_rigid_constraint_chain", admission_rejected=True,
                native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False)
            return reflected_result(result)
        return reflected_result(ensure_blueprint_rigid_constraint_chain(blueprint_name, rows, root_component_name, constraint_prefix,
            swing1_limit_degrees, swing2_limit_degrees, twist_limit_degrees, linear_damping, angular_damping,
            projection_linear_alpha, projection_angular_alpha, projection_linear_tolerance, projection_angular_tolerance,
            required_root, save, dry_run))


    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_variable_metadata(blueprint_name: str, variable_name: str,
            metadata: unreal.JsonObjectWrapper | None = None, is_editable: bool | None = None,
            instance_editable: bool | None = None, expose_on_spawn: bool | None = None) -> CubelessPythonOperationResult:
        'This metadata API additionally admits direct Actor/UObject Blueprint parents with own primitive scalar, primitive Array up to 256 current values and exact native Vector/Rotator/Transform scalar members. Authored names/GUID/full types/CDO values and native value sources remain held and checked; only actual returned metadata SDK boundaries may reacquire generated/skeleton/property roles. Existing metadata/flag order, explicit fullcompile/save0 and actual partial/unknown receipts remain. Other Lifecycle API admission is unchanged; larger arrays, inheritance/reference/enum/complex types and new usage/persistence variants remain incomplete.\n\nSet existing local variable metadata and optional flags with original defaults. Stock optional JsonObjectWrapper retains actual JSON objects/nulls; dictionary values and aliases remain native-authoritative (instance_editable wins). Matching calls still invoke real public helper/MarkModified work. Native owns structural/cache callbacks; this API requests no explicit fullcompile/save. No v1 contract is fabricated, no second save/retry, and inherited/generated/SCS/timeline variants/full original workflow remain incomplete.\n\n        Args:\n            blueprint_name: Original required Blueprint name/path in finite Game scope.\n            variable_name: Original required existing variable name.\n            metadata: Original optional JSON object/defaultNone; empty object retains omission semantics, unsupported values are native-authoritative.\n            is_editable: Original optional bool alias/defaultNone; False is meaningful.\n            instance_editable: Original optional bool/defaultNone; explicit alias wins.\n            expose_on_spawn: Original optional bool/defaultNone.\n        '
        try:
            values = None if metadata is None else json.loads(metadata.export_text())
        except Exception as error:
            result = state.fail(state.new_result(),state.OperationError("variable_metadata_invalid","Stock optional object export failed: "+str(error)[:600]))
            result["data"].update(admission_rejected=True,native_dispatched=False,native_mutation_attempted=False,
                mutation_state_unknown=False,mutation_performed=False)
            return reflected_result(result)
        return reflected_result(variable_lifecycle.set_blueprint_variable_metadata(blueprint_name,variable_name,
            values,is_editable,instance_editable,expose_on_spawn))

    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_variable_order(blueprint_name: str, variable_order: list[str],
            strict: bool = False, compile: bool = False, save: bool = False) -> CubelessPythonOperationResult:
        'These original ordering APIs additionally admit direct Actor/Object Blueprint owners with own primitive scalar, primitive Array up to 256 current values and exact native Vector/Rotator/Transform scalar members. Authored names/GUID/full types/CDO values and native value sources remain held and checked. Only exact planned variable order is adopted by SetOrder; category root/FName/append behavior is preserved. SDK-return boundaries reacquire class/CDO/Inner roles and retain actual compile/save receipts before validation failure. General references/enums/UDS/inherited/large arrays and broad use remain incomplete or usage pending.\n\nReorder local Blueprint declarations with original false defaults. The required stock string array is copied to a Python list and internal JSON only. Native owns original trimming/FName/missing/strict logic and descriptor-preserving reorder; matching order performs no compile/save even if intent requests them. Changed order discloses structural versus full compile work and optional exact native save. Full raw known/partial/unknown/save prefixes precede errors; no public opID, Python second save or retry. This declares order, without runtime layout/property-use proof.\n\n        Args:\n            blueprint_name: Original required Blueprint name/path.\n            variable_order: Original required JSON array of string names; current local scope only.\n            strict: Original False default; missing names can cause known0 refusal.\n            compile: Original False default; actual fullcompile only on changed branch.\n            save: Original False default; native owns actual changed-branch forced target save.\n        '
        return reflected_result(variable_lifecycle.set_blueprint_variable_order(blueprint_name,list(variable_order),strict,compile,save))


    @toolset_registry.tool_call
    @staticmethod
    def remove_blueprint_variable(blueprint_name: str, variable_name: str) -> CubelessPythonOperationResult:
        """Remove one admitted own ordinary primitive member declaration using the original two inputs. The finite route also removes its exact stock self-context Get/Set nodes and their reciprocal incident links from existing directly owned standard K2 graphs; an unreferenced declaration retains the existing route. Native owns public RemoveMemberVariable plus the caller's structural notification, with actual raw before/after and planned selected-node/link retirement separated from captured surviving graph identities/topology, remaining fields, SCS/source/disk/other-dirty fences. Selected members are Blueprint members, not function-local variables. The current generated property may remain stale; descriptor/node removal proves no runtime or cold layout disappearance. The existing finite direct native Actor/local scalar profile additionally admits Net/RepNotify flags, replication conditions and existing own RepNotify-function associations. Original removal retires the selected descriptor and its stock Get/Set nodes while preserving surviving descriptors, interfaces and RepNotify functions; it does not delete the RepNotify function. FieldNotify metadata is admitted only with an empty own interface list where the installed SDK does not run the interface-owned metadata cascade. The new scope is implemented / usage validation pending. Inherited members, actual interface-owned FieldNotify cascade, external/unsupported references, component and complex variants remain incomplete. No explicit fullcompile/save, Python second save, retry, public opID or fabricated v1; implicit skeleton/compiler/cache callbacks remain separately qualified.

        Args:
            blueprint_name: Original required Blueprint name/path; finite Game scope.
            variable_name: Original required member variable name with the dependency-reference guard preserved.
        """
        from cubeless_mcp_ue58.operations.variable_removal import remove_blueprint_variable
        return reflected_result(remove_blueprint_variable(blueprint_name,variable_name))


    @toolset_registry.tool_call
    @staticmethod
    def set_blueprint_category_sorting(blueprint_name: str, category_order: list[str] = [],
            category_to_front: str = "", append_existing: bool = True, save: bool = True,
            compile: bool = False) -> CubelessPythonOperationResult:
        "These original ordering APIs additionally admit direct Actor/Object Blueprint owners with own primitive scalar, primitive Array up to 256 current values and exact native Vector/Rotator/Transform scalar members. Authored names/GUID/full types/CDO values and native value sources remain held and checked. Only exact planned variable order is adopted by SetOrder; category root/FName/append behavior is preserved. SDK-return boundaries reacquire class/CDO/Inner roles and retain actual compile/save receipts before validation failure. General references/enums/UDS/inherited/large arrays and broad use remain incomplete or usage pending.\n\nSet user-defined category display order with the original six inputs. Internal stock Array(FString) with default empty array exposes the original public array/null/defaultNone through the exact field converter. Null/empty retain original omission. Native owns trim, first root category before '|', FName uniqueness, append-existing and real dirty matching-save behavior. Changed=False is independent of attempted save/notification work; compile occurs only when order changes. Full raw known/partial/unknown observations, actual counters and ordered native saves precede errors in one journal. No Python second save, public opID, retry or fabricated v1. Editor-only display sorting differs from member declaration order; internal RPC category alias, broader generated/inherited/complex owners, runtime/cold/full original workflow remain incomplete.\n\n        Args:\n            blueprint_name: Original required Blueprint name/path; finite Game scope.\n            category_order: Original optional ordered string-array/defaultNone; empty retains omission.\n            category_to_front: Original single category shortcut/default empty string.\n            append_existing: Original True default; append prior/local/generated categories in original order.\n            save: Original True default; native can force save a dirty matching target even when changed=False.\n            compile: Original False default; explicit native compile only when category order changes.\n        "
        from cubeless_mcp_ue58.operations.category_sorting import set_blueprint_category_sorting
        return reflected_result(set_blueprint_category_sorting(blueprint_name,
            None if category_order is None else list(category_order),category_to_front,append_existing,save,compile))

    @toolset_registry.tool_call
    @staticmethod
    def set_pawn_properties(blueprint_name: str, auto_possess_player: str = "",
            use_controller_rotation_yaw: bool | None = None, use_controller_rotation_pitch: bool | None = None,
            use_controller_rotation_roll: bool | None = None, can_be_damaged: bool | None = None) -> CubelessPythonOperationResult:
        'The dedicated raw Batch contract additionally preserves original HasField(null included)/AsBool/order/partial and raw empty-request semantics via a private Native helper. The public Named6 omission/empty-request contract below is unchanged. No explicit compiler/save is added; existing typed Native/source/CDO guards remain. Actual raw use and wider variants are usage pending.\n\nSet original ordered Pawn defaults with nullable omissions and meaningful False values. Already-loaded direct native Pawn subclass parents also use the existing ordered five-field action, retaining the native parent-chain and existing resolved CDO roles without source writes, new defaults, compilation or saving. Extended reflected reads check their actual export receiver and current field inventory around SDK callbacks. Already-loaded saved-clean normal Game generated Pawn/Character parent chains additionally retain generated/skeleton/actual superclass, graph membership, existing CDO, source disk/dirty and runtime-dependency observations without source writes or new defaults. Native and generated-parent actual use remains pending; cold/dirty/live-derived/custom source and wider callback/layout profiles remain incomplete. A completely empty request returns original success/message/results without target path resolution, load or native dispatch. Otherwise native reports each property in order; an invalid auto-possession enum does not suppress valid later booleans, and any-success terminal semantics are retained. Matching accepted fields still perform original work/mark-modified, while typed changed independently reports scoped value change. Full raw partial/unknown CDO/source/callback prefixes are retained before errors. No explicit Modify/PostEdit/fullcompile/save or automatic retry is added; broader Pawn/Character classes/callback/runtime variants remain incomplete.\n\n        Args:\n            blueprint_name: Exact admitted Game Blueprint package/object or simple legacy name; ignored for no-properties shortcut.\n            auto_possess_player: Empty string omits this property; literal enum spelling goes to native ordered handling.\n            use_controller_rotation_yaw: None omits; False explicitly applies controller yaw default.\n            use_controller_rotation_pitch: None omits; False explicitly applies controller pitch default.\n            use_controller_rotation_roll: None omits; False explicitly applies controller roll default.\n            can_be_damaged: None omits; False explicitly applies native actor damage default.\n        '
        from cubeless_mcp_ue58.operations.pawn import set_pawn_properties
        return reflected_result(set_pawn_properties(blueprint_name,auto_possess_player,use_controller_rotation_yaw,
            use_controller_rotation_pitch,use_controller_rotation_roll,can_be_damaged))
