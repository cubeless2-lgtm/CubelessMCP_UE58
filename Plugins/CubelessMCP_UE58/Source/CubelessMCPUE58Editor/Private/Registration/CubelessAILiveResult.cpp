#include "Registration/CubelessAILiveResult.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"

bool UCubelessAILiveResult::SetValue(const FString& TerminalReportJson)
{
    if (!IsInGameThread()) { return SetError(TEXT("AI terminal completion requires the Editor game thread.")); }
    FJsonObjectWrapper Parsed;
    if (TerminalReportJson.Len() > 1048576 || !Parsed.JsonObjectFromString(TerminalReportJson) || !Parsed.JsonObject.IsValid())
    { return SetError(TEXT("Fixed AI terminal report is not a bounded JSON object.")); }
    const auto J = Parsed.JsonObject; FString Operation, Status, RunId, ReportPath; bool Success = false, Terminal = false;
    const TSharedPtr<FJsonObject>* Config = nullptr; const TSharedPtr<FJsonObject>* Summary = nullptr; const TSharedPtr<FJsonObject>* Evidence = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* Steps = nullptr;
    if (!J->TryGetStringField(TEXT("operation"), Operation)
        || (Operation != TEXT("run_ai_dummy_patrol_chase_smoke") && Operation != TEXT("run_ai_behavior_tree_execution_smoke"))
        || !J->TryGetBoolField(TEXT("success"), Success) || !J->TryGetBoolField(TEXT("workflow_finished"), Terminal) || !Terminal
        || !J->TryGetStringField(TEXT("status"), Status) || (Status != TEXT("PASS") && Status != TEXT("FAIL") && Status != TEXT("BLOCKED"))
        || (Success != (Status == TEXT("PASS"))) || !J->TryGetStringField(TEXT("run_id"), RunId) || RunId.IsEmpty() || RunId.Len() > 96
        || !J->TryGetStringField(TEXT("report_path"), ReportPath) || !J->TryGetObjectField(TEXT("config"), Config)
        || !J->TryGetObjectField(TEXT("summary"), Summary) || !J->TryGetObjectField(TEXT("evidence_standard"), Evidence)
        || !J->TryGetArrayField(TEXT("steps"), Steps) || Steps->Num() > 128)
    { return SetError(TEXT("Only complete reports for the two fixed original AI workflows are admitted.")); }
    return MaybeBroadcastSuccessfulCompletion(MoveTemp(Parsed), Value);
}
