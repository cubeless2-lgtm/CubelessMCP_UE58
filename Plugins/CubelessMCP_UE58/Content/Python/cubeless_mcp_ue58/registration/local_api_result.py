"""Feature-specific stock SDK string future; no ticker or transport lifecycle."""

import unreal


@unreal.uclass()
class CubelessLocalCommandAsyncResult(unreal.ToolCallAsyncResultString):
    source = unreal.uproperty(unreal.ToolCallAsyncResult)
    command_type = unreal.uproperty(str)

    @unreal.ufunction(params=[unreal.ToolCallAsyncResult])
    def consume_source(self, completed):
        from ..operations.local_api import complete_command
        complete_command(self, completed)
