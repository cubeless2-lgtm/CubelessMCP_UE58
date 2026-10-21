#include "Compatibility/UE58/CubelessMaterialApi.h"

#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "HAL/PlatformTime.h"
#include "Editor.h"
#include "EdGraph/EdGraphNode.h"
#include "Engine/Texture.h"
#include "Engine/World.h"
#include "MaterialEditingLibrary.h"
#include "MaterialDomain.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionCollectionTransform.h"
#include "Materials/MaterialExpressionConstant.h"
#include "Materials/MaterialExpressionConstant2Vector.h"
#include "Materials/MaterialExpressionConstant3Vector.h"
#include "Materials/MaterialExpressionConstant4Vector.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionExternalCodeBase.h"
#include "Materials/MaterialExpressionFunctionInput.h"
#include "Materials/MaterialExpressionFunctionOutput.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionRerouteBase.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionStaticBoolParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInstanceConstant.h"
#include "Materials/MaterialInterface.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "RHIFeatureLevel.h"
#include "RHIShaderPlatform.h"
#include "RHIGlobals.h"
#include "ShaderCore.h"
#include "ShaderCompilerCore.h"
#include "Misc/Paths.h"
#include "Interfaces/IPluginManager.h"
#include "SceneTypes.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonWriter.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/FieldIterator.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::Material
{
namespace
{
bool IsAddOperation(EOperation Operation)
{
    return Operation == EOperation::AddNode || Operation == EOperation::AddCustomNode;
}

thread_local FReadMetricsScope* CurrentReadMetrics = nullptr;

void CountRead(FName Name, int64 Amount = 1)
{
    if (CurrentReadMetrics) { CurrentReadMetrics->Increment(Name, Amount); }
}

TSharedPtr<FJsonObject> MaterialApiError(const FString& Message)
{
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error"), Message);
    return Result;
}

// Audited dependency closure only; excludes all compile/save, cache repair,
// and persistent collection ticker handlers. The next two operations have a
// separate hardened implementation and a separately recorded helper closure.
#include "CubelessMaterialLegacy.inc"
#include "CubelessMaterialCollectionRead.inc"

bool IsToolPackage(const FString& PackageName)
{
    static const TCHAR* Prefixes[] = {
        TEXT("/Script/CubelessMCP"), TEXT("/Script/UnrealMCP"),
        TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"),
        TEXT("/Script/PythonScriptPlugin"), TEXT("/Script/EditorScriptingUtilities"),
        TEXT("/Script/CubelessControlRigTools"), TEXT("/Engine/PythonTypes")
    };
    for (const TCHAR* Prefix : Prefixes)
    {
        if (PackageName.StartsWith(Prefix, ESearchCase::IgnoreCase)) { return true; }
    }
    return false;
}

bool IsSupportedExistingClass(const UClass* Class)
{
    for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
    {
        const UPackage* Package = Current->GetOutermost();
        if (!Package || IsToolPackage(Package->GetName()) || Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly))
        {
            return false;
        }
    }
    return true;
}

#include "CubelessMaterialMutationLegacy.inc"
#include "CubelessMaterialCustomGuards.inc"
#include "CubelessMaterialMutation.inc"
#include "CubelessMaterialCustomLegacy.inc"
#include "CubelessMaterialCustom.inc"

TMap<FString, TSharedPtr<FJsonObject>> SnapshotNodes(const FState& State)
{
    TMap<FString, TSharedPtr<FJsonObject>> Result;
    if (!State.Snapshot.IsValid()) { return Result; }
    const TArray<TSharedPtr<FJsonValue>>* Nodes = nullptr;
    if (State.Snapshot->TryGetArrayField(TEXT("nodes"), Nodes) && Nodes)
    {
        for (const auto& Value : *Nodes)
        {
            const TSharedPtr<FJsonObject> Node = Value.IsValid() ? Value->AsObject() : nullptr;
            FString Key;
            if (Node.IsValid() && Node->TryGetStringField(TEXT("node_key"), Key)) { Result.Add(Key, Node); }
        }
    }
    return Result;
}

FString NodeWithoutPosition(const TSharedPtr<FJsonObject>& Node)
{
    auto Copy = MakeShared<FJsonObject>();
    Copy->Values = Node->Values;
    Copy->RemoveField(TEXT("x"));
    Copy->RemoveField(TEXT("y"));
    return ToJson(Copy);
}

TSharedPtr<FJsonObject> AnalyzeWithCompileObservation(const TSharedPtr<FJsonObject>& Params)
{
    auto Data = HandleAnalyzeMaterialGraph(Params);
    auto Observation = MakeShared<FJsonObject>();
    Observation->SetNumberField(TEXT("shader_platform"), static_cast<int32>(GMaxRHIShaderPlatform));
    Observation->SetBoolField(TEXT("null_rhi"), GUsingNullRHI);
    Observation->SetBoolField(TEXT("resource_available"), false);
    Observation->SetStringField(TEXT("status"), TEXT("unknown"));
    Observation->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Observation->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    Observation->SetField(TEXT("game_thread_shader_map_complete"), MakeShared<FJsonValueNull>());
    for (const TCHAR* Key : { TEXT("game_thread_shader_map_present"), TEXT("has_valid_game_thread_shader_map"), TEXT("shader_map_compilation_finalized"), TEXT("shader_map_compiled_successfully") }) { Observation->SetField(Key, MakeShared<FJsonValueNull>()); }
    Observation->SetField(TEXT("compile_errors"), MakeShared<FJsonValueNull>());
    Observation->SetStringField(TEXT("scope"), TEXT("current editor shader platform/default active quality point samples; cached completeness, current map presence, finalization and success are distinct observations. No Engine cause or save/reload/cook/render proof is inferred"));
    Observation->SetBoolField(TEXT("compile_submitted"), false);
    Observation->SetStringField(TEXT("poll_side_effect"), TEXT("IsCompilationFinished may finalize ready cache work; no explicit compile submission or blocking finish call"));
    FString Path;
    Data->TryGetStringField(TEXT("asset_path"), Path);
    UObject* Asset = Path.IsEmpty() ? nullptr : FindObject<UObject>(nullptr, *Path);
    if (UMaterialInterface* Interface = Cast<UMaterialInterface>(Asset))
    {
        Observation->SetStringField(TEXT("asset_path"), Interface->GetPathName());
        if (FMaterialResource* Resource = Interface->GetMaterialResource(GMaxRHIShaderPlatform))
        {
            const bool bFinished = Resource->IsCompilationFinished();
            const bool bCompleteMap = Resource->IsGameThreadShaderMapComplete();
            const TArray<FString>& Errors = Resource->GetCompileErrors();
            const bool bHasValidMap = Resource->HasValidGameThreadShaderMap();
            const FMaterialShaderMap* ShaderMap = Resource->GetGameThreadShaderMap(); const bool bHasMap = ShaderMap != nullptr;
            const bool bFinalized = ShaderMap && ShaderMap->IsCompilationFinalized(); const bool bShaderSuccess = ShaderMap && ShaderMap->CompiledSuccessfully();
            Observation->SetBoolField(TEXT("game_thread_shader_map_present"), bHasMap); Observation->SetBoolField(TEXT("has_valid_game_thread_shader_map"), bHasValidMap);
            if (ShaderMap)
            {
                Observation->SetBoolField(TEXT("shader_map_compilation_finalized"), bFinalized);
                Observation->SetBoolField(TEXT("shader_map_compiled_successfully"), bShaderSuccess);
            }
            Observation->SetBoolField(TEXT("resource_available"), true);
            Observation->SetBoolField(TEXT("compilation_finished"), bFinished);
            Observation->SetBoolField(TEXT("game_thread_shader_map_complete"), bCompleteMap);
            TArray<TSharedPtr<FJsonValue>> ErrorValues;
            for (const FString& Error : Errors) { ErrorValues.Add(MakeShared<FJsonValueString>(Error)); }
            Observation->SetArrayField(TEXT("compile_errors"), ErrorValues);
            Observation->SetNumberField(TEXT("compile_error_count"), Errors.Num());
            const bool bCompiled = bFinished && bCompleteMap && bHasValidMap && bHasMap && bFinalized && bShaderSuccess && Errors.IsEmpty();
            Observation->SetBoolField(TEXT("compiled"), bCompiled);
            Observation->SetStringField(TEXT("status"), !bFinished ? TEXT("pending") : bCompiled ? TEXT("current_resource_complete") : Errors.IsEmpty() ? TEXT("resource_incomplete") : TEXT("compile_errors"));
        }
        else { Observation->SetStringField(TEXT("reason"), TEXT("No current-platform resource is available; successful API calls do not establish shader compilation.")); }
    }
    else
    {
        Observation->SetStringField(TEXT("reason"), Asset && Asset->IsA<UMaterialFunctionInterface>()
            ? TEXT("Material Function has no standalone render resource; caller/wrapper material compilation is a separate proof scope.")
            : TEXT("Analysis did not resolve a loaded Material/MaterialInstance resource target."));
    }
    Data->SetObjectField(TEXT("ue58_compile_observation"), Observation);
    return AddDirectCollectionMetadata(Data, Path);
}
}

