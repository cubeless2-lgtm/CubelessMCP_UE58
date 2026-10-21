"""Six named UE58 DataTable EngineAPI bindings; no transport or loader."""
import unreal

from .functions import _decode_result
from cubeless_mcp_ue58.operations.state import OperationError


LIBRARY = "CubelessDataTableLibrary"
BINDINGS = ("get_datatable_schema", "list_datatable_rows", "get_datatable_row",
    "add_or_update_datatable_row", "remove_datatable_row", "create_datatable")
PREPARATION = "prepare_datatable_toolset"


def binding_status():
    library = getattr(unreal, LIBRARY, None)
    return {name: callable(getattr(library, name, None)) for name in BINDINGS}


def _invoke(name, *arguments):
    if name not in BINDINGS:
        raise OperationError("datatable_operation_invalid", "Only the six original DataTable methods are admitted")
    function = getattr(getattr(unreal, LIBRARY, None), name, None)
    if not callable(function):
        raise OperationError("native_datatable_api_unavailable", "Build/load the exact DataTable helper; no load, writer or fallback ran")
    try:
        raw = function(*arguments)
    finally:
        function = None
    try:
        return _decode_result(raw)
    except Exception as error:
        error.native_raw_result = raw
        error.native_call_returned = True
        raise


def get_datatable_schema(datatable_path):
    return _invoke("get_datatable_schema", datatable_path)


def list_datatable_rows(datatable_path, row_name_contains, max_rows, include_values):
    return _invoke("list_datatable_rows", datatable_path, row_name_contains, max_rows, include_values)


def get_datatable_row(datatable_path, row_name):
    return _invoke("get_datatable_row", datatable_path, row_name)


def add_or_update_datatable_row(datatable_path, row_name, row_values_json, save):
    return _invoke("add_or_update_datatable_row", datatable_path, row_name, row_values_json, save)


def remove_datatable_row(datatable_path, row_name, save):
    return _invoke("remove_datatable_row", datatable_path, row_name, save)


def create_datatable(asset_path, row_struct_path, save):
    return _invoke("create_datatable", asset_path, row_struct_path, save)
