#include "Registration/CubelessChaosClothComponentAttachmentLibrary.h"
#include "Registration/CubelessChaosClothComponentToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosClothComponentAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosClothComponentAttachmentReceipt(UClass* ToolsetClass, FChaosClothComponentAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosClothComponentAttachmentLibrary::InstallChaosClothComponentToolset(UClass* ToolsetClass)
{
    return ChaosClothComponentAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosClothComponentToolset, TEXT("install_chaos_cloth_component_toolset"));
}
FString UCubelessChaosClothComponentAttachmentLibrary::VerifyChaosClothComponentToolset(UClass* ToolsetClass)
{
    return ChaosClothComponentAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosClothComponentToolset, TEXT("verify_chaos_cloth_component_toolset"));
}
FString UCubelessChaosClothComponentAttachmentLibrary::RemoveChaosClothComponentToolset(UClass* ToolsetClass)
{
    return ChaosClothComponentAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosClothComponentToolset, TEXT("remove_chaos_cloth_component_toolset"));
}
