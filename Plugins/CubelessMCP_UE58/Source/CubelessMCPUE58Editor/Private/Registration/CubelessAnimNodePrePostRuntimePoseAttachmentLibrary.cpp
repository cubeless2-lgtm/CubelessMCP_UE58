#include "Registration/CubelessAnimNodePrePostRuntimePoseAttachmentLibrary.h"
#include "Registration/CubelessAnimNodePrePostRuntimePoseToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimNodePrePostRuntimePoseAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimNodePrePostRuntimePoseAttachmentReceipt(UClass* ToolsetClass, FAnimNodePrePostRuntimePoseAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimNodePrePostRuntimePoseAttachmentLibrary::InstallAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass)
{
    return AnimNodePrePostRuntimePoseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimNodePrePostRuntimePoseToolset, TEXT("install_anim_node_pre_post_runtime_pose_toolset"));
}
FString UCubelessAnimNodePrePostRuntimePoseAttachmentLibrary::VerifyAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass)
{
    return AnimNodePrePostRuntimePoseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimNodePrePostRuntimePoseToolset, TEXT("verify_anim_node_pre_post_runtime_pose_toolset"));
}
FString UCubelessAnimNodePrePostRuntimePoseAttachmentLibrary::RemoveAnimNodePrePostRuntimePoseToolset(UClass* ToolsetClass)
{
    return AnimNodePrePostRuntimePoseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimNodePrePostRuntimePoseToolset, TEXT("remove_anim_node_pre_post_runtime_pose_toolset"));
}
