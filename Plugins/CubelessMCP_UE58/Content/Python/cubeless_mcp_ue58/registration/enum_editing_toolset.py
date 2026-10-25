"""Additive editing tools; original create-only enum Toolset ABI is unchanged."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from ..operations import user_defined_enum_editing as enums


@unreal.uclass()
class CubelessEnumEditingTools(unreal.ToolsetDefinition):
    """Inspect/edit an existing Engine UserDefinedEnum asset in place."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_user_defined_enum(asset_path: str) -> CubelessPythonOperationResult:
        """Read an existing /Game UserDefinedEnum: stable internal entry names, numeric values, display names, tooltips and dirty state. The auto MAX entry is not editable. Native/Engine enums and other asset classes are refused. Loading may have SDK effects; this is not a proof of global purity."""
        return reflected_result(enums.inspect_user_defined_enum(asset_path))

    @toolset_registry.tool_call
    @staticmethod
    def edit_user_defined_enum(asset_path: str, edits_json: str, dry_run: bool,
            save: bool, allow_remove: bool, operation_id: str) -> CubelessPythonOperationResult:
        """Edit the SAME existing /Game UserDefinedEnum, without replacing the asset. edits_json is an ordered JSON array of at most64 objects: add {op,display_name,tooltip?}; set_display_name {op,name,display_name}; set_tooltip {op,name,tooltip}; move {op,name,new_index}; remove {op,name}. name must be a real full internal name that existed before this batch and remains present; preview placeholders are never editable. To edit a newly added item, complete the add, inspect its actual name, then issue a new operation. name is not the display label; new_index is zero-based and excludes auto MAX. All boolean flags are REQUIRED: use dry_run=true, save=false, allow_remove=false for a preview; use dry_run=false to apply. Removal also requires allow_remove=true. Existing names survive label changes/moves; move/remove numeric values are remapped by public UE EnumEditorUtils. Plan validation precedes target edits. UE broadcasts can change dependent Blueprint/struct values and mark packages dirty; only the enum is optionally saved. Bitflags ordinary integer masks are not remapped by entry names; preserving their bit values is outside this API support. Deleted values can become MAX/invalid; unloaded references are not guaranteed resolved. Dependent Blueprint compilation/saving is NOT guaranteed; dependent user-defined structs may compile implicitly. Inspect before/after/steps/dirty-package receipts on failures. No atomic rollback or automatic retry; Supply a fresh operation_id for new edits; the same operation_id retrieves an already journaled result and never replays an uncertain mutation. Unsupported native enums, auto MAX edits, malformed ops or duplicate labels are refused."""
        return reflected_result(enums.edit_user_defined_enum(asset_path, edits_json, dry_run, save, allow_remove, operation_id))
