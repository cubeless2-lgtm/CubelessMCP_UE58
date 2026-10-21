#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGGraphAuthoringLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessPCGGraphAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Opaque finite fresh constructor; existing public tool/schema dispatches it.
    UFUNCTION(BlueprintCallable, Category="Cubeless|PCG", meta=(AIIgnore, ScriptName="create_graph_from_spec_json"))
    static FString CreateGraphFromSpecJson(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|PCG", meta=(ScriptName="add_pcg_node"))
    static FString AddPcgNode(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|PCG", meta=(ScriptName="connect_pcg_nodes"))
    static FString ConnectPcgNodes(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|PCG", meta=(ScriptName="set_pcg_node_setting"))
    static FString SetPcgNodeSetting(const FString& ParamsJson);
};
