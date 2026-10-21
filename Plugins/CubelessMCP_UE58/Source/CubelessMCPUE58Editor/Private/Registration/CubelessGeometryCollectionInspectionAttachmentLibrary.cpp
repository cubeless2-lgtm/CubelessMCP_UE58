#include "Registration/CubelessGeometryCollectionInspectionAttachmentLibrary.h"
#include "Registration/CubelessGeometryCollectionInspectionToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"

namespace
{
using FGeometryCollectionInspectionAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString GeometryCollectionInspectionAttachmentReceipt(UClass* ToolsetClass, FGeometryCollectionInspectionAttachment Function, const TCHAR* Operation)
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
FString UCubelessGeometryCollectionInspectionAttachmentLibrary::InstallGeometryCollectionInspectionToolset(UClass* ToolsetClass)
{
    return GeometryCollectionInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallGeometryCollectionInspectionToolset, TEXT("install_geometry_collection_inspection_toolset"));
}
FString UCubelessGeometryCollectionInspectionAttachmentLibrary::VerifyGeometryCollectionInspectionToolset(UClass* ToolsetClass)
{
    return GeometryCollectionInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyGeometryCollectionInspectionToolset, TEXT("verify_geometry_collection_inspection_toolset"));
}
FString UCubelessGeometryCollectionInspectionAttachmentLibrary::RemoveGeometryCollectionInspectionToolset(UClass* ToolsetClass)
{
    return GeometryCollectionInspectionAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveGeometryCollectionInspectionToolset, TEXT("remove_geometry_collection_inspection_toolset"));
}
