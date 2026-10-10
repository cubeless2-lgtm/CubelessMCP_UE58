#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialLibrary.generated.h"

/** Native implementation helpers for the journaled Python Material Toolset.
 * Editor-only. Never persist calls or references to this class in game assets.
 * These methods are deliberately excluded from direct AI tool discovery.
 */
UCLASS()
class UCubelessMaterialLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString ResolveMaterialGraph(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString ListMaterialNodes(const FString& MaterialPath, const FString& GraphType, const FString& NodeType, const FString& DescContains, bool bIncludePins, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString AnalyzeMaterialGraph(const FString& MaterialPath, const FString& GraphType, bool bIncludeNodes, bool bIncludeUsage, int32 MaxReferencers, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString ListMaterialCollectionParameterNodes(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString GetMaterialParameterCollectionValues(const FString& CollectionPath, const TArray<FString>& ParameterNames, bool bIncludeAssetDefaults, bool bIncludeRuntime);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString ConnectMaterialNodes(const FString& MaterialPath, const FString& FromNodeId, const FString& ToNodeId, const FString& ToInput, const FString& FromOutput, const FString& GraphType, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString ConnectMaterialProperty(const FString& MaterialPath, const FString& FromNodeId, const FString& MaterialProperty, const FString& FromOutput, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString DeleteMaterialNode(const FString& MaterialPath, const FString& NodeId, const FString& GraphType, int32 MaxGraphNodes);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString LayoutMaterialNodes(const FString& MaterialPath, const FString& GraphType, int32 MaxGraphNodes);

    /** PropertiesJson is one JSON object; no compile/save is requested. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString AddMaterialNode(const FString& MaterialPath, const FString& ExpressionClass, const FString& GraphType, int32 NodeX, int32 NodeY, const FString& SelectedAsset, const FString& PropertiesJson, int32 MaxGraphNodes);

    /** ValueJson is exactly one JSON value, including quoted strings. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString SetMaterialNodeProperty(const FString& MaterialPath, const FString& NodeId, const FString& PropertyName, const FString& ValueJson, const FString& GraphType, int32 MaxGraphNodes);

    /** Specialized legacy Custom builder. Includes remain explicitly unsupported in this slice. */
    UFUNCTION(BlueprintCallable, Category="Cubeless|Material", meta=(AIIgnore))
    static FString AddCustomMaterialNode(const FString& MaterialPath, const FString& Code, const FString& OutputType, const FString& InputsJson, const FString& Description, const FString& GraphType, int32 NodeX, int32 NodeY, const FString& AdditionalDefinesJson, const FString& IncludeFilePathsJson, int32 MaxGraphNodes);
};
