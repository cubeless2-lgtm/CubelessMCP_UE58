#include "Registration/CubelessActorTransformAttachmentLibrary.h"
#include "Registration/CubelessActorTransformToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FActorTransformAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ActorTransformAttachmentReceipt(UClass* ToolsetClass, FActorTransformAttachment Function, const TCHAR* Operation)
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
FString UCubelessActorTransformAttachmentLibrary::InstallActorTransformToolset(UClass* ToolsetClass)
{
    return ActorTransformAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallActorTransformToolset, TEXT("install_actor_transform_toolset"));
}
FString UCubelessActorTransformAttachmentLibrary::VerifyActorTransformToolset(UClass* ToolsetClass)
{
    return ActorTransformAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyActorTransformToolset, TEXT("verify_actor_transform_toolset"));
}
FString UCubelessActorTransformAttachmentLibrary::RemoveActorTransformToolset(UClass* ToolsetClass)
{
    return ActorTransformAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveActorTransformToolset, TEXT("remove_actor_transform_toolset"));
}
