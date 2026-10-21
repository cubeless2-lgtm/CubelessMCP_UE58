#include "Registration/CubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary.h"
#include "Registration/CubelessAnimInstanceRuntimePropertyProbeToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimInstanceRuntimePropertyProbeAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimInstanceRuntimePropertyProbeAttachmentReceipt(UClass* ToolsetClass, FAnimInstanceRuntimePropertyProbeAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary::InstallAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimePropertyProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimInstanceRuntimePropertyProbeToolset, TEXT("install_anim_instance_runtime_property_probe_toolset"));
}
FString UCubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary::VerifyAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimePropertyProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimInstanceRuntimePropertyProbeToolset, TEXT("verify_anim_instance_runtime_property_probe_toolset"));
}
FString UCubelessAnimInstanceRuntimePropertyProbeAttachmentLibrary::RemoveAnimInstanceRuntimePropertyProbeToolset(UClass* ToolsetClass)
{
    return AnimInstanceRuntimePropertyProbeAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimInstanceRuntimePropertyProbeToolset, TEXT("remove_anim_instance_runtime_property_probe_toolset"));
}
