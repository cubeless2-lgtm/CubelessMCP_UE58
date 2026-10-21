#include "Compatibility/UE58/CubelessChaosClothAuthoringApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/AssetData.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothCollectionGroup.h"
#include "ChaosClothAsset/ClothSimulationModel.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "ChaosClothAsset/CollectionClothSelectionFacade.h"
#include "ChaosClothAsset/WeightMapNode.h"
#include "Dataflow/DataflowBlueprintLibrary.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowInstance.h"
#include "Dataflow/DataflowNode.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowObjectInterface.h"
#include "Dataflow/DataflowToolNode.h"
#include "Dataflow/DataflowVertexAttributeEditableNode.h"
#include "Editor.h"
#include "Engine/World.h"
#include "FileHelpers.h"
#include "InteractiveToolChange.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Serialization/MemoryWriter.h"
#include "Serialization/ObjectAndNameAsStringProxyArchive.h"
#include "String/LexFromString.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/StructOnScope.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::ChaosClothAuthoring
{
namespace
{
using Cubeless::UE58::Variable::FResult;
constexpr int32 MaxJsonChars=67108864, MaxDepth=64, MaxSlots=2004096;
constexpr int64 MaxSnapshotBytes=67108864;
constexpr int32 MaxSnapshots=256, MaxMeshElements=2000000;
constexpr const TCHAR* CurrentTokenVersion=TEXT("cubeless_ue58_current_weight_map_v2");
bool Running=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedRef<FJsonObject>& O) { FString S;FJsonSerializer::Serialize(O,TJsonWriterFactory<>::Create(&S));return S; }
bool Dependency(const FString& V)
{
    FString S=FPackageName::ExportTextPathToObjectPath(V).TrimStartAndEnd();S.TrimQuotesInline();S.ReplaceInline(TEXT("\\"),TEXT("/"));
    for(const TCHAR* K:{TEXT("/Script/UnrealMCP"),TEXT("/UnrealMCP"),TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessControlRigTools"),TEXT("CubelessMCP_UE58"),TEXT("CubelessMCPUE58")}) { if(S.Contains(K,ESearchCase::IgnoreCase)) { return true; } }return false;
}
const TSharedPtr<FJsonValue>* FindJson(const TSharedPtr<FJsonObject>& O,const FString& Name)
{
    if(O.IsValid()) { for(const auto& Row:O->Values) { if(FString(Row.Key).Equals(Name,ESearchCase::CaseSensitive)) { return &Row.Value; } } }return nullptr;
}
struct FExactKeyFuncs:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>MaxJsonChars) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeyFuncs>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
        if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; }
    }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}
struct FWork
{
    FResult R;FString Command,FailedStage;TSharedPtr<FJsonObject> Params,Legacy;
    TSharedRef<FJsonObject> Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TArray<TSharedPtr<FJsonValue>> SaveRows;
    TStrongObjectPtr<UChaosClothAssetBase> Asset{nullptr};TStrongObjectPtr<UDataflow> Dataflow{nullptr};
    TWeakObjectPtr<UPackage> AssetPackage,DataflowPackage;TWeakObjectPtr<UClass> AssetClass,DataflowClass;
    TWeakObjectPtr<UWorld> World;bool WorldCaptured=false,AssetCaptured=false,DataflowCaptured=false,NodeCaptured=false,bWriting=false,bCompensating=false;
    TSharedPtr<FDataflowNode> Node;TArray<TSharedPtr<FDataflowNode>> GraphNodeHolds;TSharedPtr<UE::Dataflow::FGraph> Graph;FGuid Guid;const UScriptStruct* NodeStruct=nullptr;
    FString AssetPath,DataflowPath,AssetPackagePath,DataflowPackagePath;TSharedPtr<FJsonObject> InitialProjection;
    explicit FWork(const TCHAR* Name):Command(Name)
    {
        R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Calls);R.Data->SetObjectField(TEXT("postconditions"),Checks);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Prefix);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);
        R.Data->SetBoolField(TEXT("explicit_wrapper_wait_performed"),false);R.Data->SetBoolField(TEXT("public_SDK_may_synchronously_wait_or_compile"),true);
        R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);R.Data->SetBoolField(TEXT("snapshot_hash_durable_roundtrip_verified"),false);
        R.Data->SetStringField(TEXT("synchronous_limit"),TEXT("LoadObject/PostLoad, cloth getters, Dataflow regeneration/evaluation and exact SavePackages may synchronously execute SDK work. No worker UObject access, forced async wait or physics-effective completion claim is added."));
        R.Data->SetStringField(TEXT("Blueprint_compile_scope"),TEXT("No explicit Blueprint compiler call; Dataflow regeneration/build/derived work is counted and reported separately, including any internal SDK compilation."));
        R.Data->SetStringField(TEXT("node_state_token_version"),CurrentTokenVersion);
        for(const TCHAR* K:{TEXT("load_calls"),TEXT("load_returns"),TEXT("Dataflow_getter_calls"),TEXT("property_import_calls"),TEXT("property_export_calls"),TEXT("referencer_load_calls"),TEXT("evaluation_calls"),TEXT("evaluation_returns"),TEXT("snapshot_serialization_calls"),TEXT("explicit_modify_calls"),TEXT("property_write_calls"),TEXT("explicit_notify_calls"),TEXT("snapshot_setter_calls"),TEXT("snapshot_restore_calls"),TEXT("regeneration_calls"),TEXT("regeneration_returns"),TEXT("initial_save_calls"),TEXT("compensation_save_calls"),TEXT("explicit_Blueprint_compile_calls"),TEXT("explicit_wrapper_wait_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    void SDK(const TCHAR* Stage) { R.Stage=Stage;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true); }
    bool Fail(const TCHAR* Code,const FString& E) { if(R.Error.IsEmpty()) { R.ErrorCode=TEXT("chaos_cloth_authoring_")+FString(Code);R.Error=E;FailedStage=R.Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }return false; }
    bool Admit()
    {
        if(!IsInGameThread()||!GEditor) { return Fail(TEXT("editor_lost"),TEXT("Editor game-thread context is unavailable")); }
        if(WorldCaptured&&GEditor->GetEditorWorldContext().World()!=World.Get()) { return Fail(TEXT("world_changed"),TEXT("Editor world changed during this operation")); }
        if(bWriting&&(GEditor->PlayWorld||GEditor->GetPIEWorldContext())) { return Fail(TEXT("play_session_started"),TEXT("PIE/SIE became active after mutation admission")); }
        if(AssetCaptured&&(!Live(Asset.Get())||Asset->GetClass()!=AssetClass.Get()||Asset->GetPackage()!=AssetPackage.Get()||!Asset->GetPathName().Equals(AssetPath,ESearchCase::CaseSensitive)||!Asset->GetPackage()->GetName().Equals(AssetPackagePath,ESearchCase::CaseSensitive))) { return Fail(TEXT("asset_identity_lost"),TEXT("Captured cloth asset/class/package identity is no longer live")); }
        if(DataflowCaptured&&(!Live(Dataflow.Get())||Dataflow->GetClass()!=DataflowClass.Get()||Dataflow->GetPackage()!=DataflowPackage.Get()||!Dataflow->GetPathName().Equals(DataflowPath,ESearchCase::CaseSensitive)||!Dataflow->GetPackage()->GetName().Equals(DataflowPackagePath,ESearchCase::CaseSensitive))) { return Fail(TEXT("Dataflow_identity_lost"),TEXT("Captured Dataflow/class/package identity is no longer live")); }
        if(DataflowCaptured&&Asset->GetDataflowInstance().GetDataflowAsset()!=Dataflow.Get()) { return Fail(TEXT("Dataflow_binding_changed"),TEXT("Captured asset no longer owns the selected Dataflow")); }
        if(NodeCaptured)
        {
            const auto CurrentGraph=Dataflow->GetDataflow();if(CurrentGraph!=Graph||!CurrentGraph.IsValid()) { return Fail(TEXT("graph_replaced"),TEXT("Captured Dataflow graph was replaced")); }
            bool Found=false;for(const auto& N:CurrentGraph->GetNodes()) { if(N.IsValid()&&N->GetGuid()==Guid) { if(N!=Node||N->TypedScriptStruct()!=NodeStruct) { return Fail(TEXT("node_replaced"),TEXT("Captured node GUID now resolves to a different node/struct")); }Found=true;break; } }
            if(!Found) { return Fail(TEXT("node_removed"),TEXT("Captured Dataflow node was removed")); }
        }return true;
    }
    TSharedRef<FJsonObject> Projection() const
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("asset_live"),Live(Asset.Get()));J->SetBoolField(TEXT("Dataflow_live"),Live(Dataflow.Get()));
        if(Live(Asset.Get())) { J->SetStringField(TEXT("asset_path"),Asset->GetPathName());J->SetStringField(TEXT("asset_package"),Asset->GetPackage()->GetName());J->SetBoolField(TEXT("asset_package_dirty"),Asset->GetPackage()->IsDirty()); }
        if(Live(Dataflow.Get())) { J->SetStringField(TEXT("Dataflow_path"),Dataflow->GetPathName());J->SetStringField(TEXT("Dataflow_package"),Dataflow->GetPackage()->GetName());J->SetBoolField(TEXT("Dataflow_package_dirty"),Dataflow->GetPackage()->IsDirty()); }
        return J;
    }
    bool Capture(UChaosClothAssetBase* A)
    {
        Asset.Reset(A);if(!Live(A)) { return Fail(TEXT("asset_load_failed"),TEXT("Loaded cloth target is not live")); }AssetCaptured=true;AssetClass=A->GetClass();AssetPackage=A->GetPackage();AssetPath=A->GetPathName();AssetPackagePath=A->GetPackage()->GetName();return Admit();
    }
    bool Capture(UDataflow* D)
    {
        Dataflow.Reset(D);if(!Live(D)) { return Fail(TEXT("Dataflow_unavailable"),TEXT("Selected Dataflow is not live")); }DataflowCaptured=true;DataflowClass=D->GetClass();DataflowPackage=D->GetPackage();DataflowPath=D->GetPathName();DataflowPackagePath=D->GetPackage()->GetName();InitialProjection=Projection();R.Data->SetObjectField(TEXT("target_before"),InitialProjection);return Admit();
    }
    bool Capture(const TSharedPtr<FDataflowNode>& N)
    {
        if(!N.IsValid()||!N->TypedScriptStruct()) { return Fail(TEXT("node_unavailable"),TEXT("Selected Dataflow node/struct is unavailable")); }
        Node=N;Guid=N->GetGuid();NodeStruct=N->TypedScriptStruct();Graph=Dataflow->GetDataflow();if(!Graph.IsValid()||Graph->GetNodes().Num()>65536) { return Fail(TEXT("graph_limit"),TEXT("Selected graph is unavailable or exceeds 65,536 nodes")); }GraphNodeHolds=Graph->GetNodes();NodeCaptured=true;return Admit();
    }
    void Write(const TCHAR* Stage) { SDK(Stage);bWriting=true;R.bMutationAttempted=true; }
    FString Finish(const TSharedPtr<FJsonObject>& L)
    {
        Legacy=L.IsValid()?L:MakeShared<FJsonObject>();bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);
        bool Attempt=false,Changed=false;Legacy->TryGetBoolField(TEXT("mutation_attempted"),Attempt);Legacy->TryGetBoolField(TEXT("changed"),Changed);R.bMutationAttempted|=Attempt;R.bChanged|=Changed;
        bool SaveRequested=false;Legacy->TryGetBoolField(TEXT("save_requested"),SaveRequested);R.bSaveRequested|=SaveRequested;
        if(AssetCaptured||DataflowCaptured) { const auto After=Projection();R.Data->SetObjectField(TEXT("target_after"),After);if(InitialProjection.IsValid()&&!Json(InitialProjection.ToSharedRef()).Equals(Json(After),ESearchCase::CaseSensitive)) { R.bChanged=true; } }
        if(R.Error.IsEmpty()&&!Success) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Fail(TEXT("operation_failed"),E.IsEmpty()?TEXT("Chaos Cloth operation failed"):E); }
        if(Success&&!R.Error.IsEmpty()) { Legacy->SetBoolField(TEXT("legacy_success_before_terminal_identity_check"),true);Legacy->SetBoolField(TEXT("success"),false);Legacy->SetBoolField(TEXT("verified_success"),false);Legacy->SetBoolField(TEXT("do_not_retry"),true);Legacy->SetStringField(TEXT("error"),R.Error);Legacy->SetStringField(TEXT("success_semantics"),TEXT("failed_terminal_identity_check")); }
        R.bPostconditionPassed=Success&&R.Error.IsEmpty();R.bSuccess=R.bPostconditionPassed;Checks->SetBoolField(TEXT("legacy_postconditions_passed"),R.bPostconditionPassed);
        Checks->SetBoolField(TEXT("explicit_Blueprint_compile_calls_zero"),!R.bCompileAttempted);R.Data->SetArrayField(TEXT("actual_save_prefix"),SaveRows);
        bool AnyCleanSave=false;for(const auto& V:SaveRows) { bool CleanSaved=false;V->AsObject()->TryGetBoolField(TEXT("save_completed"),CleanSaved);AnyCleanSave|=CleanSaved; }
        bool InitialDataflowSaved=false,InitialClothSaved=false;Legacy->TryGetBoolField(TEXT("dataflow_saved"),InitialDataflowSaved);Legacy->TryGetBoolField(TEXT("cloth_asset_saved"),InitialClothSaved);
        R.Data->SetBoolField(TEXT("any_exact_save_clean_readback_verified"),AnyCleanSave);R.Data->SetBoolField(TEXT("all_requested_initial_packages_saved"),R.bSaveRequested&&InitialDataflowSaved&&InitialClothSaved);
        R.Data->SetStringField(TEXT("saved_flag_scope"),TEXT("saved means at least one singleton SavePackages call returned true. Captured-package clean readback is separate. Initial and compensation saves remain separate in actual_save_prefix. It does not mean all requested packages were saved or reloaded."));
        R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);R.Data->SetBoolField(TEXT("semantic_restore_is_atomic"),false);
        Prefix->SetBoolField(TEXT("explicit_mutation_attempted"),R.bMutationAttempted);Prefix->SetBoolField(TEXT("any_exact_package_SDK_save_reported_success"),R.bSaved);Prefix->SetBoolField(TEXT("compensation_phase_entered"),bCompensating);
        if(R.bSuccess) { R.Stage=TEXT("complete"); }else if(!FailedStage.IsEmpty()) { R.Stage=FailedStage; }
        return Cubeless::Operations::EncodeVariableResult(R,*Command);
    }
};
FWork* Work=nullptr;
bool Guard() { return !Work||Work->Admit(); }
void SDK(const TCHAR* Stage) { if(Work) { Work->SDK(Stage); } }
void Count(const TCHAR* Key) { if(Work) { Work->Count(Key); } }
TSharedPtr<FJsonObject> ErrorResponse(const FString& E)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("status"),TEXT("error"));J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),E);
    if(Work) { Work->Fail(TEXT("operation_failed"),E);J->SetBoolField(TEXT("mutation_attempted"),Work->R.bMutationAttempted);J->SetBoolField(TEXT("partial_target_modified"),Work->R.bMutationAttempted);J->SetBoolField(TEXT("do_not_retry"),Work->R.bMutationAttempted||!Work->R.bChangeStateKnown);J->SetBoolField(TEXT("save_requested"),Work->R.bSaveRequested);J->SetBoolField(TEXT("save_attempted"),Work->R.bSaveAttempted);J->SetBoolField(TEXT("rollback_attempted"),Work->bCompensating);J->SetBoolField(TEXT("rollback_semantic_restore"),false);J->SetBoolField(TEXT("rollback_succeeded"),false);J->SetBoolField(TEXT("rollback_transaction_retained"),Work->R.bMutationAttempted);J->SetBoolField(TEXT("owned_transaction_cancelled"),false);for(const auto& Row:Work->Prefix->Values) { J->SetField(FString(Row.Key),Row.Value); } }return J;
}
struct FLegacy
{
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& E) { return ErrorResponse(E); }
    static bool IsMCPDependencyReference(const FString& E) { return Dependency(E); }
    static FString GetMCPDependencyReferenceError(const FString& Field,const FString& E) { return FString::Printf(TEXT("MCP-only dependency reference rejected in %s: %s"),*Field,*E); }
    static void AddMutationResultContract(const TSharedPtr<FJsonObject>& Result,bool bDryRun,bool bMutationPerformed,bool bChanged,const TSharedPtr<FJsonObject>& Before,const TSharedPtr<FJsonObject>& After,const TSharedPtr<FJsonObject>& Delta,bool bVerificationPassed,const FString& VerificationMethod,const TArray<FString>& VerificationChecks,bool bStrictSuccess=true);
};

void FLegacy::AddMutationResultContract(
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
    bool bStrictSuccess)
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
struct FJsonParamParseState
{
    void AddError(const FString& Error)
    {
        Errors.Add(Error);
    }

    bool IsValid() const
    {
        return Errors.IsEmpty();
    }

    FString GetErrorMessage() const
    {
        return FString::Printf(TEXT("Invalid Chaos tuning parameters: %s"), *FString::Join(Errors, TEXT("; ")));
    }

private:
    TArray<FString> Errors;
};

FString GetStringParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    FJsonParamParseState& ParseState,
    const FString& Default = FString())
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params, Name);
    if (!JsonValue)
    {
        return Default;
    }
    FString Value;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::String || !(*JsonValue)->TryGetString(Value))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON string"), Name));
        return Default;
    }
    return Value;
}

bool GetBoolParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    bool bDefault,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return bDefault;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params, Name);
    if (!JsonValue)
    {
        return bDefault;
    }
    bool bValue = bDefault;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Boolean || !(*JsonValue)->TryGetBool(bValue))
    {
        ParseState.AddError(FString::Printf(TEXT("%s must be a JSON boolean"), Name));
        return bDefault;
    }
    return bValue;
}

int32 GetIntParam(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* Name,
    int32 Default,
    FJsonParamParseState& ParseState)
{
    if (!Params.IsValid())
    {
        return Default;
    }
    const TSharedPtr<FJsonValue>* JsonValue = FindJson(Params, Name);
    if (!JsonValue)
    {
        return Default;
    }
    double Value = 0.0;
    if (!JsonValue->IsValid() || (*JsonValue)->Type != EJson::Number ||
        !(*JsonValue)->TryGetNumber(Value) || !FMath::IsFinite(Value) ||
        Value != FMath::TruncToDouble(Value) ||
        Value < static_cast<double>(TNumericLimits<int32>::Min()) ||
        Value > static_cast<double>(TNumericLimits<int32>::Max()))
    {
        ParseState.AddError(FString::Printf(
            TEXT("%s must be a JSON number containing a 32-bit signed integer"),
            Name));
        return Default;
    }
    return static_cast<int32>(Value);
}

TArray<TSharedPtr<FJsonValue>> Vector3fToJson(const FVector3f& Value)
{
    return {
        MakeShared<FJsonValueNumber>(Value.X),
        MakeShared<FJsonValueNumber>(Value.Y),
        MakeShared<FJsonValueNumber>(Value.Z)};
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(Values.Num());
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

FString JsonObjectToString(const TSharedPtr<FJsonObject>& Object)
{
    FString Result;
    if (Object.IsValid())
    {
        const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&Result);
        FJsonSerializer::Serialize(Object.ToSharedRef(), Writer);
    }
    return Result;
}

bool JsonValueMatches(const TSharedPtr<FJsonValue>& Expected, const TSharedPtr<FJsonValue>& Actual)
{
    if (!Expected.IsValid() || !Actual.IsValid())
    {
        return !Expected.IsValid() && !Actual.IsValid();
    }
    if (Expected->IsNull() || Actual->IsNull())
    {
        return Expected->IsNull() && Actual->IsNull();
    }
    if (Expected->Type != Actual->Type)
    {
        return false;
    }

    if (Expected->Type == EJson::String) { return Expected->AsString().Equals(Actual->AsString(), ESearchCase::CaseSensitive); }
    double ExpectedNumber = 0.0;
    double ActualNumber = 0.0;
    if (Expected->Type == EJson::Number && Expected->TryGetNumber(ExpectedNumber) && Actual->TryGetNumber(ActualNumber))
    {
        return FMath::IsNearlyEqual(ExpectedNumber, ActualNumber, 1.e-6);
    }

    bool bExpectedBool = false;
    bool bActualBool = false;
    if (Expected->TryGetBool(bExpectedBool) && Actual->TryGetBool(bActualBool))
    {
        return bExpectedBool == bActualBool;
    }

    FString ExpectedString;
    FString ActualString;
    if (Expected->TryGetString(ExpectedString) && Actual->TryGetString(ActualString))
    {
        return ExpectedString.Equals(ActualString, ESearchCase::CaseSensitive);
    }

    const TArray<TSharedPtr<FJsonValue>>* ExpectedArray = nullptr;
    const TArray<TSharedPtr<FJsonValue>>* ActualArray = nullptr;
    if (Expected->TryGetArray(ExpectedArray) && Actual->TryGetArray(ActualArray) &&
        ExpectedArray && ActualArray)
    {
        if (ExpectedArray->Num() != ActualArray->Num())
        {
            return false;
        }
        for (int32 Index = 0; Index < ExpectedArray->Num(); ++Index)
        {
            if (!JsonValueMatches((*ExpectedArray)[Index], (*ActualArray)[Index]))
            {
                return false;
            }
        }
        return true;
    }

    const TSharedPtr<FJsonObject>* ExpectedObject = nullptr;
    const TSharedPtr<FJsonObject>* ActualObject = nullptr;
    if (Expected->TryGetObject(ExpectedObject) && Actual->TryGetObject(ActualObject) &&
        ExpectedObject && ActualObject && ExpectedObject->IsValid() && ActualObject->IsValid())
    {
        for (const auto& Pair : (*ExpectedObject)->Values)
        {
            const TSharedPtr<FJsonValue>* ActualNested = FindJson(*ActualObject, FString(Pair.Key));
            if (!ActualNested || !JsonValueMatches(Pair.Value, *ActualNested))
            {
                return false;
            }
        }
        return true;
    }
    return false;
}

bool CheckExpectedFields(
    const TSharedPtr<FJsonObject>& Expected,
    const TSharedPtr<FJsonObject>& Actual,
    TArray<FString>& OutMismatches)
{
    if (!Expected.IsValid())
    {
        return true;
    }
    if (!Actual.IsValid())
    {
        OutMismatches.Add(TEXT("actual snapshot is unavailable"));
        return false;
    }

    for (const auto& Pair : Expected->Values)
    {
        const TSharedPtr<FJsonValue>* ActualValue = FindJson(Actual, FString(Pair.Key));
        if (!ActualValue)
        {
            OutMismatches.Add(FString::Printf(TEXT("%s is not a supported/readable field"), *FString(Pair.Key)));
        }
        else if (!JsonValueMatches(Pair.Value, *ActualValue))
        {
            OutMismatches.Add(FString::Printf(TEXT("%s does not match expected_current"), *FString(Pair.Key)));
        }
    }
    return OutMismatches.IsEmpty();
}

TSharedPtr<FJsonObject> SelectJsonFields(
    const TSharedPtr<FJsonObject>& Source,
    const TSharedPtr<FJsonObject>& FieldSelector)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Source.IsValid() || !FieldSelector.IsValid())
    {
        return Result;
    }
    for (const auto& Pair : FieldSelector->Values)
    {
        if (const TSharedPtr<FJsonValue>* Value = FindJson(Source, FString(Pair.Key)))
        {
            Result->SetField(FString(Pair.Key), *Value);
        }
    }
    return Result;
}
bool ReadNumber(const TSharedPtr<FJsonValue>& Value, double& OutValue)
{
    return Value.IsValid() && Value->Type == EJson::Number && Value->TryGetNumber(OutValue);
}

bool ReadBool(const TSharedPtr<FJsonValue>& Value, bool& bOutValue)
{
    return Value.IsValid() && Value->Type == EJson::Boolean && Value->TryGetBool(bOutValue);
}

bool ReadString(const TSharedPtr<FJsonValue>& Value, FString& OutValue)
{
    return Value.IsValid() && Value->Type == EJson::String && Value->TryGetString(OutValue);
}

bool IsFiniteFloatRange(double Value)
{
    return FMath::IsFinite(Value) &&
        FMath::Abs(Value) <= static_cast<double>(TNumericLimits<float>::Max());
}
TSharedPtr<FJsonObject> MakeDeltaCounts(bool bChanged, bool bFailed = false)
{
    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), bChanged ? 1 : 0);
    Delta->SetNumberField(TEXT("created_count"), 0);
    Delta->SetNumberField(TEXT("updated_count"), bChanged ? 1 : 0);
    Delta->SetNumberField(TEXT("removed_count"), 0);
    Delta->SetNumberField(TEXT("skipped_count"), bChanged ? 0 : 1);
    Delta->SetNumberField(TEXT("failed_count"), bFailed ? 1 : 0);
    return Delta;
}

void AlignMutationAttemptContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bMutationAttempted)
{
    if (!Result.IsValid())
    {
        return;
    }
    Result->SetBoolField(TEXT("mutation_attempted"), bMutationAttempted);
    const TSharedPtr<FJsonObject>* MutationPtr = nullptr;
    if (Result->TryGetObjectField(TEXT("mutation"), MutationPtr) &&
        MutationPtr && MutationPtr->IsValid())
    {
        (*MutationPtr)->SetBoolField(TEXT("attempted"), bMutationAttempted);
    }
}

bool IsPlaySessionActive()
{
    return GEditor && GEditor->GetPIEWorldContext() != nullptr;
}

TSharedPtr<FJsonObject> MakePropertyFlagsSnapshot(
    const Chaos::Softs::FCollectionPropertyConstFacade& Facade,
    int32 PropertyIndex)
{
    TSharedPtr<FJsonObject> Flags = MakeShared<FJsonObject>();
    Flags->SetNumberField(TEXT("raw"), static_cast<uint8>(Facade.GetFlags(PropertyIndex)));
    Flags->SetBoolField(TEXT("enabled"), Facade.IsEnabled(PropertyIndex));
    Flags->SetBoolField(TEXT("animatable"), Facade.IsAnimatable(PropertyIndex));
    Flags->SetBoolField(TEXT("legacy"), Facade.IsLegacy(PropertyIndex));
    Flags->SetBoolField(TEXT("interpolable"), Facade.IsInterpolable(PropertyIndex));
    Flags->SetBoolField(TEXT("intrinsic"), Facade.IsIntrinsic(PropertyIndex));
    Flags->SetBoolField(TEXT("dirty"), Facade.IsDirty(PropertyIndex));
    Flags->SetBoolField(TEXT("string_dirty"), Facade.IsStringDirty(PropertyIndex));
    return Flags;
}

