#include "Registration/CubelessChaosDestructionPerformanceAttachmentLibrary.h"
#include "Registration/CubelessChaosDestructionPerformanceToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosDestructionPerformanceAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosDestructionPerformanceAttachmentReceipt(UClass* ToolsetClass, FChaosDestructionPerformanceAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosDestructionPerformanceAttachmentLibrary::InstallChaosDestructionPerformanceToolset(UClass* ToolsetClass)
{
    return ChaosDestructionPerformanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosDestructionPerformanceToolset, TEXT("install_chaos_destruction_performance_toolset"));
}
FString UCubelessChaosDestructionPerformanceAttachmentLibrary::VerifyChaosDestructionPerformanceToolset(UClass* ToolsetClass)
{
    return ChaosDestructionPerformanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosDestructionPerformanceToolset, TEXT("verify_chaos_destruction_performance_toolset"));
}
FString UCubelessChaosDestructionPerformanceAttachmentLibrary::RemoveChaosDestructionPerformanceToolset(UClass* ToolsetClass)
{
    return ChaosDestructionPerformanceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosDestructionPerformanceToolset, TEXT("remove_chaos_destruction_performance_toolset"));
}
