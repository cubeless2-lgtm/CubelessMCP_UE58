#include "CubelessNiagaraEventStageAttachmentLibrary.h"
#include "Registration/CubelessNiagaraEventStageToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraEventStageAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraEventStageAttachmentReceipt(UClass* ToolsetClass, FNiagaraEventStageAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraEventStageAttachmentLibrary::InstallNiagaraEventStageToolset(UClass* ToolsetClass)
{
    return NiagaraEventStageAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraEventStageToolset, TEXT("install_niagara_event_stage_authoring_toolset"));
}
FString UCubelessNiagaraEventStageAttachmentLibrary::VerifyNiagaraEventStageToolset(UClass* ToolsetClass)
{
    return NiagaraEventStageAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraEventStageToolset, TEXT("verify_niagara_event_stage_authoring_toolset"));
}
FString UCubelessNiagaraEventStageAttachmentLibrary::RemoveNiagaraEventStageToolset(UClass* ToolsetClass)
{
    return NiagaraEventStageAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraEventStageToolset, TEXT("remove_niagara_event_stage_authoring_toolset"));
}
