"""Correct comment-box authoring for Materials and Material Functions."""
import unreal
import toolset_registry
from cubeless_mcp_ue58.operations import material_comments


@unreal.uclass()
class CubelessMaterialCommentTools(unreal.ToolsetDefinition):
    """Explicit /Game Material/MaterialFunction comment collections; no selection or runtime helper."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_material_comments(asset_path: str) -> str:
        """Read comment text, GUID, bounds and membership in Expressions/EditorComments. Distinguishes misplaced ordinary nodes from proper comment boxes. Exact /Game Material/MaterialFunction asset; no writes, shader compile or save."""
        return material_comments.run(asset_path, "inspect")

    @toolset_registry.tool_call
    @staticmethod
    def add_material_comment(asset_path: str, text: str, x: int = 0, y: int = 0,
                             width: int = 400, height: int = 100,
                             execute: bool = False, save: bool = False) -> str:
        """Create one real EditorComments box in a /Game Material or MaterialFunction, preserving existing expressions/connections/comments. Default execute=false is dry-run; text<=16384 UTF-16 units, int32 coordinates, dimensions1..1000000. Close the target's Material Editor before writing to avoid preview overwrite; reopen afterward. No shader compile or save by default. save=true explicitly saves this whole asset package, including its earlier edits. No rollback/automatic retry; unknown callback results require state inspection."""
        return material_comments.run(asset_path, "create", text, x, y, width, height, execute, save)

    @toolset_registry.tool_call
    @staticmethod
    def repair_material_comments(asset_path: str, execute: bool = False, save: bool = False) -> str:
        """Move misplaced MaterialExpressionComment objects from Expressions into EditorComments without recreation, preserving GUID/text/bounds/color and other authored comment fields. Other expression membership/order and existing comment objects are preserved. Also repairs a comment present in both lists; invalid/foreign/within-list duplicates are refused before modification. Default dry-run; close this asset's editor before mutation and reopen afterward. save=false by default; save=true saves this asset package after an actual repair. Already-correct collections are no-ops with no save attempt; use an explicit asset save for earlier unsaved edits. No compile, deletion, atomic rollback or uncertain retry."""
        return material_comments.run(asset_path, "repair", execute=execute, save=save)
