#pragma once

#include "CoreMinimal.h"

namespace RXImpostorAtlas
{
    // Packs four top-to-bottom image tiles into one horizontal atlas. Padding
    // extends edge pixels including alpha; it does not bake or alter lighting.
    bool Pack(const TArray<TArray<FColor>>& Tiles, FIntPoint TileSize, int32 Padding,
        TArray<FColor>& Pixels, FIntPoint& AtlasSize, FString& Error);
}
