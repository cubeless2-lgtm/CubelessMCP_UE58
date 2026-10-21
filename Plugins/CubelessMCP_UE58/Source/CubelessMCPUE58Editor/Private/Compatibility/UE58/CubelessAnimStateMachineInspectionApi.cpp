#include "Compatibility/UE58/CubelessAnimStateMachineInspectionApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AnimGraphNode_StateMachineBase.h"
#include "AnimationStateMachineGraph.h"
#include "AnimStateNodeBase.h"
#include "AnimStateTransitionNode.h"
#include "Curves/CurveFloat.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Blueprint/BlueprintExtension.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::AnimStateMachineInspection
{
namespace
{
using Variable::FResult;
constexpr int32 MaxGraphs=4096,MaxInspectedNodeBudget=8192,MaxPins=32768,MaxFieldChars=65536,MaxValueChars=262144,MaxJsonBytes=1024*1024;
FResult* Current=nullptr;TSet<const UEdGraphPin*> KnownPins;TSet<const UEdGraph*> KnownGraphs;int32 ValueChars=0,SerializedRows=0;
UBlueprint* CurrentBlueprint=nullptr;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool Text(const FString& S,int32 Max) { if(S.Len()>Max) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* K,int32 N=1) { double V=0;auto C=R.Data->GetObjectField(TEXT("native_call_counts"));C->TryGetNumberField(K,V);C->SetNumberField(K,V+N); }
void Unsafe(const FString& Error)
{ if(Current) { Current->Data->SetBoolField(TEXT("observation_incomplete"),true);Current->ErrorCode=TEXT("anim_state_machine_observation_incomplete");Current->Error=Error; } }
bool Value(const FString& S)
{ if(!Text(S,MaxFieldChars)||S.Len()>MaxValueChars-ValueChars) { Unsafe(TEXT("Serialized authored text exceeds finite value budget"));return false; }ValueChars+=S.Len();return true; }
bool PinKnown(const UEdGraphPin* P)
{
    if(!P||!KnownPins.Contains(P)) { Unsafe(TEXT("Pin outside current Blueprint inventory"));return false; }
    if(P->WasTrashed()) { Unsafe(TEXT("Trashed current pin; no primary or linked dereference"));return false; }
    UEdGraphNode* N=P->GetOwningNodeUnchecked();if(!Loaded(N)||!N->Pins.Contains(const_cast<UEdGraphPin*>(P))) { Unsafe(TEXT("Pin owner does not match current owning array"));return false; }return true;
}
bool GraphKnown(const UEdGraph* G)
{ if(!G) { return true; }if(!Loaded(G)||!KnownGraphs.Contains(G)) { Unsafe(TEXT("Graph reference outside loaded current Blueprint inventory"));return false; }return true; }
bool StateBoundSafe(UAnimStateNodeBase* N)
{ return !N||(Loaded(N)&&GraphKnown(N->GetBoundGraph())); }
bool NodeSafe(UEdGraphNode* N)
{
    if(!N) { return true; }if(!Loaded(N)||!Loaded(N->GetGraph())||!GraphKnown(N->GetGraph())||!N->GetGraph()->Nodes.Contains(N)) { Unsafe(TEXT("Node is not a usable current graph member"));return false; }
    if(auto M=Cast<UAnimGraphNode_StateMachineBase>(N)) { if(!GraphKnown(M->EditorStateMachineGraph.Get())) { return false; } }
    if(auto S=Cast<UAnimStateNodeBase>(N)) { if(!StateBoundSafe(S)) { Unsafe(TEXT("State bound graph unavailable"));return false; } }
    if(auto T=Cast<UAnimStateTransitionNode>(N))
    {
        if(!GraphKnown(T->GetCustomTransitionGraph())||T->Pins.Num()<2) { Unsafe(TEXT("Transition requires current input/output pin shape before public getters/title"));return false; }
        for(int32 I=0;I<2;++I)
        { UEdGraphPin* P=T->Pins[I];if(!PinKnown(P)) { return false; }
          if(!P->LinkedTo.IsEmpty()) { UEdGraphPin* L=P->LinkedTo[0];if(!PinKnown(L)) { return false; }if(!StateBoundSafe(Cast<UAnimStateNodeBase>(L->GetOwningNodeUnchecked()))) { Unsafe(TEXT("Transition endpoint bound graph unavailable"));return false; } } }
        if(!FMath::IsFinite(T->CrossfadeDuration)||!FMath::IsFinite(T->AutomaticRuleTriggerTime)||!FMath::IsFinite(T->MinTimeBeforeReentry)||!Text(T->SharedRulesName,MaxFieldChars)||!Text(T->SharedCrossfadeName,MaxFieldChars))
        { Unsafe(TEXT("Transition authored settings are nonfinite or outside finite text scope"));return false; }
        if(T->CustomBlendCurve&&!Loaded(T->CustomBlendCurve.Get())) { Unsafe(TEXT("Transition curve object unavailable"));return false; }
    }
    return true;
}
template<class T> T* CountedLoad(const FString& Path,FResult& R)
{ if(!Text(Path,NAME_SIZE-1)) { Unsafe(TEXT("Composed object/class path exceeds FName health bound"));return nullptr; }Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));return O; }
bool BoundedGraphTree(UBlueprint* BP)
{
    if(!Loaded(BP)) { Unsafe(TEXT("Blueprint unavailable before SDK recursive enumeration"));return false; }
    // Stock GetAllGraphs also invokes extension graph producers. These installed native modules
    // have no GetAllGraphs override; their extension hierarchies inherit Engine's public no-op.
    const auto Extensions=BP->GetExtensions();if(Extensions.Num()>MaxGraphs) { Unsafe(TEXT("Blueprint extension array exceeds4096"));return false; }
    for(const UBlueprintExtension* Extension:Extensions)
    {
        if(!Extension) { continue; }if(!Loaded(Extension)) { Unsafe(TEXT("Unloaded Blueprint extension before SDK enumeration"));return false; }
        for(UClass* C=Extension->GetClass();C&&C!=UBlueprintExtension::StaticClass();C=C->GetSuperClass())
        {
            if(!Loaded(C)) { Unsafe(TEXT("Extension class unavailable before SDK graph producer"));return false; }
            const FString Package=C->GetOutermost()->GetName();
            if(!C->IsNative()||(Package!=TEXT("/Script/Engine")&&Package!=TEXT("/Script/AnimGraph")&&Package!=TEXT("/Script/UMGEditor")))
            { Unsafe(TEXT("Unaudited extension graph producer outside finite native no-op hierarchy"));return false; }
        }
    }
    int32 Visits=0;TSet<const UEdGraph*> Path;
    TFunction<bool(UEdGraph*,int32)> Visit=[&](UEdGraph* G,int32 Depth)
    {
        if(!Loaded(G)||Depth>32||++Visits>MaxGraphs||Path.Contains(G))
        { Unsafe(TEXT("Unloaded/cyclic/deep graph tree or more than4096 recursive graph occurrences before SDK traversal"));return false; }
        Path.Add(G);for(UEdGraph* Child:G->SubGraphs) { if(!Visit(Child,Depth+1)) { return false; } }Path.Remove(G);return true;
    };
    auto Roots=[&](const TArray<TObjectPtr<UEdGraph>>& Array)
    { if(Array.Num()>MaxGraphs) { Unsafe(TEXT("Blueprint root graph array exceeds4096"));return false; }for(UEdGraph* G:Array) { if(G&&!Visit(G,0)) { return false; } }return true; };
    // Preserve SDK root/child order and multiplicity; this read only validates, never deduplicates/replaces traversal.
    if(!Roots(BP->FunctionGraphs)||!Roots(BP->MacroGraphs)||!Roots(BP->UbergraphPages)||!Roots(BP->DelegateSignatureGraphs)) { return false; }
    if(BP->ImplementedInterfaces.Num()>MaxGraphs) { Unsafe(TEXT("Blueprint interface array exceeds4096"));return false; }
    for(const auto& Interface:BP->ImplementedInterfaces) { if(!Roots(Interface.Graphs)) { return false; } }
    return true;
}
FString NormalizeBlueprintObjectPath(const FString& BlueprintNameOrPath)
{
    FString Path = FPackageName::ExportTextPathToObjectPath(BlueprintNameOrPath).TrimStartAndEnd();
    Path.TrimQuotesInline();

    if (Path.EndsWith(TEXT("_C")))
    {
        Path.LeftChopInline(2);
    }

    if ((Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/"))) && !Path.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(Path);
        Path = FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
    }

    return Path;
}

UBlueprint* LoadBlueprintFromObjectPath(const FString& ObjectPath,FResult& R)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }

    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    FText InvalidPackageReason;
    if (PackageName.IsEmpty() ||
        ObjectPath.Contains(TEXT("//")) ||
        PackageName.Contains(TEXT("//")) ||
        !FPackageName::IsValidLongPackageName(PackageName, true, &InvalidPackageReason))
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping invalid blueprint object path '%s': %s"), *ObjectPath, *InvalidPackageReason.ToString());
        return nullptr;
    }

    if (UBlueprint* Blueprint = CountedLoad<UBlueprint>(ObjectPath,R))
    {
        return Blueprint;
    }

    FString ClassPath = ObjectPath;
    if (!ClassPath.EndsWith(TEXT("_C")))
    {
        ClassPath += TEXT("_C");
    }

    if (UClass* BlueprintClass = CountedLoad<UClass>(ClassPath,R))
    {
        return Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
    }

    return nullptr;
}

