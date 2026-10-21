"""UE5.8 named function-graph bindings; no asset loading in the adapter."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError


def _invoke(method, *arguments):
    function = getattr(getattr(unreal, "CubelessFunctionLibrary", None), method, None)
    if function is None:
        raise OperationError("native_function_api_unavailable", "Build/load CubelessFunctionLibrary; no fallback executed")
    # LY lifetime-only selector. Native admission remains authoritative; no Engine
    # lookup is performed here to classify a Blueprint or resolve a graph GUID.
    finite = (
        method == "resolve_blueprint_graph" and len(arguments) == 5
        and str(arguments[1]).casefold() == "camerafacing"
        or method == "add_blueprint_function_parameter" and len(arguments) == 11
        and str(arguments[1]).casefold() in ("inmesh", "uselegacyfacing", "locky", "tracemeshinitrot")
        and str(arguments[7]).casefold() == "camerafacing"
        or method == "add_blueprint_return_node" and len(arguments) == 6
        and str(arguments[2]).casefold() == "camerafacing"
        or method == "add_blueprint_call_function_node" and len(arguments) == 14
        and str(arguments[1]).startswith("/Game/") and str(arguments[1]).endswith("_C")
        and arguments[2] == "CameraFacing"
    )
    # LZ adds only the finite CameraFacing body lifetime route. C++ still owns
    # Blueprint/type/signature/default/current admission and reports every outcome.
    body_calls = {
        "/Script/Engine.Actor": ("K2_GetActorLocation",),
        "/Script/Engine.GameplayStatics": ("GetPlayerCameraManager",),
        "/Script/Engine.PlayerCameraManager": ("GetCameraRotation",),
        "/Script/Engine.SceneComponent": ("K2_GetComponentLocation", "K2_SetWorldRotation"),
        "/Script/Engine.KismetMathLibrary": ("Add_DoubleDouble", "BreakRotator", "ComposeRotators", "FindLookAtRotation", "MakeRotator", "SelectRotator"),
    }
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and str(arguments[5]).casefold() == "camerafacing"
        and str(arguments[2]).casefold() in tuple(name.casefold() for name in body_calls.get(arguments[1], ()))
    )
    # MC only: release the bound native output writer before decode/raise. The
    # selector is lifetime-only; C++ independently validates type, graph and defaults.
    finite = finite or (
        method == "add_blueprint_function_parameter" and len(arguments) == 11
        and str(arguments[2]).casefold().replace(" ", "").replace("-", "_") == "map_string_double"
    )
    # MD lifetime selector only. The native exact PresetLoader profile validates
    # names, declaration/caller projections, defaults and current source roles.
    finite = finite or (
        method == "resolve_blueprint_graph" and len(arguments) == 5
        and arguments[1] == "PresetLoader"
        or method == "add_blueprint_function_parameter" and len(arguments) == 11
        and arguments[7] == "PresetLoader"
        or method == "add_blueprint_return_node" and len(arguments) == 6
        and arguments[2] == "PresetLoader"
        or method == "add_blueprint_call_function_node" and len(arguments) == 14
        and str(arguments[1]).startswith("/Game/") and str(arguments[1]).endswith("_C")
        and arguments[2] == "PresetLoader"
    )
    # ME lifetime-only route; native validates exact PresetLoader target, native
    # signature/enums/defaults and every current boundary independently.
    normalization_calls = {
        "/Script/Engine.KismetStringLibrary": ("Conv_StringToDouble", "Replace"),
        "/Script/Engine.KismetTextLibrary": ("Conv_DoubleToText", "Conv_TextToString"),
    }
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and arguments[5] == "PresetLoader"
        and arguments[2] in normalization_calls.get(arguments[1], ())
    )
    # MG lifetime-only route for the existing exact PresetLoader asset body4.
    asset_body_calls = {
        "/Script/AssetRegistry.AssetRegistryHelpers": ("GetAssetRegistry", "GetAsset"),
        "/Script/AssetRegistry.AssetRegistry": ("GetAssets",),
        "/Script/Engine.KismetSystemLibrary": ("MakeARFilter",),
    }
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and str(arguments[5]).casefold() == "presetloader"
        and str(arguments[2]).casefold() in tuple(name.casefold() for name in asset_body_calls.get(arguments[1], ()))
    )
    # MI lifetime-only selector for the exact PresetLoader name/row producer4.
    # C++ owns profile/signature/source/default/current admission independently.
    name_row_body_calls = {
        "/Script/Engine.KismetStringLibrary": ("Conv_NameToString", "Concat_StrStr", "Contains"),
        "/Script/Engine.DataTableFunctionLibrary": ("GetDataTableRowNames",),
    }
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and str(arguments[5]).casefold() == "presetloader"
        and str(arguments[2]).casefold() in tuple(name.casefold() for name in name_row_body_calls.get(arguments[1], ()))
    )
    # MQ lifetime-only selector; native owns exact PresetLoader admission.
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and arguments[1] == "/Script/Engine.KismetMathLibrary"
        and arguments[2] == "EqualEqual_IntInt"
        and str(arguments[5]).casefold() == "presetloader"
    )
    # MJ lifetime only: exact DataTable cast method, including graph_id-only
    # PresetLoader selection. Normal DataTable gains only callable RC release;
    # native alone owns target/profile/signature/current admission and DTO.
    finite = finite or (
        method == "add_blueprint_dynamic_cast_node" and len(arguments) == 8
        and arguments[1] == "/Script/Engine.DataTable"
    )
    # ND lifetime only, including graph_id-only calls. Native owns neutral Map7 admission.
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and arguments[1] == "/Script/Engine.BlueprintMapLibrary"
        and arguments[2] in ("Map_Clear", "Map_Find", "Map_Add", "Map_Contains", "Map_Remove", "Map_Keys", "Map_Values")
        and arguments[11] is True
    )
    # OW lifetime only: new referenced Array parameters retain the exact eleven
    # arguments and Native admission. Older scalar/primitive/math routes stay unchanged.
    finite = finite or (
        method == "add_blueprint_function_parameter" and len(arguments) == 11
        and arguments[6] is True
        and str(arguments[2]).lower().replace("_", "").replace("-", "").replace(" ", "") in ("enum", "byteenum", "object", "class")
    )
    # PL lifetime only: the new finite UDS scalar/Array interface, including
    # source-refusal receipts. Native owns admission and the same eleven arguments.
    finite = finite or (
        method == "add_blueprint_function_parameter" and len(arguments) == 11
        and str(arguments[2]).lower().replace("_", "").replace("-", "").replace(" ", "") in ("struct", "scriptstruct")
    )
    # PU lifetime only, including Native refusal receipts. The exact latent
    # function/graph profile and nonlatent reuse remain Native admission decisions.
    finite = finite or (
        method == "add_blueprint_call_function_node" and len(arguments) == 14
        and arguments[9] is True
    )
    # PY lifetime only: keep the original True request, including Native
    # admission failures; creation/reuse and effective options are Native-owned.
    finite = finite or (
        method == "add_blueprint_function_parameter" and len(arguments) == 11 and arguments[10] is True
        or method == "add_blueprint_call_function_node" and len(arguments) == 14 and arguments[8] is True
    )
    if finite:
        try:
            raw = function(*arguments)
        finally:
            # Drop the bound Unreal callable before JSON decode or exception
            # traceback/frame retention; no GC policy/type/private API changes.
            function = None
    else:
        try:
            raw = function(*arguments)
        finally:
            function = None
    return _decode_result(raw)


def _decode_result(raw):
    """Shared typed native mutation envelope; does not invoke an engine API."""
    try:
        value = json.loads(raw)
    except (TypeError, ValueError) as error:
        raise OperationError("invalid_native_function_result", "Function result is not valid JSON; inspect the asset and journal before another action") from error
    flags = ("success", "mutation_attempted", "change_state_known", "postcondition_passed",
             "compile_requested", "compile_attempted", "save_requested", "save_attempted", "saved")
    if (type(value) is not dict or type(value.get("data")) is not dict
            or any(type(value.get(key)) is not bool for key in flags)
            or "compiled" not in value or value["compiled"] is not None and type(value["compiled"]) is not bool
            or value["change_state_known"] and type(value.get("changed")) is not bool):
        raise OperationError("invalid_native_function_result", "Function result lacks typed outcomes; the dispatched result remains unknown")
    return value


def resolve_blueprint_graph(blueprint_path, graph_name, graph_id, graph_type, create_if_missing):
    return _invoke("resolve_blueprint_graph", blueprint_path, graph_name, graph_id, graph_type, create_if_missing)


def add_blueprint_function_parameter(blueprint_path, parameter_name, parameter_type, direction,
        default_value_json, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke("add_blueprint_function_parameter", blueprint_path, parameter_name, parameter_type,
        direction, default_value_json, type_object, is_array, graph_name, graph_id, graph_type, create_graph_if_missing)


def add_blueprint_return_node(blueprint_path, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke("add_blueprint_return_node", blueprint_path, node_position_json,
        graph_name, graph_id, graph_type, create_graph_if_missing)


def add_blueprint_dynamic_cast_node(blueprint_path, target_class, node_position_json, pure,
        graph_name, graph_id, graph_type, create_graph_if_missing):
    return _invoke("add_blueprint_dynamic_cast_node", blueprint_path, target_class,
        node_position_json, pure, graph_name, graph_id, graph_type, create_graph_if_missing)


def add_blueprint_call_function_node(blueprint_path, function_class, function_name, param_defaults_json,
        node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal):
    return _invoke("add_blueprint_call_function_node", blueprint_path, function_class, function_name,
        param_defaults_json, node_position_json, graph_name, graph_id, graph_type, create_graph_if_missing,
        allow_latent, allow_editor_only, allow_wildcard, allow_deprecated, allow_internal)
