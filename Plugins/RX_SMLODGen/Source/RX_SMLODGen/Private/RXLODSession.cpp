#include "RXLODSession.h"
#include "RXApplyLOD.h"
#include "RXWeightedPreview.h"
#include "StaticMeshCompiler.h"
#include "StaticMeshAttributes.h"
#include "JsonObjectConverter.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/UnrealType.h"
#include "Materials/MaterialInterface.h"
#include "StaticMeshResources.h"

namespace
{
    void Finish(UStaticMesh* Mesh) { TArray<UStaticMesh*> Pending = { Mesh }; FStaticMeshCompilingManager::Get().FinishCompilation(Pending); }
    const TArray<FName> GeneralProperties = { TEXT("MinLOD"), TEXT("MinQualityLevelLOD"), TEXT("NoRefStreamingLODBias"), TEXT("NumStreamedLODs"), TEXT("LODStreaming") };
    template<class T> TSharedPtr<FJsonObject> StructJson(const T& Value)
    { return FJsonObjectConverter::UStructToJsonObject(Value); }
    FString Stringify(const TSharedRef<FJsonObject>& Object)
    { FString Text; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Text)); return Text; }
    bool FiniteProperty(FProperty* P, const void* Data)
    {
        if (const auto* Number = CastField<FFloatProperty>(P)) { return FMath::IsFinite(Number->GetPropertyValue(Data)); }
        if (const auto* Number = CastField<FDoubleProperty>(P)) { return FMath::IsFinite(Number->GetPropertyValue(Data)); }
        if (const auto* Struct = CastField<FStructProperty>(P))
            for (TFieldIterator<FProperty> It(Struct->Struct); It; ++It) { if (!FiniteProperty(*It, It->ContainerPtrToValuePtr<void>(Data))) { return false; } }
        if (const auto* Map = CastField<FMapProperty>(P))
        {
            FScriptMapHelper Helper(Map, Data); if (Helper.Num() > 256) { return false; }
            for (int32 Index = 0; Index < Helper.GetMaxIndex(); ++Index) { if (Helper.IsValidIndex(Index) && !FiniteProperty(Map->ValueProp, Helper.GetValuePtr(Index))) { return false; } }
        }
        return true;
    }
    bool FiniteStruct(const UStruct* Struct, const void* Data)
    { for (TFieldIterator<FProperty> It(Struct); It; ++It) { if (!FiniteProperty(*It, It->ContainerPtrToValuePtr<void>(Data))) { return false; } } return true; }
}

bool FRXLODSession::Initialize(UStaticMesh* Mesh, FString& Error)
{
    if (!Mesh || !IsInGameThread()) { Error = TEXT("Static Mesh와 에디터 게임 스레드가 필요합니다."); return false; }
    Finish(Mesh);
    if (Mesh->GetNumSourceModels() < 1 || !Mesh->IsMeshDescriptionValid(0)) { Error = TEXT("편집 가능한 LOD 0 소스 지오메트리가 필요합니다."); return false; }
    Original.Reset(Mesh);
    if (!RXApplyLOD::Fingerprint(Mesh, Baseline, Error)) { return false; }
    Preview.Reset(DuplicateObject<UStaticMesh>(Mesh, GetTransientPackage()));
    Preview->SetFlags(RF_Transient); Preview->ClearFlags(RF_Public | RF_Standalone);
    Finish(Preview.Get());
    if (!RXApplyLOD::RestorePreviewSettings(Mesh, Preview.Get(), Weights, bImpostor, Error)) { return false; }
    return Rebuild(Error);
}

TArray<FRXSectionWeight>& FRXLODSession::GetWeights(int32 LOD)
{
    TArray<FRXSectionWeight>& Rows = Weights.FindOrAdd(LOD);
    if (Rows.IsEmpty())
    {
        if (const FMeshDescription* Base = Preview->GetMeshDescription(0))
            for (FPolygonGroupID Group : Base->PolygonGroups().GetElementIDs()) { Rows.Add({ Group, 0.0f, false }); }
    }
    return Rows;
}

