// Dedicated Chaos Solver2 domain. Public UE5.8 API only; all UObject access is on the game thread.
#include "Compatibility/UE58/CubelessChaosSolverSettingsApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Chaos/ChaosSolverActor.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "Engine/Level.h"
#include "Engine/World.h"
#include "EngineUtils.h"
#include "FileHelpers.h"
#include "HAL/IConsoleManager.h"
#include "Misc/Crc.h"
#include "Misc/PackageName.h"
#include "Misc/TransactionObjectEvent.h"
#include "PBDRigidsSolver.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/WeakObjectPtr.h"
#include <atomic>

namespace Cubeless::UE58::ChaosSolverSettings
{
namespace
{
using Variable::FResult;
bool Running=false;
constexpr int32 MaxJsonChars=1024*1024,MaxQueueTickets=65536,MaxActors=1000000;
bool Live(const UObject* O)
{ return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& A)
{ TArray<TSharedPtr<FJsonValue>> R;for(const auto& S:A) { R.Add(MakeShared<FJsonValueString>(S)); }return R; }
FString JsonObjectToString(const TSharedPtr<FJsonObject>& O)
{ FString S;if(O.IsValid()) { FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S)); }return S; }
struct FCaseSensitiveJsonKeyFuncs : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& K) { return FCrc::StrCrc32(*K); }
};
bool Parse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
    if(Text.IsEmpty()||Text.Len()>MaxJsonChars) { return false; }for(TCHAR C:Text) { if(C==0) { return false; } }
    auto Reader=TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]"));TArray<TSet<FString,FCaseSensitiveJsonKeyFuncs>> Keys;TArray<bool> Objects;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token))
    {
        if(Token==EJsonNotation::Error||++Slots>262144) { return false; }
        if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }
        if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>16) { return false; } }
        else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }
    }
    TArray<TSharedPtr<FJsonValue>> V;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+Text+TEXT("]")),V)||V.Num()!=1||!V[0].IsValid()||V[0]->Type!=EJson::Object) { return false; }Out=V[0]->AsObject();return Out.IsValid();
}

// BEGIN_LEGACY_CONTRACT_HELPERS
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

    // UE5.8 string TryGetBool always succeeds, even for unrelated enum names.
    // Compare typed strings before bool coercion so canonical enum/expected-current checks remain meaningful.
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

    FString ExpectedString;
    FString ActualString;
    if (Expected->TryGetString(ExpectedString) && Actual->TryGetString(ActualString))
    {
        return ExpectedString == ActualString;
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
        for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : (*ExpectedObject)->Values)
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

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Expected->Values)
    {
        const TSharedPtr<FJsonValue> ActualValue = Actual->TryGetField(FStringView(Pair.Key));
        if (!ActualValue.IsValid())
        {
            OutMismatches.Add(FString::Printf(TEXT("%s is not a supported/readable field"), *Pair.Key));
        }
        else if (!JsonValueMatches(Pair.Value, ActualValue))
        {
            OutMismatches.Add(FString::Printf(TEXT("%s does not match expected_current"), *Pair.Key));
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
    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : FieldSelector->Values)
    {
        if (const TSharedPtr<FJsonValue> Value = Source->TryGetField(FStringView(Pair.Key)); Value.IsValid())
        {
            Result->SetField(Pair.Key, Value);
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

template <typename TActor>
TArray<TActor*> FindActors(UWorld* World, const FString& ActorName, const FString& ActorLabel)
{
    TArray<TActor*> Result;
    if (!World)
    {
        return Result;
    }
    for (TActorIterator<TActor> It(World); It; ++It)
    {
        if (ActorMatches(*It, ActorName, ActorLabel))
        {
            Result.Add(*It);
        }
    }
    return Result;
}

FString ClusterUnionMethodToString(EClusterUnionMethod Value)
{
    if (const UEnum* Enum = StaticEnum<EClusterUnionMethod>())
    {
        return Enum->GetNameStringByValue(static_cast<int64>(Value));
    }
    return FString::FromInt(static_cast<int32>(Value));
}

TSharedPtr<FJsonObject> MakeCollisionFilterSnapshot(const FSolverCollisionFilterSettings& Filter)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("enabled"), Filter.FilterEnabled);
    Result->SetNumberField(TEXT("min_mass"), Filter.MinMass);
    Result->SetNumberField(TEXT("min_speed"), Filter.MinSpeed);
    Result->SetNumberField(TEXT("min_impulse"), Filter.MinImpulse);
    return Result;
}

TSharedPtr<FJsonObject> MakeBreakingFilterSnapshot(const FSolverBreakingFilterSettings& Filter)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("enabled"), Filter.FilterEnabled);
    Result->SetNumberField(TEXT("min_mass"), Filter.MinMass);
    Result->SetNumberField(TEXT("min_speed"), Filter.MinSpeed);
    Result->SetNumberField(TEXT("min_volume"), Filter.MinVolume);
    return Result;
}

TSharedPtr<FJsonObject> MakeTrailingFilterSnapshot(const FSolverTrailingFilterSettings& Filter)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("enabled"), Filter.FilterEnabled);
    Result->SetNumberField(TEXT("min_mass"), Filter.MinMass);
    Result->SetNumberField(TEXT("min_speed"), Filter.MinSpeed);
    Result->SetNumberField(TEXT("min_volume"), Filter.MinVolume);
    return Result;
}

TSharedPtr<FJsonObject> MakeSolverCVarOverrideSnapshot(
    const TCHAR* CVarName,
    double ConfiguredValue,
    bool bInteger)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("cvar_name"), CVarName);
    Result->SetStringField(TEXT("scope"), TEXT("global_process"));
    Result->SetNumberField(TEXT("configured_value"), ConfiguredValue);

    IConsoleVariable* CVar = IConsoleManager::Get().FindConsoleVariable(CVarName);
    Result->SetBoolField(TEXT("registered"), CVar != nullptr);
    if (!CVar)
    {
        Result->SetField(TEXT("raw_override_value"), MakeShared<FJsonValueNull>());
        Result->SetBoolField(TEXT("active"), false);
        Result->SetNumberField(TEXT("effective_value_inferred"), ConfiguredValue);
        Result->SetStringField(TEXT("effective_value_source"), TEXT("actor_configuration"));
        return Result;
    }

    const double OverrideValue = bInteger
        ? static_cast<double>(CVar->GetInt())
        : static_cast<double>(CVar->GetFloat());
    const bool bActive = OverrideValue >= 0.0;
    Result->SetNumberField(TEXT("raw_override_value"), OverrideValue);
    Result->SetBoolField(TEXT("active"), bActive);
    Result->SetNumberField(
        TEXT("effective_value_inferred"),
        bActive ? OverrideValue : ConfiguredValue);
    Result->SetStringField(
        TEXT("effective_value_source"),
        bActive ? TEXT("global_console_variable") : TEXT("actor_configuration"));
    Result->SetStringField(
        TEXT("inference_basis"),
        TEXT("UE_5.8_FPBDRigidsSolver_ApplyConfig_then_ApplyCVars"));
    return Result;
}

TSharedPtr<FJsonObject> MakeSolverCVarOverridesSnapshot(
    const FChaosSolverConfiguration& Config)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetObjectField(
        TEXT("position_iterations"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Iterations.Position"),
            Config.PositionIterations,
            true));
    Result->SetObjectField(
        TEXT("velocity_iterations"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Iterations.Velocity"),
            Config.VelocityIterations,
            true));
    Result->SetObjectField(
        TEXT("projection_iterations"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Iterations.Projection"),
            Config.ProjectionIterations,
            true));
    Result->SetObjectField(
        TEXT("collision_cull_distance"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Collision.CullDistance"),
            Config.CollisionCullDistance,
            false));
    Result->SetObjectField(
        TEXT("collision_max_push_out_velocity"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Collision.MaxPushOutVelocity"),
            Config.CollisionMaxPushOutVelocity,
            false));
    Result->SetObjectField(
        TEXT("collision_initial_overlap_depenetration_velocity"),
        MakeSolverCVarOverrideSnapshot(
            TEXT("p.Chaos.Solver.Collision.DepenetrationVelocity"),
            Config.CollisionInitialOverlapDepenetrationVelocity,
            false));
    return Result;
}

