"""Named finite callback and typed-array extensions registered on the official MCP."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result


@unreal.uclass()
class CubelessParticleReceiverTools(unreal.ToolsetDefinition):
    """Finite Niagara callback authoring on a native ActorComponent Blueprint."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_particle_receiver(blueprint_name: str, graph_name: str | None = None,
            graph_id: str | None = None, graph_type: str | None = None) -> CubelessPythonOperationResult:
        """Read the exact already-loaded native callback, array and BasicParticleData contracts and target graph/declarations before and after. Does not request edits, compile or save. Graph selection can load the asset; PostLoad effects are not independently proved absent. Reports native preservation, not behavior or Niagara transport.

        Args:
            blueprint_name: Exact /Game normal Blueprint with direct native ActorComponent parent, or simple name under /Game/Blueprints. Loaded derived Blueprints, external instances and unrelated interfaces fail native preflight.
            graph_name: Optional existing default EventGraph; no creation or alternate interface graphs.
            graph_id: Optional exact nonzero graph GUID agreeing with the selected graph.
            graph_type: Optional event selector; native guard requires the directly owned standard K2 default EventGraph.
        """
        from cubeless_mcp_ue58.operations.particle_receiver import inspect_particle_receiver
        return reflected_result(inspect_particle_receiver(blueprint_name, graph_name, graph_id, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def add_particle_data_array_length_and_wire(blueprint_name: str, event_node_id: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one pure Array_Length node and immediately connect the exact ReceiveParticleData.Data output to TargetArray using a direct K2 MAKE connection. This specializes the wildcard array to const BasicParticleData without changing existing nodes or edges. This explicit creation-and-wire extension is not the legacy generic node-only array contract. No explicit full compile/save. A failed connection can leave the created node/link; no rollback or automatic retry.

        Args:
            blueprint_name: Exact /Game native ActorComponent Blueprint meeting inspect_particle_receiver restrictions, with the exact callback interface already implemented.
            event_node_id: Exact owned native ReceiveParticleData event object path or nonzero node GUID in the selected graph; no inferred UI event names.
            node_position_json: JSON [x,y] integers in -1000000..1000000; default [0,0], JSON null means origin.
            graph_name: Optional existing default EventGraph. No implicit creation.
            graph_id: Optional exact nonzero graph GUID agreeing with the selected graph.
            graph_type: Optional event selector; native requires the directly owned standard K2 default EventGraph.
            operation_id: Optional durable request ID. Identical requests replay historical results; incomplete/uncertain outcomes never retry automatically.
        """
        from cubeless_mcp_ue58.operations.particle_receiver import add_particle_data_array_length_and_wire
        return reflected_result(add_particle_data_array_length_and_wire(blueprint_name, event_node_id,
            node_position_json, graph_name, graph_id, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_particle_data_array_get_copy_and_wire(blueprint_name: str, event_node_id: str,
            node_position_json: str = "[0,0]", graph_name: str | None = None, graph_id: str | None = None,
            graph_type: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one pure Array_Get COPY node at literal Index 0 and immediately connect ReceiveParticleData.Data to TargetArray using direct K2 MAKE. Item becomes non-reference BasicParticleData; input is const and source array is not modified. Caller must gate use on a nonempty length. This explicit creation-and-wire extension does not complete generic array/GetArrayItem variants. No explicit full compile/save, rollback or automatic retry; actual partial node/link and native pin observations are retained.

        Args:
            blueprint_name: Exact /Game native ActorComponent Blueprint meeting inspect_particle_receiver restrictions, with the exact callback interface already implemented.
            event_node_id: Exact owned native ReceiveParticleData event object path or nonzero node GUID in the selected graph.
            node_position_json: JSON [x,y] integers in -1000000..1000000; default [0,0], JSON null means origin.
            graph_name: Optional existing default EventGraph. No implicit creation.
            graph_id: Optional exact nonzero graph GUID agreeing with the selected graph.
            graph_type: Optional event selector; native requires the directly owned standard K2 default EventGraph.
            operation_id: Optional durable ID. Identical requests replay recorded results; interrupted requests never resume automatically.
        """
        from cubeless_mcp_ue58.operations.particle_receiver import add_particle_data_array_get_copy_and_wire
        return reflected_result(add_particle_data_array_get_copy_and_wire(blueprint_name, event_node_id,
            node_position_json, graph_name, graph_id, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_basic_particle_data_break_node(blueprint_name: str, node_position_json: str = "[0,0]",
            graph_name: str | None = None, graph_id: str | None = None, graph_type: str | None = None,
            operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create exactly one UK2Node_BreakStruct for /Script/Niagara.BasicParticleData after the exact callback interface is implemented, with actual Position, Size and Velocity pins. Leaves its typed input unconnected; wire the non-reference Item from the explicit copy tool separately. No generic struct choice, compile, save, rollback or retry. This new typed extension recovers the historical pipeline's unsupported BasicParticleData request without claiming generic legacy BreakStruct parity.

        Args:
            blueprint_name: Exact /Game native ActorComponent Blueprint meeting inspect_particle_receiver restrictions, with the exact callback interface already implemented.
            node_position_json: JSON [x,y] integers in -1000000..1000000; default [0,0], JSON null means origin.
            graph_name: Optional existing default EventGraph. No implicit creation.
            graph_id: Optional exact nonzero graph GUID agreeing with the selected graph.
            graph_type: Optional event selector; native requires the directly owned standard K2 default EventGraph.
            operation_id: Optional durable request ID. Same normalized input replays its result; incomplete/uncertain requests are never rerun automatically.
        """
        from cubeless_mcp_ue58.operations.particle_receiver import add_basic_particle_data_break_node
        return reflected_result(add_basic_particle_data_break_node(blueprint_name, node_position_json,
            graph_name, graph_id, graph_type, operation_id))
