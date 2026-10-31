#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessAnimationProductionLibrary.generated.h"
UCLASS()
class UCubelessAnimationProductionLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString CreateAnimationProductionAsset(const FString& Kind,const FString& Path,UObject* Reference,bool Execute=false,bool Save=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString InspectAnimationProductionAsset(UObject* Target);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString EditAnimationProductionAsset(UObject* Target,const FString& Action,const FString& Arguments,bool Execute=false,bool Save=false,bool AllowDirty=false,bool AllowRemove=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString AnimationProductionIndex(UObject* Target,const FString& JobId,bool Start=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString BindAnimationProductionNode(UObject* Blueprint,const FString& Action,const FString& Graph,const FString& NodeId,const FString& Source,bool Execute=false,bool Compile=false,bool Save=false,bool AllowDirty=false,bool AllowReplace=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString EvaluateAnimationProductionChooser(UObject* Target,UObject* Context);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString BindAnimationProductionChooser(UObject* Blueprint,const FString& Graph,const FString& NodeId,const FString& ChooserPath,const FString& FunctionName,bool Execute=false,bool Compile=false,bool Save=false,bool AllowDirty=false,bool AllowReplace=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString CreateAnimationProductionLayer(UObject* Blueprint,const FString& Name,bool Execute=false,bool Compile=false,bool Save=false,bool AllowDirty=false);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString InspectAnimationStateGraph(UObject* Blueprint,const FString& Graph);
 UFUNCTION(BlueprintCallable,meta=(AIIgnore,DevelopmentOnly))
 static FString EditAnimationStateGraph(UObject* Blueprint,const FString& Action,const FString& Graph,const FString& Arguments,bool Execute=false,bool Compile=false,bool Save=false,bool AllowDirty=false,bool AllowDestructive=false);
};
