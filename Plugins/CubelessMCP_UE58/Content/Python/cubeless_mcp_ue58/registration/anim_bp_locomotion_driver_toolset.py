"""Public27 locomotion driver; FString numeric carrier is a private reflection workaround."""
import json
import math
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import anim_bp_locomotion_driver as operation

@unreal.uclass()
class CubelessAnimBPLocomotionDriverTools(unreal.ToolsetDefinition):
    "Create or reuse a sample AnimBP locomotion EventGraph driver. Preserve public27 defaults, ordered Speed/optional Direction, IsInAir, Acceleration(float) and IsAccelerating variables; guarded pawn/movement exec and data wiring, reuse/conflicts, requested compiler/save and topology readback in one Native operation. Named optional create flags remain True by default; only legacy Batch key omission inherits the global create flag before defaults. Native owns source-target scope, alias normalization, partial duplicate/delete/variable/node/link/default changes, compiler/save and unknown SDK effects. Structural APIs can trigger implicit skeleton compilation even when explicit compile is False. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."
    @toolset_registry.tool_call
    @staticmethod
    def ensure_anim_bp_event_graph_locomotion_driver(source_anim_bp_path: str, target_anim_bp_path: str = "", target_root: str = "/Game/_MCP_Sample/AnimStudy", variant_name: str = "LocomotionEventGraph", speed_variable: str = "Speed", create_variable_if_missing: bool = True, include_direction: bool = False, direction_variable: str = "Direction", create_direction_variable_if_missing: bool = True, include_is_in_air: bool = False, is_in_air_variable: str = "IsInAir", create_is_in_air_variable_if_missing: bool = True, include_acceleration: bool = False, acceleration_variable: str = "Acceleration", create_acceleration_variable_if_missing: bool = True, include_is_accelerating: bool = False, is_accelerating_variable: str = "IsAccelerating", create_is_accelerating_variable_if_missing: bool = True, is_accelerating_threshold: str = "3.0", guard_pawn_owner: bool = True, replace_existing_links: bool = False, allow_existing_target_reuse: bool = True, overwrite_existing: bool = False, compile: bool = True, save_asset: bool = True, dry_run: bool = False, validate_only: bool = False) -> CubelessPythonOperationResult:
        "Create or reuse a sample AnimBP locomotion EventGraph driver. Preserve public27 defaults, ordered Speed/optional Direction, IsInAir, Acceleration(float) and IsAccelerating variables; guarded pawn/movement exec and data wiring, reuse/conflicts, requested compiler/save and topology readback in one Native operation. Named optional create flags remain True by default; only legacy Batch key omission inherits the global create flag before defaults. Native owns source-target scope, alias normalization, partial duplicate/delete/variable/node/link/default changes, compiler/save and unknown SDK effects. Structural APIs can trigger implicit skeleton compilation even when explicit compile is False. No atomic rollback or automatic retry is provided. Official Named HTTP numbers may already be rounded before this adapter."
        rejected, report, raw = _guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            is_accelerating_threshold = _decode_number(is_accelerating_threshold)
        except Exception as error:
            result = operation._rejected(operation.OperationError("anim_bp_locomotion_driver_registration_carrier_invalid", str(error)))
            result["stage"] = "anim_bp_locomotion_driver_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw, native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(operation.ensure_anim_bp_event_graph_locomotion_driver(source_anim_bp_path, target_anim_bp_path, target_root, variant_name, speed_variable, create_variable_if_missing, include_direction, direction_variable, create_direction_variable_if_missing, include_is_in_air, is_in_air_variable, create_is_in_air_variable_if_missing, include_acceleration, acceleration_variable, create_acceleration_variable_if_missing, include_is_accelerating, is_accelerating_variable, create_is_accelerating_variable_if_missing, is_accelerating_threshold, guard_pawn_owner, replace_existing_links, allow_existing_target_reuse, overwrite_existing, compile, save_asset, dry_run, validate_only))

def _guard_admission():
    """Admit only this exact current wrapper's captured reflected delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, 'CubelessAnimBPLocomotionDriverAttachmentLibrary', None), 'verify_anim_bp_locomotion_driver_toolset', None)
        if not callable(helper):
            raise RuntimeError('Exact AnimBP locomotion EventGraph driver adapter raw-input admission helper is unavailable')
        raw = helper(CubelessAnimBPLocomotionDriverTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get('operation') == 'verify_anim_bp_locomotion_driver_toolset'
                and report.get('delegated_call_admitted') is True and report.get('delegated_admission_only') is True
                and report.get('delegated_method') == 'ensure_anim_bp_event_graph_locomotion_driver'
                and report.get('asset_mutation') is False and report.get('metadata_mutation') is False):
            return None, report, raw
        message = 'Canonical guarded AnimBP locomotion EventGraph driver adapter delegation is required; no Native operation was dispatched'
    except Exception as admission_error:
        message = str(admission_error)
    result = operation._rejected(operation.OperationError('anim_bp_locomotion_driver_raw_guard_admission_missing', message))
    result['stage'] = 'anim_bp_locomotion_driver_registration_admission'
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
