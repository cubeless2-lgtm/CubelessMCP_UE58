#include "CubelessNiagaraPreviewLabAttachmentLibrary.h"
#include "Registration/CubelessNiagaraPreviewLabToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraPreviewLabAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraPreviewLabAttachmentReceipt(UClass* ToolsetClass, FNiagaraPreviewLabAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraPreviewLabAttachmentLibrary::InstallNiagaraPreviewLabToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewLabAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraPreviewLabToolset, TEXT("install_niagara_preview_lab_toolset"));
}
FString UCubelessNiagaraPreviewLabAttachmentLibrary::VerifyNiagaraPreviewLabToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewLabAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraPreviewLabToolset, TEXT("verify_niagara_preview_lab_toolset"));
}
FString UCubelessNiagaraPreviewLabAttachmentLibrary::RemoveNiagaraPreviewLabToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewLabAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraPreviewLabToolset, TEXT("remove_niagara_preview_lab_toolset"));
}
