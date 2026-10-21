#include "CubelessNiagaraParameterCollectionAttachmentLibrary.h"
#include "Registration/CubelessNiagaraParameterCollectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraParameterCollectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraParameterCollectionAttachmentReceipt(UClass* ToolsetClass, FNiagaraParameterCollectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraParameterCollectionAttachmentLibrary::InstallNiagaraParameterCollectionToolset(UClass* ToolsetClass)
{
    return NiagaraParameterCollectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraParameterCollectionToolset, TEXT("install_niagara_parameter_collections_toolset"));
}
FString UCubelessNiagaraParameterCollectionAttachmentLibrary::VerifyNiagaraParameterCollectionToolset(UClass* ToolsetClass)
{
    return NiagaraParameterCollectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraParameterCollectionToolset, TEXT("verify_niagara_parameter_collections_toolset"));
}
FString UCubelessNiagaraParameterCollectionAttachmentLibrary::RemoveNiagaraParameterCollectionToolset(UClass* ToolsetClass)
{
    return NiagaraParameterCollectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraParameterCollectionToolset, TEXT("remove_niagara_parameter_collections_toolset"));
}
