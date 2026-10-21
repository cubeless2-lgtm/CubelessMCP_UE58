#include "Compatibility/UE58/CubelessBlueprintTopologyApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EdGraph/EdGraphSchema.h"
#include "EdGraphSchema_K2.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "GameFramework/Actor.h"
#include "K2Node.h"
#include "K2Node_CallFunction.h"
#include "K2Node_VariableGet.h"
#include "K2Node_VariableSet.h"
#include "K2Node_Event.h"
#include "K2Node_ComponentBoundEvent.h"
#include "K2Node_ActorBoundEvent.h"
#include "K2Node_InputAction.h"
#include "K2Node_InputKey.h"
#include "K2Node_InputAxisEvent.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_AddDelegate.h"
#include "K2Node_RemoveDelegate.h"
#include "K2Node_ClearDelegate.h"
#include "K2Node_CallDelegate.h"
#include "K2Node_AssignDelegate.h"
#include "K2Node_CreateDelegate.h"
#include "K2Node_Message.h"
#include "K2Node_CustomEvent.h"
#include "K2Node_MacroInstance.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "UObject/NoExportTypes.h"
#include "UObject/UObjectGlobals.h"
namespace Cubeless::UE58::BlueprintTopology
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
FResult* CurrentResult=nullptr;
TSet<const UEdGraphPin*> KnownPins;
void Count(FResult& R,const TCHAR* Key,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(Key,J->GetNumberField(Key)+N); }
void NoteUnsafe(const TCHAR* Kind)
{
    if(CurrentResult) { CurrentResult->Data->SetBoolField(TEXT("observation_incomplete"),true);CurrentResult->ErrorCode=TEXT("topology_pointer_or_property_scope_incomplete");CurrentResult->Error=Kind; }
}
bool PinKnown(const UEdGraphPin* P)
{
    if(!P) { return false; }
    if(CurrentResult) { Count(*CurrentResult,TEXT("pins_visited")); }
    if(!KnownPins.Contains(P)) { NoteUnsafe(TEXT("Linked pin is not in the loaded Blueprint pin inventory; no pointer dereference or invented safe link"));return false; }
    UEdGraphNode* N=P->GetOwningNodeUnchecked();
    if(!Loaded(N)||!N->Pins.Contains(const_cast<UEdGraphPin*>(P))) { NoteUnsafe(TEXT("Pin owner is invalid or not reciprocal with the owning pins array"));return false; }return true;
}
void Init(FResult& R)
{
    auto J=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("get_all_graphs_calls"),TEXT("graphs_visited"),TEXT("nodes_visited"),TEXT("nodes_serialized"),TEXT("link_rows_serialized"),TEXT("reference_search_calls"),TEXT("function_target_get_calls"),TEXT("variable_source_get_calls"),TEXT("variable_property_get_calls"),TEXT("pins_inventory_calls"),TEXT("pins_visited"),TEXT("reflected_property_reads"),TEXT("member_state_capture_calls")}) { J->SetNumberField(K,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),J);R.Data->SetBoolField(TEXT("observation_incomplete"),false);R.Data->SetBoolField(TEXT("read_only_direct_API"),true);R.Data->SetBoolField(TEXT("runtime_execution_verified"),false);R.Data->SetBoolField(TEXT("total_SDK_cache_or_load_side_effects_verified"),false);R.Data->SetStringField(TEXT("operation"),TEXT("inspect_blueprint_graph_call_topology"));
}
TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Error) { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Error);return J; }

// HL metadata admission is active only for the two narrow bridges below.
// Existing topology observer keeps its original code path when this pointer is null.
struct FMetadataGuard
{
    FResult& R; TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
    struct FObject { TStrongObjectPtr<UObject> Hold;TWeakObjectPtr<UObject> Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;FString Path;
        explicit FObject(UObject* O):Hold(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Effective(O->GetPackage()),Path(O->GetPathName()){}
        bool Exact()const{auto* O=Hold.Get();return Loaded(O)&&Loaded(Class.Get())&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==Effective.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);}};
    TArray<TUniquePtr<FObject>> Pins;TArray<TArray<TWeakObjectPtr<UEdGraph>>> RootGraphs;TArray<int32> InterfaceCounts;TArray<TWeakObjectPtr<UEdGraph>> InterfaceGraphs;TWeakObjectPtr<UBlueprint> Blueprint;TWeakObjectPtr<UClass> Generated,Skeleton;TWeakObjectPtr<UEdGraph> Graph;FGuid GraphId;
    explicit FMetadataGuard(FResult& Result):R(Result),Editor(GEditor),World(GEditor?GEditor->GetEditorWorldContext().World():nullptr){}
    bool Fail(const TCHAR* Code,const FString& Error){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Error;}R.bSuccess=false;R.bPostconditionPassed=false;bool SDKUnknown=false;R.Data->TryGetBoolField(TEXT("SDK_global_purity_unverified"),SDKUnknown);if(SDKUnknown)R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("observation_incomplete"),true);return false;}
    bool Safe(const FString& Stage)
    {
        R.Stage=Stage;if(!R.Error.IsEmpty())return false;
        if(!IsInGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage())return Fail(TEXT("blueprint_metadata_context_unavailable"),TEXT("Editor GT outside GC/exit/save required; no further getter/load/replay."));
        auto* CurrentWorld=GEditor->GetEditorWorldContext().World();if(Editor.Get()!=GEditor||!Loaded(CurrentWorld)||World.Get()!=CurrentWorld||CurrentWorld->bIsTearingDown)return Fail(TEXT("blueprint_metadata_context_changed"),TEXT("Editor/world identity changed across callback."));
        for(const auto& P:Pins)if(!P->Exact())return Fail(TEXT("blueprint_metadata_identity_changed"),TEXT("Captured class/outer/package/path changed."));
        if(auto* B=Blueprint.Get()){if(B->GeneratedClass!=Generated.Get()||B->SkeletonGeneratedClass!=Skeleton.Get()||(Generated.IsValid()&&Generated->HasAnyClassFlags(CLASS_NewerVersionExists))||(Skeleton.IsValid()&&Skeleton->HasAnyClassFlags(CLASS_NewerVersionExists)))return Fail(TEXT("blueprint_metadata_class_changed"),TEXT("Current generated/skeleton class changed across metadata callback."));
            if(!RootGraphs.IsEmpty()){int32 Role=0;auto Same=[&](const auto& A){const auto& E=RootGraphs[Role++];if(A.Num()!=E.Num())return false;for(int32 I=0;I<A.Num();++I)if(A[I]!=E[I].Get())return false;return true;};if(!Same(B->FunctionGraphs)||!Same(B->MacroGraphs)||!Same(B->UbergraphPages)||!Same(B->EventGraphs)||!Same(B->DelegateSignatureGraphs)||B->ImplementedInterfaces.Num()!=InterfaceCounts.Num())return Fail(TEXT("blueprint_metadata_membership_changed"),TEXT("Blueprint graph category/interface membership changed."));int32 Index=0;for(int32 I=0;I<InterfaceCounts.Num();++I){const auto& A=B->ImplementedInterfaces[I].Graphs;if(A.Num()!=InterfaceCounts[I])return Fail(TEXT("blueprint_metadata_membership_changed"),TEXT("Interface graph count changed."));for(const auto& Entry:A)if(Entry.Get()!=InterfaceGraphs[Index++].Get())return Fail(TEXT("blueprint_metadata_membership_changed"),TEXT("Interface graph order changed."));}}}
        if(auto* G=Graph.Get()){if(G->GraphGuid!=GraphId)return Fail(TEXT("blueprint_metadata_graph_changed"),TEXT("Captured graph GUID changed."));}
        return true;
    }
    bool Pin(UObject* O){if(!Safe(TEXT("metadata_pin"))||!Loaded(O))return Fail(TEXT("blueprint_metadata_object_unavailable"),TEXT("Expected loaded object unavailable."));for(const auto& P:Pins)if(P->Hold.Get()==O)return true;if(Pins.Num()>=16384)return Fail(TEXT("blueprint_metadata_bound"),TEXT("Metadata identity bound exceeded."));Pins.Add(MakeUnique<FObject>(O));return true;}
    bool Begin(const FString& Stage){if(!Safe(Stage))return false;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);return true;}
    bool Bind(UBlueprint* B){if(!Pin(B)||!Pin(B->GetOutermost()))return false;Blueprint=B;Generated=B->GeneratedClass;Skeleton=B->SkeletonGeneratedClass;if(Generated.IsValid()&&!Pin(Generated.Get()))return false;if(Skeleton.IsValid()&&!Pin(Skeleton.Get()))return false;auto Capture=[&](const auto& A){TArray<TWeakObjectPtr<UEdGraph>> V;for(const auto& Entry:A)V.Add(Entry.Get());RootGraphs.Add(MoveTemp(V));};Capture(B->FunctionGraphs);Capture(B->MacroGraphs);Capture(B->UbergraphPages);Capture(B->EventGraphs);Capture(B->DelegateSignatureGraphs);for(const auto& I:B->ImplementedInterfaces){InterfaceCounts.Add(I.Graphs.Num());for(const auto& Entry:I.Graphs)InterfaceGraphs.Add(Entry.Get());}return Safe(TEXT("metadata_blueprint_bound"));}
};
FMetadataGuard* MetadataGuard=nullptr;
bool MetadataBefore(const FString& Stage){return !MetadataGuard||MetadataGuard->Begin(Stage);}
bool MetadataAfter(const FString& Stage){return !MetadataGuard||MetadataGuard->Safe(FString(TEXT("after_"))+Stage);}

