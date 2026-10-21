#include "Operations/CubelessNiagaraEmitterOperations.h"
#include "Compatibility/UE58/CubelessNiagaraEmitterApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
    const FString Base = EncodeFunctionResult(R, Operation); TSharedPtr<FJsonObject> J;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Base), J) || !J.IsValid()) { return Base; }
    constexpr const TCHAR* Flags[] = {TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"),
        TEXT("save_attempted"), TEXT("saved"), TEXT("save_deferred")};
    for (const TCHAR* Flag : Flags) { bool Value = false; if (R.Data->TryGetBoolField(Flag, Value)) { J->SetBoolField(Flag, Value); } }
    J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    FString Out; FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString InspectNiagaraEmitterAttachTarget(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName)
{
    return Encode(UE58::NiagaraEmitter::Inspect(TargetSystemPath, SourceEmitterPath, SourceSystemPath,
        SourceEmitterIndexJson, SourceEmitterName, NewEmitterName), TEXT("inspect_niagara_emitter_attach_target"));
}
FString DuplicateOrAttachEmitterFromSource(const FString& TargetSystemPath, const FString& SourceEmitterPath,
    const FString& SourceSystemPath, const FString& SourceEmitterIndexJson,
    const FString& SourceEmitterName, const FString& NewEmitterName, const FString& EnabledJson,
    bool bCopySourceUserParameters, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraEmitter::Attach(TargetSystemPath, SourceEmitterPath, SourceSystemPath,
        SourceEmitterIndexJson, SourceEmitterName, NewEmitterName, EnabledJson, bCopySourceUserParameters,
        bAllowSourceEdit, bSave, bRequestCompile), TEXT("duplicate_or_attach_emitter_from_source"));
}
}
