#include "EditorGuidance.h"
#include "GuideSequence.h"
#include "BlueprintEditor.h"
#include "BlueprintEditorTabs.h"
#include "ControlRigBlueprintLegacy.h"
#include "EdGraph/RigVMEdGraphNode.h"
#include "RigVMModel/RigVMNode.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMController.h"
#include "Editor/RigVMDetailsViewWrapperObject.h"
#include "SKismetInspector.h"
#include "Editor.h"
#include "Engine/Blueprint.h"
#include "Engine/SkeletalMesh.h"
#include "Animation/AnimInstance.h"
#include "UObject/UObjectIterator.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "GraphEditor.h"
#include "SGraphPanel.h"
#include "SGraphNode.h"
#include "SGraphPin.h"
#include "SMyBlueprint.h"
#include "EdGraphSchema_K2_Actions.h"
#include "K2Node_Event.h"
#include "UObject/PropertyWrapper.h"
#include "SSubobjectEditor.h"
#include "Framework/Docking/TabManager.h"
#include "Framework/Commands/InputBindingManager.h"
#include "Widgets/Docking/SDockTab.h"
#include "Types/ISlateMetaData.h"
#include "UObject/UnrealType.h"
#include "IDetailsView.h"
#include "PropertyPath.h"
#include "Toolkits/ToolkitManager.h"
#include "Toolkits/IToolkitHost.h"
#include "Framework/Application/SlateApplication.h"
#include "Layout/WidgetPath.h"
#include "Widgets/Views/STableRow.h"
#include "Widgets/SWindow.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/EngineVersion.h"
#include "Misc/CoreMisc.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "Widgets/SCanvas.h"
#include "Widgets/Layout/SBorder.h"
#include "Widgets/Input/SButton.h"
#include "Widgets/Text/STextBlock.h"
#include "Widgets/SBoxPanel.h"
#include "Rendering/DrawElements.h"
#include "Styling/AppStyle.h"
#include "Containers/Ticker.h"
#include "GenericPlatform/GenericWindow.h"
#include "HAL/PlatformProcess.h"