TSharedPtr<FJsonObject> MakeCollectionPropertySnapshot(
    const Chaos::Softs::FCollectionPropertyConstFacade& Facade,
    int32 PropertyIndex,
    bool bIncludeValues,
    bool bIncludeFlags)
{
    TSharedPtr<FJsonObject> Property = MakeShared<FJsonObject>();
    Property->SetStringField(TEXT("name"), Facade.GetKeyName(PropertyIndex).ToString());
    if (bIncludeValues)
    {
        Property->SetArrayField(
            TEXT("low_raw_vector"),
            Vector3fToJson(Facade.GetLowValue<FVector3f>(PropertyIndex)));
        Property->SetArrayField(
            TEXT("high_raw_vector"),
            Vector3fToJson(Facade.GetHighValue<FVector3f>(PropertyIndex)));
        Property->SetStringField(TEXT("string_value"), Facade.GetStringValue(PropertyIndex));
        Property->SetStringField(
            TEXT("value_type_contract"),
            TEXT("UE 5.6 collection properties store numeric values as raw FVector3f; the semantic type is defined by the consuming solver key"));
    }
    if (bIncludeFlags)
    {
        Property->SetObjectField(TEXT("flags"), MakePropertyFlagsSnapshot(Facade, PropertyIndex));
    }
    return Property;
}

bool IsSupportedDataflowProperty(const FProperty* Property)
{
    if (!Property ||
        Property->GetOwnerStruct() == FDataflowNode::StaticStruct() ||
        !Property->HasAnyPropertyFlags(CPF_Edit) ||
        Property->HasAnyPropertyFlags(CPF_Transient | CPF_Deprecated | CPF_DisableEditOnInstance | CPF_NativeAccessSpecifierPrivate | CPF_NativeAccessSpecifierProtected))
    {
        return false;
    }

    return CastField<FBoolProperty>(Property) ||
        CastField<FNumericProperty>(Property) ||
        CastField<FEnumProperty>(Property) ||
        CastField<FByteProperty>(Property) ||
        CastField<FStrProperty>(Property) ||
        CastField<FNameProperty>(Property) ||
        CastField<FStructProperty>(Property);
}

FString ExportDataflowProperty(
    const FDataflowNode& Node,
    const FProperty& Property)
{
    FString Exported;
    const void* ValuePtr = Property.ContainerPtrToValuePtr<void>(&Node);
    SDK(TEXT("export_Dataflow_property"));Count(TEXT("property_export_calls"));
    Property.ExportTextItem_Direct(Exported, ValuePtr, nullptr, nullptr, PPF_None);
    return Exported;
}

bool IsClothDataflowNode(const FDataflowNode& Node)
{
    const UScriptStruct* Struct = Node.TypedScriptStruct();
    return Struct &&
        (Struct->HasMetaData(TEXT("DataflowCloth")) ||
         Struct->GetName().StartsWith(TEXT("ChaosClothAsset")) ||
         Node.GetCategory().ToString().Contains(TEXT("Cloth"), ESearchCase::IgnoreCase));
}

TArray<FString> GetConnectedDataflowInputPaths(
    const FDataflowNode& Node,
    const FProperty& TopLevelProperty)
{
    TArray<FString> Result;
    const FString TopLevelName = TopLevelProperty.GetName();
    const FString StructDescendantPrefix = TopLevelName + TEXT(".");
    const FString ArrayDescendantPrefix = TopLevelName + TEXT("[");
    for (const FDataflowInput* Input : Node.GetInputs())
    {
        if (!Input || !Input->HasAnyConnections())
        {
            continue;
        }
        const FString InputPath = Input->GetName().ToString();
        if (InputPath.Equals(TopLevelName, ESearchCase::IgnoreCase) ||
            InputPath.StartsWith(StructDescendantPrefix, ESearchCase::IgnoreCase) ||
            InputPath.StartsWith(ArrayDescendantPrefix, ESearchCase::IgnoreCase))
        {
            Result.AddUnique(InputPath);
        }
    }
    Result.Sort();
    return Result;
}

TSharedPtr<FJsonObject> MakeDataflowPropertySnapshot(
    const FDataflowNode& Node,
    const FProperty& Property)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("name"), Property.GetName());
    Result->SetStringField(TEXT("cpp_type"), Property.GetCPPType());
    Result->SetStringField(TEXT("display_name"), Property.GetDisplayNameText().ToString());
    Result->SetStringField(TEXT("category"), Property.GetMetaData(TEXT("Category")));
    Result->SetStringField(TEXT("tooltip"), Property.GetToolTipText().ToString());
    Result->SetStringField(TEXT("export_text"), ExportDataflowProperty(Node, Property));
    Result->SetBoolField(TEXT("supported_for_mcp_write"), IsSupportedDataflowProperty(&Property));
    const TArray<FString> ConnectedInputPaths =
        GetConnectedDataflowInputPaths(Node, Property);
    Result->SetBoolField(TEXT("input_connected"), !ConnectedInputPaths.IsEmpty());
    Result->SetArrayField(
        TEXT("connected_input_paths"),
        StringsToJson(ConnectedInputPaths));

    TSharedPtr<FJsonObject> Metadata = MakeShared<FJsonObject>();
    static const TCHAR* MetadataNames[] = {
        TEXT("ClampMin"), TEXT("ClampMax"), TEXT("UIMin"), TEXT("UIMax"),
        TEXT("EditCondition"), TEXT("InteractorName"), TEXT("Units")};
    for (const TCHAR* MetadataName : MetadataNames)
    {
        const FString Value = Property.GetMetaData(MetadataName);
        if (!Value.IsEmpty())
        {
            Metadata->SetStringField(MetadataName, Value);
        }
    }
    Result->SetObjectField(TEXT("metadata"), Metadata);
    return Result;
}

FString HashFloatValues(const TConstArrayView<float> Values)
{
    FMD5 Md5;
    const int32 Count = Values.Num();
    Md5.Update(reinterpret_cast<const uint8*>(&Count), sizeof(Count));
    if (!Values.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Values.GetData()),
            static_cast<uint32>(Values.Num() * sizeof(float)));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

void UpdateMd5WithString(FMD5& Md5, const FString& Value)
{
    const FTCHARToUTF8 Utf8(*Value);
    const int32 ByteCount = Utf8.Length();
    Md5.Update(reinterpret_cast<const uint8*>(&ByteCount), sizeof(ByteCount));
    if (ByteCount > 0)
    {
        Md5.Update(reinterpret_cast<const uint8*>(Utf8.Get()), ByteCount);
    }
}
bool ReadWeightMapNodeOutputName(
    const FDataflowNode& Node,
    FString& OutWeightMapName)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    if (!Struct)
    {
        return false;
    }

    if (const FStructProperty* const OutputNameProperty =
            FindFProperty<FStructProperty>(Struct, TEXT("OutputName")))
    {
        const FStrProperty* const StringValueProperty =
            FindFProperty<FStrProperty>(OutputNameProperty->Struct, TEXT("StringValue"));
        if (!StringValueProperty)
        {
            return false;
        }
        const void* const OutputNamePtr =
            OutputNameProperty->ContainerPtrToValuePtr<void>(&Node);
        const void* const StringValuePtr =
            StringValueProperty->ContainerPtrToValuePtr<void>(OutputNamePtr);
        OutWeightMapName = StringValueProperty->GetPropertyValue(StringValuePtr);
        return true;
    }

    if (const FStrProperty* const LegacyNameProperty =
            FindFProperty<FStrProperty>(Struct, TEXT("Name")))
    {
        OutWeightMapName = LegacyNameProperty->GetPropertyValue_InContainer(&Node);
        return true;
    }
    return false;
}
bool ReadExactDataflowEnumValue(
    const FDataflowNode& Node,
    const FName PropertyName,
    const FString& ExpectedEnumName,
    int64& OutValue)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    const FEnumProperty* const EnumProperty = Struct
        ? FindFProperty<FEnumProperty>(Struct, PropertyName)
        : nullptr;
    const UEnum* const Enum = EnumProperty ? EnumProperty->GetEnum() : nullptr;
    if (!EnumProperty || !Enum || Enum->GetName() != ExpectedEnumName)
    {
        return false;
    }
    const void* const ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(&Node);
    OutValue = EnumProperty->GetUnderlyingProperty()->GetSignedIntPropertyValue(ValuePtr);
    return true;
}

bool WriteExactDataflowEnumValue(
    FDataflowNode& Node,
    const FName PropertyName,
    const FString& ExpectedEnumName,
    const FString& EnumeratorName)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    FEnumProperty* const EnumProperty = Struct
        ? FindFProperty<FEnumProperty>(Struct, PropertyName)
        : nullptr;
    const UEnum* const Enum = EnumProperty ? EnumProperty->GetEnum() : nullptr;
    if (!EnumProperty || !Enum || Enum->GetName() != ExpectedEnumName)
    {
        return false;
    }
    int64 EnumeratorValue = Enum->GetValueByNameString(EnumeratorName);
    if (EnumeratorValue == INDEX_NONE)
    {
        EnumeratorValue = Enum->GetValueByName(
            FName(*FString::Printf(TEXT("%s::%s"), *ExpectedEnumName, *EnumeratorName)));
    }
    if (EnumeratorValue == INDEX_NONE)
    {
        return false;
    }
    if(!Guard()) { return false; }SDK(TEXT("set_public_weightmap_enum"));Count(TEXT("property_write_calls"));
    void* const ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(&Node);
    EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, EnumeratorValue);
    int64 Readback = INDEX_NONE;
    return ReadExactDataflowEnumValue(
               Node,
               PropertyName,
               ExpectedEnumName,
               Readback) &&
        Readback == EnumeratorValue;
}

bool WriteExactDataflowEnumRawValue(
    FDataflowNode& Node,
    const FName PropertyName,
    const FString& ExpectedEnumName,
    int64 Value)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    FEnumProperty* const EnumProperty = Struct
        ? FindFProperty<FEnumProperty>(Struct, PropertyName)
        : nullptr;
    const UEnum* const Enum = EnumProperty ? EnumProperty->GetEnum() : nullptr;
    if (!EnumProperty || !Enum || Enum->GetName() != ExpectedEnumName ||
        !Enum->IsValidEnumValue(Value))
    {
        return false;
    }
    if(!Guard()) { return false; }SDK(TEXT("set_public_weightmap_enum"));Count(TEXT("property_write_calls"));
    void* const ValuePtr = EnumProperty->ContainerPtrToValuePtr<void>(&Node);
    EnumProperty->GetUnderlyingProperty()->SetIntPropertyValue(ValuePtr, Value);
    int64 Readback = INDEX_NONE;
    return ReadExactDataflowEnumValue(
               Node,
               PropertyName,
               ExpectedEnumName,
               Readback) &&
        Readback == Value;
}

bool ExactDataflowEnumMatches(
    const FDataflowNode& Node,
    const FName PropertyName,
    const FString& ExpectedEnumName,
    const FString& EnumeratorName,
    int64* OutCurrentValue = nullptr)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    const FEnumProperty* const EnumProperty = Struct
        ? FindFProperty<FEnumProperty>(Struct, PropertyName)
        : nullptr;
    const UEnum* const Enum = EnumProperty ? EnumProperty->GetEnum() : nullptr;
    int64 CurrentValue = INDEX_NONE;
    if (!EnumProperty || !Enum || Enum->GetName() != ExpectedEnumName ||
        !ReadExactDataflowEnumValue(
            Node,
            PropertyName,
            ExpectedEnumName,
            CurrentValue))
    {
        return false;
    }
    if (OutCurrentValue)
    {
        *OutCurrentValue = CurrentValue;
    }
    int64 ExpectedValue = Enum->GetValueByNameString(EnumeratorName);
    if (ExpectedValue == INDEX_NONE)
    {
        ExpectedValue = Enum->GetValueByName(
            FName(*FString::Printf(TEXT("%s::%s"), *ExpectedEnumName, *EnumeratorName)));
    }
    return ExpectedValue != INDEX_NONE && CurrentValue == ExpectedValue;
}
void NotifyWeightMapNodeChanged(FDataflowNode& Node)
{
    if(!Guard()) { return; }SDK(TEXT("invalidate_weightmap_node"));Count(TEXT("explicit_notify_calls"));Node.Invalidate();
    Guard();
}
UChaosClothAssetBase* LoadClothAsset(const FString& AssetPath, FString& OutError)
{
    if (AssetPath.IsEmpty())
    {
        OutError = TEXT("cloth_asset_path is required");
        return nullptr;
    }
    if (FLegacy::IsMCPDependencyReference(AssetPath))
    {
        OutError = FLegacy::GetMCPDependencyReferenceError(
            TEXT("cloth_asset_path"),
            AssetPath);
        return nullptr;
    }
    SDK(TEXT("load_cloth_asset"));Count(TEXT("load_calls"));
    UChaosClothAssetBase* Asset = LoadObject<UChaosClothAssetBase>(nullptr, *AssetPath);Count(TEXT("load_returns"));
    if (Asset && Work && !Work->Capture(Asset)) { OutError=Work->R.Error;return nullptr; }
    if (!Asset)
    {
        OutError = FString::Printf(
            TEXT("Unable to load a UChaosClothAssetBase at '%s'"),
            *AssetPath);
    }
    return Asset;
}

FString HashSimulationTopology(
    const TConstArrayView<FVector3f> Positions,
    const TConstArrayView<uint32> Indices)
{
    FMD5 Md5;
    const int32 PositionCount = Positions.Num();
    const int32 IndexCount = Indices.Num();
    Md5.Update(reinterpret_cast<const uint8*>(&PositionCount), sizeof(PositionCount));
    if (!Positions.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Positions.GetData()),
            static_cast<uint32>(Positions.Num() * sizeof(FVector3f)));
    }
    Md5.Update(reinterpret_cast<const uint8*>(&IndexCount), sizeof(IndexCount));
    if (!Indices.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Indices.GetData()),
            static_cast<uint32>(Indices.Num() * sizeof(uint32)));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

struct FCanonicalTetherEntry
{
    int32 DynamicIndex = INDEX_NONE;
    int32 KinematicIndex = INDEX_NONE;
    float ReferenceLength = 0.0f;
};

struct FClothTetherState
{
    bool bValid = false;
    int32 VertexCount = 0;
    int32 BatchCount = 0;
    int32 TetherCount = 0;
    FString TetherMd5;
    FString Error;
};

FString HashCanonicalTethers(
    const int32 VertexCount,
    TArray<FCanonicalTetherEntry> Entries)
{
    Entries.Sort([](
        const FCanonicalTetherEntry& Left,
        const FCanonicalTetherEntry& Right)
    {
        if (Left.DynamicIndex != Right.DynamicIndex)
        {
            return Left.DynamicIndex < Right.DynamicIndex;
        }
        if (Left.KinematicIndex != Right.KinematicIndex)
        {
            return Left.KinematicIndex < Right.KinematicIndex;
        }
        uint32 LeftBits = 0;
        uint32 RightBits = 0;
        FMemory::Memcpy(&LeftBits, &Left.ReferenceLength, sizeof(LeftBits));
        FMemory::Memcpy(&RightBits, &Right.ReferenceLength, sizeof(RightBits));
        return LeftBits < RightBits;
    });

    FMD5 Md5;
    const int32 TetherCount = Entries.Num();
    Md5.Update(
        reinterpret_cast<const uint8*>(&VertexCount),
        sizeof(VertexCount));
    Md5.Update(
        reinterpret_cast<const uint8*>(&TetherCount),
        sizeof(TetherCount));
    for (const FCanonicalTetherEntry& Entry : Entries)
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(&Entry.DynamicIndex),
            sizeof(Entry.DynamicIndex));
        Md5.Update(
            reinterpret_cast<const uint8*>(&Entry.KinematicIndex),
            sizeof(Entry.KinematicIndex));
        uint32 ReferenceLengthBits = 0;
        FMemory::Memcpy(
            &ReferenceLengthBits,
            &Entry.ReferenceLength,
            sizeof(ReferenceLengthBits));
        Md5.Update(
            reinterpret_cast<const uint8*>(&ReferenceLengthBits),
            sizeof(ReferenceLengthBits));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

FClothTetherState ReadCollectionTetherState(
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade& Cloth)
{
    FClothTetherState State;
    if (!Cloth.IsValid())
    {
        State.Error = TEXT("The cloth collection schema is invalid");
        return State;
    }

    State.VertexCount = Cloth.GetNumSimVertices3D();
    const TConstArrayView<TArray<int32>> KinematicIndices =
        Cloth.GetTetherKinematicIndex();
    const TConstArrayView<TArray<float>> ReferenceLengths =
        Cloth.GetTetherReferenceLength();
    if (KinematicIndices.Num() != State.VertexCount ||
        ReferenceLengths.Num() != State.VertexCount)
    {
        State.Error = TEXT("Collection tether arrays do not match SimVertices3D count");
        return State;
    }

    TArray<FCanonicalTetherEntry> Entries;
    for (int32 DynamicIndex = 0;
         DynamicIndex < State.VertexCount;
         ++DynamicIndex)
    {
        if (KinematicIndices[DynamicIndex].Num() !=
            ReferenceLengths[DynamicIndex].Num())
        {
            State.Error = FString::Printf(
                TEXT("Collection tether index/length counts differ at dynamic vertex %d"),
                DynamicIndex);
            return State;
        }
        Entries.Reserve(
            Entries.Num() + KinematicIndices[DynamicIndex].Num());
        for (int32 TetherIndex = 0;
             TetherIndex < KinematicIndices[DynamicIndex].Num();
             ++TetherIndex)
        {
            const int32 KinematicIndex =
                KinematicIndices[DynamicIndex][TetherIndex];
            const float ReferenceLength =
                ReferenceLengths[DynamicIndex][TetherIndex];
            if (KinematicIndex < 0 ||
                KinematicIndex >= State.VertexCount ||
                !FMath::IsFinite(ReferenceLength) ||
                ReferenceLength < 0.0f)
            {
                State.Error = FString::Printf(
                    TEXT("Collection tether %d at dynamic vertex %d is invalid"),
                    TetherIndex,
                    DynamicIndex);
                return State;
            }
            Entries.Add({DynamicIndex, KinematicIndex, ReferenceLength});
        }
    }

    State.bValid = true;
    State.TetherCount = Entries.Num();
    State.TetherMd5 = HashCanonicalTethers(State.VertexCount, MoveTemp(Entries));
    return State;
}

FClothTetherState ReadCompiledTetherState(
    const FChaosClothSimulationLodModel& CompiledLod)
{
    FClothTetherState State;
    State.VertexCount = CompiledLod.Positions.Num();
    State.BatchCount = CompiledLod.TetherData.Tethers.Num();
    TArray<FCanonicalTetherEntry> Entries;
    for (const TArray<TTuple<int32, int32, float>>& Batch :
         CompiledLod.TetherData.Tethers)
    {
        Entries.Reserve(Entries.Num() + Batch.Num());
        for (const TTuple<int32, int32, float>& Tether : Batch)
        {
            const int32 KinematicIndex = Tether.Get<0>();
            const int32 DynamicIndex = Tether.Get<1>();
            const float ReferenceLength = Tether.Get<2>();
            if (KinematicIndex < 0 ||
                KinematicIndex >= State.VertexCount ||
                DynamicIndex < 0 ||
                DynamicIndex >= State.VertexCount ||
                !FMath::IsFinite(ReferenceLength) ||
                ReferenceLength < 0.0f)
            {
                State.Error = TEXT("Compiled solver tether data contains an invalid tuple");
                return State;
            }
            Entries.Add({DynamicIndex, KinematicIndex, ReferenceLength});
        }
    }

    State.bValid = true;
    State.TetherCount = Entries.Num();
    State.TetherMd5 = HashCanonicalTethers(State.VertexCount, MoveTemp(Entries));
    return State;
}

FString HashCanonicalSelectionIndices(
    const int32 VertexCount,
    TArray<int32> Indices)
{
    Indices.Sort();
    FMD5 Md5;
    const int32 IndexCount = Indices.Num();
    Md5.Update(
        reinterpret_cast<const uint8*>(&VertexCount),
        sizeof(VertexCount));
    Md5.Update(
        reinterpret_cast<const uint8*>(&IndexCount),
        sizeof(IndexCount));
    if (!Indices.IsEmpty())
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(Indices.GetData()),
            static_cast<uint32>(Indices.Num() * sizeof(int32)));
    }
    uint8 Digest[16];
    Md5.Final(Digest);
    return BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

struct FClothSelectionState
{
    bool bPresent = false;
    bool bValid = false;
    int32 VertexCount = 0;
    int32 SelectionCount = 0;
    FString SelectionName;
    FString SelectionGroup;
    FString SelectionMd5;
    FString Error;
};

FClothSelectionState ReadCollectionSelectionState(
    const TSharedRef<const FManagedArrayCollection>& Collection,
    const int32 VertexCount,
    const FName SelectionName)
{
    FClothSelectionState State;
    State.VertexCount = VertexCount;
    State.SelectionName = SelectionName.ToString();
    const UE::Chaos::ClothAsset::FCollectionClothSelectionConstFacade Selections(
        Collection);
    if (!Selections.IsValid())
    {
        State.Error = TEXT("The cloth selection schema is invalid");
        return State;
    }
    State.bPresent = Selections.HasSelection(SelectionName);
    if (!State.bPresent)
    {
        State.Error = FString::Printf(
            TEXT("Collection selection '%s' is absent"),
            *State.SelectionName);
        return State;
    }
    const FName SelectionGroup = Selections.GetSelectionGroup(SelectionName);
    State.SelectionGroup = SelectionGroup.ToString();
    if (SelectionGroup != UE::Chaos::ClothAsset::ClothCollectionGroup::SimVertices3D)
    {
        State.Error = FString::Printf(
            TEXT("Collection selection '%s' targets '%s', not SimVertices3D"),
            *State.SelectionName,
            *State.SelectionGroup);
        return State;
    }

    TArray<int32> Indices = Selections.GetSelectionSet(SelectionName).Array();
    for (const int32 Index : Indices)
    {
        if (Index < 0 || Index >= VertexCount)
        {
            State.Error = FString::Printf(
                TEXT("Collection selection '%s' contains invalid vertex index %d"),
                *State.SelectionName,
                Index);
            return State;
        }
    }
    State.bValid = true;
    State.SelectionCount = Indices.Num();
    State.SelectionMd5 = HashCanonicalSelectionIndices(
        VertexCount,
        MoveTemp(Indices));
    return State;
}

FClothSelectionState ReadCompiledSelectionState(
    const FChaosClothSimulationLodModel& CompiledLod,
    const FName SelectionName)
{
    FClothSelectionState State;
    State.VertexCount = CompiledLod.Positions.Num();
    State.SelectionName = SelectionName.ToString();
    State.SelectionGroup =
        UE::Chaos::ClothAsset::ClothCollectionGroup::SimVertices3D.ToString();
    const TSet<int32>* const VertexSet = CompiledLod.VertexSets.Find(SelectionName);
    State.bPresent = VertexSet != nullptr;
    if (!VertexSet)
    {
        State.Error = FString::Printf(
            TEXT("Compiled solver VertexSets has no selection '%s'"),
            *State.SelectionName);
        return State;
    }

    TArray<int32> Indices = VertexSet->Array();
    for (const int32 Index : Indices)
    {
        if (Index < 0 || Index >= State.VertexCount)
        {
            State.Error = FString::Printf(
                TEXT("Compiled selection '%s' contains invalid vertex index %d"),
                *State.SelectionName,
                Index);
            return State;
        }
    }
    State.bValid = true;
    State.SelectionCount = Indices.Num();
    State.SelectionMd5 = HashCanonicalSelectionIndices(
        State.VertexCount,
        MoveTemp(Indices));
    return State;
}

struct FClothWeightMapAssetState
{
    int32 LodIndex = INDEX_NONE;
    int32 VertexCount = 0;
    int32 FaceCount = 0;
    FString TopologyMd5;
    FString WeightMapName;
    int32 WeightCount = 0;
    FString WeightMapMd5;
    bool bCollectionMapPresent = false;
    bool bCompiledMapPresent = false;
    int32 CompiledWeightCount = 0;
    FString CompiledWeightMapMd5;
    bool bRuntimeBound = false;
    TArray<FString> ConsumerProperties;
    FString MaxDistanceWeightMapName;
    FString FixedEndWeightMapName;
    FString KinematicSelectionName;
    FString FixedEndSetName;
    bool bRequiresMaxDistanceDerivedData = false;
    bool bRequiresFixedEndWeightMapDerivedData = false;
    bool bRequiresKinematicSelectionDerivedData = false;
    FClothTetherState CollectionTethers;
    FClothTetherState CompiledTethers;
    bool bCollectionTethersMatchCompiled = false;
    FClothSelectionState CollectionKinematicSelection;
    FClothSelectionState CompiledKinematicSelection;
    bool bCollectionSelectionMatchesCompiled = false;
};

