"""Compact tool discovery and guarded dispatch for the Unreal MCP catalog.

The server still registers every domain tool with FastMCP first.  Search mode
then keeps that validated manager as a private catalog and exposes only three
small meta-tools to MCP clients.  Eager mode leaves the legacy-compatible 429-tool surface
untouched and is available as an immediate compatibility fallback.
"""

# Keep annotations eagerly evaluated in this module. The pinned FastMCP
# version detects Context parameters by object identity while building a tool
# schema; postponed annotations would accidentally expose ``ctx`` to clients.
import difflib
import json
import logging
import os
from dataclasses import asdict, dataclass
from importlib.metadata import PackageNotFoundError, version
from types import MappingProxyType
from typing import Any, Dict, Mapping

from mcp.server.fastmcp import Context, FastMCP
from mcp.server.fastmcp.exceptions import ToolError
from mcp.server.fastmcp.tools.tool_manager import ToolManager
from pydantic import ValidationError

from tools.management_tools import MCPToolRegistry
from tools.response_schema import tool_error, tool_success
from tools.tool_policy import requires_dispatch_confirmation

logger = logging.getLogger("UnrealMCP")

TOOL_EXPOSURE_ENV = "UNREAL_MCP_TOOL_EXPOSURE"
TOOL_EXPOSURE_MODES = frozenset({"search", "eager"})
DEFAULT_TOOL_EXPOSURE_MODE = "search"
SEARCH_META_TOOLS = ("list_toolsets", "describe_toolset", "call_tool")
SUPPORTED_MCP_SDK_VERSION = "1.5.0"
TERSE_SUMMARY_MAX_CHARS = 120
ERROR_VALUE_SUMMARY_MAX_CHARS = 120

_SUMMARY_ABBREVIATIONS = frozenset(
    {
        "e.g.",
        "i.e.",
        "etc.",
        "vs.",
        "mr.",
        "mrs.",
        "ms.",
        "dr.",
        "u.s.",
    }
)
_SENSITIVE_PARAM_MARKERS = (
    "password",
    "secret",
    "token",
    "api_key",
    "apikey",
    "authorization",
)


def validate_mcp_sdk_version() -> str:
    """Fail clearly when private FastMCP contracts have not been reviewed."""

    try:
        installed_version = version("mcp")
    except PackageNotFoundError as exc:
        raise RuntimeError("The pinned mcp SDK distribution is not installed") from exc
    if installed_version != SUPPORTED_MCP_SDK_VERSION:
        raise RuntimeError(
            "Unsupported mcp SDK version "
            f"{installed_version}; expected {SUPPORTED_MCP_SDK_VERSION}. "
            "Tool exposure uses reviewed private FastMCP manager contracts."
        )
    return installed_version

_TOOLSET_DESCRIPTIONS = {
    "ai": "UE 5.6 AI, Behavior Tree, Blackboard, EQS, and perception authoring.",
    "audio": "UE 5.6 SoundCue, SoundWave, and MetaSound inspection and authoring.",
    "batch": "Single-round-trip, transactional bridge command batches.",
    "blueprint": "Blueprint assets, components, variables, functions, and compilation.",
    "blueprint_node": "Blueprint graph node creation, wiring, and graph inspection.",
    "chaos": "Chaos solver, Chaos Cloth, and destruction authoring, validation, and diagnostics.",
    "controlrig_authoring": "Control Rig and animation authoring workflows.",
    "data": "Data assets, tables, curves, and structured Unreal data.",
    "editor": "Actors, levels, viewport, selection, assets, and editor state.",
    "gas": "UE 5.6 Gameplay Ability System Blueprints, components, and attributes.",
    "ieta": "Repository-grounded Ieta analysis and orchestration helpers.",
    "landscape": "Landscape creation, inspection, materials, and foliage workflows.",
    "management": "Runtime MCP health, telemetry, and enablement controls.",
    "material": "Material graphs, instances, collections, and compilation.",
    "networking": "UE 5.6 Actor Blueprint replication defaults and readback.",
    "niagara": "Niagara systems, emitters, modules, parameters, and validation.",
    "niagara_planning": "Mutation-free Niagara planning, diagnosis, and refinement evidence.",
    "pcg": "PCG graphs, components, generation, inspection, and validation.",
    "project": "Project settings, input, gameplay framework, and asset helpers.",
    "python": "Generic Unreal Python execution bridge.",
    "sequencer": "Level Sequence tracks, bindings, keys, and playback data.",
    "skeletal": "Skeletal Mesh, Animation Blueprint, and animation asset workflows.",
    "state_tree": "UE 5.6 StateTree creation, hierarchy, compilation, and inspection.",
    "texture_generation": "Texture generation handoffs, import, material creation, and application.",
    "umg": "Widget Blueprint hierarchy, layout, styling, and events.",
}


