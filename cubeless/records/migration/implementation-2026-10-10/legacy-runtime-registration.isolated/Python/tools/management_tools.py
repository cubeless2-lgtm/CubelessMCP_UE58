"""Runtime tool registry and management helpers for Unreal MCP."""

import ipaddress
import inspect
import logging
import math
import os
import threading
import time
from collections import Counter, deque
from contextlib import contextmanager
from dataclasses import asdict, dataclass
from functools import wraps
from typing import Any, Callable, Dict, Iterable, Iterator, List, Optional, Set

from mcp.server.fastmcp import Context, FastMCP

from tools.response_schema import tool_error, tool_success
from tools.mutation_result_contract import apply_mutation_result_contract
from tools.tool_policy import ToolPolicy, infer_tool_policy

logger = logging.getLogger("UnrealMCP")


def _env_bool(name: str, default: bool = False) -> bool:
    value = os.environ.get(name)
    if value is None:
        return default
    return value.strip().lower() in {"1", "true", "yes", "on"}


def _env_int(name: str, default: int = 0) -> int:
    value = os.environ.get(name)
    if value is None or value.strip() == "":
        return default
    try:
        return int(value)
    except ValueError:
        logger.warning("Invalid integer for %s: %s", name, value)
        return default


def _is_loopback_host(host: str) -> bool:
    normalized = (host or "").strip().lower()
    if normalized in {"localhost", "localhost.localdomain"}:
        return True
    try:
        return ipaddress.ip_address(normalized).is_loopback
    except ValueError:
        return False