bool ReadClothWeightMapAssetState(
    UChaosClothAsset* Asset,
    int32 LodIndex,
    const FName WeightMapName,
    FClothWeightMapAssetState& OutState,
    FString& OutError)
{
    if (!Asset)
    {
        OutError = TEXT("The selected asset is not a UChaosClothAsset");
        return false;
    }
    SDK(TEXT("read_cloth_collections_and_compiled_model"));
    const TArray<TSharedRef<const FManagedArrayCollection>>& Collections =
        static_cast<const UChaosClothAsset*>(Asset)->GetClothCollections();
    if(!Guard()) { OutError=Work?Work->R.Error:TEXT("Cloth target changed");return false; }
    if (!Collections.IsValidIndex(LodIndex))
    {
        OutError = FString::Printf(
            TEXT("lod_index %d is out of range for %d cloth collections"),
            LodIndex,
            Collections.Num());
        return false;
    }
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(
        Collections[LodIndex]);
    if (!Cloth.IsValid())
    {
        OutError = TEXT("The selected LOD has no valid cloth collection schema");
        return false;
    }
    const TSharedPtr<const FChaosClothSimulationModel> SimulationModel =
        Asset->GetClothSimulationModel();
    if(!Guard()) { OutError=Work?Work->R.Error:TEXT("Cloth target changed");return false; }
    if (!SimulationModel.IsValid() || !SimulationModel->IsValidLodIndex(LodIndex))
    {
        OutError = TEXT("The selected LOD has no valid compiled cloth simulation model");
        return false;
    }
    const TConstArrayView<FVector3f> Positions = SimulationModel->GetPositions(LodIndex);
    const TConstArrayView<uint32> Indices = SimulationModel->GetIndices(LodIndex);
    if (Indices.Num() % 3 != 0)
    {
        OutError = TEXT("The compiled cloth simulation index buffer is not triangle aligned");
        return false;
    }

    OutState = FClothWeightMapAssetState();
    OutState.LodIndex = LodIndex;
    OutState.VertexCount = Positions.Num();
    OutState.FaceCount = Indices.Num() / 3;
    if(Positions.Num()>MaxMeshElements||Indices.Num()>MaxMeshElements) { OutError=TEXT("Compiled mesh exceeds bounded writer observation");return false; }
    for(const auto& P:Positions) { if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(P.Z)) { OutError=TEXT("Compiled positions are nonfinite");return false; } }
    for(uint32 I:Indices) { if(I>=static_cast<uint32>(Positions.Num())) { OutError=TEXT("Compiled triangle index is out of range");return false; } }
    OutState.TopologyMd5 = HashSimulationTopology(Positions, Indices);
    OutState.WeightMapName = WeightMapName.ToString();
    OutState.bCollectionMapPresent = Cloth.HasWeightMap(WeightMapName);
    if (OutState.bCollectionMapPresent)
    {
        const TConstArrayView<float> Values = Cloth.GetWeightMap(WeightMapName);
        OutState.WeightCount = Values.Num();
        OutState.WeightMapMd5 = HashFloatValues(Values);
    }

    const FChaosClothSimulationLodModel& CompiledLod =
        SimulationModel->ClothSimulationLodModels[LodIndex];
    OutState.CollectionTethers = ReadCollectionTetherState(Cloth);
    OutState.CompiledTethers = ReadCompiledTetherState(CompiledLod);
    OutState.bCollectionTethersMatchCompiled =
        OutState.CollectionTethers.bValid &&
        OutState.CompiledTethers.bValid &&
        OutState.CollectionTethers.VertexCount ==
            OutState.CompiledTethers.VertexCount &&
        OutState.CollectionTethers.TetherCount ==
            OutState.CompiledTethers.TetherCount &&
        OutState.CollectionTethers.TetherMd5 ==
            OutState.CompiledTethers.TetherMd5;
    if (const TArray<float>* const CompiledValues =
            CompiledLod.WeightMaps.Find(WeightMapName))
    {
        OutState.bCompiledMapPresent = true;
        OutState.CompiledWeightCount = CompiledValues->Num();
        OutState.CompiledWeightMapMd5 = HashFloatValues(*CompiledValues);
    }
    const Chaos::Softs::FCollectionPropertyConstFacade Properties(
        Collections[LodIndex]);
    if (Properties.IsValid())
    {
        OutState.MaxDistanceWeightMapName = Properties.GetStringValue(
            TEXT("MaxDistance"));
        OutState.FixedEndWeightMapName = Properties.GetStringValue(
            TEXT("FixedEndWeightMap"));
        OutState.KinematicSelectionName = Properties.GetStringValue(
            TEXT("KinematicVertices3D"));
        OutState.FixedEndSetName = Properties.GetStringValue(
            TEXT("FixedEndSet"));
        for (int32 PropertyIndex = 0; PropertyIndex < Properties.Num(); ++PropertyIndex)
        {
            if (Properties.GetStringValue(PropertyIndex) == WeightMapName.ToString())
            {
                OutState.ConsumerProperties.Add(Properties.GetKeyName(PropertyIndex).ToString());
            }
        }
    }
    OutState.bRequiresMaxDistanceDerivedData =
        WeightMapName == FName(TEXT("MaxDistance")) ||
        OutState.MaxDistanceWeightMapName == WeightMapName.ToString();
    OutState.bRequiresFixedEndWeightMapDerivedData =
        OutState.FixedEndWeightMapName == WeightMapName.ToString();
    OutState.bRequiresKinematicSelectionDerivedData =
        OutState.bRequiresMaxDistanceDerivedData &&
        !OutState.KinematicSelectionName.IsEmpty() &&
        OutState.FixedEndSetName == OutState.KinematicSelectionName;
    if (OutState.bRequiresKinematicSelectionDerivedData)
    {
        const FName SelectionName(*OutState.KinematicSelectionName);
        OutState.CollectionKinematicSelection = ReadCollectionSelectionState(
            Collections[LodIndex],
            OutState.VertexCount,
            SelectionName);
        OutState.CompiledKinematicSelection = ReadCompiledSelectionState(
            CompiledLod,
            SelectionName);
        OutState.bCollectionSelectionMatchesCompiled =
            OutState.CollectionKinematicSelection.bValid &&
            OutState.CompiledKinematicSelection.bValid &&
            OutState.CollectionKinematicSelection.VertexCount ==
                OutState.CompiledKinematicSelection.VertexCount &&
            OutState.CollectionKinematicSelection.SelectionCount ==
                OutState.CompiledKinematicSelection.SelectionCount &&
            OutState.CollectionKinematicSelection.SelectionMd5 ==
                OutState.CompiledKinematicSelection.SelectionMd5;
    }
    OutState.bRuntimeBound =
        OutState.bCompiledMapPresent &&
        OutState.CompiledWeightCount == OutState.WeightCount &&
        OutState.CompiledWeightMapMd5 == OutState.WeightMapMd5 &&
        !OutState.ConsumerProperties.IsEmpty();
    return true;
}

TSharedPtr<FJsonObject> MakeClothTetherStateSnapshot(
    const FClothTetherState& State)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("valid"), State.bValid);
    Result->SetStringField(TEXT("error"), State.Error);
    Result->SetNumberField(TEXT("vertex_count"), State.VertexCount);
    Result->SetNumberField(TEXT("batch_count"), State.BatchCount);
    Result->SetNumberField(TEXT("tether_count"), State.TetherCount);
    Result->SetStringField(TEXT("tether_md5"), State.TetherMd5);
    return Result;
}

TSharedPtr<FJsonObject> MakeClothSelectionStateSnapshot(
    const FClothSelectionState& State)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("present"), State.bPresent);
    Result->SetBoolField(TEXT("valid"), State.bValid);
    Result->SetStringField(TEXT("error"), State.Error);
    Result->SetNumberField(TEXT("vertex_count"), State.VertexCount);
    Result->SetStringField(TEXT("selection_name"), State.SelectionName);
    Result->SetStringField(TEXT("selection_group"), State.SelectionGroup);
    Result->SetNumberField(TEXT("selection_count"), State.SelectionCount);
    Result->SetStringField(TEXT("selection_md5"), State.SelectionMd5);
    return Result;
}

TSharedPtr<FJsonObject> MakeClothWeightMapAssetStateSnapshot(
    const FClothWeightMapAssetState& State)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetNumberField(TEXT("lod_index"), State.LodIndex);
    Result->SetNumberField(TEXT("sim_vertex_count"), State.VertexCount);
    Result->SetNumberField(TEXT("sim_face_count"), State.FaceCount);
    Result->SetStringField(TEXT("topology_md5"), State.TopologyMd5);
    Result->SetStringField(TEXT("weight_map_name"), State.WeightMapName);
    Result->SetBoolField(TEXT("collection_map_present"), State.bCollectionMapPresent);
    Result->SetNumberField(TEXT("weight_count"), State.WeightCount);
    Result->SetStringField(TEXT("weight_map_md5"), State.WeightMapMd5);
    Result->SetBoolField(TEXT("compiled_map_present"), State.bCompiledMapPresent);
    Result->SetNumberField(TEXT("compiled_weight_count"), State.CompiledWeightCount);
    Result->SetStringField(
        TEXT("compiled_weight_map_md5"),
        State.CompiledWeightMapMd5);
    Result->SetBoolField(
        TEXT("compiled_map_matches_collection"),
        State.bCollectionMapPresent && State.bCompiledMapPresent &&
            State.WeightCount == State.CompiledWeightCount &&
            State.WeightMapMd5 == State.CompiledWeightMapMd5);
    Result->SetBoolField(TEXT("runtime_bound"), State.bRuntimeBound);
    Result->SetStringField(
        TEXT("max_distance_weight_map_name"),
        State.MaxDistanceWeightMapName);
    Result->SetStringField(
        TEXT("fixed_end_weight_map_name"),
        State.FixedEndWeightMapName);
    Result->SetStringField(
        TEXT("kinematic_selection_name"),
        State.KinematicSelectionName);
    Result->SetStringField(
        TEXT("fixed_end_set_name"),
        State.FixedEndSetName);
    Result->SetBoolField(
        TEXT("requires_max_distance_derived_data"),
        State.bRequiresMaxDistanceDerivedData);
    Result->SetBoolField(
        TEXT("requires_fixed_end_weight_map_derived_data"),
        State.bRequiresFixedEndWeightMapDerivedData);
    Result->SetBoolField(
        TEXT("requires_kinematic_selection_derived_data"),
        State.bRequiresKinematicSelectionDerivedData);
    Result->SetObjectField(
        TEXT("collection_tethers"),
        MakeClothTetherStateSnapshot(State.CollectionTethers));
    Result->SetObjectField(
        TEXT("compiled_tethers"),
        MakeClothTetherStateSnapshot(State.CompiledTethers));
    Result->SetBoolField(
        TEXT("collection_tethers_match_compiled"),
        State.bCollectionTethersMatchCompiled);
    Result->SetObjectField(
        TEXT("collection_kinematic_selection"),
        MakeClothSelectionStateSnapshot(State.CollectionKinematicSelection));
    Result->SetObjectField(
        TEXT("compiled_kinematic_selection"),
        MakeClothSelectionStateSnapshot(State.CompiledKinematicSelection));
    Result->SetBoolField(
        TEXT("collection_kinematic_selection_matches_compiled"),
        State.bCollectionSelectionMatchesCompiled);
    Result->SetArrayField(
        TEXT("consumer_properties"),
        StringsToJson(State.ConsumerProperties));
    return Result;
}
bool EvaluateWeightMapNodeInputTopology(
    const TSharedPtr<FDataflowNode>& Node,
    UObject* EvaluationAsset,
    int32& OutVertexCount,
    FString& OutTopologyMd5)
{
    OutVertexCount = 0;
    OutTopologyMd5.Reset();
    if (!Node.IsValid() || !EvaluationAsset)
    {
        return false;
    }
    const FDataflowInput* const CollectionInput = Node->FindInput(FName(TEXT("Collection")));
    if (!CollectionInput)
    {
        return false;
    }

    SDK(TEXT("evaluate_cloth_collection_input"));Count(TEXT("evaluation_calls"));
    UE::Dataflow::FEngineContext Context(EvaluationAsset);
    const FManagedArrayCollection EmptyCollection;
    const FManagedArrayCollection InputCollection =
        CollectionInput->GetValue<FManagedArrayCollection>(Context, EmptyCollection);
    Count(TEXT("evaluation_returns"));if(!Guard()) { return false; }
    const TSharedRef<FManagedArrayCollection> SharedCollection =
        MakeShared<FManagedArrayCollection>(InputCollection);
    const UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(SharedCollection);
    if (!Cloth.IsValid())
    {
        return false;
    }

    TArray<FVector3f> Positions;
    TArray<FVector3f> Normals;
    TArray<uint32> Indices;
    TArray<FVector2f> PatternPositions;
    TArray<uint32> PatternIndices;
    TArray<uint32> PatternToWeldedIndices;
    Cloth.BuildSimulationMesh(
        Positions,
        Normals,
        Indices,
        PatternPositions,
        PatternIndices,
        PatternToWeldedIndices);
    if(Positions.Num()>MaxMeshElements||Indices.Num()>MaxMeshElements) { return false; }
    for(const auto& P:Positions) { if(!FMath::IsFinite(P.X)||!FMath::IsFinite(P.Y)||!FMath::IsFinite(P.Z)) { return false; } }
    for(uint32 I:Indices) { if(I>=static_cast<uint32>(Positions.Num())) { return false; } }
    OutVertexCount = Positions.Num();
    OutTopologyMd5 = HashSimulationTopology(Positions, Indices);
    return true;
}

struct FWeightMapDownstreamHop
{
    int32 HopIndex = INDEX_NONE;
    FString NodeGuid;
    FString NodeName;
    FString NodeType;
    FString CollectionInputName;
    FString CollectionInputType;
    FString CollectionOutputName;
    FString CollectionOutputType;
    bool bNodeActive = false;
    bool bCollectionOutputFrozen = false;
    FString ResolvedMaxDistanceWeightMap;
    FString MaxDistanceWeightMapSourceNodeGuid;
    FString MaxDistanceWeightMapSourceOutputName;
    FString ResolvedInKinematic;
    FString InKinematicSourceNodeGuid;
    FString InKinematicSourceOutputName;
    FString ResolvedFixedEndSet;
    FString FixedEndSetSourceNodeGuid;
    FString FixedEndSetSourceOutputName;
    FString ResolvedFixedEndWeightMap;
    FString FixedEndWeightMapSourceNodeGuid;
    FString FixedEndWeightMapSourceOutputName;
    bool bCustomTetherGenerationReadable = false;
    bool bCustomTetherGeneration = false;
    bool bDefaultInputPropertiesEvaluated = false;
    bool bDefaultInputPropertiesValid = false;
};

struct FWeightMapDownstreamPath
{
    bool bSourceNodeActive = false;
    bool bSourceOutputFrozen = false;
    FString SourceNodeGuid;
    FString SourceNodeName;
    FString SourceNodeType;
    FString SourceOutputName;
    FString SourceOutputType;
    int32 DownstreamNodeCount = 0;
    TArray<FString> DownstreamNodeTypes;
    TArray<FWeightMapDownstreamHop> Hops;
    FString TerminalNodeName;
    FString TerminalInputName;
    int32 TerminalLodIndex = INDEX_NONE;
    FString PathMd5;
};

bool ReadDataflowStringInputValue(
    const FDataflowNode& Node,
    const FName InputName,
    UObject* EvaluationAsset,
    FString& OutValue,
    FString& OutSourceNodeGuid,
    FString& OutSourceOutputName)
{
    OutValue.Reset();
    OutSourceNodeGuid.Reset();
    OutSourceOutputName.Reset();
    const FDataflowInput* const Input = Node.FindInput(InputName);
    const FStrProperty* const StringProperty = Input
        ? CastField<FStrProperty>(Input->GetProperty())
        : nullptr;
    if (!Input || !StringProperty || !EvaluationAsset || !Input->RealAddress() ||
        Input->GetType() != FName(TEXT("FString")))
    {
        return false;
    }

    const FString DefaultValue = StringProperty->GetPropertyValue(
        Input->RealAddress());
    UE::Dataflow::FEngineContext Context(EvaluationAsset);
    SDK(TEXT("evaluate_downstream_string"));Count(TEXT("evaluation_calls"));
    OutValue = Input->GetValue<FString>(Context, DefaultValue);Count(TEXT("evaluation_returns"));
    if(!Guard()) { return false; }
    const FDataflowInput* FreshInput=Node.FindInput(InputName);if(!FreshInput) { return false; }
    if (const FDataflowOutput* const SourceOutput = FreshInput->GetConnection())
    {
        if (SourceOutput->GetType() != FName(TEXT("FString")))
        {
            return false;
        }
        const FDataflowNode* const SourceNode = SourceOutput->GetOwningNode();
        if (!SourceNode)
        {
            return false;
        }
        OutSourceNodeGuid = SourceNode->GetGuid().ToString(
            EGuidFormats::DigitsWithHyphens);
        OutSourceOutputName = SourceOutput->GetName().ToString();
    }
    return true;
}

bool ReadDataflowBoolProperty(
    const FDataflowNode& Node,
    const FName PropertyName,
    bool& OutValue)
{
    const UScriptStruct* const Struct = Node.TypedScriptStruct();
    const FBoolProperty* const Property = Struct
        ? FindFProperty<FBoolProperty>(Struct, PropertyName)
        : nullptr;
    if (!Property)
    {
        return false;
    }
    OutValue = Property->GetPropertyValue_InContainer(&Node);
    return true;
}

bool EvaluateCollectionInputPropertySchema(
    const FDataflowNode& Node,
    UObject* EvaluationAsset,
    bool& bOutPropertiesValid)
{
    bOutPropertiesValid = false;
    const FDataflowInput* const CollectionInput =
        Node.FindInput(FName(TEXT("Collection")));
    if (!CollectionInput || !EvaluationAsset)
    {
        return false;
    }
    SDK(TEXT("evaluate_cloth_collection_input"));Count(TEXT("evaluation_calls"));
    UE::Dataflow::FEngineContext Context(EvaluationAsset);
    const FManagedArrayCollection EmptyCollection;
    const FManagedArrayCollection InputCollection =
        CollectionInput->GetValue<FManagedArrayCollection>(
            Context,
            EmptyCollection);
    Count(TEXT("evaluation_returns"));if(!Guard()) { return false; }
    const TSharedRef<FManagedArrayCollection> SharedCollection =
        MakeShared<FManagedArrayCollection>(InputCollection);
    if (!UE::Chaos::ClothAsset::FCollectionClothConstFacade(
            SharedCollection).IsValid())
    {
        return false;
    }
    bOutPropertiesValid =
        Chaos::Softs::FCollectionPropertyConstFacade(
            SharedCollection).IsValid();
    return true;
}

void UpdateMd5WithBool(FMD5& Md5, const bool bValue)
{
    const uint8 Value = bValue ? 1 : 0;
    Md5.Update(&Value, sizeof(Value));
}

void FinalizeWeightMapDownstreamPathHash(FWeightMapDownstreamPath& Path)
{
    FMD5 Md5;
    UpdateMd5WithBool(Md5, Path.bSourceNodeActive);
    UpdateMd5WithBool(Md5, Path.bSourceOutputFrozen);
    UpdateMd5WithString(Md5, Path.SourceNodeGuid);
    UpdateMd5WithString(Md5, Path.SourceNodeName);
    UpdateMd5WithString(Md5, Path.SourceNodeType);
    UpdateMd5WithString(Md5, Path.SourceOutputName);
    UpdateMd5WithString(Md5, Path.SourceOutputType);
    for (const FWeightMapDownstreamHop& Hop : Path.Hops)
    {
        Md5.Update(
            reinterpret_cast<const uint8*>(&Hop.HopIndex),
            sizeof(Hop.HopIndex));
        UpdateMd5WithString(Md5, Hop.NodeGuid);
        UpdateMd5WithString(Md5, Hop.NodeName);
        UpdateMd5WithString(Md5, Hop.NodeType);
        UpdateMd5WithString(Md5, Hop.CollectionInputName);
        UpdateMd5WithString(Md5, Hop.CollectionInputType);
        UpdateMd5WithString(Md5, Hop.CollectionOutputName);
        UpdateMd5WithString(Md5, Hop.CollectionOutputType);
        UpdateMd5WithBool(Md5, Hop.bNodeActive);
        UpdateMd5WithBool(Md5, Hop.bCollectionOutputFrozen);
        UpdateMd5WithString(Md5, Hop.ResolvedMaxDistanceWeightMap);
        UpdateMd5WithString(Md5, Hop.MaxDistanceWeightMapSourceNodeGuid);
        UpdateMd5WithString(Md5, Hop.MaxDistanceWeightMapSourceOutputName);
        UpdateMd5WithString(Md5, Hop.ResolvedInKinematic);
        UpdateMd5WithString(Md5, Hop.InKinematicSourceNodeGuid);
        UpdateMd5WithString(Md5, Hop.InKinematicSourceOutputName);
        UpdateMd5WithString(Md5, Hop.ResolvedFixedEndSet);
        UpdateMd5WithString(Md5, Hop.FixedEndSetSourceNodeGuid);
        UpdateMd5WithString(Md5, Hop.FixedEndSetSourceOutputName);
        UpdateMd5WithString(Md5, Hop.ResolvedFixedEndWeightMap);
        UpdateMd5WithString(Md5, Hop.FixedEndWeightMapSourceNodeGuid);
        UpdateMd5WithString(Md5, Hop.FixedEndWeightMapSourceOutputName);
        UpdateMd5WithBool(Md5, Hop.bCustomTetherGenerationReadable);
        UpdateMd5WithBool(Md5, Hop.bCustomTetherGeneration);
        UpdateMd5WithBool(Md5, Hop.bDefaultInputPropertiesEvaluated);
        UpdateMd5WithBool(Md5, Hop.bDefaultInputPropertiesValid);
    }
    UpdateMd5WithString(Md5, Path.TerminalNodeName);
    UpdateMd5WithString(Md5, Path.TerminalInputName);
    Md5.Update(
        reinterpret_cast<const uint8*>(&Path.TerminalLodIndex),
        sizeof(Path.TerminalLodIndex));
    uint8 Digest[16];
    Md5.Final(Digest);
    Path.PathMd5 = BytesToHex(Digest, UE_ARRAY_COUNT(Digest)).ToLower();
}

int32 ResolveTerminalInputLodIndex(const FDataflowInput& Input)
{
    if (Input.GetContainerIndex() != INDEX_NONE)
    {
        return Input.GetContainerIndex();
    }

    const FString InputName = Input.GetName().ToString();
    int32 OpenBracketIndex = INDEX_NONE;
    if (InputName.FindLastChar(TEXT('['), OpenBracketIndex) &&
        InputName.EndsWith(TEXT("]")))
    {
        int32 ParsedIndex = INDEX_NONE;
        const FString IndexText = InputName.Mid(
            OpenBracketIndex + 1,
            InputName.Len() - OpenBracketIndex - 2);
        if (LexTryParseString(ParsedIndex, *IndexText))
        {
            return ParsedIndex;
        }
    }

    int32 DigitStart = InputName.Len();
    while (DigitStart > 0 && FChar::IsDigit(InputName[DigitStart - 1]))
    {
        --DigitStart;
    }
    if (DigitStart < InputName.Len())
    {
        int32 ParsedIndex = INDEX_NONE;
        if (LexTryParseString(ParsedIndex, *InputName.Mid(DigitStart)))
        {
            return ParsedIndex;
        }
    }
    return INDEX_NONE;
}

