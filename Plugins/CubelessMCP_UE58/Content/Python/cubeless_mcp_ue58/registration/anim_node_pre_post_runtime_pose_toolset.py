"""Public35 AnimGraph node probe with private array/numeric reflection carriers."""
import json
import math
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_node_pre_post_runtime_pose as operation

@unreal.uclass()
class CubelessAnimNodePrePostRuntimePoseTools(unreal.ToolsetDefinition):
    "Resolve an AnimGraph node and preserve the original public35/defaults and four mode meanings: compiled mapping, same-instance PoseWatch input/output capture, active final-component tick delta, or isolated source-bypass/selected-node clone comparison. Native owns node/instance selection, sample fallbacks, ordered tick/refresh, debug attachments, temporary clones, compiler/cleanup and partial or unknown SDK effects. Isolated mode supports the original RigidBody/Trail MVP and temporary-root policy; asset duplication is refused during PIE/SIE by the existing public SDK route. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."
    @toolset_registry.tool_call
    @staticmethod
    def sample_anim_node_pre_post_runtime_pose(blueprint_name: str = "", anim_blueprint: str = "", graph_name: str = "AnimGraph", graph_id: str = "", graph_type: str = "function", node_id: str = "", node_type: str = "", title_contains: str = "", sample_bones: str = "null", sample_sockets: str = "null", mode: str = "active_component_tick_delta", skeletal_mesh: str = "", temp_root: str = "/Game/_MCP_Temp/AnimNodePrePost", cleanup: bool = True, overwrite_existing_temp_assets: bool = True, run_id: str = "", actor_label: str = "", actor_name: str = "", actor_path: str = "", component_name: str = "", anim_instance_source: str = "main", prefer_pie_world: bool = True, require_pie_world: bool = False, tick_count: str = "1", tick_delta_time: str = "0.03333333333333333", settle_tick_count: str = "0", refresh_bone_transforms: bool = True, allow_missing_bones: bool = False, pose_link_max_depth: str = "4", input_pose_mode: str = "first", input_pose_field_path: str = "", input_pose_index: str = "-1", include_pins: bool = True, max_depth: str = "3", dry_run: bool = True) -> CubelessPythonOperationResult:
        "Resolve an AnimGraph node and preserve the original public35/defaults and four mode meanings: compiled mapping, same-instance PoseWatch input/output capture, active final-component tick delta, or isolated source-bypass/selected-node clone comparison. Native owns node/instance selection, sample fallbacks, ordered tick/refresh, debug attachments, temporary clones, compiler/cleanup and partial or unknown SDK effects. Isolated mode supports the original RigidBody/Trail MVP and temporary-root policy; asset duplication is refused during PIE/SIE by the existing public SDK route. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."
        rejected, report, raw = _guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            sample_bones, sample_sockets = _decode_name_lists(sample_bones, sample_sockets)
            tick_count = _decode_integer(tick_count)
            settle_tick_count = _decode_integer(settle_tick_count)
            pose_link_max_depth = _decode_integer(pose_link_max_depth)
            input_pose_index = _decode_integer(input_pose_index)
            max_depth = _decode_integer(max_depth)
            tick_delta_time = _decode_number(tick_delta_time)
        except Exception as error:
            result = operation._rejected(operation.OperationError('anim_node_pre_post_runtime_pose_registration_carrier_invalid', str(error)))
            result['stage'] = 'anim_node_pre_post_runtime_pose_registration_carrier_admission'
            result['data'].update(registration_guard=report, registration_guard_raw=raw, native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.sample_anim_node_pre_post_runtime_pose(blueprint_name, anim_blueprint, graph_name, graph_id, graph_type, node_id, node_type, title_contains, sample_bones, sample_sockets, mode, skeletal_mesh, temp_root, cleanup, overwrite_existing_temp_assets, run_id, actor_label, actor_name, actor_path, component_name, anim_instance_source, prefer_pie_world, require_pie_world, tick_count, tick_delta_time, settle_tick_count, refresh_bone_transforms, allow_missing_bones, pose_link_max_depth, input_pose_mode, input_pose_field_path, input_pose_index, include_pins, max_depth, dry_run))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessAnimNodePrePostRuntimePoseAttachmentLibrary', None), 'verify_anim_node_pre_post_runtime_pose_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact AnimGraph node pre/post probe adapter raw-input admission helper is unavailable')
        raw = helper(CubelessAnimNodePrePostRuntimePoseTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_anim_node_pre_post_runtime_pose_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'sample_anim_node_pre_post_runtime_pose'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded AnimGraph node pre/post probe adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('anim_node_pre_post_runtime_pose_raw_guard_admission_missing', message))
    result['stage'] = 'anim_node_pre_post_runtime_pose_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError('Non-finite JSON is unsupported: '+value)

def _decode_number(text):
    if type(text) is not str or len(text.encode('utf-16-le'))//2>1048576 or '\x00' in text:
        raise ValueError('Bounded private finite-number JSON text required')
    value=json.loads(text,parse_constant=_invalid_constant)
    if type(value) not in (int,float):
        raise ValueError('Private number text must decode to a JSON number, not boolean/string/null')
    value=float(value)
    if not math.isfinite(value):
        raise ValueError('Private number exceeds finite double')
    return value


def _decode_name_lists(*carriers):
    if len(carriers) != 2:
        raise ValueError('Exactly two name-list carriers required')
    decoded = []
    for text in carriers:
        if type(text) is not str or len(text.encode('utf-16-le')) // 2 > 1048576 or '\x00' in text:
            raise ValueError('Bounded private name-list JSON text required')
        value = json.loads(text, parse_constant=_invalid_constant)
        if value is not None and (type(value) is not list or len(value) > 65535 or any(type(row) is not str or '\x00' in row for row in value)):
            raise ValueError('Formal Named samples require null or List[str]')
        decoded.append(value)
    return decoded

def _decode_integer(text):
    """Decode only the private canonical int64 decimal; Native retains the original clamp."""
    if type(text) is not str or not text or len(text) > 20:
        raise ValueError("Private integer limit requires a bounded canonical decimal")
    value = int(text)
    if str(value) != text or value < -(1 << 63) or value > (1 << 63) - 1:
        raise ValueError("Private integer limit must be an exact canonical int64 decimal")
    return value
