#include "CubelessNiagaraEmitterConfigurationAttachmentLibrary.h"
#include "Registration/CubelessNiagaraEmitterConfigurationToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraEmitterConfigurationAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraEmitterConfigurationAttachmentReceipt(UClass* ToolsetClass, FNiagaraEmitterConfigurationAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraEmitterConfigurationAttachmentLibrary::InstallNiagaraEmitterConfigurationToolset(UClass* ToolsetClass)
{
    return NiagaraEmitterConfigurationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraEmitterConfigurationToolset, TEXT("install_niagara_emitter_configuration_toolset"));
}
FString UCubelessNiagaraEmitterConfigurationAttachmentLibrary::VerifyNiagaraEmitterConfigurationToolset(UClass* ToolsetClass)
{
    return NiagaraEmitterConfigurationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraEmitterConfigurationToolset, TEXT("verify_niagara_emitter_configuration_toolset"));
}
FString UCubelessNiagaraEmitterConfigurationAttachmentLibrary::RemoveNiagaraEmitterConfigurationToolset(UClass* ToolsetClass)
{
    return NiagaraEmitterConfigurationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraEmitterConfigurationToolset, TEXT("remove_niagara_emitter_configuration_toolset"));
}
