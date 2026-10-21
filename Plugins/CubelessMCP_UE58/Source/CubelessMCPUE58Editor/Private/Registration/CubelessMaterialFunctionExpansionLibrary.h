#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessMaterialFunctionExpansionLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessMaterialFunctionExpansionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Material",meta=(AIIgnore,ScriptName="expand_material_function_calls"))
    static FString ExpandMaterialFunctionCalls(const FString& MaterialPath,const FString& NodeId,bool bRecursive,bool bExcludeEngineFunctions,int32 MaxPasses,bool bSave,bool bAllowPartialSave);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Registration",meta=(AIIgnore,ScriptName="prepare_material_function_expansion_toolset"))
    static FString PrepareMaterialFunctionExpansionToolset(UClass* ToolsetClass);
};