template<class T> T* CountedLoad(const FString& Path,FResult& R)
{
    if(MetadataGuard&&(!SafeText(Path,NAME_SIZE-1)||!MetadataBefore(TEXT("LoadObject")))){if(R.Error.IsEmpty())MetadataGuard->Fail(TEXT("blueprint_metadata_path_bound"),TEXT("Raw/expanded FName path exceeds NAME_SIZE or contains embedded NUL."));return nullptr;}
    Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));if(MetadataGuard)R.Data->SetBoolField(TEXT("last_load_returned_nonnull"),O!=nullptr);if(!MetadataAfter(TEXT("LoadObject")))return nullptr;return O;
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
    if(ObjectPath.IsEmpty()||!MetadataBefore(TEXT("ResolveLevelBlueprint"))) { return nullptr; }
    UWorld* World=FindObject<UWorld>(nullptr,*ObjectPath);
    if(!World) { World=CountedLoad<UWorld>(ObjectPath,R); }
    if(!Loaded(World)||!Loaded(World->PersistentLevel)) { return nullptr; }
    // Actual SDK argument is bDontCreate. Preserve the original true=no-create behavior.
    if(MetadataGuard&&(!MetadataGuard->Pin(World)||!MetadataGuard->Pin(World->PersistentLevel)))return nullptr;
    ULevelScriptBlueprint* B=World->PersistentLevel->GetLevelScriptBlueprint(true);if(!MetadataAfter(TEXT("ResolveLevelBlueprint")))return nullptr;if(B)return B;
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

    if(!MetadataBefore(TEXT("AssetRegistryModule")))return CandidatePaths;
    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if(!MetadataAfter(TEXT("AssetRegistryModule"))||!MetadataBefore(TEXT("GetBlueprintAssets")))return CandidatePaths;
    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);
    if(!MetadataAfter(TEXT("GetBlueprintAssets")))return CandidatePaths;

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
FString NormalizeGraphType(const FString& GraphType)
{
    if (GraphType.IsEmpty())
    {
        return FString();
    }

    FString Normalized = GraphType.ToLower();
    Normalized.ReplaceInline(TEXT(" "), TEXT(""));
    Normalized.ReplaceInline(TEXT("-"), TEXT("_"));

    if (Normalized == TEXT("eventgraph") || Normalized == TEXT("event_graph") || Normalized == TEXT("ubergraph") || Normalized == TEXT("uber_graph"))
    {
        return TEXT("event");
    }
    if (Normalized == TEXT("functiongraph") || Normalized == TEXT("function_graph"))
    {
        return TEXT("function");
    }
    if (Normalized == TEXT("macrograph") || Normalized == TEXT("macro_graph"))
    {
        return TEXT("macro");
    }

    return Normalized;
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
bool GraphMatchesSelector(const UBlueprint* Blueprint, const UEdGraph* Graph, const FString& GraphId, const FString& GraphName, const FString& GraphType)
{
    if (!Loaded(Graph))
    {
        return false;
    }

    if (!GraphId.IsEmpty())
    {
        FGuid ParsedGraphId;
        if (FGuid::Parse(GraphId, ParsedGraphId))
        {
            if (Graph->GraphGuid != ParsedGraphId)
            {
                return false;
            }
        }
        else if (Graph->GraphGuid.ToString() != GraphId)
        {
            return false;
        }
    }

    if (!GraphName.IsEmpty() && !Graph->GetName().Equals(GraphName, ESearchCase::IgnoreCase))
    {
        return false;
    }

    const FString NormalizedType = NormalizeGraphType(GraphType);
    if (!NormalizedType.IsEmpty() && NormalizedType != TEXT("any") && GetBlueprintGraphType(Blueprint, Graph) != NormalizedType)
    {
        return false;
    }

    return true;
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
    if(MetadataGuard)
    {
        if(!MetadataBefore(TEXT("Graph_GetSchema")))return GraphObject;const UEdGraphSchema* Schema=Graph->GetSchema();if(!MetadataAfter(TEXT("Graph_GetSchema")))return GraphObject;GraphObject->SetStringField(TEXT("schema"),Schema?Schema->GetClass()->GetName():FString());
    }
    else GraphObject->SetStringField(TEXT("schema"), Graph->GetSchema() ? Graph->GetSchema()->GetClass()->GetName() : FString());
    GraphObject->SetNumberField(TEXT("node_count"), Graph->Nodes.Num());
    return GraphObject;
}
FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, const FString& DefaultValue = FString())
{
    if (!Params.IsValid())
    {
        return DefaultValue;
    }

    FString Value;
    return Params->TryGetStringField(FieldName, Value) ? Value : DefaultValue;
}
bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefaultValue)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return bDefaultValue;
    }
    return Params->GetBoolField(FieldName);
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
bool MatchesNodeFilter(UEdGraphNode* Node, const FString& NodeType, const FString& TitleContains)
{
    if (!Node)
    {
        return false;
    }

    if (!NodeType.IsEmpty())
    {
        const FString ClassName = Node->GetClass()->GetName();
        const FString NodeTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (!ClassName.Contains(NodeType) && !NodeTitle.Contains(NodeType))
        {
            return false;
        }
    }

    if (!TitleContains.IsEmpty() && !Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString().Contains(TitleContains))
    {
        return false;
    }

    return true;
}

