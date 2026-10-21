#include "Operations/CubelessNiagaraParameterOperations.h"
#include "Compatibility/UE58/CubelessNiagaraParameterApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    // Reuse the common envelope while preserving this operation's real async
    // compile/save flags; the graph-only encoder supplies false placeholders.
    const FString Base = EncodeFunctionResult(R, Operation);
    TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base), J) || !J.IsValid()) { return Base; }
    constexpr const TCHAR* Flags[] = {TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"),
        TEXT("save_attempted"), TEXT("saved"), TEXT("save_deferred")};
    for (const TCHAR* Flag : Flags)
    {
        bool Value = false;
        if (R.Data->TryGetBoolField(Flag, Value)) { J->SetBoolField(Flag, Value); }
    }
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    FString Out; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString InspectNiagaraUserParameters(const FString& SystemPath)
{ return Encode(UE58::NiagaraParameter::Inspect(SystemPath), TEXT("inspect_niagara_user_parameters")); }
FString SetNiagaraUserParameter(const FString& SystemPath, const FString& ParameterName, const FString& ValueJson,
    bool bAllowSourceEdit, bool bSave)
{ return Encode(UE58::NiagaraParameter::Set(SystemPath, ParameterName, ValueJson, bAllowSourceEdit, bSave), TEXT("set_niagara_user_parameter")); }
FString UpsertNiagaraUserParameters(const FString& SystemPath, const FString& ParametersJson, bool bContinueOnError,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{ return Encode(UE58::NiagaraParameter::Upsert(SystemPath, ParametersJson, bContinueOnError, bAllowSourceEdit, bSave, bRequestCompile), TEXT("upsert_niagara_user_parameters")); }
}