def normalize_tool_exposure_mode(requested_mode: str | None = None) -> str:
    """Resolve and validate the public MCP tool exposure mode."""

    raw_mode = requested_mode
    if raw_mode is None:
        raw_mode = os.environ.get(TOOL_EXPOSURE_ENV, DEFAULT_TOOL_EXPOSURE_MODE)
    mode = raw_mode.strip().lower()
    if mode not in TOOL_EXPOSURE_MODES:
        supported = ", ".join(sorted(TOOL_EXPOSURE_MODES))
        raise ValueError(f"Invalid {TOOL_EXPOSURE_ENV}={raw_mode!r}; expected one of: {supported}")
    return mode


def _bounded_text(value: str, max_chars: int) -> str:
    """Bound diagnostic text with an ASCII ellipsis included in the limit."""

    if len(value) <= max_chars:
        return value
    return value[: max_chars - 3].rstrip() + "..."


def _terse_summary(description: str) -> str:
    """Return a cached one-line first-sentence summary."""

    normalized = " ".join(str(description or "").split()).strip()
    if not normalized:
        return ""

    sentence_end = len(normalized)
    for index, character in enumerate(normalized):
        if character not in ".!?":
            continue
        if index + 1 < len(normalized) and not normalized[index + 1].isspace():
            continue
        token_start = normalized.rfind(" ", 0, index) + 1
        token = normalized[token_start : index + 1].casefold().lstrip("([{\"'")
        if token in _SUMMARY_ABBREVIATIONS:
            continue
        sentence_end = index + 1
        break

    summary = normalized[:sentence_end].strip()
    if len(summary) <= TERSE_SUMMARY_MAX_CHARS:
        return summary

    content_limit = TERSE_SUMMARY_MAX_CHARS - 3
    word_boundary = summary.rfind(" ", 0, content_limit + 1)
    if word_boundary > 0:
        content_limit = word_boundary
    return summary[:content_limit].rstrip() + "..."


def _build_terse_summary_cache(
    registry: MCPToolRegistry,
    catalog_manager: ToolManager,
) -> Mapping[str, str]:
    """Parse catalog descriptions once while configuring tool exposure."""

    summaries: Dict[str, str] = {}
    for record in registry.report(include_tools=True).get("tools", []):
        name = str(record.get("name", ""))
        catalog_tool = catalog_manager.get_tool(name)
        description = (
            (catalog_tool.description if catalog_tool is not None else "")
            or record.get("description", "")
        )
        try:
            summaries[name] = _terse_summary(str(description or ""))
        except Exception:
            logger.debug("Could not cache terse summary for %s", name, exc_info=True)
            summaries[name] = ""
    return MappingProxyType(summaries)


def _format_validation_location(location: tuple[Any, ...]) -> str:
    """Format a Pydantic error location as a stable dotted/indexed path."""

    result = ""
    for part in location:
        if isinstance(part, int):
            result += f"[{part}]"
        elif result:
            result += f".{part}"
        else:
            result = str(part)
    return result or "<arguments>"