bool ResolveSingleWeightMapDownstreamPath(
    const TSharedPtr<FDataflowNode>& WeightMapNode,
    UObject* EvaluationAsset,
    const FName ExpectedTerminalNodeName,
    const int32 ExpectedLodIndex,
    FWeightMapDownstreamPath& OutPath,
    FString& OutError)
{
    OutPath = FWeightMapDownstreamPath();
    OutError.Reset();
    if (!WeightMapNode.IsValid() || !EvaluationAsset)
    {
        OutError = TEXT("The WeightMap node is unavailable for downstream path validation");
        return false;
    }
    if (ExpectedTerminalNodeName.IsNone())
    {
        OutError = TEXT("The cloth asset has no selected Dataflow terminal node");
        return false;
    }

    const FDataflowOutput* CurrentOutput =
        WeightMapNode->FindOutput(FName(TEXT("Collection")));
    if (!CurrentOutput)
    {
        OutError = TEXT("The WeightMap node has no Collection output");
        return false;
    }
    OutPath.bSourceNodeActive = WeightMapNode->IsActive();
    OutPath.bSourceOutputFrozen = CurrentOutput->HasFrozenValue();
    OutPath.SourceNodeGuid = WeightMapNode->GetGuid().ToString(
        EGuidFormats::DigitsWithHyphens);
    OutPath.SourceNodeName = WeightMapNode->GetName().ToString();
    OutPath.SourceNodeType = WeightMapNode->GetType().ToString();
    OutPath.SourceOutputName = CurrentOutput->GetName().ToString();
    OutPath.SourceOutputType = CurrentOutput->GetType().ToString();
    if (CurrentOutput->GetType() != FManagedArrayCollection::StaticType())
    {
        OutError = TEXT("The WeightMap Collection output is not exactly FManagedArrayCollection");
        return false;
    }
    if (!OutPath.bSourceNodeActive)
    {
        OutError = TEXT("The WeightMap node is inactive, so its authored values cannot reach the terminal");
        return false;
    }
    if (OutPath.bSourceOutputFrozen)
    {
        OutError = TEXT("The WeightMap Collection output is frozen and would preserve stale derived data");
        return false;
    }
    const FName CollectionType = CurrentOutput->GetType();
    TSet<FGuid> VisitedNodeGuids;
    VisitedNodeGuids.Add(WeightMapNode->GetGuid());

    for (int32 HopIndex = 0; HopIndex < 4096; ++HopIndex)
    {
        const TArray<FDataflowInput*>& Connections = CurrentOutput->GetConnections();
        if (Connections.Num() != 1 || !Connections[0])
        {
            OutError = FString::Printf(
                TEXT("The WeightMap Collection path must have exactly one downstream connection at every hop (found %d)"),
                Connections.Num());
            return false;
        }

        const FDataflowInput* const ConnectedInput = Connections[0];
        const FDataflowNode* const DownstreamNode = ConnectedInput->GetOwningNode();
        if (!DownstreamNode)
        {
            OutError = TEXT("A downstream Collection input has no owning Dataflow node");
            return false;
        }
        if (VisitedNodeGuids.Contains(DownstreamNode->GetGuid()))
        {
            OutError = TEXT("The downstream Collection path contains a cycle or converging revisit");
            return false;
        }
        VisitedNodeGuids.Add(DownstreamNode->GetGuid());
        ++OutPath.DownstreamNodeCount;
        const FString DownstreamNodeType = DownstreamNode->GetType().ToString();
        OutPath.DownstreamNodeTypes.Add(DownstreamNodeType);
        FWeightMapDownstreamHop Hop;
        Hop.HopIndex = HopIndex;
        Hop.NodeGuid = DownstreamNode->GetGuid().ToString(
            EGuidFormats::DigitsWithHyphens);
        Hop.NodeName = DownstreamNode->GetName().ToString();
        Hop.NodeType = DownstreamNodeType;
        Hop.CollectionInputName = ConnectedInput->GetName().ToString();
        Hop.CollectionInputType = ConnectedInput->GetType().ToString();
        Hop.bNodeActive = DownstreamNode->IsActive();
        if (ConnectedInput->GetType() != FManagedArrayCollection::StaticType())
        {
            OutError = FString::Printf(
                TEXT("Downstream Collection input '%s.%s' is not exactly FManagedArrayCollection"),
                *Hop.NodeName,
                *Hop.CollectionInputName);
            return false;
        }

        if (DownstreamNodeType ==
            TEXT("FChaosClothAssetSimulationMaxDistanceConfigNode"))
        {
            if (!ReadDataflowStringInputValue(
                    *DownstreamNode,
                    FName(TEXT("MaxDistance.WeightMap")),
                    EvaluationAsset,
                    Hop.ResolvedMaxDistanceWeightMap,
                    Hop.MaxDistanceWeightMapSourceNodeGuid,
                    Hop.MaxDistanceWeightMapSourceOutputName) ||
                !ReadDataflowStringInputValue(
                    *DownstreamNode,
                    FName(TEXT("InKinematic.StringValue")),
                    EvaluationAsset,
                    Hop.ResolvedInKinematic,
                    Hop.InKinematicSourceNodeGuid,
                    Hop.InKinematicSourceOutputName))
            {
                OutError = FString::Printf(
                    TEXT("Unable to resolve MaxDistance.WeightMap or InKinematic.StringValue on downstream node '%s'"),
                    *Hop.NodeName);
                return false;
            }
        }
        else if (DownstreamNodeType ==
                 TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode_v2"))
        {
            if (!ReadDataflowStringInputValue(
                    *DownstreamNode,
                    FName(TEXT("FixedEndSet.StringValue")),
                    EvaluationAsset,
                    Hop.ResolvedFixedEndSet,
                    Hop.FixedEndSetSourceNodeGuid,
                    Hop.FixedEndSetSourceOutputName) ||
                !ReadDataflowBoolProperty(
                    *DownstreamNode,
                    FName(TEXT("bEnableCustomTetherGeneration")),
                    Hop.bCustomTetherGeneration))
            {
                OutError = FString::Printf(
                    TEXT("Unable to resolve FixedEndSet or custom tether mode on downstream node '%s'"),
                    *Hop.NodeName);
                return false;
            }
            Hop.bCustomTetherGenerationReadable = true;
        }
        else if (DownstreamNodeType ==
                 TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode"))
        {
            if (!ReadDataflowStringInputValue(
                    *DownstreamNode,
                    FName(TEXT("FixedEndWeightMap")),
                    EvaluationAsset,
                    Hop.ResolvedFixedEndWeightMap,
                    Hop.FixedEndWeightMapSourceNodeGuid,
                    Hop.FixedEndWeightMapSourceOutputName))
            {
                OutError = FString::Printf(
                    TEXT("Unable to resolve FixedEndWeightMap on downstream node '%s'"),
                    *Hop.NodeName);
                return false;
            }
        }
        else if (DownstreamNodeType ==
                 TEXT("FChaosClothAssetSimulationDefaultConfigNode"))
        {
            Hop.bDefaultInputPropertiesEvaluated =
                EvaluateCollectionInputPropertySchema(
                    *DownstreamNode,
                    EvaluationAsset,
                    Hop.bDefaultInputPropertiesValid);
            if (!Hop.bDefaultInputPropertiesEvaluated)
            {
                OutError = FString::Printf(
                    TEXT("Unable to evaluate the input property schema for downstream DefaultConfig node '%s'"),
                    *Hop.NodeName);
                return false;
            }
        }

        if (DownstreamNode->IsA(FName(TEXT("FDataflowTerminalNode"))))
        {
            if (DownstreamNodeType != TEXT("FChaosClothAssetTerminalNode_v2"))
            {
                OutError = FString::Printf(
                    TEXT("The selected terminal must be exactly FChaosClothAssetTerminalNode_v2, not '%s'"),
                    *DownstreamNodeType);
                return false;
            }
            if (!Hop.bNodeActive)
            {
                OutError = TEXT("The selected Chaos Cloth terminal node is inactive");
                return false;
            }
            OutPath.TerminalNodeName = DownstreamNode->GetName().ToString();
            OutPath.TerminalInputName = ConnectedInput->GetName().ToString();
            OutPath.TerminalLodIndex = ResolveTerminalInputLodIndex(*ConnectedInput);
            OutPath.Hops.Add(MoveTemp(Hop));
            if (DownstreamNode->GetName() != ExpectedTerminalNodeName)
            {
                OutError = FString::Printf(
                    TEXT("The WeightMap path reaches terminal '%s', but the cloth asset selects '%s'"),
                    *OutPath.TerminalNodeName,
                    *ExpectedTerminalNodeName.ToString());
                return false;
            }
            if (OutPath.TerminalLodIndex != ExpectedLodIndex)
            {
                OutError = FString::Printf(
                    TEXT("The WeightMap path reaches terminal input '%s' at LOD %d, not requested LOD %d"),
                    *OutPath.TerminalInputName,
                    OutPath.TerminalLodIndex,
                    ExpectedLodIndex);
                return false;
            }
            FinalizeWeightMapDownstreamPathHash(OutPath);
            return true;
        }

        TArray<FDataflowOutput*> ConnectedCollectionOutputs;
        for (FDataflowOutput* const Output : DownstreamNode->GetOutputs())
        {
            if (Output && Output->HasAnyConnections() &&
                Output->GetType() == CollectionType)
            {
                ConnectedCollectionOutputs.Add(Output);
            }
        }
        if (ConnectedCollectionOutputs.Num() != 1 ||
            ConnectedCollectionOutputs[0]->GetPassthroughInput() != ConnectedInput)
        {
            OutError = FString::Printf(
                TEXT("The downstream node '%s' must have exactly one connected Collection output passthrough from the traversed input (found %d connected Collection outputs)"),
                *DownstreamNode->GetName().ToString(),
                ConnectedCollectionOutputs.Num());
            return false;
        }
        CurrentOutput = ConnectedCollectionOutputs[0];
        Hop.CollectionOutputName = CurrentOutput->GetName().ToString();
        Hop.CollectionOutputType = CurrentOutput->GetType().ToString();
        Hop.bCollectionOutputFrozen = CurrentOutput->HasFrozenValue();
        if (Hop.bCollectionOutputFrozen)
        {
            OutError = FString::Printf(
                TEXT("Downstream Collection output '%s.%s' is frozen and would preserve stale derived data"),
                *Hop.NodeName,
                *Hop.CollectionOutputName);
            return false;
        }
        OutPath.Hops.Add(MoveTemp(Hop));
    }

    OutError = TEXT("The downstream Collection path exceeded the 4096-node safety bound");
    return false;
}

TSharedPtr<FJsonObject> MakeWeightMapDownstreamPathSnapshot(
    const FWeightMapDownstreamPath& Path,
    const bool bValid,
    const FString& Error)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("valid"), bValid);
    Result->SetStringField(TEXT("error"), Error);
    Result->SetBoolField(TEXT("source_node_active"), Path.bSourceNodeActive);
    Result->SetBoolField(TEXT("source_output_frozen"), Path.bSourceOutputFrozen);
    Result->SetStringField(TEXT("source_node_guid"), Path.SourceNodeGuid);
    Result->SetStringField(TEXT("source_node_name"), Path.SourceNodeName);
    Result->SetStringField(TEXT("source_node_type"), Path.SourceNodeType);
    Result->SetStringField(TEXT("source_output_name"), Path.SourceOutputName);
    Result->SetStringField(TEXT("source_output_type"), Path.SourceOutputType);
    Result->SetNumberField(TEXT("downstream_node_count"), Path.DownstreamNodeCount);
    Result->SetArrayField(
        TEXT("downstream_node_types"),
        StringsToJson(Path.DownstreamNodeTypes));
    Result->SetStringField(TEXT("terminal_node_name"), Path.TerminalNodeName);
    Result->SetStringField(TEXT("terminal_input_name"), Path.TerminalInputName);
    Result->SetNumberField(TEXT("terminal_lod_index"), Path.TerminalLodIndex);
    Result->SetStringField(TEXT("path_md5"), Path.PathMd5);
    TArray<TSharedPtr<FJsonValue>> Hops;
    Hops.Reserve(Path.Hops.Num());
    for (const FWeightMapDownstreamHop& Hop : Path.Hops)
    {
        TSharedPtr<FJsonObject> HopJson = MakeShared<FJsonObject>();
        HopJson->SetNumberField(TEXT("hop_index"), Hop.HopIndex);
        HopJson->SetStringField(TEXT("node_guid"), Hop.NodeGuid);
        HopJson->SetStringField(TEXT("node_name"), Hop.NodeName);
        HopJson->SetStringField(TEXT("node_type"), Hop.NodeType);
        HopJson->SetStringField(TEXT("collection_input_name"), Hop.CollectionInputName);
        HopJson->SetStringField(TEXT("collection_input_type"), Hop.CollectionInputType);
        HopJson->SetStringField(TEXT("collection_output_name"), Hop.CollectionOutputName);
        HopJson->SetStringField(TEXT("collection_output_type"), Hop.CollectionOutputType);
        HopJson->SetBoolField(TEXT("node_active"), Hop.bNodeActive);
        HopJson->SetBoolField(TEXT("collection_output_frozen"), Hop.bCollectionOutputFrozen);
        HopJson->SetStringField(TEXT("resolved_max_distance_weight_map"), Hop.ResolvedMaxDistanceWeightMap);
        HopJson->SetStringField(TEXT("max_distance_weight_map_source_node_guid"), Hop.MaxDistanceWeightMapSourceNodeGuid);
        HopJson->SetStringField(TEXT("max_distance_weight_map_source_output_name"), Hop.MaxDistanceWeightMapSourceOutputName);
        HopJson->SetStringField(TEXT("resolved_in_kinematic"), Hop.ResolvedInKinematic);
        HopJson->SetStringField(TEXT("in_kinematic_source_node_guid"), Hop.InKinematicSourceNodeGuid);
        HopJson->SetStringField(TEXT("in_kinematic_source_output_name"), Hop.InKinematicSourceOutputName);
        HopJson->SetStringField(TEXT("resolved_fixed_end_set"), Hop.ResolvedFixedEndSet);
        HopJson->SetStringField(TEXT("fixed_end_set_source_node_guid"), Hop.FixedEndSetSourceNodeGuid);
        HopJson->SetStringField(TEXT("fixed_end_set_source_output_name"), Hop.FixedEndSetSourceOutputName);
        HopJson->SetStringField(TEXT("resolved_fixed_end_weight_map"), Hop.ResolvedFixedEndWeightMap);
        HopJson->SetStringField(TEXT("fixed_end_weight_map_source_node_guid"), Hop.FixedEndWeightMapSourceNodeGuid);
        HopJson->SetStringField(TEXT("fixed_end_weight_map_source_output_name"), Hop.FixedEndWeightMapSourceOutputName);
        HopJson->SetBoolField(TEXT("custom_tether_generation_readable"), Hop.bCustomTetherGenerationReadable);
        HopJson->SetBoolField(TEXT("custom_tether_generation"), Hop.bCustomTetherGeneration);
        HopJson->SetBoolField(TEXT("default_input_properties_evaluated"), Hop.bDefaultInputPropertiesEvaluated);
        HopJson->SetBoolField(TEXT("default_input_properties_valid"), Hop.bDefaultInputPropertiesValid);
        Hops.Add(MakeShared<FJsonValueObject>(HopJson));
    }
    Result->SetArrayField(TEXT("hops"), Hops);
    return Result;
}

int32 FindLastDownstreamNodeType(
    const FWeightMapDownstreamPath& Path,
    const FString& NodeType)
{
    for (int32 Index = Path.Hops.Num() - 1; Index >= 0; --Index)
    {
        if (Path.Hops[Index].NodeType == NodeType)
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

bool IsTetherGeneratorNodeType(const FString& NodeType)
{
    return NodeType == TEXT("FChaosClothAssetSimulationDefaultConfigNode") ||
        NodeType == TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode_v2") ||
        NodeType == TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode") ||
        NodeType == TEXT("FChaosClothAssetRemeshNode_v2") ||
        NodeType == TEXT("FChaosClothAssetRemeshNode");
}

int32 FindLastTetherGeneratorIndex(const FWeightMapDownstreamPath& Path)
{
    for (int32 Index = Path.Hops.Num() - 1; Index >= 0; --Index)
    {
        if (IsTetherGeneratorNodeType(Path.Hops[Index].NodeType))
        {
            return Index;
        }
    }
    return INDEX_NONE;
}

bool ValidateWeightMapDerivedDataPath(
    const FClothWeightMapAssetState& AssetState,
    const FWeightMapDownstreamPath& Path,
    FString& OutMode,
    FString& OutError)
{
    OutMode = TEXT("not_required");
    OutError.Reset();
    if (!AssetState.bRequiresMaxDistanceDerivedData &&
        !AssetState.bRequiresFixedEndWeightMapDerivedData)
    {
        return true;
    }

    const int32 RemeshV2Index = FindLastDownstreamNodeType(
        Path, TEXT("FChaosClothAssetRemeshNode_v2"));
    const int32 RemeshIndex = FindLastDownstreamNodeType(
        Path, TEXT("FChaosClothAssetRemeshNode"));
    if (RemeshV2Index != INDEX_NONE || RemeshIndex != INDEX_NONE)
    {
        OutError = TEXT("Remesh appears on a tether-affecting WeightMap path; intrinsic map, selection, and topology remapping are intentionally unsupported by this guarded route");
        return false;
    }

    const int32 LastMaxDistanceConfigIndex = FindLastDownstreamNodeType(
        Path,
        TEXT("FChaosClothAssetSimulationMaxDistanceConfigNode"));
    if (AssetState.bRequiresKinematicSelectionDerivedData)
    {
        if (!Path.Hops.IsValidIndex(LastMaxDistanceConfigIndex))
        {
            OutError = TEXT("MaxDistance requires a downstream SimulationMaxDistanceConfig node to regenerate KinematicVertices3D");
            return false;
        }
        const FWeightMapDownstreamHop& MaxDistanceHop =
            Path.Hops[LastMaxDistanceConfigIndex];
        if (!MaxDistanceHop.bNodeActive ||
            MaxDistanceHop.ResolvedMaxDistanceWeightMap != AssetState.WeightMapName ||
            MaxDistanceHop.MaxDistanceWeightMapSourceNodeGuid !=
                Path.SourceNodeGuid ||
            MaxDistanceHop.MaxDistanceWeightMapSourceOutputName !=
                TEXT("OutputName.StringValue") ||
            !MaxDistanceHop.ResolvedInKinematic.IsEmpty() ||
            !MaxDistanceHop.InKinematicSourceNodeGuid.IsEmpty() ||
            !MaxDistanceHop.InKinematicSourceOutputName.IsEmpty() ||
            (!AssetState.MaxDistanceWeightMapName.IsEmpty() &&
             AssetState.MaxDistanceWeightMapName != AssetState.WeightMapName))
        {
            OutError = TEXT("The last downstream SimulationMaxDistanceConfig node must be active, take MaxDistance.WeightMap directly from the edited WeightMap node's OutputName.StringValue, and have no connected or non-empty InKinematic source");
            return false;
        }
    }

    const int32 LastTetherGeneratorIndex = FindLastTetherGeneratorIndex(Path);
    if (!Path.Hops.IsValidIndex(LastTetherGeneratorIndex))
    {
        OutError = TEXT("No recognized downstream tether generator reaches the selected terminal LOD");
        return false;
    }

    const FWeightMapDownstreamHop& TetherHop =
        Path.Hops[LastTetherGeneratorIndex];
    if (AssetState.bRequiresFixedEndWeightMapDerivedData)
    {
        if (TetherHop.NodeType !=
                TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode") ||
            !TetherHop.bNodeActive ||
            TetherHop.ResolvedFixedEndWeightMap != AssetState.WeightMapName ||
            (AssetState.bRequiresKinematicSelectionDerivedData &&
             LastTetherGeneratorIndex <= LastMaxDistanceConfigIndex))
        {
            OutError = TEXT("The last downstream tether generator must be an active legacy LongRangeAttachment node resolving the edited FixedEndWeightMap after any required MaxDistance selection writer");
            return false;
        }
        OutMode = AssetState.bRequiresKinematicSelectionDerivedData
            ? TEXT("max_distance_selection_then_legacy_fixed_end_weight_map")
            : TEXT("legacy_fixed_end_weight_map");
        return true;
    }

    if (AssetState.bRequiresKinematicSelectionDerivedData)
    {
        const FWeightMapDownstreamHop& MaxDistanceHop =
            Path.Hops[LastMaxDistanceConfigIndex];
        if (TetherHop.NodeType !=
                TEXT("FChaosClothAssetSimulationLongRangeAttachmentConfigNode_v2") ||
            LastTetherGeneratorIndex <= LastMaxDistanceConfigIndex ||
            !TetherHop.bNodeActive ||
            !TetherHop.bCustomTetherGenerationReadable ||
            TetherHop.bCustomTetherGeneration ||
            TetherHop.ResolvedFixedEndSet != AssetState.KinematicSelectionName ||
            AssetState.FixedEndSetName != AssetState.KinematicSelectionName ||
            TetherHop.FixedEndSetSourceNodeGuid != MaxDistanceHop.NodeGuid ||
            TetherHop.FixedEndSetSourceOutputName != TEXT("KinematicVertices3D"))
        {
            OutError = TEXT("Modern MaxDistance requires an active downstream LongRangeAttachmentConfig_v2 with custom generation disabled and FixedEndSet directly connected to the selected MaxDistance node's KinematicVertices3D output");
            return false;
        }
        OutMode = TEXT("max_distance_selection_then_long_range_v2");
        return true;
    }

    if (AssetState.bRequiresMaxDistanceDerivedData)
    {
        if (TetherHop.NodeType !=
                TEXT("FChaosClothAssetSimulationDefaultConfigNode") ||
            !TetherHop.bNodeActive ||
            !TetherHop.bDefaultInputPropertiesEvaluated ||
            TetherHop.bDefaultInputPropertiesValid ||
            AssetState.WeightMapName != TEXT("MaxDistance"))
        {
            OutError = TEXT("Legacy MaxDistance requires an active downstream SimulationDefaultConfig node whose input has no existing property schema; later or no-op DefaultConfig paths are rejected");
            return false;
        }
        OutMode = TEXT("simulation_default_config");
        return true;
    }
    return true;
}

bool VerifyWeightMapTetherAndSelectionDerivedState(
    const FClothWeightMapAssetState& State,
    FString& OutError)
{
    OutError.Reset();
    if (!State.bRequiresMaxDistanceDerivedData &&
        !State.bRequiresFixedEndWeightMapDerivedData)
    {
        return true;
    }
    if (!State.CollectionTethers.bValid)
    {
        OutError = FString::Printf(
            TEXT("Collection tether readback is invalid: %s"),
            *State.CollectionTethers.Error);
        return false;
    }
    if (!State.CompiledTethers.bValid)
    {
        OutError = FString::Printf(
            TEXT("Compiled solver tether readback is invalid: %s"),
            *State.CompiledTethers.Error);
        return false;
    }
    if (!State.bCollectionTethersMatchCompiled)
    {
        OutError = TEXT("Collection and compiled solver tether count/hash differ");
        return false;
    }
    if (State.bRequiresKinematicSelectionDerivedData &&
        !State.CollectionKinematicSelection.bValid)
    {
        OutError = FString::Printf(
            TEXT("Collection KinematicVertices3D selection readback is invalid: %s"),
            *State.CollectionKinematicSelection.Error);
        return false;
    }
    if (State.bRequiresKinematicSelectionDerivedData &&
        !State.CompiledKinematicSelection.bValid)
    {
        OutError = FString::Printf(
            TEXT("Compiled solver KinematicVertices3D VertexSet readback is invalid: %s"),
            *State.CompiledKinematicSelection.Error);
        return false;
    }
    if (State.bRequiresKinematicSelectionDerivedData &&
        !State.bCollectionSelectionMatchesCompiled)
    {
        OutError = TEXT("Collection and compiled solver KinematicVertices3D selection count/hash differ");
        return false;
    }
    return true;
}
bool IsDurablySavablePackage(const UPackage* Package)
{
    if (!Package || Package == GetTransientPackage() ||
        Package->HasAnyPackageFlags(PKG_CompiledIn | PKG_PlayInEditor))
    {
        return false;
    }
    const FString PackageName = Package->GetName();
    return !FPackageName::IsMemoryPackage(PackageName) &&
        !FPackageName::IsTempPackage(PackageName) &&
        FPackageName::IsValidLongPackageName(PackageName, false);
}

bool IsDurablySavableObject(const UObject* Object)
{
    if (!Object)
    {
        return false;
    }
    for (const UObject* Outer = Object; Outer; Outer = Outer->GetOuter())
    {
        if (Outer->HasAnyFlags(RF_Transient))
        {
            return false;
        }
    }
    return IsDurablySavablePackage(Object->GetOutermost());
}

bool SaveExactPackage(UPackage* Package)
{
    if (!IsDurablySavablePackage(Package))
    {
        return false;
    }
    TArray<UPackage*> Packages{Package};
    if(!Guard()) { return false; }TStrongObjectPtr<UPackage> Captured(Package);const FString Path=Package->GetName();
    SDK(TEXT("save_exact_package"));if(Work) { Work->R.bSaveAttempted=true;Count(Work->bCompensating?TEXT("compensation_save_calls"):TEXT("initial_save_calls")); }
    const bool Returned=UEditorLoadingAndSavingUtils::SavePackages(Packages,false);
    const bool Clean=Live(Package)&&!Package->IsDirty();const bool Saved=Returned&&Clean;
    TSharedPtr<FJsonObject> Row;
    if(Work) { Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("package"),Path);Row->SetStringField(TEXT("captured_package_path_before"),Path);if(Live(Package)) { Row->SetStringField(TEXT("captured_package_path_after"),Package->GetName()); }else { Row->SetField(TEXT("captured_package_path_after"),MakeShared<FJsonValueNull>()); }Row->SetBoolField(TEXT("captured_package_name_unchanged"),Live(Package)&&Package->GetName().Equals(Path,ESearchCase::CaseSensitive));Row->SetBoolField(TEXT("compensation"),Work->bCompensating);Row->SetBoolField(TEXT("SDK_save_returned"),Returned);Row->SetBoolField(TEXT("package_live_after"),Live(Package));Row->SetBoolField(TEXT("observed_clean_after"),Clean);Row->SetBoolField(TEXT("save_completed"),Saved);Work->SaveRows.Add(MakeShared<FJsonValueObject>(Row));Work->R.bSaved|=Returned; }
    const bool Identity=Guard();if(Row) { Row->SetBoolField(TEXT("captured_target_identity_check_passed_after_save"),Identity);if(Live(Work->Asset.Get())) { Row->SetStringField(TEXT("actual_cloth_owner_package_after"),Work->Asset->GetPackage()->GetName()); }if(Live(Work->Dataflow.Get())) { Row->SetStringField(TEXT("actual_Dataflow_owner_package_after"),Work->Dataflow->GetPackage()->GetName()); } }return Saved;
}

bool SaveObjectIfRequested(UObject* Object, bool bSaveRequested)
{
    if(!bSaveRequested) { return true; }if(!Guard()||!IsDurablySavableObject(Object)) { return false; }
    const bool Saved=SaveExactPackage(Object->GetOutermost());
    if(Work) { const TCHAR* Role=Object==Work->Dataflow.Get()?(Work->bCompensating?TEXT("rollback_dataflow_saved"):TEXT("dataflow_saved")):(Work->bCompensating?TEXT("rollback_cloth_asset_saved"):TEXT("cloth_asset_saved"));Work->Prefix->SetBoolField(Role,Saved); }
    return Saved;
}
FProperty* FindEditableDataflowProperty(const UScriptStruct* Struct, const FString& Name)
{
    if (!Struct || Name.IsEmpty())
    {
        return nullptr;
    }
    FProperty* Property = FindFProperty<FProperty>(Struct, FName(*Name));
    return IsSupportedDataflowProperty(Property) ? Property : nullptr;
}

void ValidateDataflowNumericConstraints(
    const FProperty& Property,
    const void* ValuePtr,
    const FString& PropertyPath,
    TArray<FString>& OutErrors,
    const FString& InheritedClampMin = FString(),
    const FString& InheritedClampMax = FString())
{
    if (!ValuePtr)
    {
        OutErrors.Add(FString::Printf(TEXT("%s has no readable value"), *PropertyPath));
        return;
    }

    FString EffectiveClampMin = Property.GetMetaData(TEXT("ClampMin"));
    if (EffectiveClampMin.IsEmpty())
    {
        EffectiveClampMin = InheritedClampMin;
    }
    FString EffectiveClampMax = Property.GetMetaData(TEXT("ClampMax"));
    if (EffectiveClampMax.IsEmpty())
    {
        EffectiveClampMax = InheritedClampMax;
    }

    if (const FNumericProperty* NumericProperty = CastField<FNumericProperty>(&Property))
    {
        if (NumericProperty->IsEnum())
        {
            return;
        }
        FString Exported;
        SDK(TEXT("export_Dataflow_property"));Count(TEXT("property_export_calls"));
    Property.ExportTextItem_Direct(Exported, ValuePtr, nullptr, nullptr, PPF_None);
        double NumericValue = 0.0;
        if (!LexTryParseString(NumericValue, *Exported) || !FMath::IsFinite(NumericValue))
        {
            OutErrors.Add(FString::Printf(
                TEXT("%s must resolve to a finite numeric value"),
                *PropertyPath));
            return;
        }

        double ClampMin = 0.0;
        if (!EffectiveClampMin.IsEmpty() && LexTryParseString(ClampMin, *EffectiveClampMin) &&
            NumericValue < ClampMin)
        {
            OutErrors.Add(FString::Printf(
                TEXT("%s is below ClampMin %s"),
                *PropertyPath,
                *EffectiveClampMin));
        }
        double ClampMax = 0.0;
        if (!EffectiveClampMax.IsEmpty() && LexTryParseString(ClampMax, *EffectiveClampMax) &&
            NumericValue > ClampMax)
        {
            OutErrors.Add(FString::Printf(
                TEXT("%s exceeds ClampMax %s"),
                *PropertyPath,
                *EffectiveClampMax));
        }
        return;
    }

    if (const FStructProperty* StructProperty = CastField<FStructProperty>(&Property))
    {
        for (TFieldIterator<FProperty> It(StructProperty->Struct, EFieldIteratorFlags::IncludeSuper);
             It;
             ++It)
        {
            const FProperty* NestedProperty = *It;
            if (!NestedProperty)
            {
                continue;
            }
            const void* NestedValue = NestedProperty->ContainerPtrToValuePtr<void>(ValuePtr);
            ValidateDataflowNumericConstraints(
                *NestedProperty,
                NestedValue,
                FString::Printf(TEXT("%s.%s"), *PropertyPath, *NestedProperty->GetName()),
                OutErrors,
                EffectiveClampMin,
                EffectiveClampMax);
        }
    }
}

struct FLoadedDataflowOwnerReference
{
    FString ObjectPath;
    FString ClassPath;
    FString PackageName;
    bool bPackageDirty = false;
    bool bUnsaved = false;
    bool bTransient = false;
};

struct FDataflowReferencerSnapshot
{
    TArray<FString> OnDiskReferencerPackages;
    TArray<FString> OtherOnDiskReferencerPackages;
    TArray<FString> OtherReferencerPackages;
    TArray<FLoadedDataflowOwnerReference> OtherLoadedOwners;
};

TArray<TSharedPtr<FJsonValue>> LoadedDataflowOwnerReferencesToJson(
    const TArray<FLoadedDataflowOwnerReference>& References)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    Result.Reserve(References.Num());
    for (const FLoadedDataflowOwnerReference& Reference : References)
    {
        TSharedPtr<FJsonObject> Item = MakeShared<FJsonObject>();
        Item->SetStringField(TEXT("object_path"), Reference.ObjectPath);
        Item->SetStringField(TEXT("class_path"), Reference.ClassPath);
        Item->SetStringField(TEXT("package_name"), Reference.PackageName);
        Item->SetBoolField(TEXT("package_dirty"), Reference.bPackageDirty);
        Item->SetBoolField(TEXT("unsaved"), Reference.bUnsaved);
        Item->SetBoolField(TEXT("transient"), Reference.bTransient);
        Result.Add(MakeShared<FJsonValueObject>(Item));
    }
    return Result;
}

TArray<FString> DescribeLoadedDataflowOwnerReferences(
    const TArray<FLoadedDataflowOwnerReference>& References)
{
    TArray<FString> Result;
    Result.Reserve(References.Num());
    for (const FLoadedDataflowOwnerReference& Reference : References)
    {
        Result.Add(FString::Printf(
            TEXT("%s [package=%s, dirty=%s, unsaved=%s, transient=%s]"),
            *Reference.ObjectPath,
            *Reference.PackageName,
            Reference.bPackageDirty ? TEXT("true") : TEXT("false"),
            Reference.bUnsaved ? TEXT("true") : TEXT("false"),
            Reference.bTransient ? TEXT("true") : TEXT("false")));
    }
    return Result;
}

