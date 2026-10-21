#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintEditorSubsystemLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintEditorSubsystemLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="add_blueprint_get_editor_subsystem_node"))
    static FString AddBlueprintGetEditorSubsystemNode(const FString& BlueprintName,const FString& SubsystemClass,const FString& PositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
