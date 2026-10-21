"""UE58 audit14 binding; nullable JSON carriers are private Native ABI only."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessContentAuditLibrary"
METHOD = "audit_content_root_mcp"
BINDINGS = (METHOD,)
NULLABLE_FIELDS = ("forbidden_dependency_prefixes", "required_asset_paths", "expected_asset_count", "expected_class_counts")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def _nullable_carrier(value):
    # None, [], {}, and 0 retain their different meanings. Public schemas do
    # not expose these strings. No normalization or default substitution here.
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))


def audit_content_root_mcp(root_path, recursive=True, scan_paths=True,
        include_dependencies=True, include_assets=False, write_report=False,
        forbidden_dependency_prefixes=None, required_asset_paths=None,
        expected_asset_count=None, expected_class_counts=None, max_samples=50,
        fail_on_dirty_packages=False, fail_on_redirectors=True,
        fail_on_forbidden_dependencies=True):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("content_audit_native_api_unavailable", "UE58 Native content audit binding is unavailable")
    raw = fn(root_path, recursive, scan_paths, include_dependencies, include_assets,
        write_report, _nullable_carrier(forbidden_dependency_prefixes),
        _nullable_carrier(required_asset_paths), _nullable_carrier(expected_asset_count),
        _nullable_carrier(expected_class_counts), max_samples, fail_on_dirty_packages,
        fail_on_redirectors, fail_on_forbidden_dependencies)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("content_audit_native_result_invalid", "Native audit operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("content_audit_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
