#pragma once

#include "CoreMinimal.h"

class UStaticMesh;

namespace RXImpostorCapture
{
    // Captures two or four orthographic directions into transient CPU pixel arrays.
    // The preview world, component and render target are never saved.
    bool Capture(UStaticMesh* Source, FIntPoint TileSize, TArray<TArray<FColor>>& Tiles, FString& Error, int32 Directions = 4);
}
