#include "Compatibility/UE58/CubelessPhysicalMaterialTraceApi.h"

#include "CollisionQueryParams.h"
#include "Components/PrimitiveComponent.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/HitResult.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "HAL/PlatformTime.h"
#include "Misc/CoreMisc.h"
#include "PhysicalMaterials/PhysicalMaterial.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::PhysicalMaterialTrace
{
namespace
{
bool FiniteVector(const FVector& V)
{
    return FMath::IsFinite(V.X) && FMath::IsFinite(V.Y) && FMath::IsFinite(V.Z);
}

bool ReadVector(const FString& Json, FVector& V)
{
    // Public vectors have exactly 3 elements. The original hidden command
    // consumed the first 3 of arrays >=3; preserve that behavior within this bound.
    if (Json.Len() > 4096) { return false; }
    TArray<TSharedPtr<FJsonValue>> Values;
    if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Values) || Values.Num() < 3) { return false; }
    double Parts[3] = {};
    for (int32 I = 0; I < 3; ++I)
    {
        if (!Values[I].IsValid() || Values[I]->Type != EJson::Number || !Values[I]->TryGetNumber(Parts[I]) || !FMath::IsFinite(Parts[I])) { return false; }
    }
    V = FVector(Parts[0], Parts[1], Parts[2]);
    return true;
}

bool PutVector(const TSharedRef<FJsonObject>& J, const TCHAR* Key, const FVector& V)
{
    if (!FiniteVector(V)) { J->SetField(Key, MakeShared<FJsonValueNull>()); return false; }
    J->SetArrayField(Key, {MakeShared<FJsonValueNumber>(V.X), MakeShared<FJsonValueNumber>(V.Y), MakeShared<FJsonValueNumber>(V.Z)});
    return true;
}

bool ParseChannel(const FString& Input, ECollisionChannel& Channel, FString& Normalized)
{
    FString Key = Input.TrimStartAndEnd();
    if (Key.IsEmpty()) { Key = TEXT("visibility"); }
    Key = Key.ToLower(); Key.ReplaceInline(TEXT("_"), TEXT(""));
    Key.ReplaceInline(TEXT("-"), TEXT("")); Key.ReplaceInline(TEXT(" "), TEXT(""));
    struct FChannel { const TCHAR* Key; const TCHAR* ShortKey; const TCHAR* Canonical; ECollisionChannel Value; };
    const FChannel Channels[] = {
        {TEXT("visibility"), TEXT("visibility"), TEXT("visibility"), ECC_Visibility},
        {TEXT("camera"), TEXT("camera"), TEXT("camera"), ECC_Camera},
        {TEXT("worldstatic"), TEXT("static"), TEXT("world_static"), ECC_WorldStatic},
        {TEXT("worlddynamic"), TEXT("dynamic"), TEXT("world_dynamic"), ECC_WorldDynamic},
        {TEXT("pawn"), TEXT("pawn"), TEXT("pawn"), ECC_Pawn},
        {TEXT("physicsbody"), TEXT("physicsbody"), TEXT("physics_body"), ECC_PhysicsBody},
        {TEXT("vehicle"), TEXT("vehicle"), TEXT("vehicle"), ECC_Vehicle},
        {TEXT("destructible"), TEXT("destructible"), TEXT("destructible"), ECC_Destructible}
    };
    for (const FChannel& Item : Channels)
    {
        if (Key == Item.Key || Key == Item.ShortKey || Key == FString(TEXT("ecc")) + Item.Key)
        { Channel = Item.Value; Normalized = Item.Canonical; return true; }
    }
    return false;
}

UWorld* ResolveWorld(FString& Kind)
{
    Kind = TEXT("none");
    if (GEditor)
    {
        // Same default PIE instance 0 as the original GetPIEWorldContext().
        if (const FWorldContext* PIE = GEditor->GetPIEWorldContext())
        {
            if (UWorld* Play = PIE->World()) { Kind = TEXT("play"); return Play; }
        }
        // GetEditorWorldContext() asserts/creates when Editor context is absent.
        // Its installed implementation uses this same first Editor context scan.
        for (const FWorldContext& Context : GEditor->GetWorldContexts())
        {
            if (Context.WorldType == EWorldType::Editor)
            {
                if (UWorld* EditorWorld = Context.World()) { Kind = TEXT("editor"); return EditorWorld; }
                break;
            }
        }
    }
    if (GWorld) { Kind = TEXT("global"); return GWorld; }
    return nullptr;
}
}

