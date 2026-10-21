#include "Registration/CubelessSplineComponentPointsAttachmentLibrary.h"
#include "Registration/CubelessSplineComponentPointsToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FSplineComponentPointsAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString SplineComponentPointsAttachmentReceipt(UClass* ToolsetClass, FSplineComponentPointsAttachment Function, const TCHAR* Operation)
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
FString UCubelessSplineComponentPointsAttachmentLibrary::InstallSplineComponentPointsToolset(UClass* ToolsetClass)
{
    return SplineComponentPointsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallSplineComponentPointsToolset, TEXT("install_spline_component_points_toolset"));
}
FString UCubelessSplineComponentPointsAttachmentLibrary::VerifySplineComponentPointsToolset(UClass* ToolsetClass)
{
    return SplineComponentPointsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifySplineComponentPointsToolset, TEXT("verify_spline_component_points_toolset"));
}
FString UCubelessSplineComponentPointsAttachmentLibrary::RemoveSplineComponentPointsToolset(UClass* ToolsetClass)
{
    return SplineComponentPointsAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveSplineComponentPointsToolset, TEXT("remove_spline_component_points_toolset"));
}