def _resolve_schema_ref(root_schema: Mapping[str, Any], schema: Mapping[str, Any]) -> Mapping[str, Any]:
    """Resolve a local JSON Schema reference when possible."""

    reference = schema.get("$ref")
    if not isinstance(reference, str) or not reference.startswith("#/"):
        return schema
    current: Any = root_schema
    for segment in reference[2:].split("/"):
        if not isinstance(current, Mapping) or segment not in current:
            return schema
        current = current[segment]
    return current if isinstance(current, Mapping) else schema


def _schema_for_location(
    root_schema: Mapping[str, Any],
    location: tuple[Any, ...],
) -> Mapping[str, Any]:
    """Find the JSON Schema node corresponding to a validation location."""

    current: Mapping[str, Any] = root_schema
    for part in location:
        current = _resolve_schema_ref(root_schema, current)
        if isinstance(part, int):
            items = current.get("items", {})
            current = items if isinstance(items, Mapping) else {}
            continue

        properties = current.get("properties", {})
        if isinstance(properties, Mapping) and part in properties:
            candidate = properties[part]
            current = candidate if isinstance(candidate, Mapping) else {}
            continue

        branches = current.get("anyOf", [])
        matching_branch: Mapping[str, Any] | None = None
        if isinstance(branches, list):
            for branch in branches:
                if not isinstance(branch, Mapping):
                    continue
                resolved = _resolve_schema_ref(root_schema, branch)
                branch_properties = resolved.get("properties", {})
                if isinstance(branch_properties, Mapping) and part in branch_properties:
                    matching_branch = resolved
                    break
        if matching_branch is None:
            return {}
        branch_properties = matching_branch.get("properties", {})
        candidate = branch_properties.get(part, {})
        current = candidate if isinstance(candidate, Mapping) else {}
    return _resolve_schema_ref(root_schema, current)


def _schema_type_summary(root_schema: Mapping[str, Any], schema: Mapping[str, Any]) -> str:
    """Render a compact expected-type label from a JSON Schema node."""

    schema = _resolve_schema_ref(root_schema, schema)
    schema_type = schema.get("type")
    if schema_type == "array":
        items = schema.get("items", {})
        item_type = (
            _schema_type_summary(root_schema, items)
            if isinstance(items, Mapping) and items
            else "value"
        )
        return f"array[{item_type}]"
    if isinstance(schema_type, str):
        return schema_type
    if isinstance(schema_type, list):
        return " | ".join(str(value) for value in schema_type)

    branches = schema.get("anyOf")
    if isinstance(branches, list):
        branch_types = []
        for branch in branches:
            if isinstance(branch, Mapping):
                label = _schema_type_summary(root_schema, branch)
                if label not in branch_types:
                    branch_types.append(label)
        if branch_types:
            return " | ".join(branch_types)

    reference = schema.get("$ref")
    if isinstance(reference, str):
        return reference.rsplit("/", 1)[-1]
    return "value matching tool schema"


def _value_at_location(arguments: Mapping[str, Any], location: tuple[Any, ...]) -> Any:
    """Read a nested argument value without mutating the caller's object."""

    current: Any = arguments
    for part in location:
        if isinstance(current, Mapping) and part in current:
            current = current[part]
        elif isinstance(part, int) and isinstance(current, (list, tuple)) and 0 <= part < len(current):
            current = current[part]
        else:
            return _MISSING_VALUE
    return current


def _json_type_name(value: Any) -> str:
    """Return a JSON-oriented type label for received argument data."""

    if value is None:
        return "null"
    if isinstance(value, bool):
        return "boolean"
    if isinstance(value, int):
        return "integer"
    if isinstance(value, float):
        return "number"
    if isinstance(value, str):
        return "string"
    if isinstance(value, (list, tuple)):
        return "array"
    if isinstance(value, Mapping):
        return "object"
    return type(value).__name__


