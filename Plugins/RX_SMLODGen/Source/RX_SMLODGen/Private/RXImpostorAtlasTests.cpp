#if WITH_DEV_AUTOMATION_TESTS

#include "RXImpostorAtlas.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXImpostorAtlasTest, "RX.SMLODGen.ImpostorAtlas",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXImpostorAtlasTest::RunTest(const FString& Parameters)
{
    TArray<TArray<FColor>> Tiles;
    for (uint8 Tile = 0; Tile < 4; ++Tile)
    {
        Tiles.Add({ FColor(Tile, 10, 20, 30), FColor(Tile, 11, 21, 31),
                    FColor(Tile, 12, 22, 32), FColor(Tile, 13, 23, 33) });
    }
    TArray<FColor> Pixels;
    FIntPoint Size;
    FString Error;
    if (!TestTrue(TEXT("Pack four image tiles"), RXImpostorAtlas::Pack(Tiles, FIntPoint(2, 2), 1, Pixels, Size, Error)))
    {
        AddError(Error);
        return false;
    }
    TestEqual(TEXT("Atlas width"), Size.X, 16);
    TestEqual(TEXT("Atlas height"), Size.Y, 4);
    for (int32 Tile = 0; Tile < 4; ++Tile)
    {
        TestTrue(TEXT("Top-left padding preserves alpha and tile"), Pixels[Tile * 4] == Tiles[Tile][0]);
        TestTrue(TEXT("Bottom-right padding preserves alpha and tile"), Pixels[3 * Size.X + Tile * 4 + 3] == Tiles[Tile][3]);
    }
    const int32 OriginalCount = Pixels.Num();
    Tiles[0].Pop();
    TestFalse(TEXT("Reject malformed tile"), RXImpostorAtlas::Pack(Tiles, FIntPoint(2, 2), 1, Pixels, Size, Error));
    TestEqual(TEXT("Invalid input preserves output"), Pixels.Num(), OriginalCount);
    return true;
}

#endif