FString ToJson(const TSharedPtr<FJsonObject>& Object)
{
    FString Result;
    const auto Writer = TJsonWriterFactory<TCHAR, TCondensedJsonPrintPolicy<TCHAR>>::Create(&Result);
    FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    return Result;
}

FReadMetricsScope::FReadMetricsScope()
    : Previous(CurrentReadMetrics), StartedAt(FPlatformTime::Seconds())
{
    CurrentReadMetrics = this;
}

FReadMetricsScope::~FReadMetricsScope()
{
    CurrentReadMetrics = Previous;
}

void FReadMetricsScope::Increment(FName Name, int64 Amount)
{
    Counts.FindOrAdd(Name) += Amount;
}

TSharedRef<FJsonObject> FReadMetricsScope::ToJson() const
{
    auto Result = MakeShared<FJsonObject>();
    auto Counters = MakeShared<FJsonObject>();
    for (const auto& Pair : Counts) { Counters->SetNumberField(Pair.Key.ToString(), Pair.Value); }
    Result->SetObjectField(TEXT("observed_counts"), Counters);
    Result->SetNumberField(TEXT("native_elapsed_ms_before_final_serialization"), (FPlatformTime::Seconds() - StartedAt) * 1000.0);
    Result->SetStringField(TEXT("scope"), TEXT("this synchronous native helper call; excludes Python, journal, MCP transport and final envelope serialization"));
    Result->SetStringField(TEXT("counter_semantics"), TEXT("instrumented helper entries, loop visits and public getter call sites; not estimated engine-internal work; absent keys mean zero"));
    return Result;
}