def _summarize_received_value(value: Any, param_path: str) -> str:
    """Return a bounded, shallow summary without echoing whole payloads."""

    normalized_path = param_path.casefold()
    is_sensitive = any(marker in normalized_path for marker in _SENSITIVE_PARAM_MARKERS)
    if isinstance(value, str):
        if is_sensitive:
            summary = f"len={len(value)}, preview=<redacted>"
        else:
            preview = _bounded_text(value, 48)
            summary = f"len={len(value)}, preview={json.dumps(preview, ensure_ascii=False)}"
    elif isinstance(value, (list, tuple)):
        item_types = []
        for item in value[:5]:
            item_type = _json_type_name(item)
            if item_type not in item_types:
                item_types.append(item_type)
        summary = f"len={len(value)}, item_types={item_types}"
    elif isinstance(value, Mapping):
        keys = sorted(str(key) for key in value.keys())
        summary = f"keys={keys[:8]}, total_keys={len(keys)}"
    else:
        summary = repr(value)
    return _bounded_text(summary, ERROR_VALUE_SUMMARY_MAX_CHARS)


def _is_type_validation_error(error_type: str) -> bool:
    """Recognize Pydantic errors caused by incompatible received value types."""

    return error_type.endswith(("_type", "_parsing")) or error_type in {
        "bytes_invalid_encoding",
        "int_from_float",
        "json_invalid",
        "model_attributes_type",
    }


def _structured_validation_diagnostics(
    validation_error: ValidationError,
    arguments: Mapping[str, Any],
    input_schema: Mapping[str, Any],
) -> Dict[str, Any]:
    """Convert input-only validation failures into additive recovery fields."""

    missing_params: list[str] = []
    type_mismatches: list[Dict[str, Any]] = []
    for error in validation_error.errors(include_url=False, include_context=False):
        location = tuple(error.get("loc", ()))
        param_path = _format_validation_location(location)
        error_type = str(error.get("type", ""))
        if error_type == "missing":
            if param_path not in missing_params:
                missing_params.append(param_path)
            continue
        if not _is_type_validation_error(error_type):
            continue

        actual_value = _value_at_location(arguments, location)
        if actual_value is _MISSING_VALUE:
            actual_value = error.get("input")
        field_schema = _schema_for_location(input_schema, location)
        type_mismatches.append(
            {
                "param": param_path,
                "expected_type": _schema_type_summary(input_schema, field_schema),
                "actual_type": _json_type_name(actual_value),
                "actual_summary": _summarize_received_value(actual_value, param_path),
                "validation_code": error_type,
            }
        )

    diagnostics: Dict[str, Any] = {}
    messages: list[str] = []
    if missing_params:
        provided_keys = sorted(str(key) for key in arguments.keys())
        diagnostics["missing_params"] = missing_params
        diagnostics["provided_keys"] = provided_keys
        messages.append(
            "Missing required param(s): "
            f"[{', '.join(missing_params)}]. Provided keys: [{', '.join(provided_keys)}]"
        )
    if type_mismatches:
        diagnostics["type_mismatches"] = type_mismatches
        mismatch_messages = [
            f"{item['param']} expected {item['expected_type']}, received "
            f"{item['actual_type']} ({item['actual_summary']})"
            for item in type_mismatches
        ]
        messages.append("Type mismatch: " + "; ".join(mismatch_messages))
    if messages:
        diagnostics["validation_message"] = " ".join(messages)
    return diagnostics


_MISSING_VALUE = object()