class RuntimeTelemetry:
    """Small in-memory telemetry buffer for MCP tool and bridge requests."""

    _KNOWN_KINDS = frozenset({"tool", "unreal_command", "guard"})
    _KNOWN_GUARD_TYPES = frozenset({"bridge_target", "rate_limit"})
    _STAGE_ALLOWLIST = frozenset(
        {"lock_wait", "connect", "serialize", "send", "receive", "decode", "total"}
    )
    _UNKNOWN_LABEL = "__unknown__"
    _OVERSIZE_LABEL = "__oversize__"
    _OVERFLOW_LABEL = "__overflow__"
    _FIXED_LABELS = frozenset({_UNKNOWN_LABEL, _OVERSIZE_LABEL, _OVERFLOW_LABEL})
    _MAX_LABEL_CHARS = 128
    _MAX_GUARD_DETAIL_CHARS = 256
    _LABEL_CHARACTERS = frozenset(
        "abcdefghijklmnopqrstuvwxyzABCDEFGHIJKLMNOPQRSTUVWXYZ0123456789_.:-"
    )

    def __init__(
        self,
        max_events: int = 80,
        max_active: int = 256,
        max_names: int = 512,
        max_statuses: int = 64,
    ) -> None:
        if min(max_events, max_active, max_names, max_statuses) <= 0:
            raise ValueError("RuntimeTelemetry limits must be positive")
        self.started_at = time.time()
        self._lock = threading.Lock()
        self._next_id = 1
        self._max_active = max_active
        self._max_names = max_names
        self._max_statuses = max_statuses
        self._active: Dict[int, Dict[str, Any]] = {}
        self._recent_events = deque(maxlen=max_events)
        self._token_owner = object()
        self._known_name_keys: Set[str] = set()
        self._known_statuses: Set[str] = set()
        self._active_tracked = 0
        self._active_untracked = 0
        self._active_overflow_total = 0
        self._current_by_kind: Counter = Counter()
        self._stage_dropped = 0
        self._byte_dropped = 0
        self._duplicate_end_dropped = 0
        self._post_finalize_stage_dropped = 0
        self._post_finalize_bytes_dropped = 0
        self._invalid_token_dropped = 0
        self._stage_totals_ms: Counter = Counter()
        self._stage_counts: Counter = Counter()
        self._request_bytes_total = 0
        self._response_bytes_total = 0
        self._total_by_kind: Counter = Counter()
        self._failure_by_kind: Counter = Counter()
        self._total_by_name: Counter = Counter()
        self._failure_by_name: Counter = Counter()
        self._status_counts: Counter = Counter()
        self._guard_events: Counter = Counter()

    @classmethod
    def _normalized_text(cls, value: Any) -> str:
        """Return a fixed bucket before trimming unbounded/untrusted labels."""

        if not isinstance(value, str):
            return cls._UNKNOWN_LABEL
        # Check raw input before strip so a large whitespace-only label cannot
        # force an allocation or a new telemetry key.
        if len(value) > cls._MAX_LABEL_CHARS:
            return cls._OVERSIZE_LABEL
        if not value.isascii():
            return cls._UNKNOWN_LABEL
        if any(character not in cls._LABEL_CHARACTERS and not character.isspace() for character in value):
            return cls._UNKNOWN_LABEL
        label = value.strip()
        if not label:
            return cls._UNKNOWN_LABEL
        if any(character not in cls._LABEL_CHARACTERS for character in label):
            return cls._UNKNOWN_LABEL
        return label

    def _bounded_status(self, value: Any) -> str:
        label = self._normalized_text(value)
        if label in self._FIXED_LABELS:
            return label
        if label in self._known_statuses:
            return label
        if len(self._known_statuses) >= self._max_statuses:
            return self._OVERFLOW_LABEL
        self._known_statuses.add(label)
        return label

    def _bounded_name(self, kind: str, value: Any) -> str:
        """Cap the composite ``kind:name`` keys exposed by total_by_name."""

        label = self._normalized_text(value)
        if label in self._FIXED_LABELS:
            return label
        composite_key = f"{kind}:{label}"
        if composite_key in self._known_name_keys:
            return label
        if len(self._known_name_keys) >= self._max_names:
            return self._OVERFLOW_LABEL
        self._known_name_keys.add(composite_key)
        return label

    @classmethod
    def _bounded_kind(cls, value: Any) -> str:
        kind = cls._normalized_text(value)
        return kind if kind in cls._KNOWN_KINDS else cls._UNKNOWN_LABEL

    def _token_state_unlocked(self, token: Any) -> str:
        if not isinstance(token, dict) or token.get("_runtime_telemetry_owner") is not self._token_owner:
            self._invalid_token_dropped += 1
            return "invalid"
        if token.get("_runtime_telemetry_finalized", False):
            return "finalized"
        return "open"

    @staticmethod
    def _bounded_detail(value: Any) -> str:
        if not isinstance(value, str):
            return ""
        return value[: RuntimeTelemetry._MAX_GUARD_DETAIL_CHARS]

    def begin(self, kind: str, name: str) -> Dict[str, Any]:
        now = time.perf_counter()
        with self._lock:
            request_id = self._next_id
            self._next_id += 1
            bounded_kind = self._bounded_kind(kind)
            bounded_name = self._bounded_name(bounded_kind, name)
            tracked = len(self._active) < self._max_active
            token = {
                "id": request_id,
                "kind": bounded_kind,
                "name": bounded_name,
                "started": now,
                "active_tracked": tracked,
                "stages_ms": {},
                "_runtime_telemetry_owner": self._token_owner,
                "_runtime_telemetry_finalized": False,
                "_runtime_telemetry_request_id": request_id,
                "_runtime_telemetry_kind": bounded_kind,
                "_runtime_telemetry_name": bounded_name,
                "_runtime_telemetry_tracked": tracked,
            }
            if tracked:
                self._active[request_id] = token
                self._active_tracked += 1
            else:
                self._active_untracked += 1
                self._active_overflow_total += 1
            self._current_by_kind[bounded_kind] += 1
            self._total_by_kind[bounded_kind] += 1
            self._total_by_name[f"{bounded_kind}:{bounded_name}"] += 1
            return token

    def end(self, token: Dict[str, Any], success: bool, status: str = "ok", error: str = "") -> None:
        with self._lock:
            token_state = self._token_state_unlocked(token)
            if token_state != "open":
                if token_state == "finalized":
                    self._duplicate_end_dropped += 1
                return
            token["_runtime_telemetry_finalized"] = True
            finished = time.perf_counter()
            duration_ms = round(
                (finished - float(token.get("started", finished))) * 1000.0,
                3,
            )
            kind = self._bounded_kind(token.get("_runtime_telemetry_kind"))
            name = self._bounded_name(kind, token.get("_runtime_telemetry_name"))
            bounded_status = self._bounded_status(status)
            request_id = int(token.get("_runtime_telemetry_request_id", 0))
            event = {
                "id": request_id,
                "kind": kind,
                "name": name,
                "success": bool(success),
                "status": bounded_status,
                "duration_ms": duration_ms,
            }
            stages_ms = token.get("stages_ms")
            if isinstance(stages_ms, dict) and stages_ms:
                finalized_stages = {
                    stage: value
                    for stage, value in stages_ms.items()
                    if stage in self._STAGE_ALLOWLIST
                    and isinstance(value, (int, float))
                    and math.isfinite(value)
                    and value >= 0
                }
                if finalized_stages:
                    event["stages_ms"] = finalized_stages
                    for stage, value in finalized_stages.items():
                        self._stage_totals_ms[stage] += value
                        self._stage_counts[stage] += 1
            if "request_bytes" in token:
                event["request_bytes"] = token["request_bytes"]
            if "response_bytes" in token:
                event["response_bytes"] = token["response_bytes"]
            if error:
                event["error"] = str(error)[:500]
            if token.get("_runtime_telemetry_tracked", False):
                if self._active.pop(request_id, None) is not None:
                    self._active_tracked -= 1
            else:
                self._active_untracked = max(0, self._active_untracked - 1)
            self._current_by_kind[kind] -= 1
            if self._current_by_kind[kind] <= 0:
                self._current_by_kind.pop(kind, None)
            if not success:
                self._failure_by_kind[kind] += 1
                self._failure_by_name[f"{kind}:{name}"] += 1
            self._status_counts[bounded_status] += 1
            request_bytes = token.get("request_bytes")
            if isinstance(request_bytes, int) and request_bytes >= 0:
                self._request_bytes_total += request_bytes
            response_bytes = token.get("response_bytes")
            if isinstance(response_bytes, int) and response_bytes >= 0:
                self._response_bytes_total += response_bytes
            self._recent_events.append(event)

    def record_stage(self, token: Dict[str, Any], stage: str, duration_seconds: float) -> None:
        """Record one fixed-stage duration without accepting arbitrary labels."""

        with self._lock:
            token_state = self._token_state_unlocked(token)
            if token_state != "open":
                if token_state == "finalized":
                    self._post_finalize_stage_dropped += 1
                return
            if stage not in self._STAGE_ALLOWLIST:
                self._stage_dropped += 1
                return
        try:
            duration_ms = round(float(duration_seconds) * 1000.0, 3)
        except (TypeError, ValueError):
            with self._lock:
                self._stage_dropped += 1
            return
        if not math.isfinite(duration_ms) or duration_ms < 0:
            with self._lock:
                self._stage_dropped += 1
            return
        with self._lock:
            token_state = self._token_state_unlocked(token)
            if token_state != "open":
                if token_state == "finalized":
                    self._post_finalize_stage_dropped += 1
                return
            stages_ms = token.get("stages_ms")
            if not isinstance(stages_ms, dict):
                self._stage_dropped += 1
                return
            stages_ms[stage] = duration_ms

    def record_bytes(
        self,
        token: Dict[str, Any],
        *,
        request_bytes: Optional[int] = None,
        response_bytes: Optional[int] = None,
    ) -> None:
        """Keep payload sizes only when the caller observed an exact byte count."""

        with self._lock:
            token_state = self._token_state_unlocked(token)
            if token_state != "open":
                if token_state == "finalized":
                    self._post_finalize_bytes_dropped += 1
                return
            for key, value in (("request_bytes", request_bytes), ("response_bytes", response_bytes)):
                if isinstance(value, int) and value >= 0:
                    token[key] = value
                elif value is not None:
                    self._byte_dropped += 1

    def record_guard_event(self, guard_type: str, name: str, decision: Dict[str, Any]) -> None:
        bounded_guard_type = (
            guard_type if guard_type in self._KNOWN_GUARD_TYPES else self._UNKNOWN_LABEL
        )
        with self._lock:
            event = {
                "id": 0,
                "kind": "guard",
                "name": self._bounded_name("guard", name),
                "success": bool(decision.get("allowed", False)),
                "status": self._bounded_status(bounded_guard_type),
                "duration_ms": 0.0,
                "reason": self._bounded_detail(decision.get("reason", "")),
                "mode": self._bounded_detail(decision.get("mode", "")),
                "would_block": bool(decision.get("would_block", False)),
            }
            self._guard_events[bounded_guard_type] += 1
            self._recent_events.append(event)

    def report(self, include_recent: bool = True) -> Dict[str, Any]:
        with self._lock:
            active_total = self._active_tracked + self._active_untracked
            current_by_kind = dict(self._current_by_kind)
            report = {
                "started_at_epoch": self.started_at,
                "uptime_seconds": round(time.time() - self.started_at, 3),
                "active_requests": active_total,
                "active_total": active_total,
                "active_by_kind": current_by_kind,
                "current_by_kind": current_by_kind,
                "active_tracked": self._active_tracked,
                "active_untracked": self._active_untracked,
                "total_by_kind": dict(self._total_by_kind),
                "failure_by_kind": dict(self._failure_by_kind),
                "total_by_name": dict(self._total_by_name),
                "failure_by_name": dict(self._failure_by_name),
                "status_counts": dict(self._status_counts),
                "guard_events": dict(self._guard_events),
                "stage_totals_ms": dict(self._stage_totals_ms),
                "stage_counts": dict(self._stage_counts),
                "request_bytes_total": self._request_bytes_total,
                "response_bytes_total": self._response_bytes_total,
                "active_overflow": self._active_overflow_total,
                "active_overflow_total": self._active_overflow_total,
                "stage_dropped": self._stage_dropped,
                "byte_dropped": self._byte_dropped,
                "duplicate_end_dropped": self._duplicate_end_dropped,
                "post_finalize_stage_dropped": self._post_finalize_stage_dropped,
                "post_finalize_bytes_dropped": self._post_finalize_bytes_dropped,
                "invalid_token_dropped": self._invalid_token_dropped,
                "telemetry_limits": {
                    "max_active": self._max_active,
                    "max_names": self._max_names,
                    "max_statuses": self._max_statuses,
                    "max_recent_events": self._recent_events.maxlen,
                    "max_label_chars": self._MAX_LABEL_CHARS,
                    "max_guard_detail_chars": self._MAX_GUARD_DETAIL_CHARS,
                },
            }
            if include_recent:
                report["recent_events"] = list(self._recent_events)
            return report

    def reset_for_tests(self) -> None:
        with self._lock:
            self._active.clear()
            self._recent_events.clear()
            self._known_name_keys.clear()
            self._known_statuses.clear()
            self._active_tracked = 0
            self._active_untracked = 0
            self._active_overflow_total = 0
            self._current_by_kind.clear()
            self._stage_dropped = 0
            self._byte_dropped = 0
            self._duplicate_end_dropped = 0
            self._post_finalize_stage_dropped = 0
            self._post_finalize_bytes_dropped = 0
            self._invalid_token_dropped = 0
            self._stage_totals_ms.clear()
            self._stage_counts.clear()
            self._request_bytes_total = 0
            self._response_bytes_total = 0
            self._total_by_kind.clear()
            self._failure_by_kind.clear()
            self._total_by_name.clear()
            self._failure_by_name.clear()
            self._status_counts.clear()
            self._guard_events.clear()


