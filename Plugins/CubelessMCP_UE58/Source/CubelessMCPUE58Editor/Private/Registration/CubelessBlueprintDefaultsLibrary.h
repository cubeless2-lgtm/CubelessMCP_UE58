#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintDefaultsLibrary.generated.h"

UCLASS()
class UCubelessBlueprintDefaultsLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Registration", meta=(AIIgnore, ScriptName="prepare_blueprint_defaults_toolset"))
    static FString PrepareBlueprintDefaultsToolset(UClass* ToolsetClass);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_blueprint_actor_tick_settings"))
    static FString SetBlueprintActorTickSettings(const FString& BlueprintPath, double TickHz, double TickInterval,
        bool bCanEverTick, bool bStartWithTickEnabled, bool bAllowTickOnDedicatedServer,
        bool bTickEvenWhenPaused, bool bDryRun, bool bCompile, bool bSave);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore, ScriptName="set_blueprint_actor_tick_settings_raw"))
    static FString SetBlueprintActorTickSettingsRaw(const FString& BlueprintPath,const FString& SettingsJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="set_skeletal_mesh_component_anim_defaults"))
    static FString SetSkeletalMeshComponentAnimDefaults(const FString& BlueprintPath, const FString& ComponentName,
        const FString& SkeletalMesh, const FString& AnimClass, const FString& PhysicsAssetJson,
        const FString& SimulatePhysicsJson, const FString& CollisionEnabledJson,
        const FString& CollisionResponseJson, const FString& EnableGravityJson,
        const FString& VisibilityBasedAnimTickOptionJson, bool bCompile, bool bSave);
};
