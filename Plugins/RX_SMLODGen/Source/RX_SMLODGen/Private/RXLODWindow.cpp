#include "RXLODWindow.h"
#include "RXSMLODGenLibrary.h"
#include "SEditorViewport.h"
#include "PreviewScene.h"
#include "EditorViewportClient.h"
#include "Components/StaticMeshComponent.h"
#include "StaticMeshResources.h"
#include "StaticMeshAttributes.h"
#include "StaticMeshCompiler.h"
#include "PropertyEditorModule.h"
#include "IStructureDetailsView.h"
#include "IDetailsView.h"
#include "ISinglePropertyView.h"
#include "PropertyCustomizationHelpers.h"
#include "Widgets/Layout/SSplitter.h"
#include "Widgets/Layout/SScrollBox.h"
#include "Widgets/Layout/SExpandableArea.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Input/SCheckBox.h"
#include "Widgets/Input/SNumericEntryBox.h"
#include "Widgets/Input/SComboBox.h"
#include "Widgets/Text/STextBlock.h"
#include "DesktopPlatformModule.h"
#include "Framework/Application/SlateApplication.h"
#include "Misc/FileHelper.h"
#include "Misc/Paths.h"
#include "UObject/UnrealType.h"
#include "FbxMeshUtils.h"
#include "Engine/TextureCube.h"
#include "RXLODTooltips.h"
#include "IDetailCustomization.h"
#include "DetailLayoutBuilder.h"
#include "PropertyHandle.h"
#include "Dom/JsonObject.h"
#include "DragAndDrop/AssetDragDropOp.h"
#include "SceneManagement.h"
#include "SceneView.h"
#include "RXLODStats.h"

namespace
{
    FText T(const FString& Text) { return FText::FromString(Text); }
    TSharedRef<SWidget> Row(const FString& Label, TSharedRef<SWidget> Value)
    { const FText Tip = RXLODTooltips::Label(Label); Value->SetToolTipText(Tip); return SNew(SHorizontalBox).ToolTipText(Tip) + SHorizontalBox::Slot().FillWidth(0.48f).VAlign(VAlign_Center).Padding(8, 3)[SNew(STextBlock).Text(T(Label)).ToolTipText(Tip)] + SHorizontalBox::Slot().FillWidth(0.52f).Padding(4, 3)[Value]; }
    template<typename Number> TSharedRef<SWidget> NumberBox(TFunction<Number()> Get, TFunction<void(Number)> Set, Number Min, Number Max)
    { return SNew(SNumericEntryBox<Number>).MinValue(Min).MaxValue(Max).Value_Lambda([Get]() -> TOptional<Number>{ return Get(); }).OnValueCommitted_Lambda([Set, Min, Max](Number Value, ETextCommit::Type){ Set(FMath::Clamp(Value, Min, Max)); }); }
}

TSharedRef<SWidget> SRXLODWindow::Area(const FString& Label, TSharedRef<SWidget> Body, bool Expanded)
{
    FString StableLabel = BuildingLOD >= 0 && Label.StartsWith(FString::Printf(TEXT("LOD %d"), BuildingLOD)) ? TEXT("LOD") : Label;
    if (Label.StartsWith(TEXT("섹션 ")) && Label.Contains(TEXT("    "))) { StableLabel = Label.Left(Label.Find(TEXT("    "))); }
    const FString Key = FString::Printf(TEXT("%d|%s"), BuildingLOD, *StableLabel);
    const bool* Previous = ExpandedAreas.Find(Key);
    const TWeakPtr<SRXLODWindow> WeakThis = SharedThis(this);
    return SNew(SExpandableArea).InitiallyCollapsed(!(Previous ? *Previous : Expanded))
        .OnAreaExpansionChanged_Lambda([WeakThis, Key](bool Open){ if (const auto Panel = WeakThis.Pin()) { Panel->ExpandedAreas.Add(Key, Open); } })
        .HeaderContent()[SNew(STextBlock).Text(T(Label)).ToolTipText(RXLODTooltips::Label(Label)).Font(FAppStyle::GetFontStyle("DetailsView.CategoryFontStyle"))]
        .BodyContent()[SNew(SBox).Padding(FMargin(24.0f, 2.0f, 0.0f, 4.0f))[Body]];
}

class FRXKoreanDetails final : public IDetailCustomization
{
public:
    explicit FRXKoreanDetails(UScriptStruct* InType) : Type(InType) {}
    void CustomizeDetails(IDetailLayoutBuilder& Builder) override
    {
        for (TFieldIterator<FProperty> It(Type); It; ++It)
        {
            const FText Tip = RXLODTooltips::Property(Type->GetFName(), It->GetFName());
            if (!Tip.IsEmpty())
            {
                const auto Handle = Builder.GetProperty(It->GetFName());
                if (Handle->IsValidHandle()) { SetTooltip(Handle, Tip); }
            }
        }
    }
private:
    void SetTooltip(const TSharedRef<IPropertyHandle>& Handle, const FText& Tip, int32 Depth = 0)
    {
        Handle->SetToolTipText(Tip);
        if (Depth >= 6 || (Handle->GetProperty() && Handle->GetProperty()->IsA<FObjectPropertyBase>())) { return; }
        uint32 Count = 0; Handle->GetNumChildren(Count);
        for (uint32 Index = 0; Index < Count; ++Index) { if (const auto Child = Handle->GetChildHandle(Index)) { SetTooltip(Child.ToSharedRef(), Tip, Depth + 1); } }
    }
    UScriptStruct* Type;
};

class FRXLODViewportClient final : public FEditorViewportClient
{
public:
    FRXLODViewportClient(FPreviewScene* InScene, const TWeakPtr<SEditorViewport>& Widget)
        : FEditorViewportClient(nullptr, InScene, Widget)
    { ExposureSettings.bFixed = true; ExposureSettings.FixedEV100 = 1.0f; }
    void Tick(float DeltaSeconds) override
    {
        FEditorViewportClient::Tick(DeltaSeconds);
        if (PreviewScene && PreviewScene->GetWorld())
        {
            PreviewScene->GetWorld()->Tick(LEVELTICK_All, DeltaSeconds);
            ++WorldTicks;
        }
    }
    uint64 WorldTicks = 0;
    TWeakObjectPtr<UStaticMeshComponent> MeshComponent;
    int32 CurrentLOD = INDEX_NONE;
    float CurrentScreenSize = 0;
    void DrawCanvas(FViewport& InViewport, FSceneView& View, FCanvas& Canvas) override
    {
        FEditorViewportClient::DrawCanvas(InViewport, View, Canvas);
        CurrentLOD = INDEX_NONE;
        if (const auto* Component = MeshComponent.Get())
            if (const UStaticMesh* Mesh = Component->GetStaticMesh())
                if (const auto* Data = Mesh->GetRenderData(); Data && !Data->LODResources.IsEmpty())
                {
                    CurrentScreenSize = ComputeBoundsScreenSize(Component->Bounds.Origin, float(Component->Bounds.SphereRadius), View);
                    const int32 Forced = Component->GetForcedLodModel();
                    CurrentLOD = Forced > 0 ? Forced - 1 : ComputeStaticMeshLOD(Data, Component->Bounds.Origin, float(Component->Bounds.SphereRadius), View, Mesh->GetDefaultMinLOD());
                    CurrentLOD = FMath::Clamp(CurrentLOD, 0, Data->LODResources.Num() - 1);
                }
    }
    bool IsFixedExposure() const { return ExposureSettings.bFixed; }
    float GetEV100() const { return ExposureSettings.FixedEV100; }
    void SetExposure(bool Fixed, float EV100) { ExposureSettings.bFixed = Fixed; ExposureSettings.FixedEV100 = FMath::Clamp(EV100, -6.0f, 16.0f); Invalidate(); }
};

