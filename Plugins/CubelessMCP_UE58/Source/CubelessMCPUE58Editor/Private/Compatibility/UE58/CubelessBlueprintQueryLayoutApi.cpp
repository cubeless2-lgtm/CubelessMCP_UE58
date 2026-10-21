#include "Compatibility/UE58/CubelessBlueprintQueryLayoutApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphSchema.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "K2Node_Event.h"
#include "EdGraphSchema_K2.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EditorAssetLibrary.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonSerializer.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
namespace Cubeless::UE58::BlueprintQueryLayout
{
namespace
{
using Variable::FResult;
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Count(FResult& R,const TCHAR* Key,int32 N=1) { auto C=R.Data->GetObjectField(TEXT("native_call_counts"));C->SetNumberField(Key,C->GetNumberField(Key)+N); }
void Init(FResult& R,const TCHAR* Operation)
{
    auto C=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("graph_resolve_calls"),TEXT("graph_resolve_returns"),TEXT("get_all_graphs_calls"),TEXT("json_parse_calls"),TEXT("json_parse_returns"),TEXT("find_node_guid_calls"),TEXT("node_title_calls"),TEXT("graph_modify_calls"),TEXT("node_modify_calls"),TEXT("position_field_write_calls"),TEXT("mark_modified_calls"),TEXT("full_compile_calls"),TEXT("full_compile_returns"),TEXT("forced_save_calls"),TEXT("forced_save_returns"),TEXT("event_graph_create_calls"),TEXT("event_graph_create_returns"),TEXT("add_ubergraph_page_calls"),TEXT("event_nodes_visited"),TEXT("position_rows_validated"),TEXT("position_rows_read_back")}) { C->SetNumberField(Key,0); }
    R.Data->SetObjectField(TEXT("native_call_counts"),C);R.Data->SetStringField(TEXT("operation"),Operation);R.Data->SetBoolField(TEXT("read_only"),true);R.Data->SetBoolField(TEXT("automatic_replay_allowed"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("runtime_cold_pipeline_verified"),false);R.Data->SetStringField(TEXT("count_scope"),TEXT("Direct calls only; LoadObject, GetNodeTitle, AddUbergraphPage, compiler and save transitive SDK effects are not total-zero claims."));
}
TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);return J;
}
bool JsonBounded(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Visited)
{
    if(!V||Depth>16||++Visited>65536) { return false; }
    if(V->Type==EJson::Number) { return FMath::IsFinite(V->AsNumber()); }
    if(V->Type==EJson::String) { return SafeText(V->AsString(),1048576); }
    if(V->Type==EJson::Array) { for(const auto& X:V->AsArray()) { if(!JsonBounded(X,Depth+1,Visited)) { return false; } } }
    if(V->Type==EJson::Object) { for(const auto& Pair:V->AsObject()->Values) { if(!SafeText(FString(Pair.Key.ToView()),65536)||!JsonBounded(Pair.Value,Depth+1,Visited)) { return false; } } }
    return true;
}
bool Decode(const FString& Text,TSharedPtr<FJsonValue>& Value,FResult& R)
{
    if(Text.IsEmpty()||!SafeText(Text,1048576)) { R.Error=TEXT("Private JSON must be bounded 1..1Mi UTF16 characters with no interior NUL");return false; }
    Count(R,TEXT("json_parse_calls"));const FString Wrapped=TEXT("[")+Text+TEXT("]");auto Reader=TJsonReaderFactory<>::Create(Wrapped);TArray<bool> Objects;TArray<TSet<FString>> Keys;EJsonNotation Token;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error) { R.Error=TEXT("Malformed private JSON");Count(R,TEXT("json_parse_returns"));return false; }
        if(Token!=EJsonNotation::ArrayEnd&&Token!=EJsonNotation::ObjectEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString Key=Reader->GetIdentifier();if(Keys.Last().Contains(Key)) { R.Error=TEXT("Duplicate JSON key");Count(R,TEXT("json_parse_returns"));return false; }Keys.Last().Add(Key); }
        if(Token==EJsonNotation::ArrayStart||Token==EJsonNotation::ObjectStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>18) { R.Error=TEXT("JSON nesting budget exceeded");Count(R,TEXT("json_parse_returns"));return false; } }
        else if(Token==EJsonNotation::ArrayEnd||Token==EJsonNotation::ObjectEnd) { if(Objects.IsEmpty()) { R.Error=TEXT("Invalid JSON nesting");Count(R,TEXT("json_parse_returns"));return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> Values;int32 Visited=0;const bool OK=FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Wrapped),Values)&&Values.Num()==1&&JsonBounded(Values[0],0,Visited);Count(R,TEXT("json_parse_returns"));
    if(!OK) { R.Error=TEXT("One bounded JSON value required; depth16/values65536/finite numbers");return false; }Value=Values[0];return true;
}
template<class T> T* CountedLoad(const FString& Path,FResult& R)
{
    Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));return O;
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
UEdGraph* FindBlueprintGraph(UBlueprint* Blueprint, const FString& GraphId, const FString& GraphName, const FString& GraphType)
{
    if (!Loaded(Blueprint))
    {
        return nullptr;
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    for (UEdGraph* Graph : Graphs)
    {
        if (GraphMatchesSelector(Blueprint, Graph, GraphId, GraphName, GraphType))
        {
            return Graph;
        }
    }

    return nullptr;
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
void AddGraphField(TSharedPtr<FJsonObject> ResultObj, const UBlueprint* Blueprint, const UEdGraph* Graph)
{
    if (ResultObj.IsValid() && Graph)
    {
        ResultObj->SetObjectField(TEXT("graph"), GraphToJson(Blueprint, Graph));
    }
}
FString GetBlueprintNodeStatusName(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Unknown:
        return TEXT("unknown");
    case BS_Dirty:
        return TEXT("dirty");
    case BS_Error:
        return TEXT("error");
    case BS_UpToDate:
        return TEXT("up_to_date");
    case BS_BeingCreated:
        return TEXT("being_created");
    case BS_UpToDateWithWarnings:
        return TEXT("up_to_date_with_warnings");
    default:
        return TEXT("unknown");
    }
}
bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const FString& FieldName, bool bDefaultValue)
{
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return bDefaultValue;
    }
    return Params->GetBoolField(FieldName);
}