TSharedPtr<FJsonObject> MakeSolverSettingsSnapshot(const AChaosSolverActor* Actor, bool bIncludeRuntimeState)
{
    TSharedPtr<FJsonObject> Settings = MakeShared<FJsonObject>();
    if (!Actor)
    {
        return Settings;
    }

    const FChaosSolverConfiguration& Config = Actor->Properties;
    Settings->SetNumberField(TEXT("position_iterations"), Config.PositionIterations);
    Settings->SetNumberField(TEXT("velocity_iterations"), Config.VelocityIterations);
    Settings->SetNumberField(TEXT("projection_iterations"), Config.ProjectionIterations);
    Settings->SetNumberField(TEXT("collision_margin_fraction"), Config.CollisionMarginFraction);
    Settings->SetNumberField(TEXT("collision_margin_max"), Config.CollisionMarginMax);
    Settings->SetNumberField(TEXT("collision_cull_distance"), Config.CollisionCullDistance);
    Settings->SetNumberField(TEXT("collision_max_push_out_velocity"), Config.CollisionMaxPushOutVelocity);
    Settings->SetNumberField(
        TEXT("collision_initial_overlap_depenetration_velocity"),
        Config.CollisionInitialOverlapDepenetrationVelocity);
    Settings->SetNumberField(TEXT("cluster_connection_factor"), Config.ClusterConnectionFactor);
    Settings->SetStringField(
        TEXT("cluster_union_connection_type"),
        ClusterUnionMethodToString(Config.ClusterUnionConnectionType));
    Settings->SetNumberField(
        TEXT("per_advance_breaks_allowed"),
        Config.DestructionSettings.PerAdvanceBreaksAllowed);
    Settings->SetNumberField(
        TEXT("per_advance_breaks_reschedule_limit"),
        Config.DestructionSettings.PerAdvanceBreaksRescheduleLimit);
    Settings->SetNumberField(
        TEXT("clustering_particle_release_throttling_min_count"),
        Config.DestructionSettings.ClusteringParticleReleaseThrottlingMinCount);
    Settings->SetNumberField(
        TEXT("clustering_particle_release_throttling_max_count"),
        Config.DestructionSettings.ClusteringParticleReleaseThrottlingMaxCount);
    Settings->SetBoolField(
        TEXT("optimize_for_runtime_memory"),
        Config.DestructionSettings.bOptimizeForRuntimeMemory);
    Settings->SetBoolField(TEXT("generate_collision_data"), Config.bGenerateCollisionData);
    Settings->SetBoolField(TEXT("generate_break_data"), Config.bGenerateBreakData);
    Settings->SetBoolField(TEXT("generate_trailing_data"), Config.bGenerateTrailingData);
    Settings->SetObjectField(TEXT("collision_filter"), MakeCollisionFilterSnapshot(Config.CollisionFilterSettings));
    Settings->SetObjectField(TEXT("breaking_filter"), MakeBreakingFilterSnapshot(Config.BreakingFilterSettings));
    Settings->SetObjectField(TEXT("trailing_filter"), MakeTrailingFilterSnapshot(Config.TrailingFilterSettings));

    if (bIncludeRuntimeState)
    {
        TSharedPtr<FJsonObject> Runtime = MakeShared<FJsonObject>();
        Chaos::FPhysicsSolver* Solver = Actor->GetSolver();
        Runtime->SetBoolField(TEXT("solver_present"), Solver != nullptr);
        if (Solver)
        {
            Runtime->SetBoolField(TEXT("effective_deterministic"), Solver->IsDetemerministic());
        }
        else
        {
            Runtime->SetField(TEXT("effective_deterministic"), MakeShared<FJsonValueNull>());
        }
        Runtime->SetArrayField(
            TEXT("stored_but_not_applied_by_fpbd_rigids_solver_apply_config"),
            StringsToJson({TEXT("collision_margin_fraction"), TEXT("collision_margin_max")}));
        Runtime->SetObjectField(
            TEXT("global_cvar_overrides"),
            MakeSolverCVarOverridesSnapshot(Config));
        Runtime->SetStringField(
            TEXT("global_cvar_override_note"),
            TEXT("Read-only diagnostic: active non-negative CVars override actor configuration for every solver when UE 5.8 ApplyCVars runs."));
        Settings->SetObjectField(TEXT("runtime"), Runtime);
    }
    return Settings;
}

TSharedPtr<FJsonObject> MakeSolverActorSnapshot(AChaosSolverActor* Actor, bool bIncludeRuntimeState)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetStringField(TEXT("actor_name"), Actor ? Actor->GetName() : FString());
    Result->SetStringField(TEXT("actor_label"), Actor ? Actor->GetActorLabel() : FString());
    Result->SetStringField(TEXT("actor_path"), Actor ? Actor->GetPathName() : FString());
    Result->SetStringField(
        TEXT("level_package"),
        Actor && Actor->GetLevel() && Actor->GetLevel()->GetPackage()
            ? Actor->GetLevel()->GetPackage()->GetName()
            : FString());
    Result->SetStringField(
        TEXT("world_package"),
        Actor && Actor->GetWorld() && Actor->GetWorld()->GetPackage()
            ? Actor->GetWorld()->GetPackage()->GetName()
            : FString());
    Result->SetStringField(
        TEXT("target_package"),
        Actor && Actor->GetPackage() ? Actor->GetPackage()->GetName() : FString());
    Result->SetBoolField(
        TEXT("target_package_dirty"),
        Actor && Actor->GetPackage() && Actor->GetPackage()->IsDirty());
    Result->SetObjectField(TEXT("settings"), MakeSolverSettingsSnapshot(Actor, bIncludeRuntimeState));
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