bool IsMutation(EOperation Operation)
{
    return Operation == EOperation::ConnectNodes || Operation == EOperation::ConnectProperty || Operation == EOperation::DeleteNode || Operation == EOperation::LayoutNodes || IsAddOperation(Operation) || Operation == EOperation::SetNodeProperty;
}

bool PrepareCustomNodeRequest(const TSharedPtr<FJsonObject>& Input, TSharedPtr<FJsonObject>& OutParams, FString& ErrorCode, FString& Error)
{
    return PrepareCustomNodeRequestImpl(Input, OutParams, ErrorCode, Error);
}

FString OperationName(EOperation Operation)
{
    switch (Operation)
    {
        case EOperation::ResolveGraph: return TEXT("resolve_material_graph");
        case EOperation::ListNodes: return TEXT("list_material_nodes");
        case EOperation::AnalyzeGraph: return TEXT("analyze_material_graph");
        case EOperation::ListCollectionNodes: return TEXT("list_material_collection_parameter_nodes");
        case EOperation::GetCollectionValues: return TEXT("get_material_parameter_collection_values");
        case EOperation::ConnectNodes: return TEXT("connect_material_nodes");
        case EOperation::ConnectProperty: return TEXT("connect_material_property");
        case EOperation::DeleteNode: return TEXT("delete_material_node");
        case EOperation::LayoutNodes: return TEXT("layout_material_nodes");
        case EOperation::AddNode: return TEXT("add_material_node");
        case EOperation::SetNodeProperty: return TEXT("set_material_node_property");
        case EOperation::AddCustomNode: return TEXT("add_custom_material_node");
    }
    return TEXT("unknown");
}