bool FRXLODSession::Resize(int32 Count, FString& Error)
{
    if (Count < 1 || Count > MAX_STATIC_MESH_LODS - (bImpostor ? 1 : 0)) { Error = TEXT("LOD 개수가 허용 범위를 벗어났습니다."); return false; }
    const int32 Previous = Preview->GetNumSourceModels();
    Preview->SetNumSourceModels(Count);
    for (int32 LOD = Previous; LOD < Count; ++LOD)
    {
        Preview->GetSourceModel(LOD).ReductionSettings.PercentTriangles = FMath::Pow(0.5f, float(LOD));
        Preview->GetSourceModel(LOD).ReductionSettings.BaseLODModel = 0;
        Preview->GetSourceModel(LOD).ScreenSize.Default = FMath::Pow(0.5f, float(LOD));
    }
    for (auto It = Weights.CreateIterator(); It; ++It) { if (It.Key() >= Count) { It.RemoveCurrent(); } }
    for (int32 LOD = 1; LOD < Count; ++LOD) { GetWeights(LOD); }
    // Remove stale section overrides when truncating the editable LOD list.
    for (int32 LOD = Count; LOD < Previous; ++LOD)
        for (int32 S = Preview->GetSectionInfoMap().GetSectionNumber(LOD) - 1; S >= 0; --S)
        { Preview->GetSectionInfoMap().Remove(LOD, S); Preview->GetOriginalSectionInfoMap().Remove(LOD, S); }
    return true;
}

FPerPlatformFloat FRXLODSession::GetScreenSize(int32 LOD) const
{
    const auto* Data = Display.IsValid() ? Display->GetRenderData() : nullptr;
    if (Preview->GetAutoComputeLODScreenSize() && Data && Data->LODResources.IsValidIndex(LOD)) { return Data->ScreenSize[LOD]; }
    if (Preview->IsSourceModelValid(LOD)) { return Preview->GetSourceModel(LOD).ScreenSize; }
    return Display.IsValid() && Display->IsSourceModelValid(LOD) ? Display->GetSourceModel(LOD).ScreenSize : FPerPlatformFloat(0.0f);
}

void FRXLODSession::SetAutoScreenSize(bool Auto)
{
    if (!Auto && Preview->GetAutoComputeLODScreenSize())
    {
        for (int32 LOD = 0; LOD < Preview->GetNumSourceModels(); ++LOD)
        {
            const auto Computed = GetScreenSize(LOD);
            Preview->GetSourceModel(LOD).ScreenSize = Computed;
        }
        Preview->GetSourceModel(0).ScreenSize.Default = 1.0f;
    }
    Preview->SetAutoComputeLODScreenSize(Auto);
}

bool FRXLODSession::SetScreenSize(int32 LOD, const FPerPlatformFloat& Value, FString& Error)
{
    if (!Preview->IsSourceModelValid(LOD) || !FMath::IsFinite(Value.Default) || Value.Default < 0)
    { Error = TEXT("편집할 LOD와 0 이상의 화면 크기가 필요합니다."); return false; }
    for (const auto& Pair : Value.PerPlatform)
        if (!FMath::IsFinite(Pair.Value) || Pair.Value < 0) { Error = TEXT("플랫폼별 화면 크기는 0 이상의 유한한 값이어야 합니다."); return false; }
    const bool WasAuto = Preview->GetAutoComputeLODScreenSize();
    SetAutoScreenSize(false);
    Preview->GetSourceModel(LOD).ScreenSize = Value;
    Status = WasAuto ? TEXT("화면 크기를 직접 편집하여 자동 계산을 껐습니다. 미리보기 변경 내용 적용으로 확인하세요.") : TEXT("화면 크기를 변경했습니다. 미리보기 변경 내용 적용으로 확인하세요.");
    Error.Reset(); return true;
}