bool ApplySolverSettingPatch(
    const TSharedPtr<FJsonObject>& Patch,
    FChaosSolverConfiguration& InOutConfig,
    TArray<FString>& OutErrors)
{
    if (!Patch.IsValid() || Patch->Values.IsEmpty())
    {
        OutErrors.Add(TEXT("settings must be a non-empty JSON object"));
        return false;
    }

    auto ReadNonNegativeInt = [&OutErrors](
        const FString& Name,
        const TSharedPtr<FJsonValue>& Value,
        int32& OutValue,
        int32 Maximum = TNumericLimits<int32>::Max())
    {
        double Number = 0.0;
        if (!ReadNumber(Value, Number) || !FMath::IsFinite(Number) ||
            Number != FMath::TruncToDouble(Number))
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be an integer"), *Name));
            return;
        }
        if (Number < 0.0 || Number > static_cast<double>(Maximum))
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be in [0, %d]"), *Name, Maximum));
            return;
        }
        OutValue = static_cast<int32>(Number);
    };

    auto ReadThrottleInt = [&OutErrors](
        const FString& Name,
        const TSharedPtr<FJsonValue>& Value,
        int32& OutValue)
    {
        double Number = 0.0;
        if (!ReadNumber(Value, Number) || !FMath::IsFinite(Number) ||
            Number != FMath::TruncToDouble(Number))
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be an integer"), *Name));
            return;
        }
        if (Number < -1.0 || Number > static_cast<double>(TNumericLimits<int32>::Max()))
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be -1 or a non-negative integer"), *Name));
            return;
        }
        OutValue = static_cast<int32>(Number);
    };

    auto ReadNonNegativeFloat = [&OutErrors](
        const FString& Name,
        const TSharedPtr<FJsonValue>& Value,
        float& OutValue)
    {
        double Number = 0.0;
        if (!ReadNumber(Value, Number) || !IsFiniteFloatRange(Number) || Number < 0.0)
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be a finite non-negative number"), *Name));
            return;
        }
        OutValue = static_cast<float>(Number);
    };

    auto ReadSimpleBool = [&OutErrors](
        const FString& Name,
        const TSharedPtr<FJsonValue>& Value,
        bool& bOutValue)
    {
        if (!ReadBool(Value, bOutValue))
        {
            OutErrors.Add(FString::Printf(TEXT("%s must be a boolean"), *Name));
        }
    };

    for (const TPair<FString, TSharedPtr<FJsonValue>>& Pair : Patch->Values)
    {
        const FString& Name = Pair.Key;
        const TSharedPtr<FJsonValue>& Value = Pair.Value;
        if (Name == TEXT("position_iterations"))
        {
            ReadNonNegativeInt(Name, Value, InOutConfig.PositionIterations, 255);
        }
        else if (Name == TEXT("velocity_iterations"))
        {
            ReadNonNegativeInt(Name, Value, InOutConfig.VelocityIterations, 255);
        }
        else if (Name == TEXT("projection_iterations"))
        {
            ReadNonNegativeInt(Name, Value, InOutConfig.ProjectionIterations, 255);
        }
        else if (Name == TEXT("collision_cull_distance"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.CollisionCullDistance);
        }
        else if (Name == TEXT("collision_max_push_out_velocity"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.CollisionMaxPushOutVelocity);
        }
        else if (Name == TEXT("collision_initial_overlap_depenetration_velocity"))
        {
            double Number = 0.0;
            if (!ReadNumber(Value, Number) || !IsFiniteFloatRange(Number) ||
                (Number < 0.0 && Number != -1.0))
            {
                OutErrors.Add(FString::Printf(
                    TEXT("%s must be -1 or a finite non-negative number"),
                    *Name));
            }
            else
            {
                InOutConfig.CollisionInitialOverlapDepenetrationVelocity = static_cast<float>(Number);
            }
        }
        else if (Name == TEXT("cluster_connection_factor"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.ClusterConnectionFactor);
        }
        else if (Name == TEXT("cluster_union_connection_type"))
        {
            FString EnumName;
            if (!ReadString(Value, EnumName))
            {
                OutErrors.Add(TEXT("cluster_union_connection_type must be an enum name string"));
            }
            else if (const UEnum* Enum = StaticEnum<EClusterUnionMethod>())
            {
                const int64 EnumValue = Enum->GetValueByNameString(EnumName, EGetByNameFlags::None);
                if (EnumValue == INDEX_NONE)
                {
                    OutErrors.Add(FString::Printf(
                        TEXT("Unknown cluster_union_connection_type '%s'"),
                        *EnumName));
                }
                else
                {
                    InOutConfig.ClusterUnionConnectionType = static_cast<EClusterUnionMethod>(EnumValue);
                }
            }
        }
        else if (Name == TEXT("per_advance_breaks_allowed"))
        {
            ReadNonNegativeInt(Name, Value, InOutConfig.DestructionSettings.PerAdvanceBreaksAllowed);
        }
        else if (Name == TEXT("per_advance_breaks_reschedule_limit"))
        {
            ReadNonNegativeInt(Name, Value, InOutConfig.DestructionSettings.PerAdvanceBreaksRescheduleLimit);
        }
        else if (Name == TEXT("clustering_particle_release_throttling_min_count"))
        {
            ReadThrottleInt(
                Name,
                Value,
                InOutConfig.DestructionSettings.ClusteringParticleReleaseThrottlingMinCount);
        }
        else if (Name == TEXT("clustering_particle_release_throttling_max_count"))
        {
            ReadThrottleInt(
                Name,
                Value,
                InOutConfig.DestructionSettings.ClusteringParticleReleaseThrottlingMaxCount);
        }
        else if (Name == TEXT("optimize_for_runtime_memory"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.DestructionSettings.bOptimizeForRuntimeMemory);
        }
        else if (Name == TEXT("generate_collision_data"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.bGenerateCollisionData);
        }
        else if (Name == TEXT("generate_break_data"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.bGenerateBreakData);
        }
        else if (Name == TEXT("generate_trailing_data"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.bGenerateTrailingData);
        }
        else if (Name == TEXT("collision_filter_enabled"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.CollisionFilterSettings.FilterEnabled);
        }
        else if (Name == TEXT("collision_filter_min_mass"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.CollisionFilterSettings.MinMass);
        }
        else if (Name == TEXT("collision_filter_min_speed"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.CollisionFilterSettings.MinSpeed);
        }
        else if (Name == TEXT("collision_filter_min_impulse"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.CollisionFilterSettings.MinImpulse);
        }
        else if (Name == TEXT("breaking_filter_enabled"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.BreakingFilterSettings.FilterEnabled);
        }
        else if (Name == TEXT("breaking_filter_min_mass"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.BreakingFilterSettings.MinMass);
        }
        else if (Name == TEXT("breaking_filter_min_speed"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.BreakingFilterSettings.MinSpeed);
        }
        else if (Name == TEXT("breaking_filter_min_volume"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.BreakingFilterSettings.MinVolume);
        }
        else if (Name == TEXT("trailing_filter_enabled"))
        {
            ReadSimpleBool(Name, Value, InOutConfig.TrailingFilterSettings.FilterEnabled);
        }
        else if (Name == TEXT("trailing_filter_min_mass"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.TrailingFilterSettings.MinMass);
        }
        else if (Name == TEXT("trailing_filter_min_speed"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.TrailingFilterSettings.MinSpeed);
        }
        else if (Name == TEXT("trailing_filter_min_volume"))
        {
            ReadNonNegativeFloat(Name, Value, InOutConfig.TrailingFilterSettings.MinVolume);
        }
        else if (Name == TEXT("runtime_deterministic"))
        {
            OutErrors.Add(
                TEXT("runtime_deterministic is inspection-only: UE 5.8 does not persist it on AChaosSolverActor, and an unscoped toggle cannot guarantee restoration after abort/world loss"));
        }
        else if (Name == TEXT("collision_margin_fraction") || Name == TEXT("collision_margin_max"))
        {
            OutErrors.Add(FString::Printf(
                TEXT("%s is intentionally read-only: UE 5.8 FPBDRigidsSolver::ApplyConfig does not apply it"),
                *Name));
        }
        else
        {
            OutErrors.Add(FString::Printf(TEXT("Unsupported Chaos solver setting '%s'"), *Name));
        }
    }

    const int32 MinThrottle = InOutConfig.DestructionSettings.ClusteringParticleReleaseThrottlingMinCount;
    const int32 MaxThrottle = InOutConfig.DestructionSettings.ClusteringParticleReleaseThrottlingMaxCount;
    if (MinThrottle >= 0 && MaxThrottle >= 0 && MinThrottle > MaxThrottle)
    {
        OutErrors.Add(TEXT("clustering_particle_release_throttling_min_count cannot exceed max_count"));
    }
    return OutErrors.IsEmpty();
}

