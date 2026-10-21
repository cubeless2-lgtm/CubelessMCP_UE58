"""Original project Input mapping public3 on the official Toolset Registry."""
import unreal
import toolset_registry
from .results import CubelessPythonOperationResult, reflected_result
from cubeless_mcp_ue58.operations import input_mapping
from cubeless_mcp_ue58.operations import content_audit
from cubeless_mcp_ue58.operations import asset_reference
import json

@unreal.uclass()
class CubelessProjectTools(unreal.ToolsetDefinition):
    """Project InputSettings authoring. Does not create Enhanced Input assets or verify runtime bindings."""

    @toolset_registry.tool_call
    @staticmethod
    def create_input_mapping(action_name: str, key: str, input_type: str = "Action") -> CubelessPythonOperationResult:
        """Append and save one legacy Action or Axis mapping using the original public inputs. input_type accepts case-insensitive exact Action/Axis; Axis uses SDK default scale1. The original Axis implementation defect is corrected. Native uses the exported mutable append overload without sorting, deduplication or keymap rebuild, then invokes the public default-config section updater once and independently observes the authored DefaultInput file, generated Input branch and fresh mapping arrays. The updater bool only reports admission such as read-only eligibility: it is not disk-write proof. Its internal temporary SaveConfig, file write, flush and reload are separate SDK work. The original unchecked generated SaveConfig could omit InputSettings; this route repairs that persistence defect. Actual RAM/cache/file and save prefixes remain in failure results and journals, with uncertain outcomes stopping Batch follow-on work. Reissuing an accepted request can append duplicate RAM mappings; the SDK default-layer array syntax can deduplicate disk entries, and any full-array mismatch is reported as saved=False with the actual partial prefix. No automatic retry, atomic rollback, asset compile/save or runtime binding proof. Legacy hidden modifiers are available only in the specific Batch command contract."""
        return reflected_result(input_mapping.create_input_mapping(action_name, key, input_type))

    @toolset_registry.tool_call
    @staticmethod
    def audit_content_root_mcp(root_path: str, recursive: bool = True, scan_paths: bool = True,
            include_dependencies: bool = True, include_assets: bool = False, write_report: bool = False,
            forbidden_dependency_prefixes: str = "null", required_asset_paths: str = "null",
            expected_asset_count: str = "null", expected_class_counts: str = "null",
            max_samples: int = 50, fail_on_dirty_packages: bool = False, fail_on_redirectors: bool = True,
            fail_on_forbidden_dependencies: bool = True) -> CubelessPythonOperationResult:
        """Registered facade for the original content audit14. The owning native public Toolset adapter presents the original nullable arrays/integer/class-map and None defaults; these four reflected FString inputs are hidden literal JSON carriers, not public JSON-text arguments. Exact in-flight delegated admission is required before decoding or dispatch. Native optionally force-scans one valid /Game child root and checks sorted metadata, direct Package dependencies, redirectors, dirty packages, required assets and supplied counts. Completed validation_pass=False is a normal successful diagnosis. Native operation/incomplete coverage/report I/O failures retain actual partial prefix. None forbidden prefixes means /Game/_MCP_Temp; [] means no prefixes. Optional GUID reports use project cubeless/records/verification/mcp-audits with actual writer/size/readback prefix; save flags describe report files, not Unreal assets. Registry/tag SDK effects can remain unknown and stop later Batch work. No wrapper asset load, compile/save, report writer, automatic retry or atomic rollback. Actual logical schema, hidden reflected schema and Named packing are separately verified; upper pipelines remain pending."""
        admission_rejection, admission_report, admission_raw = _audit_guard_admission()
        if admission_rejection is not None:
            return reflected_result(admission_rejection)
        try:
            decoded = _decode_audit_carriers(forbidden_dependency_prefixes, required_asset_paths,
                expected_asset_count, expected_class_counts)
        except Exception as carrier_error:
            result = content_audit._rejected(content_audit.OperationError(
                "content_audit_nullable_carrier_invalid", str(carrier_error)))
            result["stage"] = "content_audit_registration_carrier_admission"
            result["data"].update(registration_guard=admission_report, registration_guard_raw=admission_raw,
                native_dispatched=False, python_dispatched=True, journal_created=False,
                mutation_observation_scope="audit_domain_not_dispatched; helper_registry_receipt_is_separate",
                helper_receipt_is_whole_SDK_change_proof=False)
            return reflected_result(result)
        return reflected_result(content_audit.audit_content_root_mcp(root_path, recursive, scan_paths,
            include_dependencies, include_assets, write_report, decoded["forbidden_dependency_prefixes"],
            decoded["required_asset_paths"], decoded["expected_asset_count"], decoded["expected_class_counts"],
            max_samples, fail_on_dirty_packages, fail_on_redirectors, fail_on_forbidden_dependencies))

    @toolset_registry.tool_call
    @staticmethod
    def replace_asset_references_mcp(replacements: str = "null", target_root: str = "",
            target_paths: str = "null", source_root: str = "", string_replacements: str = "null",
            name_replacements: str = "null", dry_run: bool = True, save_assets: bool = False,
            scan_paths: bool = True, refresh_materials: bool = True,
            include_package_objects: bool = False, allow_non_game_targets: bool = False) -> CubelessPythonOperationResult:
        """Replace original asset references using typed nullable maps3/target array1, roots2 and booleans6. The canonical native Project Toolset presents these twelve logical inputs; four FString JSON carriers are private reflection ABI. Exact current reference-method delegation is required before dispatch. Native owns scope resolution, Blueprint generated/skeleton/CDO/SCS/graph remaps, archive/property soft/string/name remaps, optional material refresh and package saves. dry_run=True skips explicit target edits but is not total SDK load/registry purity or an exact write-count prediction. Default save_assets=False and caller False/null/empty values remain distinct. Actual partial resolution/remap/compile/save and unknown callback outcomes remain in results and journals; uncertain outcomes stop subsequent Batch actions. Game assets cannot acquire Cubeless/MCP Editor dependencies. No extra Python compiler/save, shader wait, automatic retry, atomic rollback or full pipeline completion claim."""
        rejection, admission, raw = _reference_guard_admission()
        if rejection is not None:
            return reflected_result(rejection)
        try:
            decoded = _decode_reference_carriers(replacements, target_paths, string_replacements, name_replacements)
        except Exception as carrier_error:
            result = asset_reference._rejected(asset_reference.OperationError(
                "asset_reference_nullable_carrier_invalid", str(carrier_error)))
            result["stage"] = "asset_reference_registration_carrier_admission"
            result["data"].update(registration_guard=admission, registration_guard_raw=raw,
                native_dispatched=False, python_dispatched=True, journal_created=False)
            return reflected_result(result)
        return reflected_result(asset_reference.replace_asset_references_mcp(decoded["replacements"], target_root,
            decoded["target_paths"], source_root, decoded["string_replacements"], decoded["name_replacements"],
            dry_run, save_assets, scan_paths, refresh_materials, include_package_objects, allow_non_game_targets))


