#include "CubelessNiagaraPromotionManifestAttachmentLibrary.h"
#include "Registration/CubelessNiagaraPromotionManifestToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraPromotionManifestAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraPromotionManifestAttachmentReceipt(UClass* ToolsetClass, FNiagaraPromotionManifestAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraPromotionManifestAttachmentLibrary::InstallNiagaraPromotionManifestToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionManifestAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraPromotionManifestToolset, TEXT("install_niagara_promotion_manifest_toolset"));
}
FString UCubelessNiagaraPromotionManifestAttachmentLibrary::VerifyNiagaraPromotionManifestToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionManifestAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraPromotionManifestToolset, TEXT("verify_niagara_promotion_manifest_toolset"));
}
FString UCubelessNiagaraPromotionManifestAttachmentLibrary::RemoveNiagaraPromotionManifestToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionManifestAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraPromotionManifestToolset, TEXT("remove_niagara_promotion_manifest_toolset"));
}
