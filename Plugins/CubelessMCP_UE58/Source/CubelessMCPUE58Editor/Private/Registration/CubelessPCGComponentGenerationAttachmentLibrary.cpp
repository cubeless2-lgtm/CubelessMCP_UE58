#include "CubelessPCGComponentGenerationAttachmentLibrary.h"
#include "Registration/CubelessPCGComponentGenerationToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPCGComponentGenerationAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PCGComponentGenerationAttachmentReceipt(UClass* ToolsetClass, FPCGComponentGenerationAttachment Function, const TCHAR* Operation)
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
FString UCubelessPCGComponentGenerationAttachmentLibrary::InstallPCGComponentGenerationToolset(UClass* ToolsetClass)
{
    return PCGComponentGenerationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPCGComponentGenerationToolset, TEXT("install_pcg_component_generation_toolset"));
}
FString UCubelessPCGComponentGenerationAttachmentLibrary::VerifyPCGComponentGenerationToolset(UClass* ToolsetClass)
{
    return PCGComponentGenerationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPCGComponentGenerationToolset, TEXT("verify_pcg_component_generation_toolset"));
}
FString UCubelessPCGComponentGenerationAttachmentLibrary::RemovePCGComponentGenerationToolset(UClass* ToolsetClass)
{
    return PCGComponentGenerationAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePCGComponentGenerationToolset, TEXT("remove_pcg_component_generation_toolset"));
}
