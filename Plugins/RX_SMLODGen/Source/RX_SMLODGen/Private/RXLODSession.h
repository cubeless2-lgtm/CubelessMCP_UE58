#pragma once
#include "CoreMinimal.h"
#include "Engine/StaticMesh.h"
#include "UObject/StrongObjectPtr.h"
#include "RXSectionReduction.h"
#include "RXImpostorPreview.h"

class FRXLODSession : public TSharedFromThis<FRXLODSession>
{
public:
    TStrongObjectPtr<UStaticMesh> Original, Preview, Display;
    FString Baseline, Status;
    FString BuiltSettings;
    bool IsPreviewCurrent() const { return BuiltSettings == ExportJson(); }
    TMap<int32, TArray<FRXSectionWeight>> Weights;
    FRXImpostorPreview Impostor;
    bool bImpostor = false;
    int32 ImpostorDirections = 4;
    int32 ImpostorLOD = INDEX_NONE;
    int32 GetVisibleImpostorLOD() const { return bImpostor && ImpostorLOD == Preview->GetNumSourceModels() ? ImpostorLOD : INDEX_NONE; }
    bool Initialize(UStaticMesh* Mesh, FString& Error);
    bool Rebuild(FString& Error);
    bool Resize(int32 Count, FString& Error);
    bool Remove(int32 LOD, FString& Error);
    bool Apply(FString& Error);
    TArray<FRXSectionWeight>& GetWeights(int32 LOD);
    FPerPlatformFloat GetScreenSize(int32 LOD) const;
    void SetAutoScreenSize(bool Auto);
    bool SetScreenSize(int32 LOD, const FPerPlatformFloat& Value, FString& Error);
    FString ExportJson() const;
    bool ImportJson(const FString& Json, FString& Error);
};

namespace RXLOD
{
    bool Open(UStaticMesh* Mesh, FString& Error);
    bool LoadAsset(UStaticMesh* Mesh, FString& Error);
    inline constexpr const TCHAR* DefaultMeshPath = TEXT("/RX_SMLODGen/Meshes/SM_DefaultBox.SM_DefaultBox");
    FString Execute(const FString& Request);
}
