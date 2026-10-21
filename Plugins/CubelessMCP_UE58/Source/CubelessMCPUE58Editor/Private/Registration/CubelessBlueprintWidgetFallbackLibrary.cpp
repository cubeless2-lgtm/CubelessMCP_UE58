#include "Registration/CubelessBlueprintWidgetFallbackLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintWidgetFallbackApi.h"
#include "Operations/CubelessVariableOperations.h"
FString UCubelessBlueprintWidgetFallbackLibrary::AnalyzeBlueprintWidgetFallbacksMCP(const FString& SourceRoot,const FString& TargetRoot,const FString& Suffix)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::BlueprintWidgetFallback::Analyze(SourceRoot,TargetRoot,Suffix),TEXT("analyze_blueprint_widget_fallbacks_mcp"));
}