bool FRXLODSession::Remove(int32 LOD, FString& Error)
{
    if (LOD < 1 || LOD >= Preview->GetNumSourceModels()) { Error = TEXT("LOD 0은 제거할 수 없습니다."); return false; }
    Preview->RemoveSourceModel(LOD);
    TMap<int32, TArray<FRXSectionWeight>> Shifted;
    for (auto& Pair : Weights) { if (Pair.Key != LOD) { Shifted.Add(Pair.Key > LOD ? Pair.Key - 1 : Pair.Key, Pair.Value); } }
    Weights = MoveTemp(Shifted);
    return true;
}

bool FRXLODSession::Rebuild(FString& Error)
{
    Finish(Preview.Get());
    for (int32 LOD = 1; LOD < Preview->GetNumSourceModels(); ++LOD) { GetWeights(LOD); }
    TStrongObjectPtr<UStaticMesh> Work(DuplicateObject<UStaticMesh>(Preview.Get(), GetTransientPackage()));
    Work->SetFlags(RF_Transient); Work->ClearFlags(RF_Public | RF_Standalone);
    Work->PostEditChange(); Finish(Work.Get());
    for (auto& Pair : Weights)
    {
        const bool bWeighted = Pair.Value.ContainsByPredicate([](const FRXSectionWeight& W) { return W.bExcluded || W.bPreserveSilhouette || W.Weight != 0.0f; });
        if (!bWeighted) { continue; }
        TStrongObjectPtr<UStaticMesh> Reduced;
        TArray<FRXSectionReductionResult> Results;
        if (!RXWeightedPreview::Build(Work.Get(), Pair.Key, Pair.Value, Reduced, Results, Error)) { return false; }
        Work = MoveTemp(Reduced);
    }
    FRXImpostorPreview NewImpostor;
    int32 NewImpostorLOD = INDEX_NONE;
    if (bImpostor)
    {
        if (Work->IsNaniteEnabled() || Work->GetNumSourceModels() >= MAX_STATIC_MESH_LODS)
        { Error = TEXT("임포스터에는 비 Nanite 메시와 마지막 LOD 슬롯이 필요합니다."); return false; }
        if (!Preview->GetAutoComputeLODScreenSize() && Work->GetSourceModel(Work->GetNumSourceModels() - 1).ScreenSize.Default <= 0)
        { Error = TEXT("임포스터를 추가하려면 마지막 일반 LOD의 화면 크기가 0보다 커야 합니다."); return false; }
        if (!RXImpostorPreview::Build(Work.Get(), FIntPoint(512, 512), 4, 4, NewImpostor, Error)) { return false; }
        NewImpostorLOD = Work->GetNumSourceModels();
        Work->SetNumSourceModels(NewImpostorLOD + 1);
        FMeshDescription Geometry(*NewImpostor.Geometry);
        FStaticMeshAttributes Attributes(Geometry);
        const FName Slot(TEXT("RXImpostor"));
        for (FPolygonGroupID Group : Geometry.PolygonGroups().GetElementIDs()) { Attributes.GetPolygonGroupMaterialSlotNames()[Group] = Slot; }
        const int32 Material = Work->GetStaticMaterials().Add(FStaticMaterial(NewImpostor.Material.Material.Get(), Slot, Slot));
        Work->GetSectionInfoMap().Set(NewImpostorLOD, 0, FMeshSectionInfo(Material));
        Work->GetOriginalSectionInfoMap().Set(NewImpostorLOD, 0, FMeshSectionInfo(Material));
        auto& Model = Work->GetSourceModel(NewImpostorLOD);
        Model.ReductionSettings = FMeshReductionSettings(); Model.ReductionSettings.BaseLODModel = NewImpostorLOD;
        Model.BuildSettings.bRecomputeNormals = false; Model.BuildSettings.bRecomputeTangents = false; Model.BuildSettings.bGenerateLightmapUVs = false;
        Model.ScreenSize.Default = Work->GetSourceModel(NewImpostorLOD - 1).ScreenSize.Default * 0.5f;
        Work->CreateMeshDescription(NewImpostorLOD, MoveTemp(Geometry));
        UStaticMesh::FCommitMeshDescriptionParams Params; Params.bMarkPackageDirty = false;
        Work->CommitMeshDescription(NewImpostorLOD, Params);
        Work->PostEditChange(); Finish(Work.Get());
    }
    Display = MoveTemp(Work); Impostor = MoveTemp(NewImpostor); ImpostorLOD = NewImpostorLOD;
    BuiltSettings = ExportJson();
    Status = TEXT("미리보기 갱신 완료. 원본 에셋은 아직 적용되지 않았습니다.");
    Error.Reset(); return true;
}

