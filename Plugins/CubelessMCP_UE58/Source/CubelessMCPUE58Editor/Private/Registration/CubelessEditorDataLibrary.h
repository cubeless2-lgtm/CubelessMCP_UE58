#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessEditorDataLibrary.generated.h"

// Authoring/inspection only. Never reference these functions in game assets.
UCLASS()
class UCubelessEditorDataLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectBlueprintDefinition(UObject* Object);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectEditorWorldConfiguration();
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectStaticMeshMetadata(UObject* Object);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectComponentAuthoringData(UObject* Object);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectWidgetAnimationData(UObject* Object);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectPCGExtractionSettings(UObject* Object);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectEditorPerformanceSettings();
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectNiagaraParticlePositions(const FString& ComponentPath,int32 EmitterIndex,int32 MaxParticles);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString RerunActorConstruction(UObject* Object,bool Execute);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString RefreshComponentRenderState(UObject* Object,bool Execute);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString SetAssetPackageDirty(UObject* Object,bool Dirty,bool AllowClear);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString SpawnEditorActorDeferred(UClass* Class,const FString& Name,const FTransform& Transform,bool Execute);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString MaterialComments(UObject* Asset,const FString& Action,const FString& Text,int32 X,int32 Y,int32 Width,int32 Height,bool Execute,bool Save);
};