@dataclass
class ToolExposureRuntime:
    """Own the hidden domain catalog and its compact public exposure layer."""

    mode: str
    registry: MCPToolRegistry
    catalog_manager: ToolManager
    public_manager: ToolManager
    terse_summaries: Mapping[str, str]

    def catalog_tools(self) -> Mapping[str, Any]:
        """Return a shallow catalog snapshot for audits and compatibility tests."""

        tools = getattr(self.catalog_manager, "_tools", None)
        if not isinstance(tools, dict):
            raise RuntimeError("FastMCP catalog manager is unavailable")
        return dict(tools)

    def public_tools(self) -> Mapping[str, Any]:
        """Return a shallow snapshot of the tools advertised to MCP clients."""

        tools = getattr(self.public_manager, "_tools", None)
        if not isinstance(tools, dict):
            raise RuntimeError("FastMCP public tool manager is unavailable")
        return dict(tools)

    def _matching_records(self, toolset: str = "", query: str = "") -> list[Dict[str, Any]]:
        normalized_toolset = toolset.strip().lower()
        normalized_query = query.strip().casefold()
        report = self.registry.report(include_tools=True)
        records = report.get("tools", [])
        matches: list[Dict[str, Any]] = []
        for record in records:
            category = str(record.get("category", ""))
            if normalized_toolset not in {"", "all", "*"} and category.casefold() != normalized_toolset:
                continue
            catalog_tool = self.catalog_manager.get_tool(str(record.get("name", "")))
            catalog_description = str(catalog_tool.description or "") if catalog_tool is not None else ""
            haystack = " ".join(
                (
                    str(record.get("name", "")),
                    category,
                    str(record.get("description", "")),
                    catalog_description,
                    str(record.get("module", "")),
                )
            ).casefold()
            if normalized_query and normalized_query not in haystack:
                continue
            matches.append(record)
        return matches

    def list_toolsets(self, query: str = "") -> Dict[str, Any]:
        """Summarize domain toolsets, optionally filtered by a text query."""

        report = self.registry.report(include_tools=False)
        matching = self._matching_records(query=query)
        matching_counts: Dict[str, int] = {}
        for record in matching:
            category = str(record.get("category", ""))
            matching_counts[category] = matching_counts.get(category, 0) + 1

        toolsets = []
        for category, summary in sorted(report.get("categories", {}).items()):
            match_count = matching_counts.get(category, 0)
            if query.strip() and match_count == 0:
                continue
            toolsets.append(
                {
                    "name": category,
                    "description": _TOOLSET_DESCRIPTIONS.get(
                        category,
                        f"Unreal MCP tools registered in the {category} category.",
                    ),
                    "total_tools": summary.get("total", 0),
                    "enabled_tools": summary.get("enabled", 0),
                    "matching_tools": match_count,
                    "effects": summary.get("effects", {}),
                }
            )

        return tool_success(
            exposure_mode=self.mode,
            query=query,
            total_toolsets=len(toolsets),
            total_catalog_tools=report.get("total_tools", 0),
            matching_tools=len(matching),
            toolsets=toolsets,
            next_step="Call describe_toolset with a toolset name, or use toolset='all' with a query.",
        )

    def describe_toolset(
        self,
        toolset: str,
        query: str = "",
        offset: int = 0,
        limit: int = 20,
        include_schema: bool = True,
        verbosity: str = "full",
    ) -> Dict[str, Any]:
        """Return paginated contracts and policies for one toolset or a search."""

        normalized_toolset = toolset.strip().lower()
        categories = set(self.registry.report(include_tools=False).get("categories", {}))
        if normalized_toolset not in {"", "all", "*"} and normalized_toolset not in categories:
            return tool_error(
                f"Unknown toolset: {toolset}",
                error_code="unknown_toolset",
                toolset=toolset,
                available_toolsets=sorted(categories),
            )
        if offset < 0:
            return tool_error("offset must be at least 0", error_code="invalid_offset", offset=offset)
        if limit < 1 or limit > 50:
            return tool_error(
                "limit must be between 1 and 50",
                error_code="invalid_limit",
                limit=limit,
            )
        normalized_verbosity = verbosity.strip().lower()
        if normalized_verbosity not in {"full", "terse"}:
            return tool_error(
                "verbosity must be either 'full' or 'terse'",
                error_code="invalid_verbosity",
                verbosity=verbosity,
            )

        matches = self._matching_records(toolset=normalized_toolset, query=query)
        page = matches[offset : offset + limit]
        described_tools = []
        for record in page:
            name = str(record["name"])
            catalog_tool = self.catalog_manager.get_tool(name)
            if catalog_tool is None:
                return tool_error(
                    f"Catalog contract is missing for registered tool: {name}",
                    error_code="catalog_contract_missing",
                    tool=name,
                )
            if normalized_verbosity == "terse":
                policy = record.get("policy", {})
                item = {
                    "name": name,
                    "summary": self.terse_summaries.get(name, ""),
                    "effect": policy.get("effect"),
                    "confirm_required": bool(policy.get("confirmation_required", False)),
                }
            else:
                item = {
                    "name": name,
                    "toolset": record.get("category", ""),
                    "description": catalog_tool.description or record.get("description", ""),
                    "enabled": bool(record.get("enabled", False)),
                    "policy": record.get("policy", {}),
                }
                if include_schema:
                    item["input_schema"] = catalog_tool.parameters
            described_tools.append(item)

        next_offset = offset + len(page)
        return tool_success(
            exposure_mode=self.mode,
            toolset=toolset,
            query=query,
            offset=offset,
            limit=limit,
            total_matches=len(matches),
            returned_tools=len(described_tools),
            next_offset=next_offset if next_offset < len(matches) else None,
            tools=described_tools,
            next_step="Call call_tool with the exact toolset, tool_name, and arguments.",
        )

    async def dispatch(
        self,
        ctx: Context | None,
        toolset: str,
        tool_name: str,
        arguments: Dict[str, Any] | None = None,
        confirm: bool = False,
    ) -> Any:
        """Validate policy and delegate to the original FastMCP catalog wrapper."""

        name = tool_name.strip()
        record = self.registry.get_record(name)
        if record is None:
            legacy_response = tool_error(
                f"Unknown tool: {tool_name}",
                error_code="unknown_tool",
                tool=tool_name,
                hint="Use list_toolsets and describe_toolset before dispatch.",
            )
            try:
                diagnostics = self._unknown_tool_diagnostics(toolset, name)
                return {**legacy_response, **diagnostics}
            except Exception:
                logger.debug("Could not build unknown-tool recovery hints", exc_info=True)
            return legacy_response

        normalized_toolset = toolset.strip().lower()
        if normalized_toolset not in {record.category.casefold(), "all", "*"}:
            return tool_error(
                f"Tool '{name}' belongs to toolset '{record.category}', not '{toolset}'",
                error_code="toolset_mismatch",
                tool=name,
                requested_toolset=toolset,
                actual_toolset=record.category,
            )
        if arguments is None:
            arguments = {}
        if not isinstance(arguments, dict):
            return tool_error(
                "arguments must be a JSON object",
                error_code="invalid_arguments",
                tool=name,
            )

        disabled_reason = self.registry.disabled_reason(record.name, record.category)
        if disabled_reason:
            return self.registry.disabled_response(record.name, record.category, disabled_reason)

        policy = record.policy
        if requires_dispatch_confirmation(policy, arguments, confirmed=confirm):
            return tool_error(
                f"Explicit confirmation is required before calling high-risk tool '{name}'",
                error_code="confirmation_required",
                tool=name,
                toolset=record.category,
                policy=asdict(policy),
                confirmation={
                    "required": True,
                    "retry_with": {"confirm": True},
                    "dry_run_bypass_available": policy.supports_dry_run,
                },
            )

        try:
            return await self.catalog_manager.call_tool(name, dict(arguments), context=ctx)
        except ToolError as exc:
            logger.info("Search dispatcher rejected %s: %s", name, exc)
            legacy_response = tool_error(
                str(exc),
                error_code="tool_validation_or_execution_error",
                tool=name,
                toolset=record.category,
            )
            if not isinstance(exc.__cause__, ValidationError):
                return legacy_response
            try:
                diagnostics = self._validation_diagnostics(name, arguments)
                return {**legacy_response, **diagnostics}
            except Exception:
                logger.debug("Could not build validation recovery fields for %s", name, exc_info=True)
            return legacy_response

    def _unknown_tool_diagnostics(self, toolset: str, tool_name: str) -> Dict[str, Any]:
        """Build deterministic same-toolset suggestions for an unknown name."""

        normalized_toolset = toolset.strip().lower()
        records = self.registry.report(include_tools=True).get("tools", [])
        if normalized_toolset in {"all", "*"}:
            candidates = sorted(str(record.get("name", "")) for record in records)
        elif normalized_toolset:
            candidates = sorted(
                str(record.get("name", ""))
                for record in records
                if str(record.get("category", "")).casefold() == normalized_toolset
            )
        else:
            candidates = []
        suggestions = (
            difflib.get_close_matches(tool_name, candidates, n=3, cutoff=0.0)
            if tool_name and candidates
            else []
        )
        return {
            "did_you_mean": suggestions,
            "contract_hint": (
                "Call describe_toolset("
                f"toolset={toolset!r}, query={tool_name!r}) to inspect the exact contract."
            ),
        }

    def _validation_diagnostics(
        self,
        tool_name: str,
        arguments: Mapping[str, Any],
    ) -> Dict[str, Any]:
        """Re-run only FastMCP input validation after the original call failed."""

        catalog_tool = self.catalog_manager.get_tool(tool_name)
        if catalog_tool is None:
            return {}
        metadata = catalog_tool.fn_metadata
        pre_parsed = metadata.pre_parse_json(dict(arguments))
        try:
            metadata.arg_model.model_validate(pre_parsed)
        except ValidationError as validation_error:
            return _structured_validation_diagnostics(
                validation_error,
                arguments,
                catalog_tool.parameters,
            )
        return {}


