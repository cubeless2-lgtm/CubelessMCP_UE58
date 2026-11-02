#if WITH_DEV_AUTOMATION_TESTS

#include "RXImpostorCapture.h"
#include "Engine/StaticMesh.h"
#include "Misc/AutomationTest.h"
#include "RHI.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXImpostorCaptureTest, "RX.SMLODGen.ImpostorCapture",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXImpostorCaptureTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Engine cube exists"), Mesh))
    {
        return false;
    }
    TArray<TArray<FColor>> Tiles;
    FString Error;
    if (GUsingNullRHI)
    {
        TestFalse(TEXT("Capture refuses a null rendering device"), RXImpostorCapture::Capture(Mesh, FIntPoint(64, 64), Tiles, Error));
        return true;
    }
    if (!TestTrue(TEXT("Capture four views"), RXImpostorCapture::Capture(Mesh, FIntPoint(64, 64), Tiles, Error)))
    {
        AddError(Error);
        return false;
    }
    if (!TestEqual(TEXT("Four tiles returned"), Tiles.Num(), 4))
    {
        return false;
    }
    for (const TArray<FColor>& Tile : Tiles)
    {
        TestEqual(TEXT("Tile dimensions"), Tile.Num(), 64 * 64);
        int32 Visible = 0;
        int32 Transparent = 0;
        int32 Colored = 0;
        for (const FColor Pixel : Tile)
        {
            Visible += Pixel.A > 127 ? 1 : 0;
            Transparent += Pixel.A < 16 ? 1 : 0;
            Colored += Pixel.A > 127 && (Pixel.R > 4 || Pixel.G > 4 || Pixel.B > 4) ? 1 : 0;
        }
        TestTrue(TEXT("Capture contains opaque object pixels"), Visible > 0);
        TestTrue(TEXT("Capture contains transparent background"), Transparent > 0);
        TestTrue(TEXT("Visible pixels contain base color"), Colored > 0);
    }
    return true;
}
#endif
