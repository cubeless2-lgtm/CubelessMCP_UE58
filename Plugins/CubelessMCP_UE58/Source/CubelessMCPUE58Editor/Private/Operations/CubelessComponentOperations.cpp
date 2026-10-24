#include "Operations/CubelessComponentOperations.h"
#include "Compatibility/UE58/CubelessComponentApi.h"
#include "Compatibility/UE58/CubelessComponentMutationApi.h"
#include "HAL/PlatformTime.h"
#include "Serialization/JsonSerializer.h"

namespace Cubeless::Operations
{
namespace
{
FString EncodeComponentMutation(const UE58::Component::FMutationResult& R, const TCHAR* Operation, double Start)
{
    auto Json = MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("success"), R.bSuccess); Json->SetStringField(TEXT("operation"), Operation);
    Json->SetStringField(TEXT("stage"), R.Stage); Json->SetObjectField(TEXT("data"), R.Data);
    Json->SetBoolField(TEXT("mutation_attempted"), R.bMutationAttempted); Json->SetBoolField(TEXT("change_state_known"), R.bChangeStateKnown);
    if (R.bChangeStateKnown) { Json->SetBoolField(TEXT("changed"), R.bChanged); } else { Json->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    Json->SetBoolField(TEXT("postcondition_passed"), R.bPostconditionPassed);
    Json->SetBoolField(TEXT("compile_requested"), R.bCompileRequested); Json->SetBoolField(TEXT("compile_attempted"), R.bCompileAttempted);
    if (R.Compiled.IsSet()) { Json->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); } else { Json->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); }
    Json->SetBoolField(TEXT("save_requested"), false); Json->SetBoolField(TEXT("saved"), false);
    Json->SetBoolField(TEXT("atomic_rollback_available"), false); Json->SetBoolField(TEXT("auto_retry_safe"), false);
    Json->SetNumberField(TEXT("elapsed_ms"), (FPlatformTime::Seconds() - Start) * 1000.0);
    Json->SetStringField(TEXT("error_code"), R.ErrorCode); Json->SetStringField(TEXT("error"), R.Error);
    FString Text; FJsonSerializer::Serialize(Json, TJsonWriterFactory<>::Create(&Text)); return Text;
}
}

FString AddBlueprintComponent(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName,
    const FString& TransformJson, const FString& ParentComponent, const FString& ComponentPropertiesJson, bool bCompile)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::AddLocalComponent(BlueprintPath, ComponentType, ComponentName,
        TransformJson, ParentComponent, ComponentPropertiesJson, bCompile), TEXT("add_component_to_blueprint"), Start);
}

FString SetBlueprintComponentProperty(const FString& BlueprintPath, const FString& ComponentName,
    const FString& PropertyName, const FString& ValueJson)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::SetLocalComponentProperty(BlueprintPath, ComponentName, PropertyName, ValueJson), TEXT("set_component_property"), Start);
}

