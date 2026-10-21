"""Original state-machine observer6 on the existing standard11 result."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_state_machine_inspection


@unreal.uclass()
class CubelessAnimStateMachineInspectionTools(unreal.ToolsetDefinition):
    """Authored Animation Blueprint state/transition rule observations. Source loads and title/member caches may have effects; no direct graph creation, modification, compiler or save. Static observations do not prove runtime animation, cold persistence or complete original workflows."""
    @toolset_registry.tool_call
    @staticmethod
    def inspect_anim_state_machine_transitions(blueprint_name: str, state_machine_name: str = "",
            include_pins: bool = True, include_rule_graph_nodes: bool = True,
            include_state_nodes: bool = True, max_rule_graph_nodes: int = 256) -> CubelessPythonOperationResult:
        """Original IgnoreCase state-machine name/title filter, state/source/target summaries, transition blend/priority/shared settings, bound rule/custom graphs and optional pin links. Native clamps per-transition rule node limit to [-1,2048]; -1 is unlimited inside the admitted overall Native scope, 0 returns no rule nodes. Actual nested counts/truncation/errors are retained. Generic Blueprints can return matching zero; no forced Animation-class or default-graph allocation. DR node settings/protected-pose inspection is a distinct contract."""
        return reflected_result(anim_state_machine_inspection.inspect_anim_state_machine_transitions(
            blueprint_name, state_machine_name, include_pins, include_rule_graph_nodes,
            include_state_nodes, max_rule_graph_nodes))
