#include "RXImpostorMaterial.h"

#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"

void RXImpostorMaterial::SetUpNormals(UMaterial* Material)
{
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data) { return; }
    auto* Normal = Cast<UMaterialExpressionVertexNormalWS>(Data->Normal.Expression);
    if (!Normal)
    {
        Normal = NewObject<UMaterialExpressionVertexNormalWS>(Material, NAME_None, Material->HasAnyFlags(RF_Transient) ? RF_Transient : RF_Transactional);
        Material->GetExpressionCollection().AddExpression(Normal);
    }
    // The stored vertex normal is local +Z. World-space material input keeps it
    // aligned with the mesh's up axis on both sides without TwoSidedSign flipping.
    Material->bTangentSpaceNormal = false;
    Data->Normal.Connect(0, Normal);
}

void RXImpostorMaterial::SetUpNoCastShadow(UMaterial* Material)
{
    auto* Data = Material->GetEditorOnlyData();
    if (!Data || !Data->OpacityMask.Expression) { return; }
    if (Cast<UMaterialExpressionShadowReplace>(Data->OpacityMask.Expression)) { return; }
    const EObjectFlags Flags = Material->HasAnyFlags(RF_Transient) ? RF_Transient : RF_Transactional;
    auto* Switch = NewObject<UMaterialExpressionShadowReplace>(Material, NAME_None, Flags);
    auto* Zero = NewObject<UMaterialExpressionConstant>(Material, NAME_None, Flags);
    Zero->R = 0.0f;
    Switch->Default = Data->OpacityMask;
    Switch->Shadow.Connect(0, Zero);
    Material->GetExpressionCollection().AddExpression(Switch);
    Material->GetExpressionCollection().AddExpression(Zero);
    Data->OpacityMask.Connect(0, Switch);
}

bool RXImpostorMaterial::CreatePreview(const TArray<FColor>& Pixels, FIntPoint Size,
    FRXImpostorPreviewMaterial& Output, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || Size.X <= 0 || Size.Y <= 0 || Size.X > 8192 || Size.Y > 8192 ||
        int64(Size.X) * Size.Y != Pixels.Num())
    {
        Error = TEXT("Preview material requires valid atlas pixels on the game thread.");
        return false;
    }
    TStrongObjectPtr<UTexture2D> Texture(NewObject<UTexture2D>(GetTransientPackage(), NAME_None, RF_Transient));
    Texture->Source.Init(Size.X, Size.Y, 1, 1, TSF_BGRA8, reinterpret_cast<const uint8*>(Pixels.GetData()));
    Texture->SRGB = true;
    Texture->AddressX = TA_Clamp;
    Texture->AddressY = TA_Clamp;
    Texture->UpdateResource();

    TStrongObjectPtr<UMaterial> Material(NewObject<UMaterial>(GetTransientPackage(), NAME_None, RF_Transient));
    Material->BlendMode = BLEND_Masked;
    Material->TwoSided = true;
    Material->OpacityMaskClipValue = 0.333f;
    Material->SetShadingModel(MSM_DefaultLit);
    UMaterialExpressionTextureSample* Sample = NewObject<UMaterialExpressionTextureSample>(Material.Get(), NAME_None, RF_Transient);
    Sample->Texture = Texture.Get();
    Sample->SamplerType = SAMPLERTYPE_Color;
    Material->GetExpressionCollection().AddExpression(Sample);
    UMaterialEditorOnlyData* Data = Material->GetEditorOnlyData();
    if (!Data)
    {
        Error = TEXT("Material editor-only data is unavailable.");
        return false;
    }
    Data->BaseColor.Connect(0, Sample);
    Data->OpacityMask.Connect(4, Sample);
    SetUpNormals(Material.Get());
    SetUpNoCastShadow(Material.Get());
    Material->PostEditChange();
    Output.Texture = MoveTemp(Texture);
    Output.Material = MoveTemp(Material);
    return true;
}