bool EnhancedNode(const UEdGraphNode* N)
{
    UClass* C=FindObject<UClass>(nullptr,TEXT("/Script/InputBlueprintNodes.K2Node_EnhancedInputAction"));return Loaded(C)&&N->IsA(C);
}
UObject* EnhancedAction(const UEdGraphNode* N)
{
    auto P=FindFProperty<FObjectPropertyBase>(N->GetClass(),TEXT("InputAction"));
    if(!P) { NoteUnsafe(TEXT("EnhancedInput node InputAction property is unavailable"));return nullptr; }
    Count(*CurrentResult,TEXT("reflected_property_reads"));UObject* O=P->GetObjectPropertyValue_InContainer(N);if(O&&!Loaded(O)) { NoteUnsafe(TEXT("Enhanced InputAction reference is not fully loaded; metadata prefix retained without complete observation")); }return IsValid(O)?O:nullptr;
}
bool SelectedDelegateGuid(const UK2Node_CreateDelegate* N,FGuid& Guid)
{
    auto P=FindFProperty<FStructProperty>(N->GetClass(),TEXT("SelectedFunctionGuid"));
    if(!P||P->Struct!=TBaseStructure<FGuid>::Get()||P->ElementSize!=sizeof(FGuid)) { NoteUnsafe(TEXT("CreateDelegate SelectedFunctionGuid exact reflected FGuid property unavailable"));return false; }
    Count(*CurrentResult,TEXT("reflected_property_reads"));Guid=*P->ContainerPtrToValuePtr<FGuid>(N);return true;
}
UFunction* ObservedFunction(const UK2Node_CallFunction* N) { Count(*CurrentResult,TEXT("function_target_get_calls"));UFunction* F=N->GetTargetFunction();if(F&&!Loaded(F)) { NoteUnsafe(TEXT("Resolved function is not fully loaded")); }return IsValid(F)?F:nullptr; }
UClass* ObservedVariableClass(const UK2Node_Variable* N) { Count(*CurrentResult,TEXT("variable_source_get_calls"));UClass* C=N->GetVariableSourceClass();if(C&&!Loaded(C)) { NoteUnsafe(TEXT("Resolved variable source class is not fully loaded")); }return IsValid(C)?C:nullptr; }
FProperty* ObservedVariableProperty(const UK2Node_Variable* N) { Count(*CurrentResult,TEXT("variable_property_get_calls"));return N->GetPropertyForVariable(); }

