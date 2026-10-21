#include "Registration/CubelessControlRigTransientProbeAttachmentLibrary.h"
#include "Registration/CubelessControlRigTransientProbeToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FControlRigTransientProbeAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ControlRigTransientProbeAttachmentReceipt(UClass* ToolsetClass, FControlRigTransientProbeAttachment Function, const TCHAR* Operation)
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
FString UCubelessControlRigTransientProbeAttachmentLibrary::InstallControlRigTransientProbeToolset(UClass* ToolsetClass)
{
    return ControlRigTransientProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallControlRigTransientProbeToolset, TEXT("install_controlrig_transient_probe_toolset"));
}
FString UCubelessControlRigTransientProbeAttachmentLibrary::VerifyControlRigTransientProbeToolset(UClass* ToolsetClass)
{
    return ControlRigTransientProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyControlRigTransientProbeToolset, TEXT("verify_controlrig_transient_probe_toolset"));
}
FString UCubelessControlRigTransientProbeAttachmentLibrary::RemoveControlRigTransientProbeToolset(UClass* ToolsetClass)
{
    return ControlRigTransientProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveControlRigTransientProbeToolset, TEXT("remove_controlrig_transient_probe_toolset"));
}