class SRXLODViewport : public SEditorViewport
{
public:
    SLATE_BEGIN_ARGS(SRXLODViewport) {} SLATE_END_ARGS()
    void Construct(const FArguments&)
    {
        Scene = MakeUnique<FPreviewScene>(FPreviewScene::ConstructionValues());
        Scene->SetSkyCubemap(LoadObject<UTextureCube>(nullptr, TEXT("/Engine/EditorMaterials/AssetViewer/EpicQuadPanorama_CC+EV1.EpicQuadPanorama_CC+EV1")));
        Scene->SetSkyBrightness(1.0f); Scene->SetLightDirection(FRotator(-45, -135, 0)); Scene->UpdateCaptureContents();
        SEditorViewport::Construct(SEditorViewport::FArguments());
        // Always composite the viewport into Slate's own render target. This
        // window contains overlays and must not share a direct back-buffer path.
        SetRenderDirectlyToWindow(false);
    }
    ~SRXLODViewport() { if (Component.IsValid()) { Scene->RemoveComponent(Component.Get()); } Client.Reset(); Scene.Reset(); FlushRenderingCommands(); }
    void SetMesh(UStaticMesh* Mesh, int32 LOD)
    {
        if (!Component.IsValid()) { Component.Reset(NewObject<UStaticMeshComponent>(GetTransientPackage(), NAME_None, RF_Transient)); Scene->AddComponent(Component.Get(), FTransform::Identity); }
        Component->SetStaticMesh(Mesh); Component->SetForcedLodModel(LOD);
        Component->SetSectionPreview(INDEX_NONE); Component->SelectedEditorSection = INDEX_NONE;
        auto* RXClient = static_cast<FRXLODViewportClient*>(Client.Get());
        RXClient->MeshComponent = Component.Get(); RXClient->CurrentLOD = INDEX_NONE;
        SectionCounts.Empty();
        if (const auto* Data = Mesh->GetRenderData())
            for (const auto& Resource : Data->LODResources)
            {
                auto& Counts = SectionCounts.AddDefaulted_GetRef();
                for (int32 Section = 0; Section < Resource.Sections.Num(); ++Section) { Counts.Add(RXLODStats::Section(Resource, Section)); }
            }
        if (!bFramed)
        {
            const FVector Center = Mesh->GetBoundingBox().GetCenter();
            const FVector Location = Center + FVector(2, -2, 1.4) * Mesh->GetBoundingBox().GetExtent().GetMax() * 1.6f;
            Client->SetViewLocation(Location); Client->SetViewRotation((Center - Location).Rotation()); bFramed = true;
        }
        Invalidate();
    }
    void ResetFrame() { bFramed = false; }
    TFunction<void(UStaticMesh*)> OnMeshDropped;
    FReply OnDragOver(const FGeometry&, const FDragDropEvent& Event) override
    {
        const auto Op = Event.GetOperationAs<FAssetDragDropOp>();
        if (!Op) { return FReply::Unhandled(); }
        const bool Valid = Op->GetAssets().Num() == 1 && Op->GetAssets()[0].AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName();
        Op->SetToolTip(Valid ? T(TEXT("RX LOD 편집 대상으로 불러오기")) : T(TEXT("스태틱 메시 하나를 드롭하세요.")), nullptr);
        return FReply::Handled();
    }
    void OnDragLeave(const FDragDropEvent& Event) override
    { if (const auto Op = Event.GetOperationAs<FAssetDragDropOp>()) { Op->ResetToDefaultToolTip(); } }
    FReply OnDrop(const FGeometry&, const FDragDropEvent& Event) override
    {
        const auto Op = Event.GetOperationAs<FAssetDragDropOp>();
        if (!Op) { return FReply::Unhandled(); }
        Op->ResetToDefaultToolTip();
        // Reject unsupported drops before loading potentially expensive assets.
        const bool Valid = Op->GetAssets().Num() == 1 && Op->GetAssets()[0].AssetClassPath == UStaticMesh::StaticClass()->GetClassPathName();
        UStaticMesh* Mesh = Valid ? Cast<UStaticMesh>(Op->GetAssets()[0].GetAsset()) : nullptr;
        if (OnMeshDropped) { OnMeshDropped(Mesh); }
        return FReply::Handled();
    }
    FText StatisticsText() const
    {
        const auto* RXClient = static_cast<FRXLODViewportClient*>(Client.Get());
        const UStaticMesh* Mesh = Component.IsValid() ? Component->GetStaticMesh() : nullptr;
        const auto* Data = Mesh ? Mesh->GetRenderData() : nullptr;
        const int32 LOD = RXClient->CurrentLOD;
        if (!Data || !Data->LODResources.IsValidIndex(LOD)) { return T(TEXT("메시 통계 준비 중…")); }
        const auto& Resource = Data->LODResources[LOD];
        FString Text = FString::Printf(TEXT("현재 LOD: %d (%s)\n현재 스크린 크기: %.6f\n폴리곤(삼각형): %u\n버텍스: %u"), LOD,
            Component->GetForcedLodModel() == 0 ? TEXT("자동") : TEXT("고정"), RXClient->CurrentScreenSize, Resource.GetNumTriangles(), Resource.GetNumVertices());
        if (Mesh->IsNaniteEnabled()) { Text += TEXT("\nNanite: 기본 LOD 지오메트리 통계"); }
        if (SectionCounts.IsValidIndex(LOD))
            for (int32 Section = 0; Section < SectionCounts[LOD].Num(); ++Section)
            {
                const auto& Count = SectionCounts[LOD][Section];
                Text += FString::Printf(TEXT("\n섹션 %d: 폴리곤 %u · 버텍스 %u"), Section, Count.Triangles, Count.Vertices);
            }
        return T(Text);
    }
    void ForceLOD(int32 LOD) { if (Component.IsValid()) { Component->SetForcedLodModel(FMath::Clamp(LOD, 0, Component->GetStaticMesh() ? Component->GetStaticMesh()->GetNumSourceModels() : 0)); Invalidate(); } }
    void Isolate(int32 Section, bool Enabled) { if (Component.IsValid()) { Component->SetSectionPreview(Enabled ? Section : INDEX_NONE); Invalidate(); } }
    void Highlight(int32 Section, bool Enabled) { if (Component.IsValid()) { Component->SelectedEditorSection = Enabled ? Section : INDEX_NONE; Component->MarkRenderStateDirty(); Invalidate(); } }
    bool FixedExposure() const { return static_cast<FRXLODViewportClient*>(Client.Get())->IsFixedExposure(); }
    float ExposureEV100() const { return static_cast<FRXLODViewportClient*>(Client.Get())->GetEV100(); }
    void SetExposure(bool Fixed, float Value) { static_cast<FRXLODViewportClient*>(Client.Get())->SetExposure(Fixed, Value); }
    TSharedRef<FJsonObject> Diagnostics() const
    {
        auto Result = MakeShared<FJsonObject>();
        const auto* RXClient = static_cast<FRXLODViewportClient*>(Client.Get());
        Result->SetNumberField(TEXT("world_time"), Scene->GetWorld()->GetTimeSeconds());
        Result->SetNumberField(TEXT("world_ticks"), RXClient->WorldTicks);
        Result->SetBoolField(TEXT("fixed_exposure"), FixedExposure()); Result->SetNumberField(TEXT("ev100"), ExposureEV100());
        Result->SetBoolField(TEXT("slate_composited"), true);
        Result->SetNumberField(TEXT("forced_lod"), Component.IsValid() ? Component->GetForcedLodModel() - 1 : -1);
        Result->SetNumberField(TEXT("current_lod"), RXClient->CurrentLOD);
        Result->SetNumberField(TEXT("current_screen_size"), RXClient->CurrentScreenSize);
        Result->SetStringField(TEXT("statistics_text"), StatisticsText().ToString());
        TArray<TSharedPtr<FJsonValue>> Sections;
        const int32 LOD = RXClient->CurrentLOD;
        const auto* Data = Component.IsValid() && Component->GetStaticMesh() ? Component->GetStaticMesh()->GetRenderData() : nullptr;
        if (Data && Data->LODResources.IsValidIndex(LOD))
        {
            Result->SetNumberField(TEXT("triangles"), Data->LODResources[LOD].GetNumTriangles());
            Result->SetNumberField(TEXT("vertices"), Data->LODResources[LOD].GetNumVertices());
        }
        if (SectionCounts.IsValidIndex(LOD))
            for (int32 Index = 0; Index < SectionCounts[LOD].Num(); ++Index)
            {
                auto Section = MakeShared<FJsonObject>(); Section->SetNumberField(TEXT("section"), Index);
                Section->SetNumberField(TEXT("triangles"), SectionCounts[LOD][Index].Triangles);
                Section->SetNumberField(TEXT("vertices"), SectionCounts[LOD][Index].Vertices);
                Sections.Add(MakeShared<FJsonValueObject>(Section));
            }
        Result->SetArrayField(TEXT("sections"), Sections);
        return Result;
    }
protected:
    TSharedRef<FEditorViewportClient> MakeEditorViewportClient() override
    {
        auto Result = MakeShared<FRXLODViewportClient>(Scene.Get(), SharedThis(this));
        Result->SetViewLocation(FVector(200, -200, 150)); Result->SetViewRotation(FRotator(-20, 135, 0));
        Result->SetViewportType(LVT_Perspective);
        Result->SetViewMode(VMI_Lit); Result->SetRealtime(true); Result->EngineShowFlags.SetGrid(true);
        return Result;
    }
private:
    TUniquePtr<FPreviewScene> Scene;
    TStrongObjectPtr<UStaticMeshComponent> Component;
    bool bFramed = false;
    TArray<TArray<FRXSectionStats>> SectionCounts;
};

