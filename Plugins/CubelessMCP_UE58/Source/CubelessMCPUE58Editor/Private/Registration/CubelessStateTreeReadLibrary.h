#pragma once

#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessStateTreeReadLibrary.generated.h"

UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessStateTreeReadLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|StateTree", meta=(AIIgnore, ScriptName="list_state_trees"))
    static FString ListStateTrees(const FString& Directory, bool bRecursive, int64 MaxResults, int64 Offset);

    UFUNCTION(BlueprintCallable, Category="Cubeless|StateTree", meta=(AIIgnore, ScriptName="inspect_state_tree"))
    static FString InspectStateTree(const FString& AssetPath, int64 MaxStates);
};
