#include "RXImpostorAssets.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/MetaData.h"
#include "RXWeightMetadata.h"

bool RXImpostorAssets::Create(UStaticMesh* Original, const FRXImpostorPreview& Preview,
    FRXImpostorAssets& Output, FString& Error)
{
    Error.Reset();
    if (!IsInGameThread() || !Original || !Preview.Material.Texture.IsValid() || !Preview.Material.Material.IsValid())
    { Error = TEXT("Original mesh or transient impostor assets are unavailable."); return false; }
    const FString OriginalPackage = Original->GetOutermost()->GetName();
    if (!OriginalPackage.StartsWith(TEXT("/Game/")))
    { Error = TEXT("Impostor asset creation requires an original mesh under /Game."); return false; }
    int32 SampleCount = 0;
    for (UMaterialExpression* Expression : Preview.Material.Material->GetExpressions())
    {
        if (const UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
        {
            if (Sample->Texture == Preview.Material.Texture.Get()) { ++SampleCount; }
        }
    }
    if (SampleCount != 1) { Error = TEXT("The preview material's atlas binding is ambiguous."); return false; }

    const FString Metadata = Original->GetOutermost()->GetMetaData().GetValue(Original, RXWeightMetadata::Key());
    if (!Metadata.IsEmpty())
    {
        FString Source; TMap<int32, FRXStoredLODWeights> LODs; int32 LOD = INDEX_NONE, Slot = INDEX_NONE;
        if (!RXWeightMetadata::Decode(Metadata, Source, LODs, LOD, Slot, Error)) { return false; }
        if (LOD != INDEX_NONE)
        {
            UMaterial* Material = Original->GetStaticMaterials().IsValidIndex(Slot) ? Cast<UMaterial>(Original->GetStaticMaterials()[Slot].MaterialInterface) : nullptr;
            UTexture2D* Texture = nullptr; int32 Samples = 0;
            if (Material)
                for (UMaterialExpression* Expression : Material->GetExpressions())
                    if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression)) { Texture = Cast<UTexture2D>(Sample->Texture); ++Samples; }
            const FString Owner = Original->GetPathName();
            if (!Material || !Texture || Samples != 1 ||
                Material->GetOutermost()->GetMetaData().GetValue(Material, TEXT("RX_SMLODGen.Owner")) != Owner ||
                Texture->GetOutermost()->GetMetaData().GetValue(Texture, TEXT("RX_SMLODGen.Owner")) != Owner)
            { Error = TEXT("Existing impostor asset ownership is unproven. It cannot be overwritten safely."); return false; }
            if (Material->GetOutermost()->IsDirty() || Texture->GetOutermost()->IsDirty())
            { Error = TEXT("Save existing impostor material and atlas before replacing them."); return false; }
            if (!Preview.Material.Texture->Source.GetMipData(Output.ReplacementPixels, 0) || Preview.Material.Texture->Source.GetFormat() != TSF_BGRA8)
            { Error = TEXT("Could not stage replacement atlas pixels."); return false; }
            Output.ReplacementSize = FIntPoint(Preview.Material.Texture->Source.GetSizeX(), Preview.Material.Texture->Source.GetSizeY());
            Output.Material.Reset(Material); Output.Texture.Reset(Texture);
            Output.CreatedPackages = { Texture->GetOutermost()->GetName(), Material->GetOutermost()->GetName() };
            return true;
        }
    }

    const FString Folder = FPackageName::GetLongPackagePath(OriginalPackage);
    const FString Suffix = FGuid::NewGuid().ToString(EGuidFormats::Digits).Left(8);
    const FString Base = Original->GetName() + TEXT("_RXImpostor_") + Suffix;
    const FString TextureName = Base + TEXT("_T");
    const FString MaterialName = Base + TEXT("_M");
    const FString TexturePackageName = Folder / TextureName;
    const FString MaterialPackageName = Folder / MaterialName;
    if (!FPackageName::IsValidLongPackageName(TexturePackageName) || !FPackageName::IsValidLongPackageName(MaterialPackageName) ||
        FindPackage(nullptr, *TexturePackageName) || FindPackage(nullptr, *MaterialPackageName) ||
        FPackageName::DoesPackageExist(TexturePackageName) || FPackageName::DoesPackageExist(MaterialPackageName))
    { Error = TEXT("A generated asset package name is invalid or already exists."); return false; }

    UPackage* TexturePackage = CreatePackage(*TexturePackageName);
    if (!TexturePackage) { Error = TEXT("Could not create the atlas package."); return false; }
    Output.CreatedPackages.Add(TexturePackageName);
    Output.Texture.Reset(DuplicateObject<UTexture2D>(Preview.Material.Texture.Get(), TexturePackage, *TextureName));
    if (!Output.Texture.IsValid()) { Error = TEXT("Could not duplicate the atlas; an empty package may remain."); return false; }
    Output.Texture->ClearFlags(RF_Transient);
    Output.Texture->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    TexturePackage->GetMetaData().SetValue(Output.Texture.Get(), TEXT("RX_SMLODGen.Owner"), *Original->GetPathName());
    Output.Texture->PostEditChange();
    Output.Texture->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Output.Texture.Get());

    UPackage* MaterialPackage = CreatePackage(*MaterialPackageName);
    if (!MaterialPackage) { Error = TEXT("Could not create the material package; the atlas remains unsaved."); return false; }
    Output.CreatedPackages.Add(MaterialPackageName);
    Output.Material.Reset(DuplicateObject<UMaterial>(Preview.Material.Material.Get(), MaterialPackage, *MaterialName));
    if (!Output.Material.IsValid()) { Error = TEXT("Could not duplicate the material; created packages remain unsaved."); return false; }
    Output.Material->ClearFlags(RF_Transient);
    Output.Material->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    MaterialPackage->GetMetaData().SetValue(Output.Material.Get(), TEXT("RX_SMLODGen.Owner"), *Original->GetPathName());
    // The preview graph is transient; its duplicated expressions and editor data
    // must be serialized with the generated material package.
    TArray<UObject*> MaterialChildren;
    GetObjectsWithOuter(Output.Material.Get(), MaterialChildren, EGetObjectsFlags::IncludeNestedObjects);
    for (UObject* Child : MaterialChildren)
    {
        Child->ClearFlags(RF_Transient);
        Child->SetFlags(RF_Transactional);
    }
    int32 Rebound = 0;
    for (UMaterialExpression* Expression : Output.Material->GetExpressions())
    {
        if (UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
        {
            if (Sample->Texture == Preview.Material.Texture.Get()) { Sample->Texture = Output.Texture.Get(); ++Rebound; }
        }
    }
    if (Rebound != 1) { Error = TEXT("The new material did not retain a unique atlas binding; created packages remain unsaved."); return false; }
    Output.Material->PostEditChange();
    Output.Material->MarkPackageDirty();
    FAssetRegistryModule::AssetCreated(Output.Material.Get());
    return true;
}

void RXImpostorAssets::CommitReplacement(FRXImpostorAssets& Assets)
{
    if (Assets.ReplacementPixels.IsEmpty()) { return; }
    Assets.Texture->Modify();
    Assets.Texture->Source.Init(Assets.ReplacementSize.X, Assets.ReplacementSize.Y, 1, 1, TSF_BGRA8, Assets.ReplacementPixels.GetData());
    Assets.Texture->PostEditChange(); Assets.Texture->MarkPackageDirty();
    Assets.Material->Modify();
    RXImpostorMaterial::SetUpNormals(Assets.Material.Get());
    Assets.Material->PostEditChange(); Assets.Material->MarkPackageDirty();
    Assets.ReplacementPixels.Reset();
}
