#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessStateTreeCreateLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessStateTreeCreateLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|StateTree", meta=(AIIgnore, ScriptName="create_state_tree"))
    static FString CreateStateTree(const FString& Name, const FString& PackagePath, const FString& SchemaType);
};
