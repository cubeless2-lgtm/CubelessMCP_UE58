#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessUMGTextBindingLibrary.generated.h"
UCLASS()
class UCubelessUMGTextBindingLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Internal",meta=(AIIgnore,ScriptName="set_text_block_binding"))
    static FString SetTextBlockBinding(const FString& WidgetName,const FString& TextBlockName,const FString& BindingProperty,const FString& BindingType);
};
