#include "CubelessNiagaraAuthoringInspectionAttachmentLibrary.h"
#include "Registration/CubelessNiagaraAuthoringInspectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraAuthoringInspectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraAuthoringInspectionAttachmentReceipt(UClass* ToolsetClass, FNiagaraAuthoringInspectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraAuthoringInspectionAttachmentLibrary::InstallNiagaraAuthoringInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraAuthoringInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraAuthoringInspectionToolset, TEXT("install_niagara_authoring_inspection_toolset"));
}
FString UCubelessNiagaraAuthoringInspectionAttachmentLibrary::VerifyNiagaraAuthoringInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraAuthoringInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraAuthoringInspectionToolset, TEXT("verify_niagara_authoring_inspection_toolset"));
}
FString UCubelessNiagaraAuthoringInspectionAttachmentLibrary::RemoveNiagaraAuthoringInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraAuthoringInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraAuthoringInspectionToolset, TEXT("remove_niagara_authoring_inspection_toolset"));
}
