#include "Registration/CubelessActorPropertyFocusAttachmentLibrary.h"
#include "Registration/CubelessActorPropertyFocusToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FActorPropertyFocusAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ActorPropertyFocusAttachmentReceipt(UClass* ToolsetClass, FActorPropertyFocusAttachment Function, const TCHAR* Operation)
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
FString UCubelessActorPropertyFocusAttachmentLibrary::InstallActorPropertyFocusToolset(UClass* ToolsetClass)
{
    return ActorPropertyFocusAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallActorPropertyFocusToolset, TEXT("install_actor_property_focus_toolset"));
}
FString UCubelessActorPropertyFocusAttachmentLibrary::VerifyActorPropertyFocusToolset(UClass* ToolsetClass)
{
    return ActorPropertyFocusAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyActorPropertyFocusToolset, TEXT("verify_actor_property_focus_toolset"));
}
FString UCubelessActorPropertyFocusAttachmentLibrary::RemoveActorPropertyFocusToolset(UClass* ToolsetClass)
{
    return ActorPropertyFocusAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveActorPropertyFocusToolset, TEXT("remove_actor_property_focus_toolset"));
}