FDataflowReferencerSnapshot GetDataflowReferencers(
    UDataflow* Dataflow,
    const UObject* ExactTargetOwner)
{
    FDataflowReferencerSnapshot Result;
    if (!Dataflow)
    {
        return Result;
    }

    SDK(TEXT("resolve_Dataflow_owners"));
    FAssetRegistryModule& AssetRegistry =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if(!Guard()) { return Result; }
    TArray<FName> Referencers;
    AssetRegistry.Get().GetReferencers(Dataflow->GetOutermost()->GetFName(), Referencers);
    for (const FName Referencer : Referencers)
    {
        Result.OnDiskReferencerPackages.AddUnique(Referencer.ToString());
    }
    Result.OnDiskReferencerPackages.Sort();

    const FString TargetPackageName = ExactTargetOwner && ExactTargetOwner->GetOutermost()
        ? ExactTargetOwner->GetOutermost()->GetName()
        : FString();

    // The Asset Registry dependency edge is package-level. Load the other top-level
    // assets in the already selected package so a second Dataflow owner in that same
    // package can be identified by exact object rather than being hidden with the
    // selected owner's own package edge.
    if (!TargetPackageName.IsEmpty())
    {
        TArray<FAssetData> SamePackageAssets;
        AssetRegistry.Get().GetAssetsByPackageName(
            FName(*TargetPackageName),
            SamePackageAssets,
            true);
        const FString TargetObjectPath = ExactTargetOwner->GetPathName();
        for (const FAssetData& AssetData : SamePackageAssets)
        {
            if (AssetData.GetObjectPathString() != TargetObjectPath)
            {
                SDK(TEXT("load_same_package_Dataflow_owner"));Count(TEXT("referencer_load_calls"));AssetData.GetAsset();if(!Guard()) { return Result; }
            }
        }
    }

    for (TObjectIterator<UObject> It; It; ++It)
    {
        UObject* OwnerObject = *It;
        if (!IsValid(OwnerObject) || OwnerObject == ExactTargetOwner ||
            OwnerObject->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
        {
            continue;
        }
        TStrongObjectPtr<UObject> OwnerHold(OwnerObject);
        const IDataflowInstanceInterface* DataflowOwner =
            Cast<IDataflowInstanceInterface>(OwnerObject);
        SDK(TEXT("read_loaded_Dataflow_owner"));
        const UDataflow* ReferencedDataflow=DataflowOwner?DataflowOwner->GetDataflowInstance().GetDataflowAsset():nullptr;
        if(!Guard()) { return Result; }
        if (!DataflowOwner || !Live(OwnerObject) || ReferencedDataflow != Dataflow)
        {
            continue;
        }

        UPackage* OwnerPackage = OwnerObject->GetOutermost();
        FLoadedDataflowOwnerReference& Reference = Result.OtherLoadedOwners.AddDefaulted_GetRef();
        Reference.ObjectPath = OwnerObject->GetPathName();
        Reference.ClassPath = OwnerObject->GetClass()->GetPathName();
        Reference.PackageName = OwnerPackage ? OwnerPackage->GetName() : FString();
        Reference.bPackageDirty = OwnerPackage && OwnerPackage->IsDirty();
        Reference.bTransient = OwnerObject->HasAnyFlags(RF_Transient) ||
            OwnerPackage == GetTransientPackage();
        const bool bPackageExistsOnDisk = OwnerPackage &&
            FPackageName::IsValidLongPackageName(Reference.PackageName) &&
            FPackageName::DoesPackageExist(Reference.PackageName);
        Reference.bUnsaved = Reference.bTransient || !OwnerPackage ||
            OwnerPackage->HasAnyPackageFlags(PKG_NewlyCreated) ||
            !bPackageExistsOnDisk;
    }
    Result.OtherLoadedOwners.Sort(
        [](const FLoadedDataflowOwnerReference& Left, const FLoadedDataflowOwnerReference& Right)
        {
            return Left.ObjectPath < Right.ObjectPath;
        });

    for (const FString& ReferencerPackage : Result.OnDiskReferencerPackages)
    {
        if (ReferencerPackage != TargetPackageName)
        {
            Result.OtherOnDiskReferencerPackages.AddUnique(ReferencerPackage);
            Result.OtherReferencerPackages.AddUnique(ReferencerPackage);
        }
    }
    for (const FLoadedDataflowOwnerReference& Reference : Result.OtherLoadedOwners)
    {
        if (!Reference.PackageName.IsEmpty())
        {
            Result.OtherReferencerPackages.AddUnique(Reference.PackageName);
        }
    }
    Result.OtherOnDiskReferencerPackages.Sort();
    Result.OtherReferencerPackages.Sort();
    return Result;
}

TSharedPtr<FDataflowNode> FindUniqueDataflowNode(
    UE::Dataflow::FGraph* Graph,
    const FString& NodeName,
    int32& OutMatchCount)
{
    OutMatchCount = 0;
    TSharedPtr<FDataflowNode> Result;
    if (!Graph)
    {
        return Result;
    }
    for (const TSharedPtr<FDataflowNode>& Node : Graph->GetNodes())
    {
        if (Node.IsValid() && Node->GetName().ToString() == NodeName)
        {
            ++OutMatchCount;
            Result = Node;
        }
    }
    return Result;
}

TSharedPtr<FJsonObject> MakeSelectedDataflowPropertySnapshot(
    const TSharedPtr<FDataflowNode>& Node,
    const TArray<FString>& PropertyNames)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Node.IsValid())
    {
        return Result;
    }
    const UScriptStruct* Struct = Node->TypedScriptStruct();
    for (const FString& Name : PropertyNames)
    {
        if (FProperty* Property = FindEditableDataflowProperty(Struct, Name))
        {
            Result->SetStringField(Name, ExportDataflowProperty(*Node, *Property));
        }
    }
    return Result;
}

