#include "RXVolumeLOD.h"

#if WITH_DEV_AUTOMATION_TESTS

#include "Engine/SkeletalMesh.h"
#include "Engine/Texture2D.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionComponentMask.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionLinearInterpolate.h"
#include "Materials/MaterialExpressionTextureCoordinate.h"
#include "MeshDescription.h"
#include "Misc/AutomationTest.h"
#include "SkinnedAssetCompiler.h"
#include "StaticMeshAttributes.h"
#include "UObject/StrongObjectPtr.h"

namespace
{
template<typename ExpressionType>
ExpressionType* AddExpression(UMaterial& Material)
{
    ExpressionType* Expression = NewObject<ExpressionType>(&Material);
    Material.GetExpressionCollection().AddExpression(Expression);
    return Expression;
}

UMaterial* MakeHeightMaterial()
{
    UMaterial* Material = NewObject<UMaterial>(GetTransientPackage());
    UMaterialExpressionTextureCoordinate* UV = AddExpression<UMaterialExpressionTextureCoordinate>(*Material);
    UMaterialExpressionComponentMask* U = AddExpression<UMaterialExpressionComponentMask>(*Material);
    U->Input.Connect(0, UV);
    U->R = true;
    U->G = U->B = U->A = false;

    UMaterialExpressionConstant3Vector* Bottom = AddExpression<UMaterialExpressionConstant3Vector>(*Material);
    Bottom->Constant = FLinearColor::Red;
    UMaterialExpressionConstant3Vector* Top = AddExpression<UMaterialExpressionConstant3Vector>(*Material);
    Top->Constant = FLinearColor::Blue;
    UMaterialExpressionLinearInterpolate* Gradient = AddExpression<UMaterialExpressionLinearInterpolate>(*Material);
    Gradient->A.Connect(0, Bottom);
    Gradient->B.Connect(0, Top);
    Gradient->Alpha.Connect(0, U);
    Material->GetEditorOnlyData()->BaseColor.Connect(0, Gradient);

    UMaterialExpressionConstant* Roughness = AddExpression<UMaterialExpressionConstant>(*Material);
    Roughness->R = 0.25f;
    Material->GetEditorOnlyData()->Roughness.Connect(0, Roughness);
    UMaterialExpressionConstant* Metallic = AddExpression<UMaterialExpressionConstant>(*Material);
    Metallic->R = 0.75f;
    Material->GetEditorOnlyData()->Metallic.Connect(0, Metallic);
    Material->SetUsageByFlag(MATUSAGE_SkeletalMesh, true);
    Material->PostEditChange();
    return Material;
}

float HeightToU(float Z, float Bottom, float Height)
{
    return 0.10f + 0.80f * FMath::Clamp((Z - Bottom) / Height, 0.0f, 1.0f);
}

bool ReadPixels(UTexture2D* Texture, int32 Size, TArray64<uint8>& Pixels)
{
    return Texture && Texture->Source.GetFormat() == TSF_BGRA8
        && Texture->Source.GetSizeX() == Size && Texture->Source.GetSizeY() == Size
        && Texture->Source.GetMipData(Pixels, 0) && Pixels.Num() == 4ll * Size * Size;
}

FColor PixelAt(const TArray64<uint8>& Pixels, int64 Index)
{
    const uint8* BGRA = Pixels.GetData() + Index * 4;
    return FColor(BGRA[2], BGRA[1], BGRA[0], BGRA[3]);
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXVolumeLODColorPlacementTest, "RX.SkeletalVolumeLOD.ColorPlacement",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXVolumeLODColorPlacementTest::RunTest(const FString& Parameters)
{
    USkeletalMesh* Original = LoadObject<USkeletalMesh>(nullptr,
        TEXT("/Game/Characters/UEFN_Mannequin/Meshes/SKM_UEFN_Mannequin.SKM_UEFN_Mannequin"));
    if (!TestNotNull(TEXT("Load mannequin geometry for the controlled color fixture"), Original))
    {
        return false;
    }
    USkinnedAsset* OriginalCompilation[] = { Original };
    FSkinnedAssetCompilingManager::Get().FinishCompilation(OriginalCompilation);
    TStrongObjectPtr<USkeletalMesh> Source(DuplicateObject<USkeletalMesh>(Original, GetTransientPackage()));
    if (!TestTrue(TEXT("Controlled fixture is a private transient mesh"), Source.IsValid()))
    {
        return false;
    }

    USkinnedAsset* FixtureCompilation[] = { Source.Get() };
    FSkinnedAssetCompilingManager::Get().FinishCompilation(FixtureCompilation);
    FMeshDescription* SourceDescription = Source->GetMeshDescription(0);
    if (!TestTrue(TEXT("Controlled fixture has source geometry and material slots"),
            SourceDescription && SourceDescription->Vertices().Num() > 0 && !Source->GetMaterials().IsEmpty()))
    {
        return false;
    }
    float Bottom = TNumericLimits<float>::Max();
    float Top = TNumericLimits<float>::Lowest();
    for (const FVertexID Vertex : SourceDescription->Vertices().GetElementIDs())
    {
        const float Z = SourceDescription->GetVertexPosition(Vertex).Z;
        Bottom = FMath::Min(Bottom, Z);
        Top = FMath::Max(Top, Z);
    }
    const float Height = Top - Bottom;
    if (!TestTrue(TEXT("Controlled fixture has positive height"), Height > 0.0f))
    {
        return false;
    }

    auto SourceUVs = FStaticMeshAttributes(*SourceDescription).GetVertexInstanceUVs();
    if (!TestTrue(TEXT("Controlled fixture has source UV0"), SourceUVs.IsValid() && SourceUVs.GetNumChannels() > 0))
    {
        return false;
    }
    // Author the UVs read by the skeletal material and leave the older shared UVs untouched.
    for (const FVertexInstanceID Corner : SourceDescription->VertexInstances().GetElementIDs())
    {
        const float Z = SourceDescription->GetVertexPosition(SourceDescription->GetVertexInstanceVertex(Corner)).Z;
        const FVector2f OldUV = SourceUVs.Get(Corner, 0);
        SourceUVs.Set(Corner, 0, FVector2f(HeightToU(Z, Bottom, Height), OldUV.Y));
    }
    if (!TestTrue(TEXT("Commit height-encoded UVs only on the transient fixture"), Source->CommitMeshDescription(0)))
    {
        return false;
    }
    TStrongObjectPtr<UMaterial> HeightMaterial(MakeHeightMaterial());
    for (FSkeletalMaterial& MaterialSlot : Source->GetMaterials())
    {
        MaterialSlot.MaterialInterface = HeightMaterial.Get();
    }

    const FString OutputPath = TEXT("/Game/_MCP_Temp/SkeletalVolumeLOD/CubelessTests/")
        + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT("/SK_ControlledColor");
    RXVolumeLOD::FOptions Options;
    Options.TextureSize = 512;
    RXVolumeLOD::FResult Result;
    FText Error;
    if (!RXVolumeLOD::GenerateLOD1(Source.Get(), OutputPath, Options, Result, Error))
    {
        AddError(FString::Printf(TEXT("Controlled color LOD failed: %s"), *Error.ToString()));
        return false;
    }

    TArray64<uint8> BaseColor;
    TArray64<uint8> Roughness;
    TArray64<uint8> Metallic;
    const int32 Size = Options.TextureSize;
    const FMeshDescription* Target = Result.Mesh ? Result.Mesh->GetMeshDescription(1) : nullptr;
    if (!TestTrue(TEXT("Read the generated LOD1 and its three texture channels"), Target
            && ReadPixels(Result.BaseColor, Size, BaseColor) && ReadPixels(Result.Roughness, Size, Roughness)
            && ReadPixels(Result.Metallic, Size, Metallic)))
    {
        return false;
    }

    const auto UVs = FStaticMeshConstAttributes(*Target).GetVertexInstanceUVs();
    if (!TestTrue(TEXT("Generated geometry contains target UV0"), UVs.IsValid() && UVs.GetNumChannels() > 0))
    {
        return false;
    }
    int32 Compared = 0;
    int32 ColorMatches = 0;
    int32 MaskMatches = 0;
    int32 LowerSamples = 0;
    int32 UpperSamples = 0;
    for (const FTriangleID Triangle : Target->Triangles().GetElementIDs())
    {
        const TArrayView<const FVertexInstanceID> Corners = Target->GetTriangleVertexInstances(Triangle);
        const FVector2f A = UVs.Get(Corners[0], 0);
        const FVector2f B = UVs.Get(Corners[1], 0);
        const FVector2f C = UVs.Get(Corners[2], 0);
        const FVector2f Center = (A + B + C) / 3.0f;
        const FIntPoint Texel(FMath::FloorToInt(Center.X * Size), FMath::FloorToInt(Center.Y * Size));
        if (Texel.X < 0 || Texel.X >= Size || Texel.Y < 0 || Texel.Y >= Size)
        {
            continue;
        }
        const FVector2f PixelUV((Texel.X + 0.5f) / Size, (Texel.Y + 0.5f) / Size);
        const float UVArea = FVector2f::CrossProduct(B - A, C - A);
        if (FMath::Abs(UVArea) < 1.e-10f)
        {
            continue;
        }
        const float WeightB = FVector2f::CrossProduct(PixelUV - A, C - A) / UVArea;
        const float WeightC = FVector2f::CrossProduct(B - A, PixelUV - A) / UVArea;
        const float WeightA = 1.0f - WeightB - WeightC;
        if (FMath::Min3(WeightA, WeightB, WeightC) < 0.15f)
        {
            continue;
        }

        const float Z = WeightA * Target->GetVertexPosition(Target->GetVertexInstanceVertex(Corners[0])).Z
            + WeightB * Target->GetVertexPosition(Target->GetVertexInstanceVertex(Corners[1])).Z
            + WeightC * Target->GetVertexPosition(Target->GetVertexInstanceVertex(Corners[2])).Z;
        const float ExpectedBlue = HeightToU(Z, Bottom, Height);
        const int64 Index = static_cast<int64>(Texel.Y) * Size + Texel.X;
        const FLinearColor Actual = FLinearColor::FromSRGBColor(PixelAt(BaseColor, Index));
        ++Compared;
        LowerSamples += ExpectedBlue < 0.35f;
        UpperSamples += ExpectedBlue > 0.65f;
        ColorMatches += FMath::Abs(Actual.R - (1.0f - ExpectedBlue)) <= 0.10f
            && FMath::Abs(Actual.B - ExpectedBlue) <= 0.10f && Actual.G <= 0.06f;
        MaskMatches += FMath::Abs(PixelAt(Roughness, Index).R / 255.0f - 0.25f) <= 0.04f
            && FMath::Abs(PixelAt(Metallic, Index).R / 255.0f - 0.75f) <= 0.04f;
    }

    AddInfo(FString::Printf(TEXT("Controlled atlas: %d/%d spatial Base Color matches, %d linear mask matches, %d lower and %d upper samples"),
        ColorMatches, Compared, MaskMatches, LowerSamples, UpperSamples));
    TestTrue(TEXT("Sample enough interior texels from both ends of the character"), Compared >= 100 && LowerSamples >= 10 && UpperSamples >= 10);
    TestTrue(TEXT("At least 90 percent of samples retain the expected red-to-blue position"), ColorMatches * 10 >= Compared * 9);
    TestTrue(TEXT("At least 90 percent of samples preserve both linear scalar channels"), MaskMatches * 10 >= Compared * 9);
    return !HasAnyErrors();
}

#endif
