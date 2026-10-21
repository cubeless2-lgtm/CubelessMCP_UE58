#include "CubelessNiagaraDataChannelAuthoringAttachmentLibrary.h"
#include "Registration/CubelessNiagaraDataChannelAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraDataChannelAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraDataChannelAuthoringAttachmentReceipt(UClass* ToolsetClass, FNiagaraDataChannelAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraDataChannelAuthoringAttachmentLibrary::InstallNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraDataChannelAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraDataChannelAuthoringToolset, TEXT("install_niagara_data_channel_authoring_toolset"));
}
FString UCubelessNiagaraDataChannelAuthoringAttachmentLibrary::VerifyNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraDataChannelAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraDataChannelAuthoringToolset, TEXT("verify_niagara_data_channel_authoring_toolset"));
}
FString UCubelessNiagaraDataChannelAuthoringAttachmentLibrary::RemoveNiagaraDataChannelAuthoringToolset(UClass* ToolsetClass)
{
    return NiagaraDataChannelAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraDataChannelAuthoringToolset, TEXT("remove_niagara_data_channel_authoring_toolset"));
}