bool CaptureState(EOperation Operation, const TSharedPtr<FJsonObject>& Params, int32 MaxGraphNodes, FState& OutState, FString& OutError)
{
    CountRead(TEXT("capture_state_calls"));
    if (Operation == EOperation::GetCollectionValues)
    {
        FString Path;
        Params->TryGetStringField(TEXT("collection_path"), Path);
        UMaterialParameterCollection* Collection = Cast<UMaterialParameterCollection>(LoadObjectValue(Path));
        if (!Collection) { OutError = TEXT("Material Parameter Collection not found: ") + Path; return false; }
        OutState.Asset.Reset(Collection);
        OutState.AssetPath = Collection->GetPathName();
        OutState.bDirty = Collection->GetOutermost()->IsDirty();
        OutState.Snapshot = ReadEffectiveCollectionValues(Params, MaxGraphNodes);
        if (OutState.Snapshot->TryGetStringField(TEXT("error"), OutError) && !OutError.IsEmpty()) { return false; }
        return true;
    }

    FString Path, GraphType;
    Params->TryGetStringField(TEXT("material_path"), Path);
    Params->TryGetStringField(TEXT("graph_type"), GraphType);
    FMaterialGraphTarget Target;
    UMaterialInstance* Instance = nullptr;
    if (Operation == EOperation::AnalyzeGraph)
    {
        UObject* Object = LoadObject<UObject>(nullptr, *NormalizeObjectPathForLoad(Path));
        if (!Object)
        {
            TArray<FString> Candidates = FindMaterialGraphAssetPaths(Path, GraphType);
            if (NormalizeMaterialGraphType(GraphType) != TEXT("function")) { AppendAssetCandidatesForClass(UMaterialInstance::StaticClass(), Path, Candidates); }
            if (Candidates.Num() == 1) { Object = LoadObject<UObject>(nullptr, *NormalizeObjectPathForLoad(Candidates[0])); }
        }
        Instance = Cast<UMaterialInstance>(Object);
        if (Instance)
        {
            Target.Material = Instance->GetMaterial();
            Target.Asset = Target.Material;
        }
    }
    TArray<FString> Candidates;
    if (!Target.IsValid() && !ResolveMaterialGraph(Path, GraphType, Target, &Candidates, OutError))
    {
        OutState.Snapshot = MakeShared<FJsonObject>();
        TArray<TSharedPtr<FJsonValue>> CandidateValues;
        for (const auto& Candidate : Candidates) { CandidateValues.Add(MakeShared<FJsonValueString>(Candidate)); }
        OutState.Snapshot->SetArrayField(TEXT("candidates"), CandidateValues);
        return false;
    }
    OutState.NodeCount = Target.GetExpressions().Num();
    if (OutState.NodeCount > MaxGraphNodes)
    {
        OutError = FString::Printf(TEXT("Graph has %d nodes; inspection budget is %d. No mutation was dispatched."), OutState.NodeCount, MaxGraphNodes);
        return false;
    }
    OutState.Asset.Reset(Instance ? static_cast<UObject*>(Instance) : Target.Asset);
    OutState.AssetPath = OutState.Asset->GetPathName();
    OutState.bDirty = OutState.Asset->GetOutermost()->IsDirty();
    CountRead(TEXT("graph_summary_capture_calls"));
    OutState.Snapshot = GraphSummaryToJson(Target, true);
    OutState.Snapshot->SetStringField(TEXT("asset_path"), OutState.AssetPath);
    if (Operation == EOperation::ListNodes || Operation == EOperation::AnalyzeGraph || Operation == EOperation::ListCollectionNodes)
    {
        auto Metadata = DirectCollectionMetadata(Target, MaxGraphNodes, OutError);
        OutState.Snapshot->SetObjectField(TEXT("ue58_collection_semantics"), Metadata);
        if (!OutError.IsEmpty()) { return false; }
        if (Operation == EOperation::ListCollectionNodes)
        {
            auto CollectionNodes = ReadEffectiveCollectionNodes(Params);
            OutState.Snapshot->SetObjectField(TEXT("effective_collection_nodes"), CollectionNodes);
            if (CollectionNodes->TryGetStringField(TEXT("error"), OutError) && !OutError.IsEmpty()) { return false; }
        }
    }
    if (Instance) { OutState.Snapshot->SetObjectField(TEXT("material_instance"), MaterialInstanceToJson(Instance)); }
    if (Target.Material)
    {
        TArray<TSharedPtr<FJsonValue>> Properties;
        for (int32 Index = 0; Index < MP_MAX; ++Index)
        {
            const auto Property = static_cast<EMaterialProperty>(Index);
            if (Target.Material->GetExpressionInputForProperty(Property))
            {
                Properties.Add(MakeShared<FJsonValueObject>(MaterialPropertyConnectionToJson(Target.Material, Property)));
            }
        }
        OutState.Snapshot->SetArrayField(TEXT("material_properties"), Properties);
    }
    if (Operation == EOperation::DeleteNode || Operation == EOperation::SetNodeProperty)
    {
        FString NodeId, Ignored;
        Params->TryGetStringField(TEXT("node_id"), NodeId);
        UMaterialExpression* Selected = nullptr;
        if (FindExpressionById(Target, NodeId, Selected, Ignored))
        {
            OutState.SelectedNodeKey = GetExpressionKey(Selected);
            OutState.SelectedExpression.Reset(Selected);
        }
    }
    if (Operation == EOperation::SetNodeProperty || IsAddOperation(Operation))
    {
        auto Observations = MakeShared<FJsonObject>();
        TArray<FString> Names;
        if (Operation == EOperation::SetNodeProperty) { Names.Add(Params->GetStringField(TEXT("property_name"))); }
        else
        {
            TSharedPtr<FJsonObject> Properties;
            FString Ignored;
            if (ReadMutationProperties(Params, Properties, Ignored))
            {
                for (const auto& Pair : Properties->Values) { Names.Add(FString(Pair.Key.ToView())); }
            }
        }
        Names.Sort();
        for (const auto& Node : Target.GetExpressions())
        {
            if (!Node || (Operation == EOperation::SetNodeProperty && Node.Get() != OutState.SelectedExpression.Get())) { continue; }
            auto Values = MakeShared<FJsonObject>();
            for (const FString& Name : Names)
            {
                if (FProperty* Property = FindPropertyByNameLoose(Node.Get(), Name))
                {
                    FString Export;
                    Property->ExportTextItem_Direct(Export, Property->ContainerPtrToValuePtr<void>(Node.Get()), nullptr, Node.Get(), PPF_None);
                    Values->SetStringField(Property->GetName(), Export);
                }
            }
            Observations->SetObjectField(GetExpressionKey(Node.Get()), Values);
        }
        OutState.Snapshot->SetObjectField(TEXT("requested_property_readback"), Observations);
    }
    return true;
}

