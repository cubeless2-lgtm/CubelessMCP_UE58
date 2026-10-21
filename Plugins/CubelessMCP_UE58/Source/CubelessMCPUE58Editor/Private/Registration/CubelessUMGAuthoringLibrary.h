#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUMGAuthoringLibrary.generated.h"
UCLASS()
class UCubelessUMGAuthoringLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="create_umg_widget_blueprint"))
    static FString CreateUMGWidgetBlueprint(const FString& WidgetName,const FString& ParentClass,const FString& Path);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_text_block_to_widget"))
    static FString AddTextBlockToWidget(const FString& WidgetName,const FString& TextBlockName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson);
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="add_button_to_widget"))
    static FString AddButtonToWidget(const FString& WidgetName,const FString& ButtonName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson,const FString& BackgroundColorJson);
};
