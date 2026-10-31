#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimationEditingLibrary.generated.h"

UCLASS()
class UCubelessAnimationEditingLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 // Fixed internal adapter; individual operations are the public Python tools.
 UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore,DevelopmentOnly))
 static FString EditAnimationAsset(UObject* Asset, const FString& Action,
     const FString& ArgumentsJson, bool Execute=false, bool Save=false,
     bool AllowSaveDirtyPackage=false, const FString& RequestToken=TEXT(""), bool UnobservedAssetLoad=false);
 UFUNCTION(BlueprintCallable, Category="Cubeless|Internal", meta=(AIIgnore,DevelopmentOnly))
 static FString TakeAnimationEditReceipt(const FString& RequestToken);
};