namespace Cubeless::EditorGuidance
{
namespace
{
class SGuideOverlay;
struct FTarget
{
    TWeakObjectPtr<UBlueprint> Blueprint;
    TWeakObjectPtr<UEdGraphNode> Node;
    TWeakObjectPtr<UEdGraph> Graph;
    TWeakObjectPtr<UObject> DetailsObject;
    TWeakObjectPtr<URigVMNode> RigNode;
    TWeakPtr<FBlueprintEditor> Editor;
    FString Property, Message, State = TEXT("pending_layout");
    FString Kind = TEXT("node"), OwnerKind = TEXT("node"), Anchor;
    FString Identity;
    TWeakFieldPtr<FProperty> VariableProperty;
    FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits), SourceAction, SourceArguments;
    FString Badge;
    bool Certified=false;
    FGuid PinId;
    bool Resolved = false;
    TWeakPtr<SWidget> Widget;
    TWeakPtr<SWidget> CapturedAnchor;
    bool AnchorCaptured = false;
    FSlateRect Bounds;
    FSlateRect ClippedBy;
    TWeakPtr<SWindow> Window;
    TSharedPtr<SGuideOverlay> Overlay;
    bool Closed = false, ScrollPending = true, Painted = false, SelectionPending = false;
    double LastPaint = 0;
    int32 LayoutDelay = 3;
    int32 RigSelectionAttempts = 0;
    FString Mode = TEXT("combined"), StepLabel;
};
TSharedPtr<FTarget> NodeTarget, PropertyTarget;
FTSTicker::FDelegateHandle TickHandle;
bool ClearRequested = false;
bool Hidden = false;

bool RigOwnerMatches(UObject* Object, URigVMNode* Node)
{
    auto Wrapper=Cast<URigVMDetailsViewWrapperObject>(Object);
    return Node && Wrapper && Wrapper->IsActive() && Wrapper->GetSubject()==Node;
}

bool RigNodeMatches(UEdGraphNode* Node, URigVMNode* Model)
{
    auto Rig=Cast<URigVMEdGraphNode>(Node);
    return Rig && Model && Rig->GetModelNode()==Model && Model->GetGraph() && Model->GetGraph()->GetNodes().Contains(Model);
}

FString Json(const TSharedRef<FJsonObject>& Value)
{
    FString Out; auto Writer = TJsonWriterFactory<>::Create(&Out);
    FJsonSerializer::Serialize(Value, Writer); return Out;
}

void ScopedRoots(const TSharedPtr<FBlueprintEditor>& Editor, UBlueprint* BP, TArray<TSharedRef<SWidget>>& Roots)
{
    // A standalone host belongs to this toolkit even when its major tab lives
    // in the level window. Never walk the shared world-centric host.
    if (Editor->IsHosted() && !Editor->IsWorldCentricAssetEditor()) Roots.Add(Editor->GetToolkitHost()->GetParentWidget());
    if (Editor->GetTabManager()) if (auto Tab = Editor->GetTabManager()->GetOwnerTab()) Roots.Add(Tab.ToSharedRef());
    if (auto Details = Editor->GetInspector()->GetPropertyView()) Roots.Add(Details.ToSharedRef());
    if (auto My = Editor->GetMyBlueprintWidget()) Roots.Add(My.ToSharedRef());
    if (auto Components = Editor->GetSubobjectEditor()) Roots.Add(Components.ToSharedRef());
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
    for (auto Graph : Graphs) if (auto Widget = SGraphEditor::FindGraphEditorForGraph(Graph)) Roots.Add(Widget.ToSharedRef());
}

void TaggedWidgets(const TSharedRef<SWidget>& Root, TMap<FName,TArray<TSharedRef<SWidget>>>& Found, TSet<const SWidget*>& Seen, int32& Budget)
{
    if (--Budget < 0 || Seen.Contains(&Root.Get()) || !Root->GetVisibility().IsVisible()) return;
    Seen.Add(&Root.Get());
    TSet<FName> Tags;
    if (!Root->GetTag().IsNone()) Tags.Add(Root->GetTag());
    if (auto Meta = Root->GetMetaData<FTagMetaData>()) if (!Meta->Tag.IsNone()) Tags.Add(Meta->Tag);
    for (auto Tag : Tags) Found.FindOrAdd(Tag).Add(Root);
    auto Children = Root->GetChildren();
    for (int32 I=0; Children && I<Children->Num(); ++I) TaggedWidgets(Children->GetChildAt(I),Found,Seen,Budget);
}

TArray<FSubobjectEditorTreeNodePtrType> ComponentNodes(const TSharedPtr<FBlueprintEditor>& Editor)
{
    TArray<FSubobjectEditorTreeNodePtrType> Out;
    if (auto Components = Editor->GetSubobjectEditor())
    {
        Out.Append(Components->GetRootNodes());
        for (int32 I=0; I<Out.Num() && I<2048; ++I) if (Out[I]) Out.Append(Out[I]->GetChildren());
    }
    return Out;
}

TFunction<bool()> CertificateLifetime(const FTarget& Target)
{
    return [BPWeak=Target.Blueprint,EditorWeak=Target.Editor,NodeWeak=Target.Node,GraphWeak=Target.Graph,OwnerWeak=Target.DetailsObject,
        HadNode=Target.Node.IsValid(),HadGraph=Target.Graph.IsValid(),HadOwner=Target.DetailsObject.IsValid(),
        Kind=Target.Kind,OwnerKind=Target.OwnerKind,Identity=Target.Identity,Anchor=Target.Anchor,AnchorWeak=Target.CapturedAnchor,
        FieldWeak=Target.VariableProperty,PinId=Target.PinId,RigWeak=Target.RigNode,HadRig=Target.RigNode.IsValid()]()
    {
        if (!IsInGameThread() || IsGarbageCollecting() || IsEngineExitRequested()) return false;
        auto BP=BPWeak.Get();auto Editor=EditorWeak.Pin();auto Node=NodeWeak.Get();auto Graph=GraphWeak.Get();
        // RigVM Details wrappers are pooled when selection moves to another
        // node. The certificate owns the weak semantic model identity; replay
        // must reacquire and validate its freshly selected active wrapper.
        if (!BP || !Editor || FToolkitManager::Get().FindEditorForAsset(BP).Get()!=Editor.Get() || (HadOwner && !HadRig && !OwnerWeak.IsValid())) return false;
        if (HadGraph) {TArray<UEdGraph*> Graphs;BP->GetAllGraphs(Graphs);if(!Graph || !Graphs.Contains(Graph))return false;}
        if (HadNode && (!Node || Node->GetGraph()!=Graph || !Graph->Nodes.Contains(Node))) return false;
        if (HadRig && !RigNodeMatches(Node,RigWeak.Get())) return false;
        if (Kind==TEXT("pin")) {int32 Matches=0;for(auto Pin:Node->Pins)if(Pin && Pin->PinId==PinId)++Matches;if(Matches!=1)return false;}
        if (OwnerKind==TEXT("class_defaults") && (!BP->GeneratedClass || BP->GeneratedClass->GetDefaultObject(false)!=OwnerWeak.Get())) return false;
        if (OwnerKind==TEXT("component"))
        {int32 Matches=0;for(auto TreeNode:ComponentNodes(Editor))if(TreeNode && TreeNode->IsComponentNode() && TreeNode->GetObject()==OwnerWeak.Get())++Matches;if(Matches!=1)return false;}
        if (OwnerKind==TEXT("variable"))
        {
            FGuid Id;FName Name;int32 Matches=0;if(FGuid::Parse(Identity,Id))for(const auto& Var:BP->NewVariables)if(Var.VarGuid==Id){Name=Var.VarName;++Matches;}
            auto Field=FieldWeak.Get();auto Wrapper=Cast<UPropertyWrapper>(OwnerWeak.Get());
            if(Matches!=1 || !Field || !Wrapper || Wrapper->GetProperty()!=Field || Field->GetFName()!=Name || (Field->GetOwnerClass()!=BP->GeneratedClass && Field->GetOwnerClass()!=BP->SkeletonGeneratedClass))return false;
        }
        if (Kind==TEXT("widget"))
        {
            TArray<TSharedRef<SWidget>> Roots;ScopedRoots(Editor,BP,Roots);TMap<FName,TArray<TSharedRef<SWidget>>> Found;TSet<const SWidget*> Seen;int32 Budget=20000;
            for(auto Root:Roots)TaggedWidgets(Root,Found,Seen,Budget);auto Matches=Found.Find(FName(*Anchor));
            if(Budget<0 || !Matches || Matches->Num()!=1 || !AnchorWeak.IsValid() || AnchorWeak.Pin()!=(*Matches)[0])return false;
        }
        return true;
    };
}

// UE 5.8.4 compatibility boundary. The installed SDetailSingleItemRow derives
// SDetailTableRowBase : STableRow<TSharedPtr<FDetailTreeNode>>. Only this exact
// Slate runtime type is admitted; no private headers, text matching, or coordinates.
// IDetailsView's public property->row-index mapping supplies semantic identity.
void FindRows(const TSharedRef<SWidget>& Root, int32 Index, TArray<TSharedRef<SWidget>>& Found, int32& Budget)
{
    if (--Budget < 0 || !Root->GetVisibility().IsVisible()) return;
    if (Root->GetType() == FName(TEXT("SDetailSingleItemRow")))
    {
        auto Row = StaticCastSharedRef<STableRow<TSharedPtr<FDetailTreeNode>>>(Root);
        if (Row->GetIndexInList() == Index) Found.Add(Root);
        return; // Do not match embedded child tables with their own index space.
    }
    FChildren* Children = Root->GetChildren();
    for (int32 I = 0; Children && I < Children->Num(); ++I) FindRows(Children->GetChildAt(I), Index, Found, Budget);
}

TSharedPtr<SWidget> FindDetailsSearch(const TSharedRef<SWidget>& Root, int32& Budget)
{
    if (--Budget < 0 || !Root->GetVisibility().IsVisible()) return nullptr;
    if (Root->GetType() == FName(TEXT("SSearchBox")) && Root->SupportsKeyboardFocus()) return Root;
    FChildren* Children = Root->GetChildren();
    for (int32 I = 0; Children && I < Children->Num(); ++I)
        if (auto Found = FindDetailsSearch(Children->GetChildAt(I), Budget)) return Found;
    return nullptr;
}

void ActivateDetails(const TSharedPtr<FBlueprintEditor>& Editor)
{
    // Activating a native window alone does not select Details when another
    // tab (for example Preview Settings) occupies the same dock stack.
    if (Editor->GetTabManager()) Editor->GetTabManager()->TryInvokeTab(FBlueprintEditorTabs::DetailsID);
    if (auto Tab=Editor->GetInspector()->GetOwnerTab()) Tab->ActivateInParent(ETabActivationCause::SetDirectly);
}

TSharedPtr<SWidget> ResolveWidget(FTarget& Target)
{
    Target.Resolved = false;
    auto Editor = Target.Editor.Pin(); auto BP = Target.Blueprint.Get(); auto Node = Target.Node.Get();
    if (!Editor || !BP || FToolkitManager::Get().FindEditorForAsset(BP).Get() != Editor.Get()
        || ((Target.Kind == TEXT("node") || Target.Kind == TEXT("pin") || Target.OwnerKind == TEXT("node")) && (!Node || Node->GetGraph() != Target.Graph.Get())))
    { Target.State = TEXT("target_closed"); return nullptr; }
    if (Target.Graph.IsValid())
    {
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs);
        if (!Graphs.Contains(Target.Graph.Get()) || (Node && !Target.Graph->Nodes.Contains(Node)))
        { Target.State=TEXT("target_closed"); return nullptr; }
    }
    if (Target.RigNode.IsValid() ? !RigNodeMatches(Node,Target.RigNode.Get()) : Cast<URigVMEdGraphNode>(Node)!=nullptr)
    { Target.State=TEXT("target_closed"); return nullptr; }
    if (Target.OwnerKind==TEXT("class_defaults") && (!BP->GeneratedClass || BP->GeneratedClass->GetDefaultObject(false)!=Target.DetailsObject.Get()))
    { Target.State=TEXT("owner_replaced"); return nullptr; }
    if (Target.OwnerKind==TEXT("variable"))
    {
        FGuid Id; FName Name; int32 Matches=0;
        if (FGuid::Parse(Target.Identity,Id)) for (const auto& Var : BP->NewVariables) if (Var.VarGuid==Id) { Name=Var.VarName; ++Matches; }
        auto Field=Target.VariableProperty.Get();
        if (Matches!=1 || !Field || Field->GetFName()!=Name || (Field->GetOwnerClass()!=BP->GeneratedClass && Field->GetOwnerClass()!=BP->SkeletonGeneratedClass))
        { Target.State=TEXT("variable_owner_replaced"); return nullptr; }
    }
    if (Target.SelectionPending)
    {
        if (!Target.DetailsObject.IsValid()) { Target.State=TEXT("target_closed"); return nullptr; }
        if (Target.LayoutDelay-- > 0) { Target.State=TEXT("pending_details_selection"); return nullptr; }
        auto Details=Editor->GetInspector()->GetPropertyView();
        if (!Details) { Target.State=TEXT("details_unavailable"); return nullptr; }
        if (Target.RigNode.IsValid())
        {
            // A document activation can leave Details on the previous graph
            // even when the model selection was restored. Settle this explicit
            // request once through RigVM's native inspector route, never by
            // passing its UEdGraph shim to the standard Blueprint inspector.
            if (Details->GetSelectedObjects().Num()!=1 || !RigOwnerMatches(Details->GetSelectedObjects()[0].Get(),Target.RigNode.Get()))
            {
                if(Target.RigSelectionAttempts==0 && Editor->GetToolkitFName()==FName(TEXT("ControlRigEditor")) && Cast<UControlRigBlueprint>(BP) && Editor->IsBlueprintEditor())
                {
                    ++Target.RigSelectionAttempts;
                    auto Rig=Cast<URigVMEdGraphNode>(Node);
                    auto Controller=Rig ? Rig->GetController() : nullptr;
                    if(!Controller || Controller->GetGraph()!=Target.RigNode->GetGraph())
                    {Target.State=TEXT("details_selection_changed");Target.SelectionPending=false;return nullptr;}
                    // Refresh an already restored selection without deselecting
                    // it: clearing also tears down Control Rig manipulation
                    // controls. Notify is the public native UI refresh route.
                    const auto Selected=Target.RigNode->GetGraph()->GetSelectNodes();
                    if(Selected.Num()==1 && Selected[0]==Target.RigNode->GetFName())
                        Controller->Notify(ERigVMGraphNotifType::NodeSelectionChanged,nullptr);
                    else Controller->SetNodeSelection(TArray<FName>{Target.RigNode->GetFName()},false,false);
                    Target.LayoutDelay=3;Target.State=TEXT("pending_details_selection");return nullptr;
                }
                Target.State=++Target.RigSelectionAttempts<12 ? TEXT("pending_details_selection") : TEXT("details_selection_changed");
                if(Target.RigSelectionAttempts>=12)Target.SelectionPending=false;
                return nullptr;
            }
            Target.DetailsObject=Details->GetSelectedObjects()[0];
            Target.SelectionPending=false;Target.LayoutDelay=3;
            Target.State=TEXT("pending_details_layout");return nullptr;
        }
        Target.SelectionPending=false;
        if (Details->GetSelectedObjects().Num()!=1 || Details->GetSelectedObjects()[0]!=Target.DetailsObject)
            Editor->GetInspector()->ShowDetailsForSingleObject(Target.DetailsObject.Get());
        Target.LayoutDelay=3; Target.State=TEXT("pending_details_layout"); return nullptr;
    }
    if (Target.RigNode.IsValid() && (Target.Kind==TEXT("property") || Target.Kind==TEXT("selection")) && !RigOwnerMatches(Target.DetailsObject.Get(),Target.RigNode.Get()))
    { Target.State=TEXT("details_selection_changed"); return nullptr; }
    if (Target.Property.IsEmpty())
    {
        if (Target.Kind == TEXT("panel") || Target.Kind == TEXT("variable") || Target.Kind == TEXT("selection"))
        {
            Target.Resolved = true;
            if (Target.Anchor == TEXT("details") || Target.Kind == TEXT("variable") || Target.Kind == TEXT("selection"))
            {
                auto Details = Editor->GetInspector()->GetPropertyView();
                if (Target.Kind != TEXT("panel") && (!Details || Details->GetSelectedObjects().Num()!=1 || Details->GetSelectedObjects()[0] != Target.DetailsObject))
                { Target.State = TEXT("details_selection_changed"); return nullptr; }
                return Details;
            }
            if (Target.Anchor == TEXT("my_blueprint")) return Editor->GetMyBlueprintWidget();
            if (Target.Anchor == TEXT("components")) return Editor->GetSubobjectEditor();
            Target.State = TEXT("panel_unavailable"); return nullptr;
        }
        if (Target.Kind == TEXT("component"))
        {
            auto Components = Editor->GetSubobjectEditor();
            if (Components) for (auto TreeNode : ComponentNodes(Editor))
                if (TreeNode && TreeNode->GetObject() == Target.DetailsObject.Get())
                {
                    auto Row = Components->GetDragDropTree()->WidgetFromItem(TreeNode);
                    Target.Resolved = true; if (Row) return Row->AsWidget(); return nullptr;
                }
            Target.State = TEXT("component_not_realized"); return nullptr;
        }
        if (Target.Kind == TEXT("widget"))
        {
            TArray<TSharedRef<SWidget>> Roots; ScopedRoots(Editor,BP,Roots);
            TMap<FName,TArray<TSharedRef<SWidget>>> Found; TSet<const SWidget*> Seen; int32 Budget=20000;
            for (auto Root : Roots) TaggedWidgets(Root,Found,Seen,Budget);
            auto Matches = Found.Find(FName(*Target.Anchor));
            if (Budget<0 || !Matches || Matches->Num()!=1) { Target.State=TEXT("widget_tag_missing_or_ambiguous"); return nullptr; }
            // A tag can be regenerated/reused by toolbar/menu builders. An
            // existing guide must never silently move to a replacement widget.
            if (Target.AnchorCaptured && Target.CapturedAnchor.Pin()!=(*Matches)[0])
            { Target.State=TEXT("widget_anchor_replaced_reinspect_required"); return nullptr; }
            Target.CapturedAnchor=(*Matches)[0]; Target.AnchorCaptured=true;
            Target.Resolved=true; return (*Matches)[0];
        }
        auto GraphEditor = Target.Graph.IsValid() ? SGraphEditor::FindGraphEditorForGraph(Target.Graph.Get()) : nullptr;
        if (!GraphEditor || !GraphEditor->GetGraphPanel()) { Target.State = TEXT("graph_not_visible"); return nullptr; }
        Target.Resolved = true;
        if (Target.Kind == TEXT("graph")) return GraphEditor;
        auto NodeWidget = GraphEditor->GetGraphPanel()->GetNodeWidgetFromGuid(Node->NodeGuid);
        if (Target.Kind == TEXT("pin") && NodeWidget)
        {
            UEdGraphPin* Pin=nullptr; int32 Matches=0;
            for (auto Candidate : Node->Pins) if (Candidate && Candidate->PinId==Target.PinId) { Pin=Candidate; ++Matches; }
            if (Matches!=1) { Target.State=TEXT("pin_missing_or_ambiguous"); return nullptr; }
            return NodeWidget->FindWidgetForPin(Pin);
        }
        return NodeWidget;
    }
    auto Details = Editor->GetInspector()->GetPropertyView();
    if (!Target.DetailsObject.IsValid() || !Details || Details->GetSelectedObjects().Num() != 1 || Details->GetSelectedObjects()[0] != Target.DetailsObject)
    { Target.State = TEXT("details_selection_changed"); return nullptr; }
    if (Target.ScrollPending)
    {
        if (Target.LayoutDelay-- > 0) { Target.State = TEXT("pending_details_layout"); return nullptr; }
        for (const auto& Path : Details->GetPropertiesInOrderDisplayed()) if (Path.ToString(TEXT(".")) == Target.Property)
        { Details->ScrollPropertyIntoView(Path, true); Target.ScrollPending = false; break; }
    }
    if (FEngineVersion::Current().GetMajor() != 5 || FEngineVersion::Current().GetMinor() != 8 || FEngineVersion::Current().GetPatch() != 4)
    { Target.State = TEXT("unsupported_details_engine_version"); return nullptr; }
    int32 RowIndex = INDEX_NONE, Matches = 0;
    for (const auto& Pair : Details->GetPropertyRowNumbers())
    {
        if (Pair.Value.ToString(TEXT(".")) == Target.Property) { RowIndex = Pair.Key; ++Matches; }
    }
    if (Matches != 1) { Target.State = Matches ? TEXT("ambiguous_property") : TEXT("property_hidden_or_unresolved"); return nullptr; }
    Target.Resolved = true;
    int32 Budget = 10000; TArray<TSharedRef<SWidget>> Found;
    FindRows(Details.ToSharedRef(), RowIndex, Found, Budget);
    if (Budget < 0 || Found.Num() != 1) { Target.State = TEXT("row_not_realized_or_unsupported"); return nullptr; }
    return Found[0];
}