TSharedPtr<FJsonObject> PinToJson(UEdGraphPin* Pin)
{
    TSharedPtr<FJsonObject> PinObject = MakeShared<FJsonObject>();
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
void AddUniqueJsonString(TArray<TSharedPtr<FJsonValue>>& Values, TSet<FString>& SeenValues, const FString& Value, int32 MaxValues, bool& bOutTruncated)
{
    FString TrimmedValue = Value;
    TrimmedValue.TrimStartAndEndInline();
    if (TrimmedValue.IsEmpty() || SeenValues.Contains(TrimmedValue))
    {
        return;
    }

    SeenValues.Add(TrimmedValue);
    if (MaxValues >= 0 && Values.Num() >= MaxValues)
    {
        bOutTruncated = true;
        return;
    }

    Values.Add(MakeShared<FJsonValueString>(TrimmedValue));
}
void AddObjectReferencePath(TArray<TSharedPtr<FJsonValue>>& Values, TSet<FString>& SeenValues, const UObject* Object, int32 MaxValues, bool& bOutTruncated)
{
    if (Object)
    {
        AddUniqueJsonString(Values, SeenValues, Object->GetPathName(), MaxValues, bOutTruncated);
    }
}
TSharedPtr<FJsonObject> MemberReferenceToJson(const FMemberReference& MemberReference)
{
    TSharedPtr<FJsonObject> ReferenceObject = MakeShared<FJsonObject>();
    ReferenceObject->SetStringField(TEXT("member_name"), MemberReference.GetMemberName().ToString());
    ReferenceObject->SetStringField(TEXT("member_scope"), MemberReference.GetMemberScopeName());
    ReferenceObject->SetStringField(TEXT("member_guid"), MemberReference.GetMemberGuid().IsValid() ? MemberReference.GetMemberGuid().ToString() : FString());
    ReferenceObject->SetBoolField(TEXT("is_self_context"), MemberReference.IsSelfContext());
    ReferenceObject->SetBoolField(TEXT("is_local_scope"), MemberReference.IsLocalScope());
    if (UClass* ParentClass = MemberReference.GetMemberParentClass())
    {
        ReferenceObject->SetStringField(TEXT("member_parent_class"), ParentClass->GetPathName());
    }
    return ReferenceObject;
}
FString GetBlueprintTopologyNodeSearchText(UEdGraphNode* Node)
{
    if (!Node)
    {
        return FString();
    }

    Count(*CurrentResult,TEXT("reference_search_calls"));
    TArray<FString> SearchParts;
    SearchParts.Add(Node->GetName());
    SearchParts.Add(Node->GetClass()->GetName());
    SearchParts.Add(Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString());

    if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
    {
        SearchParts.Add(FunctionNode->GetFunctionName().ToString());
        SearchParts.Add(FunctionNode->FunctionReference.GetMemberName().ToString());
        SearchParts.Add(FunctionNode->FunctionReference.GetMemberScopeName());
        if (UClass* ParentClass = FunctionNode->FunctionReference.GetMemberParentClass())
        {
            SearchParts.Add(ParentClass->GetPathName());
        }
        if (UFunction* Function = ObservedFunction(FunctionNode))
        {
            SearchParts.Add(Function->GetPathName());
            if (UClass* OwnerClass = Function->GetOwnerClass())
            {
                SearchParts.Add(OwnerClass->GetPathName());
            }
        }
    }

    if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
    {
        SearchParts.Add(VariableNode->GetVarNameString());
        SearchParts.Add(VariableNode->VariableReference.GetMemberName().ToString());
        SearchParts.Add(VariableNode->VariableReference.GetMemberScopeName());
        if (UClass* SourceClass = ObservedVariableClass(VariableNode))
        {
            SearchParts.Add(SourceClass->GetPathName());
        }
        if (FProperty* Property = ObservedVariableProperty(VariableNode))
        {
            SearchParts.Add(Property->GetName());
            SearchParts.Add(Property->GetCPPType());
            if (UStruct* OwnerStruct = Property->GetOwnerStruct())
            {
                SearchParts.Add(OwnerStruct->GetPathName());
            }
        }
    }

    if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        SearchParts.Add(EventNode->GetFunctionName().ToString());
        SearchParts.Add(EventNode->EventReference.GetMemberName().ToString());
        SearchParts.Add(EventNode->EventReference.GetMemberScopeName());
    }

    if (UK2Node_InputAction* InputActionNode = Cast<UK2Node_InputAction>(Node))
    {
        SearchParts.Add(InputActionNode->InputActionName.ToString());
    }

    if (UK2Node_InputKey* InputKeyNode = Cast<UK2Node_InputKey>(Node))
    {
        SearchParts.Add(InputKeyNode->InputKey.ToString());
    }

    if (EnhancedNode(Node))
    {
        UObject* InputAction=EnhancedAction(Node);
        if (InputAction)
        {
            SearchParts.Add(InputAction->GetPathName());
            SearchParts.Add(InputAction->GetName());
        }
    }

    if (UK2Node_DynamicCast* DynamicCastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        if (DynamicCastNode->TargetType)
        {
            SearchParts.Add(DynamicCastNode->TargetType->GetPathName());
        }
    }

    for (UEdGraphPin* Pin : Node->Pins)
    {
        if (!PinKnown(Pin))
        {
            continue;
        }
        SearchParts.Add(Pin->PinName.ToString());
        SearchParts.Add(Pin->DefaultValue);
        if (Pin->DefaultObject)
        {
            SearchParts.Add(Pin->DefaultObject->GetPathName());
        }
        if (Pin->PinType.PinSubCategoryObject.IsValid())
        {
            SearchParts.Add(Pin->PinType.PinSubCategoryObject->GetPathName());
        }
    }

    return FString::Join(SearchParts, TEXT(" "));
}
TSharedPtr<FJsonObject> BlueprintTopologyNodeToJson(UEdGraphNode* Node, bool bIncludePins, bool bIncludeReferences, int32 MaxReferencesPerNode)
{
    Count(*CurrentResult,TEXT("nodes_serialized"));
    TSharedPtr<FJsonObject> NodeObject = NodeToJson(Node, bIncludePins);
    if (!Node)
    {
        return NodeObject;
    }

    FString NodeKind = TEXT("node");
    TArray<TSharedPtr<FJsonValue>> ReferenceValues;
    TSet<FString> SeenReferences;
    bool bReferencesTruncated = false;

    auto AddReference = [&ReferenceValues, &SeenReferences, MaxReferencesPerNode, &bReferencesTruncated](const FString& Value)
    {
        AddUniqueJsonString(ReferenceValues, SeenReferences, Value, MaxReferencesPerNode, bReferencesTruncated);
    };
    auto AddObjectReference = [&ReferenceValues, &SeenReferences, MaxReferencesPerNode, &bReferencesTruncated](const UObject* Object)
    {
        AddObjectReferencePath(ReferenceValues, SeenReferences, Object, MaxReferencesPerNode, bReferencesTruncated);
    };

    if (UK2Node* K2Node = Cast<UK2Node>(Node))
    {
        NodeObject->SetBoolField(TEXT("is_k2_node"), true);
        NodeObject->SetBoolField(TEXT("is_pure"), K2Node->IsNodePure());
    }
    else
    {
        NodeObject->SetBoolField(TEXT("is_k2_node"), false);
    }

    if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
    {
        NodeKind = TEXT("call_function");
        NodeObject->SetStringField(TEXT("function_name"), FunctionNode->GetFunctionName().ToString());
        NodeObject->SetObjectField(TEXT("function_reference"), MemberReferenceToJson(FunctionNode->FunctionReference));
        if (UClass* ParentClass = FunctionNode->FunctionReference.GetMemberParentClass())
        {
            AddObjectReference(ParentClass);
        }
        if (UFunction* Function = ObservedFunction(FunctionNode))
        {
            NodeObject->SetStringField(TEXT("function_path"), Function->GetPathName());
            NodeObject->SetBoolField(TEXT("is_latent"), FunctionNode->IsLatentFunction());
            NodeObject->SetBoolField(TEXT("is_const"), Function->HasAnyFunctionFlags(FUNC_Const));
            NodeObject->SetBoolField(TEXT("is_blueprint_callable"), Function->HasAnyFunctionFlags(FUNC_BlueprintCallable));
            NodeObject->SetBoolField(TEXT("is_blueprint_pure"), Function->HasAnyFunctionFlags(FUNC_BlueprintPure));
            AddObjectReference(Function);
            if (UClass* OwnerClass = Function->GetOwnerClass())
            {
                NodeObject->SetStringField(TEXT("function_owner_class"), OwnerClass->GetPathName());
                AddObjectReference(OwnerClass);
            }
        }
    }
    else if (UK2Node_VariableGet* VariableGetNode = Cast<UK2Node_VariableGet>(Node))
    {
        NodeKind = TEXT("variable_get");
        NodeObject->SetStringField(TEXT("variable_name"), VariableGetNode->GetVarNameString());
        NodeObject->SetObjectField(TEXT("variable_reference"), MemberReferenceToJson(VariableGetNode->VariableReference));
        if (UClass* SourceClass = ObservedVariableClass(VariableGetNode))
        {
            NodeObject->SetStringField(TEXT("variable_source_class"), SourceClass->GetPathName());
            AddObjectReference(SourceClass);
        }
        if (FProperty* Property = ObservedVariableProperty(VariableGetNode))
        {
            NodeObject->SetStringField(TEXT("variable_property"), Property->GetName());
            NodeObject->SetStringField(TEXT("variable_cpp_type"), Property->GetCPPType());
            if (UStruct* OwnerStruct = Property->GetOwnerStruct())
            {
                NodeObject->SetStringField(TEXT("variable_owner_struct"), OwnerStruct->GetPathName());
                AddObjectReference(OwnerStruct);
            }
        }
    }
    else if (UK2Node_VariableSet* VariableSetNode = Cast<UK2Node_VariableSet>(Node))
    {
        NodeKind = TEXT("variable_set");
        NodeObject->SetStringField(TEXT("variable_name"), VariableSetNode->GetVarNameString());
        NodeObject->SetObjectField(TEXT("variable_reference"), MemberReferenceToJson(VariableSetNode->VariableReference));
        if (UClass* SourceClass = ObservedVariableClass(VariableSetNode))
        {
            NodeObject->SetStringField(TEXT("variable_source_class"), SourceClass->GetPathName());
            AddObjectReference(SourceClass);
        }
        if (FProperty* Property = ObservedVariableProperty(VariableSetNode))
        {
            NodeObject->SetStringField(TEXT("variable_property"), Property->GetName());
            NodeObject->SetStringField(TEXT("variable_cpp_type"), Property->GetCPPType());
            if (UStruct* OwnerStruct = Property->GetOwnerStruct())
            {
                NodeObject->SetStringField(TEXT("variable_owner_struct"), OwnerStruct->GetPathName());
                AddObjectReference(OwnerStruct);
            }
        }
    }
    else if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
    {
        NodeKind = TEXT("event");
        NodeObject->SetStringField(TEXT("event_name"), EventNode->GetFunctionName().ToString());
        NodeObject->SetObjectField(TEXT("event_reference"), MemberReferenceToJson(EventNode->EventReference));
        if (UClass* ParentClass = EventNode->EventReference.GetMemberParentClass())
        {
            AddObjectReference(ParentClass);
        }

        if (UK2Node_ComponentBoundEvent* ComponentBoundEvent = Cast<UK2Node_ComponentBoundEvent>(Node))
        {
            NodeKind = TEXT("component_bound_event");
            NodeObject->SetStringField(TEXT("delegate_property_name"), ComponentBoundEvent->DelegatePropertyName.ToString());
            NodeObject->SetStringField(TEXT("component_property_name"), ComponentBoundEvent->GetComponentPropertyName().ToString());
            AddObjectReference(ComponentBoundEvent->DelegateOwnerClass);
        }
        else if (UK2Node_ActorBoundEvent* ActorBoundEvent = Cast<UK2Node_ActorBoundEvent>(Node))
        {
            NodeKind = TEXT("actor_bound_event");
            NodeObject->SetStringField(TEXT("delegate_property_name"), ActorBoundEvent->DelegatePropertyName.ToString());
            AddObjectReference(ActorBoundEvent->DelegateOwnerClass);
            AddObjectReference(ActorBoundEvent->EventOwner);
        }
    }
    else if (UK2Node_InputAction* InputActionNode = Cast<UK2Node_InputAction>(Node))
    {
        NodeKind = TEXT("input_action");
        NodeObject->SetStringField(TEXT("input_action_name"), InputActionNode->InputActionName.ToString());
        NodeObject->SetBoolField(TEXT("consume_input"), InputActionNode->bConsumeInput != 0);
        NodeObject->SetBoolField(TEXT("execute_when_paused"), InputActionNode->bExecuteWhenPaused != 0);
        NodeObject->SetBoolField(TEXT("override_parent_binding"), InputActionNode->bOverrideParentBinding != 0);
    }
    else if (UK2Node_InputKey* InputKeyNode = Cast<UK2Node_InputKey>(Node))
    {
        NodeKind = TEXT("input_key");
        NodeObject->SetStringField(TEXT("input_key"), InputKeyNode->InputKey.ToString());
        NodeObject->SetBoolField(TEXT("control"), InputKeyNode->bControl != 0);
        NodeObject->SetBoolField(TEXT("alt"), InputKeyNode->bAlt != 0);
        NodeObject->SetBoolField(TEXT("shift"), InputKeyNode->bShift != 0);
        NodeObject->SetBoolField(TEXT("command"), InputKeyNode->bCommand != 0);
        NodeObject->SetBoolField(TEXT("consume_input"), InputKeyNode->bConsumeInput != 0);
        NodeObject->SetBoolField(TEXT("execute_when_paused"), InputKeyNode->bExecuteWhenPaused != 0);
        NodeObject->SetBoolField(TEXT("override_parent_binding"), InputKeyNode->bOverrideParentBinding != 0);
    }
    else if (EnhancedNode(Node))
    {
        NodeKind = TEXT("enhanced_input_action");
        UObject* InputAction=EnhancedAction(Node);
        if (InputAction)
        {
            NodeObject->SetStringField(TEXT("input_action"), InputAction->GetPathName());
            AddObjectReference(InputAction);
        }
    }
    else if (UK2Node_InputAxisEvent* InputAxisNode = Cast<UK2Node_InputAxisEvent>(Node))
    {
        NodeKind = TEXT("input_axis");
        NodeObject->SetStringField(TEXT("input_axis_name"), InputAxisNode->InputAxisName.ToString());
        NodeObject->SetBoolField(TEXT("consume_input"), InputAxisNode->bConsumeInput != 0);
        NodeObject->SetBoolField(TEXT("execute_when_paused"), InputAxisNode->bExecuteWhenPaused != 0);
        NodeObject->SetBoolField(TEXT("override_parent_binding"), InputAxisNode->bOverrideParentBinding != 0);
    }
    else if (UK2Node_DynamicCast* DynamicCastNode = Cast<UK2Node_DynamicCast>(Node))
    {
        NodeKind = TEXT("dynamic_cast");
        if (DynamicCastNode->TargetType)
        {
            NodeObject->SetStringField(TEXT("target_type"), DynamicCastNode->TargetType->GetPathName());
            AddObjectReference(DynamicCastNode->TargetType);
        }
    }
    else if (Node->IsA<UK2Node_AddDelegate>())
    {
        NodeKind = TEXT("delegate_bind");
    }
    else if (Node->IsA<UK2Node_RemoveDelegate>())
    {
        NodeKind = TEXT("delegate_unbind");
    }
    else if (Node->IsA<UK2Node_ClearDelegate>())
    {
        NodeKind = TEXT("delegate_clear");
    }
    else if (Node->IsA<UK2Node_CallDelegate>())
    {
        NodeKind = TEXT("delegate_call");
    }
    else if (Node->IsA<UK2Node_AssignDelegate>())
    {
        NodeKind = TEXT("delegate_assign");
    }
    else if (UK2Node_CreateDelegate* CreateDelegateNode = Cast<UK2Node_CreateDelegate>(Node))
    {
        NodeKind = TEXT("create_delegate");
        NodeObject->SetStringField(TEXT("selected_function_name"), CreateDelegateNode->GetFunctionName().ToString());
        FGuid Guid;if(SelectedDelegateGuid(CreateDelegateNode,Guid)) { NodeObject->SetStringField(TEXT("selected_function_guid"),Guid.ToString()); }else { NodeObject->SetField(TEXT("selected_function_guid"),MakeShared<FJsonValueNull>()); }
    }
    else if (UK2Node_Message* MessageNode = Cast<UK2Node_Message>(Node))
    {
        NodeKind = TEXT("interface_message");
        if (UFunction* TargetFunction = ObservedFunction(MessageNode))
        {
            NodeObject->SetStringField(TEXT("function_name"), TargetFunction->GetName());
            NodeObject->SetStringField(TEXT("function_path"), TargetFunction->GetPathName());
            AddObjectReference(TargetFunction->GetOuterUClass());
        }
    }
    else if (Node->IsA<UK2Node_CustomEvent>())
    {
        NodeKind = TEXT("custom_event");
    }
    else if (Node->IsA<UK2Node_MacroInstance>())
    {
        NodeKind = TEXT("macro_instance");
    }

    if (bIncludeReferences)
    {
        for (UEdGraphPin* Pin : Node->Pins)
        {
            if (!PinKnown(Pin))
            {
                continue;
            }
            AddObjectReference(Pin->DefaultObject);
            if (Pin->PinType.PinSubCategoryObject.IsValid())
            {
                AddObjectReference(Pin->PinType.PinSubCategoryObject.Get());
            }
        }
        NodeObject->SetArrayField(TEXT("reference_paths"), ReferenceValues);
        NodeObject->SetNumberField(TEXT("reference_count"), ReferenceValues.Num());
        NodeObject->SetBoolField(TEXT("reference_paths_truncated"), bReferencesTruncated);
    }

    NodeObject->SetStringField(TEXT("node_kind"), NodeKind);
    return NodeObject;
}
TSharedPtr<FJsonObject> BlueprintLinkToJson(const UBlueprint* Blueprint, UEdGraphPin* Pin, UEdGraphPin* LinkedPin)
{
    Count(*CurrentResult,TEXT("link_rows_serialized"));
    TSharedPtr<FJsonObject> LinkObject = MakeShared<FJsonObject>();
    if (!PinKnown(Pin) || !PinKnown(LinkedPin))
    {
        return LinkObject;
    }

    UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
    UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
    UEdGraphNode* SourceNode = SourcePin->GetOwningNodeUnchecked();
    UEdGraphNode* TargetNode = TargetPin->GetOwningNodeUnchecked();

    LinkObject->SetStringField(TEXT("source_node_id"), SourceNode->NodeGuid.ToString());
    LinkObject->SetStringField(TEXT("source_node_name"), SourceNode->GetName());
    LinkObject->SetStringField(TEXT("source_node_title"), SourceNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    LinkObject->SetStringField(TEXT("source_pin"), SourcePin->PinName.ToString());
    LinkObject->SetStringField(TEXT("target_node_id"), TargetNode->NodeGuid.ToString());
    LinkObject->SetStringField(TEXT("target_node_name"), TargetNode->GetName());
    LinkObject->SetStringField(TEXT("target_node_title"), TargetNode->GetNodeTitle(ENodeTitleType::FullTitle).ToString());
    LinkObject->SetStringField(TEXT("target_pin"), TargetPin->PinName.ToString());
    LinkObject->SetStringField(TEXT("pin_category"), SourcePin->PinType.PinCategory.ToString());
    LinkObject->SetStringField(TEXT("link_kind"), SourcePin->PinType.PinCategory == UEdGraphSchema_K2::PC_Exec ? TEXT("exec") : TEXT("data"));

    if (UEdGraph* SourceGraph = SourceNode->GetGraph())
    {
        LinkObject->SetObjectField(TEXT("source_graph"), GraphToJson(Blueprint, SourceGraph));
    }
    if (UEdGraph* TargetGraph = TargetNode->GetGraph())
    {
        LinkObject->SetObjectField(TEXT("target_graph"), GraphToJson(Blueprint, TargetGraph));
    }

    return LinkObject;
}

FString MemberState(const TArray<UEdGraph*>& Graphs,FResult& R)
{
    Count(R,TEXT("member_state_capture_calls"));TArray<TSharedPtr<FJsonValue>> Rows;
    for(UEdGraph* G:Graphs) { if(!Loaded(G)) { continue; }for(UEdGraphNode* N:G->Nodes) { if(!Loaded(N)) { continue; }
        auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("path"),N->GetPathName());J->SetStringField(TEXT("guid"),N->NodeGuid.ToString());J->SetNumberField(TEXT("x"),N->NodePosX);J->SetNumberField(TEXT("y"),N->NodePosY);
        if(const auto C=Cast<UK2Node_CallFunction>(N)) { J->SetObjectField(TEXT("function_reference"),MemberReferenceToJson(C->FunctionReference)); }
        if(const auto V=Cast<UK2Node_Variable>(N)) { J->SetObjectField(TEXT("variable_reference"),MemberReferenceToJson(V->VariableReference)); }
        if(const auto E=Cast<UK2Node_Event>(N)) { J->SetObjectField(TEXT("event_reference"),MemberReferenceToJson(E->EventReference)); }
        Rows.Add(MakeShared<FJsonValueObject>(J));
    } }
    FString Text;FJsonSerializer::Serialize(Rows,TJsonWriterFactory<>::Create(&Text));return Text;
}
bool Inventory(const TArray<UEdGraph*>& Graphs,FResult& R)
{
    Count(R,TEXT("pins_inventory_calls"));KnownPins.Reset();int32 Nodes=0,PinCount=0;
    if(Graphs.Num()>4096) { R.Error=TEXT("Actual graph traversal exceeds finite4096 safety budget");return false; }
    for(UEdGraph* G:Graphs) { if(!Loaded(G)) { if(G) { NoteUnsafe(TEXT("A Blueprint graph is not fully loaded")); }continue; }for(UEdGraphNode* N:G->Nodes) { if(!Loaded(N)) { if(N) { NoteUnsafe(TEXT("A Blueprint node is not fully loaded")); }continue; }if(++Nodes>65536) { R.Error=TEXT("Actual node traversal exceeds finite65536 safety budget");return false; }
        for(const UEdGraphPin* Pin:N->Pins) { if(!Pin) { continue; }if(++PinCount>262144) { R.Error=TEXT("Actual pin traversal exceeds finite262144 safety budget");return false; }KnownPins.Add(Pin); }
    } }return true;
}
int32 GetClampedIntParam(const TSharedPtr<FJsonObject>& Params,const FString& FieldName,int32 DefaultValue,int32 MinValue,int32 MaxValue)
{
    double V=DefaultValue;Params->TryGetNumberField(FieldName,V);return static_cast<int32>(FMath::Clamp(V,static_cast<double>(MinValue),static_cast<double>(MaxValue)));
}