TSharedPtr<FJsonObject> MakeFlatSolverConfigurationSnapshot(
    const FChaosSolverConfiguration& Config)
{
    TSharedPtr<FJsonObject> Flat = MakeShared<FJsonObject>();
    Flat->SetNumberField(TEXT("position_iterations"), Config.PositionIterations);
    Flat->SetNumberField(TEXT("velocity_iterations"), Config.VelocityIterations);
    Flat->SetNumberField(TEXT("projection_iterations"), Config.ProjectionIterations);
    Flat->SetNumberField(TEXT("collision_margin_fraction"), Config.CollisionMarginFraction);
    Flat->SetNumberField(TEXT("collision_margin_max"), Config.CollisionMarginMax);
    Flat->SetNumberField(TEXT("collision_cull_distance"), Config.CollisionCullDistance);
    Flat->SetNumberField(TEXT("collision_max_push_out_velocity"), Config.CollisionMaxPushOutVelocity);
    Flat->SetNumberField(
        TEXT("collision_initial_overlap_depenetration_velocity"),
        Config.CollisionInitialOverlapDepenetrationVelocity);
    Flat->SetNumberField(TEXT("cluster_connection_factor"), Config.ClusterConnectionFactor);
    Flat->SetStringField(
        TEXT("cluster_union_connection_type"),
        ClusterUnionMethodToString(Config.ClusterUnionConnectionType));
    Flat->SetNumberField(TEXT("per_advance_breaks_allowed"), Config.DestructionSettings.PerAdvanceBreaksAllowed);
    Flat->SetNumberField(
        TEXT("per_advance_breaks_reschedule_limit"),
        Config.DestructionSettings.PerAdvanceBreaksRescheduleLimit);
    Flat->SetNumberField(
        TEXT("clustering_particle_release_throttling_min_count"),
        Config.DestructionSettings.ClusteringParticleReleaseThrottlingMinCount);
    Flat->SetNumberField(
        TEXT("clustering_particle_release_throttling_max_count"),
        Config.DestructionSettings.ClusteringParticleReleaseThrottlingMaxCount);
    Flat->SetBoolField(TEXT("optimize_for_runtime_memory"), Config.DestructionSettings.bOptimizeForRuntimeMemory);
    Flat->SetBoolField(TEXT("generate_collision_data"), Config.bGenerateCollisionData);
    Flat->SetBoolField(TEXT("collision_filter_enabled"), Config.CollisionFilterSettings.FilterEnabled);
    Flat->SetNumberField(TEXT("collision_filter_min_mass"), Config.CollisionFilterSettings.MinMass);
    Flat->SetNumberField(TEXT("collision_filter_min_speed"), Config.CollisionFilterSettings.MinSpeed);
    Flat->SetNumberField(TEXT("collision_filter_min_impulse"), Config.CollisionFilterSettings.MinImpulse);
    Flat->SetBoolField(TEXT("generate_break_data"), Config.bGenerateBreakData);
    Flat->SetBoolField(TEXT("breaking_filter_enabled"), Config.BreakingFilterSettings.FilterEnabled);
    Flat->SetNumberField(TEXT("breaking_filter_min_mass"), Config.BreakingFilterSettings.MinMass);
    Flat->SetNumberField(TEXT("breaking_filter_min_speed"), Config.BreakingFilterSettings.MinSpeed);
    Flat->SetNumberField(TEXT("breaking_filter_min_volume"), Config.BreakingFilterSettings.MinVolume);
    Flat->SetBoolField(TEXT("generate_trailing_data"), Config.bGenerateTrailingData);
    Flat->SetBoolField(TEXT("trailing_filter_enabled"), Config.TrailingFilterSettings.FilterEnabled);
    Flat->SetNumberField(TEXT("trailing_filter_min_mass"), Config.TrailingFilterSettings.MinMass);
    Flat->SetNumberField(TEXT("trailing_filter_min_speed"), Config.TrailingFilterSettings.MinSpeed);
    Flat->SetNumberField(TEXT("trailing_filter_min_volume"), Config.TrailingFilterSettings.MinVolume);
    return Flat;
}

TSharedPtr<FJsonObject> MakeFlatSolverExpectedSnapshot(const AChaosSolverActor* Actor)
{
    if (!Actor)
    {
        return MakeShared<FJsonObject>();
    }
    TSharedPtr<FJsonObject> Flat = MakeFlatSolverConfigurationSnapshot(Actor->Properties);
    if (Chaos::FPhysicsSolver* Solver = Actor->GetSolver())
    {
        Flat->SetBoolField(TEXT("runtime_deterministic"), Solver->IsDetemerministic());
    }
    else
    {
        Flat->SetField(TEXT("runtime_deterministic"), MakeShared<FJsonValueNull>());
    }
    return Flat;
}