void Refresh(FTarget& Target)
{
    Target.Widget.Reset();
    Target.ClippedBy = FSlateRect();
    auto Widget = ResolveWidget(Target);
    if (!Widget) return;
    FWidgetPath Path;
    if (!FSlateApplication::Get().GeneratePathToWidgetUnchecked(Widget.ToSharedRef(), Path))
    { Target.State = TEXT("not_visible"); return; }
    const FGeometry& Geometry = Widget->GetCachedGeometry();
    const FVector2D Size = Geometry.GetLocalSize();
    if (Size.X < 2 || Size.Y < 2) { Target.State = TEXT("pending_layout"); return; }
    Target.Bounds = Geometry.GetLayoutBoundingRect();
    // Fail closed when clipped by any ancestor; cached off-screen rows must not
    // leave a false pointer on another control after scroll/dock/zoom.
    for (int32 PathIndex = 0; PathIndex < Path.Widgets.Num(); ++PathIndex)
    {
        const auto& Arranged = Path.Widgets[PathIndex];
        if (Arranged.Widget == Widget) break;
        if (Arranged.Widget->GetClipping() == EWidgetClipping::Inherit) continue;
        const FSlateRect Clip = Arranged.Geometry.GetLayoutBoundingRect();
        if (Target.Bounds.Left < Clip.Left-.5f || Target.Bounds.Top < Clip.Top-.5f || Target.Bounds.Right > Clip.Right+.5f || Target.Bounds.Bottom > Clip.Bottom+.5f)
        { Target.ClippedBy = Clip; Target.State = TEXT("clipped"); return; }
    }
    Target.Widget = Widget;
    Target.State = TEXT("geometry_resolved");
}

bool Overlap(const FSlateRect& A, const FSlateRect& B)
{
    return A.Left < B.Right && A.Right > B.Left && A.Top < B.Bottom && A.Bottom > B.Top;
}