UBlueprint* LoadExistingLevelBlueprintFromWorldPath(const FString& ObjectPath,FResult& R)
{
    if(ObjectPath.IsEmpty()) { return nullptr; }
    UWorld* World=FindObject<UWorld>(nullptr,*ObjectPath);
    if(!World) { World=CountedLoad<UWorld>(ObjectPath,R); }
    if(!Loaded(World)||!Loaded(World->PersistentLevel)) { return nullptr; }
    // Actual SDK argument is bDontCreate. Preserve the original true=no-create behavior.
    if(ULevelScriptBlueprint* B=World->PersistentLevel->GetLevelScriptBlueprint(true)) { return B; }
    return FindObject<ULevelScriptBlueprint>(World->PersistentLevel,*World->GetName());
}

TArray<FString> FindBlueprintAssetPaths(const FString& BlueprintNameOrPath,FResult& R)
{
    TArray<FString> CandidatePaths;
    const FString Query = NormalizeBlueprintObjectPath(BlueprintNameOrPath);

    if (Query.StartsWith(TEXT("/Game/")) || Query.StartsWith(TEXT("/Engine/")))
    {
        if (LoadBlueprintFromObjectPath(Query,R))
        {
            CandidatePaths.Add(Query);
            return CandidatePaths;
        }
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);

    const FString ShortQuery = FPackageName::GetShortName(Query);
    for (const FAssetData& AssetData : BlueprintAssets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            AssetName.Equals(ShortQuery, ESearchCase::IgnoreCase) ||
            PackageName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(Query, ESearchCase::IgnoreCase))
        {
            CandidatePaths.Add(ObjectPath);
        }
    }

    return CandidatePaths;
}

