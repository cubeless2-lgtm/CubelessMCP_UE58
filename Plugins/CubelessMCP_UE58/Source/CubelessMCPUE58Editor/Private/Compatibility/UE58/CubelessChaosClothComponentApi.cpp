// Dedicated Editor-only Chaos Cloth component operations for UE5.8.3.
#include "Compatibility/UE58/CubelessChaosClothComponentApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Chaos/CollectionPropertyFacade.h"
#include "ChaosClothAsset/ClothAssetBase.h"
#include "ChaosClothAsset/ClothAsset.h"
#include "GeometryCollection/ManagedArrayCollection.h"
#include "CoreGlobals.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "ChaosClothAsset/ClothAssetInteractor.h"
#include "ChaosClothAsset/ClothComponent.h"
#include "ChaosClothAsset/ClothSimulationProxy.h"
#include "Dataflow/DataflowSimulationInterface.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::ChaosClothComponent
{
namespace
{
using Variable::FResult;
constexpr int32 MaxJsonChars=1024*1024,MaxDepth=16,MaxSlots=262144,MaxProjectionChars=4*1024*1024,MaxMatchedComponents=4096,MaxRuntimeTargets=1024;
bool Running=false;
bool Live(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
FString Json(const TSharedRef<FJsonObject>& J) { FString S;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&S));return S; }
bool Text(const FString& S) { if(S.Len()>MaxJsonChars) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }return true; }
struct FExactKeyFuncs:DefaultKeyFuncs<FString>
{ static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }static uint32 GetKeyHash(const FString& V) { return FCrc::StrCrc32(*V); } };
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||!Text(S)) { return false; }auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeyFuncs>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>MaxSlots) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>MaxDepth) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& V) { TArray<TSharedPtr<FJsonValue>> A;for(const auto& S:V) { A.Add(MakeShared<FJsonValueString>(S)); }return A; }
TArray<TSharedPtr<FJsonValue>> Vector3fToJson(const FVector3f& V) { return {MakeShared<FJsonValueNumber>(V.X),MakeShared<FJsonValueNumber>(V.Y),MakeShared<FJsonValueNumber>(V.Z)}; }
bool FiniteJson(const TSharedPtr<FJsonValue>& V,int32 D=0) { if(!V.IsValid()||D>24) { return false; }if(V->Type==EJson::Number) { return FMath::IsFinite(V->AsNumber()); }if(V->Type==EJson::Array) { for(const auto& X:V->AsArray()) { if(!FiniteJson(X,D+1)) { return false; } } }else if(V->Type==EJson::Object) { for(const auto& X:V->AsObject()->Values) { if(!FiniteJson(X.Value,D+1)) { return false; } } }return true; }
TSharedPtr<FJsonObject> MakeComponentSettingsFlatSnapshot(UChaosClothComponent* Component);
struct FWork;struct FTCRuntimeCollections;bool TCRuntimeCurrent(FWork& W);
struct FWork
{
    TSharedPtr<FTCRuntimeCollections> Runtime;bool RuntimeLost=false;
    FResult R;FString Command,FailedStage,CapturedPackageName;TArray<FString> MutationStages;bool bPrimarySaveObserved=false;TSharedPtr<FJsonObject> Params,Legacy;TSharedRef<FJsonObject> Calls=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>();
    TStrongObjectPtr<UWorld> World{nullptr};TStrongObjectPtr<AActor> Actor{nullptr};TStrongObjectPtr<UChaosClothComponent> Component{nullptr};TStrongObjectPtr<UChaosClothAssetBase> Asset{nullptr};TStrongObjectPtr<UPackage> CapturedPackage{nullptr};TSharedPtr<FJsonObject> Before;
    explicit FWork(const TCHAR* Name):Command(Name)
    {
        R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Calls);R.Data->SetObjectField(TEXT("actual_operation_prefix"),Prefix);R.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);R.Data->SetObjectField(TEXT("postconditions"),Checks);
        R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("effective_simulation_verified"),false);R.Data->SetBoolField(TEXT("runtime_exact_key_write_supported"),false);R.Data->SetBoolField(TEXT("runtime_raw_flags_restore_supported"),false);R.Data->SetBoolField(TEXT("private_persistent_true_teleport_restore_supported"),false);R.Data->SetBoolField(TEXT("whole_external_pipeline_complete"),false);
        R.Data->SetStringField(TEXT("counter_scope"),TEXT("Phase observation dispatches and explicitly wrapped mutation/save/wait calls; not a count of every nested public getter."));R.Data->SetStringField(TEXT("runtime_write_limit"),TEXT("Changed runtime updates use only checked registered native component-owned nonconst clones, exact native ClothAsset/model0, fresh LOD/key facade writes and exact raw-cell compensation. Asset/shared collection aliases, custom/Outfit models and unexpected clone replacement remain incomplete; no Interactor alias fan-out substitution."));R.Data->SetStringField(TEXT("synchronous_limit"),TEXT("Original public WaitForExistingParallelClothSimulation_GameThread is synchronous for effective changes/compensation. SDK getters, proxy recreation, PostLoad and exact SavePackages may also synchronously wait or update caches; no nonblocking guarantee."));
        for(const TCHAR* K:{TEXT("observation_phase_calls"),TEXT("mutation_dispatch_calls"),TEXT("mutation_dispatch_returns"),TEXT("wait_calls"),TEXT("wait_returns"),TEXT("save_calls"),TEXT("save_returns"),TEXT("rollback_save_calls"),TEXT("rollback_save_returns"),TEXT("runtime_property_write_calls"),TEXT("private_property_write_calls"),TEXT("explicit_compile_calls"),TEXT("Modify_calls"),TEXT("dirty_dispatch_calls"),TEXT("recreate_calls"),TEXT("recreate_returns"),TEXT("reset_proxy_calls"),TEXT("reset_proxy_returns")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    void Observe(const TCHAR* Stage) { R.Stage=Stage;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);Count(TEXT("observation_phase_calls")); }
    void Write(const TCHAR* Stage) { Observe(Stage);R.bMutationAttempted=true;Prefix->SetBoolField(TEXT("execution_mutation_attempted"),true);MutationStages.Add(Stage);Prefix->SetArrayField(TEXT("mutation_dispatch_stages"),StringsToJson(MutationStages));Count(TEXT("mutation_dispatch_calls")); }
    void Returned() { Count(TEXT("mutation_dispatch_returns")); }
    TSharedPtr<FJsonObject> Error(const FString& Message,const TCHAR* Code=TEXT("validation_failed")) { if(R.Error.IsEmpty()) { R.Error=Message;R.ErrorCode=TEXT("chaos_cloth_component_")+FString(Code);if(FailedStage.IsEmpty()) { FailedStage=R.Stage; }R.Data->SetStringField(TEXT("failed_stage"),FailedStage); }auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),R.Error);return J; }
    void NoteFailure(const TCHAR* Stage) { if(FailedStage.IsEmpty()) { FailedStage=Stage;R.Data->SetStringField(TEXT("failed_stage"),FailedStage); } }
    bool Enter(const FString& S) { R.Stage=TEXT("parse_parameters");if(!Parse(S,Params)) { Legacy=Error(TEXT("Bounded unambiguous JSON object required"),TEXT("params_invalid"));return false; }if(!IsInGameThread()||!GEditor||Running) { Legacy=Error(TEXT("Non-reentrant Editor game-thread operation required"),TEXT("editor_required"));return false; }return true; }
    TSharedRef<FJsonObject> Projection() const { auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("actor_live"),Live(Actor.Get()));J->SetBoolField(TEXT("component_live"),Live(Component.Get()));if(Live(Actor.Get())) { J->SetStringField(TEXT("actor_path"),Actor->GetPathName());J->SetStringField(TEXT("actor_label"),Actor->GetActorLabel());J->SetStringField(TEXT("actor_world"),Actor->GetWorld()?Actor->GetWorld()->GetPathName():FString()); }if(Live(Component.Get())) { J->SetStringField(TEXT("component_path"),Component->GetPathName());J->SetBoolField(TEXT("registered"),Component->IsRegistered());J->SetStringField(TEXT("asset_path"),Component->GetAsset()?Component->GetAsset()->GetPathName():FString());J->SetStringField(TEXT("package"),Component->GetPackage()->GetName());J->SetBoolField(TEXT("package_dirty"),Component->GetPackage()->IsDirty()); }return J; }
    void Capture(UWorld* InWorld,AActor* InActor,UChaosClothComponent* InComponent) { World.Reset(InWorld);Actor.Reset(InActor);Component.Reset(InComponent);Asset.Reset(InComponent?InComponent->GetAsset():nullptr);CapturedPackage.Reset(InComponent?InComponent->GetPackage():nullptr);CapturedPackageName=CapturedPackage.Get()?CapturedPackage->GetName():FString();Before=Projection();R.Data->SetObjectField(TEXT("target_before"),Before.ToSharedRef()); }
    bool Admit() { if(RuntimeLost)return false;if(Runtime&&!TCRuntimeCurrent(*this))return false;if(!Live(World.Get())||!Live(Actor.Get())||!Live(Component.Get())||Component->GetOwner()!=Actor.Get()||Actor->GetWorld()!=World.Get()||Component->GetWorld()!=World.Get()||Component->GetAsset()!=Asset.Get()||!IsValid(CapturedPackage.Get())||Component->GetPackage()!=CapturedPackage.Get()||!CapturedPackage->GetName().Equals(CapturedPackageName,ESearchCase::CaseSensitive)) { if(Runtime){RuntimeLost=true;R.bChangeStateKnown=false;}Legacy=Error(TEXT("Captured component/owner/world/asset/package identity changed during SDK dispatch"),TEXT("target_identity_lost"));return false; }return true; }
    FString Finish(TSharedPtr<FJsonObject> Result=nullptr)
    {
        if(Result) { Legacy=Result; }if(!Legacy) { Legacy=Error(TEXT("No operation result returned"),TEXT("missing_result")); }
        if(!RuntimeLost&&(!Runtime||Admit())&&Live(Component.Get())&&Command==TEXT("configure_chaos_cloth_component")) { Observe(TEXT("final_configured_settings_observation"));auto Flat=MakeComponentSettingsFlatSnapshot(Component.Get());if(FiniteJson(MakeShared<FJsonValueObject>(Flat.ToSharedRef()))) { R.Data->SetObjectField(TEXT("configured_settings_after"),Flat); }else { R.Data->SetBoolField(TEXT("configured_settings_after_nonfinite"),true); } }
        if(!RuntimeLost&&Component.Get()) { auto After=Projection();R.Data->SetObjectField(TEXT("target_after"),After);const bool Changed=Before.IsValid()&&!Json(Before.ToSharedRef()).Equals(Json(After),ESearchCase::CaseSensitive);R.Data->SetBoolField(TEXT("observed_identity_dirty_projection_changed"),Changed);if(Changed&&Command==TEXT("inspect_chaos_cloth_component")&&R.Error.IsEmpty()) { Legacy=Error(TEXT("Observer changed captured target identity/dirty projection"),TEXT("observer_projection_changed")); } }
        if(Runtime&&!RuntimeLost)Admit();
        bool LegacySuccess=false;Legacy->TryGetBoolField(TEXT("success"),LegacySuccess);bool B=false;if(Legacy->TryGetBoolField(TEXT("mutation_attempted"),B)) { R.bMutationAttempted|=B; }if(Legacy->TryGetBoolField(TEXT("changed"),B)) { R.bChanged=B; }if(Legacy->TryGetBoolField(TEXT("save_requested"),B)) { R.bSaveRequested=B; }if(!bPrimarySaveObserved&&Legacy->TryGetBoolField(TEXT("save_attempted"),B)) { R.bSaveAttempted=B; }if(!bPrimarySaveObserved&&Legacy->TryGetBoolField(TEXT("saved"),B)) { R.bSaved=B; }
        if(!LegacySuccess&&R.Error.IsEmpty()) { FString E;Legacy->TryGetStringField(TEXT("error"),E);Error(E.IsEmpty()?TEXT("Native operation validation or readback failed"):E,TEXT("readback_failed")); }
        R.bPostconditionPassed=LegacySuccess&&R.Error.IsEmpty();R.bSuccess=R.bPostconditionPassed;Checks->SetBoolField(TEXT("explicit_compile_calls_zero"),!R.bCompileAttempted);R.Data->SetBoolField(TEXT("execution_mutation_attempted"),R.bMutationAttempted);R.Data->SetBoolField(TEXT("global_change_state_unknown"),!R.bChangeStateKnown);
        if(!FiniteJson(MakeShared<FJsonValueObject>(Legacy.ToSharedRef()))||Json(Legacy.ToSharedRef()).Len()>MaxProjectionChars) { Legacy->RemoveField(TEXT("before"));Legacy->RemoveField(TEXT("after"));Legacy->RemoveField(TEXT("data"));R.Data->SetBoolField(TEXT("oversize_or_nonfinite_legacy_projection_omitted"),true);auto E=Error(TEXT("Legacy projection exceeded finite/bounded response health contract; identity/prefix/counters retained"),TEXT("projection_limit"));Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error);R.bSuccess=R.bPostconditionPassed=false; }
        R.Data->SetObjectField(TEXT("legacy_result"),Legacy.ToSharedRef());if(Json(R.Data).Len()>MaxProjectionChars) { Legacy->RemoveField(TEXT("before"));Legacy->RemoveField(TEXT("after"));Legacy->RemoveField(TEXT("data"));Prefix->RemoveField(TEXT("before_mutation_state"));Prefix->RemoveField(TEXT("after_mutation_state"));Prefix->RemoveField(TEXT("completed_component_snapshots"));R.Data->SetBoolField(TEXT("deep_projections_omitted_due_to_total_budget"),true);Error(TEXT("Total result budget exceeded; configured settings/identity/prefix/counters retained"),TEXT("result_budget"));Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error);R.bSuccess=R.bPostconditionPassed=false; }R.Stage=R.bSuccess?TEXT("complete"):FailedStage;return Cubeless::Operations::EncodeVariableResult(R,*Command);
    }
};
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
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
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
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
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
    const TSharedPtr<FJsonValue>* JsonValue = Params->Values.Find(Name);
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

    if (Expected->Type == EJson::String)
    {
        return Expected->AsString().Equals(Actual->AsString(), ESearchCase::CaseSensitive);
    }

    double ExpectedNumber = 0.0;
    double ActualNumber = 0.0;
    if (Expected->TryGetNumber(ExpectedNumber) && Actual->TryGetNumber(ActualNumber))
    {
        return FMath::IsNearlyEqual(ExpectedNumber, ActualNumber, 1.e-6);
    }

    bool bExpectedBool = false;
    bool bActualBool = false;
    if (Expected->TryGetBool(bExpectedBool) && Actual->TryGetBool(bActualBool))
    {
        return bExpectedBool == bActualBool;
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
            const TSharedPtr<FJsonValue> ActualNested = (*ActualObject)->TryGetField(FStringView(Pair.Key));
            if (!ActualNested.IsValid() || !JsonValueMatches(Pair.Value, ActualNested))
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
        const TSharedPtr<FJsonValue> ActualValue = Actual->TryGetField(FStringView(Pair.Key));
        if (!ActualValue.IsValid())
        {
            OutMismatches.Add(FString::Printf(TEXT("%s is not a supported/readable field"), *FString(Pair.Key)));
        }
        else if (!JsonValueMatches(Pair.Value, ActualValue))
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
        const TSharedPtr<FJsonValue> Value = Source->TryGetField(FStringView(Pair.Key));
        if (Value.IsValid())
        {
            Result->SetField(FString(Pair.Key), Value);
        }
    }
    return Result;
}