// Only buttons participate in hit testing. Borders, arrows, badges and explanatory
// text are passive; the graph and Details controls remain directly usable.
class SGuideOverlay final : public SCompoundWidget
{
public:
    SLATE_BEGIN_ARGS(SGuideOverlay) {} SLATE_END_ARGS()
    void Construct(const FArguments&, TSharedRef<FTarget> InTarget)
    {
        Target = InTarget;
        const bool IsProperty = !InTarget->Property.IsEmpty();
        Color = IsProperty ? FLinearColor(1.f, .65f, .02f) : FLinearColor(0.f, .8f, 1.f);
        Number = InTarget->Badge.IsEmpty() ? (IsProperty ? TEXT("2") : TEXT("1")) : InTarget->Badge;
        const FString Title = InTarget->StepLabel.IsEmpty() ? (IsProperty ? InTarget->Property : InTarget->Node.IsValid() ? InTarget->Node->GetNodeTitle(ENodeTitleType::ListView).ToString() : InTarget->Kind) : InTarget->StepLabel;
        SetVisibility(EVisibility::SelfHitTestInvisible);
        ChildSlot
        [
            SAssignNew(Canvas, SCanvas).Visibility(EVisibility::SelfHitTestInvisible)
            + SCanvas::Slot().Position_Lambda([this] { return CardPosition; }).Size_Lambda([this] { return CardSize; })
            [
                SAssignNew(Card, SBorder).Visibility(EVisibility::Collapsed)
                .BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(Color).Padding(2)
                [
                    SNew(SBorder).Visibility(EVisibility::SelfHitTestInvisible)
                    .BorderImage(FAppStyle::GetBrush("WhiteBrush")).BorderBackgroundColor(FLinearColor(.015f,.025f,.035f,.97f)).Padding(12)
                    [
                        SNew(SVerticalBox).Visibility(EVisibility::SelfHitTestInvisible)
                        + SVerticalBox::Slot().AutoHeight()
                        [
                            SNew(SHorizontalBox).Visibility(EVisibility::SelfHitTestInvisible)
                            + SHorizontalBox::Slot().FillWidth(1)
                            [ SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Text(FText::FromString(Number + TEXT("  ") + Title)).ColorAndOpacity(Color).Font(FAppStyle::GetFontStyle("BoldFont")).WrapTextAt(235) ]
                            + SHorizontalBox::Slot().AutoWidth()
                            [ SNew(SButton).Text(FText::FromString(TEXT("×"))).ToolTipText(FText::FromString(TEXT("이 안내 닫기"))).OnClicked_Lambda([Weak = Target] { if (auto P = Weak.Pin()) P->Closed = true; return FReply::Handled(); }) ]
                        ]
                        + SVerticalBox::Slot().FillHeight(1).Padding(0,8,0,0)
                        [ SNew(STextBlock).Visibility(EVisibility::HitTestInvisible).Text(FText::FromString(InTarget->Message.IsEmpty() ? TEXT("강조된 항목을 확인하세요.") : InTarget->Message)).WrapTextAt(268) ]
                        + SVerticalBox::Slot().AutoHeight().HAlign(HAlign_Right)
                        [ SNew(SButton).Text(FText::FromString(TEXT("안내 모두 숨기기"))).OnClicked_Lambda([] { Hidden = true; return FReply::Handled(); }) ]
                    ]
                ]
            ]
        ];
    }
    virtual void Tick(const FGeometry& Geometry, double Time, float Delta) override
    {
        SCompoundWidget::Tick(Geometry, Time, Delta);
        Ready = false; Card->SetVisibility(EVisibility::Collapsed);
        auto P = Target.Pin(); if (!P || P->Closed || !P->Widget.IsValid()) return;
        Card->SetVisibility(EVisibility::SelfHitTestInvisible);
        Card->SlatePrepass(Geometry.Scale);
        CardSize.Y = FMath::Max(110.0, (double)Card->GetDesiredSize().Y);
        Card->SetVisibility(EVisibility::Collapsed);
        const FVector2D A = Geometry.AbsoluteToLocal(FVector2D(P->Bounds.Left, P->Bounds.Top));
        const FVector2D B = Geometry.AbsoluteToLocal(FVector2D(P->Bounds.Right, P->Bounds.Bottom));
        LocalBounds = FSlateRect(A.X-4, A.Y-4, B.X+4, B.Y+4);
        if (P->Mode == TEXT("outline")) { Ready = true; return; }
        const FVector2D Size = Geometry.GetLocalSize();
        // Standalone Details rows can span the window from x=0. Keep the
        // above/below cards inside horizontal margins without moving the target.
        const double CardX = FMath::Clamp(A.X, 12.0, FMath::Max(12.0, Size.X-CardSize.X-12.0));
        const FVector2D Candidates[] = {FVector2D(A.X-324,A.Y), FVector2D(B.X+24,A.Y), FVector2D(CardX,A.Y-CardSize.Y-24), FVector2D(CardX,B.Y+24)};
        for (const auto& Candidate : Candidates)
        {
            const FSlateRect R(Candidate.X, Candidate.Y, Candidate.X+CardSize.X, Candidate.Y+CardSize.Y);
            if (R.Left < 12 || R.Top < 100 || R.Right > Size.X-12 || R.Bottom > Size.Y-12 || Overlap(R, LocalBounds)) continue;
            auto Other = P == NodeTarget ? PropertyTarget : NodeTarget;
            if (Other && Other->Widget.IsValid())
            {
                auto C = Geometry.AbsoluteToLocal(FVector2D(Other->Bounds.Left, Other->Bounds.Top));
                auto D = Geometry.AbsoluteToLocal(FVector2D(Other->Bounds.Right, Other->Bounds.Bottom));
                if (Overlap(R, FSlateRect(C.X,C.Y,D.X,D.Y))) continue;
                // Cyan wins placement priority; yellow avoids it. Symmetric
                // avoidance would let the two cards chase each other every frame.
                if (P == PropertyTarget && Other->Overlay && Other->Overlay->Ready && Overlap(R, FSlateRect(Other->Overlay->CardPosition.X,Other->Overlay->CardPosition.Y,Other->Overlay->CardPosition.X+Other->Overlay->CardSize.X,Other->Overlay->CardPosition.Y+Other->Overlay->CardSize.Y))) continue;
            }
            CardPosition = Candidate; Ready = true; break;
        }
        P->State = Ready ? TEXT("layout_ready") : TEXT("no_safe_card_space");
        if (Ready && P->Mode != TEXT("arrow")) Card->SetVisibility(EVisibility::SelfHitTestInvisible);
    }
    virtual int32 OnPaint(const FPaintArgs& Args, const FGeometry& Geometry, const FSlateRect& Cull, FSlateWindowElementList& Draw, int32 Layer, const FWidgetStyle& Style, bool Enabled) const override
    {
        auto P = Target.Pin();
        if (Ready && P && !P->Closed && P->Widget.IsValid())
        {
            TArray<FVector2D> Border = {{LocalBounds.Left,LocalBounds.Top},{LocalBounds.Right,LocalBounds.Top},{LocalBounds.Right,LocalBounds.Bottom},{LocalBounds.Left,LocalBounds.Bottom},{LocalBounds.Left,LocalBounds.Top}};
            if (P->Mode == TEXT("combined") || P->Mode == TEXT("outline"))
                FSlateDrawElement::MakeLines(Draw,Layer,Geometry.ToPaintGeometry(),Border,ESlateDrawEffect::None,Color,true,3);
            const FVector2D Center((LocalBounds.Left+LocalBounds.Right)*.5f,(LocalBounds.Top+LocalBounds.Bottom)*.5f);
            const FVector2D CardCenter = CardPosition + CardSize*.5;
            FVector2D Direction = (Center-CardCenter).GetSafeNormal();
            FVector2D Tip(FMath::Clamp(CardCenter.X, (double)LocalBounds.Left,(double)LocalBounds.Right), FMath::Clamp(CardCenter.Y,(double)LocalBounds.Top,(double)LocalBounds.Bottom));
            FVector2D Start(FMath::Clamp(Center.X,CardPosition.X,CardPosition.X+CardSize.X),FMath::Clamp(Center.Y,CardPosition.Y,CardPosition.Y+CardSize.Y));
            TArray<FVector2D> Line = {Start,Tip};
            if (P->Mode == TEXT("combined") || P->Mode == TEXT("arrow"))
                FSlateDrawElement::MakeLines(Draw,Layer+1,Geometry.ToPaintGeometry(),Line,ESlateDrawEffect::None,Color,true,3);
            const FVector2D Side(-Direction.Y,Direction.X);
            TArray<FVector2D> Arrow = {Tip-Direction*13+Side*7,Tip,Tip-Direction*13-Side*7};
            if (P->Mode == TEXT("combined") || P->Mode == TEXT("arrow"))
                FSlateDrawElement::MakeLines(Draw,Layer+1,Geometry.ToPaintGeometry(),Arrow,ESlateDrawEffect::None,Color,true,3);
            if (P->Mode == TEXT("combined") || P->Mode == TEXT("outline"))
                FSlateDrawElement::MakeText(Draw,Layer+2,Geometry.ToPaintGeometry(FVector2D(24,24),FSlateLayoutTransform(FVector2D(LocalBounds.Left,LocalBounds.Top-24))),Number,FAppStyle::GetFontStyle("BoldFont"),ESlateDrawEffect::None,Color);
            P->Painted = true; P->LastPaint = FPlatformTime::Seconds(); P->State = TEXT("displayed");
            if (!P->Certified) P->Certified=Sequence::Certify(P->Token,P->SourceAction,P->SourceArguments,P->Window,CertificateLifetime(*P));
            Sequence::ObservePaint(P->Token,P->Window,P->Bounds);
        }
        return SCompoundWidget::OnPaint(Args,Geometry,Cull,Draw,Layer+3,Style,Enabled);
    }
private:
    TWeakPtr<FTarget> Target;
    TSharedPtr<SCanvas> Canvas; TSharedPtr<SBorder> Card;
    FVector2D CardPosition, CardSize = FVector2D(300,144); FSlateRect LocalBounds;
    FLinearColor Color; FString Number; bool Ready = false;
};

void RemoveOverlay(FTarget& Target)
{
    if (Target.Overlay) if (auto Window = Target.Window.Pin()) Window->RemoveOverlaySlot(Target.Overlay.ToSharedRef());
    Target.Overlay.Reset(); Target.Window.Reset(); Target.Painted = false;
}

