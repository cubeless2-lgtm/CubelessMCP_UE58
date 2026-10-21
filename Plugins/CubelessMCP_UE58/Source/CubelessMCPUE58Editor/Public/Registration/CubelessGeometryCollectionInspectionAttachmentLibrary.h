#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessGeometryCollectionInspectionAttachmentLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessGeometryCollectionInspectionAttachmentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Registration-only helpers; no domain or asset operation is dispatched here.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="install_geometry_collection_inspection_toolset"))
    static FString InstallGeometryCollectionInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="verify_geometry_collection_inspection_toolset"))
    static FString VerifyGeometryCollectionInspectionToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="remove_geometry_collection_inspection_toolset"))
    static FString RemoveGeometryCollectionInspectionToolset(UClass* ToolsetClass);
};
