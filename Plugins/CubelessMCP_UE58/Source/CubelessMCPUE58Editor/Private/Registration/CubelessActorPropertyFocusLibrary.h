#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessActorPropertyFocusLibrary.generated.h"

UCLASS(meta=(AIIgnore))
class UCubelessActorPropertyFocusLibrary final:public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="set_actor_property"))
    static FString SetActorProperty(FString ParamsJson);
    UFUNCTION(BlueprintCallable,meta=(AIIgnore,ScriptName="focus_viewport"))
    static FString FocusViewport(FString ParamsJson);
};