def _audit_guard_admission():
    """Fail closed outside the owning adapter's current audit delegation."""
    report = {}; raw = None
    try:
        library = getattr(unreal, "CubelessContentAuditLibrary", None)
        helper = getattr(library, "verify_content_audit_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact content audit raw-input admission helper unavailable")
        raw = helper(CubelessProjectTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_content_audit_toolset"
                and report.get("delegated_call_admitted") is True
                and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "audit_content_root_mcp"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded audit delegation required; no Native audit operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = content_audit._rejected(content_audit.OperationError(
        "content_audit_raw_guard_admission_missing", message))
    result["stage"] = "content_audit_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        mutation_observation_scope="audit_domain_not_dispatched; helper_registry_receipt_is_separate",
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _decode_audit_carriers(forbidden_dependency_prefixes, required_asset_paths,
        expected_asset_count, expected_class_counts):
    """Registration-only private carrier decoding; typed domain remains unchanged."""
    carriers = dict(forbidden_dependency_prefixes=forbidden_dependency_prefixes,
        required_asset_paths=required_asset_paths, expected_asset_count=expected_asset_count,
        expected_class_counts=expected_class_counts)
    decoded = {}
    for field, value in carriers.items():
        if type(value) is not str or len(value) > 1048576:
            raise ValueError(field + " private JSON carrier must be a string of at most 1 Mi characters")
        decoded[field] = json.loads(value)
    for field in ("forbidden_dependency_prefixes", "required_asset_paths"):
        value = decoded[field]
        if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
            raise ValueError(field + " must decode as a string array or null")
    count = decoded["expected_asset_count"]
    if count is not None and type(count) is not int:
        raise ValueError("expected_asset_count must decode as an integer or null")
    classes = decoded["expected_class_counts"]
    if classes is not None and (type(classes) is not dict
            or any(type(key) is not str or type(value) is not int for key, value in classes.items())):
        raise ValueError("expected_class_counts must decode as a string-to-integer object or null")
    return decoded


def _reference_guard_admission():
    """Fail closed outside this method's current owned guarded delegation."""
    report = {}; raw = None
    try:
        helper = getattr(getattr(unreal, "CubelessContentAuditLibrary", None), "verify_content_audit_toolset", None)
        if not callable(helper):
            raise RuntimeError("Exact Project raw-input admission helper unavailable")
        raw = helper(CubelessProjectTools)
        report = json.loads(raw)
        if (type(report) is dict and report.get("operation") == "verify_content_audit_toolset"
                and report.get("delegated_call_admitted") is True
                and report.get("delegated_admission_only") is True
                and report.get("delegated_method") == "replace_asset_references_mcp"
                and report.get("asset_mutation") is False and report.get("metadata_mutation") is False):
            return None, report, raw
        message = "Canonical guarded reference delegation required; no Native reference operation dispatched"
    except Exception as admission_error:
        message = str(admission_error)
    result = asset_reference._rejected(asset_reference.OperationError("asset_reference_raw_guard_admission_missing", message))
    result["stage"] = "asset_reference_registration_admission"
    result["data"].update(registration_guard=report, registration_guard_raw=raw,
        native_dispatched=False, python_dispatched=True, journal_created=False,
        helper_receipt_is_whole_SDK_change_proof=False)
    return result, report, raw


def _decode_reference_carriers(replacements, target_paths, string_replacements, name_replacements):
    """Decode only the private reflection ABI; retain logical None/empty values."""
    carriers = dict(replacements=replacements, target_paths=target_paths,
        string_replacements=string_replacements, name_replacements=name_replacements)
    decoded = {}
    for field, value in carriers.items():
        if type(value) is not str or len(value) > 1048576:
            raise ValueError(field + " private JSON carrier must be a string of at most 1 Mi characters")
        decoded[field] = json.loads(value)
    for field in ("replacements", "string_replacements", "name_replacements"):
        value = decoded[field]
        if value is not None and (type(value) is not dict
                or any(type(key) is not str or type(item) is not str for key, item in value.items())):
            raise ValueError(field + " must decode as a string-to-string object or null")
    value = decoded["target_paths"]
    if value is not None and (type(value) is not list or any(type(item) is not str for item in value)):
        raise ValueError("target_paths must decode as a string array or null")
    return decoded
