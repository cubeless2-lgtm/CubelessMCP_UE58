"""Official Registry schemas. Work logic belongs to operations, not these wrappers."""

import json
import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


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


@unreal.uclass()
class CubelessBlueprintTools(unreal.ToolsetDefinition):
    """Cubeless Blueprint lifecycle tools with explicit compile, save, partial-failure and retry results."""

    @toolset_registry.tool_call
    @staticmethod
    def duplicate_blueprint_to_temp(blueprint_name: str, target_path: str, required_root: str = "/Game/_MCP_Temp",
            dry_run: bool = True, overwrite_existing: bool = False, allow_dirty_source: bool = False,
            compile: bool = False, save: bool = True, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Clone a saved, clean ordinary Blueprint below an explicit root, defaulting to dry-run. Preserves the legacy defaults and named command. Source file/loaded state and clone class, K2 graph topology/native pin types, variable descriptors/current CDO are checked; GUIDs can legitimately change in the clone. Requires SaveOnCompile=Never and source control disabled because public duplication implicitly compiles and has no SkipSave option. Settings are never changed. Loading and engine duplication may have callback/selection effects; full asset parity, cold reload and packaged execution are not claimed by this call.

        Args:
            blueprint_name: /Game Blueprint package/object path, export-text or .uasset spelling, or simple name under /Game/Blueprints. Requires exact UBlueprint asset class, a native runtime parent or an inspected saved clean compiled Actor Blueprint parent chain, current generated class/CDO and clean saved source dependencies. Parent source hashes/loaded state are independently observed. Observation is bounded to 64 graphs, 2048 nodes per graph/4096 total and the existing 4096-variable/package-audit limits. Derived asset classes remain unsupported; component-template/comment content is not fully audited.
            target_path: New /Game package/object path strictly below required_root. Existing registry/disk/loaded packages or directories and source-equals-target fail. Complete object path at most 1023 UTF-16 units without controls. Nothing is deleted.
            required_root: Defaults /Game/_MCP_Temp; explicit empty preserves that legacy default. Target must be below this directory, not equal to it. Use a dedicated owned folder for verification.
            dry_run: Defaults True. True runs preflight/readback with no duplicate, compile or save; requested compile/save flags remain visible with attempted False. Read loading is not a guarantee of no PostLoad callbacks.
            overwrite_existing: Defaults False. True is explicitly unsupported in this slice, including when the target does not exist. No destructive delete/recreate fallback.
            allow_dirty_source: Defaults False. Actual dirty sources are rejected even when True; safe dirty-source equivalence remains pending. Clean sources may pass either value.
            compile: Defaults False. Requests one additional explicit diagnostic compile with node refresh; engine PostDuplicate implicit compilation is reported separately even when False.
            save: Defaults True. Requests one explicit save after clone readback and any requested compile. False keeps the new asset unsaved and verifies no target file appeared. Save failure fails the whole result and preserves known creation state.
            operation_id: Optional durable replay ID; omitted/null generates a returned ID. Identical ID/arguments replay historical results; uncertain clone/save outcomes never automatically retry.
        """
        from cubeless_mcp_ue58.operations.blueprint_duplicates import duplicate_blueprint_to_temp
        return reflected_result(duplicate_blueprint_to_temp(blueprint_name, target_path, required_root, dry_run,
            overwrite_existing, allow_dirty_source, compile, save, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_blueprint_variable(blueprint_name: str, variable_name: str, variable_type: str, is_exposed: bool = False,
            default_value_json: str | None = None, category: str | None = None, tooltip: str | None = None,
            friendly_name: str | None = None, type_object: str | None = None, is_array: bool = False,
            metadata_json: str = "{}", operation_id: str | None = None, variable_guid: str | None = None,
            is_editable: bool | None = None) -> CubelessPythonOperationResult:
        """Add one exactly named declared member variable with type-checked default and metadata. Conflicts fail rather than silently renaming. No full compile or save is requested; implicit skeleton compilation is reported separately. Unsupported types/defaults/dependencies fail before editing. Full legacy type coverage remains separately tracked.

        Args:
            blueprint_name: Exact runtime-safe /Game Blueprint package/object path, or simple name under /Game/Blueprints. No registry-wide ambiguous name search.
            variable_name: Exact new ASCII identifier, 1..100 characters, not None. Existing names and identity conflicts fail.
            variable_type: bool/boolean, int/integer, int64/integer64, float/single, double/real, string, name, text, Vector, Rotator, Transform, or audited struct/scriptstruct, enum/byteenum, object/class with type_object. Delegates, interfaces, maps, sets and soft-reference descriptors remain unsupported. Native typed defaults must survive an exact engine text export/import roundtrip; values that cannot are rejected before editing.
            is_exposed: Defaults False. True enables instance editing; combined with is_editable using the legacy logical OR.
            default_value_json: Explicit JSON string adapter for legacy Any. Omitted/null, or the JSON text null, means no supplied add default, matching legacy default_value=None. Other values retain their JSON types. Text at most 65536 UTF-16 units, depth 8 and 1024 values; duplicate keys, nonfinite values, magnitudes above 2^53-1, NUL and unpaired surrogates fail.
            category: Optional category, at most 1023 UTF-16 units. Omitted/null/empty means no override.
            tooltip: Optional tooltip, at most 4096 UTF-16 units; line breaks permitted. Omitted/null/empty means no override.
            friendly_name: Optional display name, at most 1023 UTF-16 units. Omitted/null/empty means no override.
            type_object: Optional exact runtime-safe type path when required by the descriptor. Object/class without a type defaults to Object. Referenced assets require a complete saved dependency audit; unaudited plugin/native-module references fail.
            is_array: Legacy array request, default False. Explicit array defaults have at most 256 elements with recursively typed validation; unsupported combinations fail. Arbitrary raw Unreal import text is not accepted for aggregates.
            metadata_json: JSON object, default {}, at most 64 string/bool/finite-number values. Keys at most 1023 UTF-16 units and case-insensitively unique; string values at most 4096 units. Unsupported metadata effects fail explicitly.
            operation_id: Optional durable replay ID. The same ID and normalized arguments return the historical result; uncertain operations never automatically retry.
            variable_guid: Optional legacy internal-command GUID. Must be nonzero, valid and unique; omitted/null/empty lets the engine create one.
            is_editable: Optional legacy internal-command alias. Effective instance editing is is_exposed OR is_editable; False does not override True is_exposed.
        """
        from cubeless_mcp_ue58.operations.variables import add_blueprint_variable
        return reflected_result(add_blueprint_variable(blueprint_name, variable_name, variable_type, is_exposed,
            default_value_json, category, tooltip, friendly_name, type_object, is_array, metadata_json, operation_id, variable_guid, is_editable))

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
        """Set the type-checked default of one locally declared member variable. Preserves legacy public compile/save defaults True. Native snapshots, compiler/save outcomes and partial failures are retained. Full compilation may consume the stored default into the CDO, so an empty stored default afterwards is not proof of failure.

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
        """Create an unsaved Blueprint without overwriting. Requires SaveOnCompile=Never because the engine factory implicitly compiles without a public SkipSave option. Settings are never changed. Parent must resolve; known MCP/editor-tool parent chains are rejected.

        Args:
            name: New asset identifier, not a path. Complete new object path must be shorter than 1024 UTF-16 units.
            parent_class: Native class name (Actor/Pawn etc.) or native class path; empty means Actor. Also accepts an exact /Game/Package.Package_C Actor Blueprint class only when it and every generated parent are saved, clean, compiled and pass the runtime dependency/class guard. Parent source SHA/dirty/status are observed before/after factory work. No fallback class or automatic parent compile/save.
            package_path: Directory under /Game; defaults to /Game/Blueprints (explicit empty also uses that legacy default). No object suffix.
            operation_id: Unique request ID for durable replay protection. Omitted/null generates an ID returned in the result. Reuse only with identical arguments.

        Returns:
            Structured result. data_json includes name, path, asset_path and actual parent; creation remains unsaved.
        """
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
