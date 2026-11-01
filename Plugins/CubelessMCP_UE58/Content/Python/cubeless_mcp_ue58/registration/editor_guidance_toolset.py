"""Thin official MCP adapter to the transport-independent editor guidance service."""
import json
import unreal
import toolset_registry


def _request(action, **arguments):
    if arguments.get('step_label') == 'auto':
        arguments['step_label'] = ''
    return unreal.CubelessEditorGuidanceLibrary.editor_guidance_request(
        action, json.dumps(arguments, ensure_ascii=False))


@unreal.uclass()
class CubelessEditorGuidanceTools(unreal.ToolsetDefinition):
    """Temporary Blueprint/AnimBlueprint and legacy Control Rig Blueprint UI guidance. No asset writes or automatic setting changes."""

    @toolset_registry.tool_call
    @staticmethod
    def inspect_guidance_targets(asset_path: str) -> str:
        """Inspect an already loaded, open standard Blueprint/AnimBP or legacy Control Rig Blueprint editor. Return actual graph/node/pin GUIDs, RigVM model paths where present, and currently displayed Details paths. New non-Blueprint RigVM assets are outside this route. Does not load or open assets."""
        return _request('inspect', asset_path=asset_path)

    @toolset_registry.tool_call
    @staticmethod
    def show_node(asset_path: str, graph_id: str, node_id: str, message: str = '강조된 항목을 확인하세요.', mode: str = 'combined', step_label: str = 'auto') -> str:
        """Select and focus an existing node by graph/node GUID; show transient cyan guidance. Message max200 characters. Query status after Slate layout; accepted request is not display proof."""
        return _request('show_node', asset_path=asset_path, graph_id=graph_id, node_id=node_id, message=message, mode=mode, step_label=step_label)

    @toolset_registry.tool_call
    @staticmethod
    def show_property(asset_path: str, graph_id: str, node_id: str, property_path: str, message: str = '강조된 설정을 확인하세요.', mode: str = 'combined', step_label: str = 'auto') -> str:
        """Select node and scroll to its exact Details property path; temporary yellow guidance only. Does not change the property. Standard UE5.8.4 property rows only; unsupported/hidden/clipped rows fail closed. Use inspect_guidance_targets for actual paths."""
        return _request('show_property', asset_path=asset_path, graph_id=graph_id, node_id=node_id, property_path=property_path, message=message, mode=mode, step_label=step_label)

    @toolset_registry.tool_call
    @staticmethod
    def set_guidance_visibility(visible: bool) -> str:
        """Hide/resume retained guidance without selecting or moving anything. Show modes: combined, outline, arrow, card. step_label supports MCP-driven step switching; repeat show with the next exact target, then query status."""
        return _request('resume' if visible else 'hide')

    @toolset_registry.tool_call
    @staticmethod
    def clear_guidance() -> str:
        """Remove all owned guidance without modifying or saving assets."""
        return _request('clear')

    @toolset_registry.tool_call
    @staticmethod
    def guidance_status() -> str:
        """Return actual target resolution and display state after current layout; hidden/unsupported targets never imply visible guidance."""
        return _request('status')

    @toolset_registry.tool_call
    @staticmethod
    def explain_target(target_json: str) -> str:
        """Explain an inspected semantic target. JSON requires asset_path,target_kind; supply fresh Korean message and step_label. Kinds node/graph/pin/property/selection/component/variable/panel/widget. Graph/node/pin use GUIDs; property/selection owner_kind is node/graph/class_defaults/class_settings/component/variable; component identity is inspected object_path; variable identity is inspected variable_id; panel anchor details/my_blueprint/components; widget anchor is an inspected unique actual tag. Existing CDO only. Query guidance_status after layout for display proof. Variable highlights its actual selected inspector. Inspect returns separate direct scalar values; value reads do not prove painting. Unsupported/custom rows fail closed. No property writer, compile or save."""
        try:
            target = json.loads(target_json)
        except (TypeError, ValueError):
            return json.dumps({'success': False, 'error': 'invalid_target_json'})
        if not isinstance(target, dict):
            return json.dumps({'success': False, 'error': 'target_object_required'})
        allowed = {'asset_path', 'target_kind', 'owner_kind', 'graph_id', 'node_id',
                   'pin_id', 'property_path', 'identity', 'anchor', 'message',
                   'mode', 'step_label'}
        if set(target) - allowed:
            return json.dumps({'success': False, 'error': 'unknown_target_fields'})
        return _request('show_target', **target)

    @toolset_registry.tool_call
    @staticmethod
    def start_guide_sequence(steps_json: str) -> str:
        """Start an in-memory guide with real native Previous/Next UI. steps_json is an array of 1-32 objects: target_token (verification_token from actual prior paint in this editor process), Korean title<=60 and message<=200. Investigate/show/verify every semantic target first. No assets/scripts/settings are saved. Targets are outlined and the controller shows the current dynamic explanation and order. A stale/missing step stops without skipping. Clear/end also releases certificates."""
        try:
            steps = json.loads(steps_json)
        except (TypeError, ValueError):
            return json.dumps({'success': False, 'error': 'invalid_steps_json'})
        if not isinstance(steps, list):
            return json.dumps({'success': False, 'error': 'steps_array_required'})
        return _request('sequence_start', steps=steps)

    @toolset_registry.tool_call
    @staticmethod
    def guide_sequence_control(action: str) -> str:
        """Control current temporary guide: previous, next, pause, resume, end. Questions may pause while keeping the step index, then resume the same target. Next requires actual display. No automatic step advance or silent skip. End removes guidance, controller and temporary certificates."""
        if action not in ('previous', 'next', 'pause', 'resume', 'end'):
            return json.dumps({'success': False, 'error': 'invalid_sequence_control'})
        return _request('sequence_' + action)

    @toolset_registry.tool_call
    @staticmethod
    def guide_sequence_status() -> str:
        """Read temporary step list, zero-based current_index, one-based current_step, phase, pause/error and actual target state. phase ready requires actual recent paint; accepted start is not display proof. No tutorial content is persisted."""
        return _request('sequence_status')
