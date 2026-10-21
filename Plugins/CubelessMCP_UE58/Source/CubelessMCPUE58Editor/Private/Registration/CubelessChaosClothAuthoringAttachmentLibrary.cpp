#include "Registration/CubelessChaosClothAuthoringAttachmentLibrary.h"
#include "Registration/CubelessChaosClothAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosClothAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosClothAuthoringAttachmentReceipt(UClass* ToolsetClass, FChaosClothAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosClothAuthoringAttachmentLibrary::InstallChaosClothAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosClothAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosClothAuthoringToolset, TEXT("install_chaos_cloth_authoring_toolset"));
}
FString UCubelessChaosClothAuthoringAttachmentLibrary::VerifyChaosClothAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosClothAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosClothAuthoringToolset, TEXT("verify_chaos_cloth_authoring_toolset"));
}
FString UCubelessChaosClothAuthoringAttachmentLibrary::RemoveChaosClothAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosClothAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosClothAuthoringToolset, TEXT("remove_chaos_cloth_authoring_toolset"));
}
