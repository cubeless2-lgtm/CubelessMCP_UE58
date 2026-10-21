#include "CubelessNiagaraRuntimeInspectionAttachmentLibrary.h"
#include "Registration/CubelessNiagaraRuntimeInspectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraRuntimeInspectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraRuntimeInspectionAttachmentReceipt(UClass* ToolsetClass, FNiagaraRuntimeInspectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraRuntimeInspectionAttachmentLibrary::InstallNiagaraRuntimeInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraRuntimeInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraRuntimeInspectionToolset, TEXT("install_niagara_runtime_inspection_toolset"));
}
FString UCubelessNiagaraRuntimeInspectionAttachmentLibrary::VerifyNiagaraRuntimeInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraRuntimeInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraRuntimeInspectionToolset, TEXT("verify_niagara_runtime_inspection_toolset"));
}
FString UCubelessNiagaraRuntimeInspectionAttachmentLibrary::RemoveNiagaraRuntimeInspectionToolset(UClass* ToolsetClass)
{
    return NiagaraRuntimeInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraRuntimeInspectionToolset, TEXT("remove_niagara_runtime_inspection_toolset"));
}