UWorld* ResolveWorld(const FString& RequestedContext, FString& OutResolvedContext, FString& OutError)
{
    if (!GEditor)
    {
        OutError = TEXT("GEditor is unavailable");
        return nullptr;
    }

    const FString Context = RequestedContext.IsEmpty() ? TEXT("auto") : RequestedContext.ToLower();
    FWorldContext* PlayContext = GEditor->GetPIEWorldContext();
    UWorld* PlayWorld = PlayContext ? PlayContext->World() : nullptr;
    UWorld* EditorWorld = GEditor->GetEditorWorldContext().World();

    if (Context == TEXT("play") || Context == TEXT("pie") || Context == TEXT("sie"))
    {
        if (!PlayWorld)
        {
            OutError = TEXT("world_context requested play, but no PIE/SIE world is active");
            return nullptr;
        }
        OutResolvedContext = TEXT("play");
        return PlayWorld;
    }
    if (Context == TEXT("editor"))
    {
        if (!EditorWorld)
        {
            OutError = TEXT("The editor world is unavailable");
            return nullptr;
        }
        OutResolvedContext = TEXT("editor");
        return EditorWorld;
    }
    if (Context != TEXT("auto"))
    {
        OutError = FString::Printf(
            TEXT("Unsupported world_context '%s'; expected auto, editor, or play"),
            *RequestedContext);
        return nullptr;
    }

    if (PlayWorld)
    {
        OutResolvedContext = TEXT("play");
        return PlayWorld;
    }
    if (EditorWorld)
    {
        OutResolvedContext = TEXT("editor");
        return EditorWorld;
    }
    OutError = TEXT("No editor or PIE/SIE world is available");
    return nullptr;
}

bool ActorMatches(const AActor* Actor, const FString& ActorName, const FString& ActorLabel)
{
    if (!Actor)
    {
        return false;
    }
    const bool bNameMatches = ActorName.IsEmpty() ||
        Actor->GetName() == ActorName ||
        Actor->GetPathName() == ActorName;
    const bool bLabelMatches = ActorLabel.IsEmpty() || Actor->GetActorLabel() == ActorLabel;
    return bNameMatches && bLabelMatches;
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

bool ReadReflectedBool(const UObject* Object, const FName PropertyName, bool& bOutValue)
{
    if (!Object)
    {
        return false;
    }
    const FBoolProperty* Property = FindFProperty<FBoolProperty>(Object->GetClass(), PropertyName);
    if (!Property)
    {
        return false;
    }
    bOutValue = Property->GetPropertyValue_InContainer(Object);
    return true;
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
    Property->SetStringField(TEXT("name"), Facade.GetKey(PropertyIndex));
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
            TEXT("UE 5.8 collection properties store numeric values as raw FVector3f; the semantic type is defined by the consuming solver key"));
    }
    if (bIncludeFlags)
    {
        Property->SetObjectField(TEXT("flags"), MakePropertyFlagsSnapshot(Facade, PropertyIndex));
    }
    return Property;
}


struct FClothComponentTarget
{
    AActor* Actor = nullptr;
    UChaosClothComponent* Component = nullptr;
};

TArray<FClothComponentTarget> FindClothComponents(
    UWorld* World,
    const FString& ActorName,
    const FString& ActorLabel,
    const FString& ComponentName)
{
    TArray<FClothComponentTarget> Result;
    if (!World)
    {
        return Result;
    }
    for (TActorIterator<AActor> It(World); It; ++It)
    {
        AActor* Actor = *It;
        if (!ActorMatches(Actor, ActorName, ActorLabel))
        {
            continue;
        }
        TArray<UChaosClothComponent*> Components;
        Actor->GetComponents<UChaosClothComponent>(Components);
        for (UChaosClothComponent* Component : Components)
        {
            if (Component &&
                (ComponentName.IsEmpty() ||
                 Component->GetName() == ComponentName ||
                 Component->GetPathName() == ComponentName))
            {
                Result.Add({Actor, Component});
            }
        }
    }
    return Result;
}

FString TeleportModeToString(EClothingTeleportMode Mode)
{
    if (const UEnum* Enum = StaticEnum<EClothingTeleportMode>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Mode));
    }
    return FString::FromInt(static_cast<int32>(Mode));
}

TSharedPtr<FJsonObject> MakeClothComponentSnapshot(
    FWork& W, const FClothComponentTarget& Target,
    int32 ModelIndex,
    int32 RequestedLod,
    bool bIncludeRuntimeProperties,
    int32 MaxProperties)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    UChaosClothComponent* Component = Target.Component;
    UChaosClothAssetBase* Asset = Component ? Component->GetAsset() : nullptr;
    Result->SetStringField(TEXT("actor_name"), Target.Actor ? Target.Actor->GetName() : FString());
    Result->SetStringField(TEXT("actor_label"), Target.Actor ? Target.Actor->GetActorLabel() : FString());
    Result->SetStringField(TEXT("component_name"), Component ? Component->GetName() : FString());
    Result->SetStringField(TEXT("component_path"), Component ? Component->GetPathName() : FString());
    Result->SetStringField(
        TEXT("level_package"),
        Target.Actor && Target.Actor->GetLevel() && Target.Actor->GetLevel()->GetPackage()
            ? Target.Actor->GetLevel()->GetPackage()->GetName()
            : FString());
    Result->SetStringField(
        TEXT("target_package"),
        Component && Component->GetPackage() ? Component->GetPackage()->GetName() : FString());
    Result->SetBoolField(
        TEXT("target_package_dirty"),
        Component && Component->GetPackage() && Component->GetPackage()->IsDirty());
    Result->SetStringField(TEXT("asset_path"), Asset ? Asset->GetPathName() : FString());
    Result->SetNumberField(TEXT("model_index"), ModelIndex);
    Result->SetNumberField(TEXT("model_count"), Asset ? Asset->GetNumClothSimulationModels() : 0);
    if (!Component)
    {
        return Result;
    }

    Result->SetBoolField(TEXT("simulation_enabled"), Component->IsSimulationEnabled());
    Result->SetBoolField(TEXT("simulation_suspended"), Component->IsSimulationSuspended());
    bool bConfiguredEnableSimulation = Component->IsSimulationEnabled();
    bool bConfiguredSuspendSimulation = Component->IsSimulationSuspended();
    if (ReadReflectedBool(Component, TEXT("bEnableSimulation"), bConfiguredEnableSimulation))
    {
        Result->SetBoolField(TEXT("configured_enable_simulation"), bConfiguredEnableSimulation);
    }
    if (ReadReflectedBool(Component, TEXT("bSuspendSimulation"), bConfiguredSuspendSimulation))
    {
        Result->SetBoolField(TEXT("configured_suspend_simulation"), bConfiguredSuspendSimulation);
    }
    TSharedPtr<FJsonObject> ExpectedCurrentFields = MakeShared<FJsonObject>();
    ExpectedCurrentFields->SetBoolField(TEXT("enable_simulation"), bConfiguredEnableSimulation);
    ExpectedCurrentFields->SetBoolField(TEXT("suspend_simulation"), bConfiguredSuspendSimulation);
    ExpectedCurrentFields->SetBoolField(
        TEXT("collide_with_environment"),
        Component->GetCollideWithEnvironment());
    ExpectedCurrentFields->SetNumberField(
        TEXT("cloth_geometry_scale"),
        Component->GetClothGeometryScale());
    ExpectedCurrentFields->SetNumberField(
        TEXT("teleport_distance_threshold"),
        Component->GetTeleportDistanceThreshold());
    ExpectedCurrentFields->SetNumberField(
        TEXT("teleport_rotation_threshold"),
        Component->GetTeleportRotationThreshold());
    Result->SetObjectField(TEXT("expected_current_fields"), ExpectedCurrentFields);
    Result->SetBoolField(TEXT("collide_with_environment"), Component->GetCollideWithEnvironment());
    Result->SetNumberField(TEXT("cloth_geometry_scale"), Component->GetClothGeometryScale());
    Result->SetNumberField(TEXT("teleport_distance_threshold"), Component->GetTeleportDistanceThreshold());
    Result->SetNumberField(TEXT("teleport_rotation_threshold"), Component->GetTeleportRotationThreshold());
    Result->SetBoolField(TEXT("needs_teleport"), Component->NeedsTeleport());
    Result->SetBoolField(TEXT("needs_reset"), Component->NeedsReset());
    Result->SetStringField(TEXT("teleport_mode"), TeleportModeToString(Component->GetClothTeleportMode()));
    const UE::Chaos::ClothAsset::FClothSimulationProxy* SimulationProxy =
        Component->GetClothSimulationProxy();
    Result->SetBoolField(TEXT("simulation_proxy_present"), SimulationProxy != nullptr);
    if (SimulationProxy)
    {
        TSharedPtr<FJsonObject> RuntimeMetrics = MakeShared<FJsonObject>();
        RuntimeMetrics->SetNumberField(TEXT("cloth_count"), SimulationProxy->GetNumCloths());
        RuntimeMetrics->SetNumberField(
            TEXT("kinematic_particle_count"),
            SimulationProxy->GetNumKinematicParticles());
        RuntimeMetrics->SetNumberField(
            TEXT("dynamic_particle_count"),
            SimulationProxy->GetNumDynamicParticles());
        RuntimeMetrics->SetNumberField(TEXT("iteration_count"), SimulationProxy->GetNumIterations());
        RuntimeMetrics->SetNumberField(TEXT("substep_count"), SimulationProxy->GetNumSubsteps());
        RuntimeMetrics->SetNumberField(
            TEXT("linear_solve_iteration_count"),
            SimulationProxy->GetNumLinearSolveIterations());
        RuntimeMetrics->SetNumberField(
            TEXT("linear_solve_error"),
            SimulationProxy->GetLinearSolveError());
        RuntimeMetrics->SetNumberField(
            TEXT("simulation_time_seconds"),
            SimulationProxy->GetSimulationTime());
        RuntimeMetrics->SetBoolField(TEXT("teleported"), SimulationProxy->IsTeleported());
        Result->SetObjectField(TEXT("runtime_metrics"), RuntimeMetrics);
    }

    UChaosClothAssetInteractor* RuntimeInteractor = nullptr;
    if (Asset && ModelIndex >= 0 && ModelIndex < Asset->GetNumClothSimulationModels())
    {
        RuntimeInteractor = Component->GetClothOutfitInteractor(ModelIndex, NAME_None);
    }
    Result->SetBoolField(
        TEXT("runtime_property_collections_initialized"),
        RuntimeInteractor != nullptr);
    if (bIncludeRuntimeProperties && RuntimeInteractor)
    {
        const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
            Component->GetPropertyCollections(ModelIndex);
        if(Collections.Num()>MaxRuntimeTargets) { W.Error(TEXT("Runtime LOD count exceeds documented health bound"),TEXT("runtime_lod_limit"));return Result; }
        Result->SetNumberField(TEXT("runtime_property_lod_count"), Collections.Num());
        TArray<TSharedPtr<FJsonValue>> Lods;
        int32 ReturnedCount = 0;
        int32 TotalCount = 0;
        for (int32 LodIndex = 0; LodIndex < Collections.Num(); ++LodIndex)
        {
            if (RequestedLod >= 0 && LodIndex != RequestedLod)
            {
                continue;
            }
            TSharedPtr<FJsonObject> Lod = MakeShared<FJsonObject>();
            Lod->SetNumberField(TEXT("lod_index"), LodIndex);
            Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[LodIndex]);
            Lod->SetBoolField(TEXT("property_facade_valid"), Facade.IsValid());
            TArray<TSharedPtr<FJsonValue>> Properties;
            if (Facade.IsValid())
            {
                TotalCount += Facade.Num();
                for (int32 PropertyIndex = 0;
                     PropertyIndex < Facade.Num() && ReturnedCount < MaxProperties;
                     ++PropertyIndex)
                {
                    Properties.Add(MakeShared<FJsonValueObject>(MakeCollectionPropertySnapshot(
                        Facade,
                        PropertyIndex,
                        true,
                        true)));
                    ++ReturnedCount;
                }
            }
            Lod->SetArrayField(TEXT("properties"), Properties);
            Lods.Add(MakeShared<FJsonValueObject>(Lod));
        }
        Result->SetNumberField(TEXT("runtime_property_count"), TotalCount);
        Result->SetNumberField(TEXT("returned_runtime_property_count"), ReturnedCount);
        Result->SetBoolField(TEXT("runtime_properties_truncated"), TotalCount > ReturnedCount);
        Result->SetArrayField(TEXT("runtime_property_lods"), Lods);
    }
    return Result;
}

bool TryReadVector3(const TSharedPtr<FJsonValue>& Value, FVector& OutVector)
{
    const TArray<TSharedPtr<FJsonValue>>* Values = nullptr;
    if (!Value.IsValid() || Value->Type != EJson::Array ||
        !Value->TryGetArray(Values) || !Values || Values->Num() != 3)
    {
        return false;
    }
    double X = 0.0;
    double Y = 0.0;
    double Z = 0.0;
    if (!ReadNumber((*Values)[0], X) ||
        !ReadNumber((*Values)[1], Y) ||
        !ReadNumber((*Values)[2], Z) ||
        !IsFiniteFloatRange(X) || !IsFiniteFloatRange(Y) || !IsFiniteFloatRange(Z))
    {
        return false;
    }
    OutVector = FVector(X, Y, Z);
    return true;
}

TSharedPtr<FJsonObject> FindRuntimePropertySnapshot(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    int32 LodIndex,
    const FString& PropertyName)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Component || !Component->GetAsset() ||
        ModelIndex < 0 || ModelIndex >= Component->GetAsset()->GetNumClothSimulationModels())
    {
        return Result;
    }
    const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
        Component->GetPropertyCollections(ModelIndex);
    if (!Collections.IsValidIndex(LodIndex))
    {
        return Result;
    }
    Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[LodIndex]);
    const int32 PropertyIndex = Facade.IsValid() ? Facade.GetKeyIndex(PropertyName) : INDEX_NONE;
    if (PropertyIndex == INDEX_NONE)
    {
        return Result;
    }
    Result = MakeCollectionPropertySnapshot(Facade, PropertyIndex, true, true);
    Result->SetNumberField(TEXT("lod_index"), LodIndex);
    return Result;
}

bool JsonObjectContainsScalarFields(
    const TSharedPtr<FJsonObject>& Expected,
    const TSharedPtr<FJsonObject>& Actual)
{
    TArray<FString> Mismatches;
    return CheckExpectedFields(Expected, Actual, Mismatches);
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

bool IsClothPhysicsEnabledForRecreate()
{
    IConsoleVariable* CVarClothPhysics =
        IConsoleManager::Get().FindConsoleVariable(TEXT("p.ClothPhysics"));
    return !CVarClothPhysics || CVarClothPhysics->GetBool();
}

struct FValidatedRuntimePropertyUpdate
{
    FString Name;
    FString ValueType;
    int32 LodIndex = -1;
    double Low = 0.0;
    double High = 0.0;
    int32 IntValue = 0;
    bool bBoolValue = false;
    FVector VectorValue = FVector::ZeroVector;
    FString StringValue;
    bool bHasStringValue = false;
    bool bIntrinsic = false;
    TSharedPtr<FJsonObject> ExpectedCurrent;
};

struct FRuntimePropertyRollbackSnapshot
{
    FString Name;
    int32 LodIndex = INDEX_NONE;
    FVector3f LowRaw = FVector3f::ZeroVector;
    FVector3f HighRaw = FVector3f::ZeroVector;
    FString StringValue;
    uint8 Flags = 0;
};

TSharedPtr<FJsonObject> MakeComponentSettingsFlatSnapshot(UChaosClothComponent* Component)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Component)
    {
        return Result;
    }
    bool bEnableSimulation = Component->IsSimulationEnabled();
    bool bSuspendSimulation = Component->IsSimulationSuspended();
    ReadReflectedBool(Component, TEXT("bEnableSimulation"), bEnableSimulation);
    ReadReflectedBool(Component, TEXT("bSuspendSimulation"), bSuspendSimulation);
    Result->SetBoolField(TEXT("enable_simulation"), bEnableSimulation);
    Result->SetBoolField(TEXT("suspend_simulation"), bSuspendSimulation);
    Result->SetBoolField(TEXT("collide_with_environment"), Component->GetCollideWithEnvironment());
    Result->SetNumberField(TEXT("cloth_geometry_scale"), Component->GetClothGeometryScale());
    Result->SetNumberField(TEXT("teleport_distance_threshold"), Component->GetTeleportDistanceThreshold());
    Result->SetNumberField(TEXT("teleport_rotation_threshold"), Component->GetTeleportRotationThreshold());
    return Result;
}

