#pragma once
#include "CoreMinimal.h"
namespace Cubeless::UE58::NiagaraPreviewLab
{
    FString GetState(const FString& ParamsJson);
    FString Cleanup(const FString& ParamsJson);
    FString CaptureView(const FString& ParamsJson);
    FString PreviewSystem(const FString& ParamsJson);
}
