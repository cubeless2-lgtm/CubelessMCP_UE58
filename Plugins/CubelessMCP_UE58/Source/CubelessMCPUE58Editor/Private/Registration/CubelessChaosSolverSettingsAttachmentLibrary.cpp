#include "Registration/CubelessChaosSolverSettingsAttachmentLibrary.h"
#include "Registration/CubelessChaosSolverSettingsToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosSolverSettingsAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosSolverSettingsAttachmentReceipt(UClass* ToolsetClass, FChaosSolverSettingsAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosSolverSettingsAttachmentLibrary::InstallChaosSolverSettingsToolset(UClass* ToolsetClass)
{
    return ChaosSolverSettingsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosSolverSettingsToolset, TEXT("install_chaos_solver_settings_toolset"));
}
FString UCubelessChaosSolverSettingsAttachmentLibrary::VerifyChaosSolverSettingsToolset(UClass* ToolsetClass)
{
    return ChaosSolverSettingsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosSolverSettingsToolset, TEXT("verify_chaos_solver_settings_toolset"));
}
FString UCubelessChaosSolverSettingsAttachmentLibrary::RemoveChaosSolverSettingsToolset(UClass* ToolsetClass)
{
    return ChaosSolverSettingsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosSolverSettingsToolset, TEXT("remove_chaos_solver_settings_toolset"));
}
