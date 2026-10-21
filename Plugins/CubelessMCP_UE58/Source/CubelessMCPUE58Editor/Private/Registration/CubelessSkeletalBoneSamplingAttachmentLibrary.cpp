#include "Registration/CubelessSkeletalBoneSamplingAttachmentLibrary.h"
#include "Registration/CubelessSkeletalBoneSamplingToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FSkeletalBoneSamplingAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString SkeletalBoneSamplingAttachmentReceipt(UClass* ToolsetClass, FSkeletalBoneSamplingAttachment Function, const TCHAR* Operation)
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
FString UCubelessSkeletalBoneSamplingAttachmentLibrary::InstallSkeletalBoneSamplingToolset(UClass* ToolsetClass)
{
    return SkeletalBoneSamplingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallSkeletalBoneSamplingToolset, TEXT("install_skeletal_bone_sampling_toolset"));
}
FString UCubelessSkeletalBoneSamplingAttachmentLibrary::VerifySkeletalBoneSamplingToolset(UClass* ToolsetClass)
{
    return SkeletalBoneSamplingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifySkeletalBoneSamplingToolset, TEXT("verify_skeletal_bone_sampling_toolset"));
}
FString UCubelessSkeletalBoneSamplingAttachmentLibrary::RemoveSkeletalBoneSamplingToolset(UClass* ToolsetClass)
{
    return SkeletalBoneSamplingAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveSkeletalBoneSamplingToolset, TEXT("remove_skeletal_bone_sampling_toolset"));
}