TArray<FString> GetWritableSolverSettingNames()
{
    return {
        TEXT("position_iterations"),
        TEXT("velocity_iterations"),
        TEXT("projection_iterations"),
        TEXT("collision_cull_distance"),
        TEXT("collision_max_push_out_velocity"),
        TEXT("collision_initial_overlap_depenetration_velocity"),
        TEXT("cluster_connection_factor"),
        TEXT("cluster_union_connection_type"),
        TEXT("per_advance_breaks_allowed"),
        TEXT("per_advance_breaks_reschedule_limit"),
        TEXT("clustering_particle_release_throttling_min_count"),
        TEXT("clustering_particle_release_throttling_max_count"),
        TEXT("optimize_for_runtime_memory"),
        TEXT("generate_collision_data"),
        TEXT("collision_filter_enabled"),
        TEXT("collision_filter_min_mass"),
        TEXT("collision_filter_min_speed"),
        TEXT("collision_filter_min_impulse"),
        TEXT("generate_break_data"),
        TEXT("breaking_filter_enabled"),
        TEXT("breaking_filter_min_mass"),
        TEXT("breaking_filter_min_speed"),
        TEXT("breaking_filter_min_volume"),
        TEXT("generate_trailing_data"),
        TEXT("trailing_filter_enabled"),
        TEXT("trailing_filter_min_mass"),
        TEXT("trailing_filter_min_speed"),
        TEXT("trailing_filter_min_volume")};
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



void AddLegacyMutationResult(
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
// END_LEGACY_CONTRACT_HELPERS

struct FLifetime { std::atomic<bool> Closing{false}; };
struct FQueueTicket
{
    FString Id;std::atomic<int32> Phase{0}; // 0 queued,1 entered,2 ApplyConfig returned,3 cancelled before ApplyConfig.
    TSharedRef<FLifetime,ESPMode::ThreadSafe> Lifetime;
    FChaosSolverConfiguration Configuration;
    FQueueTicket(const FChaosSolverConfiguration& C,const TSharedRef<FLifetime,ESPMode::ThreadSafe>& L):Id(FGuid::NewGuid().ToString(EGuidFormats::DigitsWithHyphensLower)),Lifetime(L),Configuration(C) {}
};
TSharedRef<FLifetime,ESPMode::ThreadSafe> Lifetime=MakeShared<FLifetime,ESPMode::ThreadSafe>();
TArray<TWeakPtr<FQueueTicket,ESPMode::ThreadSafe>> QueueTickets;
TMap<TWeakObjectPtr<AChaosSolverActor>,TWeakPtr<FQueueTicket,ESPMode::ThreadSafe>> LatestQueues;
TSet<TWeakObjectPtr<AChaosSolverActor>> UndoActors;
FDelegateHandle UndoHandle;
int32 UndoQueueAttempts=0,UndoQueueEnqueued=0;
void Prune()
{
    QueueTickets.RemoveAll([](const auto& W){ return !W.IsValid(); });
    for(auto It=LatestQueues.CreateIterator();It;++It) { if(!It.Key().IsValid()) { It.RemoveCurrent(); } }
    for(auto It=UndoActors.CreateIterator();It;++It) { if(!It->IsValid()) { It.RemoveCurrent(); } }
}
TSharedRef<FJsonObject> QueueSnapshot(const TSharedPtr<FQueueTicket,ESPMode::ThreadSafe>& T)
{
    auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("ticket_available"),T.IsValid());J->SetBoolField(TEXT("effective_physics_configuration_verified"),false);J->SetBoolField(TEXT("simulation_frame_completion_verified"),false);
    if(T.IsValid()) { const int32 P=T->Phase.load(std::memory_order_acquire);J->SetStringField(TEXT("ticket_id"),T->Id);J->SetNumberField(TEXT("observed_phase"),P);J->SetBoolField(TEXT("callback_entered_observed"),P==1||P==2);J->SetBoolField(TEXT("callback_apply_config_returned_observed"),P==2);J->SetBoolField(TEXT("cancelled_before_apply_observed"),P==3); }
    return J;
}
TSharedPtr<FQueueTicket,ESPMode::ThreadSafe> Enqueue(AChaosSolverActor* Actor)
{
    if(!IsInGameThread()||Lifetime->Closing.load(std::memory_order_acquire)||!Live(Actor)) { return {}; }Prune();if(QueueTickets.Num()>=MaxQueueTickets) { return {}; }
    Chaos::FPhysicsSolver* Solver=Actor->GetSolver();if(!Solver) { return {}; }
    // The SDK owns this queue/solver lifetime. No UObject is captured or touched on the physics thread.
    auto T=MakeShared<FQueueTicket,ESPMode::ThreadSafe>(Actor->Properties,Lifetime);QueueTickets.Add(T);LatestQueues.Add(Actor,T);
    Solver->EnqueueCommandImmediate([Solver,T]()
    {
        int32 Queued=0;if(!T->Phase.compare_exchange_strong(Queued,1,std::memory_order_acq_rel)) { return; }
        if(T->Lifetime->Closing.load(std::memory_order_acquire)) { T->Phase.store(3,std::memory_order_release);return; }
        Solver->ApplyConfig(T->Configuration);T->Phase.store(2,std::memory_order_release);
    });return T;
}
void OnObjectTransacted(UObject* Object,const FTransactionObjectEvent& Event)
{
    if(!IsInGameThread()||Lifetime->Closing.load(std::memory_order_acquire)||Event.GetEventType()!=ETransactionObjectEventType::UndoRedo) { return; }Prune();auto* A=Cast<AChaosSolverActor>(Object);if(!Live(A)||!UndoActors.Contains(A)) { return; }
    const auto& Changed=Event.GetChangedProperties();if(!Changed.IsEmpty()&&!Changed.Contains(GET_MEMBER_NAME_CHECKED(AChaosSolverActor,Properties))) { return; }
    ++UndoQueueAttempts;if(Enqueue(A).IsValid()) { ++UndoQueueEnqueued; }
}
bool EnsureUndo()
{
    if(Lifetime->Closing.load(std::memory_order_acquire)) { return false; }if(!UndoHandle.IsValid()) { UndoHandle=FCoreUObjectDelegates::OnObjectTransacted.AddStatic(&OnObjectTransacted); }return UndoHandle.IsValid();
}
struct FWork
{
    FResult R;FString Command,FailedStage,RequestedContext,Context;bool Dry=true,Save=false,Observer=false;
    TSharedPtr<FJsonObject> Params,Before,FlatBefore,Canonical;TSharedRef<FJsonObject> Legacy=MakeShared<FJsonObject>(),Calls=MakeShared<FJsonObject>(),Checks=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();
    TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<AChaosSolverActor> Actor;TWeakObjectPtr<UClass> ActorClass;TWeakObjectPtr<UPackage> Package;
    TUniquePtr<FScopedTransaction> Transaction;TArray<TSharedPtr<FQueueTicket,ESPMode::ThreadSafe>> Tickets;
    FChaosSolverConfiguration Original;bool ActorCaptured=false,OriginalCaptured=false,PackageDirtyBefore=false,Joined=false,OwnedCancelled=false,PropertiesAssigned=false,MarkDirtyAttempted=false;
    bool InitialSaveAttempted=false,InitialSaved=false,RollbackAttempted=false,RollbackSettings=false,RollbackSaveAttempted=false,RollbackSaved=false,RollbackQueue=false,RollbackSucceeded=false,SyncEnqueued=false,SettingsVerified=false,SaveVerified=false;
    FWork(const TCHAR* Name,bool IsObserver=false):Command(Name),Observer(IsObserver)
    {
        R.Compiled=false;Legacy->SetBoolField(TEXT("success"),false);R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetObjectField(TEXT("native_call_counts"),Calls);R.Data->SetObjectField(TEXT("postconditions"),Checks);R.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);
        for(const TCHAR* K:{TEXT("atomic_rollback_available"),TEXT("automatic_retry_permitted"),TEXT("explicit_compile_attempted"),TEXT("explicit_post_edit_change_attempted"),TEXT("physics_effective_configuration_verified"),TEXT("physics_frame_completion_verified"),TEXT("wrapper_physics_wait_performed"),TEXT("complete_global_side_effect_inventory")}) { R.Data->SetBoolField(K,false); }
        R.Data->SetStringField(TEXT("scope"),TEXT("Typed Editor/PIE Solver actors, public configuration28 and bounded snapshots. Actor/queue/dirty/save/semantic restore are separate observations. No global CVar writes, private ApplyCVars, worker UObject access, additional transport or gameplay dependency."));
        R.Data->SetStringField(TEXT("queue_lifetime"),TEXT("Public EnqueueCommandImmediate registers a solver-owned one-shot. Callback captures only solver/configuration/atomic ticket, and no UObject. Existing module dynamic reload is disabled. Normal Editor physics teardown must finish before unloading the module; Shutdown cancels only not-started tickets and never waits for or interrupts an entered ApplyConfig."));
        for(const TCHAR* K:{TEXT("Modify_calls"),TEXT("Modify_returns"),TEXT("properties_assignments"),TEXT("queue_calls"),TEXT("queue_returns"),TEXT("mark_dirty_calls"),TEXT("mark_dirty_returns"),TEXT("exact_save_calls"),TEXT("exact_save_returns"),TEXT("restore_properties_assignments"),TEXT("restore_queue_calls"),TEXT("restore_queue_returns"),TEXT("restore_dirty_calls"),TEXT("restore_dirty_returns"),TEXT("restore_save_calls"),TEXT("restore_save_returns"),TEXT("begin_transaction_calls"),TEXT("begin_transaction_returns"),TEXT("cancel_transaction_calls"),TEXT("cancel_transaction_returns"),TEXT("end_transaction_calls"),TEXT("end_transaction_returns"),TEXT("compile_calls"),TEXT("PostEditChange_calls"),TEXT("wrapper_wait_calls")}) { Calls->SetNumberField(K,0); }
    }
    void Count(const TCHAR* K) { double N=0;Calls->TryGetNumberField(K,N);Calls->SetNumberField(K,N+1); }
    bool Fail(const TCHAR* Code,const FString& Error)
    { if(R.Error.IsEmpty()) { FailedStage=R.Stage;R.ErrorCode=FString(TEXT("chaos_solver_settings_"))+Code;R.Error=Error; }Legacy->SetBoolField(TEXT("success"),false);Legacy->SetStringField(TEXT("error"),R.Error);return false; }
    bool Request(const FString& Text)
    { R.Stage=TEXT("parse_params");if(!Parse(Text,Params)) { return Fail(TEXT("params_invalid"),TEXT("This dedicated Chaos Solver API requires one bounded JSON params object")); }R.Data->SetStringField(TEXT("params_json"),Text);R.Data->SetObjectField(TEXT("params"),Params.ToSharedRef());return true; }
    bool Enter()
    {
        R.Stage=TEXT("world_admission");if(!IsEditorGameThread()||!Live(GEditor)) { return Fail(TEXT("editor_required"),TEXT("Requires the Editor game thread")); }if(Running||Lifetime->Closing.load(std::memory_order_acquire)) { return Fail(TEXT("lifecycle_blocked"),TEXT("Chaos Solver operation is reentrant or shutting down")); }Editor=GEditor;R.bChangeStateKnown=false;FString Error;World=ResolveWorld(RequestedContext,Context,Error);if(!Live(World.Get())) { return Fail(TEXT("world_unavailable"),Error); }Prefix->SetStringField(TEXT("world"),World->GetPathName());return true;
    }
    bool Admit(bool Write=false)
    {
        if(!IsEditorGameThread()||!Live(Editor.Get())||GEditor!=Editor.Get()||!Live(World.Get())||Lifetime->Closing.load(std::memory_order_acquire)) { return Fail(TEXT("lifetime_changed"),TEXT("Editor/world lifetime changed or shutdown began")); }
        FString CurrentContext,Error;R.bChangeStateKnown=false;UWorld* Current=ResolveWorld(RequestedContext,CurrentContext,Error);if(Current!=World.Get()||CurrentContext!=Context) { return Fail(TEXT("world_changed"),TEXT("Resolved world context changed during the operation")); }
        if(Write&&Context==TEXT("editor")&&IsPlaySessionActive()) { return Fail(TEXT("PIE_editor_write_blocked"),TEXT("Editor-world Chaos solver mutation is blocked while PIE/SIE is active; use world_context=play for transient tuning")); }
        if(ActorCaptured&&(!Live(Actor.Get())||!Live(ActorClass.Get())||!Live(Package.Get())||Actor->GetWorld()!=World.Get()||Actor->GetClass()!=ActorClass.Get()||Actor->GetPackage()!=Package.Get())) { return Fail(TEXT("actor_package_changed"),TEXT("Solver actor/class/world/exact target package changed during a callback")); }return true;
    }
    bool CaptureActor(AChaosSolverActor* A)
    {
        if(!Live(A)||!Admit()) { return Fail(TEXT("actor_invalid"),TEXT("Solver actor is unavailable")); }Actor=A;ActorCaptured=true;ActorClass=A->GetClass();Package=A->GetPackage();if(!Live(Package.Get())) { return Fail(TEXT("package_invalid"),TEXT("Solver actor has no live target package")); }
        PackageDirtyBefore=Package->IsDirty();Prefix->SetStringField(TEXT("actor_path"),A->GetPathName());Prefix->SetStringField(TEXT("target_package"),Package->GetName());Prefix->SetStringField(TEXT("outermost_package"),A->GetOutermost()->GetName());Prefix->SetBoolField(TEXT("target_package_dirty_before"),PackageDirtyBefore);return true;
    }
    void Write(const TCHAR* Stage,const TCHAR* Counter)
    { R.Stage=Stage;R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(Counter); }
    bool Queue(bool Restore=false)
    {
        if(!Admit(true)||!Live(Actor.Get())) { return false; }if(!Actor->GetSolver()) { Prefix->SetBoolField(TEXT("live_solver_unavailable_queue_skipped"),true);return false; }Prune();if(QueueTickets.Num()>=MaxQueueTickets) { return Fail(TEXT("queue_capacity"),TEXT("Owned solver queue ticket capacity reached; no additional callback was submitted")); }
        Write(Restore?TEXT("restore_live_solver_queue"):TEXT("enqueue_live_solver"),Restore?TEXT("restore_queue_calls"):TEXT("queue_calls"));auto T=Enqueue(Actor.Get());Count(Restore?TEXT("restore_queue_returns"):TEXT("queue_returns"));if(T.IsValid()) { Tickets.Add(T); }else { Fail(TEXT("queue_admission_failed"),TEXT("A live solver queue admission changed before enqueue; no completion is assumed")); }return T.IsValid();
    }
    bool SaveExact(bool Restore=false)
    {
        if(!Admit(true)||!Live(Actor.Get())||!Live(Package.Get())||Actor->GetPackage()!=Package.Get()||!IsDurablySavablePackage(Package.Get())) { Fail(TEXT("save_lifetime_invalid"),TEXT("Exact target package is no longer durably savable"));return false; }
        Write(Restore?TEXT("restore_exact_package_save"):TEXT("save_exact_package"),Restore?TEXT("restore_save_calls"):TEXT("exact_save_calls"));R.bSaveAttempted=true;if(Restore) { RollbackSaveAttempted=true; }else { InitialSaveAttempted=true; }
        TGuardValue<bool> Guard(GIsRunningUnattendedScript,true);const bool Saved=UEditorLoadingAndSavingUtils::SavePackages(TArray<UPackage*>{Package.Get()},false);Count(Restore?TEXT("restore_save_returns"):TEXT("exact_save_returns"));
        const bool PackageLive=Live(Package.Get());const bool Clean=PackageLive&&!Package->IsDirty();Prefix->SetBoolField(Restore?TEXT("restore_save_returned"):TEXT("exact_save_returned"),Saved);Prefix->SetBoolField(Restore?TEXT("restore_package_clean_observed"):TEXT("saved_package_clean_observed"),Clean);R.bSaved=Saved&&Clean;return R.bSaved;
    }
    void Close()
    { if(Transaction) { R.Stage=TEXT("end_transaction");R.bChangeStateKnown=false;Count(TEXT("end_transaction_calls"));Transaction.Reset();Count(TEXT("end_transaction_returns")); } }
    void Restore()
    {
        if(!R.bMutationAttempted||!OriginalCaptured) { return; }RollbackAttempted=true;const FString InitialStage=R.Stage;R.Stage=TEXT("semantic_restore_admission");if(!Admit(true)||!Live(Actor.Get())) { return; }
        Write(TEXT("restore_properties"),TEXT("restore_properties_assignments"));Actor->Properties=Original;RollbackQueue=Queue(true);if(!Admit(true)||!Live(Actor.Get())) { return; }
        TArray<FString> M;RollbackSettings=CheckExpectedFields(MakeFlatSolverConfigurationSnapshot(Original),MakeFlatSolverExpectedSnapshot(Actor.Get()),M);Prefix->SetArrayField(TEXT("semantic_restore_mismatches"),StringsToJson(M));
        bool DirtyRestored=false;if(!InitialSaveAttempted&&RollbackSettings&&Live(Package.Get())) { Write(TEXT("restore_dirty_flag"),TEXT("restore_dirty_calls"));Package->SetDirtyFlag(PackageDirtyBefore);Count(TEXT("restore_dirty_returns"));DirtyRestored=Live(Package.Get())&&Package->IsDirty()==PackageDirtyBefore; }
        if(InitialSaveAttempted&&RollbackSettings) { RollbackSaved=SaveExact(true); }
        RollbackSucceeded=RollbackSettings&&(InitialSaveAttempted?(RollbackSaveAttempted&&RollbackSaved):DirtyRestored);
        if(Transaction&&RollbackSettings) { Write(TEXT("cancel_owned_transaction"),TEXT("cancel_transaction_calls"));Transaction->Cancel();Count(TEXT("cancel_transaction_returns"));OwnedCancelled=true; }R.Stage=InitialStage;
    }
    FResult Finish()
    {
        Close();TArray<TSharedPtr<FJsonValue>> QueueValues;for(const auto& T:Tickets) { QueueValues.Add(MakeShared<FJsonValueObject>(QueueSnapshot(T))); }R.Data->SetArrayField(TEXT("physics_queue_receipts"),QueueValues);R.Data->SetStringField(TEXT("failed_stage"),FailedStage);
        R.Data->SetBoolField(TEXT("explicit_save_attempted"),R.bSaveAttempted);R.Data->SetNumberField(TEXT("Undo_resync_queue_attempts_observed"),UndoQueueAttempts);R.Data->SetNumberField(TEXT("Undo_resync_enqueued_observed"),UndoQueueEnqueued);
        if(IsEditorGameThread()&&GEditor==Editor.Get()) { Prefix->SetBoolField(TEXT("actor_still_live"),Live(Actor.Get()));Prefix->SetBoolField(TEXT("target_package_still_live"),Live(Package.Get()));if(Live(Package.Get())) { Prefix->SetBoolField(TEXT("target_package_dirty_after"),Package->IsDirty()); } }
        R.bSuccess=R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;if(R.bSuccess) { R.Stage=TEXT("complete"); }else { R.Stage=FailedStage; }return R;
    }
};
} // namespace