bool ValidateMutationInputs(EOperation Operation, const TSharedPtr<FJsonObject>& Params, const FState& State, int32 MaxGraphNodes, FString& OutError)
{
    if (IsAddOperation(Operation))
    {
        if (State.NodeCount >= MaxGraphNodes) { OutError = TEXT("Adding a node would exceed max_graph_nodes."); return false; }
        UClass* Class = FindMaterialExpressionClass(Params->GetStringField(TEXT("expression_class")));
        if (!Class || !IsVerifiedRuntimeClass(Class) || Class->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists))
        {
            OutError = TEXT("Expression class must be a concrete, native Engine material expression; unaudited plugin/generated classes are not admitted."); return false;
        }
        TSharedPtr<FJsonObject> Properties;
        if (!ReadMutationProperties(Params, Properties, OutError)) { return false; }
        TSet<FName> Assigned;
        for (const auto& Pair : Properties->Values)
        {
            FPreparedMaterialProperty Prepared;
            if (!PrepareMaterialProperty(Class->GetDefaultObject(), State.Asset.Get(), FString(Pair.Key.ToView()), Pair.Value, Prepared, OutError)) { return false; }
            if (Assigned.Contains(Prepared.Property->GetFName())) { OutError = TEXT("Properties contains duplicate aliases of the same reflected member."); return false; }
            Assigned.Add(Prepared.Property->GetFName());
        }
        const FString SelectedPath = Params->GetStringField(TEXT("selected_asset"));
        if (!SelectedPath.IsEmpty())
        {
            UObject* Selected = LoadObjectValue(SelectedPath);
            if (!Selected) { OutError = TEXT("Selected asset was not found."); return false; }
            if (!ValidateReference(Selected, State.Asset.Get(), OutError)) { return false; }
            const bool bTexture = Class->IsChildOf(UMaterialExpressionTextureBase::StaticClass()) && Selected->IsA<UTexture>();
            const bool bFunction = Class->IsChildOf(UMaterialExpressionMaterialFunctionCall::StaticClass()) && Selected->IsA<UMaterialFunction>();
            const bool bCollection = Selected->IsA<UMaterialParameterCollection>() &&
                (Class->IsChildOf(UMaterialExpressionCollectionParameter::StaticClass()) || Class->GetName() == TEXT("MaterialExpressionCollectionTransform"));
            if (!bTexture && !bFunction && !bCollection) { OutError = TEXT("Selected asset is not consumed by this expression creation API/class pair."); return false; }
        }
    }
    else if (Operation == EOperation::SetNodeProperty)
    {
        if (!State.SelectedExpression.Get()) { OutError = TEXT("Node selector did not resolve uniquely."); return false; }
        if (!IsVerifiedRuntimeClass(State.SelectedExpression->GetClass())) { OutError = TEXT("Property editing requires an audited native Engine expression class."); return false; }
        TSharedPtr<FJsonValue> Value;
        FPreparedMaterialProperty Prepared;
        if (!ParseMutationJson(Params->GetStringField(TEXT("value_json")), Value, OutError) ||
            !PrepareMaterialProperty(State.SelectedExpression.Get(), State.Asset.Get(), Params->GetStringField(TEXT("property_name")), Value, Prepared, OutError)) { return false; }
    }
    return true;
}

bool ValidateMutationTarget(const FState& State, FString& OutError)
{
    UObject* Asset = State.Asset.Get();
    if (!Asset || !Asset->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))
    {
        OutError = TEXT("Material mutations require a resolved /Game/ asset; plugin and engine mounts are read-only here."); return false;
    }
    if (!IsSupportedExistingClass(Asset->GetClass()))
    {
        OutError = TEXT("Target asset uses a tool/editor-only class."); return false;
    }
    FMaterialGraphTarget Target;
    Target.Asset = Asset;
    Target.Material = Cast<UMaterial>(Asset);
    Target.Function = Cast<UMaterialFunction>(Asset);
    if (!Target.IsValid()) { OutError = TEXT("Expected a Material or Material Function asset."); return false; }
    for (const auto& Expression : Target.GetExpressions())
    {
        if (Expression && !IsSupportedExistingClass(Expression->GetClass()))
        {
            OutError = TEXT("Graph contains a tool/editor-only expression class: ") + Expression->GetClass()->GetPathName(); return false;
        }
    }
    return true;
}