// UE58 authoring uses public tool snapshots. Private legacy VertexWeights is never
// read or written by this writer. The old inspection projection is retained by FR.
bool ValidateCurrentWeightMapNodeLayout(const TSharedPtr<FDataflowNode>& Node,FString& Error)
{
    const auto* S=Node.IsValid()?Node->TypedScriptStruct():nullptr;
    if(!S||S!=FChaosClothAssetWeightMapNode::StaticStruct()||!S->IsChildOf(FDataflowVertexAttributeEditableNode::StaticStruct()))
    {
        Error=TEXT("Only the installed current FChaosClothAssetWeightMapNode with the public editable snapshot base is supported; deprecated AddWeightMap nodes are rejected");return false;
    }return true;
}
FString Md5Bytes(const TArray<uint8>& Bytes)
{
    FMD5 M;if(!Bytes.IsEmpty()) { M.Update(Bytes.GetData(),static_cast<uint32>(Bytes.Num())); }uint8 D[16];M.Final(D);return BytesToHex(D,UE_ARRAY_COUNT(D)).ToLower();
}
bool SerializeCollectionCopy(const FManagedArrayCollection& Source,FString& Hash,int64& TotalBytes,FString& Error)
{
    if(Source.GetAllocatedSize()>static_cast<SIZE_T>(MaxSnapshotBytes)) { Error=TEXT("Collection exceeds the bounded snapshot/input copy budget");return false; }
    FManagedArrayCollection Copy(Source);TArray<uint8> Bytes;FMemoryWriter Memory(Bytes,true);FObjectAndNameAsStringProxyArchive Ar(Memory,false);
    SDK(TEXT("serialize_authoring_collection_copy"));Count(TEXT("snapshot_serialization_calls"));const bool OK=Copy.Serialize(Ar);TotalBytes+=Bytes.Num();
    if(!Guard()||!OK||Ar.IsError()||TotalBytes>MaxSnapshotBytes) { Error=TEXT("Authoring collection-copy serialization failed, target changed, or aggregate bytes exceeded 64 MiB");return false; }Hash=Md5Bytes(Bytes);return true;
}
bool DescribeCurrentState(const TSharedPtr<FDataflowNode>& Node,UObject* Asset,TSharedPtr<FJsonObject>& Out,FString& Error,TArray<float>* Stored=nullptr)
{
    if(!Guard()||!Live(Asset)||!ValidateCurrentWeightMapNodeLayout(Node,Error)) { return false; }
    TStrongObjectPtr<UObject> OwnerHold(Asset);const auto* ClothOwner=Cast<UChaosClothAssetBase>(Asset);
    UDataflow* BoundDataflow=ClothOwner?ClothOwner->GetDataflowInstance().GetDataflowAsset():nullptr;
    TStrongObjectPtr<UDataflow> DataflowHold(BoundDataflow);const auto CapturedGraph=Live(BoundDataflow)?BoundDataflow->GetDataflow():nullptr;
    const FGuid CapturedGuid=Node->GetGuid();const UScriptStruct* CapturedStruct=Node->TypedScriptStruct();
    const FString CapturedAssetPath=Asset->GetPathName(),CapturedDataflowPath=Live(BoundDataflow)?BoundDataflow->GetPathName():FString();const UPackage* CapturedAssetPackage=Asset->GetPackage();const UPackage* CapturedDataflowPackage=Live(BoundDataflow)?BoundDataflow->GetPackage():nullptr;
    auto CurrentLive=[&]() { if(!Guard()||!Live(Asset)||!Live(BoundDataflow)||!CapturedGraph.IsValid()||Asset->GetPackage()!=CapturedAssetPackage||BoundDataflow->GetPackage()!=CapturedDataflowPackage||!Asset->GetPathName().Equals(CapturedAssetPath,ESearchCase::CaseSensitive)||!BoundDataflow->GetPathName().Equals(CapturedDataflowPath,ESearchCase::CaseSensitive)||BoundDataflow->GetDataflow()!=CapturedGraph||ClothOwner->GetDataflowInstance().GetDataflowAsset()!=BoundDataflow) { Error=TEXT("Captured authoring state target/graph changed during public SDK evaluation");return false; }for(const auto& N:CapturedGraph->GetNodes()) { if(N.IsValid()&&N->GetGuid()==CapturedGuid) { if(N==Node&&N->TypedScriptStruct()==CapturedStruct) { return true; }Error=TEXT("Captured authoring node identity was replaced");return false; } }Error=TEXT("Captured authoring node is no longer owned by the graph");return false; };
    if(!CurrentLive()) { return false; }
    const auto* Typed=static_cast<const FChaosClothAssetWeightMapNode*>(Node.Get());
    const auto* Tool=static_cast<const FDataflowToolNode*>(Node.Get());
    const auto* Editable=static_cast<const FDataflowVertexAttributeEditableNode*>(Node.Get());
    auto State=MakeShared<FJsonObject>();State->SetStringField(TEXT("node_name"),Node->GetName().ToString());State->SetStringField(TEXT("node_type"),Node->GetType().ToString());State->SetStringField(TEXT("node_struct"),Node->TypedScriptStruct()->GetName());State->SetStringField(TEXT("node_guid"),Node->GetGuid().ToString(EGuidFormats::DigitsWithHyphens));
    State->SetStringField(TEXT("weight_map_name"),Typed->OutputName.StringValue);State->SetBoolField(TEXT("node_active"),Node->IsActive());
    auto Settings=MakeShared<FJsonObject>();
    for(const TCHAR* Key:{TEXT("InputName"),TEXT("OutputName"),TEXT("MeshTarget"),TEXT("MapOverrideType"),TEXT("TransferType")})
    {
        const FProperty* P=FindFProperty<FProperty>(Node->TypedScriptStruct(),FName(Key));if(!P) { Error=TEXT("Installed current WeightMap public configuration schema differs");return false; }
        Settings->SetStringField(Key,ExportDataflowProperty(*Node,*P));if(!CurrentLive()) { return false; }
    }
    State->SetObjectField(TEXT("public_configuration"),Settings);State->SetStringField(TEXT("mesh_target"),Settings->GetStringField(TEXT("MeshTarget")));State->SetStringField(TEXT("map_override_type"),Settings->GetStringField(TEXT("MapOverrideType")));
    TArray<FString> Connections;
    for(const FDataflowInput* Input:Node->GetInputs())
    {
        if(!Input) { continue; }FString Row=Input->GetName().ToString()+TEXT(":")+Input->GetType().ToString();
        if(const FDataflowOutput* Source=Input->GetConnection())
        {
            const FDataflowNode* Owner=Source->GetOwningNode();if(!Owner) { Error=TEXT("Input connection has no live owner");return false; }
            Row+=TEXT("<-")+Owner->GetGuid().ToString(EGuidFormats::DigitsWithHyphens)+TEXT(":")+Owner->GetName().ToString()+TEXT(":")+Owner->GetType().ToString()+TEXT(":")+Source->GetName().ToString()+TEXT(":")+Source->GetType().ToString()+TEXT(":")+(Source->HasFrozenValue()?TEXT("frozen"):TEXT("live"));
        }Connections.Add(Row);
    }
    for(const FDataflowOutput* Output:Node->GetOutputs()) { if(Output) { Connections.Add(TEXT("out:")+Output->GetName().ToString()+TEXT(":")+Output->GetType().ToString()+TEXT(":")+(Output->HasFrozenValue()?TEXT("frozen"):TEXT("live"))); } }
    Connections.Sort([](const FString& A,const FString& B){return A.Compare(B,ESearchCase::CaseSensitive)<0;});State->SetArrayField(TEXT("connections"),StringsToJson(Connections));
    const auto Snapshots=Tool->GetSnapshots();const auto* Active=Tool->GetActiveSnapshot();if(Snapshots.Num()>MaxSnapshots) { Error=TEXT("Snapshot count exceeds bounded authoring observation");return false; }
    int32 ActiveIndex=INDEX_NONE;int64 TotalBytes=0,SnapshotAllocatedBytes=0;TArray<TSharedPtr<FJsonValue>> Rows;
    for(int32 I=0;I<Snapshots.Num();++I)
    {
        const auto& S=Snapshots[I];SnapshotAllocatedBytes+=static_cast<int64>(S.Data.GetAllocatedSize());if(SnapshotAllocatedBytes>MaxSnapshotBytes) { Error=TEXT("Aggregate snapshot allocation exceeds the bounded public ToolChange copy budget");return false; }if(&S==Active) { ActiveIndex=I; }FString H;if(!SerializeCollectionCopy(S.Data,H,TotalBytes,Error)||!CurrentLive()) { return false; }
        auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("index"),I);J->SetStringField(TEXT("name"),S.Name);J->SetStringField(TEXT("description"),S.Description);J->SetStringField(TEXT("date"),S.GetDate().ToIso8601());J->SetBoolField(TEXT("locked"),S.IsLocked());J->SetStringField(TEXT("collection_serialized_md5"),H);Rows.Add(MakeShared<FJsonValueObject>(J));
    }
    State->SetNumberField(TEXT("snapshot_count"),Snapshots.Num());State->SetNumberField(TEXT("active_snapshot_index"),ActiveIndex);State->SetArrayField(TEXT("snapshots"),Rows);
    const FDataflowInput* Input=Node->FindInput(FName(TEXT("Collection")));
    if(!Input||Input->GetType()!=FManagedArrayCollection::StaticType()) { Error=TEXT("Current WeightMap Collection input schema is unavailable");return false; }
    UE::Dataflow::FEngineContext Context(Asset);const FManagedArrayCollection Empty;
    SDK(TEXT("evaluate_weightmap_current_input"));Count(TEXT("evaluation_calls"));const FManagedArrayCollection Collection=Input->GetValue<FManagedArrayCollection>(Context,Empty);Count(TEXT("evaluation_returns"));if(!CurrentLive()) { return false; }
    const auto Shared=MakeShared<FManagedArrayCollection>(Collection);const UE::Chaos::ClothAsset::FCollectionClothConstFacade Cloth(Shared);
    if(!Cloth.IsValid()) { Error=TEXT("Current WeightMap has no valid evaluated input cloth collection");return false; }
    FString InputHash;if(!SerializeCollectionCopy(Collection,InputHash,TotalBytes,Error)||!CurrentLive()) { return false; }State->SetStringField(TEXT("current_input_collection_serialized_md5"),InputHash);
    if(const FDataflowInput* TransferInput=Node->FindInput(FName(TEXT("TransferCollection")))) { SDK(TEXT("evaluate_weightmap_transfer_input"));Count(TEXT("evaluation_calls"));const FManagedArrayCollection Transfer=TransferInput->GetValue<FManagedArrayCollection>(Context,Empty);Count(TEXT("evaluation_returns"));if(!CurrentLive()) { return false; }FString H;if(!SerializeCollectionCopy(Transfer,H,TotalBytes,Error)||!CurrentLive()) { return false; }State->SetStringField(TEXT("current_transfer_input_collection_serialized_md5"),H); }
    if(const auto* StringInput=Node->FindInput(FName(TEXT("InputName.StringValue"))))
    {
        SDK(TEXT("evaluate_weightmap_input_name"));Count(TEXT("evaluation_calls"));const FString Name=StringInput->GetValue<FString>(Context,Typed->InputName.StringValue);Count(TEXT("evaluation_returns"));if(!CurrentLive()) { return false; }State->SetStringField(TEXT("resolved_input_name"),Name);
    }
    TArray<float> Effective;SDK(TEXT("evaluate_weightmap_current_values"));Count(TEXT("evaluation_calls"));Editable->GetVertexAttributeValues(Context,Effective);Count(TEXT("evaluation_returns"));if(!CurrentLive()) { return false; }
    if(Effective.Num()>MaxMeshElements) { Error=TEXT("Current evaluated values exceed the bounded authoring observation");return false; }
    for(float V:Effective) { if(!FMath::IsFinite(V)) { Error=TEXT("Current evaluated weights are nonfinite");return false; } }
    State->SetNumberField(TEXT("current_evaluated_weight_count"),Effective.Num());State->SetStringField(TEXT("current_evaluated_weights_md5"),HashFloatValues(Effective));
    TArray<float> StoredValues;
    // Public snapshot collection read. These installed UE58 schema literals are
    // verified in WeightMapNode.cpp 505-510/560/649; no Private symbol is called.
    if(Active)
    {
        const auto* Values=Active->Data.FindAttribute<float>(FName(TEXT("Weights")),FName(TEXT("Vertices")));
        if(Values) { if(Values->Num()>MaxMeshElements) { Error=TEXT("Active snapshot weights exceed the bound");return false; }StoredValues.Append(Values->GetConstArray()); }
        State->SetStringField(TEXT("stored_weight_representation"),TEXT("public_active_snapshot_Weights_in_Vertices_source_values_before_override"));
    }
    else
    {
        StoredValues=Effective;State->SetStringField(TEXT("stored_weight_representation"),TEXT("public_evaluated_values_fallback_no_active_snapshot_private_legacy_not_read"));
    }
    for(float V:StoredValues) { if(!FMath::IsFinite(V)) { Error=TEXT("Stored snapshot weights are nonfinite");return false; } }
    State->SetNumberField(TEXT("stored_weight_count"),StoredValues.Num());State->SetStringField(TEXT("stored_weights_md5"),HashFloatValues(StoredValues));
    State->SetBoolField(TEXT("stored_weights_are_legacy_private_array"),false);State->SetBoolField(TEXT("private_legacy_VertexWeights_in_current_token"),false);State->SetBoolField(TEXT("current_snapshot_projection_complete"),true);
    // Hash only the fixed, ordered pre-hash projection. All callers share this code.
    FMD5 Hash;UpdateMd5WithString(Hash,CurrentTokenVersion);UpdateMd5WithString(Hash,Json(State));
    uint8 Digest[16];Hash.Final(Digest);const FString Token=BytesToHex(Digest,UE_ARRAY_COUNT(Digest)).ToLower();
    State->SetStringField(TEXT("node_state_md5"),Token);State->SetStringField(TEXT("node_state_hash_version"),CurrentTokenVersion);
    State->SetStringField(TEXT("node_state_hash_contract"),CurrentTokenVersion);
    State->SetStringField(TEXT("node_state_hash_scope"),TEXT("exact_current_WeightMap_guid_active_public_InputName_OutputName_MeshTarget_MapOverrideType_TransferType_input_output_connections_freeze_all_snapshot_metadata_collections_activeindex_evaluated_Collection_TransferCollection_resolved_inputname_and_effective_weights"));
    State->SetBoolField(TEXT("node_state_hash_covers_all_current_settings"),false);State->SetBoolField(TEXT("node_state_hash_covers_reviewed_writer_preconditions"),true);State->SetStringField(TEXT("node_state_hash_excluded_scope"),TEXT("entire_graph_Editor_cache_unreviewed_future_settings_and_effective_runtime_simulation_are_not_claimed"));State->SetBoolField(TEXT("node_state_hash_cross_version_compatible"),false);
    State->SetBoolField(TEXT("node_state_hash_is_security_or_collision_proof"),false);State->SetBoolField(TEXT("snapshot_hash_durable_roundtrip_verified"),false);
    State->SetBoolField(TEXT("supported_for_weight_map_write"),true);State->SetStringField(TEXT("weight_map_write_support_error"),TEXT(""));State->SetStringField(TEXT("weight_map_writer_additional_gates"),TEXT("Type eligibility and current token only; apply additionally requires Simulation mesh, exact input/terminal topology, existing map, selected exclusive path, coherent derived state, no shared owner, explicit expected values and clean exact-save packages."));
    if(Stored) { *Stored=MoveTemp(StoredValues); }Out=State;return CurrentLive();
}
struct FClothWeightMapNodeState
{
    FString NodeName,NodeType,NodeStruct,WeightMapName,MeshTarget,MapOverrideType,StoredWeightsMd5,StateMd5,WriteSupportError;
    TArray<float> StoredWeights;TSharedPtr<FJsonObject> Current;bool bSupportedForWrite=false;
};
bool ReadClothWeightMapNodeState(const TSharedPtr<FDataflowNode>& Node,FClothWeightMapNodeState& Out)
{
    FString Error;Out=FClothWeightMapNodeState();if(!Work||!DescribeCurrentState(Node,Work->Asset.Get(),Out.Current,Error,&Out.StoredWeights)) { if(Work&&!Error.IsEmpty()) { Work->Fail(TEXT("node_state_unavailable"),Error); }return false; }
    auto& J=Out.Current;Out.NodeName=J->GetStringField(TEXT("node_name"));Out.NodeType=J->GetStringField(TEXT("node_type"));Out.NodeStruct=J->GetStringField(TEXT("node_struct"));Out.WeightMapName=J->GetStringField(TEXT("weight_map_name"));Out.MeshTarget=J->GetStringField(TEXT("mesh_target"));Out.MapOverrideType=J->GetStringField(TEXT("map_override_type"));Out.StoredWeightsMd5=J->GetStringField(TEXT("stored_weights_md5"));Out.StateMd5=J->GetStringField(TEXT("node_state_md5"));Out.bSupportedForWrite=true;return true;
}
TSharedPtr<FJsonObject> MakeClothWeightMapNodeStateSnapshot(const FClothWeightMapNodeState& State)
{
    return State.Current.IsValid()?State.Current:MakeShared<FJsonObject>();
}
bool SetWeightMapNodeReplaceAll(FDataflowNode& Node,const TConstArrayView<float> Values)
{
    if(!Work||!Guard()||Node.TypedScriptStruct()!=FChaosClothAssetWeightMapNode::StaticStruct()) { return false; }
    if(!WriteExactDataflowEnumValue(Node,TEXT("MapOverrideType"),TEXT("EChaosClothAssetWeightMapOverrideType"),TEXT("ReplaceAll"))) { return false; }
    TArray<float> Copy;Copy.Append(Values);TArray<int32> NoMapping;
    auto* Editable=static_cast<FDataflowVertexAttributeEditableNode*>(&Node);UE::Dataflow::FEngineContext Context(Work->Asset.Get());
    Work->Write(TEXT("public_snapshot_vertex_attribute_setter"));Count(TEXT("snapshot_setter_calls"));Editable->SetVertexAttributeValues(Context,Copy,NoMapping);Work->Prefix->SetBoolField(TEXT("public_snapshot_setter_returned"),true);
    return Guard();
}
bool Regenerate(UChaosClothAssetBase* Asset)
{
    if(!Guard()) { return false; }SDK(TEXT("regenerate_cloth_asset"));Count(TEXT("regeneration_calls"));const bool OK=UDataflowBlueprintLibrary::RegenerateAssetFromDataflow(Asset,false);Count(TEXT("regeneration_returns"));if(Work) { Work->Prefix->SetBoolField(Work->bCompensating?TEXT("compensation_regeneration_returned"):TEXT("initial_regeneration_returned"),OK); }return Guard()&&OK;
}
TSharedPtr<FJsonObject> LegacyConfigureNode(
    const TSharedPtr<FJsonObject>& Params)
{
    FJsonParamParseState ParamState;
    const FString AssetPath = GetStringParam(Params, TEXT("cloth_asset_path"), ParamState);
    const FString NodeName = GetStringParam(Params, TEXT("node_name"), ParamState);
    const FString ExpectedNodeType = GetStringParam(Params, TEXT("expected_node_type"), ParamState);
    const FString ExpectedDataflowPath = GetStringParam(
        Params,
        TEXT("expected_dataflow_asset_path"),
        ParamState);
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true, ParamState);
    const bool bAllowConnectedInputs = GetBoolParam(
        Params,
        TEXT("allow_connected_inputs"),
        false,
        ParamState);
    const bool bRegenerateAsset = GetBoolParam(Params, TEXT("regenerate_asset"), true, ParamState);
    const bool bSaveAssets = GetBoolParam(Params, TEXT("save_assets"), false, ParamState);
    Work->R.bSaveRequested=bSaveAssets;
    if (!ParamState.IsValid())
    {
        return FLegacy::CreateErrorResponse(ParamState.GetErrorMessage());
    }
    if (NodeName.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(TEXT("node_name is required"));
    }
    if (!bDryRun && IsPlaySessionActive())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Durable Chaos Cloth Dataflow edits are blocked while PIE/SIE is active"));
    }
    if (!bDryRun && !bRegenerateAsset)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Apply mode requires regenerate_asset=true so the cloth asset cannot be left stale"));
    }

    const TSharedPtr<FJsonObject>* PropertyValuesPtr = nullptr;
    if (!Params.IsValid() ||
        !Params->TryGetObjectField(TEXT("property_values"), PropertyValuesPtr) ||
        !PropertyValuesPtr || !PropertyValuesPtr->IsValid() ||
        (*PropertyValuesPtr)->Values.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("property_values must be a non-empty object mapping top-level property names to Unreal export-text strings"));
    }
    if ((*PropertyValuesPtr)->Values.Num() > 64)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("property_values is limited to 64 fields per call"));
    }
    for (const auto& Pair : (*PropertyValuesPtr)->Values)
    {
        FString IgnoredImportText;
        if (!ReadString(Pair.Value, IgnoredImportText))
        {
            return FLegacy::CreateErrorResponse(FString::Printf(
                TEXT("property_values.%s must be an Unreal export-text string"),
                *FString(Pair.Key)));
        }
    }
    const TSharedPtr<FJsonObject>* ExpectedCurrentPtr = nullptr;
    TSharedPtr<FJsonObject> ExpectedCurrent;
    if (Params->HasField(TEXT("expected_current")) &&
        !Params->HasTypedField<EJson::Object>(TEXT("expected_current")))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("expected_current must be a JSON object when supplied"));
    }
    if (Params->TryGetObjectField(TEXT("expected_current"), ExpectedCurrentPtr) &&
        ExpectedCurrentPtr)
    {
        ExpectedCurrent = *ExpectedCurrentPtr;
    }

    FString Error;
    UChaosClothAssetBase* Asset = LoadClothAsset(AssetPath, Error);
    if (!Asset)
    {
        return FLegacy::CreateErrorResponse(Error);
    }
    SDK(TEXT("select_cloth_Dataflow"));Count(TEXT("Dataflow_getter_calls"));
    UDataflow* Dataflow = Asset->GetDataflowInstance().GetDataflowAsset();
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if(Dataflow && Work && !Work->Capture(Dataflow)) { return ErrorResponse(Work->R.Error); }
    if (!Dataflow)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The selected cloth asset has no Dataflow asset"));
    }
    if (FLegacy::IsMCPDependencyReference(Dataflow->GetPathName()))
    {
        return FLegacy::CreateErrorResponse(
            FLegacy::GetMCPDependencyReferenceError(
                TEXT("dataflow_asset_path"),
                Dataflow->GetPathName()));
    }
    if (!bDryRun && ExpectedDataflowPath.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Apply mode requires expected_dataflow_asset_path from inspect_chaos_cloth_asset"));
    }
    if (!ExpectedDataflowPath.IsEmpty() && Dataflow->GetPathName() != ExpectedDataflowPath)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Dataflow path mismatch: expected '%s', actual '%s'"),
            *ExpectedDataflowPath,
            *Dataflow->GetPathName()));
    }

    const auto ReadEdit = Dataflow->GetDataflow();
    UE::Dataflow::FGraph* Graph = ReadEdit.Get();
    int32 NodeMatchCount = 0;
    TSharedPtr<FDataflowNode> Node = FindUniqueDataflowNode(Graph, NodeName, NodeMatchCount);
    if (NodeMatchCount != 1 || !Node.IsValid())
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Dataflow node name must resolve exactly once (matched %d)"),
            NodeMatchCount));
    }
    if (!IsClothDataflowNode(*Node))
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Node '%s' is not a reviewed Chaos Cloth Dataflow node"),
            *NodeName));
    }
    if(!Work->Capture(Node)) { return ErrorResponse(Work->R.Error); }
    const UScriptStruct* NodeStruct = Node->TypedScriptStruct();
    const FString NodeType = Node->GetType().ToString();
    const FString NodeStructName = NodeStruct ? NodeStruct->GetName() : FString();
    if (!bDryRun && ExpectedNodeType.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Apply mode requires expected_node_type from inspect_chaos_cloth_asset"));
    }
    if (!ExpectedNodeType.IsEmpty() &&
        ExpectedNodeType != NodeType && ExpectedNodeType != NodeStructName)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Node type mismatch: expected '%s', actual type '%s' / struct '%s'"),
            *ExpectedNodeType,
            *NodeType,
            *NodeStructName));
    }

    TArray<FString> PropertyNames;
    TArray<FProperty*> ValidatedProperties;
    TArray<FString> ValidationErrors;
    TSharedPtr<FJsonObject> CanonicalPropertyValues = MakeShared<FJsonObject>();
    TSharedPtr<FJsonObject> ConnectedInputPathsByProperty = MakeShared<FJsonObject>();
    TUniquePtr<FStructOnScope> Scratch = MakeUnique<FStructOnScope>(NodeStruct);
    if (!Scratch.IsValid() || !Scratch->GetStructMemory())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Unable to allocate a validation copy of the Dataflow node struct"));
    }
    for (const auto& Pair : (*PropertyValuesPtr)->Values)
    {
        FString ImportText;
        if (!ReadString(Pair.Value, ImportText))
        {
            ValidationErrors.Add(FString::Printf(
                TEXT("property_values.%s must be an Unreal export-text string"),
                *FString(Pair.Key)));
            continue;
        }
        if (FLegacy::IsMCPDependencyReference(ImportText))
        {
            ValidationErrors.Add(FLegacy::GetMCPDependencyReferenceError(
                FString::Printf(TEXT("property_values.%s"), *FString(Pair.Key)),
                ImportText));
            continue;
        }
        FProperty* Property = FindEditableDataflowProperty(NodeStruct, FString(Pair.Key));
        if (!Property)
        {
            ValidationErrors.Add(FString::Printf(
                TEXT("'%s' is not an eligible top-level editable scalar/enum/struct property on %s"),
                *FString(Pair.Key),
                *NodeStructName));
            continue;
        }
        const TArray<FString> ConnectedInputPaths =
            GetConnectedDataflowInputPaths(*Node, *Property);
        ConnectedInputPathsByProperty->SetArrayField(
            FString(Pair.Key),
            StringsToJson(ConnectedInputPaths));
        if (!ConnectedInputPaths.IsEmpty() && !bAllowConnectedInputs)
        {
            ValidationErrors.Add(FString::Printf(
                TEXT("'%s' has connected input path(s): %s. Its local value is overridden. Set allow_connected_inputs=true only to acknowledge that the edit may not affect output"),
                *FString(Pair.Key),
                *FString::Join(ConnectedInputPaths, TEXT(", "))));
            continue;
        }
        void* ScratchValue = Property->ContainerPtrToValuePtr<void>(Scratch->GetStructMemory());
        const void* CurrentValue = Property->ContainerPtrToValuePtr<void>(Node.Get());
        Property->CopyCompleteValue(ScratchValue, CurrentValue);
        SDK(TEXT("validate_scratch_property_import"));Count(TEXT("property_import_calls"));
        const bool Imported=Property->ImportText_Direct(*ImportText, ScratchValue, nullptr, PPF_None)!=nullptr;
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (!Imported)
        {
            ValidationErrors.Add(FString::Printf(
                TEXT("Unable to import '%s' for %s.%s (%s)"),
                *ImportText,
                *NodeName,
                *FString(Pair.Key),
                *Property->GetCPPType()));
            continue;
        }
        const int32 ConstraintErrorCountBefore = ValidationErrors.Num();
        ValidateDataflowNumericConstraints(
            *Property,
            ScratchValue,
            FString::Printf(TEXT("property_values.%s"), *FString(Pair.Key)),
            ValidationErrors);
        if (ValidationErrors.Num() != ConstraintErrorCountBefore)
        {
            continue;
        }
        PropertyNames.Add(FString(Pair.Key));
        ValidatedProperties.Add(Property);
        FString CanonicalText;
        Property->ExportTextItem_Direct(
            CanonicalText,
            ScratchValue,
            nullptr,
            nullptr,
            PPF_None);
        CanonicalPropertyValues->SetStringField(FString(Pair.Key), CanonicalText);
    }

    const TSharedPtr<FJsonObject> BeforeProperties = MakeSelectedDataflowPropertySnapshot(Node, PropertyNames);
    TArray<FString> ExpectedMismatches;
    if (!CheckExpectedFields(ExpectedCurrent, BeforeProperties, ExpectedMismatches))
    {
        ValidationErrors.Append(ExpectedMismatches);
    }
    TArray<FString> NoOpMismatches;
    const bool bWouldChange = !CheckExpectedFields(
        CanonicalPropertyValues,
        BeforeProperties,
        NoOpMismatches);

    const FDataflowReferencerSnapshot Referencers =
        GetDataflowReferencers(Dataflow, Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (bWouldChange &&
        (!Referencers.OtherReferencerPackages.IsEmpty() ||
         !Referencers.OtherLoadedOwners.IsEmpty()))
    {
        TArray<FString> ReferencerEvidence;
        if (!Referencers.OtherOnDiskReferencerPackages.IsEmpty())
        {
            ReferencerEvidence.Add(FString::Printf(
                TEXT("on-disk package(s): %s"),
                *FString::Join(
                    Referencers.OtherOnDiskReferencerPackages,
                    TEXT(", "))));
        }
        if (!Referencers.OtherLoadedOwners.IsEmpty())
        {
            ReferencerEvidence.Add(FString::Printf(
                TEXT("loaded owner(s): %s"),
                *FString::Join(
                    DescribeLoadedDataflowOwnerReferences(Referencers.OtherLoadedOwners),
                    TEXT(", "))));
        }
        ValidationErrors.Add(FString::Printf(
            TEXT("Dataflow has other referencers (%s). Fan-out mutation is not supported because this route cannot regenerate and verify every owner"),
            *FString::Join(ReferencerEvidence, TEXT("; "))));
    }
    if (!ValidationErrors.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(FString::Join(ValidationErrors, TEXT("; ")));
    }

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    Before->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    Before->SetStringField(TEXT("node_name"), NodeName);
    Before->SetStringField(TEXT("node_type"), NodeType);
    Before->SetObjectField(TEXT("properties"), BeforeProperties);
    Before->SetObjectField(
        TEXT("connected_input_paths_by_property"),
        ConnectedInputPathsByProperty);
    Before->SetArrayField(
        TEXT("referencer_packages"),
        StringsToJson(Referencers.OnDiskReferencerPackages));
    Before->SetArrayField(
        TEXT("other_referencer_packages"),
        StringsToJson(Referencers.OtherReferencerPackages));
    Before->SetArrayField(
        TEXT("other_loaded_dataflow_owners"),
        LoadedDataflowOwnerReferencesToJson(Referencers.OtherLoadedOwners));

    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
        Result->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
        Result->SetStringField(TEXT("node_name"), NodeName);
        Result->SetStringField(TEXT("node_type"), NodeType);
        Result->SetObjectField(TEXT("requested_property_values"), *PropertyValuesPtr);
        Result->SetObjectField(TEXT("canonical_property_values"), CanonicalPropertyValues);
        Result->SetObjectField(
            TEXT("connected_input_paths_by_property"),
            ConnectedInputPathsByProperty);
        Result->SetBoolField(TEXT("regenerate_asset"), bRegenerateAsset);
        Result->SetBoolField(TEXT("would_change"), bWouldChange);
        Result->SetBoolField(TEXT("no_op"), !bWouldChange);
        Result->SetBoolField(TEXT("mutation_attempted"), false);
        Result->SetStringField(TEXT("transaction_scope"), TEXT("none"));
        Result->SetBoolField(TEXT("owned_transaction_cancelled"), false);
        Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
        Result->SetBoolField(TEXT("save_attempted"), false);
        Result->SetBoolField(TEXT("dataflow_saved"), false);
        Result->SetBoolField(TEXT("cloth_asset_saved"), false);
        Result->SetBoolField(TEXT("rollback_attempted"), false);
        Result->SetBoolField(TEXT("rollback_semantic_restore"), false);
        Result->SetBoolField(TEXT("rollback_transaction_retained"), false);
        Result->SetBoolField(TEXT("rollback_succeeded"), false);
        Result->SetBoolField(TEXT("partial_target_modified"), false);
        Result->SetBoolField(TEXT("do_not_retry"), false);
        Result->SetArrayField(
            TEXT("other_referencer_packages"),
            StringsToJson(Referencers.OtherReferencerPackages));
        Result->SetArrayField(
            TEXT("other_loaded_dataflow_owners"),
            LoadedDataflowOwnerReferencesToJson(Referencers.OtherLoadedOwners));
        FLegacy::AddMutationResultContract(
            Result,
            true,
            false,
            false,
            Before,
            Before,
            MakeDeltaCounts(false),
            true,
            TEXT("cloth_dataflow_node_dry_run_snapshot"),
            {TEXT("exact_asset"), TEXT("unique_node"), TEXT("node_type"), TEXT("property_import_validation"), TEXT("connected_input_gate"), TEXT("shared_dataflow_gate")});
        return Result;
    }

    if (!bWouldChange)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
        Result->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
        Result->SetStringField(TEXT("node_name"), NodeName);
        Result->SetStringField(TEXT("node_type"), NodeType);
        Result->SetObjectField(TEXT("requested_property_values"), *PropertyValuesPtr);
        Result->SetObjectField(TEXT("canonical_property_values"), CanonicalPropertyValues);
        Result->SetObjectField(
            TEXT("connected_input_paths_by_property"),
            ConnectedInputPathsByProperty);
        Result->SetBoolField(TEXT("cloth_asset_regenerated"), false);
        Result->SetBoolField(TEXT("would_change"), false);
        Result->SetBoolField(TEXT("no_op"), true);
        Result->SetBoolField(TEXT("mutation_attempted"), false);
        Result->SetStringField(TEXT("transaction_scope"), TEXT("none"));
        Result->SetBoolField(TEXT("owned_transaction_cancelled"), false);
        Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
        Result->SetBoolField(TEXT("save_attempted"), false);
        Result->SetBoolField(TEXT("dataflow_saved"), false);
        Result->SetBoolField(TEXT("cloth_asset_saved"), false);
        Result->SetBoolField(TEXT("rollback_attempted"), false);
        Result->SetBoolField(TEXT("rollback_regenerated"), false);
        Result->SetBoolField(TEXT("rollback_properties_verified"), false);
        Result->SetBoolField(TEXT("rollback_save_attempted"), false);
        Result->SetBoolField(TEXT("rollback_dataflow_saved"), false);
        Result->SetBoolField(TEXT("rollback_cloth_asset_saved"), false);
        Result->SetBoolField(TEXT("rollback_semantic_restore"), false);
        Result->SetBoolField(TEXT("rollback_transaction_retained"), false);
        Result->SetBoolField(TEXT("rollback_dataflow_package_dirty"), false);
        Result->SetBoolField(TEXT("rollback_cloth_package_dirty"), false);
        Result->SetBoolField(TEXT("rollback_succeeded"), false);
        Result->SetBoolField(TEXT("partial_target_modified"), false);
        Result->SetBoolField(TEXT("do_not_retry"), false);
        Result->SetArrayField(
            TEXT("other_referencer_packages"),
            StringsToJson(Referencers.OtherReferencerPackages));
        Result->SetArrayField(
            TEXT("other_loaded_dataflow_owners"),
            LoadedDataflowOwnerReferencesToJson(Referencers.OtherLoadedOwners));
        FLegacy::AddMutationResultContract(
            Result,
            false,
            false,
            false,
            Before,
            Before,
            MakeDeltaCounts(false),
            true,
            TEXT("cloth_dataflow_node_no_op_readback"),
            {TEXT("exact_asset"), TEXT("unique_node"), TEXT("node_type"), TEXT("property_import_validation"), TEXT("connected_input_gate"), TEXT("no_mutation_required")});
        AlignMutationAttemptContract(Result, false);
        return Result;
    }

    UPackage* const DataflowPackage = Dataflow->GetOutermost();
    UPackage* const ClothAssetPackage = Asset->GetOutermost();
    if (bSaveAssets &&
        (!IsDurablySavableObject(Dataflow) || !IsDurablySavableObject(Asset)))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("save_assets requires non-transient Dataflow and cloth assets in durably savable packages"));
    }
    TArray<FString> ImmediateReadbackErrors;
    if(!Guard()||!CheckExpectedFields(BeforeProperties,MakeSelectedDataflowPropertySnapshot(Node,PropertyNames),ImmediateReadbackErrors)) { return ErrorResponse(TEXT("Selected node properties changed during admission; no property write was started")); }
    if (bSaveAssets &&
        ((!DataflowPackage || DataflowPackage->IsDirty()) ||
         (!ClothAssetPackage || ClothAssetPackage->IsDirty())))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("save_assets requires clean Dataflow and cloth asset packages so unrelated pre-existing edits are not persisted"));
    }

    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    Work->Write(TEXT("begin_editor_transaction"));
    const bool bJoinedOuterTransaction = GEditor && GEditor->IsTransactionActive();
    TUniquePtr<FScopedTransaction> Transaction;
    if (!bJoinedOuterTransaction)
    {
        Transaction = MakeUnique<FScopedTransaction>(
            NSLOCTEXT("UnrealMCP", "ConfigureChaosClothDataflowNode", "Configure Chaos Cloth Dataflow Node"));
    }
    Work->Prefix->SetStringField(TEXT("transaction_scope"),Transaction?TEXT("owned"):TEXT("joined_outer"));
    if(!Guard()) { return ErrorResponse(Work->R.Error); }Count(TEXT("explicit_modify_calls"));Dataflow->Modify();Work->Prefix->SetBoolField(TEXT("Dataflow_Modify_returned"),true);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }Count(TEXT("explicit_modify_calls"));Asset->Modify();Work->Prefix->SetBoolField(TEXT("cloth_Modify_returned"),true);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }

    TArray<FString> ApplyErrors;
    {
    FDataflowAssetEdit MutationEdit=Dataflow->EditDataflow();
    for (int32 PropertyIndex = 0; PropertyIndex < ValidatedProperties.Num(); ++PropertyIndex)
    {
        FProperty* Property = ValidatedProperties[PropertyIndex];
        if (!Property || !PropertyNames.IsValidIndex(PropertyIndex))
        {
            ApplyErrors.Add(TEXT("A validated Dataflow property became unavailable before apply"));
            continue;
        }
        void* ValuePtr = Property->ContainerPtrToValuePtr<void>(Node.Get());
        const void* ScratchValue = Property->ContainerPtrToValuePtr<void>(Scratch->GetStructMemory());
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        Work->Write(TEXT("copy_Dataflow_property"));Count(TEXT("property_write_calls"));Property->CopyCompleteValue(ValuePtr, ScratchValue);
        Work->Prefix->SetNumberField(TEXT("property_write_prefix_count"),PropertyIndex+1);
        FPropertyChangedEvent PropertyEvent(Property, EPropertyChangeType::ValueSet);
        PropertyEvent.MemberProperty = Property;
        SDK(TEXT("property_invalidation_predicate"));Count(TEXT("property_invalidation_predicate_calls"));const bool ShouldInvalidate=Node->ShouldInvalidateOnPropertyChanged(PropertyEvent);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (ShouldInvalidate)
        {
            SDK(TEXT("invalidate_changed_Dataflow_property"));Count(TEXT("explicit_notify_calls"));Node->Invalidate();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
        UE::Dataflow::FEngineContext Context(Asset);
        SDK(TEXT("notify_Dataflow_property"));Count(TEXT("explicit_notify_calls"));Node->OnPropertyChanged(Context, PropertyEvent);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
    }
    }
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    SDK(TEXT("refresh_Dataflow_editor_node"));Count(TEXT("explicit_notify_calls"));Dataflow->RefreshEdNodeByGuid(Node->GetGuid());
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    Dataflow->MarkPackageDirty();
    if(!Guard()) { return ErrorResponse(Work->R.Error); }

    const bool bRegenerated = ApplyErrors.IsEmpty() &&
        Regenerate(Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (bRegenerated)
    {
        Asset->MarkPackageDirty();
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
    }
    int32 VerifyMatchCount = 0;
    TSharedPtr<FDataflowNode> VerifyNode;
    TSharedPtr<FJsonObject> AfterProperties;
    {
        const auto VerifyEdit = Dataflow->GetDataflow();
        VerifyNode = FindUniqueDataflowNode(
            VerifyEdit.Get(),
            NodeName,
            VerifyMatchCount);
        AfterProperties = MakeSelectedDataflowPropertySnapshot(VerifyNode, PropertyNames);
    }
    TArray<FString> ReadbackMismatches;
    const bool bPropertiesVerified = VerifyMatchCount == 1 &&
        CheckExpectedFields(CanonicalPropertyValues, AfterProperties, ReadbackMismatches);
    const bool bPreSaveVerified = ApplyErrors.IsEmpty() && bRegenerated && bPropertiesVerified;

    bool bSaveAttempted = false;
    bool bDataflowSaved = false;
    bool bClothAssetSaved = false;
    if (bSaveAssets && bPreSaveVerified)
    {
        bSaveAttempted = true;
        bDataflowSaved = SaveObjectIfRequested(Dataflow, true);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (bDataflowSaved)
        {
            bClothAssetSaved = SaveObjectIfRequested(Asset, true);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
    }
    const bool bSaveVerified = !bSaveAssets ||
        (bSaveAttempted && bDataflowSaved && bClothAssetSaved);
    const bool bVerified = bPreSaveVerified && bSaveVerified;

    bool bRollbackAttempted = false;
    bool bRollbackRegenerated = false;
    bool bRollbackPropertiesVerified = false;
    bool bRollbackSaveAttempted = false;
    bool bRollbackDataflowSaved = false;
    bool bRollbackClothAssetSaved = false;
    bool bRollbackSemanticRestore = false;
    bool bRollbackSaveVerified = false;
    bool bRollbackSucceeded = false;
    TArray<FString> RollbackErrors;
    if (!bVerified)
    {
        if(!Guard()) { return ErrorResponse(Work->R.Error); }Work->bCompensating=true;
        bRollbackAttempted = true;
        int32 RollbackMatchCount = 0;
        TSharedPtr<FDataflowNode> RollbackNode;
        {
            FDataflowAssetEdit RollbackMutationEdit=Dataflow->EditDataflow();
            const auto RollbackEdit = Dataflow->GetDataflow();
            RollbackNode = FindUniqueDataflowNode(
                RollbackEdit.Get(),
                NodeName,
                RollbackMatchCount);
            if (RollbackMatchCount != 1 || !RollbackNode.IsValid())
            {
                RollbackErrors.Add(FString::Printf(
                    TEXT("Rollback could not resolve the original node exactly (matched %d)"),
                    RollbackMatchCount));
            }
            else
            {
                const UScriptStruct* RollbackStruct = RollbackNode->TypedScriptStruct();
                for (const FString& PropertyName : PropertyNames)
                {
                    FString BeforeText;
                    FProperty* Property = FindEditableDataflowProperty(
                        RollbackStruct,
                        PropertyName);
                    if (!Property || !BeforeProperties->TryGetStringField(PropertyName, BeforeText))
                    {
                        RollbackErrors.Add(FString::Printf(
                            TEXT("Rollback metadata is unavailable for %s"),
                            *PropertyName));
                        continue;
                    }
                    void* ValuePtr = Property->ContainerPtrToValuePtr<void>(RollbackNode.Get());
                    if(!Guard()) { return ErrorResponse(Work->R.Error); }SDK(TEXT("restore_Dataflow_property"));Count(TEXT("property_import_calls"));
                    const bool Restored=Property->ImportText_Direct(*BeforeText, ValuePtr, nullptr, PPF_None)!=nullptr;
                    if(!Guard()) { return ErrorResponse(Work->R.Error); }
                    if (!Restored)
                    {
                        RollbackErrors.Add(FString::Printf(
                            TEXT("Rollback import failed for %s"),
                            *PropertyName));
                        continue;
                    }
                    FPropertyChangedEvent PropertyEvent(Property, EPropertyChangeType::ValueSet);
                    PropertyEvent.MemberProperty = Property;
                    SDK(TEXT("restore_property_invalidation_predicate"));Count(TEXT("property_invalidation_predicate_calls"));const bool ShouldInvalidate=RollbackNode->ShouldInvalidateOnPropertyChanged(PropertyEvent);
                    if(!Guard()) { return ErrorResponse(Work->R.Error); }
                    if (ShouldInvalidate)
                    {
                        SDK(TEXT("invalidate_restored_Dataflow_property"));Count(TEXT("explicit_notify_calls"));RollbackNode->Invalidate();
                        if(!Guard()) { return ErrorResponse(Work->R.Error); }
                    }
                    UE::Dataflow::FEngineContext Context(Asset);
                    SDK(TEXT("notify_restored_Dataflow_property"));Count(TEXT("explicit_notify_calls"));RollbackNode->OnPropertyChanged(Context, PropertyEvent);
                    if(!Guard()) { return ErrorResponse(Work->R.Error); }
                }
            }
        }

        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (RollbackNode.IsValid())
        {
            SDK(TEXT("refresh_Dataflow_editor_node"));Count(TEXT("explicit_notify_calls"));Dataflow->RefreshEdNodeByGuid(RollbackNode->GetGuid());
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Dataflow->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
        bRollbackRegenerated = RollbackErrors.IsEmpty() &&
            Regenerate(Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (bRollbackRegenerated)
        {
            Asset->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }

        int32 RollbackVerifyMatchCount = 0;
        TSharedPtr<FJsonObject> RolledBackProperties;
        {
            const auto RollbackVerifyEdit = Dataflow->GetDataflow();
            const TSharedPtr<FDataflowNode> RollbackVerifyNode = FindUniqueDataflowNode(
                RollbackVerifyEdit.Get(),
                NodeName,
                RollbackVerifyMatchCount);
            RolledBackProperties = MakeSelectedDataflowPropertySnapshot(
                RollbackVerifyNode,
                PropertyNames);
        }
        TArray<FString> RollbackReadbackMismatches;
        bRollbackPropertiesVerified = RollbackVerifyMatchCount == 1 &&
            CheckExpectedFields(
                BeforeProperties,
                RolledBackProperties,
                RollbackReadbackMismatches);
        RollbackErrors.Append(RollbackReadbackMismatches);
        AfterProperties = RolledBackProperties;
        bRollbackSemanticRestore = RollbackErrors.IsEmpty() &&
            bRollbackRegenerated && bRollbackPropertiesVerified;

        if (bSaveAttempted && bRollbackSemanticRestore)
        {
            bRollbackSaveAttempted = true;
            bRollbackDataflowSaved = SaveObjectIfRequested(Dataflow, true);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (bRollbackDataflowSaved)
            {
                bRollbackClothAssetSaved = SaveObjectIfRequested(Asset, true);
                if(!Guard()) { return ErrorResponse(Work->R.Error); }
            }
            if (!bRollbackDataflowSaved || !bRollbackClothAssetSaved)
            {
                RollbackErrors.Add(TEXT("Rollback package save failed"));
            }
        }
        bRollbackSaveVerified = !bSaveAttempted ||
            (bRollbackSaveAttempted && bRollbackDataflowSaved && bRollbackClothAssetSaved);
        bRollbackSucceeded = bRollbackSemanticRestore && bRollbackSaveVerified;
        if (bRollbackSucceeded && !bRollbackSaveAttempted)
        {
            // Regeneration can replace derived cloth data (including AssetGuid),
            // so a semantic property rollback is not a byte-exact package restore.
            // Keep both packages dirty and retain the transaction as the exact
            // manual undo path.
            Dataflow->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Asset->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
    }

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    After->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    After->SetStringField(TEXT("node_name"), NodeName);
    After->SetStringField(TEXT("node_type"), NodeType);
    After->SetObjectField(TEXT("properties"), AfterProperties);
    After->SetBoolField(TEXT("cloth_asset_regenerated"), bRegenerated);
    After->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    After->SetBoolField(TEXT("rollback_semantic_restore"), bRollbackSemanticRestore);
    After->SetBoolField(TEXT("rollback_save_verified"), bRollbackSaveVerified);
    After->SetBoolField(TEXT("rollback_transaction_retained"), bRollbackAttempted);
    After->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);

    const bool bPartialTargetModified = !bVerified && !bRollbackSucceeded;
    const bool bChanged = !bVerified ||
        !JsonObjectToString(BeforeProperties).Equals(JsonObjectToString(AfterProperties), ESearchCase::CaseSensitive);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bVerified);
    if (!bVerified)
    {
        TArray<FString> AllErrors = ApplyErrors;
        AllErrors.Append(ReadbackMismatches);
        if (!bRegenerated)
        {
            AllErrors.Add(TEXT("RegenerateAssetFromDataflow returned false"));
        }
        if (!bSaveVerified)
        {
            AllErrors.Add(TEXT("One or more requested asset saves failed"));
        }
        if (bRollbackSemanticRestore)
        {
            if (bRollbackSaveAttempted && bRollbackSaveVerified)
            {
                AllErrors.Add(TEXT("The failed mutation was semantically restored and the rollback assets were saved; the editor transaction is retained because regenerated derived state is not guaranteed to be byte-identical"));
            }
            else if (bSaveAttempted)
            {
                AllErrors.Add(TEXT("The failed mutation was restored in memory, but durable rollback saving did not complete; packages may remain partially persisted, so retain the editor transaction and do not retry"));
            }
            else
            {
                AllErrors.Add(TEXT("The failed mutation was semantically restored to its original selected node values and the cloth asset was regenerated; derived package state may differ, so the editor transaction is retained and unsaved packages remain dirty"));
            }
        }
        else if (bRollbackAttempted)
        {
            AllErrors.Append(RollbackErrors);
            AllErrors.Add(TEXT("Rollback did not complete; use the retained editor transaction/undo record before continuing"));
        }
        if (AllErrors.IsEmpty())
        {
            AllErrors.Add(TEXT("Chaos Cloth Dataflow mutation verification failed"));
        }
        Result->SetStringField(TEXT("error"), FString::Join(AllErrors, TEXT("; ")));
    }
    Result->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    Result->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    Result->SetStringField(TEXT("node_name"), NodeName);
    Result->SetBoolField(TEXT("no_op"), false);
    Result->SetBoolField(TEXT("mutation_attempted"), true);
    Result->SetStringField(
        TEXT("transaction_scope"),
        Transaction ? TEXT("owned") : TEXT("joined_outer"));
    Result->SetBoolField(TEXT("owned_transaction_cancelled"), false);
    Result->SetBoolField(TEXT("cloth_asset_regenerated"), bRegenerated);
    Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
    Result->SetBoolField(TEXT("save_attempted"), bSaveAttempted);
    Result->SetBoolField(TEXT("dataflow_saved"), bDataflowSaved);
    Result->SetBoolField(TEXT("cloth_asset_saved"), bClothAssetSaved);
    Result->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    Result->SetBoolField(TEXT("rollback_regenerated"), bRollbackRegenerated);
    Result->SetBoolField(TEXT("rollback_properties_verified"), bRollbackPropertiesVerified);
    Result->SetBoolField(TEXT("rollback_save_attempted"), bRollbackSaveAttempted);
    Result->SetBoolField(TEXT("rollback_dataflow_saved"), bRollbackDataflowSaved);
    Result->SetBoolField(TEXT("rollback_cloth_asset_saved"), bRollbackClothAssetSaved);
    Result->SetBoolField(TEXT("rollback_semantic_restore"), bRollbackSemanticRestore);
    Result->SetBoolField(TEXT("rollback_save_verified"), bRollbackSaveVerified);
    Result->SetBoolField(TEXT("rollback_transaction_retained"), bRollbackAttempted);
    Result->SetBoolField(
        TEXT("rollback_dataflow_package_dirty"),
        bRollbackAttempted && Dataflow->GetOutermost()->IsDirty());
    Result->SetBoolField(
        TEXT("rollback_cloth_package_dirty"),
        bRollbackAttempted && Asset->GetOutermost()->IsDirty());
    Result->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);
    Result->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
    Result->SetBoolField(TEXT("do_not_retry"), !bVerified);
    Result->SetObjectField(
        TEXT("connected_input_paths_by_property"),
        ConnectedInputPathsByProperty);
    Result->SetArrayField(
        TEXT("other_referencer_packages"),
        StringsToJson(Referencers.OtherReferencerPackages));
    Result->SetArrayField(
        TEXT("other_loaded_dataflow_owners"),
        LoadedDataflowOwnerReferencesToJson(Referencers.OtherLoadedOwners));
    FLegacy::AddMutationResultContract(
        Result,
        false,
        true,
        bChanged,
        Before,
        After,
        MakeDeltaCounts(bChanged, !bVerified),
        bVerified,
        TEXT("dataflow_graph_relookup_and_cloth_regeneration"),
        {TEXT("exact_asset"), TEXT("unique_node"), TEXT("node_type"), TEXT("connected_input_gate"), TEXT("shared_dataflow_gate"), TEXT("node_property_readback"), TEXT("cloth_regeneration"), TEXT("save_gate")});
    AlignMutationAttemptContract(Result, true);
    return Result;
}

TSharedPtr<FJsonObject> LegacyConfigureWeightMap(
    const TSharedPtr<FJsonObject>& Params)
{
    FJsonParamParseState ParamState;
    const FString AssetPath = GetStringParam(Params, TEXT("cloth_asset_path"), ParamState);
    const FString NodeName = GetStringParam(Params, TEXT("node_name"), ParamState);
    const FString WeightMapName = GetStringParam(Params, TEXT("weight_map_name"), ParamState);
    const int32 LodIndex = GetIntParam(Params, TEXT("lod_index"), 0, ParamState);
    const FString ExpectedNodeType = GetStringParam(
        Params,
        TEXT("expected_node_type"),
        ParamState);
    const FString ExpectedDataflowPath = GetStringParam(
        Params,
        TEXT("expected_dataflow_asset_path"),
        ParamState);
    const int32 ExpectedVertexCount = GetIntParam(
        Params,
        TEXT("expected_vertex_count"),
        -1,
        ParamState);
    const FString ExpectedTopologyMd5 = GetStringParam(
        Params,
        TEXT("expected_topology_md5"),
        ParamState);
    const FString ExpectedWeightMapMd5 = GetStringParam(
        Params,
        TEXT("expected_weight_map_md5"),
        ParamState);
    const FString ExpectedNodeStateMd5 = GetStringParam(
        Params,
        TEXT("expected_node_state_md5"),
        ParamState);
    const FString ExpectedPathMd5 = GetStringParam(
        Params,
        TEXT("expected_path_md5"),
        ParamState);
    const bool bRequireRuntimeBinding = GetBoolParam(
        Params,
        TEXT("require_runtime_binding"),
        true,
        ParamState);
    const bool bSaveAssets = GetBoolParam(
        Params,
        TEXT("save_assets"),
        false,
        ParamState);
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true, ParamState);
    Work->R.bSaveRequested=bSaveAssets;
    if (!ParamState.IsValid())
    {
        return FLegacy::CreateErrorResponse(ParamState.GetErrorMessage());
    }
    if (NodeName.IsEmpty() || WeightMapName.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("node_name and weight_map_name are required"));
    }
    if (LodIndex < 0 || ExpectedVertexCount < -1)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("lod_index must be non-negative and expected_vertex_count must be -1 or non-negative"));
    }
    if (!bDryRun && IsPlaySessionActive())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Durable Chaos Cloth weight-map edits are blocked while PIE/SIE is active"));
    }
    if (!bDryRun &&
        (ExpectedNodeType.IsEmpty() || ExpectedDataflowPath.IsEmpty() ||
         ExpectedVertexCount < 0 || ExpectedTopologyMd5.IsEmpty() ||
         ExpectedWeightMapMd5.IsEmpty() || ExpectedNodeStateMd5.IsEmpty() ||
         ExpectedPathMd5.IsEmpty()))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Apply mode requires expected_node_type, expected_dataflow_asset_path, expected_vertex_count, expected_topology_md5, expected_weight_map_md5, expected_node_state_md5, and expected_path_md5 from a fresh inspection/dry-run"));
    }

    const bool bHasWeights = Params.IsValid() && Params->HasField(TEXT("weights"));
    const bool bHasFillValue = Params.IsValid() && Params->HasField(TEXT("fill_value"));
    if (bHasWeights == bHasFillValue)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Supply exactly one value source: weights or fill_value"));
    }

    TArray<float> RequestedWeights;
    double RequestedFillValue = 0.0;
    if (bHasWeights)
    {
        const TArray<TSharedPtr<FJsonValue>>* JsonWeights = nullptr;
        if (!Params->TryGetArrayField(TEXT("weights"), JsonWeights) || !JsonWeights)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("weights must be a JSON array of normalized numbers"));
        }
        if (JsonWeights->IsEmpty() || JsonWeights->Num() > 1000000)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("weights must contain between 1 and 1000000 values"));
        }
        RequestedWeights.Reserve(JsonWeights->Num());
        for (int32 Index = 0; Index < JsonWeights->Num(); ++Index)
        {
            double Value = 0.0;
            if (!ReadNumber((*JsonWeights)[Index], Value) ||
                !IsFiniteFloatRange(Value) || Value < 0.0 || Value > 1.0)
            {
                return FLegacy::CreateErrorResponse(FString::Printf(
                    TEXT("weights[%d] must be a finite normalized number in [0,1]"),
                    Index));
            }
            RequestedWeights.Add(static_cast<float>(Value));
        }
    }
    else
    {
        const TSharedPtr<FJsonValue>* FillValue = FindJson(Params, TEXT("fill_value"));
        if (!FillValue || !ReadNumber(*FillValue, RequestedFillValue) ||
            !IsFiniteFloatRange(RequestedFillValue) ||
            RequestedFillValue < 0.0 || RequestedFillValue > 1.0)
        {
            return FLegacy::CreateErrorResponse(
                TEXT("fill_value must be a finite normalized number in [0,1]"));
        }
    }

    FString Error;
    UChaosClothAssetBase* const BaseAsset = LoadClothAsset(AssetPath, Error);
    UChaosClothAsset* const Asset = Cast<UChaosClothAsset>(BaseAsset);
    if (!BaseAsset)
    {
        return FLegacy::CreateErrorResponse(Error);
    }
    if (!Asset)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("configure_chaos_cloth_weight_map supports only the installed UChaosClothAsset type"));
    }
    SDK(TEXT("select_cloth_Dataflow"));Count(TEXT("Dataflow_getter_calls"));
    UDataflow* const Dataflow = Asset->GetDataflowInstance().GetDataflowAsset();
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if(Dataflow && Work && !Work->Capture(Dataflow)) { return ErrorResponse(Work->R.Error); }
    if (!Dataflow)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The selected cloth asset has no Dataflow asset; generated collection writes are intentionally not used because they are not regeneration-durable"));
    }
    if (FLegacy::IsMCPDependencyReference(Dataflow->GetPathName()))
    {
        return FLegacy::CreateErrorResponse(
            FLegacy::GetMCPDependencyReferenceError(
                TEXT("dataflow_asset_path"),
                Dataflow->GetPathName()));
    }
    if (!ExpectedDataflowPath.IsEmpty() && Dataflow->GetPathName() != ExpectedDataflowPath)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Dataflow path mismatch: expected '%s', actual '%s'"),
            *ExpectedDataflowPath,
            *Dataflow->GetPathName()));
    }

    FClothWeightMapAssetState BeforeAssetState;
    if (!ReadClothWeightMapAssetState(
            Asset,
            LodIndex,
            FName(*WeightMapName),
            BeforeAssetState,
            Error))
    {
        return FLegacy::CreateErrorResponse(Error);
    }
    if (!BeforeAssetState.bCollectionMapPresent ||
        BeforeAssetState.WeightCount != BeforeAssetState.VertexCount)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Weight map '%s' must already exist on the selected terminal LOD with one value per SimVertices3D vertex"),
            *WeightMapName));
    }
    if (bRequireRuntimeBinding && !BeforeAssetState.bRuntimeBound)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Weight map '%s' is not both referenced by a cloth property and copied into the compiled solver model; configure the consuming Dataflow config node first or explicitly set require_runtime_binding=false"),
            *WeightMapName));
    }

    int32 MatchCount = 0;
    TSharedPtr<FDataflowNode> Node;
    {
        const auto ReadEdit = Dataflow->GetDataflow();
        Node = FindUniqueDataflowNode(ReadEdit.Get(), NodeName, MatchCount);
    }
    if (MatchCount != 1 || !Node.IsValid())
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("WeightMap node name must resolve exactly once (matched %d)"),
            MatchCount));
    }
    if(!Work->Capture(Node)) { return ErrorResponse(Work->R.Error); }
    const FString NodeType = Node->GetType().ToString();
    const FString NodeStruct = Node->TypedScriptStruct()
        ? Node->TypedScriptStruct()->GetName()
        : FString();
    if (!ExpectedNodeType.IsEmpty() &&
        ExpectedNodeType != NodeType && ExpectedNodeType != NodeStruct)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Node type mismatch: expected '%s', actual type '%s' / struct '%s'"),
            *ExpectedNodeType,
            *NodeType,
            *NodeStruct));
    }
    if (!ValidateCurrentWeightMapNodeLayout(Node, Error))
    {
        return FLegacy::CreateErrorResponse(Error);
    }

    FClothWeightMapNodeState BeforeNodeState;
    if (!ReadClothWeightMapNodeState(Node, BeforeNodeState) ||
        !BeforeNodeState.bSupportedForWrite)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Unable to read the installed public snapshot-based WeightMap authoring state"));
    }
    if (BeforeNodeState.WeightMapName != WeightMapName)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("WeightMap node output name mismatch: requested '%s', node outputs '%s'"),
            *WeightMapName,
            *BeforeNodeState.WeightMapName));
    }
    if (!ExactDataflowEnumMatches(
            *Node,
            TEXT("MeshTarget"),
            TEXT("EChaosClothAssetWeightMapMeshTarget"),
            TEXT("Simulation")))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Only WeightMap nodes whose MeshTarget is exactly Simulation are supported"));
    }

    int32 InputVertexCount = 0;
    FString InputTopologyMd5;
    if (!EvaluateWeightMapNodeInputTopology(
            Node,
            Asset,
            InputVertexCount,
            InputTopologyMd5))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Unable to evaluate the WeightMap node input cloth collection"));
    }
    if (InputVertexCount != BeforeAssetState.VertexCount ||
        InputTopologyMd5 != BeforeAssetState.TopologyMd5)
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The WeightMap node input topology differs from the selected terminal LOD. This guarded route will not guess a remesh/LOD mapping"));
    }
    if (ExpectedVertexCount >= 0 && ExpectedVertexCount != InputVertexCount)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Vertex-count concurrency mismatch: expected %d, actual %d"),
            ExpectedVertexCount,
            InputVertexCount));
    }
    if (!ExpectedTopologyMd5.IsEmpty() &&
        !ExpectedTopologyMd5.Equals(InputTopologyMd5, ESearchCase::IgnoreCase))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Topology concurrency mismatch; inspect the WeightMap node and LOD again"));
    }
    if (!ExpectedWeightMapMd5.IsEmpty() &&
        !ExpectedWeightMapMd5.Equals(
            BeforeAssetState.WeightMapMd5,
            ESearchCase::IgnoreCase))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Generated weight-map concurrency mismatch; inspect the LOD again"));
    }
    if (!ExpectedNodeStateMd5.IsEmpty() &&
        !ExpectedNodeStateMd5.Equals(
            BeforeNodeState.StateMd5,
            ESearchCase::IgnoreCase))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("WeightMap node-state concurrency mismatch; inspect the node again"));
    }

    if (bHasFillValue)
    {
        RequestedWeights.Init(static_cast<float>(RequestedFillValue), InputVertexCount);
    }
    if (RequestedWeights.Num() != InputVertexCount)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("Requested weight count %d must exactly equal WeightMap node input SimVertices3D count %d"),
            RequestedWeights.Num(),
            InputVertexCount));
    }
    const FString RequestedWeightsMd5 = HashFloatValues(RequestedWeights);
    const bool bOverrideAlreadyReplaceAll = ExactDataflowEnumMatches(
        *Node,
        TEXT("MapOverrideType"),
        TEXT("EChaosClothAssetWeightMapOverrideType"),
        TEXT("ReplaceAll"));
    const bool bWouldChange =
        BeforeNodeState.StoredWeights.Num() != RequestedWeights.Num() ||
        BeforeNodeState.StoredWeightsMd5 != RequestedWeightsMd5 ||
        !bOverrideAlreadyReplaceAll ||
        BeforeAssetState.WeightMapMd5 != RequestedWeightsMd5;

    FWeightMapDownstreamPath BeforeDownstreamPath;
    FString BeforeDownstreamPathError;
    const bool bBeforeDownstreamPathValid = ResolveSingleWeightMapDownstreamPath(
        Node,
        Asset,
        Asset->GetDataflowInstance().GetDataflowTerminal(),
        LodIndex,
        BeforeDownstreamPath,
        BeforeDownstreamPathError);
    if (!bDryRun && bBeforeDownstreamPathValid &&
        !ExpectedPathMd5.Equals(
            BeforeDownstreamPath.PathMd5,
            ESearchCase::IgnoreCase))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Downstream path concurrency mismatch; run configure_chaos_cloth_weight_map in dry-run mode again and use its path_md5"));
    }
    FString BeforeDerivedDataPathMode;
    FString BeforeDerivedDataPathError;
    const bool bBeforeDerivedDataPathValid = bBeforeDownstreamPathValid &&
        ValidateWeightMapDerivedDataPath(
            BeforeAssetState,
            BeforeDownstreamPath,
            BeforeDerivedDataPathMode,
            BeforeDerivedDataPathError);
    FString BeforeTetherDerivedStateError;
    const bool bBeforeTetherDerivedStateVerified =
        VerifyWeightMapTetherAndSelectionDerivedState(
            BeforeAssetState,
            BeforeTetherDerivedStateError);
    const bool bBeforeDerivedDataVerified =
        bBeforeDerivedDataPathValid &&
        bBeforeTetherDerivedStateVerified;

    TSharedPtr<FJsonObject> Before = MakeShared<FJsonObject>();
    Before->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    Before->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    Before->SetObjectField(
        TEXT("asset_weight_map"),
        MakeClothWeightMapAssetStateSnapshot(BeforeAssetState));
    Before->SetObjectField(
        TEXT("weight_map_node"),
        MakeClothWeightMapNodeStateSnapshot(BeforeNodeState));
    Before->SetObjectField(
        TEXT("downstream_path"),
        MakeWeightMapDownstreamPathSnapshot(
            BeforeDownstreamPath,
            bBeforeDownstreamPathValid,
            BeforeDownstreamPathError));
    Before->SetStringField(
        TEXT("derived_data_path_mode"),
        BeforeDerivedDataPathMode);
    Before->SetBoolField(
        TEXT("derived_data_path_verified"),
        bBeforeDerivedDataPathValid);
    Before->SetStringField(
        TEXT("derived_data_path_error"),
        BeforeDerivedDataPathError);
    Before->SetBoolField(
        TEXT("tether_derived_state_verified"),
        bBeforeTetherDerivedStateVerified);
    Before->SetStringField(
        TEXT("tether_derived_state_error"),
        BeforeTetherDerivedStateError);
    Before->SetNumberField(TEXT("node_input_vertex_count"), InputVertexCount);
    Before->SetStringField(TEXT("node_input_topology_md5"), InputTopologyMd5);

    if (bDryRun || (!bWouldChange && bBeforeDerivedDataVerified))
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
        Result->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
        Result->SetStringField(TEXT("node_name"), NodeName);
        Result->SetStringField(TEXT("weight_map_name"), WeightMapName);
        Result->SetStringField(TEXT("requested_weights_md5"), RequestedWeightsMd5);
        Result->SetStringField(TEXT("path_md5"), BeforeDownstreamPath.PathMd5);
        Result->SetNumberField(TEXT("requested_weight_count"), RequestedWeights.Num());
        Result->SetStringField(
            TEXT("value_source"),
            bHasFillValue ? TEXT("fill_value") : TEXT("explicit_weights"));
        if (bHasFillValue)
        {
            Result->SetNumberField(TEXT("fill_value"), RequestedFillValue);
        }
        Result->SetBoolField(TEXT("would_change"), bWouldChange);
        Result->SetBoolField(TEXT("no_op"), !bWouldChange);
        Result->SetBoolField(
            TEXT("downstream_path_verified"),
            bBeforeDownstreamPathValid);
        Result->SetStringField(
            TEXT("downstream_path_error"),
            BeforeDownstreamPathError);
        Result->SetStringField(
            TEXT("derived_data_path_mode"),
            BeforeDerivedDataPathMode);
        Result->SetBoolField(
            TEXT("derived_data_path_verified"),
            bBeforeDerivedDataPathValid);
        Result->SetStringField(
            TEXT("derived_data_path_error"),
            BeforeDerivedDataPathError);
        Result->SetBoolField(
            TEXT("tether_derived_state_verified"),
            bBeforeTetherDerivedStateVerified);
        Result->SetStringField(
            TEXT("tether_derived_state_error"),
            BeforeTetherDerivedStateError);
        Result->SetBoolField(
            TEXT("apply_blocked"),
            bDryRun && (!bBeforeDownstreamPathValid ||
                        !bBeforeDerivedDataVerified));
        Result->SetBoolField(TEXT("mutation_attempted"), false);
        Result->SetStringField(TEXT("transaction_scope"), TEXT("none"));
        Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
        Result->SetBoolField(TEXT("save_attempted"), false);
        Result->SetBoolField(TEXT("dataflow_saved"), false);
        Result->SetBoolField(TEXT("cloth_asset_saved"), false);
        Result->SetBoolField(TEXT("rollback_attempted"), false);
        Result->SetBoolField(TEXT("rollback_succeeded"), false);
        Result->SetBoolField(TEXT("partial_target_modified"), false);
        Result->SetBoolField(TEXT("do_not_retry"), false);
        const bool bContractDryRun = bDryRun;
        FLegacy::AddMutationResultContract(
            Result,
            bContractDryRun,
            false,
            false,
            Before,
            Before,
            MakeDeltaCounts(false),
            true,
            bDryRun
                ? TEXT("weight_map_node_dry_run_snapshot")
                : TEXT("weight_map_node_no_op_readback"),
            {TEXT("exact_asset"), TEXT("unique_current_weight_map_node"), TEXT("single_selected_terminal_lod_path_md5"), TEXT("active_unfrozen_tether_and_selection_generator_path"), TEXT("collection_compiled_canonical_tether_hash"), TEXT("collection_compiled_kinematic_selection_hash"), TEXT("node_input_topology"), TEXT("terminal_topology"), TEXT("weight_count"), TEXT("normalized_values"), TEXT("runtime_binding_gate")});
        if (!bDryRun)
        {
            AlignMutationAttemptContract(Result, false);
        }
        return Result;
    }

    if (!bBeforeDownstreamPathValid)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("WeightMap mutation is blocked because its Collection output is not an exclusive path to the selected cloth terminal LOD: %s"),
            *BeforeDownstreamPathError));
    }
    if (!bBeforeDerivedDataPathValid)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("WeightMap mutation is blocked because tether-affecting derived data is not regenerated on the selected downstream path: %s"),
            *BeforeDerivedDataPathError));
    }
    if (!bBeforeTetherDerivedStateVerified)
    {
        return FLegacy::CreateErrorResponse(FString::Printf(
            TEXT("WeightMap mutation is blocked because the current collection/compiled tether or KinematicVertices3D selection state is not coherent: %s"),
            *BeforeTetherDerivedStateError));
    }

    const FDataflowReferencerSnapshot Referencers =
        GetDataflowReferencers(Dataflow, Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (!Referencers.OtherReferencerPackages.IsEmpty() ||
        !Referencers.OtherLoadedOwners.IsEmpty())
    {
        return FLegacy::CreateErrorResponse(
            TEXT("The Dataflow has other on-disk or loaded owners. Fan-out WeightMap mutation is not supported"));
    }

    UPackage* const DataflowPackage = Dataflow->GetOutermost();
    UPackage* const ClothPackage = Asset->GetOutermost();
    if (bSaveAssets &&
        (!IsDurablySavableObject(Dataflow) || !IsDurablySavableObject(Asset)))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("save_assets requires non-transient Dataflow and cloth assets in durably savable packages"));
    }
    if (bSaveAssets &&
        ((!DataflowPackage || DataflowPackage->IsDirty()) ||
         (!ClothPackage || ClothPackage->IsDirty())))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("save_assets requires clean Dataflow and cloth packages so unrelated pre-existing edits are not persisted"));
    }

    FClothWeightMapNodeState ImmediateNodeState;FClothWeightMapAssetState ImmediateAssetState;FWeightMapDownstreamPath ImmediatePath;FString ImmediateError;
    if(!Guard()||!ReadClothWeightMapNodeState(Node,ImmediateNodeState)||!ImmediateNodeState.StateMd5.Equals(BeforeNodeState.StateMd5,ESearchCase::CaseSensitive)||!ReadClothWeightMapAssetState(Asset,LodIndex,FName(*WeightMapName),ImmediateAssetState,ImmediateError)||!JsonObjectToString(MakeClothWeightMapAssetStateSnapshot(ImmediateAssetState)).Equals(JsonObjectToString(MakeClothWeightMapAssetStateSnapshot(BeforeAssetState)),ESearchCase::CaseSensitive)||!ResolveSingleWeightMapDownstreamPath(Node,Asset,Asset->GetDataflowInstance().GetDataflowTerminal(),LodIndex,ImmediatePath,ImmediateError)||!ImmediatePath.PathMd5.Equals(BeforeDownstreamPath.PathMd5,ESearchCase::CaseSensitive)) { return ErrorResponse(TEXT("WeightMap node, terminal state or path changed during admission; no writer call was started")); }
    if(bSaveAssets&&(DataflowPackage->IsDirty()||ClothPackage->IsDirty())) { return ErrorResponse(TEXT("Exact-save package became dirty during final admission")); }
    int64 OriginalOverrideValue = INDEX_NONE;
    if (!ReadExactDataflowEnumValue(
            *Node,
            TEXT("MapOverrideType"),
            TEXT("EChaosClothAssetWeightMapOverrideType"),
            OriginalOverrideValue))
    {
        return FLegacy::CreateErrorResponse(
            TEXT("Unable to snapshot the original WeightMap override enum"));
    }

    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    SDK(TEXT("capture_public_snapshot_restore_change"));
    auto* EditableNode=static_cast<FDataflowVertexAttributeEditableNode*>(Node.Get());
    TUniquePtr<FToolCommandChange> SnapshotRestore=EditableNode->MakeEditNodeToolChange();
    if(!SnapshotRestore||!Guard()) { return ErrorResponse(TEXT("Unable to capture the public snapshot semantic restore change")); }
    Work->Write(TEXT("begin_editor_transaction"));
    const bool bJoinedOuterTransaction = GEditor && GEditor->IsTransactionActive();
    TUniquePtr<FScopedTransaction> Transaction;
    if (!bJoinedOuterTransaction)
    {
        Transaction = MakeUnique<FScopedTransaction>(
            NSLOCTEXT("CubelessMCP", "ConfigureChaosClothWeightMap", "Configure Chaos Cloth Weight Map"));
    }
    Work->Prefix->SetStringField(TEXT("transaction_scope"),Transaction?TEXT("owned"):TEXT("joined_outer"));
    if(!Guard()) { return ErrorResponse(Work->R.Error); }Count(TEXT("explicit_modify_calls"));Dataflow->Modify();Work->Prefix->SetBoolField(TEXT("Dataflow_Modify_returned"),true);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }Count(TEXT("explicit_modify_calls"));Asset->Modify();Work->Prefix->SetBoolField(TEXT("cloth_Modify_returned"),true);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }

    TArray<FString> ApplyErrors;
    {
    FDataflowAssetEdit MutationEdit=Dataflow->EditDataflow();
    if (!SetWeightMapNodeReplaceAll(*Node, RequestedWeights))
    {
        ApplyErrors.Add(TEXT("Public snapshot setter failed to establish the requested ReplaceAll state"));
    }
    NotifyWeightMapNodeChanged(*Node);
    }
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    SDK(TEXT("refresh_Dataflow_editor_node"));Count(TEXT("explicit_notify_calls"));Dataflow->RefreshEdNodeByGuid(Node->GetGuid());
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    Dataflow->MarkPackageDirty();
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    const bool bRegenerated = ApplyErrors.IsEmpty() &&
        Regenerate(Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
    if (bRegenerated)
    {
        Asset->MarkPackageDirty();
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
    }

    int32 VerifyMatchCount = 0;
    TSharedPtr<FDataflowNode> VerifyNode;
    {
        const auto VerifyEdit = Dataflow->GetDataflow();
        VerifyNode = FindUniqueDataflowNode(
            VerifyEdit.Get(),
            NodeName,
            VerifyMatchCount);
    }
    FClothWeightMapNodeState AfterNodeState;
    const bool bAfterNodeReadable = VerifyMatchCount == 1 &&
        ReadClothWeightMapNodeState(VerifyNode, AfterNodeState);
    FWeightMapDownstreamPath AfterDownstreamPath;
    FString AfterDownstreamPathError;
    const bool bAfterDownstreamPathValid = bAfterNodeReadable &&
        ResolveSingleWeightMapDownstreamPath(
            VerifyNode,
            Asset,
            Asset->GetDataflowInstance().GetDataflowTerminal(),
            LodIndex,
            AfterDownstreamPath,
            AfterDownstreamPathError);
    const bool bDownstreamPathVerified = bAfterDownstreamPathValid &&
        AfterDownstreamPath.PathMd5 == BeforeDownstreamPath.PathMd5;
    int32 AfterInputVertexCount = 0;
    FString AfterInputTopologyMd5;
    const bool bAfterInputValid = bAfterNodeReadable &&
        EvaluateWeightMapNodeInputTopology(
            VerifyNode,
            Asset,
            AfterInputVertexCount,
            AfterInputTopologyMd5);
    FClothWeightMapAssetState AfterAssetState;
    FString AfterStateError;
    const bool bAfterAssetReadable = bRegenerated && ReadClothWeightMapAssetState(
        Asset,
        LodIndex,
        FName(*WeightMapName),
        AfterAssetState,
        AfterStateError);
    FString AfterDerivedDataPathMode;
    FString AfterDerivedDataPathError;
    const bool bAfterDerivedDataPathValid = bAfterAssetReadable &&
        bAfterDownstreamPathValid &&
        ValidateWeightMapDerivedDataPath(
            AfterAssetState,
            AfterDownstreamPath,
            AfterDerivedDataPathMode,
            AfterDerivedDataPathError);
    const bool bDerivedDataPathVerified = bAfterDerivedDataPathValid &&
        AfterAssetState.bRequiresMaxDistanceDerivedData ==
            BeforeAssetState.bRequiresMaxDistanceDerivedData &&
        AfterAssetState.bRequiresFixedEndWeightMapDerivedData ==
            BeforeAssetState.bRequiresFixedEndWeightMapDerivedData &&
        AfterAssetState.bRequiresKinematicSelectionDerivedData ==
            BeforeAssetState.bRequiresKinematicSelectionDerivedData &&
        AfterDerivedDataPathMode == BeforeDerivedDataPathMode;
    FString AfterTetherDerivedStateError;
    const bool bTetherDerivedStateVerified = bAfterAssetReadable &&
        VerifyWeightMapTetherAndSelectionDerivedState(
            AfterAssetState,
            AfterTetherDerivedStateError);

    const bool bNodeVerified = bAfterNodeReadable &&
        AfterNodeState.WeightMapName == WeightMapName &&
        AfterNodeState.StoredWeights.Num() == RequestedWeights.Num() &&
        AfterNodeState.StoredWeightsMd5 == RequestedWeightsMd5 &&
        ExactDataflowEnumMatches(
            *VerifyNode,
            TEXT("MapOverrideType"),
            TEXT("EChaosClothAssetWeightMapOverrideType"),
            TEXT("ReplaceAll")) &&
        ExactDataflowEnumMatches(
            *VerifyNode,
            TEXT("MeshTarget"),
            TEXT("EChaosClothAssetWeightMapMeshTarget"),
            TEXT("Simulation"));
    const bool bTopologyVerified = bAfterInputValid &&
        AfterInputVertexCount == InputVertexCount &&
        AfterInputTopologyMd5 == InputTopologyMd5 &&
        bAfterAssetReadable &&
        AfterAssetState.VertexCount == BeforeAssetState.VertexCount &&
        AfterAssetState.TopologyMd5 == BeforeAssetState.TopologyMd5;
    const bool bMapVerified = bAfterAssetReadable &&
        AfterAssetState.bCollectionMapPresent &&
        AfterAssetState.WeightCount == RequestedWeights.Num() &&
        AfterAssetState.WeightMapMd5 == RequestedWeightsMd5;
    const bool bCompiledMapVerified = bAfterAssetReadable &&
        (!AfterAssetState.bCompiledMapPresent ||
         (AfterAssetState.CompiledWeightCount == RequestedWeights.Num() &&
          AfterAssetState.CompiledWeightMapMd5 == RequestedWeightsMd5));
    const bool bRuntimeBindingVerified =
        !bRequireRuntimeBinding ||
        (bAfterAssetReadable && AfterAssetState.bRuntimeBound);
    const bool bPreSaveVerified = ApplyErrors.IsEmpty() && bRegenerated &&
        bNodeVerified && bDownstreamPathVerified &&
        bDerivedDataPathVerified && bTetherDerivedStateVerified &&
        bTopologyVerified && bMapVerified && bCompiledMapVerified &&
        bRuntimeBindingVerified &&
        Asset->HasValidClothSimulationModels();

    bool bSaveAttempted = false;
    bool bDataflowSaved = false;
    bool bClothAssetSaved = false;
    if (bSaveAssets && bPreSaveVerified)
    {
        bSaveAttempted = true;
        bDataflowSaved = SaveObjectIfRequested(Dataflow, true);
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (bDataflowSaved)
        {
            bClothAssetSaved = SaveObjectIfRequested(Asset, true);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
    }
    const bool bSaveVerified = !bSaveAssets ||
        (bSaveAttempted && bDataflowSaved && bClothAssetSaved);
    const bool bVerified = bPreSaveVerified && bSaveVerified;

    bool bRollbackAttempted = false;
    bool bRollbackRegenerated = false;
    bool bRollbackNodeVerified = false;
    bool bRollbackDownstreamPathVerified = false;
    bool bRollbackDerivedDataPathVerified = false;
    bool bRollbackTetherDerivedStateVerified = false;
    bool bRollbackAssetVerified = false;
    bool bRollbackSaveAttempted = false;
    bool bRollbackDataflowSaved = false;
    bool bRollbackClothAssetSaved = false;
    bool bRollbackSemanticRestore = false;
    bool bRollbackSucceeded = false;
    TArray<FString> RollbackErrors;
    if (!bVerified)
    {
        if(!Guard()) { return ErrorResponse(Work->R.Error); }Work->bCompensating=true;
        bRollbackAttempted = true;
        int32 RollbackMatchCount = 0;
        TSharedPtr<FDataflowNode> RollbackNode;
        {
            const auto RollbackEdit = Dataflow->GetDataflow();
            RollbackNode = FindUniqueDataflowNode(
                RollbackEdit.Get(),
                NodeName,
                RollbackMatchCount);
        }
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (RollbackMatchCount != 1 || !RollbackNode.IsValid() ||
            !ValidateCurrentWeightMapNodeLayout(RollbackNode, Error))
        {
            RollbackErrors.Add(TEXT("Rollback could not resolve the original current WeightMap node exactly"));
        }
        else
        {
            if(!Guard()||RollbackNode!=Node||!SnapshotRestore) { return ErrorResponse(TEXT("Snapshot restore target identity is unavailable")); }
            FDataflowAssetEdit RestoreMutationEdit=Dataflow->EditDataflow();
            SDK(TEXT("revert_public_snapshot_change"));Count(TEXT("snapshot_restore_calls"));SnapshotRestore->Revert(Dataflow);
            Work->Prefix->SetBoolField(TEXT("public_snapshot_Revert_returned"),true);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (!WriteExactDataflowEnumRawValue(
                    *RollbackNode,
                    TEXT("MapOverrideType"),
                    TEXT("EChaosClothAssetWeightMapOverrideType"),
                    OriginalOverrideValue))
            {
                RollbackErrors.Add(TEXT("Rollback failed to restore MapOverrideType"));
            }
            NotifyWeightMapNodeChanged(*RollbackNode);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            SDK(TEXT("refresh_Dataflow_editor_node"));Count(TEXT("explicit_notify_calls"));Dataflow->RefreshEdNodeByGuid(RollbackNode->GetGuid());
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Dataflow->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
        if(!Guard()) { return ErrorResponse(Work->R.Error); }
        bRollbackRegenerated = RollbackErrors.IsEmpty() &&
            Regenerate(Asset);
    if(!Guard()) { return ErrorResponse(Work->R.Error); }
        if (bRollbackRegenerated)
        {
            Asset->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }

        int32 RollbackVerifyMatchCount = 0;
        TSharedPtr<FDataflowNode> RollbackVerifyNode;
        {
            const auto RollbackVerifyEdit = Dataflow->GetDataflow();
            RollbackVerifyNode = FindUniqueDataflowNode(
                RollbackVerifyEdit.Get(),
                NodeName,
                RollbackVerifyMatchCount);
        }
        FClothWeightMapNodeState RolledBackNodeState;
        bRollbackNodeVerified = RollbackVerifyMatchCount == 1 &&
            ReadClothWeightMapNodeState(RollbackVerifyNode, RolledBackNodeState) &&
            RolledBackNodeState.StateMd5.Equals(BeforeNodeState.StateMd5,ESearchCase::CaseSensitive);
        FWeightMapDownstreamPath RolledBackDownstreamPath;
        FString RolledBackDownstreamPathError;
        const bool bRolledBackDownstreamPathValid = bRollbackNodeVerified &&
            ResolveSingleWeightMapDownstreamPath(
                RollbackVerifyNode,
                Asset,
                Asset->GetDataflowInstance().GetDataflowTerminal(),
                LodIndex,
                RolledBackDownstreamPath,
                RolledBackDownstreamPathError);
        bRollbackDownstreamPathVerified = bRolledBackDownstreamPathValid &&
            RolledBackDownstreamPath.PathMd5 ==
                BeforeDownstreamPath.PathMd5;
        if (!bRollbackDownstreamPathVerified)
        {
            RollbackErrors.Add(FString::Printf(
                TEXT("Rollback downstream path verification failed: %s"),
                *RolledBackDownstreamPathError));
        }
        FClothWeightMapAssetState RolledBackAssetState;
        FString RollbackStateError;
        const bool bRolledBackAssetReadable = bRollbackRegenerated &&
            ReadClothWeightMapAssetState(
                Asset,
                LodIndex,
                FName(*WeightMapName),
                RolledBackAssetState,
                RollbackStateError);
        FString RolledBackDerivedDataPathMode;
        FString RolledBackDerivedDataPathError;
        const bool bRolledBackDerivedDataPathValid =
            bRolledBackAssetReadable &&
            bRollbackDownstreamPathVerified &&
            ValidateWeightMapDerivedDataPath(
                RolledBackAssetState,
                RolledBackDownstreamPath,
                RolledBackDerivedDataPathMode,
                RolledBackDerivedDataPathError);
        bRollbackDerivedDataPathVerified =
            bRolledBackDerivedDataPathValid &&
            RolledBackAssetState.bRequiresMaxDistanceDerivedData ==
                BeforeAssetState.bRequiresMaxDistanceDerivedData &&
            RolledBackAssetState.bRequiresFixedEndWeightMapDerivedData ==
                BeforeAssetState.bRequiresFixedEndWeightMapDerivedData &&
            RolledBackAssetState.bRequiresKinematicSelectionDerivedData ==
                BeforeAssetState.bRequiresKinematicSelectionDerivedData &&
            RolledBackDerivedDataPathMode == BeforeDerivedDataPathMode;
        FString RolledBackTetherDerivedStateError;
        bRollbackTetherDerivedStateVerified = bRolledBackAssetReadable &&
            VerifyWeightMapTetherAndSelectionDerivedState(
                RolledBackAssetState,
                RolledBackTetherDerivedStateError);
        if (!bRollbackDerivedDataPathVerified)
        {
            RollbackErrors.Add(FString::Printf(
                TEXT("Rollback derived-data path verification failed: %s"),
                *RolledBackDerivedDataPathError));
        }
        if (!bRollbackTetherDerivedStateVerified)
        {
            RollbackErrors.Add(FString::Printf(
                TEXT("Rollback tether-derived-state verification failed: %s"),
                *RolledBackTetherDerivedStateError));
        }
        bRollbackAssetVerified = bRolledBackAssetReadable &&
            RolledBackAssetState.VertexCount == BeforeAssetState.VertexCount &&
            RolledBackAssetState.TopologyMd5 == BeforeAssetState.TopologyMd5 &&
            RolledBackAssetState.WeightCount == BeforeAssetState.WeightCount &&
            RolledBackAssetState.WeightMapMd5 == BeforeAssetState.WeightMapMd5 &&
            RolledBackAssetState.bCompiledMapPresent == BeforeAssetState.bCompiledMapPresent &&
            RolledBackAssetState.CompiledWeightCount == BeforeAssetState.CompiledWeightCount &&
            RolledBackAssetState.CompiledWeightMapMd5 == BeforeAssetState.CompiledWeightMapMd5 &&
            RolledBackAssetState.CollectionTethers.bValid ==
                BeforeAssetState.CollectionTethers.bValid &&
            RolledBackAssetState.CollectionTethers.TetherCount ==
                BeforeAssetState.CollectionTethers.TetherCount &&
            RolledBackAssetState.CollectionTethers.TetherMd5 ==
                BeforeAssetState.CollectionTethers.TetherMd5 &&
            RolledBackAssetState.CompiledTethers.bValid ==
                BeforeAssetState.CompiledTethers.bValid &&
            RolledBackAssetState.CompiledTethers.TetherCount ==
                BeforeAssetState.CompiledTethers.TetherCount &&
            RolledBackAssetState.CompiledTethers.TetherMd5 ==
                BeforeAssetState.CompiledTethers.TetherMd5 &&
            RolledBackAssetState.CollectionKinematicSelection.bPresent ==
                BeforeAssetState.CollectionKinematicSelection.bPresent &&
            RolledBackAssetState.CollectionKinematicSelection.bValid ==
                BeforeAssetState.CollectionKinematicSelection.bValid &&
            RolledBackAssetState.CollectionKinematicSelection.SelectionName ==
                BeforeAssetState.CollectionKinematicSelection.SelectionName &&
            RolledBackAssetState.CollectionKinematicSelection.SelectionGroup ==
                BeforeAssetState.CollectionKinematicSelection.SelectionGroup &&
            RolledBackAssetState.CollectionKinematicSelection.SelectionCount ==
                BeforeAssetState.CollectionKinematicSelection.SelectionCount &&
            RolledBackAssetState.CollectionKinematicSelection.SelectionMd5 ==
                BeforeAssetState.CollectionKinematicSelection.SelectionMd5 &&
            RolledBackAssetState.CompiledKinematicSelection.bPresent ==
                BeforeAssetState.CompiledKinematicSelection.bPresent &&
            RolledBackAssetState.CompiledKinematicSelection.bValid ==
                BeforeAssetState.CompiledKinematicSelection.bValid &&
            RolledBackAssetState.CompiledKinematicSelection.SelectionName ==
                BeforeAssetState.CompiledKinematicSelection.SelectionName &&
            RolledBackAssetState.CompiledKinematicSelection.SelectionCount ==
                BeforeAssetState.CompiledKinematicSelection.SelectionCount &&
            RolledBackAssetState.CompiledKinematicSelection.SelectionMd5 ==
                BeforeAssetState.CompiledKinematicSelection.SelectionMd5 &&
            bRollbackDerivedDataPathVerified &&
            bRollbackTetherDerivedStateVerified;
        auto Compensation=MakeShared<FJsonObject>();Compensation->SetObjectField(TEXT("node"),MakeClothWeightMapNodeStateSnapshot(RolledBackNodeState));Compensation->SetObjectField(TEXT("asset_weight_map"),MakeClothWeightMapAssetStateSnapshot(RolledBackAssetState));Compensation->SetBoolField(TEXT("node_verified"),bRollbackNodeVerified);Compensation->SetBoolField(TEXT("asset_verified"),bRollbackAssetVerified);Work->R.Data->SetObjectField(TEXT("compensation_readback"),Compensation);
        bRollbackSemanticRestore = RollbackErrors.IsEmpty() &&
            bRollbackRegenerated && bRollbackNodeVerified &&
            bRollbackDownstreamPathVerified && bRollbackAssetVerified;
        if (bSaveAttempted && bRollbackSemanticRestore)
        {
            bRollbackSaveAttempted = true;
            bRollbackDataflowSaved = SaveObjectIfRequested(Dataflow, true);
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            if (bRollbackDataflowSaved)
            {
                bRollbackClothAssetSaved = SaveObjectIfRequested(Asset, true);
                if(!Guard()) { return ErrorResponse(Work->R.Error); }
            }
        }
        const bool bRollbackSaveVerified = !bSaveAttempted ||
            (bRollbackSaveAttempted && bRollbackDataflowSaved &&
             bRollbackClothAssetSaved);
        bRollbackSucceeded = bRollbackSemanticRestore && bRollbackSaveVerified;
        if (bRollbackSucceeded && !bRollbackSaveAttempted)
        {
            Dataflow->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
            Asset->MarkPackageDirty();
            if(!Guard()) { return ErrorResponse(Work->R.Error); }
        }
    }

    TSharedPtr<FJsonObject> After = MakeShared<FJsonObject>();
    After->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    After->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    After->SetObjectField(
        TEXT("asset_weight_map"),
        MakeClothWeightMapAssetStateSnapshot(AfterAssetState));
    if (bAfterNodeReadable)
    {
        After->SetObjectField(
            TEXT("weight_map_node"),
            MakeClothWeightMapNodeStateSnapshot(AfterNodeState));
    }
    After->SetObjectField(
        TEXT("downstream_path"),
        MakeWeightMapDownstreamPathSnapshot(
            AfterDownstreamPath,
            bAfterDownstreamPathValid,
            AfterDownstreamPathError));
    After->SetBoolField(TEXT("cloth_asset_regenerated"), bRegenerated);
    After->SetBoolField(TEXT("node_readback_verified"), bNodeVerified);
    After->SetBoolField(
        TEXT("downstream_path_verified"),
        bDownstreamPathVerified);
    After->SetStringField(
        TEXT("derived_data_path_mode"),
        AfterDerivedDataPathMode);
    After->SetBoolField(
        TEXT("derived_data_path_verified"),
        bDerivedDataPathVerified);
    After->SetStringField(
        TEXT("derived_data_path_error"),
        AfterDerivedDataPathError);
    After->SetBoolField(
        TEXT("tether_and_selection_derived_state_verified"),
        bTetherDerivedStateVerified);
    After->SetStringField(
        TEXT("tether_and_selection_derived_state_error"),
        AfterTetherDerivedStateError);
    After->SetBoolField(TEXT("topology_verified"), bTopologyVerified);
    After->SetBoolField(TEXT("terminal_map_verified"), bMapVerified);
    After->SetBoolField(TEXT("compiled_map_verified"), bCompiledMapVerified);
    After->SetBoolField(TEXT("runtime_binding_verified"), bRuntimeBindingVerified);
    After->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    After->SetBoolField(TEXT("rollback_semantic_restore"), bRollbackSemanticRestore);
    After->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);

    const bool bPartialTargetModified = !bVerified && !bRollbackSucceeded;
    const bool bChanged = bVerified || bPartialTargetModified;
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bVerified);
    if (!bVerified)
    {
        TArray<FString> Errors = ApplyErrors;
        if (!bRegenerated)
        {
            Errors.Add(TEXT("RegenerateAssetFromDataflow returned false"));
        }
        if (!bNodeVerified)
        {
            Errors.Add(TEXT("WeightMap node readback failed"));
        }
        if (!bDownstreamPathVerified)
        {
            Errors.Add(FString::Printf(
                TEXT("Exclusive selected-terminal LOD path readback failed: %s"),
                *AfterDownstreamPathError));
        }
        if (!bDerivedDataPathVerified)
        {
            Errors.Add(FString::Printf(
                TEXT("Tether/selection derived-data path verification failed: %s"),
                *AfterDerivedDataPathError));
        }
        if (!bTetherDerivedStateVerified)
        {
            Errors.Add(FString::Printf(
                TEXT("Collection/compiled tether and KinematicVertices3D readback failed: %s"),
                *AfterTetherDerivedStateError));
        }
        if (!bTopologyVerified)
        {
            Errors.Add(TEXT("WeightMap node input or terminal topology changed"));
        }
        if (!bMapVerified)
        {
            Errors.Add(TEXT("Terminal collection weight-map readback failed"));
        }
        if (!bCompiledMapVerified)
        {
            Errors.Add(TEXT("Compiled solver weight-map value readback failed"));
        }
        if (!bRuntimeBindingVerified)
        {
            Errors.Add(TEXT("Compiled solver/runtime property binding readback failed"));
        }
        if (!bSaveVerified)
        {
            Errors.Add(TEXT("One or more requested asset saves failed"));
        }
        Errors.Append(RollbackErrors);
        if (bRollbackSucceeded)
        {
            Errors.Add(TEXT("The failed mutation was semantically restored; regenerated derived state is not byte-identical, so the editor transaction is retained"));
        }
        else if (bRollbackAttempted)
        {
            Errors.Add(TEXT("Rollback did not complete; use the retained editor transaction before continuing"));
        }
        Result->SetStringField(
            TEXT("error"),
            Errors.IsEmpty()
                ? TEXT("Chaos Cloth WeightMap mutation verification failed")
                : FString::Join(Errors, TEXT("; ")));
    }
    Result->SetStringField(TEXT("cloth_asset_path"), Asset->GetPathName());
    Result->SetStringField(TEXT("dataflow_asset_path"), Dataflow->GetPathName());
    Result->SetStringField(TEXT("node_name"), NodeName);
    Result->SetStringField(TEXT("weight_map_name"), WeightMapName);
    Result->SetStringField(TEXT("requested_weights_md5"), RequestedWeightsMd5);
    Result->SetStringField(TEXT("path_md5"), AfterDownstreamPath.PathMd5);
    Result->SetNumberField(TEXT("requested_weight_count"), RequestedWeights.Num());
    Result->SetBoolField(TEXT("no_op"), false);
    Result->SetBoolField(TEXT("mutation_attempted"), true);
    Result->SetStringField(
        TEXT("transaction_scope"),
        Transaction ? TEXT("owned") : TEXT("joined_outer"));
    Result->SetBoolField(TEXT("cloth_asset_regenerated"), bRegenerated);
    Result->SetBoolField(TEXT("save_requested"), bSaveAssets);
    Result->SetBoolField(TEXT("save_attempted"), bSaveAttempted);
    Result->SetBoolField(TEXT("dataflow_saved"), bDataflowSaved);
    Result->SetBoolField(TEXT("cloth_asset_saved"), bClothAssetSaved);
    Result->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    Result->SetBoolField(TEXT("rollback_regenerated"), bRollbackRegenerated);
    Result->SetBoolField(TEXT("rollback_node_verified"), bRollbackNodeVerified);
    Result->SetBoolField(
        TEXT("rollback_downstream_path_verified"),
        bRollbackDownstreamPathVerified);
    Result->SetBoolField(
        TEXT("rollback_derived_data_path_verified"),
        bRollbackDerivedDataPathVerified);
    Result->SetBoolField(
        TEXT("rollback_tether_and_selection_derived_state_verified"),
        bRollbackTetherDerivedStateVerified);
    Result->SetBoolField(TEXT("rollback_asset_verified"), bRollbackAssetVerified);
    Result->SetBoolField(TEXT("rollback_save_attempted"), bRollbackSaveAttempted);
    Result->SetBoolField(TEXT("rollback_dataflow_saved"), bRollbackDataflowSaved);
    Result->SetBoolField(TEXT("rollback_cloth_asset_saved"), bRollbackClothAssetSaved);
    Result->SetBoolField(TEXT("rollback_semantic_restore"), bRollbackSemanticRestore);
    Result->SetBoolField(TEXT("rollback_transaction_retained"), bRollbackAttempted);
    Result->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);
    Result->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
    Result->SetBoolField(TEXT("do_not_retry"), !bVerified);
    Result->SetArrayField(
        TEXT("other_referencer_packages"),
        StringsToJson(Referencers.OtherReferencerPackages));
    Result->SetArrayField(
        TEXT("other_loaded_dataflow_owners"),
        LoadedDataflowOwnerReferencesToJson(Referencers.OtherLoadedOwners));
    FLegacy::AddMutationResultContract(
        Result,
        false,
        true,
        bChanged,
        Before,
        After,
        MakeDeltaCounts(bChanged, !bVerified),
        bVerified,
        TEXT("weight_map_node_terminal_collection_compiled_map_tether_and_selection_readback"),
        {TEXT("exact_asset"), TEXT("unique_current_weight_map_node"), TEXT("single_selected_terminal_lod_path_md5"), TEXT("active_unfrozen_derived_generator_path"), TEXT("shared_dataflow_gate"), TEXT("node_input_topology"), TEXT("normalized_full_weight_array"), TEXT("ReplaceAll_node_readback"), TEXT("terminal_collection_map"), TEXT("compiled_solver_map_values"), TEXT("collection_compiled_canonical_tether_hash"), TEXT("collection_compiled_kinematic_selection_hash"), TEXT("compiled_runtime_binding"), TEXT("save_gate"), TEXT("rollback_gate")});
    AlignMutationAttemptContract(Result, true);
    return Result;
}

}
bool DescribeWeightMapAuthoringState(const TSharedPtr<FDataflowNode>& Node,UObject* Asset,TSharedPtr<FJsonObject>& State,FString& Error)
{
    if(!IsInGameThread()||!GEditor) { Error=TEXT("Editor game-thread authoring state observation required");return false; }
    return DescribeCurrentState(Node,Asset,State,Error);
}
namespace
{
FString Run(const FString& ParamsJson,const TCHAR* Name,bool WeightMap)
{
    FWork W(Name);W.R.Stage=TEXT("parse_parameters");if(!Parse(ParamsJson,W.Params)) { W.Fail(TEXT("parameters_invalid"),TEXT("Bounded finite unambiguous JSON object required"));return W.Finish(nullptr); }
    if(!IsInGameThread()||!GEditor||Running) { W.Fail(TEXT("editor_required"),TEXT("Non-reentrant Editor game-thread operation required"));return W.Finish(nullptr); }
    TGuardValue<bool> RunningScope(Running,true);TGuardValue<FWork*> WorkScope(Work,&W);W.World=GEditor->GetEditorWorldContext().World();W.WorldCaptured=true;
    const auto Legacy=WeightMap?LegacyConfigureWeightMap(W.Params):LegacyConfigureNode(W.Params);
    W.Admit();return W.Finish(Legacy);
}
}
FString ConfigureNode(const FString& ParamsJson) { return Run(ParamsJson,TEXT("configure_chaos_cloth_dataflow_node"),false); }
FString ConfigureWeightMap(const FString& ParamsJson) { return Run(ParamsJson,TEXT("configure_chaos_cloth_weight_map"),true); }
}

