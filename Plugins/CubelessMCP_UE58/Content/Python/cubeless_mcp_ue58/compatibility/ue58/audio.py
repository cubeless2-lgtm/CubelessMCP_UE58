"""Finite UE58 Audio EngineAPI; no ServerRPC, scans or speculative builder."""

import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessAudioLibrary"
BINDINGS = ("inspect_audio_asset", "create_sound_cue_from_wave", "create_basic_metasound_source")
BINDING_LIBRARIES = {"inspect_audio_asset": LIBRARY, "create_sound_cue_from_wave": LIBRARY,
    "create_basic_metasound_source": "CubelessMetaSoundLibrary"}
AUDIO_CLASS_PATHS = ("/Script/Engine.SoundWave", "/Script/Engine.SoundCue",
    "/Script/MetasoundEngine.MetaSoundSource")
_INVALID_PACKAGE_CHARS = "\\:*?\"<>|' ,.&!~\n\r\t@#"


def binding_status():
    helper = getattr(unreal, "AssetRegistryHelpers", None)
    strings = getattr(unreal, "StringLibrary", None)
    return {"asset_registry_getter_callable": callable(getattr(helper, "get_asset_registry", None)),
        "asset_data_full_name_callable": callable(getattr(helper, "get_full_name", None)),
        "ar_filter_callable": callable(getattr(unreal, "ARFilter", None)),
        "top_level_asset_path_constructor_callable": callable(getattr(unreal, "TopLevelAssetPath", None)),
        "string_trim_callable": callable(getattr(strings, "trim", None)),
        "string_trim_trailing_callable": callable(getattr(strings, "trim_trailing", None)),
        **{name: callable(getattr(getattr(unreal, BINDING_LIBRARIES[name], None), name, None)) for name in BINDINGS}}


def _directory(value, observation):
    # Same original normalization. The counters are public Python call attempts
    # and returns, not an assertion that pre-entry wrapping/CDO work is absent.
    strings = getattr(unreal, "StringLibrary", None)
    trim = getattr(strings, "trim", None)
    trailing = getattr(strings, "trim_trailing", None)
    if not callable(trim) or not callable(trailing):
        raise OperationError("audio_asset_registry_unavailable", "Public FString trimming is unavailable; no Registry query or fallback ran")
    observation.update(engine_api_dispatch_attempted=True, string_normalization_dispatched=True,
        string_normalization_call_attempts=1, engine_read_stage="normalize_trim_start")
    trimmed = trim(value)
    observation["string_normalization_calls_returned"] = 1
    observation.update(string_normalization_call_attempts=2, engine_read_stage="normalize_trim_end")
    path = trailing(trimmed)
    observation.update(string_normalization_calls_returned=2, string_normalization_returned=True)
    if path.startswith('"'):
        path = path[1:]
    if path.endswith('"'):
        path = path[:-1]
    path = path.replace("\\", "/")
    while len(path) > 1 and path.endswith("/"):
        path = path[:-1]
    observation.update(directory=path, engine_read_stage="directory_syntax_admission")
    mount_path = path.lower()
    if (len(path.encode("utf-16-le")) // 2 >= 1024
            or not (mount_path in {"/game", "/engine"} or mount_path.startswith(("/game/", "/engine/")))
            or "//" in path or any(char in path for char in _INVALID_PACKAGE_CHARS)):
        raise OperationError("audio_invalid_directory", "directory must be a valid /Game or /Engine long package path; no Registry asset query ran")
    return path


def _object_path_sort_key(path):
    # FString::operator< -> FGenericPlatformStricmp: fold ASCII A..Z only,
    # compare unsigned Windows TCHAR units. Python Unicode lower/casefold would
    # reorder non-ASCII names and supplementary characters differently.
    raw = path.encode("utf-16-le")
    units = [raw[index] | raw[index + 1] << 8 for index in range(0, len(raw), 2)]
    return tuple(unit + 32 if 65 <= unit <= 90 else unit for unit in units)


