#include "CubelessNiagaraPreviewWorkflowAttachmentLibrary.h"
#include "Registration/CubelessNiagaraPreviewWorkflowToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraPreviewWorkflowAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraPreviewWorkflowAttachmentReceipt(UClass* ToolsetClass, FNiagaraPreviewWorkflowAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraPreviewWorkflowAttachmentLibrary::InstallNiagaraPreviewWorkflowToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewWorkflowAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraPreviewWorkflowToolset, TEXT("install_niagara_preview_workflows_toolset"));
}
FString UCubelessNiagaraPreviewWorkflowAttachmentLibrary::VerifyNiagaraPreviewWorkflowToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewWorkflowAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraPreviewWorkflowToolset, TEXT("verify_niagara_preview_workflows_toolset"));
}
FString UCubelessNiagaraPreviewWorkflowAttachmentLibrary::RemoveNiagaraPreviewWorkflowToolset(UClass* ToolsetClass)
{
    return NiagaraPreviewWorkflowAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraPreviewWorkflowToolset, TEXT("remove_niagara_preview_workflows_toolset"));
}
