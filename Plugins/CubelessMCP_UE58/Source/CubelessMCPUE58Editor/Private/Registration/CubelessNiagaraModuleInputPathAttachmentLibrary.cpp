#include "CubelessNiagaraModuleInputPathAttachmentLibrary.h"
#include "Registration/CubelessNiagaraModuleInputPathToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraModuleInputPathAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraModuleInputPathAttachmentReceipt(UClass* ToolsetClass, FNiagaraModuleInputPathAttachment Function, const TCHAR* Operation)
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    const bool Ok = Function(ToolsetClass, Report, Error);
    Report->SetStringField(TEXT("operation"), Operation);
    // Admission-only Verify deliberately returns false; do not promote it to general readiness.
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok);
    Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false);
    FString Out; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString UCubelessNiagaraModuleInputPathAttachmentLibrary::InstallNiagaraModuleInputPathToolset(UClass* ToolsetClass)
{
    return NiagaraModuleInputPathAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraModuleInputPathToolset, TEXT("install_niagara_module_input_paths_toolset"));
}
FString UCubelessNiagaraModuleInputPathAttachmentLibrary::VerifyNiagaraModuleInputPathToolset(UClass* ToolsetClass)
{
    return NiagaraModuleInputPathAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraModuleInputPathToolset, TEXT("verify_niagara_module_input_paths_toolset"));
}
FString UCubelessNiagaraModuleInputPathAttachmentLibrary::RemoveNiagaraModuleInputPathToolset(UClass* ToolsetClass)
{
    return NiagaraModuleInputPathAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraModuleInputPathToolset, TEXT("remove_niagara_module_input_paths_toolset"));
}