bool Tick(float)
{
    if (ClearRequested || IsEngineExitRequested()) { Shutdown(); return false; }
    if (!IsInGameThread() || !GEditor || IsGarbageCollecting()) return true;
    for (auto* Slot : {&NodeTarget, &PropertyTarget})
    {
        auto P = *Slot; if (!P) continue;
        if (GEditor->PlayWorld)
        { P->State = TEXT("play_session"); RemoveOverlay(*P); continue; }
        if (Hidden && !P->Closed && P->Blueprint.IsValid() && P->Editor.IsValid()
            && FToolkitManager::Get().FindEditorForAsset(P->Blueprint.Get()).Get() == P->Editor.Pin().Get())
        { P->State = TEXT("hidden"); RemoveOverlay(*P); continue; }
        Refresh(*P);
        if (P->Closed || P->State == TEXT("target_closed")) { RemoveOverlay(*P); Slot->Reset(); continue; }
        auto Widget = P->Widget.Pin();
        auto Window = Widget ? FSlateApplication::Get().FindWidgetWindow(Widget.ToSharedRef()) : nullptr;
        if (!Window || Window->IsWindowMinimized() || GEditor->PlayWorld)
        { RemoveOverlay(*P); continue; }
        if (Hidden || FSlateApplication::Get().AnyMenusVisible() || FSlateApplication::Get().GetActiveModalWindow().IsValid())
        { P->State = Hidden ? TEXT("hidden") : TEXT("occluded_by_menu_or_modal"); RemoveOverlay(*P); continue; }
        const auto ActiveWindow = FSlateApplication::Get().GetActiveTopLevelWindow();
        if (ActiveWindow.IsValid() && ActiveWindow != Window && !Sequence::OwnsActiveWindowFor(Window,ActiveWindow))
        { P->State = TEXT("inactive_window"); RemoveOverlay(*P); continue; }
        const FVector2D Center((P->Bounds.Left+P->Bounds.Right)*.5f,(P->Bounds.Top+P->Bounds.Bottom)*.5f);
        TArray<TSharedRef<SWindow>> Windows; Windows.Add(Window.ToSharedRef());
        const FWidgetPath Hit = FSlateApplication::Get().LocateWindowUnderMouse(Center,Windows,true);
        if (!Hit.IsValid() || !Hit.ContainsWidget(Widget.Get()))
        { P->State = TEXT("occluded_or_not_hit_testable"); RemoveOverlay(*P); continue; }
        if (P->Window.Pin() != Window) RemoveOverlay(*P);
        if (!P->Overlay)
        {
            P->Window = Window; P->Overlay = SNew(SGuideOverlay,P.ToSharedRef());
            Window->AddOverlaySlot(1000)[P->Overlay.ToSharedRef()];
        }
    }
    if (!NodeTarget && !PropertyTarget)
    { TickHandle.Reset(); return false; }
    return true;
}

TSharedRef<FJsonObject> Snapshot(FTarget& Target)
{
    auto Out = MakeShared<FJsonObject>();
    Out->SetStringField(TEXT("state"), Target.State);
    Out->SetBoolField(TEXT("target_resolved"), Target.Resolved);
    if(auto Model=Target.RigNode.Get())
    {
        Out->SetStringField(TEXT("rigvm_node_path"),Model->GetNodePath());
        Out->SetStringField(TEXT("rigvm_model_object_path"),Model->GetPathName());
    }
    Out->SetBoolField(TEXT("displayed"), Target.State == TEXT("displayed") && Target.Painted && FPlatformTime::Seconds()-Target.LastPaint < .5);
    Out->SetStringField(TEXT("property_path"), Target.Property);
    Out->SetStringField(TEXT("mode"), Target.Mode);
    Out->SetStringField(TEXT("step_label"), Target.StepLabel);
    Out->SetStringField(TEXT("target_kind"), Target.Kind);
    Out->SetStringField(TEXT("owner_kind"), Target.OwnerKind);
    Out->SetStringField(TEXT("semantic_identity"), Target.Identity);
    Out->SetStringField(TEXT("target_token"),Target.Token);
    Out->SetStringField(TEXT("badge"),Target.Badge);
    Out->SetStringField(TEXT("verification_token"),Sequence::HasCertificate(Target.Token) ? Target.Token : TEXT(""));
    Out->SetStringField(TEXT("owner_object_path"), Target.DetailsObject.IsValid() ? Target.DetailsObject->GetPathName() : TEXT(""));
    Out->SetNumberField(TEXT("process_id"), FPlatformProcess::GetCurrentProcessId());
    Out->SetBoolField(TEXT("application_foreground"), FSlateApplication::Get().IsActive());
    Out->SetStringField(TEXT("display_scope"), TEXT("painted_in_ue_window; external_OS_occlusion_not_proven"));
    if (auto Window = Target.Window.Pin())
    {
        Out->SetStringField(TEXT("window_title"), Window->GetTitle().ToString());
        if (auto Native = Window->GetNativeWindow()) Out->SetStringField(TEXT("native_window_id"), LexToString(reinterpret_cast<UPTRINT>(Native->GetOSWindowHandle())));
    }
    Out->SetStringField(TEXT("node_id"), Target.Node.IsValid() ? Target.Node->NodeGuid.ToString() : TEXT(""));
    if (Target.Widget.IsValid())
    {
        TArray<TSharedPtr<FJsonValue>> Rect;
        for (float N : {Target.Bounds.Left, Target.Bounds.Top, Target.Bounds.Right, Target.Bounds.Bottom}) Rect.Add(MakeShared<FJsonValueNumber>(N));
        Out->SetArrayField(TEXT("screen_bounds"), Rect);
    }
    if (Target.State == TEXT("clipped"))
    {
        TArray<TSharedPtr<FJsonValue>> Rect, Clip;
        for (float N : {Target.Bounds.Left,Target.Bounds.Top,Target.Bounds.Right,Target.Bounds.Bottom}) Rect.Add(MakeShared<FJsonValueNumber>(N));
        for (float N : {Target.ClippedBy.Left,Target.ClippedBy.Top,Target.ClippedBy.Right,Target.ClippedBy.Bottom}) Clip.Add(MakeShared<FJsonValueNumber>(N));
        Out->SetArrayField(TEXT("target_bounds_when_clipped"),Rect); Out->SetArrayField(TEXT("clipping_bounds"),Clip);
    }
    return Out;
}
}