TSharedPtr<FJsonObject> Execute(EOperation Operation, const TSharedPtr<FJsonObject>& Params)
{
    CountRead(TEXT("execute_dispatch_calls"));
    if (IsMutation(Operation))
    {
        FString Path, Type, Error;
        Params->TryGetStringField(TEXT("material_path"), Path);
        Params->TryGetStringField(TEXT("graph_type"), Type);
        FMaterialGraphTarget Target;
        if (!ResolveMaterialGraph(Path, Type, Target, nullptr, Error)) { return MaterialApiError(Error); }
        // UMaterialEditingLibrary::ConnectMaterialExpressions changes an input
        // directly. Capture subobject transaction state before any edit, too.
        for (const auto& Expression : Target.GetExpressions())
        {
            if (Expression) { Expression->Modify(false); }
        }
    }
    switch (Operation)
    {
        case EOperation::ResolveGraph: return HandleResolveMaterialGraph(Params);
        case EOperation::ListNodes: return ReadNodesWithCollectionMetadata(Params);
        case EOperation::AnalyzeGraph: return AnalyzeWithCompileObservation(Params);
        case EOperation::ListCollectionNodes: return ReadEffectiveCollectionNodes(Params);
        case EOperation::GetCollectionValues: return ReadEffectiveCollectionValues(Params, MaxCollectionReadParameters);
        case EOperation::ConnectNodes: return HandleConnectMaterialNodes(Params);
        case EOperation::ConnectProperty: return HandleConnectMaterialProperty(Params);
        case EOperation::DeleteNode: return HandleDeleteMaterialNode(Params);
        case EOperation::LayoutNodes: return HandleLayoutMaterialNodes(Params);
        case EOperation::AddNode: return HandleCubelessAddMaterialNode(Params);
        case EOperation::AddCustomNode: return HandleCubelessAddMaterialNode(Params);
        case EOperation::SetNodeProperty: return HandleCubelessSetMaterialNodeProperty(Params);
    }
    return MaterialApiError(TEXT("Unsupported material operation."));
}

bool TryReuseCapturedReadData(EOperation Operation, const FState& Before, TSharedPtr<FJsonObject>& OutData)
{
    if (!Before.Snapshot.IsValid()) { return false; }
    if (Operation == EOperation::GetCollectionValues)
    {
        OutData = Before.Snapshot;
    }
    else if (Operation == EOperation::ListCollectionNodes)
    {
        const TSharedPtr<FJsonObject>* Existing = nullptr;
        if (!Before.Snapshot->TryGetObjectField(TEXT("effective_collection_nodes"), Existing) || !Existing || !Existing->IsValid())
        {
            return false;
        }
        OutData = *Existing;
    }
    else { return false; }
    // Read-only response assembly does not modify this shared local payload.
    // The caller still independently captures After and compares it with Before.
    CountRead(TEXT("captured_read_payload_reuses"));
    return true;
}