class RuntimeGuard:
    """Conservative runtime guards for future transports and current bridge use."""

    def __init__(self) -> None:
        self._lock = threading.Lock()
        self._rate_events: Dict[str, deque] = {}

    def config(self) -> Dict[str, Any]:
        mode = os.environ.get("UNREAL_MCP_GUARD_MODE", "hard").strip().lower()
        if mode not in {"off", "soft", "hard"}:
            mode = "hard"
        rate_limit = max(0, _env_int("UNREAL_MCP_RATE_LIMIT_PER_MINUTE", 0))
        return {
            "mode": mode,
            "allow_non_loopback": _env_bool("UNREAL_MCP_ALLOW_NON_LOOPBACK", False),
            "require_token_for_non_loopback": _env_bool("UNREAL_MCP_REQUIRE_TOKEN_FOR_NON_LOOPBACK", True),
            "capability_token_configured": bool(os.environ.get("UNREAL_MCP_CAPABILITY_TOKEN", "")),
            "rate_limit_per_minute": rate_limit,
        }

    def check_bridge_target(self, host: str, port: int) -> Dict[str, Any]:
        config = self.config()
        is_loopback = _is_loopback_host(host)
        if config["mode"] == "off" or is_loopback:
            return {
                "allowed": True,
                "would_block": False,
                "mode": config["mode"],
                "host": host,
                "port": port,
                "loopback": is_loopback,
                "reason": "loopback bridge target" if is_loopback else "guard mode off",
            }

        token_ok = bool(config["capability_token_configured"]) or not bool(config["require_token_for_non_loopback"])
        explicitly_allowed = bool(config["allow_non_loopback"]) and token_ok
        would_block = not explicitly_allowed
        allowed = explicitly_allowed or config["mode"] == "soft"
        reason = "non-loopback bridge target allowed by explicit configuration"
        if would_block:
            reason = "non-loopback bridge target requires explicit allow and configured capability token"
        return {
            "allowed": allowed,
            "would_block": would_block,
            "mode": config["mode"],
            "host": host,
            "port": port,
            "loopback": False,
            "reason": reason,
        }

    def check_rate_limit(self, kind: str, name: str, protected: bool = False) -> Dict[str, Any]:
        config = self.config()
        limit = int(config["rate_limit_per_minute"])
        if protected or config["mode"] == "off" or limit <= 0:
            return {"allowed": True, "would_block": False, "mode": config["mode"], "reason": "rate limit disabled"}

        now = time.monotonic()
        key = f"{kind}:{name}"
        with self._lock:
            events = self._rate_events.setdefault(key, deque())
            while events and now - events[0] >= 60.0:
                events.popleft()
            would_block = len(events) >= limit
            if not would_block or config["mode"] == "soft":
                events.append(now)

        allowed = not would_block or config["mode"] == "soft"
        reason = f"rate limit exceeded for {key}: {limit}/minute" if would_block else "within rate limit"
        return {
            "allowed": allowed,
            "would_block": would_block,
            "mode": config["mode"],
            "reason": reason,
            "limit_per_minute": limit,
            "key": key,
        }

    def reset_for_tests(self) -> None:
        with self._lock:
            self._rate_events.clear()


