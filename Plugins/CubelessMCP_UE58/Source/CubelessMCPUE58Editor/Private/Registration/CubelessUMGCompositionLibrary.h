#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUMGCompositionLibrary.generated.h"
class UWidget;
UCLASS()
class UCubelessUMGCompositionLibrary : public UBlueprintFunctionLibrary
{
 GENERATED_BODY()
public:
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="compose_widget_blueprint"))
 static FString Compose(const FString& AssetPath,const FString& NodesJson);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_widget_animation"))
 static FString Animate(const FString& AssetPath,const FString& AnimationName,double Duration,const FString& TracksJson);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="inspect_widget_blueprint"))
 static FString Inspect(const FString& AssetPath);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="finalize_widget_blueprint"))
 static FString Finalize(const FString& AssetPath,bool Save);
 UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="get_source_widget"))
 static UWidget* SourceWidget(const FString& AssetPath,const FString& WidgetName);
};
