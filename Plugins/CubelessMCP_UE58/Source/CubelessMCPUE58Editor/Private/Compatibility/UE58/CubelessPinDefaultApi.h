#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
namespace Cubeless::UE58::PinDefault
{
Function::FResult Set(const FString& GraphPath, const FString& NodePath, const FString& PinName,
    const FString& ValueJson, const FString& Direction);
}
