#include "RXImpostorAtlas.h"

bool RXImpostorAtlas::Pack(const TArray<TArray<FColor>>& Tiles, FIntPoint TileSize, int32 Padding,
    TArray<FColor>& Pixels, FIntPoint& AtlasSize, FString& Error)
{
    Error.Reset();
    if (Tiles.Num() != 4 || TileSize.X <= 0 || TileSize.Y <= 0 || Padding < 0 || Padding > 64)
    {
        Error = TEXT("Exactly four equally sized tiles and padding from 0 to 64 are required.");
        return false;
    }
    const int64 TilePixels = int64(TileSize.X) * TileSize.Y;
    const int64 PaddedWidth = int64(TileSize.X) + 2 * Padding;
    const int64 PaddedHeight = int64(TileSize.Y) + 2 * Padding;
    const int64 Width = PaddedWidth * 4;
    if (Width > 8192 || PaddedHeight > 8192 || TilePixels > MAX_int32)
    {
        Error = TEXT("The atlas exceeds the 8192 pixel size limit.");
        return false;
    }
    for (const TArray<FColor>& Tile : Tiles)
    {
        if (Tile.Num() != TilePixels)
        {
            Error = TEXT("Tile pixel counts do not match the declared size.");
            return false;
        }
    }
    TArray<FColor> Result;
    Result.SetNumUninitialized(int32(Width * PaddedHeight));
    for (int32 TileIndex = 0; TileIndex < 4; ++TileIndex)
    {
        for (int32 Y = 0; Y < int32(PaddedHeight); ++Y)
        {
            const int32 SourceY = FMath::Clamp(Y - Padding, 0, TileSize.Y - 1);
            for (int32 X = 0; X < int32(PaddedWidth); ++X)
            {
                const int32 SourceX = FMath::Clamp(X - Padding, 0, TileSize.X - 1);
                Result[Y * int32(Width) + TileIndex * int32(PaddedWidth) + X] = Tiles[TileIndex][SourceY * TileSize.X + SourceX];
            }
        }
    }
    Pixels = MoveTemp(Result);
    AtlasSize = FIntPoint(int32(Width), int32(PaddedHeight));
    return true;
}
