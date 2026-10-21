#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessContentAuditLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessContentAuditLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Four strings carry nullable JSON values only at this hidden ABI; public inputs retain native array/map/integer types.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Project", meta=(AIIgnore, ScriptName="audit_content_root_mcp"))
    static FString AuditContentRootMCP(const FString& RootPath, bool bRecursive, bool bScanPaths, bool bIncludeDependencies,
        bool bIncludeAssets, bool bWriteReport, const FString& ForbiddenPrefixesJson, const FString& RequiredAssetPathsJson,
        const FString& ExpectedAssetCountJson, const FString& ExpectedClassCountsJson, int64 MaxSamples,
        bool bFailOnDirtyPackages, bool bFailOnRedirectors, bool bFailOnForbiddenDependencies);
    // Registration-only helpers; no content audit, journal or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_content_audit_toolset"))
    static FString InstallContentAuditToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_content_audit_toolset"))
    static FString VerifyContentAuditToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_content_audit_toolset"))
    static FString RemoveContentAuditToolset(UClass* ToolsetClass);
};
