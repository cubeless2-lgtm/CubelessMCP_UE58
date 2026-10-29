"""Advanced editor authoring; generated assets contain standard UMG classes only."""
import unreal
import toolset_registry


@unreal.uclass()
class CubelessUMGCompositionTools(unreal.ToolsetDefinition):
    """Compose native UMG widget trees and create finite opacity/translation animations. Names equivalent to NAME_None are rejected before writes."""

    @toolset_registry.tool_call
    @staticmethod
    def compose_widget_blueprint(asset_path: str, nodes_json: str) -> str:
        """Create a NEW /Game WidgetBlueprint parented to native UserWidget. nodes_json is an ordered array of1..192 {name,type,parent?}; first node is a parentless CanvasPanel. Children reference earlier panel names. Native types: CanvasPanel,Overlay,Border,Image,TextBlock,Button,ProgressBar,SizeBox,HorizontalBox,VerticalBox,SafeZone,BackgroundBlur. Names are unique ASCII identifiers; case-insensitive NAME_None is refused before writes, including the asset basename. Content widgets allow one child. All become Blueprint variables. Existing assets/packages are refused before writes. Layout/style is applied separately through public Editor Python setters on get_source_widget. Returns JSON with actual stage/created prefix/dirty/compile/save state. Creation does not compile or save; call finalize_widget_blueprint after styling. No atomic rollback or automatic replay of partial creation."""
        return unreal.CubelessUMGCompositionLibrary.compose_widget_blueprint(asset_path, nodes_json)

    @toolset_registry.tool_call
    @staticmethod
    def add_widget_animation(asset_path: str, animation_name: str, duration: float, tracks_json: str) -> str:
        """Create a NEW standard WidgetAnimation on an existing native-parent /Game WidgetBlueprint. tracks_json array1..64: {widget,property,keys}. property is opacity with [time,value0..1] or translation with [time,x,y]. Each track has2..256 strictly increasing finite keys within duration(0,30], distinct after24000Hz quantization; translation magnitude<=8192. Case-insensitive NAME_None, existing animation/member names and absent widgets are refused before writes. Runtime bindings target source widget names; cubic auto tangents, translation-only mask. No overwrite, compile or save. Returns JSON actual partial/dirty state; finalize explicitly. No atomic rollback or uncertain retry."""
        return unreal.CubelessUMGCompositionLibrary.add_widget_animation(asset_path, animation_name, duration, tracks_json)

    @toolset_registry.tool_call
    @staticmethod
    def inspect_widget_blueprint(asset_path: str) -> str:
        """Inspect native UserWidget source tree names/classes/parents/variables and animation bindings/end times plus compile/dirty state. Editor only, outside PIE/SIE. Loading may have SDK effects. Returns JSON; no explicit save or authoring."""
        return unreal.CubelessUMGCompositionLibrary.inspect_widget_blueprint(asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def finalize_widget_blueprint(asset_path: str, save: bool) -> str:
        """Mark structurally modified and compile the explicit native-parent WidgetBlueprint after batched source styling/animation edits. Save only that asset when save=true. Reports compile attempt/status (including up-to-date-with-warnings) and actual save result/dirty state; dependent user assets are never automatically saved. Source/tree/world identity is checked after compile and save; failures retain partial state. No atomic rollback or automatic retry. Editor only, outside PIE/SIE."""
        return unreal.CubelessUMGCompositionLibrary.finalize_widget_blueprint(asset_path, save)
