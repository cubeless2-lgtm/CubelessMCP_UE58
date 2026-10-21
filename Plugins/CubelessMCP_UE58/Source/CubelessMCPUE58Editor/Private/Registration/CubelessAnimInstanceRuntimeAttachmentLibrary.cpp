#include "Registration/CubelessAnimInstanceRuntimeAttachmentLibrary.h"
#include "Registration/CubelessAnimInstanceRuntimeToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimInstanceRuntimeAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimInstanceRuntimeAttachmentReceipt(UClass* ToolsetClass, FAnimInstanceRuntimeAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimInstanceRuntimeAttachmentLibrary::InstallAnimInstanceRuntimeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimInstanceRuntimeToolset, TEXT("install_anim_instance_runtime_toolset"));
}
FString UCubelessAnimInstanceRuntimeAttachmentLibrary::VerifyAnimInstanceRuntimeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimInstanceRuntimeToolset, TEXT("verify_anim_instance_runtime_toolset"));
}
FString UCubelessAnimInstanceRuntimeAttachmentLibrary::RemoveAnimInstanceRuntimeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimInstanceRuntimeToolset, TEXT("remove_anim_instance_runtime_toolset"));
}
