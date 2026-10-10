"""Targeted inherited-class guard, not a substitute for dependency/cook tests."""

import json
import unreal

from cubeless_mcp_ue58.operations.state import OperationError

_TOOL_MODULE_PREFIXES = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
                         "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry",
                         "pythonscriptplugin", "editorscriptingutilities")
_EDITOR_MODULES = {"blutility", "unrealed", "blueprinteditorlibrary"}
_TOOL_MOUNTS = {"unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
                "cubelessmcp", "cubeless_mcp", "cubelessmcp_ue58", "modelcontextprotocol",
                "toolsetregistry", "pythonscriptplugin", "editorscriptingutilities"}


def reject_tool_reference(value):
    candidate = value.replace("\\", "/").lower()
    segments = candidate.split("/")
    mount = segments[1] if len(segments) > 1 else ""
    module = segments[2].split(".", 1)[0].split(":", 1)[0] if mount == "script" and len(segments) > 2 else ""
    forbidden = (mount in _TOOL_MOUNTS or module in _EDITOR_MODULES
        or bool(module and module.startswith(_TOOL_MODULE_PREFIXES))
        or candidate == "/engine/pythontypes" or candidate.startswith(("/engine/pythontypes/", "/engine/pythontypes.", "/engine/pythontypes:")))
    if forbidden:
        raise OperationError("editor_tool_dependency", "Refusing to persist an MCP/editor-tool class dependency")


def parent_chain(parent):
    if not parent:
        return []
    native = getattr(unreal, "CubelessNativeToolset", None)
    if native is None or not hasattr(native, "inspect_blueprint_parent"):
        raise OperationError("native_parent_guard_unavailable", "Build/load the native Blueprint parent inspector before authoring")
    result = json.loads(native.inspect_blueprint_parent(parent.get_path_name()))
    if not result.get("success"):
        raise OperationError("unsafe_blueprint_parent", result.get("reason") or result.get("error") or "Native parent-chain validation failed")
    paths = result.get("parent_chain", [])
    if not paths:
        raise OperationError("invalid_parent_chain", "Native parent inspector returned no superclass chain")
    for path in paths:
        reject_tool_reference(path)
    return paths
