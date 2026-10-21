#include "CubelessNiagaraCustomHlslConversionAttachmentLibrary.h"
#include "Registration/CubelessNiagaraCustomHlslConversionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraCustomHlslConversionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraCustomHlslConversionAttachmentReceipt(UClass* ToolsetClass, FNiagaraCustomHlslConversionAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraCustomHlslConversionAttachmentLibrary::InstallNiagaraCustomHlslConversionToolset(UClass* ToolsetClass)
{
    return NiagaraCustomHlslConversionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraCustomHlslConversionToolset, TEXT("install_niagara_custom_hlsl_conversion_toolset"));
}
FString UCubelessNiagaraCustomHlslConversionAttachmentLibrary::VerifyNiagaraCustomHlslConversionToolset(UClass* ToolsetClass)
{
    return NiagaraCustomHlslConversionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraCustomHlslConversionToolset, TEXT("verify_niagara_custom_hlsl_conversion_toolset"));
}
FString UCubelessNiagaraCustomHlslConversionAttachmentLibrary::RemoveNiagaraCustomHlslConversionToolset(UClass* ToolsetClass)
{
    return NiagaraCustomHlslConversionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraCustomHlslConversionToolset, TEXT("remove_niagara_custom_hlsl_conversion_toolset"));
}
