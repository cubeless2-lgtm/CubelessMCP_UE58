#include "CubelessNiagaraRendererAttachmentLibrary.h"
#include "Registration/CubelessNiagaraRendererToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraRendererAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraRendererAttachmentReceipt(UClass* ToolsetClass, FNiagaraRendererAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraRendererAttachmentLibrary::InstallNiagaraRendererToolset(UClass* ToolsetClass)
{
    return NiagaraRendererAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraRendererToolset, TEXT("install_niagara_renderers_toolset"));
}
FString UCubelessNiagaraRendererAttachmentLibrary::VerifyNiagaraRendererToolset(UClass* ToolsetClass)
{
    return NiagaraRendererAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraRendererToolset, TEXT("verify_niagara_renderers_toolset"));
}
FString UCubelessNiagaraRendererAttachmentLibrary::RemoveNiagaraRendererToolset(UClass* ToolsetClass)
{
    return NiagaraRendererAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraRendererToolset, TEXT("remove_niagara_renderers_toolset"));
}
