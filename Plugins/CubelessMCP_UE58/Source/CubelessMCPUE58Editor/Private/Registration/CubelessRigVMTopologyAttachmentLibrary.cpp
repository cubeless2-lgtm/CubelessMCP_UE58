#include "Registration/CubelessRigVMTopologyAttachmentLibrary.h"
#include "Registration/CubelessRigVMTopologyToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FRigVMTopologyAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString RigVMTopologyAttachmentReceipt(UClass* ToolsetClass, FRigVMTopologyAttachment Function, const TCHAR* Operation)
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
FString UCubelessRigVMTopologyAttachmentLibrary::InstallRigVMTopologyToolset(UClass* ToolsetClass)
{
    return RigVMTopologyAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallRigVMTopologyToolset, TEXT("install_rigvm_topology_toolset"));
}
FString UCubelessRigVMTopologyAttachmentLibrary::VerifyRigVMTopologyToolset(UClass* ToolsetClass)
{
    return RigVMTopologyAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyRigVMTopologyToolset, TEXT("verify_rigvm_topology_toolset"));
}
FString UCubelessRigVMTopologyAttachmentLibrary::RemoveRigVMTopologyToolset(UClass* ToolsetClass)
{
    return RigVMTopologyAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveRigVMTopologyToolset, TEXT("remove_rigvm_topology_toolset"));
}
