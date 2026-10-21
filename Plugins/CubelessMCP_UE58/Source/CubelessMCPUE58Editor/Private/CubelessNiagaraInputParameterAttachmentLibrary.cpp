#include "CubelessNiagaraInputParameterAttachmentLibrary.h"
#include "Registration/CubelessNiagaraInputParameterToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraInputParameterAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraInputParameterAttachmentReceipt(UClass* ToolsetClass, FNiagaraInputParameterAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraInputParameterAttachmentLibrary::InstallNiagaraInputParameterToolset(UClass* ToolsetClass)
{
    return NiagaraInputParameterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraInputParameterToolset, TEXT("install_niagara_input_parameters_toolset"));
}
FString UCubelessNiagaraInputParameterAttachmentLibrary::VerifyNiagaraInputParameterToolset(UClass* ToolsetClass)
{
    return NiagaraInputParameterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraInputParameterToolset, TEXT("verify_niagara_input_parameters_toolset"));
}
FString UCubelessNiagaraInputParameterAttachmentLibrary::RemoveNiagaraInputParameterToolset(UClass* ToolsetClass)
{
    return NiagaraInputParameterAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraInputParameterToolset, TEXT("remove_niagara_input_parameters_toolset"));
}
