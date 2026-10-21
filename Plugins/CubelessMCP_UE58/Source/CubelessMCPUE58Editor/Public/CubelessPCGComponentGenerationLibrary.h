#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessPCGComponentGenerationLibrary.generated.h"
class UPCGComponent;

// Editor-only attachment. The public Python contract is independent of this JSON carrier.
UCLASS(meta=(AIIgnore))
class CUBELESSMCPUE58EDITOR_API UCubelessPCGComponentGenerationLibrary final : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|UE58|PCG", meta=(AIIgnore, ScriptName="refresh_pcg_components"))
    static FString RefreshPCGComponents(const FString& ParamsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|UE58|PCG", meta=(AIIgnore, ScriptName="bind_pcg_generation_settings_to_blueprint"))
    static FString BindPCGGenerationSettingsToBlueprint(const FString& ParamsJson);
    // Internal compatibility for pcg_actor_smoke_test; no additional named tool or reflected bool overload substitution.
    UFUNCTION(BlueprintCallable, Category="Cubeless|UE58|PCG", meta=(AIIgnore, ScriptName="invoke_pcg_component_noarg"))
    static FString InvokePCGComponentNoarg(UPCGComponent* Component, const FString& MethodName);
};
