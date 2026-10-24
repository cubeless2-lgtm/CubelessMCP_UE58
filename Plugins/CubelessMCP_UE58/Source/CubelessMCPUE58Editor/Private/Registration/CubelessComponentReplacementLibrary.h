#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessComponentReplacementLibrary.generated.h"
UCLASS()
class UCubelessComponentReplacementLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="replace_blueprint_component_class"))
    static FString ReplaceBlueprintComponentClass(const FString& BlueprintPath, const FString& ComponentName,
        const FString& NewComponentClass, double SphereRadius, bool bDryRun, bool bCompile,
        bool bSave, bool bRefreshNodes);
    UFUNCTION(BlueprintCallable, Category="Cubeless|Blueprint", meta=(AIIgnore, ScriptName="replace_blueprint_component_class_raw"))
    static FString ReplaceBlueprintComponentClassRaw(const FString& BlueprintPath, const FString& ComponentName,
        const FString& NewComponentClass, double SphereRadius, bool bDryRun, bool bCompile,
        bool bSave, bool bRefreshNodes, bool bRequireSceneCompatibility);
};
