#pragma once

#include "CoreMinimal.h"
#include "RXImpostorPreview.h"

struct FRXImpostorAssets
{
    TStrongObjectPtr<UTexture2D> Texture;
    TStrongObjectPtr<UMaterial> Material;
    TArray<FString> CreatedPackages;
    TArray64<uint8> ReplacementPixels;
    FIntPoint ReplacementSize = FIntPoint::ZeroValue;
};

namespace RXImpostorAssets
{
    // Creates unsaved packages next to a /Game source asset. It never overwrites
    // existing names; partial creations are returned in CreatedPackages.
    bool Create(UStaticMesh* Original, const FRXImpostorPreview& Preview, FRXImpostorAssets& Output, FString& Error);
    // Called inside the apply transaction after all preflight checks.
    void CommitReplacement(FRXImpostorAssets& Assets);
}
