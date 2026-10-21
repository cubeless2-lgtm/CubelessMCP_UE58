"""Logical skeletal sampler8; private nullable name JSON2 is internal."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import skeletal_bone_sampling as sampler


@unreal.uclass()
class CubelessSkeletalBoneSamplingTools(unreal.ToolsetDefinition):
    """Observe current skeletal component bones and sockets in editor/PIE/SIE worlds."""

    @toolset_registry.tool_call
    @staticmethod
    def sample_skeletal_bones_in_sie(actor_label: str = '', actor_name: str = '', actor_path: str = '', component_name: str = '',
            bones: str = 'null', sockets: str = 'null', prefer_pie_world: bool = True, require_pie_world: bool = False) -> CubelessPythonOperationResult:
        """Sample current bone/socket transforms from the first matching skeletal component using original world priority and filters. Empty or omitted name lists remain distinct inputs and Native retains the original default-name fallback, ignored rows, duplicate keys, missing-name warnings, current transforms and partial results. Named accepts nullable List[str]2; legacy descriptor/raw fallback inputs remain Batch-only. prefer_pie_world=True and require_pie_world=False allow the original editor fallback; requiring PIE/SIE does not start a session. Native owns game-thread, world/component lifetime, parallel-evaluation and transform-safety checks. No start/stop/tick/update/initialize/compile/save or extra asset query is added by Python/registration. Getter callbacks and label initialization may still make total SDK effects unknown; original read_only/asset_modified intent is not total purity, atomic snapshot, live animation correctness or complete pipeline proof. Full actual/partial/unknown receipts reach the sole journal before errors and uncertain observations are not replayed automatically."""
        rejected, report, raw = _sampler_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            decoded_bones = _decode_names(bones)
            decoded_sockets = _decode_names(sockets)
        except Exception as error:
            result = sampler._rejected(sampler.OperationError('skeletal_bone_sampling_registration_carrier_invalid', str(error)))
            result['stage'] = 'skeletal_bone_sampling_registration_carrier_admission'
            result['data'].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(sampler.sample_skeletal_bones_in_sie(actor_label, actor_name, actor_path, component_name,
            decoded_bones, decoded_sockets, prefer_pie_world, require_pie_world))


def _sampler_guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessSkeletalBoneSamplingAttachmentLibrary', None), 'verify_skeletal_bone_sampling_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact skeletal sampler raw-input admission helper is unavailable')
        raw = helper(CubelessSkeletalBoneSamplingTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_skeletal_bone_sampling_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'sample_skeletal_bones_in_sie'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded skeletal sampler delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = sampler._rejected(sampler.OperationError('skeletal_bone_sampling_raw_guard_admission_missing', message))
    result['stage'] = 'skeletal_bone_sampling_registration_admission'
    result['data'].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _invalid_constant(value):
    raise ValueError('Non-finite JSON is unsupported: ' + value)


def _decode_names(text):
    if type(text) is not str or len(text.encode('utf-16-le')) // 2 > 1048576 or '\x00' in text:
        raise ValueError('Bounded private nullable name JSON text is required')
    depth = 0; quoted = False; escaped = False
    for char in text:
        if quoted:
            if escaped: escaped = False
            elif char == '\\': escaped = True
            elif char == '"': quoted = False
        elif char == '"': quoted = True
        elif char in '[{':
            depth += 1
            if depth > 64: raise ValueError('Name JSON depth64 was exceeded')
        elif char in ']}':
            depth -= 1
            if depth < 0: raise ValueError('Malformed name JSON')
    if quoted or depth: raise ValueError('Malformed name JSON')
    value = json.loads(text, parse_constant=_invalid_constant)
    if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
        raise ValueError('Public bones/sockets remain nullable List[str]; raw descriptors are Batch-only')
    if type(value) is list:
        if len(value) > 65535: raise ValueError('Name-list JSON value-slot bound was exceeded')
        if any('\x00' in item for item in value): raise ValueError('Decoded name NUL is refused')
    return value