TSharedPtr<FJsonObject> Run(const TSharedPtr<FJsonObject>& Params,FResult& R)
{
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    UBlueprint* Blueprint = FindBlueprintByName(BlueprintName,R);
    if (!Loaded(Blueprint))
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TStrongObjectPtr<UBlueprint> BlueprintHold(Blueprint);
    const FString GraphId = GetStringParam(Params, TEXT("graph_id"));
    const FString GraphName = GetStringParam(Params, TEXT("graph_name"));
    const FString GraphNameContains = GetStringParam(Params, TEXT("graph_name_contains"));
    const FString GraphType = GetStringParam(Params, TEXT("graph_type"));
    const FString NormalizedGraphType = NormalizeGraphType(GraphType);
    const FString NodeType = GetStringParam(Params, TEXT("node_type"));
    const FString TitleContains = GetStringParam(Params, TEXT("title_contains"));
    const FString ReferenceContains = GetStringParam(Params, TEXT("reference_contains"));
    const bool bIncludePins = GetBoolParam(Params, TEXT("include_pins"), false);
    const bool bIncludeLinks = GetBoolParam(Params, TEXT("include_links"), true);
    const bool bIncludeReferences = GetBoolParam(Params, TEXT("include_references"), true);
    const int32 MaxGraphs = GetClampedIntParam(Params, TEXT("max_graphs"), 64, -1, 4096);
    const int32 MaxNodesPerGraph = GetClampedIntParam(Params, TEXT("max_nodes_per_graph"), 512, -1, 10000);
    const int32 MaxLinksPerGraph = GetClampedIntParam(Params, TEXT("max_links_per_graph"), 1024, -1, 50000);
    const int32 MaxReferencesPerNode = GetClampedIntParam(Params, TEXT("max_references_per_node"), 64, -1, 4096);
    const bool bHasGraphSelector = !GraphId.IsEmpty() || !GraphName.IsEmpty() || !NormalizedGraphType.IsEmpty();

    TArray<UEdGraph*> AllGraphs;
    Count(R,TEXT("get_all_graphs_calls"));Blueprint->GetAllGraphs(AllGraphs);
    if(!Inventory(AllGraphs,R)) { return CreateErrorResponse(R.Error); }
    const FString Before=MemberState(AllGraphs,R);const bool DirtyBefore=Blueprint->GetOutermost()->IsDirty();
    auto ResultObj=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("legacy_result"),ResultObj);

    TArray<TSharedPtr<FJsonValue>> GraphValues;
    int32 MatchingGraphCount = 0;
    int32 IncludedGraphCount = 0;
    int32 TotalMatchingNodeCount = 0;
    int32 TotalIncludedNodeCount = 0;
    int32 TotalIncludedLinkCount = 0;
    bool bGraphsTruncated = false;

    for (UEdGraph* Graph : AllGraphs)
    {
        if (!Loaded(Graph))
        {
            continue;
        }

        if (bHasGraphSelector && !GraphMatchesSelector(Blueprint, Graph, GraphId, GraphName, GraphType))
        {
            continue;
        }
        if (!GraphNameContains.IsEmpty() && !Graph->GetName().Contains(GraphNameContains, ESearchCase::IgnoreCase))
        {
            continue;
        }

        Count(R,TEXT("graphs_visited"));++MatchingGraphCount;
        if (MaxGraphs >= 0 && IncludedGraphCount >= MaxGraphs)
        {
            bGraphsTruncated = true;
            continue;
        }

        TArray<TSharedPtr<FJsonValue>> NodeValues;
        TArray<TSharedPtr<FJsonValue>> LinkValues;
        TSet<FString> SeenLinkKeys;
        int32 MatchingNodeCount = 0;
        int32 IncludedNodeCount = 0;
        int32 LinkCountBeforeTruncation = 0;
        bool bNodesTruncated = false;
        bool bLinksTruncated = false;

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Loaded(Node))
            {
                continue;
            }
            Count(R,TEXT("nodes_visited"));
            if (!MatchesNodeFilter(Node, NodeType, TitleContains))
            {
                continue;
            }
            if (!ReferenceContains.IsEmpty() && !GetBlueprintTopologyNodeSearchText(Node).Contains(ReferenceContains, ESearchCase::IgnoreCase))
            {
                continue;
            }

            ++MatchingNodeCount;
            if (MaxNodesPerGraph >= 0 && IncludedNodeCount >= MaxNodesPerGraph)
            {
                bNodesTruncated = true;
                continue;
            }

            NodeValues.Add(MakeShared<FJsonValueObject>(BlueprintTopologyNodeToJson(Node, bIncludePins, bIncludeReferences, MaxReferencesPerNode)));
            ++IncludedNodeCount;

            if (bIncludeLinks)
            {
                for (UEdGraphPin* Pin : Node->Pins)
                {
                    if (!PinKnown(Pin))
                    {
                        continue;
                    }
                    for (UEdGraphPin* LinkedPin : Pin->LinkedTo)
                    {
                        if (!PinKnown(LinkedPin))
                        {
                            continue;
                        }

                        UEdGraphPin* SourcePin = Pin->Direction == EGPD_Output ? Pin : LinkedPin;
                        UEdGraphPin* TargetPin = Pin->Direction == EGPD_Output ? LinkedPin : Pin;
                        if (!PinKnown(SourcePin) || !PinKnown(TargetPin))
                        {
                            continue;
                        }

                        const FString LinkKey = FString::Printf(
                            TEXT("%s:%s->%s:%s"),
                            *SourcePin->GetOwningNodeUnchecked()->NodeGuid.ToString(),
                            *SourcePin->PinName.ToString(),
                            *TargetPin->GetOwningNodeUnchecked()->NodeGuid.ToString(),
                            *TargetPin->PinName.ToString());
                        if (SeenLinkKeys.Contains(LinkKey))
                        {
                            continue;
                        }
                        SeenLinkKeys.Add(LinkKey);
                        ++LinkCountBeforeTruncation;

                        if (MaxLinksPerGraph >= 0 && LinkValues.Num() >= MaxLinksPerGraph)
                        {
                            bLinksTruncated = true;
                            continue;
                        }

                        LinkValues.Add(MakeShared<FJsonValueObject>(BlueprintLinkToJson(Blueprint, Pin, LinkedPin)));
                    }
                }
            }
        }

        TSharedPtr<FJsonObject> GraphObject = GraphToJson(Blueprint, Graph);
        GraphObject->SetArrayField(TEXT("nodes"), NodeValues);
        GraphObject->SetArrayField(TEXT("links"), LinkValues);
        GraphObject->SetNumberField(TEXT("matching_node_count"), MatchingNodeCount);
        GraphObject->SetNumberField(TEXT("included_node_count"), IncludedNodeCount);
        GraphObject->SetNumberField(TEXT("included_link_count"), LinkValues.Num());
        GraphObject->SetNumberField(TEXT("matching_link_count"), LinkCountBeforeTruncation);
        GraphObject->SetBoolField(TEXT("nodes_truncated"), bNodesTruncated);
        GraphObject->SetBoolField(TEXT("links_truncated"), bLinksTruncated);
        GraphValues.Add(MakeShared<FJsonValueObject>(GraphObject));

        ++IncludedGraphCount;
        TotalMatchingNodeCount += MatchingNodeCount;
        TotalIncludedNodeCount += IncludedNodeCount;
        TotalIncludedLinkCount += LinkValues.Num();
    }

    const FString After=MemberState(AllGraphs,R);const bool DirtyAfter=Blueprint->GetOutermost()->IsDirty();
    const bool Preserved=Before.Equals(After,ESearchCase::CaseSensitive)&&DirtyBefore==DirtyAfter;
    R.Data->SetBoolField(TEXT("selected_member_position_and_dirty_state_preserved"),Preserved);R.Data->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);R.Data->SetBoolField(TEXT("package_dirty_after"),DirtyAfter);
    if(!Preserved) { R.bChanged=true;NoteUnsafe(TEXT("SDK getter resolution changed selected member/position/dirty state; returned topology prefix is preserved without a zero-change claim")); }
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObj->SetStringField(TEXT("blueprint_path"), Blueprint->GetPathName());
    ResultObj->SetStringField(TEXT("graph_id"), GraphId);
    ResultObj->SetStringField(TEXT("graph_name"), GraphName);
    ResultObj->SetStringField(TEXT("graph_name_contains"), GraphNameContains);
    ResultObj->SetStringField(TEXT("graph_type"), GraphType);
    ResultObj->SetStringField(TEXT("node_type"), NodeType);
    ResultObj->SetStringField(TEXT("title_contains"), TitleContains);
    ResultObj->SetStringField(TEXT("reference_contains"), ReferenceContains);
    ResultObj->SetBoolField(TEXT("include_pins"), bIncludePins);
    ResultObj->SetBoolField(TEXT("include_links"), bIncludeLinks);
    ResultObj->SetBoolField(TEXT("include_references"), bIncludeReferences);
    ResultObj->SetNumberField(TEXT("matching_graph_count"), MatchingGraphCount);
    ResultObj->SetNumberField(TEXT("included_graph_count"), IncludedGraphCount);
    ResultObj->SetNumberField(TEXT("matching_node_count"), TotalMatchingNodeCount);
    ResultObj->SetNumberField(TEXT("included_node_count"), TotalIncludedNodeCount);
    ResultObj->SetNumberField(TEXT("included_link_count"), TotalIncludedLinkCount);
    ResultObj->SetBoolField(TEXT("graphs_truncated"), bGraphsTruncated);
    ResultObj->SetBoolField(TEXT("read_only"), true);
    ResultObj->SetStringField(TEXT("scope_note"), TEXT("Static Blueprint graph topology only. This does not prove runtime execution order or whether a branch fired in PIE/SIE."));
    ResultObj->SetArrayField(TEXT("graphs"), GraphValues);
    return ResultObj;
}

}
Variable::FResult Inspect(const FString& BlueprintName,const FString& GraphName,const FString& GraphId,const FString& GraphType,const FString& GraphNameContains,const FString& NodeType,const FString& TitleContains,const FString& ReferenceContains,bool bIncludePins,bool bIncludeLinks,bool bIncludeReferences,int64 MaxGraphs,int64 MaxNodesPerGraph,int64 MaxLinksPerGraph,int64 MaxReferencesPerNode)
{
    FResult R;Init(R);
    if(!IsEditorGameThread()||CurrentResult) { R.ErrorCode=TEXT("editor_game_thread_or_observer_busy");R.Error=TEXT("Editor game thread/nonreentrant observer required");return R; }
    for(const FString* S:{&BlueprintName,&GraphName,&GraphId,&GraphType,&GraphNameContains,&NodeType,&TitleContains,&ReferenceContains}) { if(!SafeText(*S,65536)) { R.ErrorCode=TEXT("topology_text_scope_invalid");R.Error=TEXT("Bounded filters without interior NUL required");return R; } }
    CurrentResult=&R;struct FReset { ~FReset() { CurrentResult=nullptr;KnownPins.Reset(); } } Reset;
    auto P=MakeShared<FJsonObject>();
    P->SetStringField(TEXT("blueprint_name"),BlueprintName);
    P->SetStringField(TEXT("graph_name"),GraphName);
    P->SetStringField(TEXT("graph_id"),GraphId);
    P->SetStringField(TEXT("graph_type"),GraphType);
    P->SetStringField(TEXT("graph_name_contains"),GraphNameContains);
    P->SetStringField(TEXT("node_type"),NodeType);
    P->SetStringField(TEXT("title_contains"),TitleContains);
    P->SetStringField(TEXT("reference_contains"),ReferenceContains);
    P->SetBoolField(TEXT("include_pins"),bIncludePins);
    P->SetBoolField(TEXT("include_links"),bIncludeLinks);
    P->SetBoolField(TEXT("include_references"),bIncludeReferences);
    P->SetNumberField(TEXT("max_graphs"),static_cast<int32>(FMath::Clamp<int64>(MaxGraphs,-1,4096)));
    P->SetNumberField(TEXT("max_nodes_per_graph"),static_cast<int32>(FMath::Clamp<int64>(MaxNodesPerGraph,-1,10000)));
    P->SetNumberField(TEXT("max_links_per_graph"),static_cast<int32>(FMath::Clamp<int64>(MaxLinksPerGraph,-1,50000)));
    P->SetNumberField(TEXT("max_references_per_node"),static_cast<int32>(FMath::Clamp<int64>(MaxReferencesPerNode,-1,4096)));

    R.Stage=TEXT("observe_topology");auto Legacy=Run(P,R);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);R.bSuccess=R.bPostconditionPassed=Success&&!R.Data->GetBoolField(TEXT("observation_incomplete"));
    if(!R.bSuccess) { if(R.Error.IsEmpty()) { Legacy->TryGetStringField(TEXT("error"),R.Error); }if(R.ErrorCode.IsEmpty()) { R.ErrorCode=TEXT("blueprint_topology_observation_failed"); }R.Stage=TEXT("observation_incomplete"); }else { R.Stage=TEXT("complete"); }
    R.Data->SetBoolField(TEXT("explicit_ue_modify_compile_save_calls"),false);R.Data->SetBoolField(TEXT("direct_read_only"),true);return R;
}
// HL: no node/pin/virtual title inventory is requested by graph metadata listing.
UBlueprint* ResolveBlueprintForMetadata(const FString& Name,Variable::FResult& R)
{
    Init(R);if(!IsInGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||MetadataGuard||CurrentResult){R.ErrorCode=TEXT("blueprint_metadata_context_unavailable");R.Error=TEXT("Nonreentrant safe Editor context required.");return nullptr;}
    FMetadataGuard Guard(R);TGuardValue<FMetadataGuard*> Scope(MetadataGuard,&Guard);if(!Guard.Safe(TEXT("resolve_blueprint_metadata"))||!SafeText(Name,NAME_SIZE-1)){if(R.Error.IsEmpty())Guard.Fail(TEXT("blueprint_metadata_name_bound"),TEXT("Blueprint name must fit NAME_SIZE without embedded NUL."));return nullptr;}
    UBlueprint* B=FindBlueprintByName(Name,R);if(!Guard.Safe(TEXT("resolved_blueprint_metadata")))return nullptr;if(!Loaded(B)){Guard.Fail(TEXT("blueprint_not_found"),FString::Printf(TEXT("Blueprint not found: %s"),*Name));return nullptr;}if(!Guard.Bind(B))return nullptr;return B;
}
Variable::FResult ListGraphMetadata(const FString& Name,const FString& Type)
{
    FResult R;Init(R);R.Compiled=false;R.Data->SetStringField(TEXT("operation"),TEXT("list_blueprint_graphs"));R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
    if(!IsInGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||MetadataGuard||CurrentResult){R.ErrorCode=TEXT("blueprint_metadata_context_unavailable");R.Error=TEXT("Nonreentrant safe Editor context required.");return R;}
    FMetadataGuard Guard(R);TGuardValue<FMetadataGuard*> Scope(MetadataGuard,&Guard);if(!Guard.Safe(TEXT("list_graph_metadata_preflight"))||!SafeText(Name,NAME_SIZE-1)||!SafeText(Type,65536)){if(R.Error.IsEmpty())Guard.Fail(TEXT("blueprint_metadata_input_bound"),TEXT("Bounded name/type without embedded NUL required."));return R;}
    UBlueprint* B=FindBlueprintByName(Name,R);if(!Guard.Safe(TEXT("after_resolve_blueprint")))return R;if(!Loaded(B)){Guard.Fail(TEXT("blueprint_not_found"),FString::Printf(TEXT("Blueprint not found: %s"),*Name));return R;}if(!Guard.Bind(B))return R;
    auto Legacy=MakeShared<FJsonObject>();Legacy->SetStringField(TEXT("blueprint"),B->GetPathName());R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    const bool DirtyBefore=B->GetOutermost()->IsDirty();TArray<UEdGraph*> Graphs;if(!Guard.Begin(TEXT("GetAllGraphs")))return R;Count(R,TEXT("get_all_graphs_calls"));B->GetAllGraphs(Graphs);if(!Guard.Safe(TEXT("after_GetAllGraphs")))return R;
    if(Graphs.Num()>16384){Guard.Fail(TEXT("blueprint_metadata_graph_bound"),TEXT("Graph metadata bound exceeded; no truncation disguised as full list."));return R;}
    TArray<TSharedPtr<FJsonValue>> Rows;const FString Filter=NormalizeGraphType(Type);TArray<FGuid> Ids;TArray<int32> NodeCounts;for(auto* G:Graphs){Ids.Add(G?G->GraphGuid:FGuid());NodeCounts.Add(G?G->Nodes.Num():0);if(G&&!Guard.Pin(G))return R;}
    auto Categories=[&](){FString S;for(auto* G:Graphs)if(G)S+=G->GetPathName()+TEXT("|")+GetBlueprintGraphType(B,G)+TEXT("\n");return S;};const FString CategoriesBefore=Categories();
    for(auto* G:Graphs){if(!G)continue;if(!Guard.Safe(TEXT("graph_metadata_row")))return R;const FString CurrentType=GetBlueprintGraphType(B,G);if(!Filter.IsEmpty()&&Filter!=TEXT("any")&&CurrentType!=Filter)continue;Guard.Graph=G;Guard.GraphId=G->GraphGuid;auto Row=GraphToJson(B,G);if(!Guard.Safe(TEXT("after_graph_metadata_row")))return R;Rows.Add(MakeShared<FJsonValueObject>(Row));Legacy->SetArrayField(TEXT("graphs"),Rows);}
    Guard.Graph.Reset();if(!Guard.Safe(TEXT("final_graph_metadata")))return R;for(int32 I=0;I<Graphs.Num();++I){auto* G=Graphs[I];if(G&&(G->GraphGuid!=Ids[I]||G->Nodes.Num()!=NodeCounts[I])){Guard.Fail(TEXT("blueprint_metadata_graph_changed"),TEXT("Graph GUID/node-count changed during metadata observation; original partial rows retained."));return R;}}
    if(!Categories().Equals(CategoriesBefore,ESearchCase::CaseSensitive)||B->GetOutermost()->IsDirty()!=DirtyBefore){Guard.Fail(TEXT("blueprint_metadata_observation_changed"),TEXT("Graph category/target dirty state changed during metadata observation; partial metadata retained."));return R;}
    Legacy->SetArrayField(TEXT("graphs"),Rows);R.bSuccess=R.bPostconditionPassed=true;R.bChangeStateKnown=true;R.Stage=TEXT("complete");R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),true);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetStringField(TEXT("scope"),TEXT("Original GetAllGraphs nonnull/type/metadata5 only; no node/pin observer or global SDK purity proof."));return R;
}

