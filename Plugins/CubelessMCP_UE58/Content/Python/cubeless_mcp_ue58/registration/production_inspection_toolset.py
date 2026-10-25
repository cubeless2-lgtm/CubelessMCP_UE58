"""Named readback for optional external production pipelines."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult,reflected_result
from ..operations import production_inspection as domain

@unreal.uclass()
class CubelessProductionInspectionTools(unreal.ToolsetDefinition):
    """Bounded actual placement readback, independent of external Python clients."""
    @toolset_registry.tool_call
    @staticmethod
    def inspect_pcg_placement(actor_path:str,max_instances:int=10000)->CubelessPythonOperationResult:
        """Read all ISM/HISM world transforms and mesh refs on one exact loaded Editor actor. Fail rather than truncate above max_instances (1..10000). Does not load, mutate, generate, compile or save. Actor-owned components are not proof of PCG ownership/current completion; pair with PCG refresh terminal receipt and output inspection."""
        return reflected_result(domain.inspect_pcg_placement(actor_path,max_instances))