Variable::FResult Inspect(const FString& ParamsJson)
{
    FWork W(TEXT("inspect_chaos_solver_settings"),true);if(!W.Request(ParamsJson)) { return W.Finish(); }FJsonParamParseState P;const FString Name=GetStringParam(W.Params,TEXT("actor_name"),P),Label=GetStringParam(W.Params,TEXT("actor_label"),P);W.RequestedContext=GetStringParam(W.Params,TEXT("world_context"),P,TEXT("auto"));const bool Runtime=GetBoolParam(W.Params,TEXT("include_runtime_state"),true,P);const int32 Max=FMath::Clamp(GetIntParam(W.Params,TEXT("max_results"),16,P),1,256);
    if(!P.IsValid()) { W.Fail(TEXT("params_invalid"),P.GetErrorMessage());return W.Finish(); }if(!W.Enter()) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);const auto Matches=FindActors<AChaosSolverActor>(W.World.Get(),Name,Label);if(Matches.Num()>MaxActors) { W.Fail(TEXT("actor_limit"),TEXT("Solver match count exceeds the bounded inspection limit"));return W.Finish(); }
    TArray<TSharedPtr<FJsonValue>> Actors,Queues;for(int32 I=0;I<Matches.Num()&&I<Max;++I)
    {
        if(!W.Admit()||!Live(Matches[I])) { W.Fail(TEXT("actor_lifetime_changed"),TEXT("A selected solver disappeared during inspection"));return W.Finish(); }auto S=MakeSolverActorSnapshot(Matches[I],Runtime);S->SetObjectField(TEXT("expected_current_fields"),MakeFlatSolverExpectedSnapshot(Matches[I]));Actors.Add(MakeShared<FJsonValueObject>(S));
        auto Q=MakeShared<FJsonObject>();Q->SetStringField(TEXT("actor_path"),Matches[I]->GetPathName());const auto* T=LatestQueues.Find(Matches[I]);Q->SetObjectField(TEXT("latest_owned_queue"),QueueSnapshot(T?T->Pin():TSharedPtr<FQueueTicket,ESPMode::ThreadSafe>()));Queues.Add(MakeShared<FJsonValueObject>(Q));
    }
    auto D=MakeShared<FJsonObject>();D->SetStringField(TEXT("world_context"),W.Context);D->SetNumberField(TEXT("match_count"),Matches.Num());D->SetNumberField(TEXT("returned_count"),Actors.Num());D->SetBoolField(TEXT("truncated"),Matches.Num()>Actors.Num());D->SetArrayField(TEXT("actors"),Actors);D->SetArrayField(TEXT("writable_setting_names"),StringsToJson(GetWritableSolverSettingNames()));
    D->SetArrayField(TEXT("engine_source_findings"),StringsToJson({TEXT("UE 5.8.3 FChaosSolverConfiguration is USTRUCT(BlueprintType); this dedicated wrapper preserves the original bounded leaf contract."),TEXT("Stored CollisionMarginFraction/Max remain omitted by UE 5.8.3 FPBDRigidsSolver::ApplyConfig."),TEXT("ApplyConfig invokes private ApplyCVars after actor values; the read-only global CVar diagnostics describe that precedence."),TEXT("Runtime determinism is native solver state and is not persisted by Actor.Properties; queue admission is not completion.")}));W.Legacy->SetBoolField(TEXT("success"),true);W.Legacy->SetObjectField(TEXT("data"),D);W.R.Data->SetArrayField(TEXT("physics_queue_observations"),Queues);W.Checks->SetBoolField(TEXT("direct_writer_not_attempted"),true);return W.Finish();
}

