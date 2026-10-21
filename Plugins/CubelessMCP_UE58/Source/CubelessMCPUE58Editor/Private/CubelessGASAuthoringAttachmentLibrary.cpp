#include "CubelessGASAuthoringAttachmentLibrary.h"
#include "Registration/CubelessGASAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FGASAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString GASAuthoringAttachmentReceipt(UClass* ToolsetClass, FGASAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessGASAuthoringAttachmentLibrary::InstallGASAuthoringToolset(UClass* ToolsetClass)
{
    return GASAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallGASAuthoringToolset, TEXT("install_gas_authoring_toolset"));
}
FString UCubelessGASAuthoringAttachmentLibrary::VerifyGASAuthoringToolset(UClass* ToolsetClass)
{
    return GASAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyGASAuthoringToolset, TEXT("verify_gas_authoring_toolset"));
}
FString UCubelessGASAuthoringAttachmentLibrary::RemoveGASAuthoringToolset(UClass* ToolsetClass)
{
    return GASAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveGASAuthoringToolset, TEXT("remove_gas_authoring_toolset"));
}
