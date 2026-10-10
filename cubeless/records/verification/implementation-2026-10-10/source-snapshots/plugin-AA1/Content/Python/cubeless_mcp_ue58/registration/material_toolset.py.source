"""Named legacy Material tools on the official registry; no external bridge."""

import unreal
import toolset_registry

from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import material


@unreal.uclass()
class CubelessMaterialTools(unreal.ToolsetDefinition):
    """Cubeless Material inspection and bounded graph edits. Maximum 2048 expressions; no explicit compile/save, shader completion or disk reload proof."""

    @toolset_registry.tool_call
    @staticmethod
    def resolve_material_graph(material_path: str, graph_type: str = "auto") -> CubelessPythonOperationResult:
        """Resolve a Material or Material Function; return native candidates and loaded identity in data_json.

        Args:
            material_path: Short name, package path or object path; use full paths to avoid ambiguity.
            graph_type: auto, material or function.
        """
        return reflected_result(material.resolve_material_graph(material_path, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def list_material_nodes(material_path: str, graph_type: str = "auto", node_type: str | None = None, desc_contains: str | None = None, include_pins: bool = True) -> CubelessPythonOperationResult:
        """List Material/Function expressions and optional pins, links and root-property connections; no compile/save.

        Args:
            material_path: Material or Material Function name/package/object path.
            graph_type: auto, material or function.
            node_type: Optional expression class or object-name substring filter; omitted/null means no filter.
            desc_contains: Optional expression description substring filter; omitted/null means no filter.
            include_pins: Include pin and connection metadata.
        """
        return reflected_result(material.list_material_nodes(material_path, graph_type, node_type or "", desc_contains or "", include_pins))

    @toolset_registry.tool_call
    @staticmethod
    def analyze_material_graph(material_path: str, graph_type: str = "auto", include_nodes: bool = False, include_usage: bool = True, max_referencers: int = 25) -> CubelessPythonOperationResult:
        """Inspect a Material, Function or Instance with native graph summaries and optional usage hints; no shader compilation proof.

        Args:
            material_path: Material, Material Function or Material Instance name/path.
            graph_type: auto, material or function; Instances use auto/material.
            include_nodes: Include full expression and pin data in the analysis.
            include_usage: Include Asset Registry package referencer hints.
            max_referencers: Maximum referencer packages returned with usage hints.
        """
        return reflected_result(material.analyze_material_graph(material_path, graph_type, include_nodes, include_usage, max_referencers))

    @toolset_registry.tool_call
    @staticmethod
    def list_material_collection_parameter_nodes(material_path: str, graph_type: str = "auto") -> CubelessPythonOperationResult:
        """Inspect collection-parameter expressions, parameter identity/type and mismatch diagnostics without editing.

        Args:
            material_path: Material or Material Function name/path.
            graph_type: auto, material or function.
        """
        return reflected_result(material.list_material_collection_parameter_nodes(material_path, graph_type))

    @toolset_registry.tool_call
    @staticmethod
    def get_material_parameter_collection_values(collection_path: str, parameter_names: list[str] = [], include_asset_defaults: bool = True, include_runtime: bool = True) -> CubelessPythonOperationResult:
        """Read MPC asset defaults and available editor/PIE-world values; returned runtime_resolved flags distinguish missing world values.

        Args:
            collection_path: Material Parameter Collection name/path.
            parameter_names: Scalar/vector names; omitted or empty array means all. Supply an array, not null. Each trimmed name is limited to 1023 UTF-16 code units; an oversized entry rejects the entire filter before native name construction.
            include_asset_defaults: Include values stored on the collection asset.
            include_runtime: Query the available editor/PIE-world collection instance.
        """
        # Copy the reflected array/default; never expose the shared default to
        # an operation. Optional arrays fail UE58's JSON-to-property import.
        return reflected_result(material.get_material_parameter_collection_values(collection_path, list(parameter_names), include_asset_defaults, include_runtime))

    @toolset_registry.tool_call
    @staticmethod
    def connect_material_nodes(material_path: str, from_node_id: str, to_node_id: str, to_input: str, from_output: str | None = None, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Connect an expression output to an input, preserving before/after evidence. No explicit compile/save; editor change may schedule shaders.

        Args:
            material_path: Material or Material Function name/path.
            from_node_id: Source node_key/selector returned by list/analyze.
            to_node_id: Target node_key/selector returned by list/analyze.
            to_input: Target input pin name.
            from_output: Source output pin name; omitted/null/empty means first output.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; reuse returns completed recorded result, never retries an interrupted mutation.
        """
        return reflected_result(material.connect_material_nodes(material_path, from_node_id, to_node_id, to_input, from_output or "", graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def connect_material_property(material_path: str, from_node_id: str, property: str, from_output: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Connect an expression to a root Material property and inspect the postcondition; Functions are rejected. No explicit compile/save.

        Args:
            material_path: Material name/path.
            from_node_id: Source node_key/selector from list/analyze.
            property: Root property such as BaseColor, Roughness, Normal or MP_BaseColor.
            from_output: Source output name; omitted/null/empty means first output.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.connect_material_property(material_path, from_node_id, property, from_output or "", operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def delete_material_node(material_path: str, node_id: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Delete one expression and disconnect its links, recording observed before/after; no atomic rollback or automatic compile/save.

        Args:
            material_path: Material or Material Function name/path.
            node_id: Exact node_key; native guid:, name:, path:, GUID/name/path selectors reject ambiguity.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.delete_material_node(material_path, node_id, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def layout_material_nodes(material_path: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Apply the engine's Material expression layout and verify structural/position observations; no explicit compile/save or visual-quality guarantee.

        Args:
            material_path: Material or Material Function name/path.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; interrupted requests are not automatically retried.
        """
        return reflected_result(material.layout_material_nodes(material_path, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_material_node(material_path: str, expression_class: str, graph_type: str = "auto", node_position: list[int] = [0, 0], selected_asset: str | None = None, properties_json: str | None = None, operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one audited native Engine expression with converted property values and before/after verification; no explicit compile/save.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            expression_class: Native Engine MaterialExpression class or supported short name, such as Constant or Multiply. Plugin/generated/editor expression classes are not admitted.
            graph_type: auto, material or function.
            node_position: Two int32 editor coordinates [X,Y]; omitted means [0,0]. Supply an array, not null. Fractional coordinates are rejected.
            selected_asset: Optional asset path consumed by the selected texture/function/collection expression class; omitted/null means none. Runtime reference guards restrict accepted classes, mounts and dependencies.
            properties_json: JSON object of editable properties, e.g. {"R":0.25,"Desc":"source"}; omitted/null means {}. Replaces legacy properties dict at the reflected boundary. At most 64 properties/65536 UTF-16 code units. Property names and direct FName values are limited to 1023 UTF-16 units. Native ImportText for name-bearing, struct or unaudited values, including arrays of structs, must fit 1023 units after encoding; larger aggregates remain unsupported. Custom shader include paths and preprocessor markers (#, %:, ??=) in assigned shader text are unsupported. Native temporary conversion and reference audit precede creation; cleanup is not atomic package rollback.
            operation_id: Optional durable request ID. Interrupted outcomes never retry; an identical completed ID returns the historical result.
        """
        # Use FArrayProperty directly: an optional array is misread as a fixed
        # scalar array by UE58 JsonObjectConverter before optional unwrapping.
        # Copy the reflected array/default before strict operation validation.
        position = list(node_position)
        return reflected_result(material.add_material_node(material_path, expression_class, graph_type, position, selected_asset, properties_json, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def set_material_node_property(material_path: str, node_id: str, property_name: str, value_json: str, graph_type: str = "auto", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Convert and validate a persistent editable expression property before applying it; read back the native value and record partial failure.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            node_id: Exact node_key or unambiguous native guid:/name:/path: selector returned by list/analyze.
            property_name: Editable persistent property on a native Engine expression, at most 1023 UTF-16 code units; const/transient/instanced/export properties are rejected.
            value_json: Exactly one JSON value, e.g. 0.75, true, "caption", {"R":1,"G":0,"B":0,"A":1}, or null. String/path values require JSON quotes. Replaces legacy value Any. Maximum JSON length 65536 UTF-16 units; direct FName values are limited to 1023. Native ImportText for name-bearing, struct or unaudited types, including struct arrays, must fit 1023 units after encoding; long aggregates remain unsupported. Plain FString values retain the larger JSON limit. Custom shader include paths and preprocessor markers (#, %:, ??=) in assigned shader text are unsupported; unrelated fields do not rewrite existing shader text. No compile/save completion claim.
            graph_type: auto, material or function.
            operation_id: Optional durable request ID; no automatic retry or rollback of uncertain changes.
        """
        return reflected_result(material.set_material_node_property(material_path, node_id, property_name, value_json, graph_type, operation_id))

    @toolset_registry.tool_call
    @staticmethod
    def add_custom_material_node(material_path: str, code: str, output_type: str = "CMOT_Float3", inputs_json: str = "[]", description: str | None = None, graph_type: str = "auto", node_position_json: str = "[0,0]", additional_defines_json: str = "[]", include_file_paths_json: str = "[]", operation_id: str | None = None) -> CubelessPythonOperationResult:
        """Create one validated Custom HLSL expression using the shared native Add core; no explicit compile/save or shader-validity claim.

        Args:
            material_path: Material or Material Function name/path resolving under /Game.
            code: Nonempty HLSL source, at most 65536 UTF-16 code units. Preprocessor markers #, %: and ??= are conservatively rejected, including inside comments/strings. Creation/readback does not prove shader compilation or rendering.
            output_type: CMOT_Float1/2/3/4 or CMOT_MaterialAttributes; legacy float/number/attributes aliases are accepted. Default CMOT_Float3 is retained explicitly.
            inputs_json: JSON array of at most 64 input-name strings or objects with name/input_name, e.g. ["Tint"]. Valid nonreserved HLSL identifiers must be unique ignoring case and each fit 1023 UTF-16 units. The entire native-encoded input struct array, including field-name/quoting overhead, must also fit 1023 units; larger arrays remain unsupported even within the count cap. Replaces legacy inputs; omitted means [].
            description: Optional Custom description and inherited Desc, at most 4096 UTF-16 code units; omitted/null means empty.
            graph_type: auto, material or function.
            node_position_json: JSON array of exactly two int32 editor coordinates, e.g. [-400,200]; omitted means [0,0].
            additional_defines_json: JSON array of at most 64 objects with name/define_name and optional value/define_value (string, number or boolean; default "1"). Preprocessor markers in values are rejected. The entire native-encoded define struct array, including field-name/quoting overhead, must fit 1023 UTF-16 units; larger aggregates remain unsupported. Replaces legacy additional_defines; omitted means [].
            include_file_paths_json: Must encode [] in this slice. Nonempty include paths are unsupported until shader dependency closure is audited. Replaces legacy include_file_paths; omitted means []. Each JSON string is limited to 65536 UTF-16 code units, depth 16 and 8192 values; combined native-encoded Custom properties must also fit 65536 UTF-16 units, including encoding overhead.
            operation_id: Optional durable request ID. An identical completed ID returns its recorded result; interrupted outcomes are never automatically retried. Failed creation cleanup is not atomic rollback.
        """
        return reflected_result(material.add_custom_material_node(material_path, code, output_type, inputs_json, description,
            graph_type, node_position_json, additional_defines_json, include_file_paths_json, operation_id))
