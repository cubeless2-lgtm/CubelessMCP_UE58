#pragma once
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessBlueprintWidgetFallbackLibrary.generated.h"
UCLASS()
class CUBELESSMCPUE58EDITOR_API UCubelessBlueprintWidgetFallbackLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable,Category="Cubeless|Blueprint",meta=(AIIgnore,ScriptName="analyze_blueprint_widget_fallbacks_mcp"))
    static FString AnalyzeBlueprintWidgetFallbacksMCP(const FString& SourceRoot,const FString& TargetRoot,const FString& Suffix);
};
