#include "Registration/CubelessChaosClothInspectionAttachmentLibrary.h"
#include "Registration/CubelessChaosClothInspectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosClothInspectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosClothInspectionAttachmentReceipt(UClass* ToolsetClass, FChaosClothInspectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosClothInspectionAttachmentLibrary::InstallChaosClothInspectionToolset(UClass* ToolsetClass)
{
    return ChaosClothInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosClothInspectionToolset, TEXT("install_chaos_cloth_inspection_toolset"));
}
FString UCubelessChaosClothInspectionAttachmentLibrary::VerifyChaosClothInspectionToolset(UClass* ToolsetClass)
{
    return ChaosClothInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosClothInspectionToolset, TEXT("verify_chaos_cloth_inspection_toolset"));
}
FString UCubelessChaosClothInspectionAttachmentLibrary::RemoveChaosClothInspectionToolset(UClass* ToolsetClass)
{
    return ChaosClothInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosClothInspectionToolset, TEXT("remove_chaos_cloth_inspection_toolset"));
}