bool FRXLODSession::Apply(FString& Error)
{
    if (!IsPreviewCurrent()) { Error = TEXT("설정 변경 후 미리보기를 먼저 갱신하세요 (rebuild)."); return false; }
    TMap<int32, TArray<FRXSectionWeight>> Active;
    for (const auto& Pair : Weights)
        if (Pair.Value.ContainsByPredicate([](const FRXSectionWeight& W) { return W.bExcluded || W.bPreserveSilhouette || W.Weight != 0.0f; })) { Active.Add(Pair.Key, Pair.Value); }
    bool bStarted = false; TArray<FString> Packages;
    const bool bOK = RXApplyLOD::Apply(Original.Get(), Preview.Get(), Baseline, Active, bImpostor, bStarted, Packages, Error, bImpostor ? &Impostor : nullptr);
    if (!bOK && bStarted) { Error = TEXT("원본 변경이 시작된 뒤 실패했습니다. 원본을 확인하세요.\n") + Error; }
    for (const FString& Package : Packages) { Error += TEXT("\n미저장 에셋: ") + Package; }
    if (bOK) { Status = TEXT("원본에 적용했습니다. 원본과 생성된 에셋을 함께 저장하세요. 재적용하려면 저장 후 RX 창을 다시 여세요.") + Error; }
    return bOK;
}

