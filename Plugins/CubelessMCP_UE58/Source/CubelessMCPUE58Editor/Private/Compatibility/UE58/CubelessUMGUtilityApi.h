#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"
namespace Cubeless::UE58::UMGUtility
{
Variable::FResult AddCheckbox(const FString& ParamsJson);
Variable::FResult ViewportReadiness(const FString& WidgetName,int64 ZOrder);
}
