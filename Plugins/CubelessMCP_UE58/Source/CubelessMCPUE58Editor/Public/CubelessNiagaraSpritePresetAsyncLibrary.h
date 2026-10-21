#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessNiagaraSpritePresetAsyncLibrary.generated.h"
class UCubelessNiagaraSpritePresetJob;
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessNiagaraSpritePresetAsyncLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    // Dedicated Sprite preset composition only. ParamsJson retains the original public/raw contract.
    UFUNCTION(BlueprintCallable, Category="Cubeless|Niagara", meta=(AIIgnore, ScriptName="start_sprite_preset_job"))
    static UCubelessNiagaraSpritePresetJob* StartSpritePresetJob(const FString& ParamsJson);
};
