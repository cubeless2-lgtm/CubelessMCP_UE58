#include "Registration/CubelessAnimStateMachineVariantAttachmentLibrary.h"
#include "Registration/CubelessAnimStateMachineVariantToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimStateMachineVariantAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimStateMachineVariantAttachmentReceipt(UClass* ToolsetClass, FAnimStateMachineVariantAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimStateMachineVariantAttachmentLibrary::InstallAnimStateMachineVariantToolset(UClass* ToolsetClass)
{
    return AnimStateMachineVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimStateMachineVariantToolset, TEXT("install_anim_state_machine_variant_toolset"));
}
FString UCubelessAnimStateMachineVariantAttachmentLibrary::VerifyAnimStateMachineVariantToolset(UClass* ToolsetClass)
{
    return AnimStateMachineVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimStateMachineVariantToolset, TEXT("verify_anim_state_machine_variant_toolset"));
}
FString UCubelessAnimStateMachineVariantAttachmentLibrary::RemoveAnimStateMachineVariantToolset(UClass* ToolsetClass)
{
    return AnimStateMachineVariantAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimStateMachineVariantToolset, TEXT("remove_anim_state_machine_variant_toolset"));
}
