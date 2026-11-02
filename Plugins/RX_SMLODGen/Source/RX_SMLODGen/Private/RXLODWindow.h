#pragma once
#include "CoreMinimal.h"
#include "Widgets/SCompoundWidget.h"
#include "RXLODSession.h"
#include "Widgets/Input/SComboBox.h"

class SRXLODViewport;
class SVerticalBox;
class IStructureDetailsView;
class FJsonObject;
class SRXLODWindow : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SRXLODWindow) {} SLATE_ARGUMENT(TSharedPtr<FRXLODSession>, Session) SLATE_END_ARGS()
    void Construct(const FArguments& Args);
    void RefreshPanel();
    bool RefreshPreview(FString& Error);
    void SelectLOD(int32 LOD);
    TSharedRef<FJsonObject> ViewportDiagnostics() const;
private:
    TSharedPtr<FRXLODSession> Session;
    TSharedPtr<SRXLODViewport> Viewport;
    TSharedPtr<SVerticalBox> Panel;
    TSharedPtr<SComboBox<TSharedPtr<FString>>> ViewportLODSelector;
    TArray<TSharedPtr<IStructureDetailsView>> StructureViews;
    TArray<TSharedPtr<FString>> LODChoices, GroupChoices, MaterialChoices, CriterionChoices;
    TSet<int32> VisibleLODs;
    int32 SelectedLOD = 0;
    bool bCustom = false;
    FString JsonFilename;
    TMap<FString, bool> ExpandedAreas;
    int32 BuildingLOD = INDEX_NONE;
    TSharedRef<SWidget> Area(const FString& Label, TSharedRef<SWidget> Body, bool Expanded = true);
    TSharedRef<SWidget> LODPanel(int32 LOD);
    TSharedRef<SWidget> SectionsPanel(int32 LOD);
    TSharedRef<SWidget> CommonPanel();
    TSharedRef<SWidget> StructEditor(UScriptStruct* Type, const void* Data, TFunction<void(const void*)> Commit, bool bHidePercent = false);
    FReply Rebuild();
    FReply Apply();
    FReply JsonFile(bool bLoad, bool bSaveAs);
    void Error(const FString& Text);
    EActiveTimerReturnType RefreshPanelNextTick(double Time, float Delta) { RefreshPanel(); return EActiveTimerReturnType::Stop; }
};
