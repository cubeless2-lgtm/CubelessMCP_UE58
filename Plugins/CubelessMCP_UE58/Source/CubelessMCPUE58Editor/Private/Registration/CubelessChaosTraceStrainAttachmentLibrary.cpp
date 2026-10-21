#include "Registration/CubelessChaosTraceStrainAttachmentLibrary.h"
#include "Registration/CubelessChaosTraceStrainToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosTraceStrainAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosTraceStrainAttachmentReceipt(UClass* ToolsetClass, FChaosTraceStrainAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosTraceStrainAttachmentLibrary::InstallChaosTraceStrainToolset(UClass* ToolsetClass)
{
    return ChaosTraceStrainAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosTraceStrainToolset, TEXT("install_chaos_trace_strain_toolset"));
}
FString UCubelessChaosTraceStrainAttachmentLibrary::VerifyChaosTraceStrainToolset(UClass* ToolsetClass)
{
    return ChaosTraceStrainAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosTraceStrainToolset, TEXT("verify_chaos_trace_strain_toolset"));
}
FString UCubelessChaosTraceStrainAttachmentLibrary::RemoveChaosTraceStrainToolset(UClass* ToolsetClass)
{
    return ChaosTraceStrainAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosTraceStrainToolset, TEXT("remove_chaos_trace_strain_toolset"));
}
