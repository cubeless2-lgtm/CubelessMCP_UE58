"""Exact Linked AnimGraph public14 and guarded owner admission."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import linked_anim_graph_variant as operation

@unreal.uclass()
class CubelessLinkedAnimGraphVariantTools(unreal.ToolsetDefinition):
    "Duplicate or reuse a guarded sample AnimBP and attach the existing same-skeleton linked AnimBP generated class. Reconstruct the LinkedAnimGraph node, connect an exposed input pose when present and its output to Root, then preserve compiler/save and topology readback. dry_run or validate_only performs admission and loaded diagnostics before target mutation. Overwrite, graph callbacks, partial links, compiler and save effects are non-atomic; actual Native prefixes and SDK change uncertainty remain authoritative and no failed request is automatically retried. Linked Anim Layer/interface creation is unsupported. Command aliases and private graph selection remain Batch-only."
    @toolset_registry.tool_call
    @staticmethod
    def ensure_linked_anim_graph_sample_variant(source_anim_bp_path: str, linked_anim_bp_path: str, target_anim_bp_path: str = "", target_root: str = "/Game/_MCP_Sample/AnimStudy", variant_name: str = "LinkedAnimGraphSample", insert_mode: str = "input_pose_to_linked_graph_to_root", allow_existing_target_reuse: bool = True, overwrite_existing: bool = False, replace_existing_links: bool = True, require_input_pose_pin: bool = False, compile: bool = True, save_asset: bool = False, dry_run: bool = False, validate_only: bool = False) -> CubelessPythonOperationResult:
        "Duplicate or reuse a guarded sample AnimBP and attach the existing same-skeleton linked AnimBP generated class. Reconstruct the LinkedAnimGraph node, connect an exposed input pose when present and its output to Root, then preserve compiler/save and topology readback. dry_run or validate_only performs admission and loaded diagnostics before target mutation. Overwrite, graph callbacks, partial links, compiler and save effects are non-atomic; actual Native prefixes and SDK change uncertainty remain authoritative and no failed request is automatically retried. Linked Anim Layer/interface creation is unsupported. Command aliases and private graph selection remain Batch-only."
        rejected, report, raw = _guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        return reflected_result(operation.ensure_linked_anim_graph_sample_variant(source_anim_bp_path, linked_anim_bp_path, target_anim_bp_path, target_root, variant_name, insert_mode, allow_existing_target_reuse, overwrite_existing, replace_existing_links, require_input_pose_pin, compile, save_asset, dry_run, validate_only))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessLinkedAnimGraphVariantAttachmentLibrary', None), 'verify_linked_anim_graph_variant_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Linked AnimGraph adapter raw-input admission helper is unavailable')
        raw = helper(CubelessLinkedAnimGraphVariantTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_linked_anim_graph_variant_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'ensure_linked_anim_graph_sample_variant'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Linked AnimGraph adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('linked_anim_graph_variant_raw_guard_admission_missing', message))
    result['stage'] = 'linked_anim_graph_variant_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