def _register_search_meta_tools(mcp: FastMCP, runtime: ToolExposureRuntime) -> None:
    @mcp.tool(name="list_toolsets")
    def list_toolsets(query: str = "") -> Dict[str, Any]:
        """List Unreal MCP toolsets and counts, optionally filtered by text."""

        return runtime.list_toolsets(query=query)

    @mcp.tool(name="describe_toolset")
    def describe_toolset(
        toolset: str,
        query: str = "",
        offset: int = 0,
        limit: int = 20,
        include_schema: bool = True,
        verbosity: str = "full",
    ) -> Dict[str, Any]:
        """Describe paginated tool contracts in a toolset; use 'all' to search globally."""

        return runtime.describe_toolset(
            toolset=toolset,
            query=query,
            offset=offset,
            limit=limit,
            include_schema=include_schema,
            verbosity=verbosity,
        )

    @mcp.tool(name="call_tool")
    async def call_tool(
        ctx: Context,
        toolset: str,
        tool_name: str,
        arguments: Dict[str, Any] | None = None,
        confirm: bool = False,
    ) -> Any:
        """Call one discovered tool; high-risk mutations require confirm=true."""

        return await runtime.dispatch(
            ctx=ctx,
            toolset=toolset,
            tool_name=tool_name,
            arguments=arguments,
            confirm=confirm,
        )


