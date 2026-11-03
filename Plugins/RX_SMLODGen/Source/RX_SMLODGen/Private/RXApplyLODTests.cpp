#if WITH_DEV_AUTOMATION_TESTS

#include "RXApplyLOD.h"
#include "Misc/AutomationTest.h"
#include "StaticMeshCompiler.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/Package.h"
#include "RHI.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionShadowReplace.h"
#include "Materials/MaterialExpressionConstant.h"
#include "UObject/SavePackage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/FileHelper.h"
#include "Misc/CommandLine.h"
#include "Misc/Parse.h"
#include "Misc/SecureHash.h"
#include "HAL/FileManager.h"
#include "Materials/MaterialExpressionVertexNormalWS.h"
#include "StaticMeshAttributes.h"

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXApplyLODTest, "RX.SMLODGen.ApplyWeightedLOD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXApplyLODTest::RunTest(const FString& Parameters)
{
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Cube source exists"), Cube) || Cube->IsCompiling()) { return false; }
    TStrongObjectPtr<UStaticMesh> Original(DuplicateObject<UStaticMesh>(Cube, GetTransientPackage()));
    TStrongObjectPtr<UStaticMesh> Preview(DuplicateObject<UStaticMesh>(Original.Get(), GetTransientPackage()));
    Preview->SetNumSourceModels(2);
    Preview->GetSourceModel(1).ReductionSettings.PercentTriangles = 0.5f;
    Preview->GetSourceModel(1).ReductionSettings.BaseLODModel = 0;
    const FMeshDescription* Base = Preview->GetMeshDescription(0);
    if (!TestNotNull(TEXT("LOD0 source exists"), Base)) { return false; }
    TArray<FRXSectionWeight> Sections;
    for (FPolygonGroupID Group : Base->PolygonGroups().GetElementIDs()) { Sections.Add({ Group, 1.0f, false, true }); }
    TMap<int32, TArray<FRXSectionWeight>> Weights;
    Weights.Add(1, Sections);
    FString Fingerprint, Error;
    if (!TestTrue(TEXT("Snapshot original"), RXApplyLOD::Fingerprint(Original.Get(), Fingerprint, Error))) { AddError(Error); return false; }
    bool bMutationStarted = false;
    TArray<FString> CreatedPackages;
    TestFalse(TEXT("Stale snapshot is rejected"), RXApplyLOD::Apply(Original.Get(), Preview.Get(), TEXT("stale"), Weights, false, bMutationStarted, CreatedPackages, Error));
    TestFalse(TEXT("Stale snapshot does not mutate"), bMutationStarted);
    if (!TestTrue(TEXT("Apply weighted LOD"), RXApplyLOD::Apply(Original.Get(), Preview.Get(), Fingerprint, Weights, false, bMutationStarted, CreatedPackages, Error)))
    { AddError(Error); return false; }
    TestTrue(TEXT("Apply reports mutation"), bMutationStarted);
    TArray<UStaticMesh*> ToFinish = { Original.Get() };
    FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    TestEqual(TEXT("Baked LOD is not reduced twice"), Original->GetSourceModel(1).ReductionSettings.PercentTriangles, 1.0f);
    TestTrue(TEXT("Baked geometry exists"), Original->IsMeshDescriptionValid(1));
    TStrongObjectPtr<UStaticMesh> Reopened(DuplicateObject<UStaticMesh>(Original.Get(), GetTransientPackage()));
    TMap<int32, TArray<FRXSectionWeight>> Restored;
    bool bImpostor = true;
    if (TestTrue(TEXT("Restore original controls"), RXApplyLOD::RestorePreviewSettings(Original.Get(), Reopened.Get(), Restored, bImpostor, Error)))
    {
        TestEqual(TEXT("Requested reduction restored"), Reopened->GetSourceModel(1).ReductionSettings.PercentTriangles, 0.5f);
        TestTrue(TEXT("Weights restored"), Restored.Contains(1) && Restored[1].Num() == Sections.Num());
        TestTrue(TEXT("Silhouette preservation restored after applying"), Restored.Contains(1) && Restored[1][0].bPreserveOpenBoundary);
        TestFalse(TEXT("Impostor remains disabled"), bImpostor);
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXApplyImpostorLODTest, "RX.SMLODGen.ApplyImpostorLOD",
    EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)

bool FRXApplyImpostorLODTest::RunTest(const FString& Parameters)
{
    if (GUsingNullRHI) { AddWarning(TEXT("Impostor capture requires a rendering RHI.")); return true; }
    UStaticMesh* Cube = LoadObject<UStaticMesh>(nullptr, TEXT("/Engine/BasicShapes/Cube.Cube"));
    if (!TestNotNull(TEXT("Cube source exists"), Cube) || Cube->IsCompiling()) { return false; }
    const FString Name = TEXT("RXTest_") + FGuid::NewGuid().ToString(EGuidFormats::Digits);
    UPackage* Package = CreatePackage(*(TEXT("/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/") + Name + TEXT("/") + Name));
    TStrongObjectPtr<UStaticMesh> Original(DuplicateObject<UStaticMesh>(Cube, Package, *Name));
    if (!TestNotNull(TEXT("Owned source created"), Original.Get())) { return false; }
    Original->SetFlags(RF_Public | RF_Standalone);
    TArray<UStaticMesh*> ToFinish = { Original.Get() };
    FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    Package->SetDirtyFlag(false);
    TStrongObjectPtr<UStaticMesh> Preview(DuplicateObject<UStaticMesh>(Original.Get(), GetTransientPackage()));
    ToFinish = { Preview.Get() };
    FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    FString Fingerprint, Error;
    if (!TestTrue(TEXT("Snapshot original"), RXApplyLOD::Fingerprint(Original.Get(), Fingerprint, Error))) { AddError(Error); return false; }
    const int32 OriginalLODs = Original->GetNumSourceModels();
    bool bMutationStarted = false;
    TArray<FString> CreatedPackages;
    if (!TestTrue(TEXT("Apply impostor"), RXApplyLOD::Apply(Original.Get(), Preview.Get(), Fingerprint, {}, true, bMutationStarted, CreatedPackages, Error, nullptr, 2)))
    { AddError(Error); return false; }
    ToFinish = { Original.Get() };
    FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    TestTrue(TEXT("Mutation reported"), bMutationStarted);
    TestEqual(TEXT("Two generated asset packages"), CreatedPackages.Num(), 2);
    TestEqual(TEXT("Impostor appended"), Original->GetNumSourceModels(), OriginalLODs + 1);
    TestTrue(TEXT("Impostor geometry stored"), Original->IsMeshDescriptionValid(OriginalLODs));
    TestEqual(TEXT("Applied two-card geometry"), Original->GetMeshDescription(OriginalLODs)->Triangles().Num(), 64);
    TestFalse(TEXT("Applied impostor casts no self shadow"), Original->GetSectionInfoMap().Get(OriginalLODs, 0).bCastShadow);
    const FStaticMaterial& Material = Original->GetStaticMaterials().Last();
    TestTrue(TEXT("Impostor material is persistent"), Material.MaterialInterface && Material.MaterialInterface->GetOutermost() != GetTransientPackage());
    if (UMaterial* Generated = Cast<UMaterial>(Material.MaterialInterface))
    {
        int32 Samples = 0;
        for (UMaterialExpression* Expression : Generated->GetExpressions())
        {
            TestFalse(TEXT("Material graph expression is serializable"), Expression->HasAnyFlags(RF_Transient));
            if (UMaterialExpressionTextureSample* Sample = Cast<UMaterialExpressionTextureSample>(Expression))
            {
                ++Samples;
                TestTrue(TEXT("Atlas is in a persistent package"), Sample->Texture && Sample->Texture->GetOutermost() != GetTransientPackage());
            }
        }
        TestEqual(TEXT("One atlas sample"), Samples, 1);
    }
    else { AddError(TEXT("Generated material is unavailable.")); }
    TStrongObjectPtr<UStaticMesh> Reopened(DuplicateObject<UStaticMesh>(Original.Get(), GetTransientPackage()));
    TMap<int32, TArray<FRXSectionWeight>> Restored;
    bool bImpostor = false; int32 Directions = 0;
    if (TestTrue(TEXT("Restore impostor option"), RXApplyLOD::RestorePreviewSettings(Original.Get(), Reopened.Get(), Restored, bImpostor, Error, &Directions)))
    {
        TestEqual(TEXT("Two directions restored after apply"), Directions, 2);
        TestTrue(TEXT("Impostor option restored"), bImpostor);
        TestEqual(TEXT("Preview removes generated last LOD"), Reopened->GetNumSourceModels(), OriginalLODs);
    }
    else { AddError(Error); }
    // Only owned synthetic packages are marked clean here; no user packages or
    // disk files are saved by this test.
    UMaterialInterface* FirstMaterial = Original->GetStaticMaterials().Last().MaterialInterface;
    for (const FString& PackageName : CreatedPackages) { if (UPackage* P = FindPackage(nullptr, *PackageName)) { P->SetDirtyFlag(false); } }
    Original->GetOutermost()->SetDirtyFlag(false);
    ToFinish = { Reopened.Get() }; FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    if (!TestTrue(TEXT("Snapshot for replacement"), RXApplyLOD::Fingerprint(Original.Get(), Fingerprint, Error))) { AddError(Error); return false; }
    const TArray<FString> FirstPackages = CreatedPackages;
    if (!TestTrue(TEXT("Replace existing impostor"), RXApplyLOD::Apply(Original.Get(), Reopened.Get(), Fingerprint, {}, true, bMutationStarted, CreatedPackages, Error)))
    { AddError(Error); return false; }
    ToFinish = { Original.Get() }; FStaticMeshCompilingManager::Get().FinishCompilation(ToFinish);
    TestEqual(TEXT("Replacement can switch to four cards"), Original->GetMeshDescription(OriginalLODs)->Triangles().Num(), 128);
    TestEqual(TEXT("Replacement retains last LOD index"), Original->GetNumSourceModels(), OriginalLODs + 1);
    TestEqual(TEXT("Replacement retains material asset"), Original->GetStaticMaterials().Last().MaterialInterface.Get(), FirstMaterial);
    TestTrue(TEXT("Replacement retains asset package paths"), CreatedPackages == FirstPackages);
    if (FParse::Param(FCommandLine::Get(), TEXT("RXSMLODTestPersist")))
    {
        TArray<FString> Packages = CreatedPackages; Packages.Add(Original->GetOutermost()->GetName());
        for (const FString& PackageName : Packages)
        {
            UPackage* P = FindPackage(nullptr, *PackageName);
            UObject* Asset = FindObject<UObject>(P, *FPackageName::GetShortName(PackageName));
            const FString Filename = FPackageName::LongPackageNameToFilename(PackageName, FPackageName::GetAssetPackageExtension());
            IFileManager::Get().MakeDirectory(*FPaths::GetPath(Filename), true);
            FSavePackageArgs Args; Args.TopLevelFlags = RF_Public | RF_Standalone; Args.SaveFlags = SAVE_NoError;
            if (!TestTrue(TEXT("Save owned fixture package"), P && Asset && UPackage::SavePackage(P, Asset, *Filename, Args))) { return false; }
        }
        UMaterial* M = Cast<UMaterial>(FirstMaterial); UTexture2D* Texture = nullptr;
        for (UMaterialExpression* Expression : M->GetExpressions()) { if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression)) { Texture = Cast<UTexture2D>(Sample->Texture); } }
        TArray64<uint8> Pixels; if (!Texture || !Texture->Source.GetMipData(Pixels, 0)) { AddError(TEXT("Atlas pixels unavailable")); return false; }
        const FString Manifest = Original->GetPathName() + TEXT("\n") + FString::FromInt(Original->GetNumSourceModels()) + TEXT("\n") + FirstMaterial->GetPathName() + TEXT("\n") + Texture->GetPathName() + TEXT("\n") + FMD5::HashBytes(Pixels.GetData(), Pixels.Num());
        const FString Folder = FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen"); IFileManager::Get().MakeDirectory(*Folder, true);
        TestTrue(TEXT("Write persistence evidence"), FFileHelper::SaveStringToFile(Manifest, *(Folder / TEXT("persistence_manifest.txt"))));
    }
    return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FRXReloadImpostorTest, "RX.SMLODGenPersistence.Reload", EAutomationTestFlags::EditorContext | EAutomationTestFlags::EngineFilter)
