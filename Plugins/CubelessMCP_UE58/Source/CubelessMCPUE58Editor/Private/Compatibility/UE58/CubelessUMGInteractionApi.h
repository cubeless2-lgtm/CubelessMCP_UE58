#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::UMGInteraction
{
Variable::FResult AddSliders(const FString& WidgetName,const FString& SlidersJson);
Variable::FResult BindEvent(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName);
Variable::FResult BindEventRaw(const FString& WidgetName,const FString& WidgetComponentName,const FString& EventName,const FString& FunctionName,const FString& NodePositionJson);
}
