#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUMGUtilityLibrary.generated.h"
UCLASS()
class UCubelessUMGUtilityLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_checkbox_to_widget"))
    static FString AddCheckboxToWidget(const FString& ParamsJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_widget_to_viewport"))
    static FString AddWidgetToViewport(const FString& WidgetName,int64 ZOrder);
};
