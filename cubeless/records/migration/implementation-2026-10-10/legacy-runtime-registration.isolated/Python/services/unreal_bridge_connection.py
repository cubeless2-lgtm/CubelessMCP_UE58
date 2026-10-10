"""Reusable TCP client for the UnrealMCP editor bridge.

This module contains only transport behavior.  Server configuration, shared
connection lifecycle, FastMCP registration, and backward-compatible exports
remain in ``unreal_mcp_server``.
"""

from __future__ import annotations

import json
import logging
import select
import socket
import threading
import time
from typing import Any, Dict, Optional


logger = logging.getLogger("UnrealMCP")
TRANSPORT_MARKER_TYPE = "unreal_bridge_transport_v1"
PRISTINE_SOCKET_MAX_AGE_SECONDS = 1.0
MAX_CONNECT_TIMEOUT_SECONDS = 2.0


def unreal_python_result_unknown_error(
    message: str,
    *,
    error_code: str,
    unreal_response: Any,
    failure_kind: str,
    exception: Exception | None = None,
) -> Dict[str, Any]:
    """Fail closed when executed Unreal Python has no trustworthy side-channel result."""

    result: Dict[str, Any] = {
        "success": False,
        "status": "error",
        "message": message,
        "error": message,
        "error_code": error_code,
        "unreal_response": unreal_response,
        "transport": {
            "marker_type": TRANSPORT_MARKER_TYPE,
            "request_write_started": True,
            "request_fully_sent": True,
            "response_bytes_received": 0,
            "response_complete": True,
            "response_decoded": True,
            "response_contract_valid": True,
            "command_outcome": "unknown",
            "command_may_still_be_in_flight": False,
            "failure_phase": "result_side_channel",
            "failure_kind": failure_kind,
            "retry_safety": "requires_effect_classification",
            "exception_type": type(exception).__name__ if exception is not None else "",
        },
    }
    if exception is not None:
        result["details"] = {
            "exception_type": type(exception).__name__,
            "message": str(exception),
        }
    return result


def _set_response_progress(
    exc: Exception,
    *,
    bytes_received: int,
    complete: bool,
    decoded: bool,
    contract_valid: bool,
) -> None:
    """Attach best-effort receive facts while preserving exception types."""

    exc.unreal_bridge_response_bytes_received = bytes_received  # type: ignore[attr-defined]
    exc.unreal_bridge_response_complete = complete  # type: ignore[attr-defined]
    exc.unreal_bridge_response_decoded = decoded  # type: ignore[attr-defined]
    exc.unreal_bridge_response_contract_valid = contract_valid  # type: ignore[attr-defined]


def _unknown_command_outcome_marker(
    exc: Exception,
    *,
    request_write_started: bool,
    request_fully_sent: bool,
) -> Dict[str, Any]:
    """Describe an indeterminate command after a socket write began.

    The marker reports only what this client observed.  Policy code classifies
    the command effect before deciding whether a retry is safe.
    """

    is_timeout = isinstance(exc, (TimeoutError, socket.timeout))
    response_bytes_received = int(
        getattr(exc, "unreal_bridge_response_bytes_received", 0)
    )
    response_complete = bool(getattr(exc, "unreal_bridge_response_complete", False))
    response_decoded = bool(getattr(exc, "unreal_bridge_response_decoded", False))
    response_contract_valid = bool(
        getattr(exc, "unreal_bridge_response_contract_valid", False)
    )
    return {
        "marker_type": TRANSPORT_MARKER_TYPE,
        "request_write_started": request_write_started,
        "request_fully_sent": request_fully_sent,
        "response_bytes_received": response_bytes_received,
        "response_complete": response_complete,
        "response_decoded": response_decoded,
        "response_contract_valid": response_contract_valid,
        "command_outcome": "unknown",
        # A complete response, even an invalid one, proves the editor's
        # command path reached a response boundary. Its result remains unknown
        # but is not still executing.
        "command_may_still_be_in_flight": not response_complete,
        "failure_phase": "response" if request_fully_sent else "write",
        "failure_kind": (
            "timeout"
            if is_timeout
            else "response_contract_invalid"
            if response_complete and response_decoded and not response_contract_valid
            else "response_decode_error"
            if response_complete and not response_decoded
            else "response_exception"
            if request_fully_sent
            else "write_exception"
        ),
        "retry_safety": "requires_effect_classification",
        "exception_type": type(exc).__name__,
    }


