#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintComponentReferenceLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintComponentReferenceLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_get_self_component_reference"))
    static FString AddBlueprintGetSelfComponentReference(const FString& BlueprintName,const FString& ComponentName,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
