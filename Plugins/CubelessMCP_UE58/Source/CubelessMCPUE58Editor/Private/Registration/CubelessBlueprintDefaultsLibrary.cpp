#include "Registration/CubelessBlueprintDefaultsLibrary.h"
#include "Compatibility/UE58/CubelessBlueprintDefaultsApi.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"
#include "Compatibility/UE58/CubelessBlueprintDefaultsConverter.h"


FString UCubelessBlueprintDefaultsLibrary::SetBlueprintActorTickSettings(const FString& BlueprintPath,
    double TickHz, double TickInterval, bool bCanEverTick, bool bStartWithTickEnabled,
    bool bAllowTickOnDedicatedServer, bool bTickEvenWhenPaused, bool bDryRun, bool bCompile, bool bSave)
{
    return Cubeless::UE58::BlueprintDefaults::SetActorTickSettings(BlueprintPath, TickHz, TickInterval,
        bCanEverTick, bStartWithTickEnabled, bAllowTickOnDedicatedServer, bTickEvenWhenPaused, bDryRun, bCompile, bSave);
}

FString UCubelessBlueprintDefaultsLibrary::SetSkeletalMeshComponentAnimDefaults(const FString& BlueprintPath,
    const FString& ComponentName, const FString& SkeletalMesh, const FString& AnimClass,
    const FString& PhysicsAssetJson, const FString& SimulatePhysicsJson, const FString& CollisionEnabledJson,
    const FString& CollisionResponseJson, const FString& EnableGravityJson,
    const FString& VisibilityBasedAnimTickOptionJson, bool bCompile, bool bSave)
{
    return Cubeless::UE58::BlueprintDefaults::SetSkeletalAnimDefaults(BlueprintPath, ComponentName,
        SkeletalMesh, AnimClass, PhysicsAssetJson, SimulatePhysicsJson, CollisionEnabledJson,
        CollisionResponseJson, EnableGravityJson, VisibilityBasedAnimTickOptionJson, bCompile, bSave);
}

FString UCubelessBlueprintDefaultsLibrary::PrepareBlueprintDefaultsToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("success"), false);
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    Report->SetBoolField(TEXT("both_exact_converters_active"), false);
    Report->SetBoolField(TEXT("both_defaults_decoded_empty_string"), false);
    Report->SetBoolField(TEXT("mutation_attempted"), false);
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("changed"), false);
    Report->SetBoolField(TEXT("postcondition_passed"), false);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0);
    Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetBoolField(TEXT("asset_mutation_attempted"), false);
    Report->SetBoolField(TEXT("compile_requested"), false); Report->SetBoolField(TEXT("compile_attempted"), false);
    Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Report->SetBoolField(TEXT("save_requested"), false); Report->SetBoolField(TEXT("save_attempted"), false);
    Report->SetBoolField(TEXT("saved"), false);
    Report->SetStringField(TEXT("scope"), TEXT("Only exact two in-memory generated UFunction CPP_Default metadata values; no Python body, Engine source, asset, compile, save or generic string converter changes."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("Blueprint defaults schema preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools"))
    { Error = TEXT("Only the exact merged mounted CubelessBlueprintTools generated class may be prepared."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    { Error = TEXT("Blueprint toolset schema is already cached; invalidate the exact class before preparation and registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Blueprint defaults default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("A live official registry subsystem is required."); }
        else
        {
            bool Both = true;
            for (const TPair<FName, FName>& Names : {
                TPair<FName, FName>(TEXT("set_skeletal_mesh_component_anim_defaults"), TEXT("skeletal_mesh")),
                TPair<FName, FName>(TEXT("set_skeletal_mesh_component_anim_defaults"), TEXT("anim_class")) })
            {
                UFunction* Function = ToolsetClass->FindFunctionByName(Names.Key);
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Names.Value) : nullptr;
                auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Both &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.BlueprintDefaultsEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("both_exact_converters_active"), Both);
            if (!Both) { Error = TEXT("Both exact Blueprint inputs must be claimed by the registered Blueprint-only converter before schema preparation."); }
            else if (Cubeless::UE58::PrepareBlueprintDefaults(ToolsetClass, Report, Error))
            { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("blueprint_defaults_preparation_failed"));
    Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}


FString UCubelessBlueprintDefaultsLibrary::SetBlueprintActorTickSettingsRaw(const FString& BlueprintPath,const FString& SettingsJson)
{return Cubeless::UE58::BlueprintDefaults::SetActorTickSettingsRaw(BlueprintPath,SettingsJson);}
