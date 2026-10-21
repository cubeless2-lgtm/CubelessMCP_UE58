"""UE58 exact MPC mutation bindings; no loading, registry or server calls."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessMaterialCollectionMutationLibrary"
BINDINGS = ("set_material_parameter_collection_values", "mirror_material_parameter_collection")


def _invoke(method, *arguments):
    if method not in BINDINGS:
        raise OperationError("native_material_collection_api_unavailable", "Only the two named MPC mutation leaves are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), method, None)
    if not callable(function):
        raise OperationError("native_material_collection_api_unavailable",
            "Build/load CubelessMaterialCollectionMutationLibrary; no fallback or asset load executed")
    return _decode_result(function(*arguments))


def set_material_parameter_collection_values(collection_path, scalars_json,
        vectors_json, set_asset_defaults, set_runtime, save):
    return _invoke(BINDINGS[0], collection_path, scalars_json, vectors_json,
        set_asset_defaults, set_runtime, save)


def mirror_material_parameter_collection(source_collection_path,
        target_collection_path, parameter_names_json, copy_defaults, preserve_ids, save):
    return _invoke(BINDINGS[1], source_collection_path, target_collection_path,
        parameter_names_json, copy_defaults, preserve_ids, save)
