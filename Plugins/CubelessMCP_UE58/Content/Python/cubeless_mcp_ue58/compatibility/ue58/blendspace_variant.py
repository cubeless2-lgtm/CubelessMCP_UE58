"""UE58 specific BlendSpace variant operation; engine work stays in Native."""
import json
import unreal

from .functions import _decode_result
from ...operations.state import OperationError

LIBRARY = "CubelessBlendSpaceVariantLibrary"
METHOD = "ensure_blendspace_sample_variant"
BINDINGS = (METHOD,)
MAX_CARRIER_UTF16_UNITS = 1048576


def binding_status():
    return {METHOD: callable(getattr(getattr(unreal, LIBRARY, None), METHOD, None))}


def _carrier(value):
    try:
        encoded = json.dumps(value, ensure_ascii=False, allow_nan=False, separators=(",", ":"))
        units = len(encoded.encode("utf-16-le")) // 2
    except (TypeError, ValueError, UnicodeError, OverflowError, RecursionError) as error:
        raise OperationError("blendspace_variant_carrier_invalid", "BlendSpace edits require finite valid Unicode JSON") from error
    if units > MAX_CARRIER_UTF16_UNITS or "\x00" in encoded:
        raise OperationError("blendspace_variant_carrier_bound", "Private edit carrier exceeds 1 Mi UTF16 units or contains raw NUL")
    return encoded


def ensure_blendspace_sample_variant(source_blendspace, variant_name="Variant",
        target_root="/Game/_MCP_Sample/AnimStudy", target_blendspace="", axis_edits=None,
        sample_edits=None, add_samples=None, overwrite_existing=False, save=True,
        dry_run=False, validate_only=False, allow_non_sample=False, force_resample=False):
    fn = getattr(getattr(unreal, LIBRARY, None), METHOD, None)
    if not callable(fn):
        raise OperationError("blendspace_variant_native_api_unavailable", "UE58 BlendSpace variant binding is unavailable; no fallback executed")
    axis_json = _carrier(axis_edits)
    sample_json = _carrier(sample_edits)
    add_json = _carrier(add_samples)
    raw = fn(source_blendspace, variant_name, target_root, target_blendspace,
        axis_json, sample_json, add_json, overwrite_existing, save, dry_run,
        validate_only, allow_non_sample, force_resample)
    try:
        native = _decode_result(raw)
        if native.get("operation") != METHOD or "changed" not in native:
            raise OperationError("blendspace_variant_native_result_invalid", "Native variant operation or change receipt differs")
        if native["changed"] is not None and type(native["changed"]) is not bool:
            raise OperationError("blendspace_variant_native_result_invalid", "Native changed must be boolean or null")
    except Exception as error:
        error.native_call_returned = True
        error.native_raw_result = raw
        raise
    native["_raw_native_json"] = raw
    return native