bool ValidateComponentSettings(
    const TSharedPtr<FJsonObject>& Settings,
    TArray<FString>& OutErrors,
    bool& bOutRequiresRecreate)
{
    bOutRequiresRecreate = false;
    if (!Settings.IsValid())
    {
        return true;
    }

    for (const auto& Pair : Settings->Values)
    {
        const FString Name(Pair.Key);
        if (Name == TEXT("enable_simulation") ||
            Name == TEXT("suspend_simulation") ||
            Name == TEXT("collide_with_environment"))
        {
            bool bValue = false;
            if (!ReadBool(Pair.Value, bValue))
            {
                OutErrors.Add(FString::Printf(TEXT("component_settings.%s must be a boolean"), *Name));
            }
        }
        else if (Name == TEXT("cloth_geometry_scale") ||
                 Name == TEXT("teleport_distance_threshold") ||
                 Name == TEXT("teleport_rotation_threshold"))
        {
            double Value = 0.0;
            const double MaxValue = Name == TEXT("cloth_geometry_scale")
                ? 10000.0
                : (Name == TEXT("teleport_rotation_threshold")
                    ? 180.0
                    : TNumericLimits<float>::Max());
            if (!ReadNumber(Pair.Value, Value) || !FMath::IsFinite(Value) || Value < 0.0 || Value > MaxValue)
            {
                OutErrors.Add(FString::Printf(
                    TEXT("component_settings.%s must be a finite number in [0, %.0f]"),
                    *Name,
                    MaxValue));
            }
            if (Name == TEXT("cloth_geometry_scale"))
            {
                bOutRequiresRecreate = true;
            }
        }
        else if (Name == TEXT("teleport_action"))
        {
            FString Value;
            if (!ReadString(Pair.Value, Value) ||
                !(Value.Equals(TEXT("none"), ESearchCase::IgnoreCase) ||
                  Value.Equals(TEXT("reset_mode"), ESearchCase::IgnoreCase) ||
                  Value.Equals(TEXT("teleport"), ESearchCase::IgnoreCase) ||
                  Value.Equals(TEXT("teleport_and_reset"), ESearchCase::IgnoreCase)))
            {
                OutErrors.Add(TEXT("component_settings.teleport_action must be none, reset_mode, teleport, or teleport_and_reset"));
            }
        }
        else
        {
            OutErrors.Add(FString::Printf(TEXT("Unsupported component setting '%s'"), *Name));
        }
    }
    return OutErrors.IsEmpty();
}

bool HasPersistentComponentSettings(const TSharedPtr<FJsonObject>& Settings)
{
    if (!Settings.IsValid())
    {
        return false;
    }
    static const TCHAR* PersistentFields[] = {
        TEXT("enable_simulation"),
        TEXT("suspend_simulation"),
        TEXT("collide_with_environment"),
        TEXT("cloth_geometry_scale"),
        TEXT("teleport_distance_threshold"),
        TEXT("teleport_rotation_threshold")};
    for (const TCHAR* Field : PersistentFields)
    {
        if (Settings->HasField(Field))
        {
            return true;
        }
    }
    FString TeleportAction;
    return Settings->TryGetStringField(TEXT("teleport_action"), TeleportAction) &&
        TeleportAction.Equals(TEXT("reset_mode"), ESearchCase::IgnoreCase);
}

bool ValidateRuntimePropertyUpdates(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    int32 DefaultLodIndex,
    const TArray<TSharedPtr<FJsonValue>>& UpdateValues,
    TArray<FValidatedRuntimePropertyUpdate>& OutUpdates,
    TArray<FString>& OutErrors,
    bool& bOutRequiresRecreate)
{
    if (UpdateValues.IsEmpty())
    {
        return true;
    }

    UChaosClothAssetBase* Asset = Component ? Component->GetAsset() : nullptr;
    if (!Asset || ModelIndex < 0 || ModelIndex >= Asset->GetNumClothSimulationModels())
    {
        OutErrors.Add(FString::Printf(
            TEXT("model_index %d is invalid for this component's cloth asset"),
            ModelIndex));
        return false;
    }

    if (!Component->GetClothOutfitInteractor(ModelIndex, NAME_None))
    {
        OutErrors.Add(
            TEXT("The component has no initialized runtime property interactor; register the component before changing runtime cloth properties"));
        return false;
    }
    const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
        Component->GetPropertyCollections(ModelIndex);
    if (Collections.IsEmpty())
    {
        OutErrors.Add(TEXT("The component has no initialized runtime property collections; register the component or assign a valid cloth asset first"));
        return false;
    }
    if (DefaultLodIndex >= Collections.Num())
    {
        OutErrors.Add(FString::Printf(
            TEXT("lod_index %d is outside the runtime property collection range [0, %d)"),
            DefaultLodIndex,
            Collections.Num()));
        return false;
    }

    if (UpdateValues.Num() > 256)
    {
        OutErrors.Add(TEXT("property_updates is limited to 256 entries"));
        return false;
    }

    TSet<FString> SeenPropertyLodTargets;int64 TargetCount=0;
    for (int32 UpdateIndex = 0; UpdateIndex < UpdateValues.Num(); ++UpdateIndex)
    {
        const TSharedPtr<FJsonObject> UpdateObject =
            UpdateValues[UpdateIndex].IsValid() && UpdateValues[UpdateIndex]->Type == EJson::Object
            ? UpdateValues[UpdateIndex]->AsObject()
            : nullptr;
        if (!UpdateObject.IsValid())
        {
            OutErrors.Add(FString::Printf(TEXT("property_updates[%d] must be an object"), UpdateIndex));
            continue;
        }

        FValidatedRuntimePropertyUpdate Update;
        const TSharedPtr<FJsonValue>* NameValue = UpdateObject->Values.Find(TEXT("name"));
        if (!NameValue || !ReadString(*NameValue, Update.Name) || Update.Name.IsEmpty())
        {
            OutErrors.Add(FString::Printf(
                TEXT("property_updates[%d].name must be a non-empty string"),
                UpdateIndex));
            continue;
        }
        const TSharedPtr<FJsonValue>* ValueTypeValue = UpdateObject->Values.Find(TEXT("value_type"));
        if (!ValueTypeValue)
        {
            ValueTypeValue = UpdateObject->Values.Find(TEXT("type"));
        }
        if (!ValueTypeValue || !ReadString(*ValueTypeValue, Update.ValueType) || Update.ValueType.IsEmpty())
        {
            OutErrors.Add(FString::Printf(
                TEXT("property_updates[%d].value_type must be a non-empty string"),
                UpdateIndex));
            continue;
        }
        Update.ValueType = Update.ValueType.ToLower();
        Update.LodIndex = DefaultLodIndex;
        if (const TSharedPtr<FJsonValue>* ExplicitLodValue = UpdateObject->Values.Find(TEXT("lod_index")))
        {
            double ExplicitLod = 0.0;
            if (!ReadNumber(*ExplicitLodValue, ExplicitLod) || !FMath::IsFinite(ExplicitLod) ||
                ExplicitLod != FMath::TruncToDouble(ExplicitLod) ||
                ExplicitLod < static_cast<double>(TNumericLimits<int32>::Min()) ||
                ExplicitLod > static_cast<double>(TNumericLimits<int32>::Max()))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d].lod_index must be a JSON number containing a 32-bit signed integer"),
                    UpdateIndex));
                continue;
            }
            Update.LodIndex = static_cast<int32>(ExplicitLod);
        }
        if (Update.LodIndex < -1 || Update.LodIndex >= Collections.Num())
        {
            OutErrors.Add(FString::Printf(
                TEXT("property_updates[%d].lod_index is outside [-1, %d)"),
                UpdateIndex,
                Collections.Num()));
            continue;
        }

        const int32 FirstLod = Update.LodIndex < 0 ? 0 : Update.LodIndex;
        const int32 LastLod = Update.LodIndex < 0 ? Collections.Num() - 1 : Update.LodIndex;
        TargetCount+=LastLod-FirstLod+1;if(TargetCount>MaxRuntimeTargets) { OutErrors.Add(TEXT("Expanded runtime property targets exceed documented health bound1024"));return false; }
        bool bOverlapsEarlierUpdate = false;
        for (int32 TargetLod = FirstLod; TargetLod <= LastLod; ++TargetLod)
        {
            const FString TargetKey = FString::Printf(TEXT("%d:%s"), TargetLod, *Update.Name);
            if (SeenPropertyLodTargets.Contains(TargetKey))
            {
                bOverlapsEarlierUpdate = true;
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d] overlaps an earlier update for '%s' at LOD %d"),
                    UpdateIndex,
                    *Update.Name,
                    TargetLod));
            }
            else
            {
                SeenPropertyLodTargets.Add(TargetKey);
            }
        }
        if (bOverlapsEarlierUpdate)
        {
            continue;
        }
        for (int32 LodIndex = FirstLod; LodIndex <= LastLod; ++LodIndex)
        {
            Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[LodIndex]);
            const int32 PropertyIndex = Facade.IsValid() ? Facade.GetKeyIndex(Update.Name) : INDEX_NONE;
            if (PropertyIndex == INDEX_NONE)
            {
                OutErrors.Add(FString::Printf(
                    TEXT("Runtime property '%s' does not exist at LOD %d"),
                    *Update.Name,
                    LodIndex));
                continue;
            }
            Update.bIntrinsic |= Facade.IsIntrinsic(PropertyIndex);
        }

        const TSharedPtr<FJsonObject>* ExpectedPtr = nullptr;
        if (UpdateObject->HasField(TEXT("expected_current")) &&
            !UpdateObject->HasTypedField<EJson::Object>(TEXT("expected_current")))
        {
            OutErrors.Add(FString::Printf(
                TEXT("property_updates[%d].expected_current must be a JSON object"),
                UpdateIndex));
            continue;
        }
        if (UpdateObject->TryGetObjectField(TEXT("expected_current"), ExpectedPtr) && ExpectedPtr)
        {
            Update.ExpectedCurrent = *ExpectedPtr;
            for (int32 LodIndex = FirstLod; LodIndex <= LastLod; ++LodIndex)
            {
                const TSharedPtr<FJsonObject> Current = FindRuntimePropertySnapshot(
                    Component,
                    ModelIndex,
                    LodIndex,
                    Update.Name);
                if (!JsonObjectContainsScalarFields(Update.ExpectedCurrent, Current))
                {
                    OutErrors.Add(FString::Printf(
                        TEXT("property_updates[%d].expected_current mismatch for '%s' at LOD %d"),
                        UpdateIndex,
                        *Update.Name,
                        LodIndex));
                }
            }
        }

        if (Update.bIntrinsic)
        {
            bOutRequiresRecreate = true;
        }

        if (Update.ValueType == TEXT("float"))
        {
            const TSharedPtr<FJsonValue>* Value = UpdateObject->Values.Find(TEXT("value"));
            if (!Value || !ReadNumber(*Value, Update.Low) || !IsFiniteFloatRange(Update.Low))
            {
                OutErrors.Add(FString::Printf(TEXT("property_updates[%d].value must be a finite float"), UpdateIndex));
            }
            else
            {
                Update.Low = static_cast<float>(Update.Low);
                Update.High = Update.Low;
            }
        }
        else if (Update.ValueType == TEXT("weighted_float"))
        {
            const TSharedPtr<FJsonValue>* LowValue = UpdateObject->Values.Find(TEXT("low"));
            const TSharedPtr<FJsonValue>* HighValue = UpdateObject->Values.Find(TEXT("high"));
            if (!LowValue || !HighValue ||
                !ReadNumber(*LowValue, Update.Low) || !ReadNumber(*HighValue, Update.High) ||
                !IsFiniteFloatRange(Update.Low) || !IsFiniteFloatRange(Update.High))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d] weighted_float requires finite low and high"),
                    UpdateIndex));
            }
            else
            {
                Update.Low = static_cast<float>(Update.Low);
                Update.High = static_cast<float>(Update.High);
            }
        }
        else if (Update.ValueType == TEXT("int"))
        {
            const TSharedPtr<FJsonValue>* JsonValue = UpdateObject->Values.Find(TEXT("value"));
            double Value = 0.0;
            if (!JsonValue || !ReadNumber(*JsonValue, Value) || !FMath::IsFinite(Value) ||
                Value != FMath::TruncToDouble(Value) ||
                Value < -16777215.0 || Value > 16777215.0)
            {
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d].value must be an integer in [-16777215, 16777215]"),
                    UpdateIndex));
            }
            else
            {
                Update.IntValue = static_cast<int32>(Value);
            }
        }
        else if (Update.ValueType == TEXT("bool"))
        {
            const TSharedPtr<FJsonValue>* Value = UpdateObject->Values.Find(TEXT("value"));
            if (!Value || !ReadBool(*Value, Update.bBoolValue))
            {
                OutErrors.Add(FString::Printf(TEXT("property_updates[%d].value must be a boolean"), UpdateIndex));
            }
        }
        else if (Update.ValueType == TEXT("vector"))
        {
            const TSharedPtr<FJsonValue>* Value = UpdateObject->Values.Find(TEXT("value"));
            if (!Value || !TryReadVector3(*Value, Update.VectorValue))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d].value must be [x, y, z]"),
                    UpdateIndex));
            }
            else
            {
                Update.VectorValue = FVector(FVector3f(Update.VectorValue));
            }
        }
        else if (Update.ValueType == TEXT("string"))
        {
            const TSharedPtr<FJsonValue>* Value = UpdateObject->Values.Find(TEXT("value"));
            if (!Value || !ReadString(*Value, Update.StringValue))
            {
                OutErrors.Add(FString::Printf(TEXT("property_updates[%d].value must be a string"), UpdateIndex));
            }
            Update.bHasStringValue = true;
        }
        else
        {
            OutErrors.Add(FString::Printf(
                TEXT("property_updates[%d].value_type '%s' is unsupported; use float, weighted_float, int, bool, vector, or string"),
                UpdateIndex,
                *Update.ValueType));
        }

        if (const TSharedPtr<FJsonValue>* AssociatedStringValue = UpdateObject->Values.Find(TEXT("string_value")))
        {
            FString AssociatedString;
            if (!ReadString(*AssociatedStringValue, AssociatedString))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("property_updates[%d].string_value must be a string"),
                    UpdateIndex));
            }
            else
            {
                Update.StringValue = AssociatedString;
                Update.bHasStringValue = true;
            }
        }
        OutUpdates.Add(MoveTemp(Update));
    }
    return OutErrors.IsEmpty();
}

