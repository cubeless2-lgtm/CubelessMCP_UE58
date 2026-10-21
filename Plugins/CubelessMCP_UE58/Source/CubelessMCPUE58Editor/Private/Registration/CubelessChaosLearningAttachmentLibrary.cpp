#include "Registration/CubelessChaosLearningAttachmentLibrary.h"
#include "Registration/CubelessChaosLearningToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosLearningAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosLearningAttachmentReceipt(UClass* ToolsetClass, FChaosLearningAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosLearningAttachmentLibrary::InstallChaosLearningToolset(UClass* ToolsetClass)
{
    return ChaosLearningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosLearningToolset, TEXT("install_chaos_learning_toolset"));
}
FString UCubelessChaosLearningAttachmentLibrary::VerifyChaosLearningToolset(UClass* ToolsetClass)
{
    return ChaosLearningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosLearningToolset, TEXT("verify_chaos_learning_toolset"));
}
FString UCubelessChaosLearningAttachmentLibrary::RemoveChaosLearningToolset(UClass* ToolsetClass)
{
    return ChaosLearningAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosLearningToolset, TEXT("remove_chaos_learning_toolset"));
}
