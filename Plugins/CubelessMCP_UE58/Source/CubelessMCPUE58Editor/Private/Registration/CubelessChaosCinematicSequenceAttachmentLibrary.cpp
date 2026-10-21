#include "Registration/CubelessChaosCinematicSequenceAttachmentLibrary.h"
#include "Registration/CubelessChaosCinematicSequenceToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosCinematicSequenceAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosCinematicSequenceAttachmentReceipt(UClass* ToolsetClass, FChaosCinematicSequenceAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosCinematicSequenceAttachmentLibrary::InstallChaosCinematicSequenceToolset(UClass* ToolsetClass)
{
    return ChaosCinematicSequenceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosCinematicSequenceToolset, TEXT("install_chaos_cinematic_sequence_toolset"));
}
FString UCubelessChaosCinematicSequenceAttachmentLibrary::VerifyChaosCinematicSequenceToolset(UClass* ToolsetClass)
{
    return ChaosCinematicSequenceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosCinematicSequenceToolset, TEXT("verify_chaos_cinematic_sequence_toolset"));
}
FString UCubelessChaosCinematicSequenceAttachmentLibrary::RemoveChaosCinematicSequenceToolset(UClass* ToolsetClass)
{
    return ChaosCinematicSequenceAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosCinematicSequenceToolset, TEXT("remove_chaos_cinematic_sequence_toolset"));
}