bool VerifyMutation(EOperation Operation, const TSharedPtr<FJsonObject>& Params, const FState& Before, const FState& After, FString& OutMethod, FString& OutError)
{
    const auto BeforeNodes = SnapshotNodes(Before);
    const auto AfterNodes = SnapshotNodes(After);
    if (IsAddOperation(Operation) || Operation == EOperation::SetNodeProperty)
    {
        OutMethod = TEXT("node_identity_count_and_requested_reflected_property_readback");
        UMaterialExpression* Node = nullptr;
        FMaterialGraphTarget Target;
        Target.Asset = After.Asset.Get();
        Target.Material = Cast<UMaterial>(Target.Asset);
        Target.Function = Cast<UMaterialFunction>(Target.Asset);
        if (IsAddOperation(Operation))
        {
            if (After.NodeCount != Before.NodeCount + 1) { OutError = TEXT("Node creation did not increase the graph count by exactly one."); return false; }
            for (const auto& Expression : Target.GetExpressions())
            {
                if (Expression && !BeforeNodes.Contains(GetExpressionKey(Expression.Get())))
                {
                    if (Node) { OutError = TEXT("More than one new node was observed."); return false; }
                    Node = Expression.Get();
                }
            }
            if (!Node || Node->GetClass() != FindMaterialExpressionClass(Params->GetStringField(TEXT("expression_class")))) { OutError = TEXT("Created node class/identity did not match."); return false; }
            if (Node->MaterialExpressionEditorX != static_cast<int32>(Params->GetNumberField(TEXT("node_x"))) || Node->MaterialExpressionEditorY != static_cast<int32>(Params->GetNumberField(TEXT("node_y"))))
            {
                OutError = TEXT("Created node position did not match."); return false;
            }
        }
        else
        {
            Node = Cast<UMaterialExpression>(After.SelectedExpression.Get());
            if (!Node || Node != Before.SelectedExpression.Get() || After.NodeCount != Before.NodeCount) { OutError = TEXT("Property update changed node identity/count."); return false; }
        }
        if (!IsVerifiedRuntimeClass(Node->GetClass())) { OutError = TEXT("Resulting node class failed runtime ownership validation."); return false; }
        TSharedPtr<FJsonObject> Properties;
        if (IsAddOperation(Operation))
        {
            if (!ReadMutationProperties(Params, Properties, OutError)) { return false; }
        }
        else
        {
            TSharedPtr<FJsonValue> Value;
            if (!ParseMutationJson(Params->GetStringField(TEXT("value_json")), Value, OutError)) { return false; }
            Properties = MakeShared<FJsonObject>();
            Properties->SetField(Params->GetStringField(TEXT("property_name")), Value);
        }
        for (const auto& Pair : Properties->Values)
        {
            FPreparedMaterialProperty Expected;
            if (!PrepareMaterialProperty(Node, Target.Asset, FString(Pair.Key.ToView()), Pair.Value, Expected, OutError)) { return false; }
            if (!Expected.Property->Identical(Expected.Property->ContainerPtrToValuePtr<void>(Node), Expected.Storage.GetObjAddress(), PPF_None))
            {
                OutError = TEXT("Post-update property readback mismatch: ") + FString(Pair.Key.ToView()); return false;
            }
        }
        if (Operation == EOperation::AddCustomNode)
        {
            UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Node);
            if (!Custom || Custom->GetInputsView().Num() != Custom->Inputs.Num() || Custom->GetOutputs().Num() != 1)
            {
                OutError = TEXT("Custom input/output topology readback disagreed after rebuilding outputs."); return false;
            }
            for (int32 Index = 0; Index < Custom->Inputs.Num(); ++Index)
            {
                if (Custom->GetInput(Index) != &Custom->Inputs[Index].Input || Custom->GetInputName(Index) != Custom->Inputs[Index].InputName)
                {
                    OutError = TEXT("Custom input name/address readback disagreed with its typed input array."); return false;
                }
            }
            OutMethod = TEXT("custom_node_identity_properties_inputs_and_outputs_readback_no_shader_compile");
        }
        return true;
    }
    if (Operation == EOperation::DeleteNode)
    {
        OutMethod = TEXT("deleted_node_absent_count_decreased_and_no_dangling_inputs");
        if (Before.SelectedNodeKey.IsEmpty() || AfterNodes.Contains(Before.SelectedNodeKey) || After.NodeCount != Before.NodeCount - 1)
        {
            OutError = TEXT("Deletion readback did not match the requested node and count."); return false;
        }
        FMaterialGraphTarget Target;
        Target.Asset = After.Asset.Get();
        Target.Material = Cast<UMaterial>(Target.Asset);
        Target.Function = Cast<UMaterialFunction>(Target.Asset);
        UMaterialExpression* Deleted = Cast<UMaterialExpression>(Before.SelectedExpression.Get());
        for (const auto& Expression : Target.GetExpressions())
        {
            if (!Expression) { continue; }
            for (FExpressionInputIterator It(Expression.Get()); It; ++It)
            {
                if (It.Input->Expression == Deleted) { OutError = TEXT("A remaining input still references the deleted expression."); return false; }
            }
        }
        if (Target.Material)
        {
            for (int32 Index = 0; Index < MP_MAX; ++Index)
            {
                const FExpressionInput* Input = Target.Material->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Index));
                if (Input && Input->Expression == Deleted) { OutError = TEXT("A material property still references the deleted expression."); return false; }
            }
        }
        return true;
    }
    if (Operation == EOperation::LayoutNodes)
    {
        OutMethod = TEXT("node_positions_read_back_and_graph_topology_preserved");
        if (BeforeNodes.Num() != AfterNodes.Num()) { OutError = TEXT("Layout changed node count."); return false; }
        for (const auto& Pair : BeforeNodes)
        {
            const auto* AfterNode = AfterNodes.Find(Pair.Key);
            double X, Y;
            if (!AfterNode || NodeWithoutPosition(Pair.Value) != NodeWithoutPosition(*AfterNode) || !(*AfterNode)->TryGetNumberField(TEXT("x"), X) || !(*AfterNode)->TryGetNumberField(TEXT("y"), Y) || !FMath::IsFinite(X) || !FMath::IsFinite(Y))
            {
                OutError = TEXT("Layout readback found missing coordinates or an unexpected topology change."); return false;
            }
        }
        return true;
    }
    OutMethod = TEXT("engine_expression_input_and_output_index_readback");
    FString Path, Type, FromId, FromOutput;
    Params->TryGetStringField(TEXT("material_path"), Path);
    Params->TryGetStringField(TEXT("graph_type"), Type);
    Params->TryGetStringField(TEXT("from_node_id"), FromId);
    Params->TryGetStringField(TEXT("from_output"), FromOutput);
    FMaterialGraphTarget Target;
    if (!ResolveMaterialGraph(Path, Type, Target, nullptr, OutError)) { return false; }
    UMaterialExpression* From = nullptr;
    if (!FindExpressionById(Target, FromId, From, OutError)) { return false; }
    const int32 OutputIndex = FindOutputIndexByName(From, FromOutput);
    FExpressionInput* Input = nullptr;
    if (Operation == EOperation::ConnectProperty)
    {
        FString PropertyName;
        Params->TryGetStringField(TEXT("property"), PropertyName);
        EMaterialProperty Property;
        if (!Target.Material || !MaterialPropertyFromString(PropertyName, Property, OutError)) { return false; }
        Input = Target.Material->GetExpressionInputForProperty(Property);
    }
    else
    {
        FString ToId, ToInput;
        Params->TryGetStringField(TEXT("to_node_id"), ToId);
        Params->TryGetStringField(TEXT("to_input"), ToInput);
        UMaterialExpression* To = nullptr;
        if (!FindExpressionById(Target, ToId, To, OutError)) { return false; }
        const auto Names = GetMaterialExpressionInputNamesSafe(To);
        int32 InputIndex = ToInput.IsEmpty() ? 0 : INDEX_NONE;
        for (int32 Index = 0; Index < Names.Num(); ++Index)
        {
            if (Names[Index].Equals(ToInput, ESearchCase::IgnoreCase)) { InputIndex = Index; break; }
        }
        if (InputIndex != INDEX_NONE) { Input = To->GetInput(InputIndex); }
    }
    if (!Input || Input->Expression != From || Input->OutputIndex != OutputIndex)
    {
        OutError = TEXT("Connected expression input did not read back as requested."); return false;
    }
    return true;
}

