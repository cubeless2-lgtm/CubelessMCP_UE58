#include "CubelessNiagaraSpritePresetAttachmentLibrary.h"
#include "Registration/CubelessNiagaraSpritePresetToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FNiagaraSpritePresetAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString NiagaraSpritePresetAttachmentReceipt(UClass* ToolsetClass, FNiagaraSpritePresetAttachment Function, const TCHAR* Operation)
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
FString UCubelessNiagaraSpritePresetAttachmentLibrary::InstallNiagaraSpritePresetToolset(UClass* ToolsetClass)
{
    return NiagaraSpritePresetAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallNiagaraSpritePresetToolset, TEXT("install_niagara_sprite_preset_authoring_toolset"));
}
FString UCubelessNiagaraSpritePresetAttachmentLibrary::VerifyNiagaraSpritePresetToolset(UClass* ToolsetClass)
{
    return NiagaraSpritePresetAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyNiagaraSpritePresetToolset, TEXT("verify_niagara_sprite_preset_authoring_toolset"));
}
FString UCubelessNiagaraSpritePresetAttachmentLibrary::RemoveNiagaraSpritePresetToolset(UClass* ToolsetClass)
{
    return NiagaraSpritePresetAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveNiagaraSpritePresetToolset, TEXT("remove_niagara_sprite_preset_authoring_toolset"));
}