void SRXLODWindow::Construct(const FArguments& Args)
{
    Session = Args._Session;
    for (const TCHAR* Name : { TEXT("Triangles"), TEXT("Vertices"), TEXT("First Percent Satisfied") }) { CriterionChoices.Add(MakeShared<FString>(Name)); }
    ChildSlot [SNew(SSplitter)
        + SSplitter::Slot().Value(0.6f)
        [ SNew(SOverlay)
            + SOverlay::Slot()[SAssignNew(Viewport, SRXLODViewport)]
            + SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Left).Padding(8)
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(T(TEXT("원본에 적용"))).ToolTipText(RXLODTooltips::Label(TEXT("원본에 적용"))).OnClicked(this, &SRXLODWindow::Apply)]
                + SHorizontalBox::Slot().AutoWidth().Padding(8, 0)[SNew(SBox).WidthOverride(145)
                    [SAssignNew(ViewportLODSelector, SComboBox<TSharedPtr<FString>>).OptionsSource(&LODChoices).ToolTipText(RXLODTooltips::Label(TEXT("LOD")))
                        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(T(*Item)); })
                        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type Info){ if (Info != ESelectInfo::Direct) { const int32 Index = LODChoices.IndexOfByKey(Item); if (Index >= 0) { SelectLOD(Index - 1); } } })
                        [SNew(STextBlock).Text_Lambda([this](){ return T(LODChoices.IsValidIndex(SelectedLOD) ? *LODChoices[SelectedLOD] : TEXT("LOD Auto")); })]]]
                + SHorizontalBox::Slot().AutoWidth().Padding(12, 0)[SNew(SCheckBox).ToolTipText(RXLODTooltips::Label(TEXT("고정 노출")))
                    .IsChecked_Lambda([this](){ return Viewport->FixedExposure() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                    .OnCheckStateChanged_Lambda([this](ECheckBoxState State){ Viewport->SetExposure(State == ECheckBoxState::Checked, Viewport->ExposureEV100()); })
                    [SNew(STextBlock).Text(T(TEXT("고정 노출")))]]
                + SHorizontalBox::Slot().AutoWidth().VAlign(VAlign_Center)[SNew(STextBlock).Text(T(TEXT("EV100 "))).ToolTipText(RXLODTooltips::Label(TEXT("노출 (EV100)"))) ]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SBox).WidthOverride(70)[SNew(SNumericEntryBox<float>).ToolTipText(RXLODTooltips::Label(TEXT("노출 (EV100)")))
                    .MinValue(-6.0f).MaxValue(16.0f).Value_Lambda([this]() -> TOptional<float> { return Viewport->ExposureEV100(); })
                    .IsEnabled_Lambda([this](){ return Viewport->FixedExposure(); })
                    .OnValueCommitted_Lambda([this](float Value, ETextCommit::Type){ Viewport->SetExposure(true, Value); })]]
            ]
            + SOverlay::Slot().VAlign(VAlign_Top).HAlign(HAlign_Left).Padding(16, 48, 16, 40)
            [SNew(SBox).WidthOverride(320).MaxDesiredHeight(300)
                [SNew(SBorder).BorderImage(FAppStyle::GetBrush("ToolPanel.GroupBorder")).BorderBackgroundColor(FLinearColor(0.015f, 0.015f, 0.015f, 0.8f)).Padding(10)
                    [SNew(SScrollBox) + SScrollBox::Slot()
                        [SNew(STextBlock).Text_Lambda([this](){ return Viewport->StatisticsText(); }).SimpleTextMode(true)
                            .ColorAndOpacity(FLinearColor::White).ShadowOffset(FVector2D(1, 1))
                            .ToolTipText(T(TEXT("현재 카메라에서 계산한 스크린 크기와 표시 LOD의 렌더 지오메트리 통계입니다. 폴리곤은 삼각형 수이며, 섹션 버텍스는 해당 섹션이 참조하는 고유 렌더 버텍스 수입니다. UV·노멀 경계에서 분리된 버텍스도 포함합니다.")))]]]]
            + SOverlay::Slot().VAlign(VAlign_Bottom).Padding(8)
            [SNew(SBorder)[SNew(STextBlock).AutoWrapText(true).Text_Lambda([this](){ return T(Session->Status); })]]
        ]
        + SSplitter::Slot().Value(0.4f)
        [SNew(SVerticalBox)
            + SVerticalBox::Slot().AutoHeight()
            [SNew(SHorizontalBox)
                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(T(TEXT("JSON 불러오기"))).ToolTipText(RXLODTooltips::Label(TEXT("JSON 불러오기"))).OnClicked_Lambda([this](){ return JsonFile(true, false); })]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(T(TEXT("저장"))).ToolTipText(RXLODTooltips::Label(TEXT("저장"))).OnClicked_Lambda([this](){ return JsonFile(false, false); })]
                + SHorizontalBox::Slot().AutoWidth()[SNew(SButton).Text(T(TEXT("내보내기"))).ToolTipText(RXLODTooltips::Label(TEXT("내보내기"))).OnClicked_Lambda([this](){ return JsonFile(false, true); })]
            ]
            + SVerticalBox::Slot().FillHeight(1)[SNew(SScrollBox) + SScrollBox::Slot()[SAssignNew(Panel, SVerticalBox)]]
        ]
    ];
    Viewport->OnMeshDropped = [this](UStaticMesh* Mesh){ FString E; if (!RXLOD::LoadAsset(Mesh, E)) { Error(E); } };
    Viewport->SetMesh(Session->Display.Get(), SelectedLOD);
    RefreshPanel();
}

