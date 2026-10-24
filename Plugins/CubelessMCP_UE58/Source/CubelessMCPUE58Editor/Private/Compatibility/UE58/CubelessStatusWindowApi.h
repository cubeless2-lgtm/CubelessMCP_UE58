#pragma once
#include "CoreMinimal.h"

namespace Cubeless::UE58::StatusWindow
{
    FString Show(const FString& Text, const FString& Command);
    // Editor game thread only; unregister closes UI, module/engine shutdown also retires calls.
    void Close();
    void Shutdown();
}
