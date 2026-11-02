#pragma once

#include "CoreMinimal.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "UObject/StrongObjectPtr.h"

struct FRXImpostorPreviewMaterial
{
    TStrongObjectPtr<UTexture2D> Texture;
    TStrongObjectPtr<UMaterial> Material;
};

namespace RXImpostorMaterial
{
    // Produces transient editor-preview objects only. The caller owns their lifetime.
    bool CreatePreview(const TArray<FColor>& Pixels, FIntPoint Size, FRXImpostorPreviewMaterial& Output, FString& Error);
    void SetUpNormals(UMaterial* Material);
}