bool VerifyRuntimePropertyUpdate(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    const FValidatedRuntimePropertyUpdate& Update,
    TArray<FString>& OutErrors)
{
    const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
        Component->GetPropertyCollections(ModelIndex);
    const int32 FirstLod = Update.LodIndex < 0 ? 0 : Update.LodIndex;
    const int32 LastLod = Update.LodIndex < 0 ? Collections.Num() - 1 : Update.LodIndex;
    bool bVerified = true;
    for (int32 LodIndex = FirstLod; LodIndex <= LastLod; ++LodIndex)
    {
        Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[LodIndex]);
        const int32 PropertyIndex = Facade.IsValid() ? Facade.GetKeyIndex(Update.Name) : INDEX_NONE;
        if (PropertyIndex == INDEX_NONE)
        {
            bVerified = false;
            OutErrors.Add(FString::Printf(TEXT("Property '%s' disappeared from LOD %d"), *Update.Name, LodIndex));
            continue;
        }

        bool bValueMatches = true;
        if (Update.ValueType == TEXT("float"))
        {
            bValueMatches = FMath::IsNearlyEqual(
                static_cast<double>(Facade.GetLowValue<float>(PropertyIndex)),
                Update.Low,
                1.e-5) &&
                FMath::IsNearlyEqual(
                    static_cast<double>(Facade.GetHighValue<float>(PropertyIndex)),
                    Update.High,
                    1.e-5);
        }
        else if (Update.ValueType == TEXT("weighted_float"))
        {
            bValueMatches = FMath::IsNearlyEqual(
                static_cast<double>(Facade.GetLowValue<float>(PropertyIndex)),
                Update.Low,
                1.e-5) &&
                FMath::IsNearlyEqual(
                    static_cast<double>(Facade.GetHighValue<float>(PropertyIndex)),
                    Update.High,
                    1.e-5);
        }
        else if (Update.ValueType == TEXT("int"))
        {
            bValueMatches = Facade.GetLowValue<int32>(PropertyIndex) == Update.IntValue &&
                Facade.GetHighValue<int32>(PropertyIndex) == Update.IntValue;
        }
        else if (Update.ValueType == TEXT("bool"))
        {
            bValueMatches = Facade.GetLowValue<bool>(PropertyIndex) == Update.bBoolValue &&
                Facade.GetHighValue<bool>(PropertyIndex) == Update.bBoolValue;
        }
        else if (Update.ValueType == TEXT("vector"))
        {
            const FVector3f Low = Facade.GetLowValue<FVector3f>(PropertyIndex);
            const FVector3f High = Facade.GetHighValue<FVector3f>(PropertyIndex);
            const FVector3f Expected(Update.VectorValue);
            bValueMatches = Low.Equals(Expected, 1.e-5f) && High.Equals(Expected, 1.e-5f);
        }
        else if (Update.ValueType == TEXT("string"))
        {
            bValueMatches = Facade.GetStringValue(PropertyIndex) == Update.StringValue;
        }
        if (Update.bHasStringValue)
        {
            bValueMatches &= Facade.GetStringValue(PropertyIndex) == Update.StringValue;
        }
        if (!bValueMatches)
        {
            bVerified = false;
            OutErrors.Add(FString::Printf(
                TEXT("Runtime property '%s' failed readback at LOD %d"),
                *Update.Name,
                LodIndex));
        }
    }
    return bVerified;
}

void ApplyComponentSettings(
    FWork& W, UChaosClothComponent* Component,
    const TSharedPtr<FJsonObject>& Settings)
{
    if (!Component || !Settings.IsValid())
    {
        return;
    }
    bool bValue = false;
    if (Settings->TryGetBoolField(TEXT("enable_simulation"), bValue))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("SetEnableSimulation"));Component->SetEnableSimulation(bValue);W.Returned();
    }
    if (Settings->TryGetBoolField(TEXT("suspend_simulation"), bValue))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("set_suspend_simulation"));bValue ? Component->SuspendSimulation() : Component->ResumeSimulation();W.Returned();
    }
    if (Settings->TryGetBoolField(TEXT("collide_with_environment"), bValue))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("SetCollideWithEnvironment"));Component->SetCollideWithEnvironment(bValue);W.Returned();
    }
    double Number = 0.0;
    if (Settings->TryGetNumberField(TEXT("cloth_geometry_scale"), Number))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("SetClothGeometryScale"));Component->SetClothGeometryScale(static_cast<float>(Number));W.Returned();
    }
    if (Settings->TryGetNumberField(TEXT("teleport_distance_threshold"), Number))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("SetTeleportDistanceThreshold"));Component->SetTeleportDistanceThreshold(static_cast<float>(Number));W.Returned();
    }
    if (Settings->TryGetNumberField(TEXT("teleport_rotation_threshold"), Number))
    {
        if(!W.Admit()) { return; }W.Write(TEXT("SetTeleportRotationThreshold"));Component->SetTeleportRotationThreshold(static_cast<float>(Number));W.Returned();
    }
}

void ApplyComponentTeleportAction(
    FWork& W, UChaosClothComponent* Component,
    const TSharedPtr<FJsonObject>& Settings)
{
    if (!Component || !Settings.IsValid())
    {
        return;
    }
    FString TeleportAction;
    if (Settings->TryGetStringField(TEXT("teleport_action"), TeleportAction))
    {
        if (TeleportAction.Equals(TEXT("reset_mode"), ESearchCase::IgnoreCase))
        {
            if(!W.Admit()) { return; }W.Write(TEXT("ResetTeleportMode"));Component->ResetTeleportMode();W.Returned();
        }
        else if (TeleportAction.Equals(TEXT("teleport"), ESearchCase::IgnoreCase))
        {
            if(!W.Admit()) { return; }W.Write(TEXT("ForceNextUpdateTeleport"));Component->ForceNextUpdateTeleport();W.Returned();
        }
        else if (TeleportAction.Equals(TEXT("teleport_and_reset"), ESearchCase::IgnoreCase))
        {
            if(!W.Admit()) { return; }W.Write(TEXT("ForceNextUpdateTeleportAndReset"));Component->ForceNextUpdateTeleportAndReset();W.Returned();
        }
    }
}

TSharedPtr<FJsonObject> MakeVerifiableComponentSettings(
    const TSharedPtr<FJsonObject>& Settings)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Settings.IsValid())
    {
        return Result;
    }
    static const TCHAR* BoolFields[] = {
        TEXT("enable_simulation"),
        TEXT("suspend_simulation"),
        TEXT("collide_with_environment")};
    for (const TCHAR* Field : BoolFields)
    {
        bool bValue = false;
        if (Settings->TryGetBoolField(Field, bValue))
        {
            Result->SetBoolField(Field, bValue);
        }
    }
    static const TCHAR* FloatFields[] = {
        TEXT("cloth_geometry_scale"),
        TEXT("teleport_distance_threshold"),
        TEXT("teleport_rotation_threshold")};
    for (const TCHAR* Field : FloatFields)
    {
        double Value = 0.0;
        if (Settings->TryGetNumberField(Field, Value))
        {
            Result->SetNumberField(Field, static_cast<float>(Value));
        }
    }
    return Result;
}

TSharedPtr<FJsonObject> MakeComponentMutationState(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    const TSharedPtr<FJsonObject>& ComponentSettings,
    const TArray<FValidatedRuntimePropertyUpdate>& Updates)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    if (!Component)
    {
        return Result;
    }

    Result->SetObjectField(
        TEXT("component_settings"),
        SelectJsonFields(MakeComponentSettingsFlatSnapshot(Component), ComponentSettings));
    if (ComponentSettings.IsValid() && ComponentSettings->HasField(TEXT("teleport_action")))
    {
        TSharedPtr<FJsonObject> TeleportState = MakeShared<FJsonObject>();
        TeleportState->SetBoolField(TEXT("needs_teleport"), Component->NeedsTeleport());
        TeleportState->SetBoolField(TEXT("needs_reset"), Component->NeedsReset());
        Result->SetObjectField(TEXT("teleport_state"), TeleportState);
    }

    TSharedPtr<FJsonObject> RuntimeProperties = MakeShared<FJsonObject>();
    for (const FValidatedRuntimePropertyUpdate& Update : Updates)
    {
        const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
            Component->GetPropertyCollections(ModelIndex);
        const int32 FirstLod = Update.LodIndex < 0 ? 0 : Update.LodIndex;
        const int32 LastLod = Update.LodIndex < 0 ? Collections.Num() - 1 : Update.LodIndex;
        for (int32 TargetLod = FirstLod; TargetLod <= LastLod; ++TargetLod)
        {
            RuntimeProperties->SetObjectField(
                FString::Printf(TEXT("lod_%d:%s"), TargetLod, *Update.Name),
                FindRuntimePropertySnapshot(Component, ModelIndex, TargetLod, Update.Name));
        }
    }
    Result->SetObjectField(TEXT("runtime_properties"), RuntimeProperties);
    return Result;
}

bool VerifyTeleportAction(
    UChaosClothComponent* Component,
    const TSharedPtr<FJsonObject>& Settings,
    TArray<FString>& OutErrors)
{
    FString Action;
    if (!Component || !Settings.IsValid() ||
        !Settings->TryGetStringField(TEXT("teleport_action"), Action) ||
        Action.Equals(TEXT("none"), ESearchCase::IgnoreCase))
    {
        return true;
    }
    bool bVerified = true;
    if (Action.Equals(TEXT("reset_mode"), ESearchCase::IgnoreCase))
    {
        bool bPersistentTeleport = false;
        bool bPersistentReset = false;
        bVerified = ReadReflectedBool(Component, TEXT("bTeleport"), bPersistentTeleport) &&
            ReadReflectedBool(Component, TEXT("bReset"), bPersistentReset) &&
            !bPersistentTeleport && !bPersistentReset;
    }
    else if (Action.Equals(TEXT("teleport"), ESearchCase::IgnoreCase))
    {
        bVerified = Component->NeedsTeleport() && !Component->NeedsReset();
    }
    else if (Action.Equals(TEXT("teleport_and_reset"), ESearchCase::IgnoreCase))
    {
        bVerified = Component->NeedsTeleport() && Component->NeedsReset();
    }
    if (!bVerified)
    {
        OutErrors.Add(FString::Printf(TEXT("teleport_action '%s' failed readback"), *Action));
    }
    return bVerified;
}

bool IsOneShotTeleportAction(const TSharedPtr<FJsonObject>& Settings)
{
    FString Action;
    return Settings.IsValid() &&
        Settings->TryGetStringField(TEXT("teleport_action"), Action) &&
        (Action.Equals(TEXT("teleport"), ESearchCase::IgnoreCase) ||
         Action.Equals(TEXT("teleport_and_reset"), ESearchCase::IgnoreCase));
}

bool IsResetTeleportAction(const TSharedPtr<FJsonObject>& Settings)
{
    FString Action;
    return Settings.IsValid() &&
        Settings->TryGetStringField(TEXT("teleport_action"), Action) &&
        Action.Equals(TEXT("reset_mode"), ESearchCase::IgnoreCase);
}

bool RuntimePropertyMatches(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    const FValidatedRuntimePropertyUpdate& Update)
{
    TArray<FString> IgnoredErrors;
    return VerifyRuntimePropertyUpdate(Component, ModelIndex, Update, IgnoredErrors);
}

bool CaptureRuntimePropertyRollbackValues(
    UChaosClothComponent* Component,
    int32 ModelIndex,
    const TArray<FValidatedRuntimePropertyUpdate>& Updates,
    TArray<FRuntimePropertyRollbackSnapshot>& OutRollbackValues,
    TArray<FString>& OutErrors)
{
    if (Updates.IsEmpty())
    {
        return true;
    }
    if (!Component)
    {
        OutErrors.Add(TEXT("Runtime property rollback snapshot has no component"));
        return false;
    }
    const TArray<TSharedPtr<const FManagedArrayCollection>>& Collections =
        Component->GetPropertyCollections(ModelIndex);
    for (const FValidatedRuntimePropertyUpdate& Update : Updates)
    {
        const int32 FirstLod = Update.LodIndex < 0 ? 0 : Update.LodIndex;
        const int32 LastLod = Update.LodIndex < 0 ? Collections.Num() - 1 : Update.LodIndex;
        for (int32 TargetLod = FirstLod; TargetLod <= LastLod; ++TargetLod)
        {
            if (!Collections.IsValidIndex(TargetLod))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("Runtime rollback snapshot LOD %d is unavailable"),
                    TargetLod));
                continue;
            }
            Chaos::Softs::FCollectionPropertyConstFacade Facade(Collections[TargetLod]);
            const int32 PropertyIndex = Facade.IsValid()
                ? Facade.GetKeyIndex(Update.Name)
                : INDEX_NONE;
            if (PropertyIndex == INDEX_NONE)
            {
                OutErrors.Add(FString::Printf(
                    TEXT("Runtime rollback snapshot cannot find '%s' at LOD %d"),
                    *Update.Name,
                    TargetLod));
                continue;
            }

            FRuntimePropertyRollbackSnapshot& Restore = OutRollbackValues.AddDefaulted_GetRef();
            Restore.Name = Update.Name;
            Restore.LodIndex = TargetLod;
            Restore.LowRaw = Facade.GetLowValue<FVector3f>(PropertyIndex);
            Restore.HighRaw = Facade.GetHighValue<FVector3f>(PropertyIndex);
            Restore.StringValue = Facade.GetStringValue(PropertyIndex);
            Restore.Flags = static_cast<uint8>(Facade.GetFlags(PropertyIndex));
        }
    }
    return OutErrors.IsEmpty();
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
    bool bStrictSuccess = true)
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

