"""Logical Post Process variant17; one reflected rotation FString is private."""
import json
import math
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import postprocess_anim_demo_variant as variant


@unreal.uclass()
class CubelessPostProcessAnimDemoVariantTools(unreal.ToolsetDefinition):
    """Create or reuse a stock Post Process AnimBP/SkeletalMesh demo variant."""

    @toolset_registry.tool_call
    @staticmethod
    def ensure_postprocess_anim_demo_variant(source_blueprint_name: str, source_skeletal_mesh: str,
            variant_name: str = 'Variant', target_root: str = '/Game/_MCP_Sample/AnimStudy',
            target_blueprint_name: str = '', target_skeletal_mesh: str = '', bone_name: str = 'head',
            rotation: str = 'null', graph_name: str = 'AnimGraph', graph_id: str = '', graph_type: str = 'function',
            replace_existing: bool = True, overwrite_existing: bool = False, compile: bool = True,
            save: bool = True, dry_run: bool = False, allow_non_sample: bool = False) -> CubelessPythonOperationResult:
        """Duplicate or reuse the target stock AnimBlueprint and SkeletalMesh, ensure the Modify Bone chain, compile as requested or when a generated class is needed, assign the mesh Post Process class and optionally save in the original order. Required sources2 and original public17 defaults are preserved; rotation remains whole Any JSON and only None becomes [0,0,6] in the domain. Empty public target paths select derived variant paths; private raw key presence and graph aliases remain Batch-only. Native owns target path/same-source protection, typed reuse, overwrite deletion, graph change, compiler/class validation, assignment and actual save receipts. Overwrite is destructive within the permitted target scope and late failures can leave earlier duplicates, graph changes, compiler work, mesh assignment or saves. dry_run reports prepared paths without explicitly dispatching those edits, but source loading and SDK callbacks can still have unknown effects. Full actual/partial/compile/save/change-or-unknown state reaches the single journal before errors. No atomic rollback, uncertain-action retry, total SDK purity, live pose or complete external pipeline is promised."""
        rejected, report, raw = _variant_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            decoded_rotation = _decode_rotation(rotation)
        except Exception as error:
            result = variant._rejected(variant.OperationError('postprocess_anim_demo_variant_registration_carrier_invalid', str(error)))
            result['stage'] = 'postprocess_anim_demo_variant_registration_carrier_admission'
            result['data'].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(variant.ensure_postprocess_anim_demo_variant(source_blueprint_name, source_skeletal_mesh,
            variant_name, target_root, target_blueprint_name, target_skeletal_mesh, bone_name, decoded_rotation,
            graph_name, graph_id, graph_type, replace_existing, overwrite_existing, compile, save, dry_run, allow_non_sample))


def _variant_guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessPostProcessAnimDemoVariantAttachmentLibrary', None), 'verify_postprocess_anim_demo_variant_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact Post Process variant raw-input admission helper is unavailable')
        raw = helper(CubelessPostProcessAnimDemoVariantTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_postprocess_anim_demo_variant_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'ensure_postprocess_anim_demo_variant'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded Post Process variant delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = variant._rejected(variant.OperationError('postprocess_anim_demo_variant_raw_guard_admission_missing', message))
    result['stage'] = 'postprocess_anim_demo_variant_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError('Non-finite JSON is unsupported: ' + value)


def _decode_rotation(text):
    if type(text) is not str or len(text.encode('utf-16-le')) // 2 > 1048576 or '\x00' in text:
        raise ValueError('Bounded private rotation JSON text is required')
    depth = 0; quoted = False; escaped = False
    for char in text:
        if quoted:
            if escaped: escaped = False
            elif char == '\\': escaped = True
            elif char == '"': quoted = False
        elif char == '"': quoted = True
        elif char in '[{':
            depth += 1
            if depth > 64: raise ValueError('Rotation JSON depth64 was exceeded')
        elif char in ']}':
            depth -= 1
            if depth < 0: raise ValueError('Malformed rotation JSON')
    if quoted or depth: raise ValueError('Malformed rotation JSON')
    result = json.loads(text, parse_constant=_invalid_constant)
    work = [result]; slots = 0
    while work:
        value = work.pop(); slots += 1
        if slots > 65536: raise ValueError('Rotation JSON slot bound was exceeded')
        if type(value) is float and not math.isfinite(value): raise ValueError('Finite rotation JSON is required')
        if type(value) is str and '\x00' in value: raise ValueError('Decoded rotation NUL is refused')
        if type(value) is dict:
            if any('\x00' in key for key in value): raise ValueError('Decoded rotation key NUL is refused')
            work.extend(value.values())
        elif type(value) is list: work.extend(value)
    return result
