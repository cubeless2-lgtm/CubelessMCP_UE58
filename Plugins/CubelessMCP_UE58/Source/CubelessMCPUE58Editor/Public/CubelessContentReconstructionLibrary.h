#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessContentReconstructionLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessContentReconstructionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Content Reconstruction")
    static FString RecreateContentFolderMCP(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Content Reconstruction")
    static FString PostprocessContentFolderMCP(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Content Reconstruction")
    static FString RepairWorldActorInstancesMCP(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable, Category="Cubeless MCP|Content Reconstruction")
    static FString RunContentValidationPipelineMCP(const FString& ParamsJson);
};
