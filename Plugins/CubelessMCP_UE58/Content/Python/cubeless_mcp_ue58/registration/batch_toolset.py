"""Official registration for the explicitly bounded domain batch."""

import unreal
import toolset_registry

from .async_interop_toolset import CubelessPythonAsyncOperationResult


@unreal.uclass()
class CubelessBatchTools(unreal.ToolsetDefinition):
    'Guarded serial domain batches with per-command outcomes; no atomic rollback. Detailed contracts: CubelessDiscoveryTools.describe_toolset with this exact registered toolset name, verbosity="terse" to select a tool, then its exact method query, limit=1 and verbosity="full". Full detail may be large; defaults, source references and safety checks are unchanged.'

    @toolset_registry.tool_call
    @staticmethod
    def execute_batch(commands_json: str, stop_on_error: bool = True, operation_id: str | None = None) -> CubelessPythonAsyncOperationResult:
        'Execute 1..100 supported commands serially through the existing guarded domain handlers. commands_json is a JSON array of {"type":"command_name","params":{...}} entries, at most 1 MiB; nested batches, duplicate JSON keys and non-finite values are rejected. stop_on_error defaults true; unknown or unsafe partial outcomes always stop even when false. Child compile/save requests remain explicit and are not silently coalesced. Returns per-command results and actual mutation/compile/save/stop receipts via the existing async result. No atomic rollback, arbitrary command dispatch or automatic retry of uncertain work. Details: CubelessDiscoveryTools.describe_toolset(toolset="cubeless_mcp_ue58.registration.batch_toolset.CubelessBatchTools", query="execute_batch", limit=1, verbosity="full").'
        from cubeless_mcp_ue58.operations.batch import execute_batch_mcp
        return execute_batch_mcp(commands_json, stop_on_error, operation_id)
