#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"
namespace Cubeless::Operations
{
FString EncodeFunctionResult(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("operation"), Operation); J->SetBoolField(TEXT("success"), R.bSuccess);
    J->SetStringField(TEXT("stage"), R.Stage); J->SetStringField(TEXT("error_code"), R.ErrorCode); J->SetStringField(TEXT("error"), R.Error);
    J->SetBoolField(TEXT("mutation_attempted"), R.bMutationAttempted); J->SetBoolField(TEXT("change_state_known"), R.bChangeStateKnown);
    if (R.bChangeStateKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPostconditionPassed); J->SetBoolField(TEXT("compile_requested"), false);
    J->SetBoolField(TEXT("compile_attempted"), false); J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    J->SetBoolField(TEXT("save_requested"), false); J->SetBoolField(TEXT("save_attempted"), false); J->SetBoolField(TEXT("saved"), false);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false); J->SetObjectField(TEXT("data"), R.Data);
    FString Text; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&Text)); return Text;
}
}