UEdGraph* ResolveBlueprintGraph(UBlueprint* BP,const TSharedPtr<FJsonObject>& Params,bool,FString& Error,FResult& R,bool AllowDefaultCreate)
{
    Count(R,TEXT("graph_resolve_calls"));
    FString Id,Name,Type;Params->TryGetStringField(TEXT("graph_id"),Id);Params->TryGetStringField(TEXT("graph_name"),Name);Params->TryGetStringField(TEXT("graph_type"),Type);
    UEdGraph* G=nullptr;
    if(!SafeText(Id,128)||!SafeText(Name,1024)||!SafeText(Type,128)) { Error=TEXT("Bounded graph selectors without interior NUL required"); }
    else if(Id.IsEmpty()&&Name.IsEmpty()&&NormalizeGraphType(Type).IsEmpty())
    {
        for(UEdGraph* Page:BP->UbergraphPages) { if(Loaded(Page)&&Page->GetName().Contains(TEXT("EventGraph"))) { G=Page;break; } }
        if(!G&&AllowDefaultCreate)
        {
            if(!BP->GetOutermost()->GetName().StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive)) { Error=TEXT("Implicit default EventGraph creation is limited to owned /Game Blueprint packages"); }
            else
            {
                R.Stage=TEXT("create_default_event_graph");R.bMutationAttempted=true;R.Data->SetBoolField(TEXT("read_only"),false);Count(R,TEXT("event_graph_create_calls"));
                G=FBlueprintEditorUtils::CreateNewGraph(BP,FName(TEXT("EventGraph")),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());Count(R,TEXT("event_graph_create_returns"));
                R.Data->SetBoolField(TEXT("default_event_graph_returned"),G!=nullptr);if(G) { R.Data->SetStringField(TEXT("default_event_graph_path"),G->GetPathName());R.bChanged=true; }
                if(Loaded(G)) { Count(R,TEXT("add_ubergraph_page_calls"));FBlueprintEditorUtils::AddUbergraphPage(BP,G);R.Data->SetBoolField(TEXT("default_event_graph_attached"),BP->UbergraphPages.Contains(G));R.Data->SetBoolField(TEXT("package_dirty_after_default_graph"),BP->GetOutermost()->IsDirty());if(!Loaded(G)||G->GetOuter()!=BP||!BP->UbergraphPages.Contains(G)) { Error=TEXT("Created default EventGraph could not be verified as attached to its Blueprint");R.bChangeStateKnown=false;G=nullptr; } }
                else { Error=TEXT("Failed to get event graph");R.bChangeStateKnown=false; }
            }
        }
        if(!G&&Error.IsEmpty()) { Error=TEXT("Failed to get event graph"); }
    }
    else
    {
        TArray<UEdGraph*> All;Count(R,TEXT("get_all_graphs_calls"));BP->GetAllGraphs(All);
        if(All.Num()>256) { Error=TEXT("Blueprint graph count exceeds finite 256 scope"); }
        else { for(UEdGraph* Candidate:All) { if(Loaded(Candidate)&&GraphMatchesSelector(BP,Candidate,Id,Name,Type)) { G=Candidate;break; } } }
        if(!G&&Error.IsEmpty()) { Error=FString::Printf(TEXT("Blueprint graph not found. graph_name='%s', graph_id='%s', graph_type='%s'"),*Name,*Id,*Type); }
    }
    if(Loaded(G)&&G->Nodes.Num()>16384) { G=nullptr;Error=TEXT("Graph node count exceeds finite 16384 scope"); }
    Count(R,TEXT("graph_resolve_returns"));return Loaded(G)?G:nullptr;
}
UBlueprint* FindBlueprint(const FString& Name,FResult& R)
{
    if(!SafeText(Name,4096)) { R.Error=TEXT("Bounded Blueprint name without interior NUL required");return nullptr; }
    UBlueprint* BP=FindBlueprintByName(Name,R);return Loaded(BP)?BP:nullptr;
}

constexpr int32 MaxBlueprintNodePositionUpdates=2048;
struct FValidatedBlueprintNodePositionUpdate
{
    UEdGraphNode* Node = nullptr;
    TStrongObjectPtr<UEdGraphNode> Hold;
    FString NodeId;
    FString NodeName;
    FString NodeClass;
    FString NodeTitle;
    int32 OriginalX = 0;
    int32 OriginalY = 0;
    int32 TargetX = 0;
    int32 TargetY = 0;
};

TArray<TSharedPtr<FJsonValue>> PositionToJson(int32 X, int32 Y)
{
    TArray<TSharedPtr<FJsonValue>> Values;
    Values.Reserve(2);
    Values.Add(MakeShared<FJsonValueNumber>(X));
    Values.Add(MakeShared<FJsonValueNumber>(Y));
    return Values;
}

bool ParseStrictNodePosition(
    const TSharedPtr<FJsonObject>& UpdateObject,
    const TCHAR* FieldName,
    int32& OutX,
    int32& OutY,
    FString& OutError)
{
    OutError.Reset();

    const TArray<TSharedPtr<FJsonValue>>* PositionValues = nullptr;
    if (!UpdateObject.IsValid() ||
        !UpdateObject->TryGetArrayField(FieldName, PositionValues) ||
        !PositionValues ||
        PositionValues->Num() != 2)
    {
        OutError = FString::Printf(TEXT("'%s' must be an [X, Y] array"), FieldName);
        return false;
    }

    int32 ParsedValues[2] = {0, 0};
    for (int32 AxisIndex = 0; AxisIndex < 2; ++AxisIndex)
    {
        const TSharedPtr<FJsonValue>& AxisValue = (*PositionValues)[AxisIndex];
        if (!AxisValue.IsValid() || AxisValue->Type != EJson::Number)
        {
            OutError = FString::Printf(TEXT("'%s' values must be integers"), FieldName);
            return false;
        }

        const double Number = AxisValue->AsNumber();
        if (!FMath::IsFinite(Number) ||
            Number < -2147483648.0 ||
            Number > 2147483647.0 ||
            static_cast<double>(static_cast<int64>(Number)) != Number)
        {
            OutError = FString::Printf(TEXT("'%s' values must be finite 32-bit integers"), FieldName);
            return false;
        }

        ParsedValues[AxisIndex] = static_cast<int32>(Number);
    }

    OutX = ParsedValues[0];
    OutY = ParsedValues[1];
    return true;
}