bool RestorePersistentTeleport(FWork& W,UChaosClothComponent* C,bool Teleport,bool Reset)
{
    bool T=false,R=false;if(!ReadReflectedBool(C,TEXT("bTeleport"),T)||!ReadReflectedBool(C,TEXT("bReset"),R)) { return false; }
    if(T==Teleport&&R==Reset) { return true; }if(Teleport||Reset) { W.Prefix->SetBoolField(TEXT("persistent_true_teleport_restore_unavailable"),true);return false; }
    if(!W.Admit()) { return false; }W.Write(TEXT("rollback_ResetTeleportMode"));C->ResetTeleportMode();W.Returned();return ReadReflectedBool(C,TEXT("bTeleport"),T)&&ReadReflectedBool(C,TEXT("bReset"),R)&&!T&&!R;
}
// TC: only native component-owned nonconst clones, never asset/shared input collections.
struct FTCRuntimeCell
{
    FName Name; FString Spelling,Text; FVector3f Low=FVector3f::ZeroVector,High=FVector3f::ZeroVector;uint8 Flags=0;
};
struct FTCRuntimeCollections
{
    int32 Model=0,PendingLod=INDEX_NONE,PendingCell=INDEX_NONE;bool PermitDirtyClear=false,ReadbackFailed=false,LastWriteMatched=true;
    TStrongObjectPtr<UEditorEngine> Editor{nullptr};TStrongObjectPtr<UWorld> EditorWorld{nullptr};
    TStrongObjectPtr<UChaosClothAssetInteractor> Interactor{nullptr};
    TArray<TSharedPtr<const FManagedArrayCollection>> Runtime,Source;
    TArray<TArray<FTCRuntimeCell>> Expected,SourceBefore;
};
bool TCReadCells(const TSharedPtr<const FManagedArrayCollection>& Collection,TArray<FTCRuntimeCell>& Out)
{
    if(!Collection)return false;
    const Chaos::Softs::FCollectionPropertyConstFacade P(Collection);
    if(!P.IsValid()||P.Num()<0||P.Num()>4096||Collection->NumElements(FName(TEXT("Property")))!=P.Num())return false;
    Out.Reset();TSet<FName> Names;
    for(int32 I=0;I<P.Num();++I)
    {
        FTCRuntimeCell E;E.Name=P.GetKeyName(I);if(E.Name.IsNone()||Names.Contains(E.Name))return false;Names.Add(E.Name);
        E.Spelling=E.Name.ToString();E.Low=P.GetLowValue<FVector3f>(I);E.High=P.GetHighValue<FVector3f>(I);E.Text=P.GetStringValue(I);E.Flags=uint8(P.GetFlags(I));
        if(E.Spelling.Len()>1024||E.Text.Len()>8192||E.Low.ContainsNaN()||E.High.ContainsNaN())return false;
        Out.Add(MoveTemp(E));
    }
    return true;
}
bool TCSameCell(const FTCRuntimeCell& A,const FTCRuntimeCell& B)
{return A.Name==B.Name&&A.Spelling.Equals(B.Spelling,ESearchCase::CaseSensitive)&&A.Low==B.Low&&A.High==B.High&&A.Text.Equals(B.Text,ESearchCase::CaseSensitive)&&A.Flags==B.Flags;}
bool TCRuntimeLost(FWork& W,const TCHAR* Field,const FString& Actual)
{
    W.RuntimeLost=true;W.R.bChangeStateKnown=false;W.R.bSuccess=W.R.bPostconditionPassed=false;
    if(!W.R.Data->HasField(TEXT("runtime_first_mismatch"))){auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("field"),Field);J->SetStringField(TEXT("expected"),TEXT("captured native component-owned clone and planned exact typed cells"));J->SetStringField(TEXT("actual"),Actual);W.R.Data->SetObjectField(TEXT("runtime_first_mismatch"),J);}
    W.Prefix->SetBoolField(TEXT("runtime_boundary_unknown"),true);W.R.Data->SetBoolField(TEXT("post_failure_live_observation_skipped"),true);W.R.Data->SetBoolField(TEXT("do_not_retry"),true);
    W.Error(TEXT("Runtime collection/source/current boundary changed; copied actual prefix retained, no further SDK or compensation"),TEXT("runtime_boundary_unknown"));return false;
}
bool TCRuntimeCurrent(FWork& W)
{
    if(W.RuntimeLost)return false;if(!W.Runtime)return true;auto& S=*W.Runtime;
    if(!IsInGameThread()||GEditor!=S.Editor.Get()||!Live(S.Editor.Get())||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()||!Live(S.EditorWorld.Get())||GEditor->GetEditorWorldContext().World()!=S.EditorWorld.Get()||!Live(W.World.Get())||W.World->bIsTearingDown||!Live(W.Actor.Get())||W.Actor->IsActorBeingDestroyed()||!Live(W.Component.Get())||W.Component->GetClass()!=UChaosClothComponent::StaticClass()||W.Component->IsTemplate()||!W.Component->IsRegistered()||W.Component->GetOwner()!=W.Actor.Get()||W.Component->GetWorld()!=W.World.Get()||W.Actor->GetWorld()!=W.World.Get()||!W.Actor->GetComponents().Contains(W.Component.Get())||!Live(W.Asset.Get())||W.Asset->GetClass()!=UChaosClothAsset::StaticClass()||W.Component->GetAsset()!=W.Asset.Get()||W.Component->GetPackage()!=W.CapturedPackage.Get())return TCRuntimeLost(W,TEXT("context_owner"),TEXT("Editor/world/native registered component/asset/owner changed"));
    if(S.Model!=0||W.Asset->GetNumClothSimulationModels()!=1||!Live(S.Interactor.Get())||W.Component->GetClothOutfitInteractor(S.Model,NAME_None)!=S.Interactor.Get())return TCRuntimeLost(W,TEXT("model_interactor"),TEXT("model or held interactor changed"));
    const auto& Runtime=W.Component->GetPropertyCollections(S.Model);const auto& Source=W.Asset->GetCollections(S.Model);
    if(Runtime.Num()!=S.Runtime.Num()||Source.Num()!=S.Source.Num())return TCRuntimeLost(W,TEXT("collection_count"),TEXT("model LOD membership changed"));
    const uint8 Dirty=uint8(Chaos::Softs::ECollectionPropertyFlags::Dirty)|uint8(Chaos::Softs::ECollectionPropertyFlags::StringDirty);
    for(int32 L=0;L<Runtime.Num();++L)
    {
        if(Runtime[L]!=S.Runtime[L]||Source[L].ToSharedPtr()!=S.Source[L])return TCRuntimeLost(W,TEXT("collection_identity"),FString::Printf(TEXT("LOD=%d"),L));
        TArray<FTCRuntimeCell> Actual,SourceActual;if(!TCReadCells(Runtime[L],Actual)||!TCReadCells(Source[L].ToSharedPtr(),SourceActual)||Actual.Num()!=S.Expected[L].Num()||SourceActual.Num()!=S.SourceBefore[L].Num())return TCRuntimeLost(W,TEXT("collection_layout"),FString::Printf(TEXT("LOD=%d"),L));
        for(int32 I=0;I<Actual.Num();++I)
        {
            auto& E=S.Expected[L][I];if(S.PermitDirtyClear&&Actual[I].Flags==uint8(E.Flags&~Dirty)){auto Check=Actual[I];Check.Flags=E.Flags;if(TCSameCell(Check,E))E.Flags=Actual[I].Flags;}
            if(!TCSameCell(Actual[I],E))
            {
                // Only the single just-returned owned setter cell is a known
                // failed readback. Preserve that actual outcome for the original
                // compensation path; never replace Before or any unselected row.
                const bool SelectedReturn=L==S.PendingLod&&I==S.PendingCell&&Actual[I].Name==E.Name&&Actual[I].Spelling.Equals(E.Spelling,ESearchCase::CaseSensitive)&&uint8(Actual[I].Flags&~Dirty)==uint8(E.Flags&~Dirty);
                if(!SelectedReturn)return TCRuntimeLost(W,TEXT("runtime_cell"),FString::Printf(TEXT("LOD=%d index=%d name=%s"),L,I,*Actual[I].Spelling));
                S.ReadbackFailed=true;S.LastWriteMatched=false;
                if(!W.Prefix->HasField(TEXT("runtime_first_failed_readback"))){auto J=MakeShared<FJsonObject>();J->SetNumberField(TEXT("lod"),L);J->SetStringField(TEXT("key"),E.Spelling);J->SetArrayField(TEXT("expected_low"),Vector3fToJson(E.Low));J->SetArrayField(TEXT("actual_low"),Vector3fToJson(Actual[I].Low));J->SetArrayField(TEXT("expected_high"),Vector3fToJson(E.High));J->SetArrayField(TEXT("actual_high"),Vector3fToJson(Actual[I].High));J->SetStringField(TEXT("expected_string"),E.Text);J->SetStringField(TEXT("actual_string"),Actual[I].Text);J->SetNumberField(TEXT("expected_flags"),E.Flags);J->SetNumberField(TEXT("actual_flags"),Actual[I].Flags);W.Prefix->SetObjectField(TEXT("runtime_first_failed_readback"),J);}
                E=Actual[I];
            }
        }
        for(int32 I=0;I<SourceActual.Num();++I)if(!TCSameCell(SourceActual[I],S.SourceBefore[L][I]))return TCRuntimeLost(W,TEXT("asset_property_source"),FString::Printf(TEXT("LOD=%d index=%d"),L,I));
    }
    S.PermitDirtyClear=false;S.PendingLod=S.PendingCell=INDEX_NONE;W.R.bChangeStateKnown=true;return true;
}
bool TCBeginRuntime(FWork& W,int32 Model,const TArray<FRuntimePropertyRollbackSnapshot>& Rollback)
{
    if(!W.Admit())return false;
    if(!Live(W.Component.Get())||W.Component->GetClass()!=UChaosClothComponent::StaticClass()||W.Component->IsTemplate()||!W.Component->IsRegistered()||!Live(W.Asset.Get())||W.Asset->GetClass()!=UChaosClothAsset::StaticClass()||Model!=0||W.Asset->GetNumClothSimulationModels()!=1){W.Error(TEXT("Changed runtime updates require a registered exact native ClothComponent and native ClothAsset; custom component/Outfit sources remain incomplete"),TEXT("runtime_clone_scope_pending"));return false;}
    auto S=MakeShared<FTCRuntimeCollections>();S->Model=Model;S->Editor.Reset(GEditor);if(!IsInGameThread()||!Live(GEditor)||GExitPurge||IsEngineExitRequested()||IsGarbageCollecting()||UE::IsSavingPackage()){W.Error(TEXT("Idle live Editor required before runtime clone capture"),TEXT("runtime_context_unavailable"));return false;}S->EditorWorld.Reset(GEditor->GetEditorWorldContext().World());
    UChaosClothAssetInteractor* Returned=W.Component->GetClothOutfitInteractor(Model,NAME_None);S->Interactor.Reset(Returned);
    if(!W.Admit()||!Live(Returned)){W.Error(TEXT("Actual initialized component interactor is unavailable"),TEXT("runtime_interactor_unavailable"));return false;}
    const auto& Runtime=W.Component->GetPropertyCollections(Model);const auto& Source=W.Asset->GetCollections(Model);
    if(Runtime.IsEmpty()||Runtime.Num()>32||Runtime.Num()!=Source.Num()){W.Error(TEXT("Bounded same native asset/runtime LOD inventory required"),TEXT("runtime_collections_unavailable"));return false;}
    TSet<const FManagedArrayCollection*> Seen;int32 Total=0;
    for(int32 L=0;L<Runtime.Num();++L)
    {
        if(!Runtime[L]||Seen.Contains(Runtime[L].Get())){W.Error(TEXT("Runtime LOD clone is missing or shared across LODs"),TEXT("runtime_clone_alias"));return false;}Seen.Add(Runtime[L].Get());
        for(const auto& AssetCollection:Source)if(Runtime[L].Get()==&AssetCollection.Get()){W.Error(TEXT("Runtime collection aliases a read-only asset collection; no const cast"),TEXT("runtime_asset_alias"));return false;}
        TArray<FTCRuntimeCell> A,B;if(!TCReadCells(Runtime[L],A)||!TCReadCells(Source[L].ToSharedPtr(),B)||(Total+=A.Num()+B.Num())>MaxSlots){W.Error(TEXT("Native property layouts exceed bounded source/current capture"),TEXT("runtime_layout_unavailable"));return false;}
        S->Runtime.Add(Runtime[L]);S->Source.Add(Source[L].ToSharedPtr());S->Expected.Add(MoveTemp(A));S->SourceBefore.Add(MoveTemp(B));
    }
    W.Runtime=S;if(!W.Admit())return false;
    for(const auto& V:Rollback)
    {
        const FTCRuntimeCell* E=S->Expected.IsValidIndex(V.LodIndex)?S->Expected[V.LodIndex].FindByPredicate([&](const FTCRuntimeCell& Row){return Row.Name==FName(*V.Name);}):nullptr;
        if(!E||E->Low!=V.LowRaw||E->High!=V.HighRaw||!E->Text.Equals(V.StringValue,ESearchCase::CaseSensitive)||E->Flags!=V.Flags)return TCRuntimeLost(W,TEXT("rollback_capture_binding"),V.Name);
    }
    W.R.Data->SetBoolField(TEXT("runtime_exact_key_write_supported"),true);W.R.Data->SetBoolField(TEXT("runtime_raw_flags_restore_supported"),true);W.R.Data->SetBoolField(TEXT("runtime_component_clone_provenance_checked"),true);W.R.Data->SetNumberField(TEXT("runtime_lod_count"),S->Runtime.Num());return true;
}
bool TCApplyRuntime(FWork& W,const FValidatedRuntimePropertyUpdate& Update)
{
    if(!W.Runtime||!W.Admit())return false;auto& S=*W.Runtime;
    const int32 First=Update.LodIndex<0?0:Update.LodIndex,Last=Update.LodIndex<0?S.Runtime.Num()-1:Update.LodIndex;
    for(int32 L=First;L<=Last;++L)
    {
        if(!W.Admit())return false;
        const int32 I=S.Expected[L].IndexOfByPredicate([&](const FTCRuntimeCell& E){return E.Name==FName(*Update.Name);});if(I==INDEX_NONE)return TCRuntimeLost(W,TEXT("exact_key"),Update.Name);
        auto Planned=S.Expected[L][I];
        if(Update.ValueType!=TEXT("string"))
        {
            FVector3f Low,High;
            if(Update.ValueType==TEXT("float")){Low=High=FVector3f(float(Update.Low));}
            else if(Update.ValueType==TEXT("weighted_float")){Low=FVector3f(float(Update.Low));High=FVector3f(float(Update.High));}
            else if(Update.ValueType==TEXT("int")){Low=High=FVector3f(Update.IntValue);}
            else if(Update.ValueType==TEXT("bool")){Low=High=FVector3f(Update.bBoolValue);}
            else {Low=High=FVector3f(Update.VectorValue);}
            if(Planned.Low!=Low||Planned.High!=High)Planned.Flags|=uint8(Chaos::Softs::ECollectionPropertyFlags::Dirty);Planned.Low=Low;Planned.High=High;
        }
        if(Update.bHasStringValue||Update.ValueType==TEXT("string")){if(!Planned.Text.Equals(Update.StringValue,ESearchCase::CaseSensitive))Planned.Flags|=uint8(Chaos::Softs::ECollectionPropertyFlags::StringDirty)|uint8(Chaos::Softs::ECollectionPropertyFlags::Dirty);Planned.Text=Update.StringValue;}
        // Public nonconst owner origin is established by native ResetConfigProperties/Initialize,
        // and exact clone identity is freshly checked after every callback. No helper/view survives a call.
        W.Write(TEXT("runtime_property_exact_key"));W.Count(TEXT("runtime_property_write_calls"));
        {
            auto Mutable=ConstCastSharedPtr<FManagedArrayCollection>(W.Component->GetPropertyCollections(S.Model)[L]);Chaos::Softs::FCollectionPropertyFacade P(Mutable);
            if(Update.ValueType==TEXT("float"))P.SetValue<float>(I,float(Update.Low));else if(Update.ValueType==TEXT("weighted_float"))P.SetWeightedFloatValue(I,FVector2f(float(Update.Low),float(Update.High)));else if(Update.ValueType==TEXT("int"))P.SetValue<int32>(I,Update.IntValue);else if(Update.ValueType==TEXT("bool"))P.SetValue<bool>(I,Update.bBoolValue);else if(Update.ValueType==TEXT("vector"))P.SetValue<FVector3f>(I,FVector3f(Update.VectorValue));
            if(Update.bHasStringValue||Update.ValueType==TEXT("string"))P.SetStringValue(I,Update.StringValue);
        }
        S.Expected[L][I]=MoveTemp(Planned);S.PendingLod=L;S.PendingCell=I;S.LastWriteMatched=true;W.Returned();W.Prefix->SetNumberField(TEXT("runtime_last_returned_lod"),L);W.Prefix->SetStringField(TEXT("runtime_last_returned_key"),Update.Name);if(!W.Admit())return false;
    }
    return true;
}