FString Request(const FString& Action, const FString& Arguments)
{
    auto Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("success"), false);
    auto Fail = [&](const TCHAR* Error) { Out->SetStringField(TEXT("error"), Error); return Json(Out); };
    if (!IsInGameThread() || !FSlateApplication::IsInitialized() || IsEngineExitRequested() || IsGarbageCollecting() || !GEditor)
        return Fail(TEXT("idle_editor_game_thread_required"));
    // Clearing/hiding and reading status do not select, scroll, or change UObjects.
    // Keep lifetime guards above, but allow these cleanup operations during PIE.
    if (GEditor->PlayWorld && Action != TEXT("clear") && Action != TEXT("hide") && Action != TEXT("status") && Action!=TEXT("sequence_status") && Action!=TEXT("sequence_end") && Action!=TEXT("sequence_pause"))
        return Fail(TEXT("idle_editor_game_thread_required"));
    if (Action.StartsWith(TEXT("sequence_"))) return Sequence::Request(Action,Arguments);
    if (Action == TEXT("clear")) { Shutdown(); Out->SetBoolField(TEXT("success"), true); return Json(Out); }
    if (Action==TEXT("clear_slots"))
    {
        if(NodeTarget)RemoveOverlay(*NodeTarget);if(PropertyTarget)RemoveOverlay(*PropertyTarget);
        NodeTarget.Reset();PropertyTarget.Reset();Hidden=false;Out->SetBoolField(TEXT("success"),true);return Json(Out);
    }
    if (Action == TEXT("hide") || Action == TEXT("resume"))
    {
        Hidden = Action == TEXT("hide");
        if (Hidden) { if (NodeTarget) RemoveOverlay(*NodeTarget); if (PropertyTarget) RemoveOverlay(*PropertyTarget); }
        Out->SetBoolField(TEXT("success"), true); Out->SetBoolField(TEXT("hidden"), Hidden); return Json(Out);
    }
    if (Action == TEXT("status"))
    {
        if (NodeTarget) Out->SetObjectField(TEXT("node"), Snapshot(*NodeTarget));
        if (PropertyTarget) Out->SetObjectField(TEXT("property"), Snapshot(*PropertyTarget));
        Out->SetBoolField(TEXT("success"), true); return Json(Out);
    }
    if (Action != TEXT("inspect") && Action != TEXT("show_node") && Action != TEXT("show_property") && Action != TEXT("show_target")) return Fail(TEXT("unknown_action"));
    TSharedPtr<FJsonObject> Input;
    if (Arguments.Len() > 16384 || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Arguments), Input) || !Input) return Fail(TEXT("invalid_arguments"));
    FString AssetPath, GraphId, NodeId, Property, Message, Mode = TEXT("combined"), StepLabel;
    double StepNumber=0;
    if(Input->HasField(TEXT("guide_step_number")) && (!Input->TryGetNumberField(TEXT("guide_step_number"),StepNumber) || StepNumber<1 || StepNumber>32 || StepNumber!=FMath::FloorToDouble(StepNumber))) return Fail(TEXT("invalid_guide_step_number"));
    if (!Input->TryGetStringField(TEXT("asset_path"), AssetPath)) return Fail(TEXT("asset_path_required"));
    Input->TryGetStringField(TEXT("property_path"), Property); Input->TryGetStringField(TEXT("message"), Message);
    Input->TryGetStringField(TEXT("mode"), Mode); Input->TryGetStringField(TEXT("step_label"), StepLabel);
    if ((Mode != TEXT("combined") && Mode != TEXT("outline") && Mode != TEXT("arrow") && Mode != TEXT("card")) || StepLabel.Len() > 60) return Fail(TEXT("invalid_mode_or_step_label"));
    if (Message.Len() > 200 || (Action == TEXT("show_property") && Property.IsEmpty())) return Fail(TEXT("invalid_property_or_message"));
    auto BP = FindObject<UBlueprint>(nullptr, *AssetPath);
    if (!BP) return Fail(TEXT("loaded_blueprint_required"));
    auto Toolkit = FToolkitManager::Get().FindEditorForAsset(BP);
    // ControlRig's public legacy editor derives from FBlueprintEditor. The
    // toolkit name alone is insufficient: the new RigVM editor is unrelated.
    // IsBlueprintEditor plus the native asset class gate the installed 5.8
    // legacy path; non-Blueprint RigVM editor assets remain explicitly refused.
    const bool IsRig=Toolkit && Toolkit->GetToolkitFName()==FName(TEXT("ControlRigEditor")) && Cast<UControlRigBlueprint>(BP) && Toolkit->IsBlueprintEditor();
    if (!Toolkit || (!IsRig && Toolkit->GetToolkitFName() != FName(TEXT("BlueprintEditor")) && Toolkit->GetToolkitFName() != FName(TEXT("AnimationBlueprintEditor")))) return Fail(TEXT("open_supported_blueprint_editor_required"));
    auto Editor = StaticCastSharedPtr<FBlueprintEditor>(Toolkit);
    if (Action == TEXT("inspect"))
    {
        Out->SetStringField(TEXT("editor_type"),IsRig ? TEXT("control_rig_legacy") : TEXT("blueprint"));
        Out->SetStringField(TEXT("asset_class"),BP->GetClass()->GetPathName());
        TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); TArray<TSharedPtr<FJsonValue>> Items, GraphItems;
        for (auto Graph : Graphs) if (Graph)
        {
            auto G=MakeShared<FJsonObject>(); G->SetStringField(TEXT("graph_id"),Graph->GraphGuid.ToString()); G->SetStringField(TEXT("name"),Graph->GetName());
            G->SetStringField(TEXT("graph_role"),BP->FunctionGraphs.Contains(Graph) ? TEXT("function_or_animation_layer") : BP->MacroGraphs.Contains(Graph) ? TEXT("macro") : BP->UbergraphPages.Contains(Graph) ? TEXT("event") : BP->DelegateSignatureGraphs.Contains(Graph) ? TEXT("delegate_signature") : TEXT("nested_or_specialized"));
            G->SetStringField(TEXT("object_path"),Graph->GetPathName()); G->SetStringField(TEXT("schema_class"),Graph->Schema ? Graph->Schema->GetPathName() : TEXT(""));
            GraphItems.Add(MakeShared<FJsonValueObject>(G));
            for (auto Node : Graph->Nodes) if (Node)
            {
            auto Item = MakeShared<FJsonObject>();
            Item->SetStringField(TEXT("graph_id"), Graph->GraphGuid.ToString());
            Item->SetStringField(TEXT("graph_name"), Graph->GetName());
            Item->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
            Item->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::ListView).ToString());
            Item->SetStringField(TEXT("class"),Node->GetClass()->GetPathName());
            Item->SetStringField(TEXT("object_path"),Node->GetPathName());
            if(auto Rig=Cast<URigVMEdGraphNode>(Node)) if(auto Model=Rig->GetModelNode())
            {
                Item->SetStringField(TEXT("rigvm_node_path"),Model->GetNodePath());
                Item->SetStringField(TEXT("rigvm_model_object_path"),Model->GetPathName());
            }
            if (auto Event=Cast<UK2Node_Event>(Node))
            {
                Item->SetStringField(TEXT("event_member"),Event->EventReference.GetMemberName().ToString());
                auto Parent=Event->EventReference.GetMemberParentClass();
                Item->SetStringField(TEXT("event_parent_class"),Parent ? Parent->GetPathName() : TEXT(""));
            }
            TArray<TSharedPtr<FJsonValue>> Pins;
            for (auto Pin : Node->Pins) if (Pin)
            {
                auto P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("pin_id"),Pin->PinId.ToString()); P->SetStringField(TEXT("name"),Pin->PinName.ToString());
                P->SetStringField(TEXT("direction"),Pin->Direction==EGPD_Input ? TEXT("input") : TEXT("output")); P->SetStringField(TEXT("category"),Pin->PinType.PinCategory.ToString());
                P->SetBoolField(TEXT("hidden"),Pin->bHidden); Pins.Add(MakeShared<FJsonValueObject>(P));
                if(auto Rig=Cast<URigVMEdGraphNode>(Node)) if(auto Model=Rig->FindModelPinFromGraphPin(Pin)) P->SetStringField(TEXT("rigvm_pin_path"),Model->GetPinPath());
            }
            Item->SetArrayField(TEXT("pins"),Pins);
            Items.Add(MakeShared<FJsonValueObject>(Item));
            if (Items.Num() > 4096) return Fail(TEXT("graph_inventory_budget_exceeded"));
            }
        }
        Out->SetArrayField(TEXT("graphs"),GraphItems);
        Out->SetArrayField(TEXT("nodes"), Items); TArray<TSharedPtr<FJsonValue>> Properties;
        auto Details = Editor->GetInspector()->GetPropertyView();
        if (Details) for (const auto& Path : Details->GetPropertiesInOrderDisplayed()) Properties.Add(MakeShared<FJsonValueString>(Path.ToString(TEXT("."))));
        Out->SetArrayField(TEXT("displayed_property_paths"), Properties);
        TArray<TSharedPtr<FJsonValue>> Selected;
        if (Details) for (auto Object : Details->GetSelectedObjects()) if (Object.IsValid()) Selected.Add(MakeShared<FJsonValueString>(Object->GetPathName()));
        Out->SetArrayField(TEXT("details_objects"),Selected);
        TArray<TSharedPtr<FJsonValue>> RigSubjects;
        if(IsRig && Details)for(auto Object:Details->GetSelectedObjects())if(auto Wrapper=Cast<URigVMDetailsViewWrapperObject>(Object.Get()))
        {
            auto Subject=MakeShared<FJsonObject>();Subject->SetStringField(TEXT("wrapper_object_path"),Wrapper->GetPathName());
            Subject->SetBoolField(TEXT("active"),Wrapper->IsActive());
            if(auto Model=Cast<URigVMNode>(Wrapper->GetSubject()))Subject->SetStringField(TEXT("rigvm_node_path"),Model->GetNodePath());
            RigSubjects.Add(MakeShared<FJsonValueObject>(Subject));
        }
        Out->SetArrayField(TEXT("details_rigvm_subjects"),RigSubjects);
        // Readback is independent of focus/paint evidence. Only direct scalar
        // reflected fields of the actual current selection are read here;
        // nested/container/custom rows are not evaluated or guessed.
        TArray<TSharedPtr<FJsonValue>> Values;
        if (Details && Details->GetSelectedObjects().Num()==1)
        {
            auto Object=Details->GetSelectedObjects()[0].Get();
            if (Object) for (const auto& Path : Details->GetPropertiesInOrderDisplayed())
            {
                if (Values.Num()>=512) break;
                if (Path.GetNumProperties()!=1) continue;
                auto PropertyField=Path.GetRootProperty().Property.Get();
                if (!PropertyField || PropertyField->ArrayDim!=1 || !PropertyField->GetOwnerClass() || !Object->IsA(PropertyField->GetOwnerClass())) continue;
                if (!CastField<FBoolProperty>(PropertyField) && !CastField<FNumericProperty>(PropertyField) && !CastField<FEnumProperty>(PropertyField)
                    && !CastField<FNameProperty>(PropertyField) && !CastField<FStrProperty>(PropertyField) && !CastField<FTextProperty>(PropertyField)) continue;
                FString Value; PropertyField->ExportTextItem_Direct(Value,PropertyField->ContainerPtrToValuePtr<void>(Object),nullptr,Object,PPF_None);
                auto Item=MakeShared<FJsonObject>(); Item->SetStringField(TEXT("property_path"),Path.ToString(TEXT(".")));
                Item->SetStringField(TEXT("owner_object_path"),Object->GetPathName()); Item->SetStringField(TEXT("value_text"),Value.Left(2048));
                Item->SetBoolField(TEXT("truncated"),Value.Len()>2048); Values.Add(MakeShared<FJsonValueObject>(Item));
            }
        }
        Out->SetArrayField(TEXT("current_scalar_property_values"),Values);
        Out->SetStringField(TEXT("value_read_scope"),TEXT("direct_scalar_reflection_only; separate_from_UI_display_evidence"));
        Out->SetStringField(TEXT("class_settings_object"),BP->GetPathName());
        auto CDO=BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
        Out->SetStringField(TEXT("existing_cdo"),CDO ? CDO->GetPathName() : TEXT(""));
        TArray<TSharedPtr<FJsonValue>> Variables;
        for (const auto& Var : BP->NewVariables)
        {
            auto V=MakeShared<FJsonObject>(); V->SetStringField(TEXT("variable_id"),Var.VarGuid.ToString()); V->SetStringField(TEXT("name"),Var.VarName.ToString());
            Variables.Add(MakeShared<FJsonValueObject>(V));
        }
        Out->SetArrayField(TEXT("variables"),Variables);
        TArray<TSharedPtr<FJsonValue>> Interfaces;
        for (const auto& Interface : BP->ImplementedInterfaces)
        {
            auto I=MakeShared<FJsonObject>(); I->SetStringField(TEXT("interface_class"),Interface.Interface ? Interface.Interface->GetPathName() : TEXT(""));
            TArray<TSharedPtr<FJsonValue>> IDs; for (auto Graph : Interface.Graphs) if (Graph) IDs.Add(MakeShared<FJsonValueString>(Graph->GraphGuid.ToString()));
            I->SetArrayField(TEXT("implemented_graph_ids"),IDs); Interfaces.Add(MakeShared<FJsonValueObject>(I));
        }
        Out->SetArrayField(TEXT("interfaces"),Interfaces);
        TArray<TSharedPtr<FJsonValue>> Components;
        for (auto TreeNode : ComponentNodes(Editor)) if (TreeNode && TreeNode->IsComponentNode() && TreeNode->GetObject())
        {
            auto C=MakeShared<FJsonObject>(); C->SetStringField(TEXT("object_path"),TreeNode->GetObject()->GetPathName());
            C->SetStringField(TEXT("variable_name"),TreeNode->GetVariableName().ToString()); Components.Add(MakeShared<FJsonValueObject>(C));
        }
        Out->SetArrayField(TEXT("components"),Components);
        TArray<TSharedRef<SWidget>> Roots; ScopedRoots(Editor,BP,Roots);
        TMap<FName,TArray<TSharedRef<SWidget>>> Tags; TSet<const SWidget*> Seen; int32 Budget=20000;
        for (auto Root : Roots) TaggedWidgets(Root,Tags,Seen,Budget);
        TArray<TSharedPtr<FJsonValue>> Anchors;
        for (const auto& Pair : Tags)
        {
            auto A=MakeShared<FJsonObject>(); A->SetStringField(TEXT("tag"),Pair.Key.ToString()); A->SetNumberField(TEXT("matches"),Pair.Value.Num());
            A->SetStringField(TEXT("widget_type"),Pair.Value.Num()==1 ? Pair.Value[0]->GetTypeAsString() : TEXT("ambiguous"));
            Anchors.Add(MakeShared<FJsonValueObject>(A));
        }
        Out->SetArrayField(TEXT("widget_tags"),Anchors); Out->SetBoolField(TEXT("widget_inventory_complete"),Budget>=0);
        TArray<TSharedPtr<FJsonValue>> Meshes;
        for (TObjectIterator<USkeletalMesh> It; It; ++It) if (!It->HasAnyFlags(RF_ClassDefaultObject))
        {
            auto Class=It->GetPostProcessAnimBlueprint();
            if (Class && Class->ClassGeneratedBy==BP) Meshes.Add(MakeShared<FJsonValueString>(It->GetPathName()));
        }
        Out->SetArrayField(TEXT("loaded_post_process_mesh_assignments"),Meshes);
        Out->SetBoolField(TEXT("success"), true); return Json(Out);
    }
    // Select the owning major tab as well as its document. Multiple Blueprint
    // toolkits can share one native window; a live graph is not necessarily in
    // the currently visible toolkit. This is an explicit request, never a tick.
    Editor->FocusWindow(BP);
    if (Action == TEXT("show_target"))
    {
        FString Kind, OwnerKind=TEXT("node"), Identity, Anchor;
        if (!Input->TryGetStringField(TEXT("target_kind"),Kind)) return Fail(TEXT("target_kind_required"));
        if (Kind==TEXT("property") && Property.IsEmpty()) return Fail(TEXT("property_path_required"));
        Input->TryGetStringField(TEXT("owner_kind"),OwnerKind); Input->TryGetStringField(TEXT("identity"),Identity); Input->TryGetStringField(TEXT("anchor"),Anchor);
        if(IsRig && (Kind==TEXT("variable") || Kind==TEXT("component") || ((Kind==TEXT("property") || Kind==TEXT("selection")) && OwnerKind!=TEXT("node")) || (Kind==TEXT("panel") && Anchor!=TEXT("details"))))
            return Fail(TEXT("control_rig_target_requires_graph_node_pin_or_node_details"));
        auto Target=MakeShared<FTarget>(); Target->Blueprint=BP; Target->Editor=Editor; Target->Kind=Kind; Target->OwnerKind=OwnerKind;
        Target->SourceAction=Action;Target->SourceArguments=Arguments;
        if(StepNumber>0)Target->Badge=LexToString((int32)StepNumber);
        Target->Mode=Mode; Target->StepLabel=StepLabel; Target->Message=Message; Target->Anchor=Anchor; Target->Property=Property;
        Target->Identity=Identity;
        const bool WasDirty=BP->GetOutermost()->IsDirty();
        UEdGraph* Graph=nullptr; UEdGraphNode* Node=nullptr;
        if (Kind==TEXT("graph") || Kind==TEXT("node") || Kind==TEXT("pin") || ((Kind==TEXT("property") || Kind==TEXT("selection")) && (OwnerKind==TEXT("node") || OwnerKind==TEXT("graph"))))
        {
            FGuid Gid; Input->TryGetStringField(TEXT("graph_id"),GraphId);
            if (!FGuid::Parse(GraphId,Gid)) return Fail(TEXT("valid_graph_guid_required"));
            TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); int32 Count=0;
            for (auto Candidate : Graphs) if (Candidate && Candidate->GraphGuid==Gid) { Graph=Candidate; ++Count; }
            if (Count!=1) return Fail(TEXT("graph_missing_or_ambiguous"));
            Target->Graph=Graph;
            if (Kind!=TEXT("graph") && OwnerKind!=TEXT("graph"))
            {
                FGuid Nid; Input->TryGetStringField(TEXT("node_id"),NodeId);
                if (!FGuid::Parse(NodeId,Nid)) return Fail(TEXT("valid_node_guid_required")); Count=0;
                for (auto Candidate : Graph->Nodes) if (Candidate && Candidate->NodeGuid==Nid) { Node=Candidate; ++Count; }
                if (Count!=1) return Fail(TEXT("node_missing_or_ambiguous"));
                Target->Node=Node; Target->DetailsObject=Node;
                if(auto Rig=Cast<URigVMEdGraphNode>(Node))
                {Target->RigNode=Rig->GetModelNode();if(!Target->RigNode.IsValid())return Fail(TEXT("rigvm_model_node_unavailable"));}
                if (Kind==TEXT("pin"))
                {
                    FString Pin; Input->TryGetStringField(TEXT("pin_id"),Pin);
                    if (!FGuid::Parse(Pin,Target->PinId)) return Fail(TEXT("valid_pin_guid_required"));
                    Count=0; for (auto Candidate : Node->Pins) if (Candidate && Candidate->PinId==Target->PinId) ++Count;
                    if (Count!=1) return Fail(TEXT("pin_missing_or_ambiguous"));
                }
                Editor->JumpToNode(Node,false);
            }
            else { Editor->JumpToHyperlink(Graph); Target->DetailsObject=Graph; Target->OwnerKind=TEXT("graph"); }
        }
        else if (Kind==TEXT("component") || ((Kind==TEXT("property") || Kind==TEXT("selection")) && OwnerKind==TEXT("component")))
        {
            auto Components=Editor->GetSubobjectEditor(); FSubobjectEditorTreeNodePtrType Match; int32 Count=0;
            for (auto TreeNode : ComponentNodes(Editor)) if (TreeNode && TreeNode->IsComponentNode() && TreeNode->GetObject() && TreeNode->GetObject()->GetPathName()==Identity)
            { Match=TreeNode; ++Count; }
            if (!Components || Count!=1) return Fail(TEXT("component_identity_missing_or_ambiguous"));
            Target->OwnerKind=TEXT("component"); Target->DetailsObject=const_cast<UObject*>(Match->GetObject());
            Components->SelectNode(Match,false); Components->GetDragDropTree()->RequestScrollIntoView(Match);
        }
        else if (Kind==TEXT("variable") || ((Kind==TEXT("property") || Kind==TEXT("selection")) && OwnerKind==TEXT("variable")))
        {
            FGuid Id; if (!FGuid::Parse(Identity,Id)) return Fail(TEXT("valid_variable_guid_required"));
            FName Name; int32 Count=0; for (const auto& Var : BP->NewVariables) if (Var.VarGuid==Id) { Name=Var.VarName; ++Count; }
            auto My=Editor->GetMyBlueprintWidget();
            if (Count!=1 || !My) return Fail(TEXT("variable_missing_or_ambiguous"));
            // Direct selection intentionally does not invoke SMyBlueprint's
            // selection callback. OnNavigation selects, without double-click's
            // execute/create behavior. Details itself may settle next frame.
            My->SelectItemByName(Name,ESelectInfo::OnNavigation);
            auto VariableAction=My->SelectionAsVar();
            auto Field=VariableAction ? VariableAction->GetProperty() : nullptr;
            if (!VariableAction || VariableAction->GetVariableName()!=Name || !Field
                || (Field->GetOwnerClass()!=BP->GeneratedClass && Field->GetOwnerClass()!=BP->SkeletonGeneratedClass)) return Fail(TEXT("variable_selection_identity_mismatch"));
            // This is the same transient wrapper used by the standard callback;
            // it is not a content asset or a newly created class default object.
            auto Wrapper=Field->GetUPropertyWrapper();
            if (!Wrapper || Wrapper->GetProperty()!=Field) return Fail(TEXT("variable_inspector_unavailable"));
            Target->VariableProperty=Field;
            Target->DetailsObject=Wrapper; Target->OwnerKind=TEXT("variable"); Target->Anchor=TEXT("details");
        }
        else if ((Kind==TEXT("property") || Kind==TEXT("selection")) && OwnerKind==TEXT("class_defaults"))
        {
            auto Existing=BP->GeneratedClass ? BP->GeneratedClass->GetDefaultObject(false) : nullptr;
            if (!Existing) return Fail(TEXT("existing_cdo_required_no_creation"));
            Target->DetailsObject=Existing; Editor->StartEditingDefaults(true,false);
        }
        else if ((Kind==TEXT("property") || Kind==TEXT("selection")) && OwnerKind==TEXT("class_settings"))
        {
            auto Command=FInputBindingManager::Get().FindCommandInContext(FName(TEXT("FullBlueprintEditor")),FName(TEXT("EditGlobalOptions")));
            if (!Command || !Editor->GetToolkitCommands()->CanExecuteAction(Command.ToSharedRef())) return Fail(TEXT("class_settings_command_unavailable"));
            Target->DetailsObject=BP; Editor->GetToolkitCommands()->ExecuteAction(Command.ToSharedRef());
        }
        else if (Kind==TEXT("panel") && (Anchor==TEXT("details") || Anchor==TEXT("my_blueprint") || Anchor==TEXT("components"))) Target->OwnerKind=TEXT("panel");
        else if (Kind==TEXT("widget") && !Anchor.IsEmpty()) Target->OwnerKind=TEXT("widget");
        else return Fail(TEXT("unsupported_semantic_target"));
        if (Kind==TEXT("property") && (Property.IsEmpty() || !Target->DetailsObject.IsValid())) return Fail(TEXT("property_owner_and_path_required"));
        if (Kind!=TEXT("property") && !Property.IsEmpty()) return Fail(TEXT("property_path_requires_property_kind"));
        if (!Property.IsEmpty() || Kind==TEXT("selection") || Kind==TEXT("variable"))
        {
            Target->SelectionPending=true;
            ActivateDetails(Editor);
            auto Details=Editor->GetInspector()->GetPropertyView();
            if (Details) if (auto Window=FSlateApplication::Get().FindWidgetWindow(Details.ToSharedRef()))
            {
                Window->BringToFront(true); auto& Slate=FSlateApplication::Get(); auto Focus=Slate.GetKeyboardFocusedWidget();
                if (Slate.GetActiveTopLevelWindow()!=Window && (!Focus || Slate.FindWidgetWindow(Focus.ToSharedRef())!=Window))
                { int32 Budget=10000; if (auto Search=FindDetailsSearch(Details.ToSharedRef(),Budget)) Slate.SetKeyboardFocus(Search,EFocusCause::SetDirectly); }
            }
        }
        auto& Slot=Kind==TEXT("property") ? PropertyTarget : NodeTarget;
        if (Slot) RemoveOverlay(*Slot); Slot=Target; Hidden=false;
        if (!TickHandle.IsValid()) TickHandle=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
        Out->SetBoolField(TEXT("package_dirty_before"),WasDirty); Out->SetBoolField(TEXT("package_dirty_after"),BP->GetOutermost()->IsDirty());
        Out->SetObjectField(TEXT("target"),Snapshot(*Target)); Out->SetBoolField(TEXT("success"),true); return Json(Out);
    }
    if (!Input->TryGetStringField(TEXT("graph_id"), GraphId) || !Input->TryGetStringField(TEXT("node_id"), NodeId)) return Fail(TEXT("explicit_graph_node_required"));
    FGuid Gid, Nid;
    if (!FGuid::Parse(GraphId, Gid) || !FGuid::Parse(NodeId, Nid)) return Fail(TEXT("valid_graph_and_node_guids_required"));
    TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); UEdGraphNode* Node = nullptr; int32 Matches = 0;
    for (auto Graph : Graphs) if (Graph && Graph->GraphGuid == Gid)
        for (auto Candidate : Graph->Nodes) if (Candidate && Candidate->NodeGuid == Nid) { Node = Candidate; ++Matches; }
    if (Matches != 1) return Fail(TEXT("node_missing_or_ambiguous"));
    const bool WasDirty = BP->GetOutermost()->IsDirty();
    auto GraphEditor = SGraphEditor::FindGraphEditorForGraph(Node->GetGraph());
    // Explicit show_node reframes even an already selected off-screen node.
    // show_property keeps the current graph/Details layout when selection matches.
    if (Action == TEXT("show_node") || !GraphEditor || !GraphEditor->GetSelectedNodes().Contains(Node))
    { Editor->JumpToNode(Node, false); GraphEditor = SGraphEditor::FindGraphEditorForGraph(Node->GetGraph()); }
    if (!GraphEditor) return Fail(TEXT("graph_focus_unavailable_after_jump"));
    if (GraphEditor->GetSelectedNodes().Num() != 1 || !GraphEditor->GetSelectedNodes().Contains(Node))
    { GraphEditor->ClearSelectionSet(); GraphEditor->SetNodeSelection(Node, true); }
    auto Target = MakeShared<FTarget>(); Target->Blueprint = BP; Target->Node = Node; Target->Graph = Node->GetGraph(); Target->Editor = Editor;
    Target->SourceAction=Action;Target->SourceArguments=Arguments;
    if(StepNumber>0)Target->Badge=LexToString((int32)StepNumber);
    Target->DetailsObject = Node;
    if(auto Rig=Cast<URigVMEdGraphNode>(Node))
    {Target->RigNode=Rig->GetModelNode();if(!Target->RigNode.IsValid())return Fail(TEXT("rigvm_model_node_unavailable"));}
    Target->Mode = Mode; Target->StepLabel = StepLabel;
    Target->Property = Action == TEXT("show_property") ? Property : FString(); Target->Message = Message;
    Target->SelectionPending = !Target->Property.IsEmpty();
    if (!Target->Property.IsEmpty())
    {
        ActivateDetails(Editor);
        auto Details = Editor->GetInspector()->GetPropertyView();
        // An explicit property explanation targets its actual window, including
        // a detached Details tab. Tracking itself never steals window focus.
        if (Details)
            if (auto DetailsWindow = FSlateApplication::Get().FindWidgetWindow(Details.ToSharedRef()))
            {
                DetailsWindow->BringToFront(true);
                auto& Slate = FSlateApplication::Get();
                auto Focused = Slate.GetKeyboardFocusedWidget();
                auto FocusWindow = Focused ? Slate.FindWidgetWindow(Focused.ToSharedRef()) : nullptr;
                // Preserve inline editing focus when this window already owns it.
                if (Slate.GetActiveTopLevelWindow() != DetailsWindow && FocusWindow != DetailsWindow)
                {
                    int32 Budget = 10000;
                    if (auto Search = FindDetailsSearch(Details.ToSharedRef(), Budget))
                        Slate.SetKeyboardFocus(Search, EFocusCause::SetDirectly);
                }
            }
        // Activating a root window can restore another document's selection.
        // Apply this explicit request once, after activation; tracking never
        // keeps overriding later user selections.
        if (!Target->RigNode.IsValid() && Details && (Details->GetSelectedObjects().Num() != 1 || Details->GetSelectedObjects()[0].Get() != Node))
            Editor->GetInspector()->ShowDetailsForSingleObject(Node);
        bool Found = false;
        if (Details) for (const auto& Path : Details->GetPropertiesInOrderDisplayed())
            if (Path.ToString(TEXT(".")) == Property) { Details->ScrollPropertyIntoView(Path, true); Found = true; break; }
        Out->SetBoolField(TEXT("scroll_requested"), Found);
    }
    Out->SetBoolField(TEXT("package_dirty_before"), WasDirty);
    Out->SetBoolField(TEXT("package_dirty_after"), BP->GetOutermost()->IsDirty());
    Out->SetBoolField(TEXT("dirty_state_changed"), WasDirty != BP->GetOutermost()->IsDirty());
    Out->SetBoolField(TEXT("node_selected"), GraphEditor->GetSelectedNodes().Contains(Node));
    auto& Slot = Action == TEXT("show_node") ? NodeTarget : PropertyTarget;
    if (Slot) RemoveOverlay(*Slot);
    Slot = MoveTemp(Target); Out->SetObjectField(TEXT("target"), Snapshot(*Slot));
    if (!TickHandle.IsValid()) TickHandle = FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateStatic(&Tick));
    Out->SetBoolField(TEXT("success"), true); return Json(Out);
}

void Shutdown()
{
    Sequence::Shutdown();
    if (TickHandle.IsValid()) { FTSTicker::GetCoreTicker().RemoveTicker(TickHandle); TickHandle.Reset(); }
    if (PropertyTarget) RemoveOverlay(*PropertyTarget);
    if (NodeTarget) RemoveOverlay(*NodeTarget);
    PropertyTarget.Reset(); NodeTarget.Reset(); ClearRequested = false; Hidden = false;
}
}
