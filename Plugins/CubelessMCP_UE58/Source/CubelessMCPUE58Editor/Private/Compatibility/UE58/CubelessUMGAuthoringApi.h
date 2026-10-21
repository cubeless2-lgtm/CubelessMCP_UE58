#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::UMGAuthoring
{
Variable::FResult Create(const FString& WidgetName,const FString& ParentClass,const FString& Path);
Variable::FResult AddText(const FString& WidgetName,const FString& TextBlockName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson);
Variable::FResult AddButton(const FString& WidgetName,const FString& ButtonName,const FString& Text,const FString& PositionJson,const FString& SizeJson,int64 FontSize,const FString& ColorJson,const FString& BackgroundColorJson);
}
