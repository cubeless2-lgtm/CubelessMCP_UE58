"""Original StateTree public5 contracts through the official Registry."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import state_tree_read
from cubeless_mcp_ue58.operations import state_tree_create
from cubeless_mcp_ue58.operations import state_tree_mutators


def _add_guard_admission():
    """Registration-only fail-closed admission, including a filtered original handler."""
    report = {}
    try:
        library = getattr(unreal, "CubelessStateTreeMutationLibrary", None)
        helper = getattr(library, "verify_state_tree_strict_dry_run_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact StateTree raw-input admission helper unavailable")
        report = json.loads(helper(CubelessStateTreeTools))
        if (type(report) is dict
                and report.get("operation") == "verify_state_tree_strict_dry_run_toolset"
                and report.get("delegated_call_admitted") is True
                and report.get("delegated_admission_only") is True
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None
        message = "Canonical guarded add delegation required; no Native asset operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    from cubeless_mcp_ue58.operations.state import OperationError
    result = state_tree_mutators._rejected("add_state_tree_state",
        OperationError("state_tree_raw_guard_admission_missing", message))
    result["stage"] = "strict_dry_run_registration_admission"
    result["data"].update(registration_guard=report, change_state_known=True, changed=False,
        native_dispatched=False, python_dispatched=True, journal_created=False)
    return result


@unreal.uclass()
class CubelessStateTreeTools(unreal.ToolsetDefinition):
    """StateTree asset inventory and bounded hierarchy observations. LoadAsset/PostLoad and readiness can invoke the installed SDK compiler implicitly. Explicit authoring/compiler/save and runtime execution remain separate; observations do not prove total SDK purity, runtime behavior, persistence or completed pipelines."""

    @toolset_registry.tool_call
    @staticmethod
    def list_state_trees(directory: str = "/Game", recursive: bool = True,
            max_results: int = 200, offset: int = 0) -> CubelessPythonOperationResult:
        """List StateTree and derived assets under /Game or /Engine using the original AssetRegistry class/path filter. Native normalizes the directory, admits int32 bounds, clamps max_results to [1,1000] and offset to >=0, sorts object paths, and returns assets/count/total/offset/has_more with next_offset only when more remain. The returned registry array is observed without asset loads; a false GetAssets return and empty array retain the legacy projection and query scope rather than proving inventory completeness. Native owns dependency and path guards; no wrapper load/compiler/save or retry."""
        return reflected_result(state_tree_read.list_state_trees(directory, recursive, max_results, offset))

    @toolset_registry.tool_call
    @staticmethod
    def inspect_state_tree(asset_path: str, max_states: int = 500) -> CubelessPythonOperationResult:
        """Inspect the original StateTree schema, readiness/compiled hash, global task/evaluator counts and bounded root/child hierarchy including names/paths/IDs/types/selection/enabled/task/condition/transition counts. Native admits int32 and clamps max_states to [1,2000], guards cycles/depth/unreadable states and retains truncation or failed partial prefixes. Hierarchy is observed before IsReadyToRun; loading/PostLoad/readiness may compile implicitly. Positive hash/dirty changes are reported; equal observations do not prove total changes known. Named and Batch parent journals retain unknown outcomes, explicit compile/save flags and original errors. No automatic retry, rollback or runtime execution."""
        return reflected_result(state_tree_read.inspect_state_tree(asset_path, max_states))


    @toolset_registry.tool_call
    @staticmethod
    def create_state_tree(name: str, package_path: str = "/Game/AI/StateTrees",
            schema_type: str = "component") -> CubelessPythonOperationResult:
        """Create and save the original idempotent StateTree using name, package_path and component/ai schema_type. Native owns original name/package/dependency/schema/PIE guards, typed/schema/readiness checks when reusing an existing asset, or factory creation followed by explicit compile, live audit, forced save, persisted audit and final schema/readiness proof. Compiler/save errors and conditional cleanup retain actual changed/partial/unknown prefixes; cleanup is not atomic rollback. UE58 loading, factory and readiness may compile implicitly, separately from requested explicit calls. Named and Batch parent journals retain raw/data/compiler/save/audit outcomes before terminal errors; no wrapper second compile/save, new overwrite flag, fallback or retry."""
        return reflected_result(state_tree_create.create_state_tree(name, package_path, schema_type))


    @toolset_registry.tool_call
    @staticmethod
    def add_state_tree_state(asset_path: str, state_name: str, parent_path: str = "Root",
            state_type: str = "state", selection_behavior: str = "children_in_order",
            dry_run: bool = True) -> CubelessPythonOperationResult:
        """Add or inspect the original bounded StateTree child operation; dry_run defaults true and must be an exact JSON boolean. Native resolves an unambiguous bounded parent path, validates names/type/selection, verifies matching existing children or returns a dry-run prediction without explicit authoring/compiler/save. Real mutation validates readiness, clean persisted target and dependency independence, modifies only admitted tree/editor/parent state, adds the child, compiles/audits/saves and verifies current settings. Structural failure recovery conditionally removes only the new child, recompiles/forces save and reports actual recovery outcome; it is not atomic rollback. Loading/readiness can compile implicitly. Full actual mutation/partial/recovery/compiler/save/audit prefix is journaled before errors; no extra Python asset action or retry."""
        admission_rejection = _add_guard_admission()
        if admission_rejection is not None:
            return reflected_result(admission_rejection)
        return reflected_result(state_tree_mutators.add_state_tree_state(asset_path, state_name, parent_path,
            state_type, selection_behavior, dry_run))

    @toolset_registry.tool_call
    @staticmethod
    def compile_state_tree(asset_path: str, save: bool = True) -> CubelessPythonOperationResult:
        """Validate and compile the original StateTree in one Native operation; save defaults true. Native rejects PIE/SIE, loads the typed /Game asset, audits actual dependencies, executes the public compiler with actual messages, and when compiled/audited and save requested marks dirty and forces save before persisted audit. Schema/readiness/compiled hash and current partial/compiler/save outcomes remain in the returned prefix on failure. SDK LoadAsset/PostLoad/readiness compilation is separate from the explicit compiler count. Named or Batch parent journals retain actual flags/raw before errors; no Python second compiler/save, unconditional rollback, replay or runtime evaluation."""
        return reflected_result(state_tree_mutators.compile_state_tree(asset_path, save))