bool RestoreRuntimePropertyValues(FWork& W,UChaosClothComponent* C,int32 ModelIndex,const TArray<FRuntimePropertyRollbackSnapshot>& Values,TArray<FString>& Errors)
{
    if(W.Runtime)
    {
        W.Prefix->SetBoolField(TEXT("runtime_raw_restore_write_attempted"),false);if(Values.IsEmpty())return true;
        auto& S=*W.Runtime;bool Matched=true;
        for(const auto& V:Values)
        {
            if(!W.Admit())return false;
            if(!S.Runtime.IsValidIndex(V.LodIndex))return TCRuntimeLost(W,TEXT("rollback_lod"),FString::FromInt(V.LodIndex));
            const int32 I=S.Expected[V.LodIndex].IndexOfByPredicate([&](const FTCRuntimeCell& E){return E.Name==FName(*V.Name);});if(I==INDEX_NONE)return TCRuntimeLost(W,TEXT("rollback_key"),V.Name);
            // Get every writable cell afresh. These are public ManagedArrayCollection
            // attributes on the native component clone; no protected facade access.
            {
                auto Mutable=ConstCastSharedPtr<FManagedArrayCollection>(W.Component->GetPropertyCollections(ModelIndex)[V.LodIndex]);const FName Group(TEXT("Property"));
                auto* Low=Mutable->FindAttributeTyped<FVector3f>(FName(TEXT("LowValue")),Group);auto* High=Mutable->FindAttributeTyped<FVector3f>(FName(TEXT("HighValue")),Group);auto* TextValue=Mutable->FindAttributeTyped<FString>(FName(TEXT("StringValue")),Group);auto* Flags=Mutable->FindAttributeTyped<uint8>(FName(TEXT("Flags")),Group);
                if(!Low||!High||!TextValue||!Flags||!Low->IsValidIndex(I)||!High->IsValidIndex(I)||!TextValue->IsValidIndex(I)||!Flags->IsValidIndex(I))return TCRuntimeLost(W,TEXT("rollback_storage"),V.Name);
                W.Write(TEXT("rollback_runtime_exact_raw_cells"));W.Count(TEXT("runtime_restore_write_calls"));W.Prefix->SetBoolField(TEXT("runtime_raw_restore_write_attempted"),true);
                (*Low)[I]=V.LowRaw;(*High)[I]=V.HighRaw;(*TextValue)[I]=V.StringValue;(*Flags)[I]=V.Flags;
            }
            auto& E=S.Expected[V.LodIndex][I];E.Low=V.LowRaw;E.High=V.HighRaw;E.Text=V.StringValue;E.Flags=V.Flags;S.PendingLod=V.LodIndex;S.PendingCell=I;S.LastWriteMatched=true;W.Returned();W.Prefix->SetNumberField(TEXT("runtime_restore_last_returned_lod"),V.LodIndex);W.Prefix->SetStringField(TEXT("runtime_restore_last_returned_key"),V.Name);if(!W.Admit())return false;Matched&=S.LastWriteMatched;
        }
        if(!Matched)Errors.Add(TEXT("Actual exact runtime raw-cell rollback readback did not match its captured values/flags"));W.R.Data->SetBoolField(TEXT("runtime_exact_raw_restore_readback_passed"),Matched);return Matched;
    }
    W.Observe(TEXT("verify_unchanged_runtime_rollback_snapshot"));W.Prefix->SetBoolField(TEXT("runtime_raw_restore_write_attempted"),false);if(Values.IsEmpty()) { return true; }
    if(!W.Admit()||!C->GetAsset()||ModelIndex<0||ModelIndex>=C->GetAsset()->GetNumClothSimulationModels()||!C->GetClothOutfitInteractor(ModelIndex,NAME_None)) { Errors.Add(TEXT("Original runtime state cannot be reacquired for read-only compensation verification"));return false; }
    const auto& Collections=C->GetPropertyCollections(ModelIndex);bool OK=true;
    for(const auto& V:Values)
    {
        if(!Collections.IsValidIndex(V.LodIndex)||!Collections[V.LodIndex]) { OK=false;continue; }const Chaos::Softs::FCollectionPropertyConstFacade P(Collections[V.LodIndex]);const int32 I=P.IsValid()?P.GetKeyIndex(V.Name):INDEX_NONE;
        if(I==INDEX_NONE||!P.GetLowValue<FVector3f>(I).Equals(V.LowRaw,0.f)||!P.GetHighValue<FVector3f>(I).Equals(V.HighRaw,0.f)||!P.GetStringValue(I).Equals(V.StringValue,ESearchCase::CaseSensitive)||static_cast<uint8>(P.GetFlags(I))!=V.Flags) { OK=false; }
    }
    if(!OK) { Errors.Add(TEXT("Runtime raw values/flags drifted; exact-key raw restore has no confirmed public mutable route and was not attempted")); }return OK;
}
void WaitForCloth(FWork& W,UChaosClothComponent* C)
{
    if(!W.Admit()) { return; }W.Observe(TEXT("wait_existing_parallel_cloth"));W.Count(TEXT("wait_calls"));if(W.Runtime)W.Runtime->PermitDirtyClear=true;C->WaitForExistingParallelClothSimulation_GameThread();W.Count(TEXT("wait_returns"));W.Prefix->SetBoolField(TEXT("synchronous_wrapper_wait_performed"),true);W.Admit();
}
void Recreate(FWork& W,UChaosClothComponent* C)
{
    if(!W.Admit()) { return; }W.Write(TEXT("recreate_cloth_simulation_proxy"));W.Count(TEXT("recreate_calls"));if(W.Runtime)W.Runtime->PermitDirtyClear=true;C->RecreateClothSimulationProxy();W.Count(TEXT("recreate_returns"));W.Returned();W.Admit();
}
void ResetProxy(FWork& W,UChaosClothComponent* C)
{
    if(!W.Admit()) { return; }W.Write(TEXT("reset_cloth_simulation_proxy"));W.Count(TEXT("reset_proxy_calls"));static_cast<IDataflowSimulationInterface*>(C)->ResetSimulationProxy();W.Count(TEXT("reset_proxy_returns"));W.Returned();W.Admit();
}
bool SaveExactPackage(FWork& W,UPackage* P,bool Rollback=false)
{
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetBoolField(TEXT("evaluated"),true);Receipt->SetBoolField(TEXT("attempted"),false);Receipt->SetBoolField(TEXT("returned"),false);Receipt->SetStringField(TEXT("captured_package"),W.CapturedPackageName);
    W.R.Data->SetObjectField(Rollback?TEXT("rollback_save_execution"):TEXT("save_execution"),Receipt);if(!Rollback) { W.bPrimarySaveObserved=true; }
    const bool CapturedPackageMatches=IsValid(P)&&P==W.CapturedPackage.Get()&&P->GetName().Equals(W.CapturedPackageName,ESearchCase::CaseSensitive);Receipt->SetBoolField(TEXT("captured_package_matches_before_dispatch"),CapturedPackageMatches);
    if(!CapturedPackageMatches||!IsDurablySavablePackage(P)||!W.Admit()) { Receipt->SetBoolField(TEXT("guard_rejected_before_dispatch"),true);if(!CapturedPackageMatches) { W.Error(TEXT("Captured target package changed before exact save"),TEXT("save_target_package_changed")); }return false; }
    W.Write(Rollback?TEXT("rollback_save_exact_component_package"):TEXT("save_exact_component_package"));if(!Rollback) { W.R.bSaveAttempted=true; }Receipt->SetBoolField(TEXT("attempted"),true);W.Count(Rollback?TEXT("rollback_save_calls"):TEXT("save_calls"));
    const bool Saved=UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{P},false);
    // Record the actual SDK return before any target revalidation or failure path.
    Receipt->SetBoolField(TEXT("returned"),true);Receipt->SetBoolField(TEXT("SDK_reported_success"),Saved);if(!Rollback) { W.R.bSaved=Saved; }W.R.Data->SetBoolField(Rollback?TEXT("rollback_save_SDK_reported_success"):TEXT("save_SDK_reported_success"),Saved);
    if(W.Runtime&&!W.Admit()){Receipt->SetBoolField(TEXT("package_dirty_known_after_return"),false);W.Count(Rollback?TEXT("rollback_save_returns"):TEXT("save_returns"));W.Returned();return false;}
    const bool PackageLive=IsValid(P);Receipt->SetBoolField(TEXT("package_dirty_known_after_return"),PackageLive);const bool Dirty=PackageLive?P->IsDirty():true;if(PackageLive) { Receipt->SetBoolField(TEXT("package_dirty_after_return"),Dirty); }else { Receipt->SetField(TEXT("package_dirty_after_return"),MakeShared<FJsonValueNull>()); }
    Receipt->SetStringField(TEXT("saved_semantics"),TEXT("SDK_reported_success_preserved_separately_from_identity_dirty_postcondition_and_cold_disk_proof"));W.Count(Rollback?TEXT("rollback_save_returns"):TEXT("save_returns"));W.Returned();const bool TargetValid=W.Admit();Receipt->SetBoolField(TEXT("target_identity_valid_after_return"),TargetValid);Receipt->SetBoolField(TEXT("dirty_postcondition_passed"),PackageLive&&!Dirty);
    return Saved&&TargetValid&&PackageLive&&!Dirty;
}

TSharedPtr<FJsonObject> Success(const TSharedPtr<FJsonObject>& Data) { auto R=MakeShared<FJsonObject>();R->SetBoolField(TEXT("success"),true);R->SetObjectField(TEXT("data"),Data);return R; }

} // namespace

FString Inspect(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_chaos_cloth_component"));if(!W.Enter(ParamsJson)) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);const auto Params=W.Params;
    const auto Execute=[&]() -> TSharedPtr<FJsonObject>
    {
    FJsonParamParseState ParamState;
    const FString ActorName = GetStringParam(Params, TEXT("actor_name"), ParamState);
    const FString ActorLabel = GetStringParam(Params, TEXT("actor_label"), ParamState);
    const FString ComponentName = GetStringParam(Params, TEXT("component_name"), ParamState);
    const FString RequestedContext = GetStringParam(Params, TEXT("world_context"), ParamState, TEXT("auto"));
    const int32 ModelIndex = GetIntParam(Params, TEXT("model_index"), 0, ParamState);
    const int32 LodIndex = GetIntParam(Params, TEXT("lod_index"), -1, ParamState);
    const bool bIncludeRuntimeProperties = GetBoolParam(
        Params,
        TEXT("include_runtime_properties"),
        true,
        ParamState);
    const int32 MaxProperties = FMath::Clamp(GetIntParam(Params, TEXT("max_properties"), 512, ParamState), 1, 4096);
    const int32 MaxResults = FMath::Clamp(GetIntParam(Params, TEXT("max_results"), 16, ParamState), 1, 256);
    if (!ParamState.IsValid())
    {
        return W.Error(ParamState.GetErrorMessage());
    }
    if (ModelIndex < 0 || LodIndex < -1)
    {
        return W.Error(
            TEXT("model_index must be non-negative and lod_index must be -1 or non-negative"));
    }

    FString WorldContext;
    FString Error;
    W.Observe(TEXT("resolve_world"));UWorld* World = ResolveWorld(RequestedContext, WorldContext, Error);
    if (!World)
    {
        return W.Error(Error);
    }
    const TArray<FClothComponentTarget> Matches = FindClothComponents(
        World,
        ActorName,
        ActorLabel,
        ComponentName);
    if(Matches.Num()>MaxMatchedComponents) { return W.Error(TEXT("Matched component count exceeds documented health bound4096"),TEXT("component_limit")); }
    TArray<TSharedPtr<FJsonValue>> Components;
    int32 RemainingPropertyBudget = MaxProperties;
    int32 ReturnedRuntimePropertyCount = 0;int64 ProjectionChars=0;
    for (int32 Index = 0; Index < Matches.Num() && Index < MaxResults; ++Index)
    {
        W.Capture(World,Matches[Index].Actor,Matches[Index].Component);if(!W.Admit()) { return W.Error(W.R.Error); }W.Observe(TEXT("inspect_component_snapshot"));
        TSharedPtr<FJsonObject> ComponentSnapshot = MakeClothComponentSnapshot(W,
            Matches[Index],
            ModelIndex,
            LodIndex,
            bIncludeRuntimeProperties,
            RemainingPropertyBudget);
        double ComponentReturnedPropertyCount = 0.0;
        if (ComponentSnapshot->TryGetNumberField(
                TEXT("returned_runtime_property_count"),
                ComponentReturnedPropertyCount))
        {
            const int32 ReturnedCount = FMath::Max(
                0,
                FMath::RoundToInt(ComponentReturnedPropertyCount));
            ReturnedRuntimePropertyCount += ReturnedCount;
            RemainingPropertyBudget = FMath::Max(0, RemainingPropertyBudget - ReturnedCount);
        }
        if(!W.Admit()||!W.R.Error.IsEmpty()) { W.Prefix->SetArrayField(TEXT("completed_component_snapshots"),Components);return W.Error(W.R.Error); }
        if(!FiniteJson(MakeShared<FJsonValueObject>(ComponentSnapshot.ToSharedRef()))) { return W.Error(TEXT("Component snapshot contains nonfinite JSON data"),TEXT("nonfinite_snapshot")); }
        auto TargetAfter=W.Projection();if(!Json(W.Before.ToSharedRef()).Equals(Json(TargetAfter),ESearchCase::CaseSensitive)) { W.Prefix->SetObjectField(TEXT("observer_changed_target"),TargetAfter);return W.Error(TEXT("Captured component projection changed during observer SDK dispatch"),TEXT("observer_projection_changed")); }
        ProjectionChars+=Json(ComponentSnapshot.ToSharedRef()).Len();if(ProjectionChars>MaxProjectionChars) { W.Prefix->SetArrayField(TEXT("completed_component_snapshots"),Components);return W.Error(TEXT("Component response projection budget exceeded"),TEXT("projection_limit")); }
        Components.Add(MakeShared<FJsonValueObject>(ComponentSnapshot));
    }

    TSharedPtr<FJsonObject> Data = MakeShared<FJsonObject>();
    Data->SetStringField(TEXT("world_context"), WorldContext);
    Data->SetNumberField(TEXT("match_count"), Matches.Num());
    Data->SetNumberField(TEXT("returned_count"), Components.Num());
    Data->SetBoolField(TEXT("truncated"), Matches.Num() > Components.Num());
    Data->SetNumberField(
        TEXT("returned_runtime_property_count"),
        ReturnedRuntimePropertyCount);
    Data->SetBoolField(
        TEXT("runtime_properties_budget_exhausted"),
        RemainingPropertyBudget == 0);
    Data->SetArrayField(TEXT("components"), Components);
    Data->SetStringField(
        TEXT("runtime_property_persistence"),
        TEXT("component-local overrides are transient and are intentionally not saved into the source cloth asset"));
    return Success(Data);
    };
    return W.Finish(Execute());
}

