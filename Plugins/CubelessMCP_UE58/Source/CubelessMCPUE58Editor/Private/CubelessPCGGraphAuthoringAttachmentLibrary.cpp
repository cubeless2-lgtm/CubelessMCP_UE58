#include "CubelessPCGGraphAuthoringAttachmentLibrary.h"
#include "Registration/CubelessPCGGraphAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FPCGGraphAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString PCGGraphAuthoringAttachmentReceipt(UClass* ToolsetClass, FPCGGraphAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessPCGGraphAuthoringAttachmentLibrary::InstallPCGGraphAuthoringToolset(UClass* ToolsetClass)
{
    return PCGGraphAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallPCGGraphAuthoringToolset, TEXT("install_pcg_graph_authoring_toolset"));
}
FString UCubelessPCGGraphAuthoringAttachmentLibrary::VerifyPCGGraphAuthoringToolset(UClass* ToolsetClass)
{
    return PCGGraphAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyPCGGraphAuthoringToolset, TEXT("verify_pcg_graph_authoring_toolset"));
}
FString UCubelessPCGGraphAuthoringAttachmentLibrary::RemovePCGGraphAuthoringToolset(UClass* ToolsetClass)
{
    return PCGGraphAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemovePCGGraphAuthoringToolset, TEXT("remove_pcg_graph_authoring_toolset"));
}
