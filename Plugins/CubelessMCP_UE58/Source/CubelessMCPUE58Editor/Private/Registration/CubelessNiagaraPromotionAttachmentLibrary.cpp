#include "CubelessNiagaraPromotionAttachmentLibrary.h"
#include "Registration/CubelessNiagaraPromotionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraPromotionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraPromotionAttachmentReceipt(UClass* ToolsetClass, FNiagaraPromotionAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraPromotionAttachmentLibrary::InstallNiagaraPromotionToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraPromotionToolset, TEXT("install_niagara_promotion_toolset"));
}
FString UCubelessNiagaraPromotionAttachmentLibrary::VerifyNiagaraPromotionToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraPromotionToolset, TEXT("verify_niagara_promotion_toolset"));
}
FString UCubelessNiagaraPromotionAttachmentLibrary::RemoveNiagaraPromotionToolset(UClass* ToolsetClass)
{
    return NiagaraPromotionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraPromotionToolset, TEXT("remove_niagara_promotion_toolset"));
}
