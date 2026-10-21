#include "Registration/CubelessBlendSpacePoseGridAttachmentLibrary.h"
#include "Registration/CubelessBlendSpacePoseGridToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlendSpacePoseGridAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlendSpacePoseGridAttachmentReceipt(UClass* ToolsetClass, FBlendSpacePoseGridAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlendSpacePoseGridAttachmentLibrary::InstallBlendSpacePoseGridToolset(UClass* ToolsetClass)
{
    return BlendSpacePoseGridAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlendSpacePoseGridToolset, TEXT("install_blendspace_pose_grid_toolset"));
}
FString UCubelessBlendSpacePoseGridAttachmentLibrary::VerifyBlendSpacePoseGridToolset(UClass* ToolsetClass)
{
    return BlendSpacePoseGridAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlendSpacePoseGridToolset, TEXT("verify_blendspace_pose_grid_toolset"));
}
FString UCubelessBlendSpacePoseGridAttachmentLibrary::RemoveBlendSpacePoseGridToolset(UClass* ToolsetClass)
{
    return BlendSpacePoseGridAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlendSpacePoseGridToolset, TEXT("remove_blendspace_pose_grid_toolset"));
}