request_telemetry = RuntimeTelemetry()
runtime_guard = RuntimeGuard()
_health_provider: Optional[Callable[..., Dict[str, Any]]] = None


def set_health_provider(provider: Callable[..., Dict[str, Any]]) -> None:
    global _health_provider
    _health_provider = provider


def _provider_health(ping_unreal: bool = False) -> Dict[str, Any]:
    if _health_provider is None:
        return {
            "success": True,
            "unreal_bridge": {
                "status": "unknown",
                "message": "No Unreal bridge health provider has been registered",
            },
        }
    try:
        return _health_provider(ping_unreal=ping_unreal)
    except Exception as exc:
        logger.warning("Runtime health provider failed: %s", exc)
        return tool_error(
            str(exc),
            error_code="health_provider_failed",
            unreal_bridge={
                "status": "error",
                "message": str(exc),
            },
        )


@dataclass(frozen=True)
class ToolRecord:
    name: str
    category: str
    module: str
    line: int
    description: str
    policy: ToolPolicy
    protected: bool = False


class MCPToolRegistry:
    """Track FastMCP tools and apply explicit runtime enable/disable policy."""

    def __init__(self) -> None:
        self._tools: Dict[str, ToolRecord] = {}
        self._disabled_tools: Set[str] = set()
        self._disabled_categories: Set[str] = set()
        self._protected_tools: Set[str] = set()

    @contextmanager
    def capture_tools(
        self,
        mcp: FastMCP,
        category: str,
        protected_tools: Iterable[str] = (),
    ) -> Iterator[None]:
        original_tool = mcp.tool
        protected_names = set(protected_tools)

        def managed_tool(name: Optional[str] = None, description: Optional[str] = None):
            original_decorator = original_tool(name=name, description=description)

            def decorator(func: Callable[..., Any]) -> Callable[..., Any]:
                tool_name = name or func.__name__
                wrapped = self._wrap_tool(tool_name, category, func)
                original_decorator(wrapped)
                self.register_tool(
                    tool_name,
                    category,
                    func,
                    description=description,
                    protected=tool_name in protected_names,
                )
                return wrapped

            return decorator

        mcp.tool = managed_tool  # type: ignore[method-assign]
        try:
            yield
        finally:
            mcp.tool = original_tool  # type: ignore[method-assign]

    def register_tool(
        self,
        name: str,
        category: str,
        func: Callable[..., Any],
        description: Optional[str] = None,
        protected: bool = False,
    ) -> None:
        doc = inspect.getdoc(func) or ""
        public_description = description or doc.splitlines()[0] if doc else description or ""
        record = ToolRecord(
            name=name,
            category=category,
            module=getattr(func, "__module__", ""),
            line=getattr(getattr(func, "__code__", None), "co_firstlineno", 0),
            description=public_description,
            policy=infer_tool_policy(
                name=name,
                category=category,
                signature=inspect.signature(func),
                description=public_description,
            ),
            protected=protected,
        )
        if name in self._tools:
            logger.warning("Replacing existing MCP tool registry record for %s", name)
        self._tools[name] = record
        if protected:
            self._protected_tools.add(name)
            self._disabled_tools.discard(name)

    def _wrap_tool(self, name: str, category: str, func: Callable[..., Any]) -> Callable[..., Any]:
        tool_category = category
        if inspect.iscoroutinefunction(func):

            @wraps(func)
            async def async_wrapper(*args: Any, **kwargs: Any) -> Any:
                return await self._run_async_tool(name, tool_category, func, *args, **kwargs)

            async_wrapper.__signature__ = inspect.signature(func)  # type: ignore[attr-defined]
            return async_wrapper

        @wraps(func)
        def wrapper(*args: Any, **kwargs: Any) -> Any:
            return self._run_sync_tool(name, tool_category, func, *args, **kwargs)

        wrapper.__signature__ = inspect.signature(func)  # type: ignore[attr-defined]
        return wrapper

    def _preflight_tool(self, name: str, tool_category: str) -> Optional[Dict[str, Any]]:
        protected = name in self._protected_tools
        rate_decision = runtime_guard.check_rate_limit("tool", name, protected=protected)
        if rate_decision.get("would_block"):
            request_telemetry.record_guard_event("rate_limit", name, rate_decision)
        if not rate_decision.get("allowed", True):
            return tool_error(
                rate_decision.get("reason", "rate limit exceeded"),
                error_code="rate_limited",
                tool=name,
                category=tool_category,
                guard=rate_decision,
            )

        disabled_reason = self.disabled_reason(name, tool_category)
        if disabled_reason:
            return self.disabled_response(name, tool_category, disabled_reason)
        return None

    def _result_success(self, result: Any) -> bool:
        if isinstance(result, dict):
            if result.get("success") is False:
                return False
            if result.get("status") in {"error", "disabled", "rate_limited", "blocked"}:
                return False
        return True

    def _result_status(self, result: Any) -> str:
        if isinstance(result, dict):
            status = result.get("status")
            if isinstance(status, str) and status:
                return status
            error_code = result.get("error_code")
            if isinstance(error_code, str) and error_code:
                return error_code
            if result.get("success") is False:
                return "error"
        return "ok"

    def _result_error(self, result: Any) -> str:
        if isinstance(result, dict):
            error = result.get("error") or result.get("message")
            if isinstance(error, str):
                return error
        return ""

    def _run_sync_tool(
        self,
        registered_name: str,
        registered_category: str,
        registered_func: Callable[..., Any],
        /,
        *args: Any,
        **kwargs: Any,
    ) -> Any:
        token = request_telemetry.begin("tool", registered_name)
        try:
            preflight = self._preflight_tool(registered_name, registered_category)
            if preflight is not None:
                request_telemetry.end(token, False, status=self._result_status(preflight), error=self._result_error(preflight))
                return preflight
            result = registered_func(*args, **kwargs)
            result = self._apply_result_contract(registered_name, registered_func, args, kwargs, result)
            request_telemetry.end(token, self._result_success(result), status=self._result_status(result), error=self._result_error(result))
            return result
        except Exception as exc:
            request_telemetry.end(token, False, status="exception", error=str(exc))
            raise

    async def _run_async_tool(
        self,
        registered_name: str,
        registered_category: str,
        registered_func: Callable[..., Any],
        /,
        *args: Any,
        **kwargs: Any,
    ) -> Any:
        token = request_telemetry.begin("tool", registered_name)
        try:
            preflight = self._preflight_tool(registered_name, registered_category)
            if preflight is not None:
                request_telemetry.end(token, False, status=self._result_status(preflight), error=self._result_error(preflight))
                return preflight
            result = await registered_func(*args, **kwargs)
            result = self._apply_result_contract(registered_name, registered_func, args, kwargs, result)
            request_telemetry.end(token, self._result_success(result), status=self._result_status(result), error=self._result_error(result))
            return result
        except Exception as exc:
            request_telemetry.end(token, False, status="exception", error=str(exc))
            raise

    def _apply_result_contract(
        self,
        name: str,
        func: Callable[..., Any],
        args: tuple[Any, ...],
        kwargs: Dict[str, Any],
        result: Any,
    ) -> Any:
        record = self._tools.get(name)
        if record is None or record.policy.effect == "read":
            return result

        arguments: Dict[str, Any] = dict(kwargs)
        try:
            bound = inspect.signature(func).bind_partial(*args, **kwargs)
            bound.apply_defaults()
            arguments = dict(bound.arguments)
        except (TypeError, ValueError):
            logger.warning("Could not bind arguments for mutation result contract: %s", name)

        return apply_mutation_result_contract(
            result,
            tool_name=name,
            policy=record.policy,
            arguments=arguments,
        )

    def disabled_reason(self, name: str, category: str) -> str:
        if name in self._protected_tools:
            return ""
        if name in self._disabled_tools:
            return f"tool '{name}' is disabled"
        if category in self._disabled_categories:
            return f"category '{category}' is disabled"
        return ""

    def disabled_response(self, name: str, category: str, reason: str) -> Dict[str, Any]:
        return tool_error(
            reason,
            error_code="disabled",
            tool=name,
            category=category,
        )

    def set_tool_enabled(self, name: str, enabled: bool) -> Dict[str, Any]:
        if name not in self._tools:
            return tool_error(f"Unknown tool: {name}", error_code="unknown_tool", tool=name)
        if name in self._protected_tools and not enabled:
            return tool_error(
                f"Protected tool cannot be disabled: {name}",
                error_code="protected_tool",
                tool=name,
            )
        if enabled:
            self._disabled_tools.discard(name)
        else:
            self._disabled_tools.add(name)
        return tool_success(tool=name, enabled=self.is_tool_enabled(name))

    def set_category_enabled(self, category: str, enabled: bool) -> Dict[str, Any]:
        categories = {record.category for record in self._tools.values()}
        if category not in categories:
            return tool_error(f"Unknown category: {category}", error_code="unknown_category", category=category)
        if enabled:
            self._disabled_categories.discard(category)
        else:
            self._disabled_categories.add(category)
        return tool_success(category=category, enabled=category not in self._disabled_categories)

    def reset(self) -> Dict[str, Any]:
        self._disabled_tools.clear()
        self._disabled_categories.clear()
        return tool_success(message="All runtime tool policy overrides were cleared")

    def is_tool_enabled(self, name: str) -> bool:
        record = self._tools.get(name)
        if not record:
            return False
        return self.disabled_reason(record.name, record.category) == ""

    def get_record(self, name: str) -> Optional[ToolRecord]:
        """Return immutable registry metadata for one public domain tool."""

        return self._tools.get(name)

    def report(self, include_tools: bool = True) -> Dict[str, Any]:
        category_summary: Dict[str, Dict[str, Any]] = {}
        for record in sorted(self._tools.values(), key=lambda value: (value.category, value.name)):
            summary = category_summary.setdefault(
                record.category,
                {
                    "total": 0,
                    "enabled": 0,
                    "disabled": 0,
                    "effects": {},
                    "disabled_by_category": record.category in self._disabled_categories,
                },
            )
            summary["total"] += 1
            effects = summary["effects"]
            effects[record.policy.effect] = effects.get(record.policy.effect, 0) + 1
            if self.is_tool_enabled(record.name):
                summary["enabled"] += 1
            else:
                summary["disabled"] += 1

        tools: List[Dict[str, Any]] = []
        if include_tools:
            for record in sorted(self._tools.values(), key=lambda value: (value.category, value.name)):
                item = asdict(record)
                item["enabled"] = self.is_tool_enabled(record.name)
                item["disabled_reason"] = self.disabled_reason(record.name, record.category)
                tools.append(item)

        policy_effects = Counter(record.policy.effect for record in self._tools.values())
        policy_risks = Counter(record.policy.risk for record in self._tools.values())
        policy_verification_levels = Counter(
            record.policy.verification_level for record in self._tools.values()
        )
        policy_result_contracts = Counter(
            record.policy.result_contract for record in self._tools.values()
        )
        return {
            "success": True,
            "total_tools": len(self._tools),
            "enabled_tools": sum(1 for name in self._tools if self.is_tool_enabled(name)),
            "disabled_tools": sorted(self._disabled_tools),
            "disabled_categories": sorted(self._disabled_categories),
            "protected_tools": sorted(self._protected_tools),
            "policy_summary": {
                "effects": dict(sorted(policy_effects.items())),
                "risks": dict(sorted(policy_risks.items())),
                "verification_levels": dict(sorted(policy_verification_levels.items())),
                "result_contracts": dict(sorted(policy_result_contracts.items())),
                "strict_postconditions": sum(
                    1 for record in self._tools.values() if record.policy.strict_postcondition
                ),
                "confirmation_required": sum(
                    1 for record in self._tools.values() if record.policy.confirmation_required
                ),
                "supports_dry_run": sum(
                    1 for record in self._tools.values() if record.policy.supports_dry_run
                ),
            },
            "categories": category_summary,
            "tools": tools,
        }

    def health_report(self, include_tools: bool = False, include_recent: bool = True, ping_unreal: bool = False) -> Dict[str, Any]:
        health = _provider_health(ping_unreal=ping_unreal)
        health.update(
            {
                "success": bool(health.get("success", True)),
                "registry": self.report(include_tools=include_tools),
                "telemetry": request_telemetry.report(include_recent=include_recent),
                "guard": runtime_guard.config(),
            }
        )
        return health

    def manage(
        self,
        action: str = "status",
        tool_name: str = "",
        category: str = "",
        enabled: Optional[bool] = None,
        include_tools: bool = True,
        include_recent: bool = True,
        ping_unreal: bool = False,
    ) -> Dict[str, Any]:
        normalized_action = action.lower().strip()

        if normalized_action in {"status", "list", "list_tools"}:
            return self.report(include_tools=include_tools)
        if normalized_action in {"health", "heartbeat"}:
            if normalized_action == "heartbeat":
                ping_unreal = True
            return self.health_report(include_tools=include_tools, include_recent=include_recent, ping_unreal=ping_unreal)
        if normalized_action == "reset":
            result = self.reset()
            result["registry"] = self.report(include_tools=include_tools)
            return result
        if normalized_action in {"enable_tool", "disable_tool", "set_tool"}:
            if not tool_name:
                return tool_error("tool_name is required", error_code="missing_tool_name")
            if normalized_action == "set_tool" and enabled is None:
                return tool_error("enabled is required for set_tool", error_code="missing_enabled")
            desired = enabled if normalized_action == "set_tool" else normalized_action == "enable_tool"
            result = self.set_tool_enabled(tool_name, bool(desired))
            result["registry"] = self.report(include_tools=include_tools)
            return result
        if normalized_action in {"enable_category", "disable_category", "set_category"}:
            if not category:
                return tool_error("category is required", error_code="missing_category")
            if normalized_action == "set_category" and enabled is None:
                return tool_error("enabled is required for set_category", error_code="missing_enabled")
            desired = enabled if normalized_action == "set_category" else normalized_action == "enable_category"
            result = self.set_category_enabled(category, bool(desired))
            result["registry"] = self.report(include_tools=include_tools)
            return result

        return tool_error(
            f"Unknown action: {action}",
            error_code="unknown_action",
            supported_actions=[
                "status",
                "list",
                "list_tools",
                "health",
                "heartbeat",
                "enable_tool",
                "disable_tool",
                "set_tool",
                "enable_category",
                "disable_category",
                "set_category",
                "reset",
            ],
        )


tool_registry = MCPToolRegistry()


def register_management_tools(mcp: FastMCP, registry: MCPToolRegistry = tool_registry) -> None:
    """Register protected runtime management tools."""

    with registry.capture_tools(mcp, category="management", protected_tools=("manage_tools",)):

        @mcp.tool()
        def manage_tools(
            ctx: Context,
            action: str = "status",
            tool_name: str = "",
            category: str = "",
            enabled: Optional[bool] = None,
            include_tools: bool = True,
            include_recent: bool = True,
            ping_unreal: bool = False,
        ) -> Dict[str, Any]:
            """
            Inspect or adjust runtime MCP tool policy.

            Defaults to a read-only status report. Enable/disable actions are
            in-memory only and reset when the MCP server restarts.
            """

            return registry.manage(
                action=action,
                tool_name=tool_name,
                category=category,
                enabled=enabled,
                include_tools=include_tools,
                include_recent=include_recent,
                ping_unreal=ping_unreal,
            )

    logger.info("Management tools registered successfully")
