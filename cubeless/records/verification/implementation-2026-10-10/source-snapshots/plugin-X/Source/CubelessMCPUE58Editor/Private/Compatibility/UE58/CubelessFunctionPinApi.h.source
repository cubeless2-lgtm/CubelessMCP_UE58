#pragma once
#include "Compatibility/UE58/CubelessFunctionApi.h"
#include "EdGraph/EdGraphPin.h"
namespace Cubeless::UE58::Function::Pin
{
bool SafeName(const FString& Text, bool bEmpty = false);
bool Identifier(const FString& Text);
bool ParseJson(const FString& Text, TSharedPtr<FJsonValue>& Value, FResult& Result);
bool PrimitiveType(const FString& Name, FEdGraphPinType& Type, FResult& Result);
bool IsPrimitive(const FEdGraphPinType& Type);
bool DefaultText(const FEdGraphPinType& Type, const TSharedPtr<FJsonValue>& Value, FString& Text, FResult& Result);
TSharedRef<FJsonObject> Describe(const FEdGraphPinType& Type);
}