def _summarize_for_log(value: Any, max_chars: int) -> str:
    """Return a bounded JSON-ish representation for operational logs."""

    try:
        text = json.dumps(value, ensure_ascii=False, default=str)
    except Exception:
        text = str(value)
    if len(text) <= max_chars:
        return text
    return f"{text[:max_chars]}... <truncated {len(text) - max_chars} chars>"


class UnrealBridgeConnection:
    """One-command-per-connection TCP client for an Unreal Editor bridge."""

    def __init__(
        self,
        *,
        host: str,
        port: int,
        response_timeout_seconds: int = 120,
        log_payload_chars: int = 1200,
        max_response_bytes: int = 16 * 1024 * 1024,
        connect_timeout_seconds: float = MAX_CONNECT_TIMEOUT_SECONDS,
        guard: Any = None,
        telemetry: Any = None,
        transport_telemetry: bool = False,
    ) -> None:
        self.host = host
        self.port = port
        self.response_timeout_seconds = response_timeout_seconds
        self.log_payload_chars = log_payload_chars
        if max_response_bytes <= 0:
            raise ValueError("max_response_bytes must be positive")
        self.max_response_bytes = max_response_bytes
        if not 0 < connect_timeout_seconds <= MAX_CONNECT_TIMEOUT_SECONDS:
            raise ValueError(
                f"connect_timeout_seconds must be in (0, {MAX_CONNECT_TIMEOUT_SECONDS}]"
            )
        self.connect_timeout_seconds = float(connect_timeout_seconds)
        self.guard = guard
        self.telemetry = telemetry
        # Stage/byte timing is deliberately opt-in. Generic begin/end telemetry
        # remains enabled for compatibility even when this is false.
        self.transport_telemetry = bool(transport_telemetry)
        self.socket: Optional[socket.socket] = None
        self.connected = False
        # The public connection object is shared by all registered MCP tools.
        # Its one-shot socket state must therefore move through connect, send,
        # receive, and close as one indivisible lifecycle.
        self._io_lock = threading.RLock()
        # ``close()`` is deliberately able to interrupt a blocking receive
        # without waiting for the lifecycle lock.  Socket publication and the
        # permanent-close bit therefore have their own tiny state lock.  Keep
        # the lock order one-way: normal request work takes _io_lock and then
        # _state_lock; close() takes only _state_lock.
        self._state_lock = threading.Lock()
        self._socket_is_pristine = False
        self._connected_at_monotonic: float | None = None
        self._permanently_closed = False

    def _check_target(self) -> Dict[str, Any]:
        if self.guard is None:
            return {
                "allowed": True,
                "would_block": False,
                "reason": "no runtime guard configured",
            }
        return self.guard.check_bridge_target(self.host, self.port)

    def _begin_telemetry(self, command: str) -> Any:
        if self.telemetry is None:
            return None
        try:
            return self.telemetry.begin("unreal_command", command)
        except Exception as telemetry_error:
            logger.warning(
                "Ignoring telemetry begin failure for '%s': %s",
                command,
                telemetry_error,
            )
            return None

    def _end_telemetry(
        self,
        token: Any,
        success: bool,
        *,
        status: str,
        error: str = "",
    ) -> None:
        if self.telemetry is not None and token is not None:
            try:
                self.telemetry.end(token, success, status=status, error=error)
            except Exception as telemetry_error:
                logger.warning("Ignoring telemetry end failure: %s", telemetry_error)

    def _begin_request_telemetry(self, command: str) -> Dict[str, Any]:
        """Create one client-owned finalization state after generic begin()."""

        token = self._begin_telemetry(command)
        timing_enabled = self.transport_telemetry and token is not None
        state: Dict[str, Any] = {
            "token": token,
            "timing_enabled": timing_enabled,
            "finished": False,
        }
        # This starts after begin() by design: a slow telemetry backend must
        # not be misreported as contention on the shared _io_lock.
        if timing_enabled:
            state["total_started"] = time.perf_counter()
            state["lock_wait_started"] = state["total_started"]
        return state

    @staticmethod
    def _stage_started(telemetry_state: Dict[str, Any]) -> Optional[float]:
        if not telemetry_state.get("timing_enabled", False):
            return None
        return time.perf_counter()

    def _record_telemetry_stage(
        self,
        telemetry_state: Dict[str, Any],
        stage: str,
        started: Optional[float],
    ) -> None:
        """Best-effort fixed-stage timing that cannot affect bridge behavior."""

        if (
            not telemetry_state.get("timing_enabled", False)
            or self.telemetry is None
            or started is None
        ):
            return
        try:
            self.telemetry.record_stage(
                telemetry_state.get("token"),
                stage,
                time.perf_counter() - started,
            )
        except Exception as telemetry_error:
            logger.warning("Ignoring telemetry stage failure for %s: %s", stage, telemetry_error)

    def _record_telemetry_bytes(
        self,
        telemetry_state: Dict[str, Any],
        *,
        request_bytes: Optional[int] = None,
        response_bytes: Optional[int] = None,
    ) -> None:
        """Best-effort exact payload facts that cannot affect bridge behavior."""

        if not telemetry_state.get("timing_enabled", False) or self.telemetry is None:
            return
        try:
            self.telemetry.record_bytes(
                telemetry_state.get("token"),
                request_bytes=request_bytes,
                response_bytes=response_bytes,
            )
        except Exception as telemetry_error:
            logger.warning("Ignoring telemetry byte-record failure: %s", telemetry_error)

    def _finish_telemetry(
        self,
        telemetry_state: Dict[str, Any],
        success: bool,
        *,
        status: str,
        error: str = "",
    ) -> None:
        if telemetry_state.get("finished", False):
            return
        telemetry_state["finished"] = True
        self._record_telemetry_stage(
            telemetry_state,
            "total",
            telemetry_state.get("total_started"),
        )
        self._end_telemetry(
            telemetry_state.get("token"),
            success,
            status=status,
            error=error,
        )

    def connect(self, timeout_seconds: Optional[int] = None) -> bool:
        """Connect to the configured Unreal Engine instance."""

        with self._io_lock:
            return self._connect_unlocked(timeout_seconds)

    def _connect_unlocked(self, timeout_seconds: Optional[int] = None) -> bool:
        """Connect while the caller owns ``_io_lock``."""

        stale_socket = self._take_socket_unlocked()
        self._close_socket(stale_socket)
        with self._state_lock:
            if self._permanently_closed:
                logger.debug("Refusing connection attempt after permanent bridge close")
                return False

        connected_socket: Optional[socket.socket] = None
        try:
            response_timeout = timeout_seconds or self.response_timeout_seconds
            connect_timeout = min(float(response_timeout), self.connect_timeout_seconds)
            guard_decision = self._check_target()
            if guard_decision.get("would_block") and self.telemetry is not None:
                try:
                    self.telemetry.record_guard_event(
                        "bridge_target",
                        f"{self.host}:{self.port}",
                        guard_decision,
                    )
                except Exception as telemetry_error:
                    logger.warning(
                        "Ignoring telemetry guard-event failure: %s",
                        telemetry_error,
                    )
            if not guard_decision.get("allowed", True):
                logger.error(
                    "Unreal bridge connection blocked by runtime guard: %s",
                    guard_decision.get("reason"),
                )
                self.connected = False
                return False

            logger.info("Connecting to Unreal at %s:%s...", self.host, self.port)
            connected_socket = socket.create_connection(
                (self.host, self.port),
                timeout=connect_timeout,
            )
            connected_socket.settimeout(response_timeout)
            connected_socket.setsockopt(socket.IPPROTO_TCP, socket.TCP_NODELAY, 1)
            connected_socket.setsockopt(socket.SOL_SOCKET, socket.SO_KEEPALIVE, 1)
            connected_socket.setsockopt(socket.SOL_SOCKET, socket.SO_RCVBUF, 65536)
            connected_socket.setsockopt(socket.SOL_SOCKET, socket.SO_SNDBUF, 65536)
            with self._state_lock:
                closed_during_connect = self._permanently_closed
                if not closed_during_connect:
                    self.socket = connected_socket
                    self.connected = True
                    # get_unreal_connection() intentionally performs eager
                    # connection health/error checking.  Preserve that behavior,
                    # but reuse this untouched socket for its first command.
                    self._socket_is_pristine = True
                    self._connected_at_monotonic = time.monotonic()
                    connected_socket = None
            if closed_during_connect:
                self._close_socket(connected_socket, shutdown=True)
                return False
            logger.info("Connected to Unreal Engine")
            return True
        except Exception as exc:
            logger.error("Failed to connect to Unreal: %s", exc)
            self._close_socket(connected_socket)
            failed_socket = self._take_socket_unlocked()
            self._close_socket(failed_socket)
            return False

    def disconnect(self) -> None:
        """Disconnect from the Unreal Engine instance, allowing later reconnects.

        This preserves the public method's historical behavior.  Server
        shutdown should use ``close()`` so an in-flight receive is interrupted
        and callers queued behind it cannot reconnect after shutdown starts.
        """

        with self._io_lock:
            self._disconnect_unlocked()

    def _disconnect_unlocked(self) -> None:
        """Disconnect while the caller owns ``_io_lock``."""

        disconnected_socket = self._take_socket_unlocked()
        self._close_socket(disconnected_socket)

    @staticmethod
    def _close_socket(sock: Optional[socket.socket], *, shutdown: bool = False) -> None:
        """Best-effort socket disposal without holding either connection lock."""

        if sock is None:
            return
        if shutdown:
            try:
                sock.shutdown(socket.SHUT_RDWR)
            except OSError:
                pass
        try:
            sock.close()
        except OSError as close_error:
            logger.debug("Ignoring socket close error: %s", close_error)

    def _take_socket_unlocked(self) -> Optional[socket.socket]:
        """Unpublish the current socket while the caller owns ``_io_lock``."""

        with self._state_lock:
            current_socket = self.socket
            self.socket = None
            self.connected = False
            self._socket_is_pristine = False
            self._connected_at_monotonic = None
            return current_socket

    def _take_socket_if_current_unlocked(
        self,
        expected_socket: Optional[socket.socket],
    ) -> Optional[socket.socket]:
        """Unpublish only the socket owned by the active one-shot request."""

        with self._state_lock:
            if self.socket is not expected_socket:
                return None
            self.socket = None
            self.connected = False
            self._socket_is_pristine = False
            self._connected_at_monotonic = None
            return expected_socket

    def _can_reuse_pristine_socket_unlocked(self) -> bool:
        """Return whether eager connection setup left one safe first-use socket."""

        with self._state_lock:
            candidate_socket = self.socket
            connected_at = self._connected_at_monotonic
            reusable = (
                not self._permanently_closed
                and self.connected
                and candidate_socket is not None
                and self._socket_is_pristine
                and connected_at is not None
                and time.monotonic() - connected_at <= PRISTINE_SOCKET_MAX_AGE_SECONDS
            )
        if not reusable:
            return False
        # A peer FIN can be observed before the first write.  Discard that
        # eager socket and reconnect before sending so its first failed write
        # does not become an avoidable unknown-outcome command.  Test doubles
        # intentionally omit fileno(), so retain their state-only behavior.
        if not isinstance(candidate_socket, socket.socket):
            return True
        try:
            readable, _, _ = select.select([candidate_socket], [], [], 0)
            if not readable:
                return True
            candidate_socket.recv(1, socket.MSG_PEEK)
            # The one-shot protocol never has unsolicited response bytes
            # before a request, so either FIN or queued bytes makes reuse
            # unsafe and triggers a clean reconnect.
            return False
        except BlockingIOError:
            return True
        except (OSError, ValueError):
            return False

    def _get_socket_for_request_unlocked(self) -> Optional[socket.socket]:
        """Claim the current fresh socket for exactly one one-shot request."""

        with self._state_lock:
            if self._permanently_closed or not self.connected or self.socket is None:
                return None
            self._socket_is_pristine = False
            return self.socket

    def close(self) -> None:
        """Permanently close this connection and interrupt an active receive.

        Unlike ``disconnect()``, this method never waits for ``_io_lock``.  It
        is safe for FastMCP lifespan shutdown: an active recv wakes promptly,
        and callers already queued on the one-shot lifecycle lock observe the
        permanent-close bit instead of opening a new socket.
        """

        with self._state_lock:
            self._permanently_closed = True
            closing_socket = self.socket
            self.socket = None
            self.connected = False
            self._socket_is_pristine = False
            self._connected_at_monotonic = None
        self._close_socket(closing_socket, shutdown=True)

    def receive_full_response(
        self,
        sock: socket.socket,
        buffer_size: int = 65536,
        timeout_seconds: Optional[int] = None,
    ) -> bytes:
        """Receive one complete top-level JSON object from Unreal."""

        chunks = []
        received_bytes = 0
        json_depth = 0
        json_started = False
        in_string = False
        escaped = False
        sock.settimeout(timeout_seconds or self.response_timeout_seconds)
        try:
            while True:
                chunk = sock.recv(buffer_size)
                if not chunk:
                    if not chunks:
                        raise ConnectionError("Connection closed before receiving data")
                    break
                chunks.append(chunk)
                received_bytes += len(chunk)
                if received_bytes > self.max_response_bytes:
                    raise ValueError(
                        "Unreal bridge response exceeded "
                        f"the {self.max_response_bytes}-byte safety limit"
                    )

                # Track object depth in bytes so large graph payloads do not
                # repeatedly reparse the complete response.
                for byte in chunk:
                    if escaped:
                        escaped = False
                        continue
                    if in_string:
                        if byte == 0x5C:  # backslash
                            escaped = True
                        elif byte == 0x22:  # quote
                            in_string = False
                        continue
                    if byte == 0x22:  # quote
                        in_string = True
                    elif byte == 0x7B:  # {
                        json_started = True
                        json_depth += 1
                    elif byte == 0x7D and json_started:  # }
                        json_depth -= 1
                        if json_depth == 0:
                            data = b"".join(chunks)
                            json.loads(data.decode("utf-8"))
                            logger.info("Received complete response (%d bytes)", len(data))
                            return data

            data = b"".join(chunks)
            json.loads(data.decode("utf-8"))
            logger.info("Received complete response after socket close (%d bytes)", len(data))
            return data
        except socket.timeout:
            logger.warning("Socket timeout during receive (%d bytes buffered)", received_bytes)
            if chunks:
                data = b"".join(chunks)
                try:
                    json.loads(data.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError):
                    timeout_error = TimeoutError(
                        f"Timeout receiving Unreal response: {received_bytes} bytes buffered "
                        "but payload is incomplete/undecodable; discarding partial data"
                    )
                    _set_response_progress(
                        timeout_error,
                        bytes_received=received_bytes,
                        complete=json_started and json_depth == 0,
                        decoded=False,
                        contract_valid=False,
                    )
                    raise timeout_error from None
                logger.warning(
                    "Accepting complete JSON response that arrived before a socket timeout (%d bytes); "
                    "the depth tracker should normally return earlier - investigate if this repeats",
                    len(data),
                )
                return data
            timeout_error = TimeoutError(
                f"Timeout receiving Unreal response after "
                f"{timeout_seconds or self.response_timeout_seconds}s (no data received)"
            )
            _set_response_progress(
                timeout_error,
                bytes_received=0,
                complete=False,
                decoded=False,
                contract_valid=False,
            )
            raise timeout_error from None
        except Exception as exc:
            _set_response_progress(
                exc,
                bytes_received=received_bytes,
                complete=json_started and json_depth == 0,
                decoded=False,
                contract_valid=False,
            )
            logger.error("Error during receive: %s", exc)
            raise

    def send_command(
        self,
        command: str,
        params: Optional[Dict[str, Any]] = None,
        timeout_seconds: Optional[int] = None,
    ) -> Optional[Dict[str, Any]]:
        """Send a command to Unreal Engine and return its decoded response."""

        telemetry_state = self._begin_request_telemetry(command)
        try:
            with self._io_lock:
                self._record_telemetry_stage(
                    telemetry_state,
                    "lock_wait",
                    telemetry_state.get("lock_wait_started"),
                )
                return self._send_command_unlocked(
                    command,
                    params,
                    timeout_seconds,
                    telemetry_state,
                )
        except BaseException as exc:
            # This covers failures before the main request try block too (for
            # example a custom pristine-socket probe). Re-raise exactly the
            # same exception after closing generic telemetry once.
            self._finish_telemetry(
                telemetry_state,
                False,
                status="exception",
                error=str(exc),
            )
            raise

    def _send_command_unlocked(
        self,
        command: str,
        params: Optional[Dict[str, Any]] = None,
        timeout_seconds: Optional[int] = None,
        telemetry_state: Optional[Dict[str, Any]] = None,
    ) -> Optional[Dict[str, Any]]:
        """Run one complete request while the caller owns ``_io_lock``."""

        if telemetry_state is None:
            telemetry_state = self._begin_request_telemetry(command)
        request_write_started = False
        request_fully_sent = False
        # A connection made by get_unreal_connection() is fresh: keep its
        # eager health/error behavior but consume it for this first command.
        # Every completed/failed command still closes its socket because the
        # native bridge retains the one-command-per-connection protocol.
        if self._can_reuse_pristine_socket_unlocked():
            # The eager pristine socket was already connected. ``connect`` is
            # deliberately omitted from stages_ms here: absent means N/A, not
            # a measured zero-duration connection attempt.
            request_socket = self._get_socket_for_request_unlocked()
        else:
            stale_socket = self._take_socket_unlocked()
            self._close_socket(stale_socket)
            connect_started = self._stage_started(telemetry_state)
            connected = self.connect(timeout_seconds=timeout_seconds)
            self._record_telemetry_stage(telemetry_state, "connect", connect_started)
            if not connected:
                logger.error("Failed to connect to Unreal Engine for command")
                self._finish_telemetry(
                    telemetry_state,
                    False,
                    status="connect_failed",
                    error="Failed to connect to Unreal Engine",
                )
                return None
            request_socket = self._get_socket_for_request_unlocked()

        if request_socket is None:
            logger.error("Failed to connect to Unreal Engine for command")
            self._finish_telemetry(
                telemetry_state,
                False,
                status="connect_failed",
                error="Unreal bridge connection is closed",
            )
            # Preserve the historical pre-write missing-socket error envelope.
            return {"status": "error", "error": "Unreal bridge connection is closed"}

        try:
            command_obj = {"type": command, "params": params or {}}
            serialize_started = self._stage_started(telemetry_state)
            try:
                command_json = json.dumps(command_obj)
                # Serialize includes the exact UTF-8 encoding used by sendall.
                command_bytes = command_json.encode("utf-8")
            finally:
                self._record_telemetry_stage(telemetry_state, "serialize", serialize_started)
            self._record_telemetry_bytes(
                telemetry_state,
                request_bytes=len(command_bytes),
            )
            logger.info(
                "Sending command '%s' (%d bytes): %s",
                command,
                len(command_bytes),
                _summarize_for_log(command_obj, self.log_payload_chars),
            )
            request_write_started = True
            send_started = self._stage_started(telemetry_state)
            try:
                request_socket.sendall(command_bytes)
            finally:
                self._record_telemetry_stage(telemetry_state, "send", send_started)
            request_fully_sent = True

            receive_started = self._stage_started(telemetry_state)
            try:
                # receive includes frame assembly plus receive_full_response's
                # first JSON/UTF-8 validation.
                response_data = self.receive_full_response(
                    request_socket,
                    timeout_seconds=timeout_seconds,
                )
            finally:
                self._record_telemetry_stage(telemetry_state, "receive", receive_started)
            response_bytes_received = len(response_data)
            self._record_telemetry_bytes(
                telemetry_state,
                response_bytes=response_bytes_received,
            )
            decode_started = self._stage_started(telemetry_state)
            try:
                try:
                    response = json.loads(response_data.decode("utf-8"))
                except (UnicodeDecodeError, json.JSONDecodeError) as response_error:
                    _set_response_progress(
                        response_error,
                        bytes_received=response_bytes_received,
                        complete=True,
                        decoded=False,
                        contract_valid=False,
                    )
                    raise
                if not isinstance(response, dict):
                    response_error = ValueError("Unreal bridge response must be a JSON object")
                    _set_response_progress(
                        response_error,
                        bytes_received=response_bytes_received,
                        complete=True,
                        decoded=True,
                        contract_valid=False,
                    )
                    raise response_error
                status_present = "status" in response
                success_present = "success" in response
                status_value = response.get("status")
                success_value = response.get("success")
                response_contract_valid = (
                    (status_present or success_present)
                    and (
                        not status_present
                        or (isinstance(status_value, str) and bool(status_value.strip()))
                    )
                    and (not success_present or isinstance(success_value, bool))
                )
                if not response_contract_valid:
                    response_error = ValueError(
                        "Unreal bridge response has missing or invalid status/success contract fields"
                    )
                    _set_response_progress(
                        response_error,
                        bytes_received=response_bytes_received,
                        complete=True,
                        decoded=True,
                        contract_valid=False,
                    )
                    raise response_error
            finally:
                # decode is the second decode plus type/status/success contract
                # validation after receive_full_response's first validation.
                self._record_telemetry_stage(telemetry_state, "decode", decode_started)
            logger.info(
                "Complete response from Unreal for '%s': %s",
                command,
                _summarize_for_log(response, self.log_payload_chars),
            )

            if response.get("status") == "error":
                error_message = response.get("error") or response.get(
                    "message",
                    "Unknown Unreal error",
                )
                logger.error("Unreal error (status=error): %s", error_message)
                if "error" not in response:
                    response["error"] = error_message
            elif response.get("success") is False:
                error_message = response.get("error") or response.get(
                    "message",
                    "Unknown Unreal error",
                )
                logger.error("Unreal error (success=false): %s", error_message)
                response.setdefault("status", "error")
                response.setdefault("error", error_message)

            completed_socket = self._take_socket_if_current_unlocked(request_socket)
            self._close_socket(completed_socket)

            success = response.get("status") != "error" and response.get("success") is not False
            self._finish_telemetry(
                telemetry_state,
                success,
                status=str(response.get("status") or ("ok" if success else "error")),
                error=str(response.get("error") or response.get("message") or ""),
            )
            return response
        except Exception as exc:
            logger.error("Error sending command: %s", exc)
            failed_socket = self._take_socket_if_current_unlocked(request_socket)
            self._close_socket(failed_socket)
            response_bytes_received = getattr(exc, "unreal_bridge_response_bytes_received", None)
            if isinstance(response_bytes_received, int) and response_bytes_received >= 0:
                self._record_telemetry_bytes(
                    telemetry_state,
                    response_bytes=response_bytes_received,
                )
            self._finish_telemetry(
                telemetry_state,
                False,
                status="exception",
                error=str(exc),
            )
            if request_write_started:
                marker = _unknown_command_outcome_marker(
                    exc,
                    request_write_started=request_write_started,
                    request_fully_sent=request_fully_sent,
                )
                logger.warning(
                    "Bridge failure after write start for '%s'; outcome is unknown and retry needs effect classification: %s",
                    command,
                    marker["failure_kind"],
                )
                return {
                    "status": "error",
                    "error": str(exc),
                    "transport": marker,
                }
            return {"status": "error", "error": str(exc)}