def configure_tool_exposure(
    mcp: FastMCP,
    registry: MCPToolRegistry,
    requested_mode: str | None = None,
) -> ToolExposureRuntime:
    """Configure eager compatibility or compact search-mode tool exposure."""

    validate_mcp_sdk_version()
    mode = normalize_tool_exposure_mode(requested_mode)
    catalog_manager = getattr(mcp, "_tool_manager", None)
    if not isinstance(catalog_manager, ToolManager):
        raise RuntimeError("FastMCP tool manager API is unavailable")

    if mode == "eager":
        runtime = ToolExposureRuntime(
            mode=mode,
            registry=registry,
            catalog_manager=catalog_manager,
            public_manager=catalog_manager,
            terse_summaries=MappingProxyType({}),
        )
        logger.info("MCP tool exposure mode: eager (%d public tools)", len(runtime.public_tools()))
        return runtime

    public_manager = ToolManager()
    terse_summaries = _build_terse_summary_cache(registry, catalog_manager)
    mcp._tool_manager = public_manager
    runtime = ToolExposureRuntime(
        mode=mode,
        registry=registry,
        catalog_manager=catalog_manager,
        public_manager=public_manager,
        terse_summaries=terse_summaries,
    )
    _register_search_meta_tools(mcp, runtime)
    logger.info(
        "MCP tool exposure mode: search (%d catalog tools, %d public tools)",
        len(runtime.catalog_tools()),
        len(runtime.public_tools()),
    )
    return runtime