// HM: original resolver result and narrow graph metadata access. Existing observer and HL paths are unchanged.
Variable::FResult ResolveBlueprintLegacy(const FString& Name)
{
    FResult R;Init(R);R.Compiled=false;R.Data->SetStringField(TEXT("operation"),TEXT("resolve_blueprint"));R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);
    if(!IsInGameThread()||!GEditor||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||MetadataGuard||CurrentResult){R.ErrorCode=TEXT("blueprint_metadata_context_unavailable");R.Error=TEXT("Nonreentrant safe Editor context required.");return R;}
    FMetadataGuard Guard(R);TGuardValue<FMetadataGuard*> Scope(MetadataGuard,&Guard);
    if(!Guard.Safe(TEXT("resolve_blueprint_candidates"))||!SafeText(Name,NAME_SIZE-1)){if(R.Error.IsEmpty())Guard.Fail(TEXT("blueprint_metadata_name_bound"),TEXT("Blueprint name must fit NAME_SIZE without embedded NUL."));return R;}
    auto Legacy=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("legacy_result"),Legacy);
    const auto Paths=FindBlueprintAssetPaths(Name,R);TArray<TSharedPtr<FJsonValue>> Candidates;for(const auto& Path:Paths)Candidates.Add(MakeShared<FJsonValueString>(Path));Legacy->SetArrayField(TEXT("candidates"),Candidates);
    if(!Guard.Safe(TEXT("after_resolve_candidates")))return R;
    UBlueprint* B=FindBlueprintByName(Name,R);if(!Guard.Safe(TEXT("after_resolve_blueprint")))return R;
    if(B&&!Guard.Bind(B))return R;Legacy->SetBoolField(TEXT("resolved"),B!=nullptr);if(B){Legacy->SetStringField(TEXT("asset_path"),B->GetPathName());Legacy->SetStringField(TEXT("name"),B->GetName());}
    R.bSuccess=R.bPostconditionPassed=true;R.bChangeStateKnown=true;R.Stage=TEXT("complete");R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),true);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetStringField(TEXT("scope"),TEXT("Original candidate search then resolver; resolved=false/ambiguity is a normal result. No explicit asset writer/compile/save; implicit load/cache effects are not a global purity proof."));return R;
}
UEdGraph* FindGraphForMetadata(UBlueprint* B,const FString& Id,const FString& Name,const FString& Type,Variable::FResult& R)
{
    if(MetadataGuard||CurrentResult){R.ErrorCode=TEXT("blueprint_metadata_reentrant");R.Error=TEXT("Metadata helper is already active.");return nullptr;}
    FMetadataGuard Guard(R);TGuardValue<FMetadataGuard*> Scope(MetadataGuard,&Guard);if(!Guard.Safe(TEXT("select_graph_metadata"))||!Guard.Bind(B))return nullptr;
    TArray<UEdGraph*> Graphs;if(!Guard.Begin(TEXT("GetAllGraphs")))return nullptr;Count(R,TEXT("get_all_graphs_calls"));B->GetAllGraphs(Graphs);if(!Guard.Safe(TEXT("after_GetAllGraphs")))return nullptr;
    if(Graphs.Num()>16384){Guard.Fail(TEXT("blueprint_metadata_graph_bound"),TEXT("Graph selection bound exceeded."));return nullptr;}
    for(auto* G:Graphs){if(!G)continue;if(!Guard.Pin(G))return nullptr;if(GraphMatchesSelector(B,G,Id,Name,Type))return G;}return nullptr;
}
TSharedPtr<FJsonObject> DescribeGraphMetadata(UBlueprint* B,UEdGraph* G,Variable::FResult& R)
{
    if(MetadataGuard||CurrentResult){R.ErrorCode=TEXT("blueprint_metadata_reentrant");R.Error=TEXT("Metadata helper is already active.");return nullptr;}
    FMetadataGuard Guard(R);TGuardValue<FMetadataGuard*> Scope(MetadataGuard,&Guard);if(!Guard.Safe(TEXT("describe_graph_metadata"))||!Guard.Bind(B)||!Guard.Pin(G))return nullptr;Guard.Graph=G;Guard.GraphId=G->GraphGuid;
    auto Row=GraphToJson(B,G);if(!Guard.Safe(TEXT("after_describe_graph_metadata")))return nullptr;return Row;
}

}
