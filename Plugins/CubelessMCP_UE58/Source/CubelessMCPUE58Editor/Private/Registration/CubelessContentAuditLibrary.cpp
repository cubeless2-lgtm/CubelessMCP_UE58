#include "Registration/CubelessContentAuditLibrary.h"
#include "Compatibility/UE58/CubelessContentAuditApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessContentAuditToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
FString UCubelessContentAuditLibrary::AuditContentRootMCP(const FString& RootPath, bool bRecursive, bool bScanPaths,
    bool bIncludeDependencies, bool bIncludeAssets, bool bWriteReport, const FString& ForbiddenPrefixesJson,
    const FString& RequiredAssetPathsJson, const FString& ExpectedAssetCountJson, const FString& ExpectedClassCountsJson,
    int64 MaxSamples, bool bFailOnDirtyPackages, bool bFailOnRedirectors, bool bFailOnForbiddenDependencies)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ContentAudit::Audit(RootPath, bRecursive,
        bScanPaths, bIncludeDependencies, bIncludeAssets, bWriteReport, ForbiddenPrefixesJson, RequiredAssetPathsJson,
        ExpectedAssetCountJson, ExpectedClassCountsJson, MaxSamples, bFailOnDirtyPackages, bFailOnRedirectors,
        bFailOnForbiddenDependencies), TEXT("audit_content_root_mcp"));
}

namespace
{
using FContentAuditAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString ContentAuditAttachmentReceipt(UClass* ToolsetClass, FContentAuditAttachment Function, const TCHAR* Operation)
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
FString UCubelessContentAuditLibrary::InstallContentAuditToolset(UClass* ToolsetClass)
{
    return ContentAuditAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallContentAuditToolset, TEXT("install_content_audit_toolset"));
}
FString UCubelessContentAuditLibrary::VerifyContentAuditToolset(UClass* ToolsetClass)
{
    return ContentAuditAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyContentAuditToolset, TEXT("verify_content_audit_toolset"));
}
FString UCubelessContentAuditLibrary::RemoveContentAuditToolset(UClass* ToolsetClass)
{
    return ContentAuditAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveContentAuditToolset, TEXT("remove_content_audit_toolset"));
}
