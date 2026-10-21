"""Official JSON-string IO adapter; nested domain contracts remain intact."""

import json

from . import _CONTEXT
from .response_schema import tool_error


def _unique_object(pairs):
    result = {}
    for key, value in pairs:
        if key in result:
            raise ValueError("duplicate JSON object key: " + key)
        result[key] = value
    return result


def _reject_constant(value):
    raise ValueError("non-finite JSON value: " + value)


def call_json(function, scalar_arguments, json_arguments):
    """Decode known named arguments only; no engine dispatch or arbitrary code."""
    try:
        values = dict(scalar_arguments)
        for name, text in json_arguments.items():
            if text is None:
                values[name] = None
            elif isinstance(text, str):
                values[name] = json.loads(text, object_pairs_hook=_unique_object,
                    parse_constant=_reject_constant)
            else:
                raise ValueError(name + " must be a JSON string or null")
    except (TypeError, ValueError) as error:
        response = tool_error(str(error), error_code="niagara_planning_json_invalid",
            dry_run=True, mutation_performed=False, writes_assets=False,
            exception_type=type(error).__name__)
        response["migration_context"] = dict(_CONTEXT)
        return response
    return function(**values)


def operation_record(response):
    """Wrap full legacy response for the existing reflected operation struct."""
    success = response.get("success") is True
    return {
        "success": success, "operation_id": "", "stage": "completed" if success else "validation",
        "error_code": response.get("error_code", ""),
        "error": response.get("error", response.get("message", "")),
        "changes": [],
        "compile": {"requested": False, "attempted": False, "status": "not_requested", "input_evidence_only": True},
        "save": {"requested": False, "attempted": False, "saved": False},
        "validation": {"status": "pure_contract_evaluated", "engine_state_validation": "not_run",
            "disk_reload": "not_applicable", "packaged_runtime": "not_applicable"},
        "retry": {"automatic_retry": False, "duplicate_risk": "none_pure_computation",
            "atomic_rollback": False, "replayed": False, "journal_written": False},
        "data": response,
    }
