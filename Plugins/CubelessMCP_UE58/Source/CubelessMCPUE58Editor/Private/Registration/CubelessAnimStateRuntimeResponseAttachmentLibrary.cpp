#include "Registration/CubelessAnimStateRuntimeResponseAttachmentLibrary.h"
#include "Registration/CubelessAnimStateRuntimeResponseToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FAnimStateRuntimeResponseAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString AnimStateRuntimeResponseAttachmentReceipt(UClass* ToolsetClass, FAnimStateRuntimeResponseAttachment Function, const TCHAR* Operation)
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
FString UCubelessAnimStateRuntimeResponseAttachmentLibrary::InstallAnimStateRuntimeResponseToolset(UClass* ToolsetClass)
{
    return AnimStateRuntimeResponseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallAnimStateRuntimeResponseToolset, TEXT("install_anim_state_machine_runtime_response_toolset"));
}
FString UCubelessAnimStateRuntimeResponseAttachmentLibrary::VerifyAnimStateRuntimeResponseToolset(UClass* ToolsetClass)
{
    return AnimStateRuntimeResponseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyAnimStateRuntimeResponseToolset, TEXT("verify_anim_state_machine_runtime_response_toolset"));
}
FString UCubelessAnimStateRuntimeResponseAttachmentLibrary::RemoveAnimStateRuntimeResponseToolset(UClass* ToolsetClass)
{
    return AnimStateRuntimeResponseAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveAnimStateRuntimeResponseToolset, TEXT("remove_anim_state_machine_runtime_response_toolset"));
}
