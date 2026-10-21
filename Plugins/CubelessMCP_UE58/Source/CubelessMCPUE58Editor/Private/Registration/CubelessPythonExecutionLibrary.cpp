#include "Registration/CubelessPythonExecutionLibrary.h"

#include "Compatibility/UE58/CubelessPythonExecutionApi.h"
#include "Serialization/JsonSerializer.h"
#include "Policies/CondensedJsonPrintPolicy.h"

FString UCubelessPythonExecutionLibrary::ExecutePython(const FString& Code, const FString& Mode,
    bool bDeferToTicker, bool bAllowUnsafeEditorScriptingDuringPIE,
    const FString& InterchangePolicy, const FString& TransactionPolicy, bool bAllowUnsafePCGSelectorPython)
{
    Cubeless::UE58::PythonExecution::FRequest Request;
    Request.Code = Code; Request.Mode = Mode; Request.DeferToTicker = bDeferToTicker;
    Request.AllowUnsafeEditorScriptingDuringPIE = bAllowUnsafeEditorScriptingDuringPIE;
    Request.InterchangePolicy = InterchangePolicy.TrimStartAndEnd().ToLower();
    Request.TransactionPolicy = TransactionPolicy; Request.AllowUnsafePCGSelectorPython = bAllowUnsafePCGSelectorPython;
    Cubeless::UE58::PythonExecution::FDispatch Dispatch;
    // A synchronous caller must never block waiting for the ticker. Import/defer is
    // rejected in the same core before any transaction/CVar/Python execution.
    Dispatch.SynchronousNested = true; Dispatch.IsCurrent = []() { return true; };
    auto Actual = Cubeless::UE58::PythonExecution::Execute(Request, Dispatch);
    FString Output;
    FJsonSerializer::Serialize(Actual, TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Output));
    return Output;
}