def list_audio_assets(directory, recursive, max_results, offset):
    observation = {"engine_api_dispatch_attempted": False,
        "string_normalization_dispatched": False, "string_normalization_returned": False,
        "string_normalization_call_attempts": 0, "string_normalization_calls_returned": 0,
        "registry_getter_dispatched": False, "registry_getter_returned": False,
        "registry_getter_call_attempts": 0, "registry_getter_calls_returned": 0,
        "filter_creation_dispatched": False, "filter_creation_returned": False,
        "registry_asset_query_dispatched": False, "registry_asset_query_returned": False,
        "registry_asset_query_call_attempts": 0, "registry_asset_query_calls_returned": 0,
        "engine_read_stage": "binding_admission",
        "dispatch_count_scope": "public_python_call_attempts_and_returns_only_not_native_preentry_or_implicit_work",
        "inventory_complete": False}
    items = []
    try:
        helper = getattr(unreal, "AssetRegistryHelpers", None)
        getter = getattr(helper, "get_asset_registry", None)
        full_name = getattr(helper, "get_full_name", None)
        filter_type = getattr(unreal, "ARFilter", None)
        make_path = getattr(unreal, "TopLevelAssetPath", None)
        if not all(callable(function) for function in (getter, full_name, filter_type, make_path)):
            raise OperationError("audio_asset_registry_unavailable", "Required public Registry/filter metadata bindings are unavailable; no query, loader or fallback ran")
        normalized = _directory(directory, observation)
        observation.update(engine_api_dispatch_attempted=True, registry_getter_dispatched=True,
            registry_getter_call_attempts=1, engine_read_stage="registry_getter")
        registry = getter()
        observation.update(registry_getter_returned=True, registry_getter_calls_returned=1)
        query = getattr(registry, "get_assets", None)
        if not callable(query):
            raise OperationError("audio_asset_registry_unavailable", "Public AssetRegistry.get_assets is unavailable; normalization/getter calls may already have returned; no asset query or fallback ran")
        observation.update(filter_creation_dispatched=True, engine_read_stage="registry_filter_creation")
        # HasNativeMake moves KismetSystemLibrary.MakeTopLevelAssetPath into
        # the exported TopLevelAssetPath constructor. Keep its two native args.
        paths = [make_path(path, "") for path in AUDIO_CLASS_PATHS]
        asset_filter = filter_type(package_paths=[normalized], class_paths=paths,
            recursive_paths=recursive, recursive_classes=True, include_only_on_disk_assets=False)
        observation.update(filter_creation_returned=True, registry_asset_query_dispatched=True,
            registry_asset_query_call_attempts=1, engine_read_stage="registry_asset_query")
        # Same original filter/default platform filtering; no load or rescan.
        assets = query(asset_filter)
        observation.update(registry_asset_query_returned=True, registry_asset_query_calls_returned=1,
            engine_read_stage="metadata_projection")
        if assets is None:
            raise ValueError("Registry GetAssets returned no observable array")
        for asset in assets:
            class_path, separator, object_path = full_name(asset).partition(" ")
            if not separator or not class_path or not object_path:
                raise ValueError("AssetData.GetFullName did not return Class ObjectPath")
            items.append({"name": str(asset.asset_name), "asset_path": object_path,
                "class": class_path, "package_path": str(asset.package_path)})
        observation["engine_read_stage"] = "metadata_sort"
        items.sort(key=lambda item: _object_path_sort_key(item["asset_path"]))
    except Exception as error:
        failure = error if isinstance(error, OperationError) else OperationError("audio_list_assets_failed", str(error))
        failure.observation_prefix = {**observation, "assets_observed_prefix": items,
            "registry_query_returned": observation["registry_asset_query_returned"],
            "exception_type": type(error).__name__,
            "prefix_order_scope": "projection_prefix_or_partially_sorted_rows_not_a_complete_page"}
        if failure is error:
            raise
        raise failure from error
    page = items[offset:offset + max_results]
    next_offset = offset + len(page)
    payload = {**observation, "directory": normalized, "assets": page, "count": len(page),
        "total": len(items), "offset": offset, "has_more": next_offset < len(items),
        "inventory_complete": True, "engine_read_stage": "complete",
        "registry_recursive": recursive, "registry_recursive_classes": True,
        "registry_include_only_on_disk_assets": False, "registry_synchronous_scan_requested": False,
        "registry_query_returned": True, "explicit_asset_load_calls": 0,
        "explicit_compile_calls": 0, "explicit_save_calls": 0,
        "registry_implicit_metadata_effects_independently_observed": False,
        "library_cdo_before_native_entry_independently_observed": False}
    if payload["has_more"]:
        payload["next_offset"] = next_offset
    return payload


def _invoke(name, *arguments):
    function = getattr(getattr(unreal, BINDING_LIBRARIES.get(name, ""), None), name, None)
    if name not in BINDINGS or not callable(function):
        raise OperationError("native_audio_api_unavailable", "Build/load the exact Audio helper binding; no fallback, Python asset load or writer ran")
    raw = function(*arguments)
    try:
        return _decode_result(raw)
    except Exception as error:
        # Preserve the returned document even if decoding/typed observation
        # fails. It is not evidence that the already dispatched action failed
        # before any Engine work.
        error.native_raw_result = raw
        error.native_call_returned = True
        raise


def inspect_audio_asset(asset_path):
    return _invoke("inspect_audio_asset", asset_path)


def create_sound_cue_from_wave(name, sound_wave_path, package_path):
    return _invoke("create_sound_cue_from_wave", name, sound_wave_path, package_path)


def create_basic_metasound_source(name, package_path, oscillator, frequency, output_format):
    return _invoke("create_basic_metasound_source", name, package_path, oscillator, frequency, output_format)
