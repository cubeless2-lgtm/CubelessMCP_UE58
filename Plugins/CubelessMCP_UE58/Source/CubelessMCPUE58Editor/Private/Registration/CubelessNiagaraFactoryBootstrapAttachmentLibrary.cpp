#include "CubelessNiagaraFactoryBootstrapAttachmentLibrary.h"
#include "Registration/CubelessNiagaraFactoryBootstrapToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraFactoryBootstrapAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraFactoryBootstrapAttachmentReceipt(UClass* ToolsetClass, FNiagaraFactoryBootstrapAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraFactoryBootstrapAttachmentLibrary::InstallNiagaraFactoryBootstrapToolset(UClass* ToolsetClass)
{
    return NiagaraFactoryBootstrapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraFactoryBootstrapToolset, TEXT("install_niagara_factory_bootstrap_toolset"));
}
FString UCubelessNiagaraFactoryBootstrapAttachmentLibrary::VerifyNiagaraFactoryBootstrapToolset(UClass* ToolsetClass)
{
    return NiagaraFactoryBootstrapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraFactoryBootstrapToolset, TEXT("verify_niagara_factory_bootstrap_toolset"));
}
FString UCubelessNiagaraFactoryBootstrapAttachmentLibrary::RemoveNiagaraFactoryBootstrapToolset(UClass* ToolsetClass)
{
    return NiagaraFactoryBootstrapAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraFactoryBootstrapToolset, TEXT("remove_niagara_factory_bootstrap_toolset"));
}