TSharedRef<SWidget> SRXLODWindow::StructEditor(UScriptStruct* Type, const void* Data, TFunction<void(const void*)> Commit, bool HidePercent)
{
    auto Buffer = MakeShared<FStructOnScope>(Type); Type->CopyScriptStruct(Buffer->GetStructMemory(), Data);
    FDetailsViewArgs Args; Args.bAllowSearch = false; Args.bShowOptions = false; Args.bHideSelectionTip = true; Args.NameAreaSettings = FDetailsViewArgs::HideNameArea;
    FStructureDetailsViewArgs StructureArgs;
    auto View = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateStructureDetailView(Args, StructureArgs, Buffer);
    View->GetDetailsView()->SetGenericLayoutDetailsDelegate(FOnGetDetailCustomizationInstance::CreateLambda([Type](){ return MakeShared<FRXKoreanDetails>(Type); }));
    if (HidePercent)
        View->GetDetailsView()->SetIsPropertyVisibleDelegate(FIsPropertyVisible::CreateLambda([](const FPropertyAndParent& P)
        { const FName Name = P.Property.GetFName(); return Name != TEXT("PercentTriangles") && Name != TEXT("PercentVertices") && Name != TEXT("MaxNumOfTriangles") && Name != TEXT("MaxNumOfVerts") && Name != TEXT("WeldingThreshold") && Name != TEXT("BaseLODModel") && Name != TEXT("TerminationCriterion"); }));
    View->GetDetailsView()->ForceRefresh();
    View->GetOnFinishedChangingPropertiesDelegate().AddLambda([Buffer, Commit](const FPropertyChangedEvent&){ Commit(Buffer->GetStructMemory()); });
    StructureViews.Add(View);
    return View->GetWidget().ToSharedRef();
}

