#include "Operations/CubelessNiagaraModuleOperations.h"
#include "Compatibility/UE58/CubelessNiagaraModuleApi.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString Encode(const UE58::Function::FResult& R, const TCHAR* Operation)
{
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
FString InspectNiagaraStockModuleTarget(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName)
{
    return Encode(UE58::NiagaraModule::Inspect(SystemPath, ModuleScriptPath, TargetUsage, TargetUsageId,
        TargetEmitterIndexJson, TargetEmitterName), TEXT("inspect_niagara_stock_module_target"));
}
FString AddNiagaraStockModuleToStack(const FString& SystemPath, const FString& ModuleScriptPath,
    const FString& TargetUsage, const FString& TargetUsageId, const FString& TargetEmitterIndexJson,
    const FString& TargetEmitterName, const FString& TargetIndexJson, const FString& SuggestedName,
    bool bSkipIfDuplicate, bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Encode(UE58::NiagaraModule::Add(SystemPath, ModuleScriptPath, TargetUsage, TargetUsageId,
        TargetEmitterIndexJson, TargetEmitterName, TargetIndexJson, SuggestedName,
        bSkipIfDuplicate, bAllowSourceEdit, bSave, bRequestCompile), TEXT("add_niagara_stock_module_to_stack"));
}
}
