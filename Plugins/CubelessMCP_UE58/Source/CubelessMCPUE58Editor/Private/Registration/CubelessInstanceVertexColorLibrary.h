#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessInstanceVertexColorLibrary.generated.h"

// Editor authoring only. Never call or reference this library in game assets.
UCLASS()
class UCubelessInstanceVertexColorLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString InspectInstanceVertexColors(UObject* Component,int32 LODIndex=0,int32 StartIndex=0,int32 Count=256,bool IncludePositions=false);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString SetInstanceVertexColors(UObject* Component,const FString& ColorsJson,int32 LODIndex=0,bool Execute=false,bool AllowReplace=false,bool Save=false,bool AllowSaveDirtyPackage=false);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,DevelopmentOnly))
 static FString ClearInstanceVertexColors(UObject* Component,int32 LODIndex=0,bool Execute=false,bool AllowRemove=false,bool Save=false,bool AllowSaveDirtyPackage=false);
};