FString FRXLODSession::ExportJson() const
{
    auto Root = MakeShared<FJsonObject>();
    Root->SetStringField(TEXT("schema"), TEXT("RX_SMLODGen")); Root->SetNumberField(TEXT("version"), 3);
    Root->SetStringField(TEXT("weight_semantics"), TEXT("preservation"));
    Root->SetStringField(TEXT("asset"), Original->GetPathName());
    const auto* Bulk = Preview->GetSourceModel(0).GetMeshDescriptionBulkData();
    Root->SetStringField(TEXT("source_id"), Bulk ? Bulk->GetUnversionedIdString() : FString());
    Root->SetBoolField(TEXT("impostor"), bImpostor); Root->SetBoolField(TEXT("auto_screen_size"), Preview->GetAutoComputeLODScreenSize());
    Root->SetStringField(TEXT("lod_group"), Preview->GetLODGroup().ToString());
    auto General = MakeShared<FJsonObject>();
    for (FName Name : GeneralProperties)
        if (FProperty* P = FindFProperty<FProperty>(UStaticMesh::StaticClass(), Name))
        { General->SetField(Name.ToString(), FJsonObjectConverter::UPropertyToJsonValue(P, P->ContainerPtrToValuePtr<void>(Preview.Get()), 0, 0)); }
    Root->SetObjectField(TEXT("general"), General);
    TArray<TSharedPtr<FJsonValue>> Materials;
    for (const auto& Slot : Preview->GetStaticMaterials())
    { auto Item = MakeShared<FJsonObject>(); Item->SetStringField(TEXT("slot"), Slot.MaterialSlotName.ToString()); Item->SetStringField(TEXT("imported_slot"), Slot.ImportedMaterialSlotName.ToString()); Item->SetStringField(TEXT("material"), Slot.MaterialInterface ? Slot.MaterialInterface->GetPathName() : FString()); Materials.Add(MakeShared<FJsonValueObject>(Item)); }
    Root->SetArrayField(TEXT("materials"), Materials);
    TArray<TSharedPtr<FJsonValue>> LODs;
    for (int32 LOD = 0; LOD < Preview->GetNumSourceModels(); ++LOD)
    {
        const auto& Model = Preview->GetSourceModel(LOD); auto Item = MakeShared<FJsonObject>();
        Item->SetNumberField(TEXT("index"), LOD);
        const auto* Geometry = Model.GetMeshDescriptionBulkData();
        Item->SetStringField(TEXT("geometry_id"), Preview->IsMeshDescriptionValid(LOD) && Geometry ? Geometry->GetUnversionedIdString() : FString());
        Item->SetStringField(TEXT("import_filename"), Model.SourceImportFilename);
        Item->SetObjectField(TEXT("reduction"), StructJson(Model.ReductionSettings));
        Item->SetObjectField(TEXT("build"), StructJson(Model.BuildSettings));
        Item->SetObjectField(TEXT("screen_size"), StructJson(Model.ScreenSize));
        TArray<TSharedPtr<FJsonValue>> Rows;
        if (const auto* Found = Weights.Find(LOD))
            for (const auto& W : *Found)
            { auto Row = MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("group"), W.Group.GetValue()); Row->SetNumberField(TEXT("weight"), W.Weight); Row->SetBoolField(TEXT("excluded"), W.bExcluded); Row->SetBoolField(TEXT("preserve_silhouette"), W.bPreserveSilhouette); Rows.Add(MakeShared<FJsonValueObject>(Row)); }
        Item->SetArrayField(TEXT("sections"), Rows); LODs.Add(MakeShared<FJsonValueObject>(Item));
        TArray<TSharedPtr<FJsonValue>> Overrides;
        for (int32 Section = 0; Section < Preview->GetSectionInfoMap().GetSectionNumber(LOD); ++Section)
        {
            const auto Info = Preview->GetSectionInfoMap().Get(LOD, Section); auto Override = MakeShared<FJsonObject>();
            Override->SetNumberField(TEXT("section"), Section); Override->SetNumberField(TEXT("material"), Info.MaterialIndex);
            Override->SetBoolField(TEXT("shadow"), Info.bCastShadow); Override->SetBoolField(TEXT("collision"), Info.bEnableCollision);
            Override->SetBoolField(TEXT("ray_tracing"), Info.bVisibleInRayTracing); Override->SetBoolField(TEXT("distance_field_lighting"), Info.bAffectDistanceFieldLighting); Override->SetBoolField(TEXT("force_opaque"), Info.bForceOpaque);
            Overrides.Add(MakeShared<FJsonValueObject>(Override));
        }
        Item->SetArrayField(TEXT("section_overrides"), Overrides);
    }
    Root->SetArrayField(TEXT("lods"), LODs);
    return Stringify(Root);
}

