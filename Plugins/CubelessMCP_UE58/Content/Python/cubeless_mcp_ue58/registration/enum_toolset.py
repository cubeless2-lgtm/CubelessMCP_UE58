"""Original enum6 on official Toolset Registry; array JSON is a private ABI."""
import json
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import user_defined_enum as enum

@unreal.uclass()
class CubelessEnumTools(unreal.ToolsetDefinition):
    """Create standard Engine UserDefinedEnum assets with guarded original inputs."""

    @toolset_registry.tool_call
    @staticmethod
    def create_user_defined_enum(asset_path: str, display_names: str, tooltips: str = "null",
            dry_run: bool = True, save: bool = False, skip_if_exists: bool = False) -> CubelessPythonOperationResult:
        """Create a standard UserDefinedEnum from asset_path and ordered display_names strings1..32. The owning public adapter exposes display_names as an array and tooltips as an optional string array with null default. These FString JSON carriers are private reflection ABI. Supplied tooltips must match the display-name count; null is preserved separately from an empty array. Exact booleans retain dry_run=True/save=False/skip_if_exists=False. Native owns path/scope/name validation, Engine-only asset creation, enum display/tooltip metadata, existing readback and optional package save. Dry run and successful readback do not prove all SDK effects are known. Actual created/partial/save/unknown prefix reaches one Named journal before errors. No extra Python creator/compiler/save, rollback, retry, runtime Cubeless dependency or complete pipeline claim."""
        rejected, report, raw = _enum_guard_admission()
        if rejected is not None:
            return reflected_result(rejected)
        try:
            arrays = _decode_enum_carriers(display_names, tooltips)
        except Exception as carrier_error:
            result = enum._rejected(enum.OperationError("enum_carrier_invalid", str(carrier_error)))
            result["stage"] = "enum_registration_carrier_admission"
            result["data"].update(registration_guard=report, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(enum.create_user_defined_enum(asset_path, arrays["display_names"], arrays["tooltips"],
            dry_run, save, skip_if_exists))


def _enum_guard_admission():
    """Only this exact current registered wrapper delegation is admitted."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessEnumAttachmentLibrary", None), "verify_enum_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact enum raw-input admission helper unavailable")
        raw = helper(CubelessEnumTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_enum_toolset"
                and report.get("delegated_call_admitted") is True
                and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "create_user_defined_enum"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded enum delegation required; no Native enum operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = enum._rejected(enum.OperationError("enum_raw_guard_admission_missing", message))
    result["stage"] = "enum_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _decode_enum_carriers(display_names, tooltips):
    """Decode private array2 ABI without trim/default substitution or None-to-empty."""
    decoded = {}
    for field, text in (("display_names", display_names), ("tooltips", tooltips)):
        if type(text) is not str or len(text) > 1048576:
            raise ValueError(field + " private JSON carrier requires at most1Mi characters")
        value = json.loads(text)
        if field == "tooltips" and value is None:
            decoded[field] = None
            continue
        if type(value) is not list or len(value) > 32 or any(type(item) is not str for item in value):
            raise ValueError(field + " must decode as a bounded array of exact strings")
        if field == "display_names" and not value:
            raise ValueError("display_names requires at least one string")
        decoded[field] = value
    if decoded["tooltips"] is not None and len(decoded["tooltips"]) != len(decoded["display_names"]):
        raise ValueError("Provided tooltips must match display_names length; null and [] differ")
    return decoded