void SRXLODWindow::RefreshPanel()
{
    Panel->ClearChildren(); StructureViews.Reset(); LODChoices.Reset(); GroupChoices.Reset(); MaterialChoices.Reset();
    for (int32 Index = 0; Index < Session->Display->GetStaticMaterials().Num(); ++Index)
        MaterialChoices.Add(MakeShared<FString>(FString::Printf(TEXT("[%d] %s"), Index, *Session->Display->GetStaticMaterials()[Index].MaterialSlotName.ToString())));
    const int32 Count = Session->Preview->GetNumSourceModels() + (Session->GetVisibleImpostorLOD() != INDEX_NONE ? 1 : 0);
    for (int32 LOD = 0; LOD <= Count; ++LOD) { LODChoices.Add(MakeShared<FString>(LOD == 0 ? TEXT("LOD Auto") : FString::Printf(TEXT("LOD %d%s"), LOD - 1, LOD - 1 == Session->GetVisibleImpostorLOD() ? TEXT(" (임포스터)") : TEXT("")))); }
    SelectedLOD = FMath::Clamp(SelectedLOD, 0, Count);
    if (ViewportLODSelector.IsValid()) { ViewportLODSelector->RefreshOptions(); ViewportLODSelector->SetSelectedItem(LODChoices[SelectedLOD]); }
    auto Select = SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&LODChoices).InitiallySelectedItem(LODChoices[SelectedLOD])
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(T(*Item)); })
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type Info){ const int32 Index = LODChoices.IndexOfByKey(Item); if (Index >= 0 && Info != ESelectInfo::Direct) { SelectLOD(Index - 1); } })
        [SNew(STextBlock).Text_Lambda([this](){ return T(LODChoices.IsValidIndex(SelectedLOD) ? *LODChoices[SelectedLOD] : TEXT("LOD Auto")); })];
    auto Selection = SNew(SVerticalBox);
    Selection->AddSlot().AutoHeight()[Row(TEXT("LOD"), Select)];
    Selection->AddSlot().AutoHeight()[Row(TEXT("커스텀"), SNew(SCheckBox).IsChecked(bCustom ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).OnCheckStateChanged_Lambda([this](ECheckBoxState State){ bCustom = State == ECheckBoxState::Checked; if (bCustom && VisibleLODs.IsEmpty()) { for (int32 LOD = 0; LOD < Session->Display->GetNumSourceModels(); ++LOD) { VisibleLODs.Add(LOD); } } RefreshPanel(); }))];
    if (bCustom)
    {
        for (int32 LOD = 0; LOD < Count; ++LOD)
            Selection->AddSlot().AutoHeight()[Row(FString::Printf(TEXT("LOD %d"), LOD), SNew(SCheckBox).IsChecked(VisibleLODs.Contains(LOD) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).OnCheckStateChanged_Lambda([this, LOD](ECheckBoxState State){ if (State == ECheckBoxState::Checked) { VisibleLODs.Add(LOD); } else { VisibleLODs.Remove(LOD); } RefreshPanel(); }))];
    }
    Panel->AddSlot().AutoHeight()[Area(TEXT("LOD 선택 툴"), Selection)];
    for (int32 LOD = 0; LOD < Count; ++LOD)
        if ((!bCustom && (SelectedLOD == 0 || LOD == SelectedLOD - 1)) || (bCustom && VisibleLODs.Contains(LOD)))
            Panel->AddSlot().AutoHeight().Padding(0, 3)[LODPanel(LOD)];
    Panel->AddSlot().AutoHeight()[Area(TEXT("LOD 설정"), CommonPanel())];
    auto ImpostorControls = SNew(SVerticalBox);
    ImpostorControls->AddSlot().AutoHeight()[Row(TEXT("마지막 LOD에 임포스터"), SNew(SCheckBox).IsChecked(Session->bImpostor ? ECheckBoxState::Checked : ECheckBoxState::Unchecked).OnCheckStateChanged_Lambda([this](ECheckBoxState State){ Session->bImpostor = State == ECheckBoxState::Checked; Rebuild(); }))];
    ImpostorControls->AddSlot().AutoHeight()[SNew(SButton).Text(T(TEXT("임포스터 다시 생성 (마지막 LOD 교체)"))).ToolTipText(RXLODTooltips::Label(TEXT("임포스터 다시 생성 (마지막 LOD 교체)"))).IsEnabled_Lambda([this](){ return Session->bImpostor; }).OnClicked(this, &SRXLODWindow::Rebuild)];
    Panel->AddSlot().AutoHeight().Padding(0, 6)[Area(TEXT("임포스터"), ImpostorControls)];
    Panel->AddSlot().AutoHeight()[SNew(SButton).Text(T(TEXT("미리보기 변경 내용 적용"))).ToolTipText(RXLODTooltips::Label(TEXT("미리보기 변경 내용 적용"))).OnClicked(this, &SRXLODWindow::Rebuild)];
}

TSharedRef<SWidget> SRXLODWindow::LODPanel(int32 LOD)
{
    TGuardValue<int32> LODScope(BuildingLOD, LOD);
    const auto* Data = Session->Display->GetRenderData();
    FString Label = FString::Printf(TEXT("LOD %d%s"), LOD, LOD == Session->GetVisibleImpostorLOD() ? TEXT(" · 임포스터") : TEXT(""));
    if (Data && Data->LODResources.IsValidIndex(LOD)) { Label += FString::Printf(TEXT("    트라이앵글: %u  버텍스: %u"), Data->LODResources[LOD].GetNumTriangles(), Data->LODResources[LOD].GetNumVertices()); }
    auto Body = SNew(SVerticalBox);
    Body->AddSlot().AutoHeight()[Area(TEXT("섹션"), SectionsPanel(LOD))];
    if (LOD == Session->GetVisibleImpostorLOD()) { return Area(Label, Body); }
    auto& Model = Session->Preview->GetSourceModel(LOD);
    const auto ScreenSize = Session->GetScreenSize(LOD);
    FRXLODScreenSizeSettings Screen;
    Screen.Default = ScreenSize.Default; Screen.PerPlatform = ScreenSize.PerPlatform;
    Body->AddSlot().AutoHeight()[Area(TEXT("화면 크기"), StructEditor(FRXLODScreenSizeSettings::StaticStruct(), &Screen, [this, LOD](const void* P)
    {
        const auto& Value = *static_cast<const FRXLODScreenSizeSettings*>(P);
        FPerPlatformFloat Target(Value.Default); Target.PerPlatform = Value.PerPlatform;
        FString E;
        if (!Session->SetScreenSize(LOD, Target, E)) { Error(E); }
        RegisterActiveTimer(0.0f, FWidgetActiveTimerDelegate::CreateSP(this, &SRXLODWindow::RefreshPanelNextTick));
    }))];
    auto Reduction = SNew(SVerticalBox);
    Reduction->AddSlot().AutoHeight()[Row(TEXT("종료 기준"), SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&CriterionChoices)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(T(*Item)); })
        .OnSelectionChanged_Lambda([this, LOD](TSharedPtr<FString> Item, ESelectInfo::Type){ const int32 Index = CriterionChoices.IndexOfByKey(Item); if (Index >= 0) { Session->Preview->GetSourceModel(LOD).ReductionSettings.TerminationCriterion = static_cast<EStaticMeshReductionTerimationCriterion>(Index); } })
        [SNew(STextBlock).Text_Lambda([this, LOD](){ const int32 Index = int32(Session->Preview->GetSourceModel(LOD).ReductionSettings.TerminationCriterion); return T(CriterionChoices.IsValidIndex(Index) ? *CriterionChoices[Index] : TEXT("Triangles")); })])];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("퍼센트 트라이앵글"), NumberBox<float>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.PercentTriangles * 100.0f; }, [this, LOD](float Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.PercentTriangles = Value / 100.0f; }, 0, 100))];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("퍼센트 버텍스"), NumberBox<float>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.PercentVertices * 100.0f; }, [this, LOD](float Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.PercentVertices = Value / 100.0f; }, 0, 100))];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("최대 트라이앵글 수"), NumberBox<uint32>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.MaxNumOfTriangles; }, [this, LOD](uint32 Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.MaxNumOfTriangles = Value; }, 2, MAX_uint32))];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("최대 버텍스 수"), NumberBox<uint32>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.MaxNumOfVerts; }, [this, LOD](uint32 Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.MaxNumOfVerts = Value; }, 4, MAX_uint32))];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("결합 한계치"), NumberBox<float>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.WeldingThreshold; }, [this, LOD](float Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.WeldingThreshold = Value; }, 0, MAX_flt))];
    Reduction->AddSlot().AutoHeight()[Row(TEXT("베이스 LOD"), NumberBox<int32>([this, LOD](){ return Session->Preview->GetSourceModel(LOD).ReductionSettings.BaseLODModel; }, [this, LOD](int32 Value){ Session->Preview->GetSourceModel(LOD).ReductionSettings.BaseLODModel = Value; }, 0, LOD))];
    Reduction->AddSlot().AutoHeight()[StructEditor(FMeshReductionSettings::StaticStruct(), &Model.ReductionSettings, [this, LOD](const void* P)
    {
        auto& Settings = Session->Preview->GetSourceModel(LOD).ReductionSettings;
        const auto Previous = Settings;
        Settings = *static_cast<const FMeshReductionSettings*>(P);
        Settings.PercentTriangles = Previous.PercentTriangles; Settings.PercentVertices = Previous.PercentVertices;
        Settings.MaxNumOfTriangles = Previous.MaxNumOfTriangles; Settings.MaxNumOfVerts = Previous.MaxNumOfVerts;
        Settings.WeldingThreshold = Previous.WeldingThreshold; Settings.BaseLODModel = Previous.BaseLODModel; Settings.TerminationCriterion = Previous.TerminationCriterion;
    }, true)];
    Reduction->AddSlot().AutoHeight()[SNew(SButton).Text(T(TEXT("변경 내용 적용"))).ToolTipText(RXLODTooltips::Label(TEXT("변경 내용 적용"))).OnClicked(this, &SRXLODWindow::Rebuild)];
    Body->AddSlot().AutoHeight()[Area(TEXT("감소 세팅"), Reduction)];
    if (LOD > 0)
        Body->AddSlot().AutoHeight()[SNew(SButton).Text(T(TEXT("LOD 제거"))).ToolTipText(RXLODTooltips::Label(TEXT("LOD 제거"))).OnClicked_Lambda([this, LOD](){ FString E; if (!Session->Remove(LOD, E)) { Error(E); } else { Rebuild(); } return FReply::Handled(); })];
    return Area(Label, Body, LOD > 0);
}