bool FRXReloadImpostorTest::RunTest(const FString& Parameters)
{
    FString Manifest; TArray<FString> Lines;
    if (!FFileHelper::LoadFileToString(Manifest, *(FPaths::ProjectSavedDir() / TEXT("RX_SMLODGen/persistence_manifest.txt")))) { AddError(TEXT("Run ApplyImpostorLOD with -RXSMLODTestPersist first.")); return false; }
    Manifest.ParseIntoArrayLines(Lines);
    if (Lines.Num() != 5 || !Lines[0].StartsWith(TEXT("/Game/_MCP_Temp/RX_SMLODGen/CubelessTests/"))) { AddError(TEXT("Invalid owned fixture manifest")); return false; }
    UStaticMesh* Mesh = LoadObject<UStaticMesh>(nullptr, *Lines[0]);
    UMaterial* Material = LoadObject<UMaterial>(nullptr, *Lines[2]); UTexture2D* Texture = LoadObject<UTexture2D>(nullptr, *Lines[3]);
    if (!TestNotNull(TEXT("Saved mesh reloaded"), Mesh) || !TestNotNull(TEXT("Saved material reloaded"), Material) || !TestNotNull(TEXT("Saved atlas reloaded"), Texture)) { return false; }
    TArray<UStaticMesh*> Pending = { Mesh }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending);
    TestEqual(TEXT("LOD count survives restart"), Mesh->GetNumSourceModels(), FCString::Atoi(*Lines[1]));
    TestEqual(TEXT("Material reference survives restart"), Mesh->GetStaticMaterials().Last().MaterialInterface.Get(), static_cast<UMaterialInterface*>(Material));
    TestTrue(TEXT("Last LOD source survives restart"), Mesh->IsMeshDescriptionValid(Mesh->GetNumSourceModels() - 1));
    TestFalse(TEXT("No-cast-shadow section flag survives restart"), Mesh->GetSectionInfoMap().Get(Mesh->GetNumSourceModels() - 1, 0).bCastShadow);
    const auto* ShadowSwitch = Cast<UMaterialExpressionShadowReplace>(Material->GetEditorOnlyData()->OpacityMask.Expression);
    const auto* ShadowZero = ShadowSwitch ? Cast<UMaterialExpressionConstant>(ShadowSwitch->Shadow.Expression) : nullptr;
    TestTrue(TEXT("Shadow-discard material graph survives restart"), ShadowZero && ShadowZero->R == 0.0f);
    TestTrue(TEXT("Generated material stays two sided and masked"), Material->TwoSided && Material->BlendMode == BLEND_Masked);
    TestTrue(TEXT("World-space up normal graph survives restart"), !Material->bTangentSpaceNormal && Material->GetEditorOnlyData()->Normal.Expression && Material->GetEditorOnlyData()->Normal.Expression->IsA<UMaterialExpressionVertexNormalWS>());
    const FMeshDescription* Geometry = Mesh->GetMeshDescription(Mesh->GetNumSourceModels() - 1);
    if (Geometry)
    {
        FStaticMeshConstAttributes Attributes(*Geometry); const auto Normals = Attributes.GetVertexInstanceNormals();
        for (FVertexInstanceID Instance : Geometry->VertexInstances().GetElementIDs())
            if (!Normals[Instance].Equals(FVector3f(0, 0, 1), KINDA_SMALL_NUMBER)) { AddError(TEXT("Saved impostor normals must remain local up")); break; }
    }
    int32 Samples = 0;
    for (UMaterialExpression* Expression : Material->GetExpressions()) { if (auto* Sample = Cast<UMaterialExpressionTextureSample>(Expression)) { ++Samples; TestEqual(TEXT("Atlas graph binding survives restart"), Sample->Texture.Get(), static_cast<UTexture*>(Texture)); } }
    TestEqual(TEXT("Serialized material graph retained"), Samples, 1);
    TArray64<uint8> Pixels;
    if (!TestTrue(TEXT("Atlas source reloaded"), Texture->Source.GetMipData(Pixels, 0))) { return false; }
    TestEqual(TEXT("Atlas pixels survive restart"), FMD5::HashBytes(Pixels.GetData(), Pixels.Num()), Lines[4]);
    TStrongObjectPtr<UStaticMesh> Preview(DuplicateObject<UStaticMesh>(Mesh, GetTransientPackage()));
    Pending = { Preview.Get() }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending);
    TMap<int32, TArray<FRXSectionWeight>> Weights; bool Impostor = false; FString Error; int32 Directions = 0;
    if (!TestTrue(TEXT("Metadata restores after disk reload"), RXApplyLOD::RestorePreviewSettings(Mesh, Preview.Get(), Weights, Impostor, Error, &Directions))) { AddError(Error); return false; }
    TestTrue(TEXT("Impostor option survives restart"), Impostor);
    TestEqual(TEXT("Four-direction metadata survives restart"), Directions, 4);
    TestEqual(TEXT("Only generated last LOD removed from editable controls"), Preview->GetNumSourceModels(), Mesh->GetNumSourceModels() - 1);
    return true;
}
#endif
