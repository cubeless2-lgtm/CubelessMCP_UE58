#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUMGInteractionLibrary.generated.h"
UCLASS()
class UCubelessUMGInteractionLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_sliders_to_widget"))
    static FString AddSlidersToWidget(const FString& WidgetName,const FString& SlidersJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="bind_widget_event"))
    static FString BindWidgetEvent(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="bind_widget_event_raw"))
    static FString BindWidgetEventRaw(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName,const FString& NodePositionJson);
};