FString ReadBlueprintComponents(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertyName,
    bool bReadProperty, int32 MaxComponents, int32 MaxValueChars)
{
    const double Start = FPlatformTime::Seconds();
    UE58::Component::FReadResult Read;
    FString Code, Error;
    const bool bSuccess = UE58::Component::ReadLocalComponents(BlueprintPath, ComponentName, PropertyName, bReadProperty,
        MaxComponents, MaxValueChars, Read, Code, Error);
    auto Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), bSuccess);
    Result->SetStringField(TEXT("operation"), bReadProperty ? TEXT("get_component_property") : TEXT("list_blueprint_components"));
    Result->SetStringField(TEXT("stage"), bSuccess ? TEXT("complete") : TEXT("read"));
    Result->SetObjectField(TEXT("data"), Read.Data);
    Result->SetBoolField(TEXT("mutation_attempted"), false);
    Result->SetBoolField(TEXT("compile_requested"), false);
    Result->SetBoolField(TEXT("save_requested"), false);
    Result->SetBoolField(TEXT("saved"), false);
    Result->SetBoolField(TEXT("loaded_during_read"), Read.bLoadedDuringRead);
    Result->SetBoolField(TEXT("implicit_postload_work_possible"), true);
    Result->SetStringField(TEXT("load_scope"), TEXT("Asset loading/reference resolution may run engine PostLoad; the Blueprint dirty baseline is observed after its load. No SCS mutation, inherited override creation or explicit Blueprint compile API is called."));
    // A failed asset load has no post-load baseline. Do not claim its implicit
    // engine effects are known merely because no explicit mutation was called.
    const bool bKnown = Read.bDirtyObserved ? Read.bDirtyAfterObserved : !Read.bLoadedDuringRead;
    const bool bObservedChange = Read.bDirtyObserved && (Read.bDirtyBefore != Read.bDirtyAfter || Read.StatusBefore != Read.StatusAfter);
    Result->SetBoolField(TEXT("change_state_known"), bKnown);
    if (Read.bDirtyObserved)
    {
        Result->SetBoolField(TEXT("dirty_before"), Read.bDirtyBefore);
        Result->SetNumberField(TEXT("blueprint_status_before"), Read.StatusBefore);
    }
    if (Read.bDirtyAfterObserved)
    {
        Result->SetBoolField(TEXT("dirty_after"), Read.bDirtyAfter);
        Result->SetNumberField(TEXT("blueprint_status_after"), Read.StatusAfter);
    }
    if (bKnown) { Result->SetBoolField(TEXT("changed"), bObservedChange); }
    Result->SetBoolField(TEXT("postcondition_passed"), bSuccess && bKnown);
    Result->SetBoolField(TEXT("auto_retry_safe"), bSuccess && bKnown && !bObservedChange && !Read.bLoadedDuringRead);
    Result->SetStringField(TEXT("verification_scope"), TEXT("bounded local reachable SCS observation with loaded Blueprint dirty/status checks; no inherited/default-object completeness, arbitrary object-state or disk persistence proof"));
    auto Performance = MakeShared<FJsonObject>();
    Performance->SetNumberField(TEXT("local_scs_nodes_visited"), Read.NodeVisits);
    Performance->SetNumberField(TEXT("elapsed_ms_before_serialization"), (FPlatformTime::Seconds() - Start) * 1000.0);
    Result->SetObjectField(TEXT("performance"), Performance);
    auto Limits = MakeShared<FJsonObject>();
    Limits->SetNumberField(TEXT("max_components"), MaxComponents);
    Limits->SetNumberField(TEXT("max_value_chars"), MaxValueChars);
    Limits->SetNumberField(TEXT("max_scalar_array_elements"), 256);
    Result->SetObjectField(TEXT("limits"), Limits);
    if (!bSuccess) { Result->SetStringField(TEXT("error_code"), Code); Result->SetStringField(TEXT("error"), Error); }
    FString Json;
    FJsonSerializer::Serialize(Result, TJsonWriterFactory<>::Create(&Json));
    return Json;
}

FString SetBlueprintComponentPhysics(const FString& BlueprintPath, const FString& ComponentName,
    bool bSimulatePhysics, bool bGravityEnabled, double Mass, double LinearDamping, double AngularDamping)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::SetLocalPhysicsProperties(BlueprintPath, ComponentName,
        bSimulatePhysics, bGravityEnabled, Mass, LinearDamping, AngularDamping), TEXT("set_physics_properties"), Start);
}

FString SetBlueprintStaticMeshProperties(const FString& BlueprintPath, const FString& ComponentName,
    const FString& StaticMeshPath, const FString& MaterialPath)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::SetLocalStaticMeshProperties(BlueprintPath, ComponentName,
        StaticMeshPath, MaterialPath), TEXT("set_static_mesh_properties"), Start);
}
}

namespace Cubeless::Operations
{
FString ConfigureBlueprintPCGTemplate(const FString& ParamsJson)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::ConfigureBlueprintPCGTemplate(ParamsJson), TEXT("configure_blueprint_pcg_template"), Start);
}
}

namespace Cubeless::Operations
{
FString SetBlueprintStaticMeshPropertiesRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& AssetsJson)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::SetLocalStaticMeshPropertiesRaw(BlueprintPath, ComponentName, AssetsJson),
        TEXT("set_static_mesh_properties"), Start);
}
}

namespace Cubeless::Operations
{
FString SetBlueprintComponentPhysicsRaw(const FString& BlueprintPath, const FString& ComponentName, const FString& PropertiesJson)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::SetLocalPhysicsPropertiesRaw(BlueprintPath, ComponentName, PropertiesJson),
        TEXT("set_physics_properties"), Start);
}
}

namespace Cubeless::Operations
{
FString AddBlueprintComponentRaw(const FString& BlueprintPath, const FString& ComponentType, const FString& ComponentName, const FString& TransformJson, const FString& ParentComponent)
{
    const double Start = FPlatformTime::Seconds();
    return EncodeComponentMutation(UE58::Component::AddLocalComponentRaw(BlueprintPath, ComponentType, ComponentName, TransformJson, ParentComponent), TEXT("add_component_to_blueprint"), Start);
}
}
