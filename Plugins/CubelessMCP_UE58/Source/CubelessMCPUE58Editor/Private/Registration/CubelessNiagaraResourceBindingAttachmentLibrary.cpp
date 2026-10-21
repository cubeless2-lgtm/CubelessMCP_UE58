#include "CubelessNiagaraResourceBindingAttachmentLibrary.h"
#include "Registration/CubelessNiagaraResourceBindingToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraResourceBindingAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraResourceBindingAttachmentReceipt(UClass* ToolsetClass, FNiagaraResourceBindingAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraResourceBindingAttachmentLibrary::InstallNiagaraResourceBindingToolset(UClass* ToolsetClass)
{
    return NiagaraResourceBindingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraResourceBindingToolset, TEXT("install_niagara_resource_bindings_toolset"));
}
FString UCubelessNiagaraResourceBindingAttachmentLibrary::VerifyNiagaraResourceBindingToolset(UClass* ToolsetClass)
{
    return NiagaraResourceBindingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraResourceBindingToolset, TEXT("verify_niagara_resource_bindings_toolset"));
}
FString UCubelessNiagaraResourceBindingAttachmentLibrary::RemoveNiagaraResourceBindingToolset(UClass* ToolsetClass)
{
    return NiagaraResourceBindingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraResourceBindingToolset, TEXT("remove_niagara_resource_bindings_toolset"));
}
