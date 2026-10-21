"""UE58 reference replacement12; nullable4 JSON strings are private Native ABI."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessAssetReferenceLibrary"
METHOD = "replace_asset_references_mcp"
BINDINGS = (METHOD,)
NULLABLE_FIELDS = ("replacements", "target_paths", "string_replacements", "name_replacements")


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {METHOD: callable(getattr(library, METHOD, None))}


def _nullable_carrier(value):
    # Public inputs stay typed. None, {}, and [] are neither defaulted nor
    # normalized here; the Native handler owns the admitted scope semantics.
    return json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))


def replace_asset_references_mcp(replacements=None, target_root="", target_paths=None,
        source_root="", string_replacements=None, name_replacements=None, dry_run=True,
        save_assets=False, scan_paths=True, refresh_materials=True,
        include_package_objects=False, allow_non_game_targets=False):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("asset_reference_native_api_unavailable", "UE58 Native asset reference replacement binding is unavailable")
    raw = fn(_nullable_carrier(replacements), target_root, _nullable_carrier(target_paths),
        source_root, _nullable_carrier(string_replacements), _nullable_carrier(name_replacements),
        dry_run, save_assets, scan_paths, refresh_materials, include_package_objects, allow_non_game_targets)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("asset_reference_native_result_invalid", "Native reference operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("asset_reference_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
