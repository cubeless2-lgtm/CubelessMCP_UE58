import unreal
import toolset_registry
import cubeless_kimodo as kimodo

@unreal.uclass()
class CubelessKimodoTools(unreal.ToolsetDefinition):
    @toolset_registry.tool_call
    @staticmethod
    def generate_kimodo_motion(prompt: str, duration: float = 2.0, seed: int = 1218) -> str:
        """Generate local Kimodo motion for the configured mannequin. Returns a job ID for poll_kimodo_motion."""
        return kimodo.generate(prompt,duration,seed)

    @toolset_registry.tool_call
    @staticmethod
    def poll_kimodo_motion(job_id: str) -> str:
        """Advance an editor authoring job and return phase, result asset, or error."""
        return kimodo.poll(job_id)

    @toolset_registry.tool_call
    @staticmethod
    def correct_kimodo_motion(source_job: str, markers_json: str) -> str:
        """Rebake from the original motion and update the same animation asset in place."""
        return kimodo.correct(source_job,markers_json)

unreal.ToolsetRegistry.register_toolset_class(CubelessKimodoTools)

import kimodo_native
kimodo_native.restore()

