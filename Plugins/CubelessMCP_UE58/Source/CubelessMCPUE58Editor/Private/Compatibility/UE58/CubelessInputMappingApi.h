#pragma once
#include "Operations/CubelessVariableOperations.h"
namespace Cubeless::UE58::InputMapping
{
    Variable::FResult Create(const FString& ActionName, const FString& Key, const FString& InputType,
        bool bShift, bool bCtrl, bool bAlt, bool bCmd);
}
