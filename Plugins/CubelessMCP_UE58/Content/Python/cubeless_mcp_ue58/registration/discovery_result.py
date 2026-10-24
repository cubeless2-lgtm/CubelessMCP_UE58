"""Stock SDK string future adapter for one existing registered tool call."""
import unreal


@unreal.uclass()
class CubelessDiscoveryAsyncResult(unreal.ToolCallAsyncResultString):
    source = unreal.uproperty(unreal.ToolCallAsyncResult)
    legacy_tool_name = unreal.uproperty(str)

    @unreal.ufunction(params=[unreal.ToolCallAsyncResult])
    def consume_source(self, completed):
        from ..operations.tool_discovery import complete_call
        complete_call(self, completed)