UBlueprint* FindBlueprintByName(const FString& BlueprintName,FResult& R)
{
    const FString NormalizedPath = NormalizeBlueprintObjectPath(BlueprintName);
    if (UBlueprint* DirectBlueprint = LoadBlueprintFromObjectPath(NormalizedPath,R))
    {
        return DirectBlueprint;
    }

    if (UBlueprint* LevelBlueprint = LoadExistingLevelBlueprintFromWorldPath(NormalizedPath,R))
    {
        return LevelBlueprint;
    }

    const bool bIsPathLikeInput = BlueprintName.Contains(TEXT("/")) || BlueprintName.Contains(TEXT("."));
    if (!bIsPathLikeInput)
    {
        const FString LegacyObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
        if (UBlueprint* LegacyBlueprint = LoadBlueprintFromObjectPath(LegacyObjectPath,R))
        {
            return LegacyBlueprint;
        }
    }

    const TArray<FString> CandidatePaths = FindBlueprintAssetPaths(BlueprintName,R);
    if (CandidatePaths.Num() == 1)
    {
        return LoadBlueprintFromObjectPath(CandidatePaths[0],R);
    }

    if (CandidatePaths.Num() > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ambiguous blueprint name '%s'. Use a full asset path. Candidates:"), *BlueprintName);
        for (const FString& CandidatePath : CandidatePaths)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - %s"), *CandidatePath);
        }
    }

    return nullptr;
}

bool BlueprintGraphArrayContains(const TArray<TObjectPtr<UEdGraph>>& GraphArray, const UEdGraph* Graph)
{
    for (const TObjectPtr<UEdGraph>& CandidateGraph : GraphArray)
    {
        if (CandidateGraph.Get() == Graph)
        {
            return true;
        }
    }

    return false;
}

FString GetBlueprintGraphType(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (!Blueprint || !Graph)
    {
        return TEXT("unknown");
    }

    if (BlueprintGraphArrayContains(Blueprint->FunctionGraphs, Graph))
    {
        return TEXT("function");
    }
    if (BlueprintGraphArrayContains(Blueprint->MacroGraphs, Graph))
    {
        return TEXT("macro");
    }
    if (BlueprintGraphArrayContains(Blueprint->UbergraphPages, Graph) || BlueprintGraphArrayContains(Blueprint->EventGraphs, Graph))
    {
        return TEXT("event");
    }
    if (BlueprintGraphArrayContains(Blueprint->DelegateSignatureGraphs, Graph))
    {
        return TEXT("delegate");
    }

    return TEXT("unknown");
}

TSharedPtr<FJsonObject> GraphToJson(const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    TSharedPtr<FJsonObject> GraphObject = MakeShared<FJsonObject>();
    if (!Graph)
    {
        return GraphObject;
    }

    GraphObject->SetStringField(TEXT("graph_id"), Graph->GraphGuid.ToString());
    GraphObject->SetStringField(TEXT("graph_name"), Graph->GetName());
    GraphObject->SetStringField(TEXT("graph_type"), GetBlueprintGraphType(Blueprint, Graph));
    GraphObject->SetStringField(TEXT("schema"), Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetName() : FString());
    GraphObject->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    return GraphObject;
}

FString GetPinDirectionName(const UEdGraphPin* Pin)
{
    return Pin && Pin->Direction == EGPD_Output ? TEXT("output") : TEXT("input");
}

