#include "CubelessNiagaraScratchUserRecipeAttachmentLibrary.h"
#include "Registration/CubelessNiagaraScratchUserRecipeToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraScratchUserRecipeAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraScratchUserRecipeAttachmentReceipt(UClass* ToolsetClass, FNiagaraScratchUserRecipeAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraScratchUserRecipeAttachmentLibrary::InstallNiagaraScratchUserRecipeToolset(UClass* ToolsetClass)
{
    return NiagaraScratchUserRecipeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraScratchUserRecipeToolset, TEXT("install_niagara_scratch_user_recipes_toolset"));
}
FString UCubelessNiagaraScratchUserRecipeAttachmentLibrary::VerifyNiagaraScratchUserRecipeToolset(UClass* ToolsetClass)
{
    return NiagaraScratchUserRecipeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraScratchUserRecipeToolset, TEXT("verify_niagara_scratch_user_recipes_toolset"));
}
FString UCubelessNiagaraScratchUserRecipeAttachmentLibrary::RemoveNiagaraScratchUserRecipeToolset(UClass* ToolsetClass)
{
    return NiagaraScratchUserRecipeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraScratchUserRecipeToolset, TEXT("remove_niagara_scratch_user_recipes_toolset"));
}