Variable::FResult Trace(const FString& StartJson, const FString& EndJson, const FString& TraceChannel,
    bool bTraceComplex, bool bReturnPhysicalMaterial)
{
    Variable::FResult R; auto J = R.Data;
    const double Began = FPlatformTime::Seconds();
    J->SetStringField(TEXT("operation"), TEXT("line_trace_physical_material"));
    J->SetStringField(TEXT("trace_channel_requested"), TraceChannel);
    J->SetBoolField(TEXT("trace_complex"), bTraceComplex);
    J->SetBoolField(TEXT("return_physical_material"), bReturnPhysicalMaterial);
    J->SetBoolField(TEXT("success"), false);
    J->SetBoolField(TEXT("partial_result"), true);
    J->SetBoolField(TEXT("total_change_state_known"), true);
    J->SetBoolField(TEXT("observed_actor_label_initialized"), false);
    J->SetBoolField(TEXT("observed_explicit_change"), false);
    J->SetBoolField(TEXT("query_is_total_sdk_purity_proof"), false);
    for (const TCHAR* Key : {TEXT("trace_calls"), TEXT("actor_fetch_calls"), TEXT("actor_label_calls"),
        TEXT("body_instance_lookup_calls"), TEXT("simple_physical_material_calls"),
        TEXT("explicit_asset_load_calls"), TEXT("explicit_modify_calls"), TEXT("explicit_compile_calls"), TEXT("explicit_save_calls")})
    { J->SetNumberField(Key, 0); }
    auto Failure = [&](const TCHAR* Code, const FString& Message) -> Variable::FResult
    {
        R.ErrorCode = Code; R.Error = Message;
        J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began);
        return R;
    };
    if (!IsInGameThread() || IsEngineExitRequested())
    { return Failure(TEXT("physical_material_trace_editor_unavailable"), TEXT("A live Editor game thread is required")); }
    FVector Start, End;
    if (!ReadVector(StartJson, Start))
    { return Failure(TEXT("editor_invalid_vector_parameter"), TEXT("Missing or invalid 'start' parameter. Expected [X, Y, Z].")); }
    PutVector(J, TEXT("start"), Start);
    if (!ReadVector(EndJson, End))
    { return Failure(TEXT("editor_invalid_vector_parameter"), TEXT("Missing or invalid 'end' parameter. Expected [X, Y, Z].")); }
    PutVector(J, TEXT("end"), End);
    if (!FiniteVector(End - Start))
    { return Failure(TEXT("editor_invalid_vector_parameter"), TEXT("Trace delta must have finite coordinates")); }
    ECollisionChannel Channel = ECC_Visibility; FString Normalized;
    if (!ParseChannel(TraceChannel, Channel, Normalized))
    { return Failure(TEXT("physical_material_trace_channel_unsupported"), FString::Printf(TEXT("Unsupported trace_channel: %s"), *TraceChannel)); }
    J->SetStringField(TEXT("trace_channel"), Normalized);
    if (!GEditor && !GWorld)
    { return Failure(TEXT("physical_material_trace_world_unavailable"), TEXT("No Unreal world is available for line trace")); }
    if (GEditor && !IsValid(GEditor))
    { return Failure(TEXT("physical_material_trace_editor_unavailable"), TEXT("Editor ownership is invalid before world resolution")); }
    FString WorldKind;
    TStrongObjectPtr<UEditorEngine> EditorPin(GEditor);
    TStrongObjectPtr<UWorld> WorldPin(ResolveWorld(WorldKind));
    if (!IsValid(WorldPin.Get()) || WorldPin->bIsTearingDown)
    { return Failure(TEXT("physical_material_trace_world_unavailable"), TEXT("Failed to resolve a world for line trace")); }
    J->SetStringField(TEXT("world_context"), WorldKind);
    J->SetStringField(TEXT("world_name"), WorldPin->GetName());
    J->SetStringField(TEXT("world_path"), WorldPin->GetPathName());
    J->SetStringField(TEXT("world_selection"), TEXT("PIE_instance_0_then_first_Editor_context_then_GWorld"));
    auto CurrentWorld = [&]() -> bool
    {
        if (IsEngineExitRequested() || GEditor != EditorPin.Get() || (EditorPin.Get() && !IsValid(EditorPin.Get())) || !IsValid(WorldPin.Get()) || WorldPin->bIsTearingDown) { return false; }
        FString CurrentKind;
        return ResolveWorld(CurrentKind) == WorldPin.Get() && CurrentKind == WorldKind;
    };
    FCollisionQueryParams Params(FName(TEXT("CubelessLineTracePhysicalMaterial")), bTraceComplex);
    Params.bReturnPhysicalMaterial = bReturnPhysicalMaterial;
    FHitResult Hit;
    R.Stage = TEXT("line_trace");
    // The query, actor-handle materialization and label delegates are not an
    // exhaustively observed transaction. No explicit Modify/load/compile/save.
    R.bChangeStateKnown = false;
    J->SetBoolField(TEXT("total_change_state_known"), false);
    J->SetNumberField(TEXT("trace_calls"), 1);
    const bool bBlockingHit = WorldPin->LineTraceSingleByChannel(Hit, Start, End, Channel, Params);
    J->SetBoolField(TEXT("blocking_hit"), bBlockingHit);
    bool bFiniteHit = PutVector(J, TEXT("hit_location"), Hit.Location);
    bFiniteHit = PutVector(J, TEXT("impact_point"), Hit.ImpactPoint) && bFiniteHit;
    bFiniteHit = PutVector(J, TEXT("hit_normal"), Hit.Normal) && bFiniteHit;
    bFiniteHit = PutVector(J, TEXT("impact_normal"), Hit.ImpactNormal) && bFiniteHit;
    if (FMath::IsFinite(Hit.Distance)) { J->SetNumberField(TEXT("distance"), Hit.Distance); }
    else { J->SetField(TEXT("distance"), MakeShared<FJsonValueNull>()); bFiniteHit = false; }
    if (FMath::IsFinite(Hit.Time)) { J->SetNumberField(TEXT("time"), Hit.Time); }
    else { J->SetField(TEXT("time"), MakeShared<FJsonValueNull>()); bFiniteHit = false; }
    if (!CurrentWorld())
    { return Failure(TEXT("physical_material_trace_world_changed"), TEXT("Selected world changed during the trace; partial hit retained, no replay")); }
    if (!bFiniteHit)
    { return Failure(TEXT("physical_material_trace_hit_nonfinite"), TEXT("Trace returned non-finite hit data; null placeholders retain the failed prefix")); }

    R.Stage = TEXT("hit_actor_resolution");
    J->SetNumberField(TEXT("actor_fetch_calls"), 1);
    AActor* Actor = Hit.GetActor(); // SDK may FindOrCreateActor for instance handles.
    if (!CurrentWorld() || (Actor && (!IsValid(Actor) || Actor->GetWorld() != WorldPin.Get())))
    { return Failure(TEXT("physical_material_trace_hit_owner_changed"), TEXT("Actor resolution invalidated the selected world or hit actor")); }
    TStrongObjectPtr<AActor> ActorPin(Actor);
    UPrimitiveComponent* Component = Hit.GetComponent();
    if (Component && (!IsValid(Component) || Component->GetWorld() != WorldPin.Get()))
    { return Failure(TEXT("physical_material_trace_hit_owner_changed"), TEXT("Hit component is no longer valid in the selected world")); }
    TStrongObjectPtr<UPrimitiveComponent> ComponentPin(Component);
    auto CurrentHitOwners = [&]() -> bool
    {
        return CurrentWorld() && Hit.GetComponent() == ComponentPin.Get()
            && (!ActorPin.Get() || (IsValid(ActorPin.Get()) && ActorPin->GetWorld() == WorldPin.Get()))
            && (!ComponentPin.Get() || (IsValid(ComponentPin.Get()) && ComponentPin->GetWorld() == WorldPin.Get()));
    };
    J->SetStringField(TEXT("hit_actor_name"), IsValid(ActorPin.Get()) ? ActorPin->GetName() : FString());
    J->SetStringField(TEXT("hit_actor_path"), IsValid(ActorPin.Get()) ? ActorPin->GetPathName() : FString());
    J->SetStringField(TEXT("hit_component_name"), IsValid(ComponentPin.Get()) ? ComponentPin->GetName() : FString());
    J->SetStringField(TEXT("hit_component_path"), IsValid(ComponentPin.Get()) ? ComponentPin->GetPathName() : FString());
    J->SetStringField(TEXT("hit_actor_label"), FString());

    R.Stage = TEXT("physical_material_resolution");
    UPhysicalMaterial* PhysicalMaterial = Hit.PhysMaterial.Get();
    if (PhysicalMaterial && !IsValid(PhysicalMaterial))
    { return Failure(TEXT("physical_material_trace_material_invalid"), TEXT("Hit physical material is invalid")); }
    const bool bHitHadPhysicalMaterial = PhysicalMaterial != nullptr;
    FString MaterialSource = PhysicalMaterial ? TEXT("hit_result") : TEXT("");
    J->SetBoolField(TEXT("hit_result_had_phys_material"), bHitHadPhysicalMaterial);
    if (!PhysicalMaterial && bReturnPhysicalMaterial && IsValid(ComponentPin.Get()))
    {
        J->SetNumberField(TEXT("body_instance_lookup_calls"), 1);
        FBodyInstance* Body = ComponentPin->GetBodyInstance();
        if (!CurrentHitOwners())
        { return Failure(TEXT("physical_material_trace_hit_owner_changed"), TEXT("Hit owner changed during BodyInstance resolution")); }
        if (Body)
        {
            J->SetNumberField(TEXT("simple_physical_material_calls"), 1);
            PhysicalMaterial = Body->GetSimplePhysicalMaterial();
            // Never retain or dereference Body again across SDK material lookups.
            if (!CurrentHitOwners() || (PhysicalMaterial && !IsValid(PhysicalMaterial)))
            { return Failure(TEXT("physical_material_trace_hit_owner_changed"), TEXT("Hit owner or physical material changed during simple-material resolution")); }
            if (PhysicalMaterial) { MaterialSource = TEXT("component_body_instance_simple"); }
        }
    }
    TStrongObjectPtr<UPhysicalMaterial> MaterialPin(PhysicalMaterial);
    J->SetStringField(TEXT("phys_material_name"), IsValid(MaterialPin.Get()) ? MaterialPin->GetName() : FString());
    J->SetStringField(TEXT("phys_material_path"), IsValid(MaterialPin.Get()) ? MaterialPin->GetPathName() : FString());
    J->SetStringField(TEXT("phys_material_source"), MaterialSource);
    J->SetNumberField(TEXT("surface_type"), IsValid(MaterialPin.Get()) ? static_cast<int32>(MaterialPin->SurfaceType.GetValue()) : -1);
    FString SurfaceName;
    if (IsValid(MaterialPin.Get()))
    {
        if (const UEnum* SurfaceEnum = StaticEnum<EPhysicalSurface>())
        { SurfaceName = SurfaceEnum->GetNameStringByValue(static_cast<int64>(MaterialPin->SurfaceType.GetValue())); }
    }
    J->SetStringField(TEXT("surface_type_name"), SurfaceName);

    R.Stage = TEXT("actor_label_resolution");
    if (IsValid(ActorPin.Get()))
    {
        const FString LabelBefore = ActorPin->GetActorLabel(false);
        J->SetNumberField(TEXT("actor_label_calls"), 2);
        const FString Label = ActorPin->GetActorLabel(); // Preserve original create-if-empty/default-label behavior.
        J->SetStringField(TEXT("hit_actor_label"), Label);
        const bool bObservedLabelInitialization = LabelBefore.IsEmpty() && !Label.IsEmpty();
        J->SetBoolField(TEXT("observed_actor_label_initialized"), bObservedLabelInitialization);
        J->SetBoolField(TEXT("observed_explicit_change"), false);
        R.bChanged = bObservedLabelInitialization; // Positive observation only; total remains unknown/null.
        if (!CurrentHitOwners() || (MaterialPin.Get() && !IsValid(MaterialPin.Get())))
        { return Failure(TEXT("physical_material_trace_hit_owner_changed"), TEXT("Hit owner or material changed during actor-label callbacks; observed prefix retained")); }
    }
    if (!CurrentHitOwners())
    { return Failure(TEXT("physical_material_trace_world_changed"), TEXT("Selected world or hit owner changed before result completion")); }
    // These legacy fields were read after GetActorLabel() in the old handler.
    // Refresh them after the guarded callback instead of returning stale names.
    J->SetStringField(TEXT("hit_actor_path"), IsValid(ActorPin.Get()) ? ActorPin->GetPathName() : FString());
    J->SetStringField(TEXT("hit_component_name"), IsValid(ComponentPin.Get()) ? ComponentPin->GetName() : FString());
    J->SetStringField(TEXT("hit_component_path"), IsValid(ComponentPin.Get()) ? ComponentPin->GetPathName() : FString());
    J->SetStringField(TEXT("phys_material_name"), IsValid(MaterialPin.Get()) ? MaterialPin->GetName() : FString());
    J->SetStringField(TEXT("phys_material_path"), IsValid(MaterialPin.Get()) ? MaterialPin->GetPathName() : FString());
    J->SetBoolField(TEXT("success"), true);
    J->SetBoolField(TEXT("partial_result"), false);
    J->SetNumberField(TEXT("elapsed_seconds"), FPlatformTime::Seconds() - Began);
    R.Stage = TEXT("physical_material_trace_complete");
    R.bSuccess = true; R.bPostconditionPassed = true;
    return R; // A miss is a completed query, not an operation failure.
}
}