TSharedPtr<FJsonObject> PinTypeToJson(const FEdGraphPinType& PinType)
{
    TSharedPtr<FJsonObject> TypeObject = MakeShared<FJsonObject>();
    TypeObject->SetStringField(TEXT("category"), PinType.PinCategory.ToString());
    TypeObject->SetStringField(TEXT("subcategory"), PinType.PinSubCategory.ToString());
    TypeObject->SetBoolField(TEXT("is_array"), PinType.IsArray());
    if (PinType.PinSubCategoryObject.IsValid())
    {
        TypeObject->SetStringField(TEXT("subcategory_object"), PinType.PinSubCategoryObject->GetPathName());
    }
    return TypeObject;
}

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
    if (Pin && !PinKnown(Pin))
    {
        PinObject->SetBoolField(TEXT("observation_incomplete"), true);
        return PinObject;
    }
    if(Pin) { if(!Value(Pin->DefaultValue)) { return PinObject; }Count(*Current,TEXT("pins_serialized")); }
    PinObject->SetStringField(TEXT("name"), Pin ? Pin->PinName.ToString() : FString());
    PinObject->SetStringField(TEXT("direction"), GetPinDirectionName(Pin));

    if (Pin)
    {
        PinObject->SetStringField(TEXT("category"), Pin->PinType.PinCategory.ToString());
        PinObject->SetStringField(TEXT("subcategory"), Pin->PinType.PinSubCategory.ToString());
        PinObject->SetBoolField(TEXT("is_array"), Pin->PinType.IsArray());
        PinObject->SetObjectField(TEXT("pin_type"), PinTypeToJson(Pin->PinType));
        PinObject->SetStringField(TEXT("default_value"), Pin->DefaultValue);

        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            PinObject->SetStringField(TEXT("subcategory_object"), Pin->PinType.PinSubCategoryObject->GetPathName());
        }
        if (Pin->DefaultObject)
        {
            PinObject->SetStringField(TEXT("default_object"), Pin->DefaultObject->GetPathName());
        }

        TArray<TSharedPtr<FJsonValue>> LinkedPins;
        for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
        {
            if (!PinKnown(LinkedPin))
            {
                continue;
            }

            TSharedPtr<FJsonObject> LinkedPinObject = MakeShared<FJsonObject>();
            LinkedPinObject->SetStringField(TEXT("node_id"), LinkedPin->GetOwningNodeUnchecked()->NodeGuid.ToString());
            LinkedPinObject->SetStringField(TEXT("node_name"), LinkedPin->GetOwningNodeUnchecked()->GetName());
            LinkedPinObject->SetStringField(TEXT("pin_name"), LinkedPin->PinName.ToString());
            Count(*Current,TEXT("links_serialized"));
            LinkedPins.Add(MakeShared<FJsonValueObject>(LinkedPinObject));
        }
        PinObject->SetArrayField(TEXT("linked_to"), LinkedPins);
    }

    return PinObject;
}

