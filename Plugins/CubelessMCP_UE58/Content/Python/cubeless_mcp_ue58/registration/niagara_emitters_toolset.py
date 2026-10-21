"""Original finite emitter attachment tool on the official Registry."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


@unreal.uclass()
class CubelessNiagaraEmitterTools(unreal.ToolsetDefinition):
    """Copy a native emitter, detach parents and localize private function references."""

    @toolset_registry.tool_call
    @staticmethod
    def duplicate_or_attach_emitter_from_source(target_system_path: str,
            source_emitter_path: str | None = None, source_system_path: str | None = None,
            source_emitter_index: int | None = None, source_emitter_name: str | None = None,
            new_emitter_name: str | None = None, enabled: bool | None = None,
            copy_source_user_parameters: bool = False, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Attach one copied standard Niagara emitter through the public Engine API. Native resolves the primary target first, then the selected emitter asset or source system, using one original typed asset load only when that root is absent; already-loaded roots use zero loads and incomplete loaded roots are never reloaded. Returned roots are held immediately and checked against the Editor/world and prior target handles/graphs/dirty projection before native owner/version/source admission. Actual loader return/count prefixes remain distinct from complete PostLoad/compiler/CDO/global-effect proof. Inspector and internal postchecks stay find-only. This primary resolution expansion preserves the existing writer order; it does not claim the original target-Modify-before-source-load failure prefix. It removes both parent references and recursively localize external private FunctionScripts into the target system. Preserve the original source asset/system selector precedence, optional enabled presence, requested name and actual returned unique name/GUID. Exact native bounded graph/version/source observations are authoritative. Existing system emitter nodes may be rebuilt by the Engine. This operation does not implement a whole-system template copy or the simulation-stage/DI/HLSL particle exporter. Source-system User copying supports bounded builtin scalar/vector/color, full-precision Position, existing nullable UObject/Material references and exact native ArrayFloat/ArrayInt32 CPU payload/config via public CopyTo. The original copy order retains added default slots and false/skip partial values, copied/skipped/count reports, then refresh/compile/requested-save. Source emitter assets have no system User copy. Same-name different-type collisions, other DI/enum/custom payloads and copy combined with external private-script localization remain valid wider original scope not yet implemented. No rollback or automatic retry. Compile is nonblocking and completion remains separately observed. A requested save while compilation is outstanding/needed returns a known deferred failure with the attachment retained; inspect readiness, then save the exact asset rather than reattaching.

        Args:
            target_system_path: Exact native /Game Niagara System package or object path, resolved from an already-loaded root or one primary asset load. Default writes permit legacy generated and CubelessMCPTests roots, with no live world NiagaraComponent references.
            source_emitter_path: Optional native emitter asset path, resolved from an already-loaded root or one primary asset load. Null/omission represents the original empty string. A nonempty emitter asset takes precedence over source_system_path.
            source_system_path: Optional distinct native system path when no emitter asset is selected, resolved from an already-loaded root or one primary asset load; existing source compilation/version/private-graph guards still apply. Native selects the first matching standard source handle by joint index/name and its exact version.
            source_emitter_index: Null/omission or -1 leaves the index unspecified; otherwise a nonnegative exact int32. Booleans, fractional or out-of-range indices reject before dispatch.
            source_emitter_name: Optional case-insensitive source-system display name selector. Null/omission represents the original empty string, with no falsey coercion.
            new_emitter_name: Optional requested new name. Null/omission uses the selected source name. Engine sanitization/uniqueness determines the returned actual name.
            enabled: Null/omission skips the override and uses the new native handle's default true, even if the source handle is disabled. An explicit boolean overrides the new handle; it does not alter the source.
            copy_source_user_parameters: Default false preserves the existing no-explicit-User-write path. True copies supported source-system User defaults into the target after attachment and before refresh/compile/save; it preserves source and unrelated target values, reuses existing same-type declarations/DIs, and reports actual added/copied/updated/skipped partial prefixes. Unknown callbacks stop subsequent work without retry; unsupported wider types are explicitly pending.
            allow_source_edit: Default false permits isolated generated/test targets. True opts into another explicit /Game target without relaxing source/identity/private-closure guards.
            save: Request at most one exact target save. Pending/needed compile defers saving with observed retained work; no wait, save-all or mutation replay.
            request_compile: Request one native compile after verified attachment/detachment/localization. Actual completion and live particle execution are outside the mutation result.
            operation_id: Optional durable ID. Completed identical calls replay their journal; incomplete or uncertain calls never dispatch automatically again.
        """
        from cubeless_mcp_ue58.operations.niagara_emitters import duplicate_or_attach_emitter_from_source
        return reflected_result(duplicate_or_attach_emitter_from_source(target_system_path,
            source_emitter_path, source_system_path, source_emitter_index, source_emitter_name,
            new_emitter_name, enabled, copy_source_user_parameters, allow_source_edit,
            save, request_compile, operation_id))
