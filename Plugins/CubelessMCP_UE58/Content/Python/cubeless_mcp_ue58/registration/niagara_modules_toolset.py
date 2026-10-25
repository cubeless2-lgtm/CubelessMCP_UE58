import json
"""Exact legacy stock module public tool on the official Registry; no transport logic."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from .async_interop_toolset import CubelessPythonAsyncOperationResult


@unreal.uclass()
class CubelessNiagaraModuleTools(unreal.ToolsetDefinition):
    """Finite exported stock-stack insertion, with explicit original-selector gaps."""

    @toolset_registry.tool_call
    @staticmethod
    def add_niagara_stock_module_to_stack(target_system_path: str, module_script_path: str,
            target_usage: str = "ParticleSpawnScriptInterpolated", target_usage_id: str | None = None,
            target_emitter_index: int | None = None, target_emitter_name: str | None = None,
            target_index: int | None = None, suggested_name: str | None = None,
            skip_if_duplicate: bool = True, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Insert one exact Niagara Module script into one exact existing native output stack using the public exported Engine API. Preserves original absolute insertion index, suggested name, usage/GUID and optional emitter selectors. Skip-if-duplicate examines the same script in the selected output stack and returns without compile/save on an exact skip. Add acquires the primary target first, then the module: loaded-fast0 or one conditional typed LoadObject each. Returned assets are held and checked against the captured Editor/world and fresh identity before use. Named, Direct and Batch Add share this Native path; Inspect and internal observers remain find-only. Source baselines are captured after loading, so dependency/PostLoad/cache/global loader effects are not proved unchanged. No automatic reload/retry, redirect substitution, or creation of a system/emitter/output. Supported native usages/versions/selectors are bounded; unsupported variants reject before mutation and remain incomplete. Does not implement scratch/HLSL, an export stage, DI binding or particle transport. Native partial failures retain actual stack/package/compiler observations; no rollback or automatic retry. Requested compile is nonblocking. A requested save is deferred with a known partial failure while compilation is pending; inspect native readiness and issue one exact official save instead of resubmitting insertion.

        Args:
            target_system_path: Exact runtime /Game Niagara System package or object path; Add may conditionally load this primary target once before the module, under the existing write/source admission.
            module_script_path: Exact external native Niagara Module asset path from /Niagara, /Engine or /Game; loaded-fast0 or one conditional primary load. Editor/Cubeless/MCP dependencies, inline scratch scripts and incompatible usages remain outside this admission.
            target_usage: Original Niagara usage name, default ParticleSpawnScriptInterpolated. Native finite admission is authoritative; no implicit replacement stack.
            target_usage_id: Original output usage GUID selector. Omitted/null becomes the legacy empty selector; invalid or unsupported selectors fail before insertion.
            target_emitter_index: Optional exact int32, omitted/null or -1 means the original unspecified selector. Name and index jointly select the emitter; unsupported emitter variants are not silently ignored.
            target_emitter_name: Optional display name selector. Omitted/null becomes the original empty name; no non-string coercion.
            target_index: Absolute int32 insertion index; omitted/null or -1 appends. Native clamps other negative indices to the first position and indices beyond the module count to the end, preserving the original stack utility behavior and reporting the actual index. Fractions, booleans and out-of-int32 values fail before native dispatch.
            suggested_name: Optional suggested module node name. Omitted/null becomes the legacy empty name; actual native name is returned.
            skip_if_duplicate: Default true requests original exact-script duplicate skip in the selected output stack, with compile/save not dispatched on a skip.
            allow_source_edit: Default false permits legacy generated Niagara and current CubelessMCPTests targets. True permits other explicit /Game targets without weakening native reference/source guards.
            save: Request one exact system save after insertion. Pending/needed compilation yields a precise known deferred failure with no save attempt; no save-all or implicit mutation retry.
            request_compile: Request native compile at most once after actual insertion. Completion is separately observed; this call does not wait/flush on the game thread.
            operation_id: Optional durable ID. Identical completed calls replay their journal; incomplete or uncertain calls never dispatch automatically again.
        """
        from cubeless_mcp_ue58.operations.niagara_modules import add_niagara_stock_module_to_stack
        return reflected_result(add_niagara_stock_module_to_stack(target_system_path,
            module_script_path, target_usage, target_usage_id, target_emitter_index,
            target_emitter_name, target_index, suggested_name, skip_if_duplicate,
            allow_source_edit, save, request_compile, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_niagara_module_inputs_batch(system_path: str, edits: list[unreal.JsonObjectWrapper],
            operation: str = "set_existing", overwrite_existing: bool = False,
            continue_on_error: bool = False, allow_source_edit: bool = False,
            save: bool = True, request_compile: bool = True, wait_for_completion: bool = False,
            timeout_seconds: float = 10.0, poll_interval_seconds: float = 0.1) -> CubelessPythonAsyncOperationResult:
        'Apply ordered RapidIteration module input edits to an already loaded native Niagara System. Each edit retains its emitter/module selectors, input_name, value and optional operation/overwrite flag. This writes the owning Script parameter store and its exact enabled-emitter Engine dependency (Emitter to System, GPU compute, or CPU interpolated spawn), following the installed editor input binder. Each dependent setter attempt/return and partial prefix is reported; unrelated RI keys remain protected. Existing registered local Scratch sources are observed without cloning or rewriting them; graph literal/linked/DataInterface overrides and User parameters use their distinct tools. set_existing/set requires an existing value; create_override/create/upsert creates a missing value or needs overwrite_existing=True to overwrite. Accepted matching values still perform the original writes. Ordinary per-edit errors follow continue_on_error; returned partial rows, values, actual counts and compile/save status are retained before any failure. Unknown state or a scoped preservation failure stops further work. No automatic retry or rollback.\n\n        The first finite scope has ordinary owned CPU/GPU emitters, scalar/enum/float32 vector/color inputs, a unique selected module/input and no live world system components. Target/script/graph readiness is authoritative in native output. Named uses one finite Native Job for the original compile→independent wait→optional forced-save tail after applied rows, including partial row failures. Timeout remains a Native diagnostic and does not by itself mean compile success or suppress save. stateless/ambiguous/position or arbitrary object/struct input variants remain incomplete. Requested compilation and nullable completion status remain distinct; no row replay is used to finish a save. No extra Python compiler or saver is invoked. This is current-session authoring, not runtime simulation or cold persistence proof.\n\n        Args:\n            system_path: Original required Niagara System package or object path; load deliberately through a public asset tool first.\n            edits: Required JSON array of edit objects via stock JsonObjectWrapper. Each edit needs input_name/value and selectors for one module; unknown fields stay in the raw request.\n            operation: Original default set_existing. Recognizes set, create_override, create and upsert aliases without trimming or rewriting.\n            overwrite_existing: Original False default; per-edit value overrides this flag. Public upsert does not silently force overwriteTrue.\n            continue_on_error: Original False default. True continues ordinary known row failures, while unknown or preservation failures stop.\n            allow_source_edit: Original False default restricts to /Game/_MCP_Temp/NiagaraGenerated; True admits other explicit /Game targets through native guards.\n            save: Original True default; request one exact forced save after applied rows and admitted readiness/preservation. Actual save return remains authoritative.\n            request_compile: Original True default; request once after applied rows. Dispatch is not compiledTrue; actual script status is observed separately.\n            wait_for_completion: Original False default. Named True observes finite GT continuation on the same Native Job; Direct/raw/ordinary Batch genuine wait remains unsupported.\n            timeout_seconds: Original10 default and native0..60 clamp retained; the Native Job owns its monotonic deadline.\n            poll_interval_seconds: Original.1 default and native.02..1 clamp retained; Native owns the finite GT observations.\n         ADM additionally admits already-loaded owned ordinary GPUComputeSim emitters in the same RapidIteration batch and read-only batch inspection. GetScripts(false,false) includes the current GPUComputeScript in existing store/source observations; authored SimTarget and native source/script routing are separately held and guarded before SDK calls. Private UPROPERTY storage is read through public reflected slot access only. Shader/executable/render-thread caches are not frozen. Existing ordered row errors, continue_on_error, compile/wait/optional forced-save and unknown-stop receipts remain unchanged; GPU shader readiness uses existing HasOutstandingCompilationRequests(true), without a new wait or flush. This is implemented / usage validation pending; stateless/external emitters, ambiguous selectors, broader input types and GPU execution correctness remain incomplete. Registration or stored authoring does not prove simulation, shader success or cold persistence.'
        from cubeless_mcp_ue58.operations import niagara_module_batch, state
        try:
            rows = [json.loads(row.export_text()) for row in edits]
        except Exception as error:
            result = state.fail(state.new_result(), state.OperationError("niagara_module_batch_edits_invalid",
                "Stock JsonObjectWrapper edit conversion failed: " + str(error)))
            result["data"].update(operation="set_niagara_module_inputs_batch", admission_rejected=True,
                native_dispatched=False, native_mutation_attempted=False, mutation_state_unknown=False, mutation_performed=False)
            return niagara_module_batch.completed_result(result)
        return niagara_module_batch._named_mcp(dict(system_path=system_path,edits=rows,operation=operation,
            overwrite_existing=overwrite_existing,continue_on_error=continue_on_error,allow_source_edit=allow_source_edit,
            save=save,request_compile=request_compile,wait_for_completion=wait_for_completion,
            timeout_seconds=timeout_seconds,poll_interval_seconds=poll_interval_seconds))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_niagara_module_rapid_iteration(system_path: str) -> CubelessPythonOperationResult:
        'Read current RapidIteration stores, typed supported values, owned graphs/module sources and compile state of one already-loaded finite CPU/GPU Niagara System. Keeps all owned scripts, including inline emitter stores; standalone compile eligibility excludes only EmitterSpawn/EmitterUpdate. Never loads, writes parameters, requests/polls/waits for compilation, or saves. A pending/error compile status may be observed successfully; compiled remains nullable. Complete read-scope drift or incomplete prefixes return a truthful failure. Use this read to validate the retained writer prefix before a separately authorized exact save; never replay edits to finish compilation. No runtime/cook or exhaustive cache/world effect claim. ADM additionally admits already-loaded owned ordinary GPUComputeSim emitters in the same RapidIteration batch and read-only batch inspection. GetScripts(false,false) includes the current GPUComputeScript in existing store/source observations; authored SimTarget and native source/script routing are separately held and guarded before SDK calls. Private UPROPERTY storage is read through public reflected slot access only. Shader/executable/render-thread caches are not frozen. Existing ordered row errors, continue_on_error, compile/wait/optional forced-save and unknown-stop receipts remain unchanged; GPU shader readiness uses existing HasOutstandingCompilationRequests(true), without a new wait or flush. This is implemented / usage validation pending; stateless/external emitters, ambiguous selectors, broader input types and GPU execution correctness remain incomplete. Registration or stored authoring does not prove simulation, shader success or cold persistence.'
        from cubeless_mcp_ue58.operations.niagara_rapid_iteration import inspect_niagara_module_rapid_iteration
        return reflected_result(inspect_niagara_module_rapid_iteration(system_path))