TSharedRef<SWidget> SRXLODWindow::SectionsPanel(int32 LOD)
{
    auto Body = SNew(SVerticalBox);
    const auto* Data = Session->Display->GetRenderData();
    if (!Data || !Data->LODResources.IsValidIndex(LOD)) { return Body; }
    const auto& Sections = Data->LODResources[LOD].Sections;
    for (int32 Section = 0; Section < Sections.Num(); ++Section)
    {
        auto Fields = SNew(SVerticalBox);
        const int32 MaterialIndex = Sections[Section].MaterialIndex;
        Fields->AddSlot().AutoHeight()[Row(TEXT("머티리얼 슬롯"), SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&MaterialChoices).IsEnabled(LOD != Session->GetVisibleImpostorLOD())
            .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(T(*Item)); })
            .OnSelectionChanged_Lambda([this, LOD, Section](TSharedPtr<FString> Item, ESelectInfo::Type){ const int32 Index = MaterialChoices.IndexOfByKey(Item); if (Session->Preview->GetStaticMaterials().IsValidIndex(Index)) { auto Info = Session->Preview->GetSectionInfoMap().Get(LOD, Section); Info.MaterialIndex = Index; Session->Preview->GetSectionInfoMap().Set(LOD, Section, Info); Session->Preview->GetOriginalSectionInfoMap().Set(LOD, Section, Info); } })
            [SNew(STextBlock).Text_Lambda([this, LOD, Section, MaterialIndex](){ const int32 Index = LOD == Session->GetVisibleImpostorLOD() ? MaterialIndex : Session->Preview->GetSectionInfoMap().Get(LOD, Section).MaterialIndex; return T(MaterialChoices.IsValidIndex(Index) ? *MaterialChoices[Index] : TEXT("없음")); })])];
        Fields->AddSlot().AutoHeight()[Row(TEXT("강조"), SNew(SCheckBox).OnCheckStateChanged_Lambda([this, LOD, Section](ECheckBoxState State){ SelectedLOD = LOD + 1; Viewport->ForceLOD(LOD + 1); Viewport->Highlight(Section, State == ECheckBoxState::Checked); }))];
        Fields->AddSlot().AutoHeight()[Row(TEXT("격리"), SNew(SCheckBox).OnCheckStateChanged_Lambda([this, LOD, Section](ECheckBoxState State){ SelectedLOD = LOD + 1; Viewport->ForceLOD(LOD + 1); Viewport->Isolate(Section, State == ECheckBoxState::Checked); }))];
        for (const auto& Flag : TArray<TPair<FString, bool FMeshSectionInfo::*>>{ { TEXT("그림자 드리우기"), &FMeshSectionInfo::bCastShadow }, { TEXT("충돌 활성화"), &FMeshSectionInfo::bEnableCollision }, { TEXT("레이 트레이싱 표시"), &FMeshSectionInfo::bVisibleInRayTracing }, { TEXT("디스턴스 필드 라이팅 영향"), &FMeshSectionInfo::bAffectDistanceFieldLighting }, { TEXT("불투명 강제"), &FMeshSectionInfo::bForceOpaque } })
            Fields->AddSlot().AutoHeight()[Row(Flag.Key, SNew(SCheckBox).IsEnabled(LOD != Session->GetVisibleImpostorLOD())
                .IsChecked_Lambda([this, LOD, Section, Flag](){ const auto Info = LOD == Session->GetVisibleImpostorLOD() ? Session->Display->GetSectionInfoMap().Get(LOD, Section) : Session->Preview->GetSectionInfoMap().Get(LOD, Section); return (Info.*Flag.Value) ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                .OnCheckStateChanged_Lambda([this, LOD, Section, Flag](ECheckBoxState State){ auto Info = Session->Preview->GetSectionInfoMap().Get(LOD, Section); Info.*Flag.Value = State == ECheckBoxState::Checked; Session->Preview->GetSectionInfoMap().Set(LOD, Section, Info); Session->Preview->GetOriginalSectionInfoMap().Set(LOD, Section, Info); }))];
        if (LOD > 0 && LOD != Session->GetVisibleImpostorLOD())
        {
            const FMeshDescription* Base = Session->Preview->GetMeshDescription(0);
            auto& Weights = Session->GetWeights(LOD);
            // Render sections are matched by imported material slot name. Ambiguous
            // merged sections expose each source group explicitly instead of guessing.
            const FName Slot = Session->Display->GetStaticMaterials().IsValidIndex(MaterialIndex) ? Session->Display->GetStaticMaterials()[MaterialIndex].ImportedMaterialSlotName : NAME_None;
            if (Base)
            {
                FStaticMeshConstAttributes Attributes(*Base); const auto Names = Attributes.GetPolygonGroupMaterialSlotNames();
                bool bAnyMatch = false;
                for (const auto& Weight : Weights) { bAnyMatch |= Names[Weight.Group] == Slot; }
                for (int32 WeightIndex = 0; WeightIndex < Weights.Num(); ++WeightIndex)
                {
                    const auto& Weight = Weights[WeightIndex];
                    if (bAnyMatch ? Names[Weight.Group] != Slot : (Weights.Num() == Sections.Num() ? WeightIndex != Section : Section != 0)) { continue; }
                    const FPolygonGroupID Group = Weight.Group;
                    auto Get = [this, LOD, Group]() -> FRXSectionWeight& { return *Session->GetWeights(LOD).FindByPredicate([Group](const FRXSectionWeight& W){ return W.Group == Group; }); };
                    Fields->AddSlot().AutoHeight()[Row(FString::Printf(TEXT("섹션 가중치 (그룹 %d)"), Group.GetValue()), NumberBox<float>([Get](){ return Get().Weight; }, [Get](float V){ Get().Weight = V; }, 0, 1))];
                    for (const auto& Option : TArray<TPair<FString, bool FRXSectionWeight::*>>{
                        { TEXT("열린 외곽 경계 보존"), &FRXSectionWeight::bPreserveOpenBoundary },
                        { TEXT("꺾인 모서리 보존"), &FRXSectionWeight::bPreserveSharpEdges },
                        { TEXT("UV·노멀·머티리얼 경계 보존"), &FRXSectionWeight::bPreserveAttributeSeams } })
                        Fields->AddSlot().AutoHeight()[Row(Option.Key, SNew(SCheckBox)
                            .IsChecked_Lambda([Get, Option](){ return Get().*Option.Value ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; })
                            .OnCheckStateChanged_Lambda([Get, Option](ECheckBoxState State){ Get().*Option.Value = State == ECheckBoxState::Checked; }))];
                    Fields->AddSlot().AutoHeight()[Row(TEXT("감소 제외"), SNew(SCheckBox).IsChecked_Lambda([Get](){ return Get().bExcluded ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([Get](ECheckBoxState State){ Get().bExcluded = State == ECheckBoxState::Checked; }))];
                }
            }
        }
        else if (LOD == 0)
        {
            Fields->AddSlot().AutoHeight()[Row(TEXT("섹션 가중치 (LOD 0 원본 보존)"), SNew(SNumericEntryBox<float>).Value(1.0f).IsEnabled(false))];
            for (const TCHAR* Label : { TEXT("열린 외곽 경계 보존"), TEXT("꺾인 모서리 보존"), TEXT("UV·노멀·머티리얼 경계 보존") })
                Fields->AddSlot().AutoHeight()[Row(Label, SNew(SCheckBox).IsChecked(ECheckBoxState::Unchecked).IsEnabled(false))];
            Fields->AddSlot().AutoHeight()[Row(TEXT("감소 제외"), SNew(SCheckBox).IsChecked(ECheckBoxState::Checked).IsEnabled(false))];
        }
        const auto Counts = RXLODStats::Section(Data->LODResources[LOD], Section);
        Body->AddSlot().AutoHeight()[Area(FString::Printf(TEXT("섹션 %d    폴리곤: %u  버텍스: %u"), Section, Counts.Triangles, Counts.Vertices), Fields)];
    }
    return Body;
}

TSharedRef<SWidget> SRXLODWindow::CommonPanel()
{
    auto Body = SNew(SVerticalBox);
    TArray<FName> Groups; UStaticMesh::GetLODGroups(Groups); if (!Groups.Contains(NAME_None)) { Groups.Insert(NAME_None, 0); }
    for (FName Group : Groups) { GroupChoices.Add(MakeShared<FString>(Group.ToString())); }
    Body->AddSlot().AutoHeight()[Row(TEXT("LOD 그룹"), SNew(SComboBox<TSharedPtr<FString>>).OptionsSource(&GroupChoices)
        .OnGenerateWidget_Lambda([](TSharedPtr<FString> Item){ return SNew(STextBlock).Text(T(*Item)); })
        .OnSelectionChanged_Lambda([this](TSharedPtr<FString> Item, ESelectInfo::Type){ if (Item) { Session->Preview->SetLODGroup(FName(**Item), false, false); Rebuild(); } })
        [SNew(STextBlock).Text_Lambda([this](){ return FText::FromName(Session->Preview->GetLODGroup()); })])];
    Body->AddSlot().AutoHeight()[Row(TEXT("LOD 임포트"), SNew(SButton).Text(T(TEXT("파일에서 LOD 가져오기…"))).ToolTipText(RXLODTooltips::Label(TEXT("파일에서 LOD 가져오기…"))).OnClicked_Lambda([this]()
    {
        TArray<FString> Files; auto* Desktop = FDesktopPlatformModule::Get();
        if (Desktop && Desktop->OpenFileDialog(FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared()), TEXT("LOD 임포트"), FString(), FString(), TEXT("Static Mesh|*.fbx;*.obj"), 0, Files) && !Files.IsEmpty())
        {
            const int32 LOD = SelectedLOD > 1 ? SelectedLOD - 1 : Session->Preview->GetNumSourceModels();
            if (LOD >= MAX_STATIC_MESH_LODS - (Session->bImpostor ? 1 : 0)) { Error(TEXT("LOD 슬롯이 없습니다.")); }
            else if (!FbxMeshUtils::ImportStaticMeshLOD(Session->Preview.Get(), Files[0], LOD, false)) { Error(TEXT("LOD 임포트에 실패했습니다.")); }
            else { Session->Weights.Remove(LOD); Rebuild(); }
        }
        return FReply::Handled();
    }))];
    FRXLODCommonSettings Common;
    for (TFieldIterator<FProperty> It(FRXLODCommonSettings::StaticStruct()); It; ++It)
        if (FProperty* P = FindFProperty<FProperty>(UStaticMesh::StaticClass(), It->GetFName()))
            if (P->SameType(*It)) { It->CopyCompleteValue(It->ContainerPtrToValuePtr<void>(&Common), P->ContainerPtrToValuePtr<void>(Session->Preview.Get())); }
    Body->AddSlot().AutoHeight()[StructEditor(FRXLODCommonSettings::StaticStruct(), &Common, [this](const void* Ptr)
    {
        for (TFieldIterator<FProperty> It(FRXLODCommonSettings::StaticStruct()); It; ++It)
            if (FProperty* P = FindFProperty<FProperty>(UStaticMesh::StaticClass(), It->GetFName()))
                if (P->SameType(*It)) { P->CopyCompleteValue(P->ContainerPtrToValuePtr<void>(Session->Preview.Get()), It->ContainerPtrToValuePtr<void>(Ptr)); }
    })];
    Body->AddSlot().AutoHeight()[Row(TEXT("LOD 개수 (임포스터 제외)"), NumberBox<int32>([this](){ return Session->Preview->GetNumSourceModels(); }, [this](int32 Count){ FString E; if (Session->Resize(Count, E)) { Rebuild(); } else { Error(E); } }, 1, MAX_STATIC_MESH_LODS))];
    Body->AddSlot().AutoHeight()[Row(TEXT("LOD 거리 자동 계산"), SNew(SCheckBox).IsChecked_Lambda([this](){ return Session->Preview->GetAutoComputeLODScreenSize() ? ECheckBoxState::Checked : ECheckBoxState::Unchecked; }).OnCheckStateChanged_Lambda([this](ECheckBoxState State){ Session->SetAutoScreenSize(State == ECheckBoxState::Checked); Rebuild(); }))];
    FSinglePropertyParams Params; Params.NameOverride = T(TEXT("LOD 스트리밍"));
    auto Streaming = FModuleManager::LoadModuleChecked<FPropertyEditorModule>(TEXT("PropertyEditor")).CreateSingleProperty(Session->Preview.Get(), TEXT("LODStreaming"), Params);
    if (Streaming.IsValid()) { Streaming->SetToolTipText(RXLODTooltips::Label(TEXT("LOD 스트리밍"))); Streaming->GetPropertyHandle()->SetToolTipText(RXLODTooltips::Label(TEXT("LOD 스트리밍"))); Body->AddSlot().AutoHeight()[Streaming.ToSharedRef()]; }
    Body->AddSlot().AutoHeight()[SNew(SButton).Text(T(TEXT("변경 내용 적용"))).ToolTipText(RXLODTooltips::Label(TEXT("변경 내용 적용"))).OnClicked(this, &SRXLODWindow::Rebuild)];
    return Body;
}

void SRXLODWindow::Error(const FString& Text) { Session->Status = Text; }
FReply SRXLODWindow::OnDragOver(const FGeometry&, const FDragDropEvent& Event)
{
    return Viewport->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()) ? Viewport->OnDragOver(Viewport->GetCachedGeometry(), Event) : FReply::Unhandled();
}
FReply SRXLODWindow::OnDrop(const FGeometry&, const FDragDropEvent& Event)
{
    // The stats overlay can be the hit-test target. Forward its unhandled drop
    // only inside the viewport so the property panel never changes the asset.
    return Viewport->GetCachedGeometry().IsUnderLocation(Event.GetScreenSpacePosition()) ? Viewport->OnDrop(Viewport->GetCachedGeometry(), Event) : FReply::Unhandled();
}
void SRXLODWindow::OnDragLeave(const FDragDropEvent& Event) { Viewport->OnDragLeave(Event); }
void SRXLODWindow::SetSession(TSharedPtr<FRXLODSession> NewSession)
{
    Session = MoveTemp(NewSession); SelectedLOD = 0; bCustom = false; VisibleLODs.Empty(); ExpandedAreas.Empty(); JsonFilename.Empty();
    Viewport->ResetFrame(); Viewport->SetMesh(Session->Display.Get(), 0); RefreshPanel();
}
#if WITH_DEV_AUTOMATION_TESTS
bool SRXLODWindow::DropAssetsForTest(const TArray<FAssetData>& Assets)
{
    const FDragDropEvent Event(FPointerEvent(), FAssetDragDropOp::New(Assets));
    return Viewport->OnDragOver(FGeometry(), Event).IsEventHandled() && Viewport->OnDrop(FGeometry(), Event).IsEventHandled();
}
#endif
TSharedRef<FJsonObject> SRXLODWindow::ViewportDiagnostics() const { return Viewport->Diagnostics(); }
void SRXLODWindow::SelectLOD(int32 LOD) { SelectedLOD = LOD + 1; Viewport->ForceLOD(SelectedLOD); RefreshPanel(); }
bool SRXLODWindow::RefreshPreview(FString& E)
{
    if (!Session->Rebuild(E)) { Error(E); return false; }
    SelectedLOD = FMath::Min(SelectedLOD, Session->Display->GetNumSourceModels());
    Viewport->SetMesh(Session->Display.Get(), SelectedLOD); RefreshPanel(); return true;
}
FReply SRXLODWindow::Rebuild() { FString E; RefreshPreview(E); return FReply::Handled(); }
FReply SRXLODWindow::Apply() { FString E; if (!Session->IsPreviewCurrent() && !RefreshPreview(E)) { return FReply::Handled(); } if (!Session->Apply(E)) { Error(E); } return FReply::Handled(); }
FReply SRXLODWindow::JsonFile(bool Load, bool SaveAs)
{
    auto* Desktop = FDesktopPlatformModule::Get(); TArray<FString> Files;
    if (!Desktop) { Error(TEXT("파일 대화상자를 사용할 수 없습니다.")); return FReply::Handled(); }
    FString Filename = JsonFilename;
    if (Load || SaveAs || Filename.IsEmpty())
    {
        const void* Parent = FSlateApplication::Get().FindBestParentWindowHandleForDialogs(AsShared());
        const bool Chosen = Load ? Desktop->OpenFileDialog(Parent, TEXT("RX LOD JSON 불러오기"), FPaths::ProjectSavedDir(), FString(), TEXT("JSON|*.json"), 0, Files)
            : Desktop->SaveFileDialog(Parent, TEXT("RX LOD JSON 저장"), FPaths::ProjectSavedDir(), TEXT("RX_LOD.json"), TEXT("JSON|*.json"), 0, Files);
        if (!Chosen || Files.IsEmpty()) { return FReply::Handled(); } Filename = Files[0];
    }
    FString E, Json;
    if (Load)
    {
        if (!FFileHelper::LoadFileToString(Json, *Filename) || !Session->ImportJson(Json, E)) { Error(E.IsEmpty() ? TEXT("JSON 파일을 읽지 못했습니다.") : E); return FReply::Handled(); }
        RefreshPanel();
    }
    else if (!FFileHelper::SaveStringToFile(Session->ExportJson(), *Filename, FFileHelper::EEncodingOptions::ForceUTF8WithoutBOM)) { Error(TEXT("JSON 저장에 실패했습니다.")); return FReply::Handled(); }
    else { Session->Status = TEXT("JSON 저장 완료: ") + Filename; }
    JsonFilename = Filename; return FReply::Handled();
}
