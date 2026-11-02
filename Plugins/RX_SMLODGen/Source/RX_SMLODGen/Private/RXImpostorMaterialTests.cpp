#if WITH_DEV_AUTOMATION_TESTS

#include "RXImpostorMaterial.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "Misc/AutomationTest.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXImpostorMaterialTest, "RX.SMLODGen.ImpostorMaterial",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXImpostorMaterialTest::RunTest(const FString& Parameters)
{
    TArray<FColor> Pixels;
    Pixels.Init(FColor(120, 100, 80, 255), 16);
    FRXImpostorPreviewMaterial Preview;
    FString Error;
    if (!TestTrue(TEXT("Create transient preview material"), RXImpostorMaterial::CreatePreview(Pixels, FIntPoint(4, 4), Preview, Error)))
    {
        AddError(Error);
        return false;
    }
    TestTrue(TEXT("Texture is transient"), Preview.Texture->HasAnyFlags(RF_Transient));
    TestTrue(TEXT("Material is transient, masked and two-sided"), Preview.Material->HasAnyFlags(RF_Transient) &&
        Preview.Material->GetBlendMode() == BLEND_Masked && Preview.Material->IsTwoSided());
    UMaterialEditorOnlyData* Data = Preview.Material->GetEditorOnlyData();
    if (TestNotNull(TEXT("Material graph data"), Data))
    {
        TestTrue(TEXT("Normal uses world-space vertex normal without two-sided tangent flip"), !Preview.Material->bTangentSpaceNormal && Data->Normal.Expression && Data->Normal.Expression->IsA<UMaterialExpressionVertexNormalWS>());
        UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Data->BaseColor.Expression);
        TestTrue(TEXT("Base color and opacity use the generated atlas"), Sample && Sample->Texture == Preview.Texture.Get() &&
            Data->OpacityMask.Expression == Sample && Data->OpacityMask.OutputIndex == 4);
    }
    UTexture2D* ExistingTexture = Preview.Texture.Get();
    Pixels.Pop();
    TestFalse(TEXT("Invalid atlas size is rejected"), RXImpostorMaterial::CreatePreview(Pixels, FIntPoint(4, 4), Preview, Error));
    TestTrue(TEXT("Invalid input retains previous output"), Preview.Texture.Get() == ExistingTexture);
    return true;
}

#endif
