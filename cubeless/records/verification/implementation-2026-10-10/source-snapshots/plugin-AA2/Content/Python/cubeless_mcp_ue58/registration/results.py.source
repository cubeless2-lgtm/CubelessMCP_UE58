"""Reflected tool result; nested variable records are explicitly JSON strings."""

import json
import unreal


@unreal.ustruct()
class CubelessPythonOperationResult(unreal.StructBase):
    """Editor operation outcome, including partial changes and persistence state."""

    success = unreal.uproperty(bool)
    operation_id = unreal.uproperty(str)
    stage = unreal.uproperty(str)
    error_code = unreal.uproperty(str)
    error = unreal.uproperty(str)
    changes_json = unreal.uproperty(str, meta={"ToolTip": "JSON array of attempted and observed changes; no atomic rollback."})
    compile_json = unreal.uproperty(str, meta={"ToolTip": "JSON object describing requested/attempted compile, status and diagnostics."})
    save_json = unreal.uproperty(str, meta={"ToolTip": "JSON object describing requested/attempted save and saved state."})
    validation_json = unreal.uproperty(str, meta={"ToolTip": "JSON object describing performed checks and missing validation, including reload."})
    retry_json = unreal.uproperty(str, meta={"ToolTip": "JSON object describing retry policy, duplicate risk and request replay."})
    data_json = unreal.uproperty(str, meta={"ToolTip": "JSON object containing operation-specific output and legacy contract fields."})


def reflected_result(record):
    result = CubelessPythonOperationResult()
    for name in ("success", "operation_id", "stage", "error_code", "error"):
        setattr(result, name, record[name])
    for name in ("changes", "compile", "save", "validation", "retry", "data"):
        setattr(result, name + "_json", json.dumps(record[name], ensure_ascii=False, sort_keys=True))
    return result
