#include "CubelessNiagaraSystemAnalysisAttachmentLibrary.h"
#include "Registration/CubelessNiagaraSystemAnalysisToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraSystemAnalysisAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraSystemAnalysisAttachmentReceipt(UClass* ToolsetClass, FNiagaraSystemAnalysisAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraSystemAnalysisAttachmentLibrary::InstallNiagaraSystemAnalysisToolset(UClass* ToolsetClass)
{
    return NiagaraSystemAnalysisAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraSystemAnalysisToolset, TEXT("install_niagara_system_analysis_toolset"));
}
FString UCubelessNiagaraSystemAnalysisAttachmentLibrary::VerifyNiagaraSystemAnalysisToolset(UClass* ToolsetClass)
{
    return NiagaraSystemAnalysisAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraSystemAnalysisToolset, TEXT("verify_niagara_system_analysis_toolset"));
}
FString UCubelessNiagaraSystemAnalysisAttachmentLibrary::RemoveNiagaraSystemAnalysisToolset(UClass* ToolsetClass)
{
    return NiagaraSystemAnalysisAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraSystemAnalysisToolset, TEXT("remove_niagara_system_analysis_toolset"));
}
