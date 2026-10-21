#include "Registration/CubelessChaosGeometryAuthoringAttachmentLibrary.h"
#include "Registration/CubelessChaosGeometryAuthoringToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FChaosGeometryAuthoringAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ChaosGeometryAuthoringAttachmentReceipt(UClass* ToolsetClass, FChaosGeometryAuthoringAttachment Function, const TCHAR* Operation)
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
FString UCubelessChaosGeometryAuthoringAttachmentLibrary::InstallChaosGeometryAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosGeometryAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallChaosGeometryAuthoringToolset, TEXT("install_chaos_geometry_authoring_toolset"));
}
FString UCubelessChaosGeometryAuthoringAttachmentLibrary::VerifyChaosGeometryAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosGeometryAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyChaosGeometryAuthoringToolset, TEXT("verify_chaos_geometry_authoring_toolset"));
}
FString UCubelessChaosGeometryAuthoringAttachmentLibrary::RemoveChaosGeometryAuthoringToolset(UClass* ToolsetClass)
{
    return ChaosGeometryAuthoringAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveChaosGeometryAuthoringToolset, TEXT("remove_chaos_geometry_authoring_toolset"));
}