bool FRXLODSession::ImportJson(const FString& Json, FString& Error)
{
    TSharedPtr<FJsonObject> Root;
    if (Json.Len() > 4 * 1024 * 1024 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Root) || !Root)
    { Error = TEXT("유효한 JSON이 아닙니다 (최대 4 MB)."); return false; }
    FString Schema, Asset, Source, Group; int32 Version = 0; bool Include = false, Auto = false;
    const TArray<TSharedPtr<FJsonValue>>* LODs = nullptr;
    const FMeshDescriptionBulkData* Bulk = Preview->GetSourceModel(0).GetMeshDescriptionBulkData();
    if (!Root->TryGetStringField(TEXT("schema"), Schema) || Schema != TEXT("RX_SMLODGen") || !Root->TryGetNumberField(TEXT("version"), Version) || (Version < 1 || Version > 3) ||
        !Root->TryGetStringField(TEXT("asset"), Asset) || Asset != Original->GetPathName() || !Root->TryGetStringField(TEXT("source_id"), Source) || !Bulk || Source != Bulk->GetUnversionedIdString() ||
        !Root->TryGetBoolField(TEXT("impostor"), Include) || !Root->TryGetBoolField(TEXT("auto_screen_size"), Auto) || !Root->TryGetStringField(TEXT("lod_group"), Group) ||
        !Root->TryGetArrayField(TEXT("lods"), LODs) || LODs->Num() < 1 || LODs->Num() > MAX_STATIC_MESH_LODS - (Include ? 1 : 0))
    { Error = TEXT("스키마 버전, 에셋/소스 식별자 또는 LOD 범위가 맞지 않습니다."); return false; }
    FString Semantics;
    if (Version >= 2 && (!Root->TryGetStringField(TEXT("weight_semantics"), Semantics) || Semantics != TEXT("preservation")))
    { Error = TEXT("버전 2 이상 JSON은 보존 가중치(weight_semantics: preservation)를 사용해야 합니다."); return false; }
    TArray<FName> Groups; UStaticMesh::GetLODGroups(Groups);
    if (FName(*Group) != NAME_None && !Groups.Contains(FName(*Group))) { Error = TEXT("알 수 없는 LOD 그룹입니다."); return false; }
    TStrongObjectPtr<UStaticMesh> Candidate(DuplicateObject<UStaticMesh>(Preview.Get(), GetTransientPackage())); Finish(Candidate.Get());
    Candidate->SetNumSourceModels(LODs->Num()); Candidate->SetLODGroup(FName(*Group), false, false); Candidate->SetAutoComputeLODScreenSize(Auto);
    const TArray<TSharedPtr<FJsonValue>>* Materials = nullptr;
    if (!Root->TryGetArrayField(TEXT("materials"), Materials) || Materials->Num() != Candidate->GetStaticMaterials().Num()) { Error = TEXT("머티리얼 슬롯 개수가 맞지 않습니다."); return false; }
    for (int32 Index = 0; Index < Materials->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Item = nullptr; FString Slot, ImportedSlot, Path;
        if (!(*Materials)[Index]->TryGetObject(Item) || !(*Item)->TryGetStringField(TEXT("slot"), Slot) || !(*Item)->TryGetStringField(TEXT("imported_slot"), ImportedSlot) || !(*Item)->TryGetStringField(TEXT("material"), Path) ||
            FName(*Slot) != Candidate->GetStaticMaterials()[Index].MaterialSlotName || FName(*ImportedSlot) != Candidate->GetStaticMaterials()[Index].ImportedMaterialSlotName)
        { Error = TEXT("머티리얼 슬롯 식별자가 맞지 않습니다."); return false; }
        UMaterialInterface* Material = Path.IsEmpty() ? nullptr : LoadObject<UMaterialInterface>(nullptr, *Path);
        if (!Path.IsEmpty() && (!Material || Material->HasAnyFlags(RF_Transient))) { Error = TEXT("저장된 머티리얼을 찾을 수 없습니다."); return false; }
        Candidate->GetStaticMaterials()[Index].MaterialInterface = Material;
    }
    TMap<int32, TArray<FRXSectionWeight>> NewWeights;
    for (int32 Index = 0; Index < LODs->Num(); ++Index)
    {
        const TSharedPtr<FJsonObject>* Item = nullptr;
        if (!(*LODs)[Index]->TryGetObject(Item) || !Item || !*Item) { Error = TEXT("LOD 항목이 잘못되었습니다."); return false; }
        int32 Number = -1; const TSharedPtr<FJsonObject>* Reduction = nullptr; const TSharedPtr<FJsonObject>* Build = nullptr; const TSharedPtr<FJsonObject>* Screen = nullptr;
        const TArray<TSharedPtr<FJsonValue>>* Sections = nullptr;
        auto& Model = Candidate->GetSourceModel(Index);
        FString GeometryID;
        const auto* GeometryBulk = Model.GetMeshDescriptionBulkData();
        const FString CurrentGeometry = Candidate->IsMeshDescriptionValid(Index) && GeometryBulk ? GeometryBulk->GetUnversionedIdString() : FString();
        if (!(*Item)->TryGetStringField(TEXT("geometry_id"), GeometryID) || GeometryID != CurrentGeometry)
        { Error = TEXT("가져온 LOD 지오메트리가 다릅니다. JSON은 지오메트리를 포함하지 않으므로 같은 LOD 소스를 먼저 임포트하세요."); return false; }
        if (!(*Item)->TryGetNumberField(TEXT("index"), Number) || Number != Index || !(*Item)->TryGetObjectField(TEXT("reduction"), Reduction) ||
            !(*Item)->TryGetObjectField(TEXT("build"), Build) || !(*Item)->TryGetObjectField(TEXT("screen_size"), Screen) || !(*Item)->TryGetArrayField(TEXT("sections"), Sections) ||
            !FJsonObjectConverter::JsonObjectToUStruct((*Reduction).ToSharedRef(), &Model.ReductionSettings, 0, 0) ||
            !FJsonObjectConverter::JsonObjectToUStruct((*Build).ToSharedRef(), &Model.BuildSettings, 0, 0) ||
            !FJsonObjectConverter::JsonObjectToUStruct((*Screen).ToSharedRef(), &Model.ScreenSize, 0, 0) ||
            !FMath::IsFinite(Model.ReductionSettings.PercentTriangles) || Model.ReductionSettings.PercentTriangles < 0 || Model.ReductionSettings.PercentTriangles > 1 ||
            !FMath::IsFinite(Model.ReductionSettings.PercentVertices) || Model.ReductionSettings.PercentVertices < 0 || Model.ReductionSettings.PercentVertices > 1 ||
            Model.ReductionSettings.BaseLODModel < 0 || Model.ReductionSettings.BaseLODModel > Index || !FMath::IsFinite(Model.ScreenSize.Default) || Model.ScreenSize.Default < 0)
        { Error = TEXT("LOD 설정 값이 잘못되었습니다."); return false; }
        if (!FiniteStruct(FMeshReductionSettings::StaticStruct(), &Model.ReductionSettings) || !FiniteStruct(FMeshBuildSettings::StaticStruct(), &Model.BuildSettings) || !FiniteStruct(FPerPlatformFloat::StaticStruct(), &Model.ScreenSize) ||
            Model.ReductionSettings.WeldingThreshold < 0 || Model.ReductionSettings.MaxDeviation < 0 || Model.ReductionSettings.PixelError < 0 || Model.ReductionSettings.HardAngleThreshold < 0 || Model.ReductionSettings.HardAngleThreshold > 180 ||
            Model.ReductionSettings.MaxNumOfTriangles < 2 || Model.ReductionSettings.MaxNumOfVerts < 4 || int32(Model.ReductionSettings.TerminationCriterion) > 2)
        { Error = TEXT("유한한 범위 안의 감소·빌드 설정이 필요합니다."); return false; }
        TArray<FRXSectionWeight> Rows; TSet<int32> Seen;
        for (const auto& Value : *Sections)
        {
            const TSharedPtr<FJsonObject>* Row = nullptr; int32 GroupIndex = -1; double Weight = 0; bool Excluded = false, PreserveSilhouette = false;
            if (!Value->TryGetObject(Row) || !Row || !*Row || !(*Row)->TryGetNumberField(TEXT("group"), GroupIndex) || Seen.Contains(GroupIndex) ||
                !Candidate->GetMeshDescription(0)->IsPolygonGroupValid(FPolygonGroupID(GroupIndex)) || !(*Row)->TryGetNumberField(TEXT("weight"), Weight) ||
                !FMath::IsFinite(Weight) || Weight < 0 || Weight > 1 || !(*Row)->TryGetBoolField(TEXT("excluded"), Excluded) || Index == 0)
            { Error = TEXT("섹션 식별자 또는 가중치가 잘못되었습니다."); return false; }
            if ((Version >= 3 || (*Row)->HasField(TEXT("preserve_silhouette"))) &&
                (!(*Row)->HasTypedField<EJson::Boolean>(TEXT("preserve_silhouette")) || !(*Row)->TryGetBoolField(TEXT("preserve_silhouette"), PreserveSilhouette)))
            { Error = TEXT("외곽 실루엣 보존 값은 참/거짓이어야 합니다."); return false; }
            Seen.Add(GroupIndex); Rows.Add({ FPolygonGroupID(GroupIndex), Version == 1 ? 1.0f - float(Weight) : float(Weight), Excluded, PreserveSilhouette });
        }
        if (!Rows.IsEmpty()) { NewWeights.Add(Index, Rows); }
        const TArray<TSharedPtr<FJsonValue>>* Overrides = nullptr;
        if (!(*Item)->TryGetArrayField(TEXT("section_overrides"), Overrides) || Overrides->Num() > 65536) { Error = TEXT("섹션 설정이 잘못되었습니다."); return false; }
        TSet<int32> OverrideIDs;
        for (const auto& Value : *Overrides)
        {
            const TSharedPtr<FJsonObject>* O = nullptr; int32 Section = -1, Material = -1; bool Shadow = true, Collision = true;
            if (!Value->TryGetObject(O) || !(*O)->TryGetNumberField(TEXT("section"), Section) || Section < 0 || Section > 65535 || OverrideIDs.Contains(Section) ||
                !(*O)->TryGetNumberField(TEXT("material"), Material) || !Candidate->GetStaticMaterials().IsValidIndex(Material) || !(*O)->TryGetBoolField(TEXT("shadow"), Shadow) || !(*O)->TryGetBoolField(TEXT("collision"), Collision))
            { Error = TEXT("섹션 머티리얼 또는 플래그가 잘못되었습니다."); return false; }
            OverrideIDs.Add(Section); auto Info = Candidate->GetSectionInfoMap().Get(Index, Section); Info.MaterialIndex = Material; Info.bCastShadow = Shadow; Info.bEnableCollision = Collision;
            if (!(*O)->TryGetBoolField(TEXT("ray_tracing"), Info.bVisibleInRayTracing) || !(*O)->TryGetBoolField(TEXT("distance_field_lighting"), Info.bAffectDistanceFieldLighting) || !(*O)->TryGetBoolField(TEXT("force_opaque"), Info.bForceOpaque))
            { Error = TEXT("섹션 렌더링 플래그가 잘못되었습니다."); return false; }
            Candidate->GetSectionInfoMap().Set(Index, Section, Info); Candidate->GetOriginalSectionInfoMap().Set(Index, Section, Info);
        }
    }
    const TSharedPtr<FJsonObject>* General = nullptr;
    if (!Root->TryGetObjectField(TEXT("general"), General)) { Error = TEXT("LOD 공통 설정이 없습니다."); return false; }
    for (const auto& Field : (*General)->Values)
    {
        if (!GeneralProperties.Contains(FName(*Field.Key))) { Error = TEXT("허용되지 않은 공통 설정입니다."); return false; }
        FProperty* P = FindFProperty<FProperty>(UStaticMesh::StaticClass(), FName(*Field.Key));
        if (!P || !FJsonObjectConverter::JsonValueToUProperty(Field.Value, P, P->ContainerPtrToValuePtr<void>(Candidate.Get()), 0, 0))
        { Error = TEXT("공통 설정 값을 읽지 못했습니다."); return false; }
    }
    Preview = MoveTemp(Candidate); Weights = MoveTemp(NewWeights); bImpostor = Include;
    Status = TEXT("JSON 설정을 가져왔습니다. 미리보기 갱신을 눌러 확인하세요.");
    Error.Reset(); return true;
}
