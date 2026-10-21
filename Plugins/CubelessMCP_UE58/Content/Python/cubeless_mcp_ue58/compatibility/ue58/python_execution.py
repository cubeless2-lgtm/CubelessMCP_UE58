"""UP synchronous internal Python execution seam; no socket, ticker wait or replay.

The public wrapper accepts only the original caller5; the raw command also
accepts its original two native-only options. The standalone Raw tool owns its
retained next-tick dispatch; this helper remains a synchronous nested caller.
"""
import json
import unreal
from ...operations.state import OperationError

LIBRARY = "CubelessPythonExecutionLibrary"


def execute_python(code, mode="ExecuteStatement", defer_to_ticker=False,
                   allow_unsafe_editor_scripting_during_pie=False,
                   interchange_policy="auto", transaction_policy="command",
                   allow_unsafe_pcg_selector_python=False):
    fn = getattr(getattr(unreal, LIBRARY, None), "execute_python", None)
    if not callable(fn):
        error = OperationError("python_execution_native_api_unavailable", "Build/load the dedicated Python execution core; no fallback")
        error.native_call_dispatched = False
        raise error
    try:
        # Raw command-only options are supplied by its distinct seven-field action.
        raw = fn(code, mode, defer_to_ticker, allow_unsafe_editor_scripting_during_pie,
                 interchange_policy, transaction_policy, allow_unsafe_pcg_selector_python)
    finally:
        # Do not retain a reflected callable in persistent script globals.
        fn = None
    try:
        if type(raw) is not str:
            raise ValueError("Native execution must return a JSON string")
        actual = json.loads(raw)
        if type(actual) is not dict:
            raise ValueError("Native execution must return an object")
        for field in ("success", "python_executed", "mutation_attempted", "change_state_known", "mutation_state_unknown"):
            if type(actual.get(field)) is not bool:
                raise ValueError("Missing Boolean receipt: " + field)
        if "mutation_performed" not in actual or actual["mutation_performed"] is not None and type(actual["mutation_performed"]) is not bool:
            raise ValueError("Native mutation_performed must be Boolean or null")
    except Exception as cause:
        error = OperationError("python_execution_native_result_invalid", "Python execution returned an invalid receipt; inspect raw output before any new operation: " + str(cause))
        error.native_call_returned = True
        error.native_raw_result = raw
        raise error from cause
    return actual, raw


def execute_unreal_python(code, mode="ExecuteFile", defer_to_ticker=False,
                          allow_unsafe_editor_scripting_during_pie=False,
                          interchange_policy="auto"):
    return execute_python(code, mode, defer_to_ticker, allow_unsafe_editor_scripting_during_pie,
                          interchange_policy, "command", False)