UEdGraphNode* FindNodeByGuid(UEdGraph* Graph, const FGuid& NodeGuid,FResult& R)
{
    Count(R,TEXT("find_node_guid_calls"));
    if (!Loaded(Graph))
    {
        return nullptr;
    }

    for (UEdGraphNode* Node : Graph->Nodes)
    {
        if (Loaded(Node) && Node->NodeGuid == NodeGuid)
        {
            return Node;
        }
    }

    return nullptr;
}

TSharedPtr<FJsonObject> CreateNodePositionValidationError(
    const FString& Message,
    int32 UpdateIndex = INDEX_NONE,
    const FString& NodeId = FString())
{
    TSharedPtr<FJsonObject> ErrorObject = CreateErrorResponse(Message);
    ErrorObject->SetStringField(TEXT("validation_phase"), TEXT("pre_mutation"));
    ErrorObject->SetBoolField(TEXT("mutation_applied"), false);
    if (UpdateIndex != INDEX_NONE)
    {
        ErrorObject->SetNumberField(TEXT("failed_update_index"), UpdateIndex);
    }
    if (!NodeId.IsEmpty())
    {
        ErrorObject->SetStringField(TEXT("node_id"), NodeId);
    }
    return ErrorObject;
}
void AddMutationResultContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const TSharedPtr<FJsonObject>& Before,
    const TSharedPtr<FJsonObject>& After,
    const TSharedPtr<FJsonObject>& Delta,
    bool bVerificationPassed,
    const FString& VerificationMethod,
    const TArray<FString>& VerificationChecks,
    bool bStrictSuccess=true)
{
    if (!Result.IsValid())
    {
        return;
    }

    bool bLegacySuccess = true;
    if (Result->HasTypedField<EJson::Boolean>(TEXT("success")))
    {
        bLegacySuccess = Result->GetBoolField(TEXT("success"));
    }

    Result->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Result->SetStringField(TEXT("mode"), bDryRun ? TEXT("dry_run") : TEXT("apply"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("DRY_RUN"), bDryRun);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);

    TSharedPtr<FJsonObject> Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !bDryRun);
    Mutation->SetBoolField(TEXT("performed"), bMutationPerformed);
    Mutation->SetStringField(
        TEXT("state"),
        bMutationPerformed ? TEXT("performed") : TEXT("not_performed"));
    Result->SetObjectField(TEXT("mutation"), Mutation);

    if (Before.IsValid())
    {
        Result->SetObjectField(TEXT("before"), Before);
    }
    if (After.IsValid())
    {
        Result->SetObjectField(TEXT("after"), After);
    }

    TSharedPtr<FJsonObject> NormalizedDelta = Delta.IsValid()
        ? Delta
        : MakeShared<FJsonObject>();
    static const TCHAR* CanonicalCountFields[] =
    {
        TEXT("affected_count"),
        TEXT("created_count"),
        TEXT("updated_count"),
        TEXT("removed_count"),
        TEXT("skipped_count"),
        TEXT("failed_count")
    };
    for (const TCHAR* FieldName : CanonicalCountFields)
    {
        if (!NormalizedDelta->HasField(FieldName))
        {
            NormalizedDelta->SetField(FieldName, MakeShared<FJsonValueNull>());
        }
    }
    Result->SetObjectField(TEXT("delta"), NormalizedDelta);

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(
        TEXT("status"),
        bVerificationPassed ? TEXT("passed") : TEXT("failed"));
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("before_observed"), Before.IsValid());
    Verification->SetBoolField(TEXT("after_observed"), After.IsValid());
    Verification->SetBoolField(TEXT("strict"), bStrictSuccess);
    TArray<TSharedPtr<FJsonValue>> CheckValues;
    CheckValues.Reserve(VerificationChecks.Num());
    for (const FString& Check : VerificationChecks)
    {
        CheckValues.Add(MakeShared<FJsonValueString>(Check));
    }
    Verification->SetArrayField(TEXT("checks"), CheckValues);
    Result->SetObjectField(TEXT("verification"), Verification);

    const bool bVerifiedSuccess = bLegacySuccess && bVerificationPassed;
    Result->SetBoolField(TEXT("verified_success"), bVerifiedSuccess);
    FString SuccessSemantics = TEXT("legacy_execution");
    if (bDryRun)
    {
        SuccessSemantics = bVerifiedSuccess ? TEXT("verified_dry_run") : TEXT("failed_dry_run");
    }
    else if (bVerifiedSuccess)
    {
        SuccessSemantics = TEXT("verified_postcondition");
    }
    else if (bStrictSuccess && bLegacySuccess)
    {
        SuccessSemantics = TEXT("postcondition_failed");
    }
    else if (!bLegacySuccess)
    {
        SuccessSemantics = TEXT("failed_execution");
    }
    Result->SetStringField(TEXT("success_semantics"), SuccessSemantics);

    if (!Result->HasField(TEXT("success")))
    {
        Result->SetBoolField(TEXT("success"), bLegacySuccess);
    }
    if (bStrictSuccess && bLegacySuccess && !bVerificationPassed)
    {
        Result->SetBoolField(TEXT("legacy_success"), true);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed"));
        if (!Result->HasField(TEXT("error")))
        {
            Result->SetStringField(
                TEXT("error"),
                TEXT("Mutation postcondition failed independent readback"));
        }
    }
}
TSharedPtr<FJsonObject> RunFind(const TSharedPtr<FJsonObject>& Params,FResult& R)
{
    // Get required parameters
    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName))
    {
        return CreateErrorResponse(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString NodeType;
    if (!Params->TryGetStringField(TEXT("node_type"), NodeType))
    {
        return CreateErrorResponse(TEXT("Missing 'node_type' parameter"));
    }

    // Find the blueprint
    UBlueprint* Blueprint = FindBlueprint(BlueprintName,R);
    if (!Blueprint)
    {
        return CreateErrorResponse(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TStrongObjectPtr<UBlueprint> BlueprintHold(Blueprint);
    FString GraphError;
    UEdGraph* TargetGraph = ResolveBlueprintGraph(Blueprint,Params,false,GraphError,R,true);
    if (!TargetGraph)
    {
        return CreateErrorResponse(GraphError);
    }

    TStrongObjectPtr<UEdGraph> GraphHold(TargetGraph);
    // Create a JSON array for the node GUIDs
    TArray<TSharedPtr<FJsonValue>> NodeGuidArray;

    // Filter nodes by the exact requested type
    if (NodeType == TEXT("Event"))
    {
        FString EventName;
        if (!Params->TryGetStringField(TEXT("event_name"), EventName))
        {
            return CreateErrorResponse(TEXT("Missing 'event_name' parameter for Event node search"));
        }

        // Look for nodes with exact event name (e.g., ReceiveBeginPlay)
        for (UEdGraphNode* Node : TargetGraph->Nodes)
        {
            Count(R,TEXT("event_nodes_visited"));
            UK2Node_Event* EventNode = Loaded(Node)?Cast<UK2Node_Event>(Node):nullptr;
            if (EventNode && EventNode->EventReference.GetMemberName() == FName(*EventName))
            {
                UE_LOG(LogTemp, Display, TEXT("Found event node with name %s: %s"), *EventName, *EventNode->NodeGuid.ToString());
                NodeGuidArray.Add(MakeShared<FJsonValueString>(EventNode->NodeGuid.ToString()));
            }
        }
    }
    // Add other node types as needed (InputAction, etc.)

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"),true);
    ResultObj->SetArrayField(TEXT("node_guids"), NodeGuidArray);
    AddGraphField(ResultObj, Blueprint, TargetGraph);

    return ResultObj;
}
TSharedPtr<FJsonObject> RunLayout(const TSharedPtr<FJsonObject>& Params,FResult& R)
{
    if (!Params.IsValid())
    {
        return CreateNodePositionValidationError(TEXT("Missing parameters"));
    }

    if (IsPlaySessionActive())
    {
        return CreateNodePositionValidationError(TEXT("Blueprint node position edits are blocked while PIE/SIE is active. End play mode before mutating Blueprint graphs through UnrealMCP."));
    }

    FString BlueprintName;
    if (!Params->TryGetStringField(TEXT("blueprint_name"), BlueprintName) || BlueprintName.TrimStartAndEnd().IsEmpty())
    {
        return CreateNodePositionValidationError(TEXT("Missing 'blueprint_name' parameter"));
    }

    FString GraphName;
    FString GraphId;
    Params->TryGetStringField(TEXT("graph_name"), GraphName);
    Params->TryGetStringField(TEXT("graph_id"), GraphId);
    GraphName.TrimStartAndEndInline();
    GraphId.TrimStartAndEndInline();
    if (GraphName.IsEmpty() && GraphId.IsEmpty())
    {
        return CreateNodePositionValidationError(TEXT("set_blueprint_node_positions requires an explicit 'graph_name' or 'graph_id'"));
    }

    const TArray<TSharedPtr<FJsonValue>>* UpdateValues = nullptr;
    if (!Params->TryGetArrayField(TEXT("updates"), UpdateValues) || !UpdateValues)
    {
        return CreateNodePositionValidationError(TEXT("Missing 'updates' array"));
    }
    if (UpdateValues->Num() == 0)
    {
        return CreateNodePositionValidationError(TEXT("'updates' must contain at least one node position update"));
    }
    if (UpdateValues->Num() > MaxBlueprintNodePositionUpdates)
    {
        return CreateNodePositionValidationError(FString::Printf(
            TEXT("'updates' contains %d entries; the maximum is %d"),
            UpdateValues->Num(),
            MaxBlueprintNodePositionUpdates));
    }

    UBlueprint* Blueprint = FindBlueprint(BlueprintName,R);
    if (!Blueprint)
    {
        return CreateNodePositionValidationError(FString::Printf(TEXT("Blueprint not found: %s"), *BlueprintName));
    }

    TStrongObjectPtr<UBlueprint> BlueprintHold(Blueprint);
    FString GraphError;
    bool bGraphCreated = false;
    UEdGraph* TargetGraph = ResolveBlueprintGraph(Blueprint,Params,false,GraphError,R,false);
    if (!TargetGraph)
    {
        return CreateNodePositionValidationError(GraphError);
    }

    TStrongObjectPtr<UEdGraph> GraphHold(TargetGraph);
    auto ResultObject=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("legacy_result"),ResultObject);
    TArray<TSharedPtr<FJsonValue>> GuardReceipts;R.Data->SetArrayField(TEXT("validated_row_receipts"),GuardReceipts);
    TSet<FGuid> SeenNodeGuids;
    TArray<FValidatedBlueprintNodePositionUpdate> ValidatedUpdates;
    ValidatedUpdates.Reserve(UpdateValues->Num());

    // Validate every requested update before calling Modify() or changing any
    // node coordinate. A stale guard therefore leaves the entire graph intact.
    for (int32 UpdateIndex = 0; UpdateIndex < UpdateValues->Num(); ++UpdateIndex)
    {
        const TSharedPtr<FJsonObject> UpdateObject = (*UpdateValues)[UpdateIndex].IsValid()&&(*UpdateValues)[UpdateIndex]->Type==EJson::Object
            ? (*UpdateValues)[UpdateIndex]->AsObject()
            : nullptr;
        if (!UpdateObject.IsValid())
        {
            return CreateNodePositionValidationError(TEXT("Each 'updates' entry must be an object"), UpdateIndex);
        }

        FString NodeId;
        if (!UpdateObject->TryGetStringField(TEXT("node_id"), NodeId) || NodeId.TrimStartAndEnd().IsEmpty())
        {
            return CreateNodePositionValidationError(TEXT("Each update requires a non-empty 'node_id'"), UpdateIndex);
        }
        NodeId.TrimStartAndEndInline();

        FGuid NodeGuid;
        if (!FGuid::Parse(NodeId, NodeGuid) || !NodeGuid.IsValid())
        {
            return CreateNodePositionValidationError(FString::Printf(TEXT("Invalid node GUID: %s"), *NodeId), UpdateIndex, NodeId);
        }
        if (SeenNodeGuids.Contains(NodeGuid))
        {
            return CreateNodePositionValidationError(FString::Printf(TEXT("Duplicate node GUID in 'updates': %s"), *NodeId), UpdateIndex, NodeId);
        }
        SeenNodeGuids.Add(NodeGuid);

        UEdGraphNode* Node = FindNodeByGuid(TargetGraph,NodeGuid,R);
        if (!Node)
        {
            return CreateNodePositionValidationError(FString::Printf(TEXT("Node not found in target graph: %s"), *NodeId), UpdateIndex, NodeId);
        }

        FString ExpectedClass;
        if (!UpdateObject->TryGetStringField(TEXT("expected_class"), ExpectedClass) || ExpectedClass.TrimStartAndEnd().IsEmpty())
        {
            return CreateNodePositionValidationError(TEXT("Each update requires a non-empty 'expected_class' guard"), UpdateIndex, NodeId);
        }
        ExpectedClass.TrimStartAndEndInline();

        FString ExpectedTitle;
        if (!UpdateObject->TryGetStringField(TEXT("expected_title"), ExpectedTitle))
        {
            return CreateNodePositionValidationError(TEXT("Each update requires an 'expected_title' guard"), UpdateIndex, NodeId);
        }

        const FString ActualClassName = Node->GetClass() ? Node->GetClass()->GetName() : FString();
        const FString ActualClassPath = Node->GetClass() ? Node->GetClass()->GetPathName() : FString();
        if (!ExpectedClass.Equals(ActualClassName, ESearchCase::IgnoreCase) &&
            !ExpectedClass.Equals(ActualClassPath, ESearchCase::IgnoreCase))
        {
            return CreateNodePositionValidationError(FString::Printf(
                TEXT("Expected node class '%s' but found '%s'"),
                *ExpectedClass,
                *ActualClassPath), UpdateIndex, NodeId);
        }

        Count(R,TEXT("node_title_calls"));
        const FString ActualTitle = Node->GetNodeTitle(ENodeTitleType::FullTitle).ToString();
        if (!ExpectedTitle.Equals(ActualTitle, ESearchCase::CaseSensitive))
        {
            return CreateNodePositionValidationError(FString::Printf(
                TEXT("Expected node title '%s' but found '%s'"),
                *ExpectedTitle,
                *ActualTitle), UpdateIndex, NodeId);
        }

        int32 ExpectedX = 0;
        int32 ExpectedY = 0;
        int32 TargetX = 0;
        int32 TargetY = 0;
        FString PositionError;
        if (!ParseStrictNodePosition(UpdateObject, TEXT("expected_position"), ExpectedX, ExpectedY, PositionError))
        {
            return CreateNodePositionValidationError(PositionError, UpdateIndex, NodeId);
        }
        if (!ParseStrictNodePosition(UpdateObject, TEXT("position"), TargetX, TargetY, PositionError))
        {
            return CreateNodePositionValidationError(PositionError, UpdateIndex, NodeId);
        }

        if (Node->NodePosX != ExpectedX || Node->NodePosY != ExpectedY)
        {
            return CreateNodePositionValidationError(FString::Printf(
                TEXT("Expected node position [%d, %d] but found [%d, %d]"),
                ExpectedX,
                ExpectedY,
                Node->NodePosX,
                Node->NodePosY), UpdateIndex, NodeId);
        }

        FValidatedBlueprintNodePositionUpdate& Validated = ValidatedUpdates.AddDefaulted_GetRef();
        Validated.Node = Node;Validated.Hold.Reset(Node);
        Validated.NodeId = NodeGuid.ToString();
        Validated.NodeName = Node->GetName();
        Validated.NodeClass = ActualClassPath;
        Validated.NodeTitle = ActualTitle;
        Validated.OriginalX = Node->NodePosX;
        Validated.OriginalY = Node->NodePosY;
        Validated.TargetX = TargetX;
        Validated.TargetY = TargetY;
        Count(R,TEXT("position_rows_validated"));auto Guard=MakeShared<FJsonObject>();Guard->SetStringField(TEXT("node_id"),Validated.NodeId);Guard->SetStringField(TEXT("node_class"),ActualClassPath);Guard->SetStringField(TEXT("node_title"),ActualTitle);Guard->SetArrayField(TEXT("original_position"),PositionToJson(Validated.OriginalX,Validated.OriginalY));Guard->SetArrayField(TEXT("target_position"),PositionToJson(TargetX,TargetY));GuardReceipts.Add(MakeShared<FJsonValueObject>(Guard));R.Data->SetArrayField(TEXT("validated_row_receipts"),GuardReceipts);
    }

    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true);
    const bool bCompileRequested = GetBoolParam(Params, TEXT("compile"), false);
    const bool bSaveRequested = GetBoolParam(Params, TEXT("save"), false);

    int32 WouldChangeCount = 0;
    for (const FValidatedBlueprintNodePositionUpdate& Validated : ValidatedUpdates)
    {
        if (Validated.OriginalX != Validated.TargetX || Validated.OriginalY != Validated.TargetY)
        {
            ++WouldChangeCount;
        }
    }

    R.bCompileRequested=bCompileRequested;R.bSaveRequested=bSaveRequested;
    const int32 GraphNodeCountBefore = TargetGraph->Nodes.Num();
    int32 ChangedCount = 0;
    if (!bDryRun && WouldChangeCount > 0)
    {
        R.Stage=TEXT("mutate_positions");R.bMutationAttempted=true;R.Data->SetBoolField(TEXT("read_only"),false);Count(R,TEXT("graph_modify_calls"));TargetGraph->Modify();
        for (const FValidatedBlueprintNodePositionUpdate& Validated : ValidatedUpdates)
        {
            if (Validated.OriginalX == Validated.TargetX && Validated.OriginalY == Validated.TargetY)
            {
                continue;
            }

            Count(R,TEXT("node_modify_calls"));Validated.Node->Modify();
            Count(R,TEXT("position_field_write_calls"));Validated.Node->NodePosX = Validated.TargetX;
            Count(R,TEXT("position_field_write_calls"));Validated.Node->NodePosY = Validated.TargetY;R.bChanged=true;R.Data->SetNumberField(TEXT("accepted_position_write_count"),ChangedCount+1);
            ++ChangedCount;
        }
        Count(R,TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
    }

    bool bCompiled = false;
    bool bCompileSucceeded = true;
    FString CompileStatus = GetBlueprintNodeStatusName(Blueprint->Status);
    if (!bDryRun && bCompileRequested)
    {
        R.Stage=TEXT("compile");R.bMutationAttempted=true;R.bCompileAttempted=true;R.Data->SetBoolField(TEXT("read_only"),false);Count(R,TEXT("full_compile_calls"));FCompilerResultsLog Log;Log.SetSourcePath(Blueprint->GetPathName());
        FKismetEditorUtilities::CompileBlueprint(Blueprint,EBlueprintCompileOptions::None,&Log);
        Count(R,TEXT("full_compile_returns"));ResultObject->SetNumberField(TEXT("compile_error_count"),Log.NumErrors);ResultObject->SetNumberField(TEXT("compile_warning_count"),Log.NumWarnings);R.Compiled=(Blueprint->Status==BS_UpToDate||Blueprint->Status==BS_UpToDateWithWarnings)&&Log.NumErrors==0;ResultObject->SetBoolField(TEXT("compiled"),true);ResultObject->SetBoolField(TEXT("compile_health"),R.Compiled.GetValue());
        bCompiled = true;
        CompileStatus = GetBlueprintNodeStatusName(Blueprint->Status);
        bCompileSucceeded = Blueprint->Status == BS_UpToDate || Blueprint->Status == BS_UpToDateWithWarnings;
    }

    bool bSaved = false;
    bool bSaveAttempted = false;
    if (!bDryRun && bSaveRequested && bCompileSucceeded)
    {
        R.Stage=TEXT("save");R.bMutationAttempted=true;R.bSaveAttempted=true;R.Data->SetBoolField(TEXT("read_only"),false);Count(R,TEXT("forced_save_calls"));bSaveAttempted = true;
        bSaved = UEditorAssetLibrary::SaveLoadedAsset(Blueprint, false);
        Count(R,TEXT("forced_save_returns"));R.bSaved=bSaved;ResultObject->SetBoolField(TEXT("save_attempted"),true);ResultObject->SetBoolField(TEXT("saved"),bSaved);
    }

    const bool bSaveSucceeded = bDryRun || !bSaveRequested || bSaved;

    FString ReadbackGraphError;
    bool bReadbackGraphCreated = false;
    UEdGraph* ReadbackGraph = ResolveBlueprintGraph(Blueprint,Params,false,ReadbackGraphError,R,false);
    int32 ActualChangedCount = 0;
    int32 ReadbackFailureCount = 0;
    TArray<TSharedPtr<FJsonValue>> UpdateResults;
    TArray<TSharedPtr<FJsonValue>> BeforePositions;
    TArray<TSharedPtr<FJsonValue>> AfterPositions;
    TArray<TSharedPtr<FJsonValue>> PlannedPositions;
    UpdateResults.Reserve(ValidatedUpdates.Num());
    BeforePositions.Reserve(ValidatedUpdates.Num());
    AfterPositions.Reserve(ValidatedUpdates.Num());
    PlannedPositions.Reserve(ValidatedUpdates.Num());
    for (const FValidatedBlueprintNodePositionUpdate& Validated : ValidatedUpdates)
    {
        Count(R,TEXT("position_rows_read_back"));
        const bool bWouldChange = Validated.OriginalX != Validated.TargetX || Validated.OriginalY != Validated.TargetY;
        FGuid ReadbackGuid;
        const bool bGuidParsed = FGuid::Parse(Validated.NodeId, ReadbackGuid);
        UEdGraphNode* ReadbackNode = bGuidParsed && ReadbackGraph
            ? FindNodeByGuid(ReadbackGraph,ReadbackGuid,R)
            : nullptr;
        const int32 ExpectedX = bDryRun ? Validated.OriginalX : Validated.TargetX;
        const int32 ExpectedY = bDryRun ? Validated.OriginalY : Validated.TargetY;
        const bool bReadbackMatches =
            ReadbackNode &&
            ReadbackNode->NodePosX == ExpectedX &&
            ReadbackNode->NodePosY == ExpectedY;
        const bool bActuallyChanged =
            ReadbackNode &&
            (ReadbackNode->NodePosX != Validated.OriginalX || ReadbackNode->NodePosY != Validated.OriginalY);
        if (bActuallyChanged)
        {
            ++ActualChangedCount;
        }
        if (!bReadbackMatches)
        {
            ++ReadbackFailureCount;
        }

        TSharedPtr<FJsonObject> UpdateResult = MakeShared<FJsonObject>();
        UpdateResult->SetStringField(TEXT("node_id"), Validated.NodeId);
        UpdateResult->SetStringField(TEXT("node_name"), Validated.NodeName);
        UpdateResult->SetStringField(TEXT("node_class"), Validated.NodeClass);
        UpdateResult->SetStringField(TEXT("node_title"), Validated.NodeTitle);
        UpdateResult->SetArrayField(TEXT("original_position"), PositionToJson(Validated.OriginalX, Validated.OriginalY));
        UpdateResult->SetArrayField(TEXT("target_position"), PositionToJson(Validated.TargetX, Validated.TargetY));
        if (ReadbackNode)
        {
            UpdateResult->SetArrayField(TEXT("actual_position"), PositionToJson(ReadbackNode->NodePosX, ReadbackNode->NodePosY));
        }
        else
        {
            UpdateResult->SetField(TEXT("actual_position"), MakeShared<FJsonValueNull>());
        }
        UpdateResult->SetBoolField(TEXT("node_found_after"), ReadbackNode != nullptr);
        UpdateResult->SetBoolField(TEXT("readback_matches_expected"), bReadbackMatches);
        UpdateResult->SetBoolField(TEXT("would_change"), bWouldChange);
        UpdateResult->SetBoolField(TEXT("changed"), bActuallyChanged);
        UpdateResults.Add(MakeShared<FJsonValueObject>(UpdateResult));

        TSharedPtr<FJsonObject> BeforePosition = MakeShared<FJsonObject>();
        BeforePosition->SetStringField(TEXT("node_id"), Validated.NodeId);
        BeforePosition->SetArrayField(TEXT("position"), PositionToJson(Validated.OriginalX, Validated.OriginalY));
        BeforePositions.Add(MakeShared<FJsonValueObject>(BeforePosition));

        TSharedPtr<FJsonObject> AfterPosition = MakeShared<FJsonObject>();
        AfterPosition->SetStringField(TEXT("node_id"), Validated.NodeId);
        AfterPosition->SetBoolField(TEXT("exists"), ReadbackNode != nullptr);
        if (ReadbackNode)
        {
            AfterPosition->SetArrayField(TEXT("position"), PositionToJson(ReadbackNode->NodePosX, ReadbackNode->NodePosY));
        }
        AfterPositions.Add(MakeShared<FJsonValueObject>(AfterPosition));

        TSharedPtr<FJsonObject> PlannedPosition = MakeShared<FJsonObject>();
        PlannedPosition->SetStringField(TEXT("node_id"), Validated.NodeId);
        PlannedPosition->SetArrayField(TEXT("position"), PositionToJson(Validated.TargetX, Validated.TargetY));
        PlannedPositions.Add(MakeShared<FJsonValueObject>(PlannedPosition));
    }

    const bool bReadbackVerified = ReadbackGraph && ReadbackFailureCount == 0;
    const bool bOverallSuccess = bCompileSucceeded && bSaveSucceeded && bReadbackVerified;
    const int32 CompletionFailureCount =
        (bCompileSucceeded ? 0 : 1) +
        (bSaveSucceeded ? 0 : 1);
    TArray<TSharedPtr<FJsonValue>> CompletionErrors;
    FString CompletionErrorMessage;
    if (!bCompileSucceeded)
    {
        CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("compile_failed")));
        CompletionErrorMessage = FString::Printf(
            TEXT("Blueprint compile failed with status '%s'"),
            *CompileStatus);
        if (bSaveRequested)
        {
            CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_skipped_due_to_compile_failure")));
            CompletionErrorMessage += TEXT("; requested save was skipped");
        }
    }
    else if (!bSaveSucceeded)
    {
        CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("save_failed")));
        CompletionErrorMessage = TEXT("Blueprint save failed");
    }
    if (!bReadbackVerified)
    {
        CompletionErrors.Add(MakeShared<FJsonValueString>(TEXT("position_readback_failed")));
        if (!CompletionErrorMessage.IsEmpty())
        {
            CompletionErrorMessage += TEXT("; ");
        }
        CompletionErrorMessage += ReadbackGraph
            ? FString::Printf(TEXT("%d node position readback(s) did not match"), ReadbackFailureCount)
            : FString::Printf(TEXT("readback graph resolution failed: %s"), *ReadbackGraphError);
    }

    R.Stage=TEXT("position_readback");
    if(!ReadbackGraph||ReadbackFailureCount>0) { R.bChangeStateKnown=false; }
    R.bChanged=ActualChangedCount>0;
    ResultObject->SetBoolField(TEXT("success"), bOverallSuccess);
    if (!bOverallSuccess)
    {
        ResultObject->SetStringField(TEXT("error"), CompletionErrorMessage);
        ResultObject->SetArrayField(TEXT("completion_errors"), CompletionErrors);
    }
    ResultObject->SetStringField(TEXT("blueprint_name"), BlueprintName);
    ResultObject->SetBoolField(TEXT("all_guards_passed"), true);
    ResultObject->SetBoolField(TEXT("dry_run"), bDryRun);
    ResultObject->SetBoolField(TEXT("mutation_applied"), !bDryRun && ChangedCount > 0);
    ResultObject->SetBoolField(TEXT("partial_target_modified"), !bOverallSuccess && !bDryRun && ChangedCount > 0);
    ResultObject->SetNumberField(TEXT("requested_update_count"), ValidatedUpdates.Num());
    ResultObject->SetNumberField(TEXT("would_change_count"), WouldChangeCount);
    ResultObject->SetNumberField(TEXT("mutation_write_count"), ChangedCount);
    ResultObject->SetNumberField(TEXT("changed_count"), ActualChangedCount);
    ResultObject->SetNumberField(TEXT("readback_failure_count"), ReadbackFailureCount);
    ResultObject->SetBoolField(TEXT("compile_requested"), bCompileRequested);
    ResultObject->SetBoolField(TEXT("compiled"), bCompiled);
    ResultObject->SetBoolField(TEXT("compile_succeeded"), bCompileSucceeded);
    ResultObject->SetStringField(TEXT("compile_status"), CompileStatus);
    ResultObject->SetBoolField(TEXT("save_requested"), bSaveRequested);
    ResultObject->SetBoolField(TEXT("save_attempted"), bSaveAttempted);
    ResultObject->SetBoolField(TEXT("saved"), bSaved);
    ResultObject->SetBoolField(TEXT("save_succeeded"), bSaveSucceeded);
    ResultObject->SetBoolField(TEXT("package_dirty"), Blueprint->GetOutermost() && Blueprint->GetOutermost()->IsDirty());
    ResultObject->SetBoolField(TEXT("position_readback_verified"), bReadbackVerified);
    ResultObject->SetBoolField(TEXT("postcondition_verified"), bOverallSuccess);
    ResultObject->SetNumberField(TEXT("completion_failure_count"), CompletionFailureCount);
    ResultObject->SetArrayField(TEXT("updates"), UpdateResults);

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetNumberField(TEXT("graph_node_count"), GraphNodeCountBefore);
    Before->SetArrayField(TEXT("node_positions"), BeforePositions);
    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetNumberField(TEXT("graph_node_count"), ReadbackGraph ? ReadbackGraph->Nodes.Num() : 0);
    After->SetArrayField(TEXT("node_positions"), AfterPositions);
    TSharedPtr<FJsonObject> PlannedAfter = MakeShared<FJsonObject>();
    PlannedAfter->SetNumberField(TEXT("graph_node_count"), GraphNodeCountBefore);
    PlannedAfter->SetArrayField(TEXT("node_positions"), PlannedPositions);
    ResultObject->SetObjectField(TEXT("after_expected"), PlannedAfter);

    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), ActualChangedCount);
    Delta->SetNumberField(TEXT("created_count"), 0);
    Delta->SetNumberField(TEXT("updated_count"), ActualChangedCount);
    Delta->SetNumberField(TEXT("removed_count"), 0);
    Delta->SetNumberField(TEXT("skipped_count"), bDryRun ? 0 : ValidatedUpdates.Num() - ActualChangedCount);
    Delta->SetNumberField(TEXT("readback_failure_count"), ReadbackFailureCount);
    Delta->SetNumberField(TEXT("completion_failure_count"), CompletionFailureCount);
    Delta->SetNumberField(TEXT("failed_count"), ReadbackFailureCount + CompletionFailureCount);
    AddMutationResultContract(
        ResultObject,
        bDryRun,
        !bDryRun && ChangedCount > 0,
        ActualChangedCount > 0,
        Before,
        After,
        Delta,
        bOverallSuccess,
        TEXT("blueprint_graph_node_guid_relookup"),
        {TEXT("all_nodes_resolved"), TEXT("positions_match_expected"), TEXT("compile_gate"), TEXT("save_gate")});
    ResultObject->SetBoolField(TEXT("graph_descriptor_fallback_old"),ReadbackGraph==nullptr);
    AddGraphField(ResultObject, Blueprint, ReadbackGraph ? ReadbackGraph : TargetGraph);
    return ResultObject;
}

