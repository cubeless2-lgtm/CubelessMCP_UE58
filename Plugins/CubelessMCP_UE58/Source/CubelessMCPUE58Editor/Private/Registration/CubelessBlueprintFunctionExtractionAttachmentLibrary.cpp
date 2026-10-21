#include "Registration/CubelessBlueprintFunctionExtractionAttachmentLibrary.h"
#include "Registration/CubelessBlueprintFunctionExtractionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FBlueprintFunctionExtractionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString BlueprintFunctionExtractionAttachmentReceipt(UClass* ToolsetClass, FBlueprintFunctionExtractionAttachment Function, const TCHAR* Operation)
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
FString UCubelessBlueprintFunctionExtractionAttachmentLibrary::InstallBlueprintFunctionExtractionToolset(UClass* ToolsetClass)
{
    return BlueprintFunctionExtractionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallBlueprintFunctionExtractionToolset, TEXT("install_blueprint_function_extraction_toolset"));
}
FString UCubelessBlueprintFunctionExtractionAttachmentLibrary::VerifyBlueprintFunctionExtractionToolset(UClass* ToolsetClass)
{
    return BlueprintFunctionExtractionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyBlueprintFunctionExtractionToolset, TEXT("verify_blueprint_function_extraction_toolset"));
}
FString UCubelessBlueprintFunctionExtractionAttachmentLibrary::RemoveBlueprintFunctionExtractionToolset(UClass* ToolsetClass)
{
    return BlueprintFunctionExtractionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveBlueprintFunctionExtractionToolset, TEXT("remove_blueprint_function_extraction_toolset"));
}