Variable::FResult Configure(const FString& ParamsJson)
{
    FWork W(TEXT("configure_chaos_solver_settings"));if(!W.Request(ParamsJson)) { return W.Finish(); }FJsonParamParseState P;const FString Name=GetStringParam(W.Params,TEXT("actor_name"),P),Label=GetStringParam(W.Params,TEXT("actor_label"),P);W.RequestedContext=GetStringParam(W.Params,TEXT("world_context"),P,TEXT("editor"));W.Dry=GetBoolParam(W.Params,TEXT("dry_run"),true,P);W.Save=GetBoolParam(W.Params,TEXT("save_level"),false,P);W.R.bSaveRequested=W.Save;
    if(!P.IsValid()) { W.Fail(TEXT("params_invalid"),P.GetErrorMessage());return W.Finish(); }if(Name.IsEmpty()&&Label.IsEmpty()) { W.Fail(TEXT("selector_required"),TEXT("configure_chaos_solver_settings requires actor_name or actor_label"));return W.Finish(); }
    const TSharedPtr<FJsonObject>* SettingsPtr=nullptr,*ExpectedPtr=nullptr;TSharedPtr<FJsonObject> Expected;if(!W.Params->TryGetObjectField(TEXT("settings"),SettingsPtr)||!SettingsPtr||!SettingsPtr->IsValid()||(*SettingsPtr)->Values.IsEmpty()) { W.Fail(TEXT("settings_invalid"),TEXT("settings must be a non-empty JSON object"));return W.Finish(); }
    if(W.Params->HasField(TEXT("expected_current"))&&!W.Params->HasTypedField<EJson::Object>(TEXT("expected_current"))) { W.Fail(TEXT("expected_invalid"),TEXT("expected_current must be a JSON object when supplied"));return W.Finish(); }if(W.Params->TryGetObjectField(TEXT("expected_current"),ExpectedPtr)&&ExpectedPtr) { Expected=*ExpectedPtr; }
    if(!W.Enter()) { return W.Finish(); }TGuardValue<bool> Guard(Running,true);if(W.Save&&W.Context!=TEXT("editor")) { W.Fail(TEXT("play_save_disabled"),TEXT("save_level is allowed only for the editor world"));return W.Finish(); }
    if(!W.Dry&&W.Context==TEXT("editor")&&IsPlaySessionActive()) { W.Fail(TEXT("PIE_editor_write_blocked"),TEXT("Editor-world Chaos solver mutation is blocked while PIE/SIE is active; use world_context=play for transient tuning"));return W.Finish(); }
    const auto Matches=FindActors<AChaosSolverActor>(W.World.Get(),Name,Label);if(Matches.Num()!=1) { W.Fail(TEXT("selector_not_unique"),FString::Printf(TEXT("Chaos solver selector must resolve exactly one actor (matched %d)"),Matches.Num()));return W.Finish(); }if(!W.CaptureActor(Matches[0])||!W.Admit()) { return W.Finish(); }
    W.FlatBefore=MakeFlatSolverExpectedSnapshot(W.Actor.Get());TArray<FString> Mismatches;if(!CheckExpectedFields(Expected,W.FlatBefore,Mismatches)) { W.Fail(TEXT("expected_mismatch"),TEXT("expected_current mismatch: ")+FString::Join(Mismatches,TEXT("; ")));return W.Finish(); }
    W.Original=W.Actor->Properties;W.OriginalCaptured=true;FChaosSolverConfiguration Candidate=W.Original;TArray<FString> PatchErrors;if(!ApplySolverSettingPatch(*SettingsPtr,Candidate,PatchErrors)) { W.Fail(TEXT("settings_invalid"),FString::Join(PatchErrors,TEXT("; ")));return W.Finish(); }W.Canonical=SelectJsonFields(MakeFlatSolverConfigurationSnapshot(Candidate),*SettingsPtr);TArray<FString> NoOp;const bool Change=!CheckExpectedFields(W.Canonical,W.FlatBefore,NoOp);W.Before=MakeSolverActorSnapshot(W.Actor.Get(),true);
    W.Legacy->SetStringField(TEXT("actor_path"),W.Actor->GetPathName());W.Legacy->SetStringField(TEXT("world_context"),W.Context);W.Legacy->SetObjectField(TEXT("requested_settings"),*SettingsPtr);W.Legacy->SetObjectField(TEXT("canonical_settings"),W.Canonical);W.Legacy->SetBoolField(TEXT("save_requested"),W.Save);W.Legacy->SetBoolField(TEXT("would_change"),Change);W.Legacy->SetBoolField(TEXT("no_op"),!Change);
    if(W.Dry)
    {
        W.Legacy->SetBoolField(TEXT("success"),true);W.Legacy->SetBoolField(TEXT("save_attempted"),false);W.Legacy->SetBoolField(TEXT("saved"),false);W.Legacy->SetBoolField(TEXT("mutation_attempted"),false);W.Legacy->SetStringField(TEXT("transaction_scope"),TEXT("none"));W.Legacy->SetBoolField(TEXT("owned_transaction_cancelled"),false);
        AddLegacyMutationResult(W.Legacy,true,false,false,W.Before,W.Before,MakeDeltaCounts(false),true,TEXT("solver_actor_snapshot_dry_run"),{TEXT("unique_target"),TEXT("settings_validated"),TEXT("expected_current_matches")},true);return W.Finish();
    }
    if(Change&&W.Save&&(!IsDurablySavableObject(W.Actor.Get())||!IsDurablySavablePackage(W.Package.Get()))) { W.Fail(TEXT("save_target_not_durable"),TEXT("save_level requires a non-transient actor in a durably savable package"));return W.Finish(); }
    if(Change&&W.Save&&W.PackageDirtyBefore) { W.Fail(TEXT("save_target_dirty"),TEXT("save_level requires a clean target package so unrelated pre-existing edits are not persisted"));return W.Finish(); }
    bool Admission=true;if(Change)
    {
        W.R.Stage=TEXT("write_admission");Admission=W.Admit(true);TArray<FString> FreshMismatch;if(Admission&&!CheckExpectedFields(MakeFlatSolverConfigurationSnapshot(W.Original),MakeFlatSolverExpectedSnapshot(W.Actor.Get()),FreshMismatch)) { W.Fail(TEXT("changed_before_apply"),TEXT("Stored solver fields changed after preflight; no apply was attempted"));Admission=false; }
        if(Admission&&W.Context==TEXT("editor")&&!EnsureUndo()) { W.Fail(TEXT("Undo_registration_failed"),TEXT("Owned Undo live-sync delegate could not be registered before apply"));Admission=false; }
        if(Admission&&W.Context==TEXT("editor"))
        {
            W.Joined=GEditor->IsTransactionActive();if(!W.Joined) { W.Write(TEXT("begin_transaction"),TEXT("begin_transaction_calls"));W.Transaction=MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Configure Chaos Solver Settings")));W.Count(TEXT("begin_transaction_returns")); }
            if(W.Admit(true)) { W.Write(TEXT("Modify_actor"),TEXT("Modify_calls"));W.Actor->Modify();W.Count(TEXT("Modify_returns")); }else { Admission=false; }
        }
        if(Admission&&W.Admit(true)) { W.Write(TEXT("assign_properties"),TEXT("properties_assignments"));W.Actor->Properties=Candidate;W.PropertiesAssigned=true;W.SyncEnqueued=W.Queue();if(!W.R.Error.IsEmpty()) { Admission=false; } }
        else { Admission=false; }
        if(Admission&&W.Context==TEXT("editor")&&W.Admit(true)) { W.Write(TEXT("mark_actor_package_dirty"),TEXT("mark_dirty_calls"));W.Actor->MarkPackageDirty();W.Count(TEXT("mark_dirty_returns"));W.MarkDirtyAttempted=true; }
    }
    if(Admission&&W.Admit()) { W.SettingsVerified=CheckExpectedFields(W.Canonical,MakeFlatSolverExpectedSnapshot(W.Actor.Get()),Mismatches); }
    if(!W.SettingsVerified&&W.R.Error.IsEmpty()) { W.R.Stage=TEXT("settings_readback");W.Fail(TEXT("settings_readback_failed"),Mismatches.IsEmpty()?TEXT("Chaos solver setting readback failed"):FString::Join(Mismatches,TEXT("; "))); }
    if(W.Save&&W.SettingsVerified&&W.PropertiesAssigned) { W.InitialSaved=W.SaveExact(); }
    W.SaveVerified=!W.Save||!W.PropertiesAssigned||(W.InitialSaveAttempted&&W.InitialSaved);if(!W.SaveVerified&&W.R.Error.IsEmpty()) { W.Fail(TEXT("save_failed"),TEXT("Chaos solver exact package save/readback verification failed")); }
    bool Verified=W.SettingsVerified&&W.SaveVerified&&W.R.Error.IsEmpty();if(!Verified&&W.R.bMutationAttempted) { W.Restore(); }
    W.Close();TSharedPtr<FJsonObject> After;
    if(W.Admit()&&Live(W.Actor.Get()))
    {
        auto FinalFields=MakeFlatSolverExpectedSnapshot(W.Actor.Get());TArray<FString> FinalMismatch;
        if(Verified&&!CheckExpectedFields(W.Canonical,FinalFields,FinalMismatch)) { W.R.Stage=TEXT("final_readback_after_transaction");W.Fail(TEXT("final_settings_mismatch"),TEXT("Solver fields changed at transaction completion: ")+FString::Join(FinalMismatch,TEXT("; ")));Verified=false;W.Restore(); }
        if(W.Admit()&&Live(W.Actor.Get())) { After=MakeSolverActorSnapshot(W.Actor.Get(),true);const bool Different=JsonObjectToString(MakeFlatSolverConfigurationSnapshot(W.Original))!=JsonObjectToString(MakeFlatSolverConfigurationSnapshot(W.Actor->Properties));W.R.bChanged=Different;W.Prefix->SetObjectField(TEXT("actual_fields_after"),MakeFlatSolverExpectedSnapshot(W.Actor.Get())); }
    }
    if(!W.R.Error.IsEmpty()) { Verified=false; }
    if(!Verified&&W.R.bMutationAttempted&&!W.RollbackAttempted) { W.Restore(); }
    if(Verified&&W.PropertiesAssigned&&W.Context==TEXT("editor")&&Live(W.Actor.Get())) { Prune();UndoActors.Add(W.Actor); }
    const bool Partial=!Verified&&W.R.bMutationAttempted&&!W.RollbackSucceeded;W.R.bChanged=W.R.bChanged||Partial;W.Legacy->SetBoolField(TEXT("success"),Verified&&W.R.Error.IsEmpty());W.Legacy->SetBoolField(TEXT("save_attempted"),W.InitialSaveAttempted);W.Legacy->SetBoolField(TEXT("saved"),W.InitialSaved);W.Legacy->SetBoolField(TEXT("save_skipped_no_change"),W.Save&&!W.PropertiesAssigned);W.Legacy->SetStringField(TEXT("target_package"),Live(W.Package.Get())?W.Package->GetName():FString());W.Legacy->SetBoolField(TEXT("target_package_dirty"),Live(W.Package.Get())&&W.Package->IsDirty());W.Legacy->SetBoolField(TEXT("mutation_attempted"),W.R.bMutationAttempted);W.Legacy->SetStringField(TEXT("transaction_scope"),W.Calls->GetNumberField(TEXT("begin_transaction_calls"))>0?TEXT("owned"):(W.Joined?TEXT("joined_outer"):TEXT("none")));W.Legacy->SetBoolField(TEXT("owned_transaction_cancelled"),W.OwnedCancelled);
    W.Legacy->SetBoolField(TEXT("live_solver_available"),Live(W.Actor.Get())&&W.Actor->GetSolver()!=nullptr);W.Legacy->SetBoolField(TEXT("live_solver_sync_required"),false);W.Legacy->SetBoolField(TEXT("live_solver_sync_enqueued"),W.SyncEnqueued);W.Legacy->SetBoolField(TEXT("undo_redo_live_sync_registered"),Verified&&W.PropertiesAssigned&&W.Context==TEXT("editor")&&UndoActors.Contains(W.Actor));W.Legacy->SetBoolField(TEXT("rollback_attempted"),W.RollbackAttempted);W.Legacy->SetBoolField(TEXT("rollback_settings_verified"),W.RollbackSettings);W.Legacy->SetBoolField(TEXT("rollback_save_attempted"),W.RollbackSaveAttempted);W.Legacy->SetBoolField(TEXT("rollback_saved"),W.RollbackSaved);W.Legacy->SetBoolField(TEXT("rollback_live_solver_sync_enqueued"),W.RollbackQueue);W.Legacy->SetBoolField(TEXT("rollback_succeeded"),W.RollbackSucceeded);W.Legacy->SetBoolField(TEXT("partial_target_modified"),Partial);W.Legacy->SetBoolField(TEXT("do_not_retry"),Partial);W.Legacy->SetBoolField(TEXT("runtime_deterministic_persistent"),false);
    AddLegacyMutationResult(W.Legacy,false,W.PropertiesAssigned,W.R.bChanged,W.Before,After,MakeDeltaCounts(W.R.bChanged,!Verified),Verified&&W.R.Error.IsEmpty(),TEXT("solver_actor_configuration_property_readback"),{TEXT("unique_target"),TEXT("actor_properties_match"),TEXT("live_solver_sync_optional"),TEXT("save_gate"),TEXT("rollback_gate")},true);const TSharedPtr<FJsonObject>* Mutation=nullptr;if(W.Legacy->TryGetObjectField(TEXT("mutation"),Mutation)&&Mutation&&Mutation->IsValid()) { (*Mutation)->SetBoolField(TEXT("attempted"),W.R.bMutationAttempted); }
    W.Checks->SetBoolField(TEXT("actor_settings_verified"),W.SettingsVerified);W.Checks->SetBoolField(TEXT("save_gate_verified"),W.SaveVerified);W.Checks->SetBoolField(TEXT("semantic_restore_is_not_atomic_rollback"),true);return W.Finish();
}

void Shutdown()
{
    Lifetime->Closing.store(true,std::memory_order_release);if(UndoHandle.IsValid()) { FCoreUObjectDelegates::OnObjectTransacted.Remove(UndoHandle);UndoHandle.Reset(); }
    for(const auto& W:QueueTickets) { if(const auto T=W.Pin()) { int32 Queued=0;T->Phase.compare_exchange_strong(Queued,3,std::memory_order_acq_rel); } }
    UndoActors.Reset();LatestQueues.Reset();QueueTickets.Reset();
}
} // namespace Cubeless::UE58::ChaosSolverSettings