FString Configure(const FString& ParamsJson)
{
    FWork W(TEXT("configure_chaos_cloth_component"));if(!W.Enter(ParamsJson)) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);const auto Params=W.Params;
    const auto Execute=[&]() -> TSharedPtr<FJsonObject>
    {
    FJsonParamParseState ParamState;
    const FString ActorName = GetStringParam(Params, TEXT("actor_name"), ParamState);
    const FString ActorLabel = GetStringParam(Params, TEXT("actor_label"), ParamState);
    const FString ComponentName = GetStringParam(Params, TEXT("component_name"), ParamState);
    const int32 ModelIndex = GetIntParam(Params, TEXT("model_index"), 0, ParamState);
    const int32 LodIndex = GetIntParam(Params, TEXT("lod_index"), -1, ParamState);
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), true, ParamState);
    const bool bSaveLevel = GetBoolParam(Params, TEXT("save_level"), false, ParamState);W.R.bSaveRequested=bSaveLevel;
    const bool bAllowIntrinsicRecreate = GetBoolParam(
        Params,
        TEXT("allow_intrinsic_recreate"),
        false,
        ParamState);
    const FString RecreateMode = GetStringParam(
        Params,
        TEXT("recreate_simulation"),
        ParamState,
        TEXT("auto")).ToLower();
    const FString RequestedContext = GetStringParam(
        Params,
        TEXT("world_context"),
        ParamState,
        TEXT("auto"));
    if (!ParamState.IsValid())
    {
        return W.Error(ParamState.GetErrorMessage());
    }
    if (ActorName.IsEmpty() && ActorLabel.IsEmpty())
    {
        return W.Error(
            TEXT("configure_chaos_cloth_component requires actor_name or actor_label"));
    }
    if (!(RecreateMode == TEXT("auto") || RecreateMode == TEXT("always") || RecreateMode == TEXT("never")))
    {
        return W.Error(
            TEXT("recreate_simulation must be auto, always, or never"));
    }

    const TSharedPtr<FJsonObject>* SettingsPtr = nullptr;
    TSharedPtr<FJsonObject> ComponentSettings;
    if (Params.IsValid() && Params->HasField(TEXT("component_settings")) &&
        !Params->HasTypedField<EJson::Object>(TEXT("component_settings")))
    {
        return W.Error(
            TEXT("component_settings must be a JSON object when supplied"));
    }
    if (Params.IsValid() &&
        Params->TryGetObjectField(TEXT("component_settings"), SettingsPtr) && SettingsPtr)
    {
        ComponentSettings = *SettingsPtr;
    }
    const TArray<TSharedPtr<FJsonValue>>* UpdatesPtr = nullptr;
    TArray<TSharedPtr<FJsonValue>> EmptyUpdates;
    if (Params.IsValid() && Params->HasField(TEXT("property_updates")) &&
        !Params->HasTypedField<EJson::Array>(TEXT("property_updates")))
    {
        return W.Error(
            TEXT("property_updates must be a JSON array when supplied"));
    }
    const TArray<TSharedPtr<FJsonValue>>& UpdateValues =
        Params.IsValid() && Params->TryGetArrayField(TEXT("property_updates"), UpdatesPtr) && UpdatesPtr
            ? *UpdatesPtr
            : EmptyUpdates;
    for (int32 UpdateIndex = 0; UpdateIndex < UpdateValues.Num(); ++UpdateIndex)
    {
        if (!UpdateValues[UpdateIndex].IsValid() ||
            UpdateValues[UpdateIndex]->Type != EJson::Object)
        {
            return W.Error(FString::Printf(
                TEXT("property_updates[%d] must be an object"),
                UpdateIndex));
        }
    }
    if ((!ComponentSettings.IsValid() || ComponentSettings->Values.IsEmpty()) &&
        UpdateValues.IsEmpty())
    {
        return W.Error(
            TEXT("component_settings or property_updates must be provided"));
    }

    FString WorldContext;
    FString Error;
    W.Observe(TEXT("resolve_world"));UWorld* World = ResolveWorld(
        RequestedContext,
        WorldContext,
        Error);
    if (!World)
    {
        return W.Error(Error);
    }
    if (bSaveLevel && (WorldContext != TEXT("editor") || IsPlaySessionActive()))
    {
        return W.Error(
            TEXT("save_level is blocked for play worlds and while PIE/SIE is active"));
    }
    if (!bDryRun && WorldContext == TEXT("editor") && IsPlaySessionActive())
    {
        return W.Error(
            TEXT("Editor-world Chaos Cloth mutation is blocked while PIE/SIE is active; use world_context=play for transient tuning"));
    }

    const TArray<FClothComponentTarget> Matches = FindClothComponents(
        World,
        ActorName,
        ActorLabel,
        ComponentName);
    if (Matches.Num() != 1)
    {
        return W.Error(FString::Printf(
            TEXT("Cloth component selector must resolve exactly one component (matched %d)"),
            Matches.Num()));
    }
    const FClothComponentTarget Target = Matches[0];
    UChaosClothComponent* Component = Target.Component;W.Capture(World,Target.Actor,Component);if(!W.Admit()) { return W.Error(W.R.Error); }
    if (!Component->GetAsset())
    {
        return W.Error(
            TEXT("The selected Chaos Cloth component has no cloth asset"));
    }

    W.Observe(TEXT("validate_component_and_runtime_updates"));TArray<FString> ValidationErrors;
    bool bComponentRequiresRecreate = false;
    ValidateComponentSettings(
        ComponentSettings,
        ValidationErrors,
        bComponentRequiresRecreate);
    const bool bHasPersistentComponentSettings = HasPersistentComponentSettings(ComponentSettings);
    if (bSaveLevel && !bHasPersistentComponentSettings)
    {
        ValidationErrors.Add(
            TEXT("save_level has no durable effect for runtime property updates or one-shot teleport actions; include a persistent component setting or leave save_level=false"));
    }

    TArray<FValidatedRuntimePropertyUpdate> Updates;
    bool bPropertyRequiresRecreate = false;
    ValidateRuntimePropertyUpdates(
        Component,
        ModelIndex,
        LodIndex,
        UpdateValues,
        Updates,
        ValidationErrors,
        bPropertyRequiresRecreate);
    const bool bRequestedRequiresRecreate =
        bComponentRequiresRecreate || bPropertyRequiresRecreate;
    if (!ValidationErrors.IsEmpty())
    {
        return W.Error(FString::Join(ValidationErrors, TEXT("; ")));
    }

    const TSharedPtr<FJsonObject>* ExpectedPtr = nullptr;
    TSharedPtr<FJsonObject> Expected;
    if (Params->HasField(TEXT("expected_current")) &&
        !Params->HasTypedField<EJson::Object>(TEXT("expected_current")))
    {
        return W.Error(
            TEXT("expected_current must be a JSON object when supplied"));
    }
    if (Params->TryGetObjectField(TEXT("expected_current"), ExpectedPtr) && ExpectedPtr)
    {
        Expected = *ExpectedPtr;
    }
    TArray<FString> ExpectedMismatches;
    const TSharedPtr<FJsonObject> FlatSettingsBefore = MakeComponentSettingsFlatSnapshot(Component);if(FiniteJson(MakeShared<FJsonValueObject>(FlatSettingsBefore.ToSharedRef()))) { W.R.Data->SetObjectField(TEXT("configured_settings_before"),FlatSettingsBefore); }
    if (!CheckExpectedFields(Expected, FlatSettingsBefore, ExpectedMismatches))
    {
        return W.Error(FString::Printf(
            TEXT("expected_current mismatch: %s"),
            *FString::Join(ExpectedMismatches, TEXT("; "))));
    }

    const TSharedPtr<FJsonObject> VerifiableSettings =
        MakeVerifiableComponentSettings(ComponentSettings);
    TArray<FString> SettingsNoOpMismatches;
    const bool bComponentSettingsMatch = CheckExpectedFields(
        VerifiableSettings,
        FlatSettingsBefore,
        SettingsNoOpMismatches);

    bool bPersistentTeleportBefore = false;
    bool bPersistentResetBefore = false;
    const bool bPersistentTeleportStateReadable =
        ReadReflectedBool(Component, TEXT("bTeleport"), bPersistentTeleportBefore) &&
        ReadReflectedBool(Component, TEXT("bReset"), bPersistentResetBefore);
    const bool bResetTeleportAction = IsResetTeleportAction(ComponentSettings);
    const bool bOneShotTeleportAction = IsOneShotTeleportAction(ComponentSettings);
    if (bResetTeleportAction && !bPersistentTeleportStateReadable)
    {
        ValidationErrors.Add(TEXT("Unable to snapshot the persistent cloth teleport state"));
    }
    const bool bResetTeleportWouldChange = bResetTeleportAction &&
        (bPersistentTeleportBefore || bPersistentResetBefore);
    const bool bPersistentSettingsWouldChange =
        !bComponentSettingsMatch || bResetTeleportWouldChange;

    TArray<FValidatedRuntimePropertyUpdate> UpdatesToApply;
    bool bIntrinsicRuntimePropertyWouldChange = false;
    for (const FValidatedRuntimePropertyUpdate& Update : Updates)
    {
        if (!RuntimePropertyMatches(Component, ModelIndex, Update))
        {
            UpdatesToApply.Add(Update);
            bIntrinsicRuntimePropertyWouldChange |= Update.bIntrinsic;
        }
    }
    if (bIntrinsicRuntimePropertyWouldChange && !bAllowIntrinsicRecreate)
    {
        for (const FValidatedRuntimePropertyUpdate& Update : UpdatesToApply)
        {
            if (Update.bIntrinsic)
            {
                ValidationErrors.Add(FString::Printf(
                    TEXT("Runtime property '%s' is Intrinsic and would change; set allow_intrinsic_recreate=true to acknowledge proxy reconstruction"),
                    *Update.Name));
            }
        }
    }

    bool bGeometryScaleWouldChange = false;
    if (const TSharedPtr<FJsonValue>* GeometryValue =
            VerifiableSettings->Values.Find(TEXT("cloth_geometry_scale")))
    {
        TSharedPtr<FJsonObject> GeometryExpected = MakeShared<FJsonObject>();
        GeometryExpected->SetField(TEXT("cloth_geometry_scale"), *GeometryValue);
        TArray<FString> GeometryMismatches;
        bGeometryScaleWouldChange = !CheckExpectedFields(
            GeometryExpected,
            FlatSettingsBefore,
            GeometryMismatches);
    }

    const bool bRequiresRecreate =
        bGeometryScaleWouldChange || bIntrinsicRuntimePropertyWouldChange;
    const bool bRecreateRequested = RecreateMode == TEXT("always") ||
        (RecreateMode == TEXT("auto") && bRequiresRecreate);
    if (bRequiresRecreate && RecreateMode == TEXT("never"))
    {
        ValidationErrors.Add(TEXT("The effective changes require proxy reconstruction, but recreate_simulation is never"));
    }
    if (bRecreateRequested && !Component->IsRegistered())
    {
        ValidationErrors.Add(TEXT("Proxy reconstruction requires a registered Chaos Cloth component"));
    }
    if (bRecreateRequested && !Component->GetAsset()->HasValidClothSimulationModels())
    {
        ValidationErrors.Add(TEXT("Proxy reconstruction requires valid cloth simulation models"));
    }
    if (!bDryRun && bRecreateRequested && !IsClothPhysicsEnabledForRecreate())
    {
        ValidationErrors.Add(TEXT("Proxy reconstruction is unavailable while p.ClothPhysics=0"));
    }
    if (!ValidationErrors.IsEmpty())
    {
        return W.Error(FString::Join(ValidationErrors, TEXT("; ")));
    }
    const bool bWouldChange = bPersistentSettingsWouldChange ||
        !UpdatesToApply.IsEmpty() || bOneShotTeleportAction || bRecreateRequested;

    W.Observe(TEXT("observe_before_mutation"));const TSharedPtr<FJsonObject> BeforeMutationState = MakeComponentMutationState(
        Component,
        ModelIndex,
        ComponentSettings,
        Updates);
    const TSharedPtr<FJsonObject> Before = MakeClothComponentSnapshot(W,
        Target,
        ModelIndex,
        LodIndex,
        true,
        4096);
    if(!W.Admit()||!W.R.Error.IsEmpty()) { return W.Error(W.R.Error); }
    if(!FiniteJson(MakeShared<FJsonValueObject>(BeforeMutationState.ToSharedRef()))||!FiniteJson(MakeShared<FJsonValueObject>(Before.ToSharedRef()))) { return W.Error(TEXT("Nonfinite before snapshot rejected before writer"),TEXT("nonfinite_snapshot")); }
    if(Json(BeforeMutationState.ToSharedRef()).Len()>MaxProjectionChars||Json(Before.ToSharedRef()).Len()>MaxProjectionChars) { return W.Error(TEXT("Before projection budget exceeded before writer"),TEXT("before_projection_limit")); }
    W.Prefix->SetObjectField(TEXT("before_mutation_state"),BeforeMutationState);W.R.Data->SetBoolField(TEXT("runtime_changes_requested"),!UpdatesToApply.IsEmpty());W.R.Data->SetBoolField(TEXT("persistent_teleport_reset_would_change"),bResetTeleportWouldChange);W.R.Data->SetBoolField(TEXT("planned_application_supported"),!bResetTeleportWouldChange&&(UpdatesToApply.IsEmpty()||(Component->GetClass()==UChaosClothComponent::StaticClass()&&Component->IsRegistered()&&Component->GetAsset()->GetClass()==UChaosClothAsset::StaticClass()&&ModelIndex==0)));W.R.Data->SetBoolField(TEXT("runtime_apply_admission_not_executed_in_dry_run"),bDryRun&&!UpdatesToApply.IsEmpty());
    if (bDryRun)
    {
        TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
        Result->SetBoolField(TEXT("success"), true);
        Result->SetStringField(TEXT("component_path"), Component->GetPathName());
        Result->SetStringField(TEXT("world_context"), WorldContext);
        Result->SetBoolField(TEXT("requires_proxy_recreation"), bRequiresRecreate);
        Result->SetBoolField(TEXT("requested_requires_proxy_recreation"), bRequestedRequiresRecreate);
        Result->SetBoolField(TEXT("proxy_recreation_requested"), bRecreateRequested);
        Result->SetBoolField(TEXT("proxy_recreation_available"), IsClothPhysicsEnabledForRecreate());
        Result->SetBoolField(TEXT("would_change"), bWouldChange);
        Result->SetBoolField(TEXT("no_op"), !bWouldChange);
        Result->SetBoolField(TEXT("mutation_attempted"), false);
        Result->SetStringField(TEXT("transaction_scope"), TEXT("none"));
        Result->SetBoolField(TEXT("owned_transaction_cancelled"), false);
        Result->SetBoolField(TEXT("save_requested"), bSaveLevel);
        Result->SetBoolField(TEXT("save_attempted"), false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetStringField(TEXT("runtime_property_persistence"), TEXT("transient_component_override"));
        Result->SetStringField(
            TEXT("save_scope"),
            TEXT("serialized component settings only; runtime property overrides and one-shot teleport actions are transient"));
        AddMutationResultContract(
            Result,
            true,
            false,
            false,
            Before,
            Before,
            MakeDeltaCounts(false),
            true,
            TEXT("cloth_component_dry_run_snapshot"),
            {TEXT("unique_target"), TEXT("existing_property_keys"), TEXT("intrinsic_gate"), TEXT("expected_current_matches")});
        return Result;
    }

    if (!Updates.IsEmpty() &&
        !Component->GetClothOutfitInteractor(ModelIndex, NAME_None))
    {
        return W.Error(
            TEXT("The selected component has no initialized cloth property interactor"));
    }

    TArray<FRuntimePropertyRollbackSnapshot> RuntimeRollbackValues;
    TArray<FString> RollbackSnapshotErrors;
    if (!CaptureRuntimePropertyRollbackValues(
            Component,
            ModelIndex,
            UpdatesToApply,
            RuntimeRollbackValues,
            RollbackSnapshotErrors))
    {
        return W.Error(FString::Printf(
            TEXT("Unable to capture runtime rollback state: %s"),
            *FString::Join(RollbackSnapshotErrors, TEXT("; "))));
    }

    UPackage* const TargetPackage = Component->GetPackage();
    const bool bTargetPackageWasDirty = TargetPackage && TargetPackage->IsDirty();
    if (bSaveLevel && bPersistentSettingsWouldChange &&
        !IsDurablySavableObject(Component))
    {
        return W.Error(
            TEXT("save_level requires a non-transient component and owner in a durably savable package"));
    }
    if (bSaveLevel && bPersistentSettingsWouldChange &&
        (!TargetPackage || bTargetPackageWasDirty))
    {
        return W.Error(
            TEXT("save_level requires a clean target package so unrelated pre-existing edits are not persisted"));
    }
    const bool bHadProxyBefore = Component->GetClothSimulationProxy() != nullptr;
    const bool bNeededTeleportBefore = Component->NeedsTeleport();
    const bool bNeededResetBefore = Component->NeedsReset();

    W.R.Stage=TEXT("prepare_public_mutation_support");
    if(!UpdatesToApply.IsEmpty()&&!TCBeginRuntime(W,ModelIndex,RuntimeRollbackValues))return W.Error(W.R.Error);
    if(bResetTeleportWouldChange) { return W.Error(TEXT("Changing persistent teleport/reset to false requires restoring prior true flags on failure, but a public persistent-true setter is not confirmed; no writer was dispatched"),TEXT("persistent_teleport_restore_unsupported")); }
    TUniquePtr<FScopedTransaction> Transaction;
    bool bJoinedOuterTransaction = false;
    bool bOwnedTransactionCancelled = false;
    if (WorldContext == TEXT("editor") && bPersistentSettingsWouldChange)
    {
        bJoinedOuterTransaction = GEditor && GEditor->IsTransactionActive();
        if (!bJoinedOuterTransaction)
        {
            W.Write(TEXT("begin_owned_transaction"));Transaction = MakeUnique<FScopedTransaction>(
                NSLOCTEXT("CubelessMCP", "ConfigureChaosClothComponent", "Configure Chaos Cloth Component"));W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        }
        W.Write(TEXT("Modify_component"));W.Count(TEXT("Modify_calls"));Component->Modify();W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        if (Target.Actor)
        {
            W.Write(TEXT("Modify_actor"));W.Count(TEXT("Modify_calls"));Target.Actor->Modify();W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        }
    }

    if (bWouldChange)
    {
        WaitForCloth(W,Component);if(!W.Admit()) { return W.Error(W.R.Error); }
    }
    bool bPersistentMutationAttempted = false;
    if (bPersistentSettingsWouldChange)
    {
        ApplyComponentSettings(W,Component, ComponentSettings);if(!W.Admit()||!W.R.Error.IsEmpty()) { return W.Error(W.R.Error); }
        if (bResetTeleportAction)
        {
            ApplyComponentTeleportAction(W,Component, ComponentSettings);
        }
        bPersistentMutationAttempted = true;
    }
    const bool bRuntimeMutationAttempted=!UpdatesToApply.IsEmpty();
    for(const auto& Update:UpdatesToApply)if(!TCApplyRuntime(W,Update))return W.Error(W.R.Error);
    bool bRecreated = false;
    if (bRecreateRequested)
    {
        Recreate(W,Component);if(!W.Admit()) { return W.Error(W.R.Error); }
        bRecreated = Component->GetClothSimulationProxy() != nullptr;
    }
    if (WorldContext == TEXT("editor") && bPersistentMutationAttempted)
    {
        W.Write(TEXT("mark_component_package_dirty"));W.Count(TEXT("dirty_dispatch_calls"));Component->MarkPackageDirty();W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        if (Target.Actor)
        {
            W.Write(TEXT("mark_actor_package_dirty"));W.Count(TEXT("dirty_dispatch_calls"));Target.Actor->MarkPackageDirty();W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        }
    }

    W.Observe(TEXT("verify_component_settings_runtime_and_proxy"));TArray<FString> VerificationErrors;
    if (bRecreateRequested && !bRecreated)
    {
        W.NoteFailure(TEXT("recreate_proxy_readback"));VerificationErrors.Add(TEXT("Cloth simulation proxy reconstruction failed"));
    }
    const bool bSettingsVerified = CheckExpectedFields(
        VerifiableSettings,
        MakeComponentSettingsFlatSnapshot(Component),
        VerificationErrors);
    bool bPersistentTeleportVerified = true;
    if (bResetTeleportAction)
    {
        bPersistentTeleportVerified = VerifyTeleportAction(
            Component,
            ComponentSettings,
            VerificationErrors);
    }
    bool bPropertiesVerified = !W.Runtime||!W.Runtime->ReadbackFailed;if(!bPropertiesVerified)VerificationErrors.Add(TEXT("Actual exact runtime-cell setter readback did not match the planned typed value/flags"));
    for (const FValidatedRuntimePropertyUpdate& Update : Updates)
    {
        bPropertiesVerified &= VerifyRuntimePropertyUpdate(
            Component,
            ModelIndex,
            Update,
            VerificationErrors);
    }
    const bool bPreSaveVerified = bSettingsVerified && bPersistentTeleportVerified &&
        bPropertiesVerified && (!bRecreateRequested || bRecreated);
    if(!bPreSaveVerified) { W.NoteFailure(TEXT("verify_settings_runtime_proxy")); }
    const bool bSaveAttempted = bSaveLevel && bPreSaveVerified &&
        bPersistentMutationAttempted;
    const bool bSaved = bSaveAttempted && SaveExactPackage(W,TargetPackage);
    if(bSaveAttempted&&!bSaved) { W.NoteFailure(TEXT("save_exact_component_package")); }
    const bool bSaveVerified = !bSaveLevel || !bPersistentMutationAttempted ||
        (bSaveAttempted && bSaved);

    bool bOneShotTeleportApplied = false;
    bool bOneShotTeleportVerified = true;
    if (bOneShotTeleportAction && bPreSaveVerified && bSaveVerified)
    {
        ApplyComponentTeleportAction(W,Component, ComponentSettings);
        bOneShotTeleportApplied = true;if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        bOneShotTeleportVerified = VerifyTeleportAction(
            Component,
            ComponentSettings,
            VerificationErrors);
    }
    if(!bOneShotTeleportVerified) { W.NoteFailure(TEXT("verify_one_shot_teleport")); }
    const bool bVerified = bPreSaveVerified && bSaveVerified && bOneShotTeleportVerified;
    const bool bMutationAttempted = bPersistentMutationAttempted ||
        bRuntimeMutationAttempted || bRecreateRequested || bOneShotTeleportApplied;

    bool bRollbackAttempted = false;
    bool bRollbackSettingsVerified = false;
    bool bRollbackRuntimeVerified = false;
    bool bRollbackProxyPresenceRestored = false;
    bool bRollbackSimulationStateRestored = false;
    bool bRollbackProxyRestored = false;
    bool bRollbackTeleportVerified = false;
    bool bRollbackSaveAttempted = false;
    bool bRollbackSaved = false;
    bool bRollbackSucceeded = false;
    if (!bVerified && bMutationAttempted)
    {
        bRollbackAttempted = true;
        WaitForCloth(W,Component);if(!W.Admit()) { return W.Error(W.R.Error); }
        ApplyComponentSettings(W,Component, FlatSettingsBefore);if(!W.Admit()) { return W.Error(W.R.Error); }
        const bool bPersistentTeleportRestored = !bPersistentTeleportStateReadable ||
            RestorePersistentTeleport(W,Component,bPersistentTeleportBefore,bPersistentResetBefore);
        bRollbackRuntimeVerified = RestoreRuntimePropertyValues(
            W,Component,
            ModelIndex,
            RuntimeRollbackValues,
            VerificationErrors);if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        if (!bRecreateRequested)
        {
            bRollbackProxyPresenceRestored =
                (Component->GetClothSimulationProxy() != nullptr) == bHadProxyBefore;
            bRollbackSimulationStateRestored = bRollbackProxyPresenceRestored;
        }
        else if (bHadProxyBefore)
        {
            if (IsClothPhysicsEnabledForRecreate())
            {
                Recreate(W,Component);if(!W.Admit()) { return W.Error(W.R.Error); }
                bRollbackProxyPresenceRestored =
                    Component->GetClothSimulationProxy() != nullptr;
            }
            // RecreateClothSimulationProxy resets the previous proxy before creating a
            // replacement.  Restored settings can rebuild equivalent configuration,
            // but particle positions/velocities and other live simulation state cannot
            // be recovered through the public UE 5.6 API.
            bRollbackSimulationStateRestored = false;
        }
        else
        {
            ResetProxy(W,Component);if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
            bRollbackProxyPresenceRestored =
                Component->GetClothSimulationProxy() == nullptr;
            bRollbackSimulationStateRestored = bRollbackProxyPresenceRestored;
        }
        bRollbackProxyRestored =
            bRollbackProxyPresenceRestored && bRollbackSimulationStateRestored;
        TArray<FString> RollbackSettingsErrors;
        bRollbackSettingsVerified = CheckExpectedFields(
            FlatSettingsBefore,
            MakeComponentSettingsFlatSnapshot(Component),
            RollbackSettingsErrors) && bPersistentTeleportRestored;
        bRollbackTeleportVerified =
            Component->NeedsTeleport() == bNeededTeleportBefore &&
            Component->NeedsReset() == bNeededResetBefore;

        if (bPersistentMutationAttempted && !bSaveAttempted && TargetPackage &&
            bRollbackSettingsVerified)
        {
            W.Write(TEXT("rollback_package_dirty_flag"));W.Count(TEXT("dirty_dispatch_calls"));TargetPackage->SetDirtyFlag(bTargetPackageWasDirty);W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
        }
        if (bPersistentMutationAttempted && bSaveAttempted &&
            bRollbackSettingsVerified && bRollbackRuntimeVerified &&
            bRollbackProxyPresenceRestored && bRollbackTeleportVerified)
        {
            bRollbackSaveAttempted = true;
            bRollbackSaved = SaveExactPackage(W,TargetPackage,true);
        }
        const bool bRollbackDurable = !bPersistentMutationAttempted || !bSaveAttempted ||
            (bRollbackSaveAttempted && bRollbackSaved);
        bRollbackSucceeded = bRollbackSettingsVerified && bRollbackRuntimeVerified &&
            bRollbackProxyRestored && bRollbackTeleportVerified && bRollbackDurable;
        if (Transaction && bRollbackSettingsVerified)
        {
            W.Write(TEXT("cancel_owned_transaction"));Transaction->Cancel();W.Returned();if(W.Runtime&&!W.Admit())return W.Error(W.R.Error);
            bOwnedTransactionCancelled = true;
        }
        if (!bRollbackSettingsVerified)
        {
            VerificationErrors.Add(TEXT("Component settings rollback readback failed"));
        }
        if (!bRollbackProxyRestored)
        {
            VerificationErrors.Add(TEXT("Cloth simulation proxy rollback failed"));
        }
        if (!bRollbackTeleportVerified)
        {
            VerificationErrors.Add(TEXT("Teleport state rollback failed"));
        }
        if (bRollbackSaveAttempted && !bRollbackSaved)
        {
            VerificationErrors.Add(TEXT("Component rollback package save failed"));
        }
    }

    if(!W.Admit()) { return W.Error(W.R.Error); }W.Observe(TEXT("observe_after_mutation"));const TSharedPtr<FJsonObject> After = MakeClothComponentSnapshot(W,
        Target,
        ModelIndex,
        LodIndex,
        true,
        4096);
    const TSharedPtr<FJsonObject> AfterMutationState = MakeComponentMutationState(
        Component,
        ModelIndex,
        ComponentSettings,
        Updates);
    if(FiniteJson(MakeShared<FJsonValueObject>(AfterMutationState.ToSharedRef()))&&Json(AfterMutationState.ToSharedRef()).Len()<=MaxProjectionChars) { W.Prefix->SetObjectField(TEXT("after_mutation_state"),AfterMutationState); }else { W.Prefix->SetBoolField(TEXT("after_mutation_state_nonfinite"),true); }
    const bool bPartialTargetModified =
        !bVerified && bMutationAttempted && !bRollbackSucceeded;
    const bool bSuccessfulLifecycleChange = bVerified &&
        (bRecreated || bOneShotTeleportApplied);
    const bool bChanged = bPartialTargetModified || bSuccessfulLifecycleChange ||
        (!bRollbackSucceeded &&
         !JsonObjectToString(BeforeMutationState).Equals(JsonObjectToString(AfterMutationState),ESearchCase::CaseSensitive));

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bVerified);
    if (!bVerified)
    {
        Result->SetStringField(
            TEXT("error"),
            VerificationErrors.IsEmpty()
                ? TEXT("Chaos Cloth component save/readback verification failed")
                : FString::Join(VerificationErrors, TEXT("; ")));
    }
    Result->SetStringField(TEXT("component_path"), Component->GetPathName());
    Result->SetStringField(TEXT("world_context"), WorldContext);
    Result->SetBoolField(TEXT("proxy_recreated"), bRecreated);
    Result->SetBoolField(TEXT("proxy_existed_before"), bHadProxyBefore);
    Result->SetBoolField(TEXT("proxy_recreation_requested"), bRecreateRequested);
    Result->SetBoolField(TEXT("save_requested"), bSaveLevel);
    Result->SetBoolField(TEXT("save_attempted"), bSaveAttempted);
    Result->SetBoolField(TEXT("saved"), bSaved);
    Result->SetBoolField(
        TEXT("save_skipped_no_change"),
        bSaveLevel && !bPersistentMutationAttempted);
    Result->SetStringField(
        TEXT("target_package"),
        TargetPackage ? TargetPackage->GetName() : FString());
    Result->SetBoolField(
        TEXT("target_package_dirty"),
        TargetPackage && TargetPackage->IsDirty());
    Result->SetBoolField(TEXT("no_op"), !bWouldChange);
    Result->SetBoolField(TEXT("mutation_attempted"), bMutationAttempted);
    Result->SetStringField(
        TEXT("transaction_scope"),
        Transaction
            ? TEXT("owned")
            : (bJoinedOuterTransaction ? TEXT("joined_outer") : TEXT("none")));
    Result->SetBoolField(
        TEXT("owned_transaction_cancelled"),
        bOwnedTransactionCancelled);
    Result->SetBoolField(TEXT("one_shot_teleport_deferred_until_verified"), bOneShotTeleportAction);
    Result->SetBoolField(TEXT("one_shot_teleport_applied"), bOneShotTeleportApplied);
    Result->SetBoolField(TEXT("rollback_attempted"), bRollbackAttempted);
    Result->SetBoolField(TEXT("rollback_settings_verified"), bRollbackSettingsVerified);
    Result->SetBoolField(TEXT("rollback_runtime_verified"), bRollbackRuntimeVerified);
    Result->SetBoolField(
        TEXT("rollback_proxy_presence_restored"),
        bRollbackProxyPresenceRestored);
    Result->SetBoolField(
        TEXT("rollback_simulation_state_restored"),
        bRollbackSimulationStateRestored);
    Result->SetBoolField(TEXT("rollback_proxy_restored"), bRollbackProxyRestored);
    Result->SetBoolField(TEXT("rollback_teleport_verified"), bRollbackTeleportVerified);
    Result->SetBoolField(TEXT("rollback_save_attempted"), bRollbackSaveAttempted);
    Result->SetBoolField(TEXT("rollback_saved"), bRollbackSaved);
    Result->SetBoolField(TEXT("rollback_succeeded"), bRollbackSucceeded);
    Result->SetBoolField(TEXT("partial_target_modified"), bPartialTargetModified);
    Result->SetBoolField(TEXT("do_not_retry"), bPartialTargetModified);
    const bool bEditorUndoAvailable = bVerified && bPersistentMutationAttempted &&
        WorldContext == TEXT("editor");
    Result->SetStringField(
        TEXT("undo_scope"),
        bEditorUndoAvailable
            ? TEXT("serialized_component_settings_only")
            : TEXT("none"));
    Result->SetBoolField(TEXT("runtime_updates_included_in_undo"), false);
    Result->SetBoolField(
        TEXT("undo_resets_runtime_overrides_and_simulation"),
        bEditorUndoAvailable);
    Result->SetStringField(TEXT("runtime_property_persistence"), TEXT("transient_component_override"));
    Result->SetStringField(
        TEXT("change_basis"),
        TEXT("requested component settings, teleport state, targeted runtime property snapshots, and explicit proxy lifecycle actions"));
    Result->SetStringField(
        TEXT("save_scope"),
        TEXT("serialized component settings only; runtime property overrides and one-shot teleport actions are transient"));
    AddMutationResultContract(
        Result,
        false,
        bMutationAttempted,
        bChanged,
        Before,
        After,
        MakeDeltaCounts(bChanged, !bVerified),
        bVerified,
        TEXT("cloth_component_property_collection_readback"),
        {TEXT("unique_target"), TEXT("component_settings_match"), TEXT("runtime_properties_match"), TEXT("proxy_recreation_gate"), TEXT("save_gate"), TEXT("rollback_gate")});
    AlignMutationAttemptContract(Result, bMutationAttempted);
    return Result;
    };
    return W.Finish(Execute());
}
} // namespace Cubeless::UE58::ChaosClothComponent