TSharedPtr<FJsonObject> NodeToJson(UEdGraphNode* Node, bool bIncludePins)
{
    TSharedPtr<FJsonObject> NodeObject = MakeShared<FJsonObject>();
    if (!Node)
    {
        return NodeObject;
    }

    if(++SerializedRows>MaxInspectedNodeBudget) { Unsafe(TEXT("Repeated serialized node rows exceed8192 health bound"));return NodeObject; }
    if(!NodeSafe(Node)||!Value(Node->NodeComment)||!Value(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString())) { return NodeObject; }Count(*Current,TEXT("nodes_serialized"));
    NodeObject->SetStringField(TEXT("node_id"), Node->NodeGuid.ToString());
    NodeObject->SetStringField(TEXT("name"), Node->GetName());
    NodeObject->SetStringField(TEXT("class"), Node->GetClass()->GetName());
    NodeObject->SetStringField(TEXT("title"), Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    NodeObject->SetNumberField(TEXT("x"), Node->NodePosX);
    NodeObject->SetNumberField(TEXT("y"), Node->NodePosY);
    NodeObject->SetStringField(TEXT("node_comment"), Node->NodeComment);
    NodeObject->SetBoolField(TEXT("comment_bubble_visible"), Node->bCommentBubbleVisible);
    NodeObject->SetBoolField(TEXT("comment_bubble_pinned"), Node->bCommentBubblePinned);

    if (bIncludePins)
    {
        TArray<TSharedPtr<FJsonValue>> Pins;
        for (UEdGraphPin* Pin : Node->Pins)
        {
            Pins.Add(MakeShared<FJsonValueObject>(PinToJson(Pin)));
        }
        NodeObject->SetArrayField(TEXT("pins"), Pins);
    }

    return NodeObject;
}
TSharedPtr<FJsonValue> EnumValueToJson(const UEnum* Enum, int64 RawValue)
{
    TSharedPtr<FJsonObject> EnumObject = MakeShared<FJsonObject>();
    EnumObject->SetNumberField(TEXT("value"), static_cast<double>(RawValue));
    if (Enum)
    {
        EnumObject->SetStringField(TEXT("name"), Enum->GetNameStringByValue(RawValue));
        EnumObject->SetStringField(TEXT("enum_type"), Enum->GetName());
    }
    return MakeShared<FJsonValueObject>(EnumObject);
}

TSharedPtr<FJsonObject> AnimStateNodeToJson(const UBlueprint* Blueprint, UAnimStateNodeBase* StateNode, bool bIncludePins)
{
    TSharedPtr<FJsonObject> StateObject = MakeShared<FJsonObject>();
    if (!StateNode)
    {
        StateObject->SetBoolField(TEXT("valid"), false);
        return StateObject;
    }

    if(!NodeSafe(StateNode)) { StateObject->SetBoolField(TEXT("valid"),false);return StateObject; }Count(*Current,TEXT("state_rows_serialized"));
    StateObject = NodeToJson(StateNode, bIncludePins);
    StateObject->SetBoolField(TEXT("valid"), true);
    StateObject->SetStringField(TEXT("state_name"), StateNode->GetStateName());

    if (UEdGraph* BoundGraph = StateNode->GetBoundGraph())
    {
        StateObject->SetObjectField(TEXT("bound_graph"), GraphToJson(Blueprint, BoundGraph));
    }

    return StateObject;
}

TSharedPtr<FJsonObject> AnimStateTransitionSettingsToJson(const UAnimStateTransitionNode* TransitionNode)
{
    TSharedPtr<FJsonObject> SettingsObject = MakeShared<FJsonObject>();
    if (!TransitionNode)
    {
        return SettingsObject;
    }

    if(!NodeSafe(const_cast<UAnimStateTransitionNode*>(TransitionNode))) { return SettingsObject; }Count(*Current,TEXT("transition_settings_serialized"));
    SettingsObject->SetNumberField(TEXT("priority_order"), TransitionNode->PriorityOrder);
    SettingsObject->SetNumberField(TEXT("crossfade_duration"), TransitionNode->CrossfadeDuration);
    SettingsObject->SetField(TEXT("blend_mode"), EnumValueToJson(StaticEnum<EAlphaBlendOption>(), static_cast<int64>(TransitionNode->BlendMode)));
    SettingsObject->SetBoolField(TEXT("automatic_rule_based_on_sequence_player_in_state"), TransitionNode->bAutomaticRuleBasedOnSequencePlayerInState);
    SettingsObject->SetNumberField(TEXT("automatic_rule_trigger_time"), TransitionNode->AutomaticRuleTriggerTime);
#if UE_VERSION_OLDER_THAN(5, 7, 0)
    SettingsObject->SetNumberField(TEXT("min_time_before_reentry"), 0.0);
    SettingsObject->SetStringField(TEXT("min_time_before_reentry_note"), TEXT("unavailable_in_ue_5_6"));
#else
    SettingsObject->SetNumberField(TEXT("min_time_before_reentry"), TransitionNode->MinTimeBeforeReentry);
#endif
    SettingsObject->SetStringField(TEXT("sync_group_name_to_require_valid_markers_rule"), TransitionNode->SyncGroupNameToRequireValidMarkersRule.ToString());
    SettingsObject->SetField(TEXT("logic_type"), EnumValueToJson(StaticEnum<ETransitionLogicType::Type>(), TransitionNode->LogicType.GetValue()));
    SettingsObject->SetBoolField(TEXT("bidirectional"), TransitionNode->Bidirectional);
    SettingsObject->SetBoolField(TEXT("disabled"), TransitionNode->bDisabled);
    SettingsObject->SetBoolField(TEXT("shared_rules"), TransitionNode->bSharedRules);
    SettingsObject->SetStringField(TEXT("shared_rules_name"), TransitionNode->SharedRulesName);
    SettingsObject->SetStringField(TEXT("shared_rules_guid"), TransitionNode->SharedRulesGuid.ToString());
    SettingsObject->SetBoolField(TEXT("shared_crossfade"), TransitionNode->bSharedCrossfade);
    SettingsObject->SetStringField(TEXT("shared_crossfade_name"), TransitionNode->SharedCrossfadeName);
    SettingsObject->SetStringField(TEXT("shared_crossfade_guid"), TransitionNode->SharedCrossfadeGuid.ToString());
    SettingsObject->SetNumberField(TEXT("shared_crossfade_index"), TransitionNode->SharedCrossfadeIdx);
#if UE_VERSION_OLDER_THAN(5, 7, 0)
    SettingsObject->SetBoolField(TEXT("allow_inertialization_for_self_transitions"), false);
    SettingsObject->SetStringField(TEXT("allow_inertialization_for_self_transitions_note"), TEXT("unavailable_in_ue_5_6"));
#else
    SettingsObject->SetBoolField(TEXT("allow_inertialization_for_self_transitions"), TransitionNode->bAllowInertializationForSelfTransitions);
#endif
    SettingsObject->SetStringField(TEXT("custom_blend_curve"), TransitionNode->CustomBlendCurve ? TransitionNode->CustomBlendCurve->GetPathName() : FString());
    SettingsObject->SetBoolField(TEXT("has_custom_transition_graph"), TransitionNode->GetCustomTransitionGraph() != nullptr);
    if(!BoundedGraphTree(CurrentBlueprint)) { return SettingsObject; }
    Count(*Current,TEXT("bound_graph_shared_getter_calls"));
    SettingsObject->SetBoolField(TEXT("is_bound_graph_shared"), TransitionNode->IsBoundGraphShared());
    return SettingsObject;
}

TSharedPtr<FJsonObject> TransitionGraphNodesToJson(const UBlueprint* Blueprint, UEdGraph* Graph, bool bIncludePins, int32 MaxNodes)
{
    TSharedPtr<FJsonObject> GraphObject = GraphToJson(Blueprint, Graph);
    if (!Graph)
    {
        return GraphObject;
    }

    TArray<TSharedPtr<FJsonValue>> Nodes;
    int32 AddedNodeCount = 0;
    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if(Current->Data->GetBoolField(TEXT("observation_incomplete"))) { break; }
        if (!Node)
        {
            continue;
        }

        if (MaxNodes >= 0 && AddedNodeCount >= MaxNodes)
        {
            break;
        }

        Nodes.Add(MakeShared<FJsonValueObject>(NodeToJson(Node, bIncludePins)));
        ++AddedNodeCount;
    }

    GraphObject->SetArrayField(TEXT("nodes"), Nodes);
    GraphObject->SetNumberField(TEXT("included_node_count"), AddedNodeCount);
    GraphObject->SetBoolField(TEXT("truncated"), MaxNodes >= 0 && Graph->Nodes.Num() > MaxNodes);
    return GraphObject;
}
bool Inventory(UBlueprint* BP,TArray<UEdGraph*>& Graphs,FResult& R)
{
    if(!BoundedGraphTree(BP)) { return false; }
    Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(Graphs);KnownGraphs.Reset();KnownPins.Reset();
    if(Graphs.Num()>MaxGraphs) { Unsafe(TEXT("Blueprint graph count exceeds4096"));return false; }
    for(UEdGraph* G:Graphs) { if(!Loaded(G)) { Unsafe(TEXT("Null/unusable Blueprint graph would make SDK enumeration unsafe"));return false; }KnownGraphs.Add(G); }
    int32 Nodes=0;Count(R,TEXT("pins_inventory_calls"));
    for(UEdGraph* G:Graphs)
    { Count(R,TEXT("graphs_visited"));for(UEdGraphNode* N:G->Nodes)
      { if(!N) { continue; }if(++Nodes>MaxInspectedNodeBudget||!Loaded(N)||N->GetGraph()!=G) { Unsafe(TEXT("Unusable node or Blueprint node inventory exceeds8192"));return false; }Count(R,TEXT("nodes_inventory_visited"));
        for(UEdGraphPin* P:N->Pins) { if(!P) { continue; }if(KnownPins.Num()>=MaxPins) { Unsafe(TEXT("Pin inventory exceeds32768"));return false; }KnownPins.Add(P); } }
    }
    for(UEdGraph* G:Graphs) { for(UEdGraphNode* N:G->Nodes) { if(N&&!NodeSafe(N)) { return false; } } }return true;
}
FString Capture(UBlueprint* BP,const TArray<UEdGraph*>& Graphs,FResult& R)
{
    Count(R,TEXT("state_capture_calls"));ValueChars=SerializedRows=0;auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("package_dirty"),BP->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> Rows;
    for(UEdGraph* G:Graphs)
    { if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { break; }if(!GraphKnown(G)) { continue; }auto Row=GraphToJson(BP,G);TArray<TSharedPtr<FJsonValue>> Nodes;
      for(UEdGraphNode* N:G->Nodes)
      { if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { break; }if(!N) { Nodes.Add(MakeShared<FJsonValueNull>());continue; }auto Node=NodeToJson(N,true);
        if(auto T=Cast<UAnimStateTransitionNode>(N)) { if(NodeSafe(T)) { Node->SetObjectField(TEXT("transition_settings"),AnimStateTransitionSettingsToJson(T)); } }
        Nodes.Add(MakeShared<FJsonValueObject>(Node)); }
      Row->SetArrayField(TEXT("nodes"),Nodes);Rows.Add(MakeShared<FJsonValueObject>(Row)); }
    J->SetArrayField(TEXT("graphs"),Rows);FString Out;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Out));
    if(FTCHARToUTF8(*Out).Length()>MaxJsonBytes) { Unsafe(TEXT("Authored state capture exceeds1MiB health bound"));return FString(); }return Out;
}
void Init(FResult& R)
{
    R.Data->SetStringField(TEXT("operation"),TEXT("inspect_anim_state_machine_transitions"));R.Data->SetBoolField(TEXT("read_only_direct_API"),true);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("runtime_state_machine_evaluation_verified"),false);R.Data->SetBoolField(TEXT("total_load_or_cache_side_effects_verified"),false);
    R.Data->SetStringField(TEXT("authored_state_scope"),TEXT("Current captured graph/node identity, layout, comments, bubbles, pin metadata/link order, returned transition settings and package dirty flag; not all state properties, private caches, persisted disk, runtime evaluation or load-side-effect proof"));
    auto C=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("get_all_graphs_calls"),TEXT("get_all_nodes_of_class_calls"),TEXT("graphs_visited"),TEXT("nodes_inventory_visited"),TEXT("pins_inventory_calls"),TEXT("nodes_serialized"),TEXT("pins_serialized"),TEXT("links_serialized"),TEXT("state_capture_calls"),TEXT("state_machine_name_calls"),TEXT("previous_state_getter_calls"),TEXT("next_state_getter_calls"),TEXT("state_rows_serialized"),TEXT("transition_rows_serialized"),TEXT("transition_settings_serialized"),TEXT("bound_graph_shared_getter_calls"),TEXT("explicit_modify_calls"),TEXT("explicit_compile_calls"),TEXT("explicit_save_calls"),TEXT("graph_create_calls"),TEXT("runtime_evaluation_calls")}) { C->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),C);
}
}
Variable::FResult Inspect(const FString& BlueprintName,const FString& StateMachineName,bool bIncludePins,bool bIncludeRuleGraphNodes,bool bIncludeStateNodes,int64 MaxRuleGraphNodesInput)
{
    FResult R;Init(R);
    if(!IsEditorGameThread()||Current) { R.ErrorCode=TEXT("anim_state_machine_editor_state_invalid");R.Error=TEXT("Nonreentrant Editor game thread required");return R; }
    struct FScope { FScope(FResult& R) { Current=&R;ValueChars=SerializedRows=0; }~FScope() { Current=nullptr;CurrentBlueprint=nullptr;KnownGraphs.Reset();KnownPins.Reset();ValueChars=SerializedRows=0; } } Scope(R);
    if(!Text(BlueprintName,NAME_SIZE-1)||!Text(StateMachineName,MaxFieldChars)) { R.ErrorCode=TEXT("anim_state_machine_input_invalid");R.Error=TEXT("Bounded literal strings without interiorNUL required");return R; }
    R.Stage=TEXT("load_blueprint");UBlueprint* BP=FindBlueprintByName(BlueprintName,R);if(!Loaded(BP)) { if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("blueprint_not_found");R.Error=TEXT("Blueprint not found: ")+BlueprintName; }return R; }
    const TStrongObjectPtr<UBlueprint> Keep(BP);CurrentBlueprint=BP;TArray<UEdGraph*> Graphs;
    R.Stage=TEXT("inventory");if(!Inventory(BP,Graphs,R)) { R.bChangeStateKnown=false;return R; }
    const FString Before=Capture(BP,Graphs,R);ValueChars=SerializedRows=0;
    R.Data->SetNumberField(TEXT("max_rule_graph_nodes"),static_cast<int32>(FMath::Clamp<int64>(MaxRuleGraphNodesInput,-1,2048)));
    UBlueprint* Blueprint=BP;const FString StateMachineFilter=StateMachineName;const int32 MaxRuleGraphNodes=static_cast<int32>(FMath::Clamp<int64>(MaxRuleGraphNodesInput,-1,2048));
    TArray<UAnimGraphNode_StateMachineBase*> StateMachineNodes;
    if(!BoundedGraphTree(BP)) { R.bChangeStateKnown=false;return R; }
    Count(R,TEXT("get_all_nodes_of_class_calls"));FBlueprintEditorUtils::GetAllNodesOfClass<UAnimGraphNode_StateMachineBase>(Blueprint, StateMachineNodes);

    TArray<TSharedPtr<FJsonValue>> StateMachineValues;
    int32 TotalTransitionCount = 0;
    for (UAnimGraphNode_StateMachineBase* StateMachineNode : StateMachineNodes)
    {
        if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { break; }
        if (!StateMachineNode)
        {
            continue;
        }

        if(!NodeSafe(StateMachineNode)) { continue; }Count(R,TEXT("state_machine_name_calls"));
        const FString StateMachineName = StateMachineNode->GetStateMachineName();
        const FString StateMachineTitle = StateMachineNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (!StateMachineFilter.IsEmpty() &&
            !StateMachineName.Contains(StateMachineFilter, ESearchCase::IgnoreCase) &&
            !StateMachineTitle.Contains(StateMachineFilter, ESearchCase::IgnoreCase))
        {
            continue;
        }

        UAnimationStateMachineGraph* StateMachineGraph = StateMachineNode->EditorStateMachineGraph;
        TSharedPtr<FJsonObject> StateMachineObject = MakeShared<FJsonObject>();
        StateMachineObject->SetStringField(TEXT("state_machine_name"), StateMachineName);
        StateMachineObject->SetObjectField(TEXT("state_machine_node"), NodeToJson(StateMachineNode, bIncludePins));
        StateMachineObject->SetObjectField(TEXT("state_machine_graph"), GraphToJson(Blueprint, StateMachineGraph));

        TArray<TSharedPtr<FJsonValue>> StateValues;
        TArray<TSharedPtr<FJsonValue>> TransitionValues;
        if (StateMachineGraph)
        {
            for (UEdGraphNode* GraphNode : StateMachineGraph->Nodes)
            {
                if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { break; }
                if (!GraphNode)
                {
                    continue;
                }

                if (UAnimStateTransitionNode* TransitionNode = Cast<UAnimStateTransitionNode>(GraphNode))
                {
                    if(!NodeSafe(TransitionNode)) { continue; }Count(R,TEXT("transition_rows_serialized"));Count(R,TEXT("previous_state_getter_calls"));Count(R,TEXT("next_state_getter_calls"));
                    UAnimStateNodeBase* PreviousState = TransitionNode->GetPreviousState();
                    UAnimStateNodeBase* NextState = TransitionNode->GetNextState();

                    TSharedPtr<FJsonObject> TransitionObject = MakeShared<FJsonObject>();
                    TransitionObject->SetStringField(TEXT("transition_id"), TransitionNode->NodeGuid.ToString());
                    TransitionObject->SetStringField(TEXT("transition_title"), static_cast<UEdGraphNode*>(TransitionNode)->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
                    TransitionObject->SetObjectField(TEXT("transition_node"), NodeToJson(TransitionNode, bIncludePins));
                    TransitionObject->SetObjectField(TEXT("source_state"), AnimStateNodeToJson(Blueprint, PreviousState, false));
                    TransitionObject->SetObjectField(TEXT("target_state"), AnimStateNodeToJson(Blueprint, NextState, false));
                    TransitionObject->SetObjectField(TEXT("settings"), AnimStateTransitionSettingsToJson(TransitionNode));

                    if (UEdGraph* BoundGraph = TransitionNode->GetBoundGraph())
                    {
                        TransitionObject->SetObjectField(
                            TEXT("rule_graph"),
                            bIncludeRuleGraphNodes
                                ? TransitionGraphNodesToJson(Blueprint, BoundGraph, bIncludePins, MaxRuleGraphNodes)
                                : GraphToJson(Blueprint, BoundGraph));
                    }

                    if (UEdGraph* CustomTransitionGraph = TransitionNode->GetCustomTransitionGraph())
                    {
                        TransitionObject->SetObjectField(TEXT("custom_transition_graph"), GraphToJson(Blueprint, CustomTransitionGraph));
                    }

                    TransitionValues.Add(MakeShared<FJsonValueObject>(TransitionObject));
                    ++TotalTransitionCount;
                }
                else if (bIncludeStateNodes)
                {
                    if (UAnimStateNodeBase* StateNode = Cast<UAnimStateNodeBase>(GraphNode))
                    {
                        StateValues.Add(MakeShared<FJsonValueObject>(AnimStateNodeToJson(Blueprint, StateNode, false)));
                    }
                }
            }
        }

        StateMachineObject->SetArrayField(TEXT("states"), StateValues);
        StateMachineObject->SetArrayField(TEXT("transitions"), TransitionValues);
        StateMachineObject->SetNumberField(TEXT("state_count"), StateValues.Num());
        StateMachineObject->SetNumberField(TEXT("transition_count"), TransitionValues.Num());
        StateMachineValues.Add(MakeShared<FJsonValueObject>(StateMachineObject));
    }

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    ResultObj->SetStringField(TEXT("state_machine_filter"), StateMachineFilter);
    ResultObj->SetNumberField(TEXT("state_machine_count"), StateMachineValues.Num());
    ResultObj->SetNumberField(TEXT("transition_count"), TotalTransitionCount);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetArrayField(TEXT("state_machines"), StateMachineValues);

    R.Data->SetObjectField(TEXT("legacy_result"),ResultObj);R.Data->SetArrayField(TEXT("state_machines"),StateMachineValues);R.Data->SetNumberField(TEXT("state_machine_count"),StateMachineValues.Num());R.Data->SetNumberField(TEXT("transition_count"),TotalTransitionCount);R.Data->SetStringField(TEXT("blueprint_name"),BlueprintName);R.Data->SetStringField(TEXT("blueprint_path"),BP->GetPathName());R.Data->SetStringField(TEXT("state_machine_filter"),StateMachineName);R.Data->SetBoolField(TEXT("read_only"),true);
    R.Stage=TEXT("after_observation");TArray<UEdGraph*> AfterGraphs;if(!Inventory(BP,AfterGraphs,R)) { R.bChangeStateKnown=false;return R; }const FString After=Capture(BP,AfterGraphs,R);
    const bool Same=Before.Equals(After,ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("authored_state_preserved"),Same);R.bChanged=!Same;
    const FString Envelope=Cubeless::Operations::EncodeVariableResult(R,TEXT("inspect_anim_state_machine_transitions"));
    if(FTCHARToUTF8(*Envelope).Length()>MaxJsonBytes) { R.Data->RemoveField(TEXT("state_machines"));R.Data->RemoveField(TEXT("legacy_result"));R.Data->SetBoolField(TEXT("observation_incomplete"),true);R.bChangeStateKnown=false;R.ErrorCode=TEXT("anim_state_machine_result_bound");R.Error=TEXT("Native result exceeds1MiB reflected-result health scope; no complete unbounded output claim");return R; }
    if(R.Data->GetBoolField(TEXT("observation_incomplete"))) { R.bChangeStateKnown=false;R.Stage=TEXT("observation_incomplete");return R; }
    if(!Same) { R.ErrorCode=TEXT("anim_state_machine_authored_state_changed");R.Error=TEXT("Public getters changed captured authored state; actual prefix retained without retry");return R; }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return R;
}
}