void Finish(FResult& R,const TSharedPtr<FJsonObject>& Legacy)
{
    R.Data->SetObjectField(TEXT("legacy_result"),Legacy);bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);R.bSuccess=Success;R.bPostconditionPassed=Success;
    if(R.bCompileAttempted&&R.Compiled.IsSet()&&!R.Compiled.GetValue()) { R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=TEXT("blueprint_compile_failed");R.Error=TEXT("Actual compiler errors/status are not healthy; original status-only completion and actual save prefix remain in legacy_result"); }
    if(!R.bSuccess) { if(R.Error.IsEmpty()) { Legacy->TryGetStringField(TEXT("error"),R.Error); }if(R.ErrorCode.IsEmpty()) { R.ErrorCode=R.bMutationAttempted?TEXT("blueprint_layout_completion_failed"):TEXT("blueprint_query_layout_validation_failed"); } }
    if(R.bSuccess&&R.bChangeStateKnown) { R.Stage=TEXT("complete"); }else if(R.Stage==TEXT("preflight")) { R.Stage=TEXT("validation"); }
    R.Data->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted);if(R.Data->GetStringField(TEXT("operation"))==TEXT("set_blueprint_node_positions")) { R.Data->SetBoolField(TEXT("coordinate_change_scope_only"),true);R.Data->SetBoolField(TEXT("current_positions_known"),R.bChangeStateKnown); }else { R.Data->SetStringField(TEXT("change_scope"),TEXT("existing read or original implicit default EventGraph creation")); }
}
}
Variable::FResult Find(const FString& BlueprintName,const FString& NodeTypeJson,const FString& EventTypeJson,const FString& GraphName,const FString& GraphId,const FString& GraphType)
{
    FResult R;Init(R,TEXT("find_blueprint_nodes"));if(!IsEditorGameThread()) { Finish(R,CreateErrorResponse(TEXT("Editor game thread required")));return R; }
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);
    TSharedPtr<FJsonValue> Type,Event;if(!Decode(NodeTypeJson,Type,R)||!Decode(EventTypeJson,Event,R)) { Finish(R,CreateErrorResponse(R.Error));return R; }P->SetField(TEXT("node_type"),Type);P->SetField(TEXT("event_name"),Event);P->SetField(TEXT("event_type"),Event);Finish(R,RunFind(P,R));return R;
}
Variable::FResult SetPositions(const FString& BlueprintName,const FString& GraphName,const FString& UpdatesJson,const FString& GraphId,const FString& GraphType,bool bDryRun,bool bCompile,bool bSave)
{
    FResult R;Init(R,TEXT("set_blueprint_node_positions"));R.bCompileRequested=bCompile;R.bSaveRequested=bSave;
    if(!IsEditorGameThread()) { Finish(R,CreateErrorResponse(TEXT("Editor game thread required")));return R; }
    // Original PIE/name/explicit-selector guards precede array validation.
    if(IsPlaySessionActive()) { Finish(R,CreateNodePositionValidationError(TEXT("Blueprint node position edits are blocked while PIE/SIE is active. End play mode before mutating Blueprint graphs through UnrealMCP.")));return R; }
    if(BlueprintName.TrimStartAndEnd().IsEmpty()) { Finish(R,CreateNodePositionValidationError(TEXT("Missing 'blueprint_name' parameter")));return R; }
    if(GraphName.TrimStartAndEnd().IsEmpty()&&GraphId.TrimStartAndEnd().IsEmpty()) { Finish(R,CreateNodePositionValidationError(TEXT("set_blueprint_node_positions requires an explicit 'graph_name' or 'graph_id'")));return R; }
    TSharedPtr<FJsonValue> Updates;if(!Decode(UpdatesJson,Updates,R)) { Finish(R,CreateNodePositionValidationError(R.Error));return R; }
    auto P=MakeShared<FJsonObject>();P->SetStringField(TEXT("blueprint_name"),BlueprintName);P->SetStringField(TEXT("graph_name"),GraphName);P->SetStringField(TEXT("graph_id"),GraphId);P->SetStringField(TEXT("graph_type"),GraphType);P->SetField(TEXT("updates"),Updates);P->SetBoolField(TEXT("dry_run"),bDryRun);P->SetBoolField(TEXT("compile"),bCompile);P->SetBoolField(TEXT("save"),bSave);Finish(R,RunLayout(P,R));return R;
}
}
