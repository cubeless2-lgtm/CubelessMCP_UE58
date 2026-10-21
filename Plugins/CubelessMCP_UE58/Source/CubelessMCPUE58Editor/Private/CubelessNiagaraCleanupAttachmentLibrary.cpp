#include "CubelessNiagaraCleanupAttachmentLibrary.h"
#include "Registration/CubelessNiagaraCleanupToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraCleanupAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraCleanupAttachmentReceipt(UClass* ToolsetClass, FNiagaraCleanupAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraCleanupAttachmentLibrary::InstallNiagaraCleanupToolset(UClass* ToolsetClass)
{
    return NiagaraCleanupAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraCleanupToolset, TEXT("install_niagara_cleanup_toolset"));
}
FString UCubelessNiagaraCleanupAttachmentLibrary::VerifyNiagaraCleanupToolset(UClass* ToolsetClass)
{
    return NiagaraCleanupAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraCleanupToolset, TEXT("verify_niagara_cleanup_toolset"));
}
FString UCubelessNiagaraCleanupAttachmentLibrary::RemoveNiagaraCleanupToolset(UClass* ToolsetClass)
{
    return NiagaraCleanupAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraCleanupToolset, TEXT("remove_niagara_cleanup_toolset"));
}
