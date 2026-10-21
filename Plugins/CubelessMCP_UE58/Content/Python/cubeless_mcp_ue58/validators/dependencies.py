"""Targeted inherited-class guard, not a substitute for dependency/cook tests."""

import json
import re
import unreal

from cubeless_mcp_ue58.operations.state import OperationError

_TOOL_MODULE_PREFIXES = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
                         "cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry",
                         "pythonscriptplugin", "editorscriptingutilities")
_EDITOR_MODULES = {"blutility", "unrealed", "blueprinteditorlibrary"}
_TOOL_MOUNTS = {"unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools",
                "cubelessmcp", "cubeless_mcp", "cubelessmcp_ue58", "modelcontextprotocol",
                "toolsetregistry", "pythonscriptplugin", "editorscriptingutilities"}


_LEGACY_COMMAND_TOKENS = ("unrealmcp", "mcpunreal", "mcp_unreal", "cubelesscontrolrigtools")
_MIGRATED_MOUNTS = {"cubelessmcp", "cubeless_mcp", "cubelessmcp_ue58",
                    "modelcontextprotocol", "toolsetregistry"}
_MIGRATED_MODULE_PREFIXES = ("cubelessmcp", "cubeless_mcp", "modelcontextprotocol", "toolsetregistry")


def _tool_reference(value, *, command=False):
    if not isinstance(value, str):
        return False
    candidate = value.strip().strip("\"'").replace("\\", "/").lower()
    # Preserve UE56 command rejection, including legacy tokens in ordinary text.
    if command and any(token in candidate for token in _LEGACY_COMMAND_TOKENS):
        return True
    mounts = _MIGRATED_MOUNTS if command else _TOOL_MOUNTS
    modules = _MIGRATED_MODULE_PREFIXES if command else _TOOL_MODULE_PREFIXES
    # Recognize absolute object/package paths, including quoted/export-text and
    # struct-value wrappers. Qualified paths in search text remain conservative.
    for match in re.finditer(r"(?:^|[\s\"'=,(\[{])(/[^\s\"'=,)\]}]+)", candidate):
        path = match.group(1)
        segments = path.split("/")
        mount = segments[1].split(".", 1)[0].split(":", 1)[0]
        module = segments[2].split(".", 1)[0].split(":", 1)[0] if mount == "script" and len(segments) > 2 else ""
        if (mount in mounts or bool(module and module.startswith(modules))
                or bool(not command and module in _EDITOR_MODULES)
                or path == "/engine/pythontypes"
                or path.startswith(("/engine/pythontypes/", "/engine/pythontypes.", "/engine/pythontypes:"))):
            return True
    return False


def has_legacy_command_tool_reference(value):
    """UE56 broad legacy tokens plus qualified references to migrated tools."""
    return _tool_reference(value, command=True)


def reject_tool_reference(value):
    candidate = value.replace("\\", "/")
    if _tool_reference(candidate):
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
