#include "Registration/CubelessAnimBPLocomotionDriverAttachmentLibrary.h"
#include "Registration/CubelessAnimBPLocomotionDriverToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimBPLocomotionDriverAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimBPLocomotionDriverAttachmentReceipt(UClass* ToolsetClass, FAnimBPLocomotionDriverAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimBPLocomotionDriverAttachmentLibrary::InstallAnimBPLocomotionDriverToolset(UClass* ToolsetClass)
{
    return AnimBPLocomotionDriverAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimBPLocomotionDriverToolset, TEXT("install_anim_bp_locomotion_driver_toolset"));
}
FString UCubelessAnimBPLocomotionDriverAttachmentLibrary::VerifyAnimBPLocomotionDriverToolset(UClass* ToolsetClass)
{
    return AnimBPLocomotionDriverAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimBPLocomotionDriverToolset, TEXT("verify_anim_bp_locomotion_driver_toolset"));
}
FString UCubelessAnimBPLocomotionDriverAttachmentLibrary::RemoveAnimBPLocomotionDriverToolset(UClass* ToolsetClass)
{
    return AnimBPLocomotionDriverAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimBPLocomotionDriverToolset, TEXT("remove_anim_bp_locomotion_driver_toolset"));
}
