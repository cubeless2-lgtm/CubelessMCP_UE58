#pragma once

#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessComponentLibrary.generated.h"

/** Hidden editor-only local SCS inspection adapters. No game asset may depend on this class. */
UCLASS()
class UCubelessComponentLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString ListBlueprintComponents(const FString& BlueprintPath, const FString& ComponentName, int32 MaxComponents);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString GetComponentProperty(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName, int32 MaxComponents, int32 MaxValueChars);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString AddComponentToBlueprint(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
        const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString SetComponentProperty(const FString& BlueprintPath, const FString& ComponentName,
        const FString& PropertyName, const FString& ValueJson);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString SetPhysicsProperties(const FString& BlueprintPath, const FString& ComponentName,
        bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping);

    UFUNCTION(BlueprintCallable, Category="Cubeless|Component", meta=(AIIgnore))
    static FString SetStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
        const FString& StaticMeshPath, const FString& MaterialPath);
};
