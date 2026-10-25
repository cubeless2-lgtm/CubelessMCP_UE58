"""Pure receipt helpers shared by production callers and historical validators."""
import json
from official_mcp_client import tool_payload


def canonical(name):
    return name.replace('_', '').lower()


def require(value, message):
    if not value:
        raise AssertionError(message)


def success(value):
    require(isinstance(value, dict) and value.get('success') is True, 'Operation failed: ' + str(value))
    require(value.get('error_code', '') == '' and value.get('error', '') == '', 'Success contradicts error fields')


def operation_result(raw):
    value = tool_payload(raw)
    if isinstance(value, str):
        value = json.loads(value)
    if not isinstance(value, dict):
        return value
    mapped = {canonical(k): v for k, v in value.items()}
    if 'changesjson' not in mapped:
        return value
    result = {k: mapped[canonical(k)] for k in ('success', 'operation_id', 'stage', 'error_code', 'error')}
    result.update({k: json.loads(mapped[k + 'json']) for k in ('changes', 'compile', 'save', 'validation', 'retry', 'data')})
    return result
