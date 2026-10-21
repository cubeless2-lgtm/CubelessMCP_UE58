#pragma once
#include "Compatibility/UE58/CubelessVariableApi.h"

namespace Cubeless::UE58::ActorPropertyFocus
{
Variable::FResult SetProperty(const FString& ParamsJson);
Variable::FResult Focus(const FString& ParamsJson);
}