TSharedPtr<FJsonObject> DescribeDelta(const FState& Before, const FState& After)
{
    const auto BeforeNodes = SnapshotNodes(Before);
    const auto AfterNodes = SnapshotNodes(After);
    TArray<TSharedPtr<FJsonValue>> Created, Removed, Updated;
    for (const auto& Pair : BeforeNodes)
    {
        const auto* NewNode = AfterNodes.Find(Pair.Key);
        if (!NewNode) { Removed.Add(MakeShared<FJsonValueString>(Pair.Key)); }
        else if (ToJson(Pair.Value) != ToJson(*NewNode)) { Updated.Add(MakeShared<FJsonValueString>(Pair.Key)); }
    }
    for (const auto& Pair : AfterNodes)
    {
        if (!BeforeNodes.Contains(Pair.Key)) { Created.Add(MakeShared<FJsonValueString>(Pair.Key)); }
    }
    auto Delta = MakeShared<FJsonObject>();
    Delta->SetArrayField(TEXT("created_node_keys"), Created);
    Delta->SetArrayField(TEXT("removed_node_keys"), Removed);
    Delta->SetArrayField(TEXT("updated_node_keys"), Updated);
    Delta->SetNumberField(TEXT("created_node_count"), Created.Num());
    Delta->SetNumberField(TEXT("removed_node_count"), Removed.Num());
    Delta->SetNumberField(TEXT("updated_node_count"), Updated.Num());
    auto BeforeProperties = MakeShared<FJsonObject>();
    auto AfterProperties = MakeShared<FJsonObject>();
    if (const auto* Value = Before.Snapshot->Values.Find(TEXT("material_properties"))) { BeforeProperties->SetField(TEXT("value"), *Value); }
    if (const auto* Value = After.Snapshot->Values.Find(TEXT("material_properties"))) { AfterProperties->SetField(TEXT("value"), *Value); }
    Delta->SetBoolField(TEXT("material_property_connections_changed"), ToJson(BeforeProperties) != ToJson(AfterProperties));
    Delta->SetStringField(TEXT("scope"), TEXT("observed expression JSON including links; not a complete binary package diff"));
    return Delta;
}
}
