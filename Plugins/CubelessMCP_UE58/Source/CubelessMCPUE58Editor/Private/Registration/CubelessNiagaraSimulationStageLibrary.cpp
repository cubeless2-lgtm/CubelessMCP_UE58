#include "Registration/CubelessNiagaraSimulationStageLibrary.h"
#include "Operations/CubelessNiagaraSimulationStageOperations.h"
#include "Registration/CubelessNiagaraStageConverters.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"

FString UCubelessNiagaraSimulationStageLibrary::PrepareStageToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false);
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    Report->SetBoolField(TEXT("both_exact_converters_active"), false); Report->SetBoolField(TEXT("normalized_empty_array_default"), false);
    FString Error; bool Normalized = false; auto Subsystem = UToolsetRegistrySubsystem::Get();
    if (!IsInGameThread() || !Subsystem.HasValue()) { Error = TEXT("Stage preparation requires game thread and live official registry."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/niagara_simulation_stages_toolset_PY.CubelessNiagaraSimulationStageTools"))
    { Error = TEXT("Only the exact mounted Stage Python generated class may be prepared."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Stage toolset schema is already cached; prepare before registration and restart instead of changing registered metadata."); }
    else
    {
        bool Both = true;
        for (const FName Name : {FName(TEXT("add_niagara_grid_simulation_stage")), FName(TEXT("set_niagara_simulation_stage_settings"))})
        { auto* Function = ToolsetClass->FindFunctionByName(Name); auto* Property = Function ? FindFProperty<FProperty>(Function, TEXT("element_count")) : nullptr;
            auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
            Both &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.NiagaraStageElementCount.v1"); }
        Report->SetBoolField(TEXT("both_exact_converters_active"), Both);
        if (!Both) { Error = TEXT("Both exact Stage element_count inputs must be claimed by the Stage-only converter."); }
        else if (Cubeless::Registration::PrepareNiagaraStageArrayDefaults(ToolsetClass, Error, Normalized))
        { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("normalized_empty_array_default"), Normalized);
            Report->SetArrayField(TEXT("array_default"), {}); Report->SetField(TEXT("null_default"), MakeShared<FJsonValueNull>());
            Report->SetField(TEXT("public_default"), MakeShared<FJsonValueNull>()); Report->SetStringField(TEXT("property_type"), TEXT("FArrayProperty<FIntProperty>"));
        }
    }
    Report->SetStringField(TEXT("error"), Error); FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessNiagaraSimulationStageLibrary::InspectNiagaraSimulationStageTarget(
    const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName)
{
    return Cubeless::Operations::InspectNiagaraSimulationStageTarget(SystemPath, EmitterIndexJson, EmitterName);
}
FString UCubelessNiagaraSimulationStageLibrary::AddNiagaraGridSimulationStage(
    const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageName, const FString& DataInterfaceName, const FString& InsertIndexJson,
    bool bSkipIfDuplicate, bool bEnabled, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::AddNiagaraGridSimulationStage(SystemPath, EmitterIndexJson, EmitterName,
        StageName, DataInterfaceName, InsertIndexJson, bSkipIfDuplicate, bEnabled, OptionsJson,
        bAllowSourceEdit, bSave, bRequestCompile);
}
FString UCubelessNiagaraSimulationStageLibrary::InspectNiagaraSimulationStageSettingsTarget(
    const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName)
{
    return Cubeless::Operations::InspectNiagaraSimulationStageSettingsTarget(SystemPath,
        EmitterIndexJson, EmitterName, StageIndexJson, StageName);
}
FString UCubelessNiagaraSimulationStageLibrary::SetNiagaraSimulationStageSettings(
    const FString& SystemPath, const FString& EmitterIndexJson, const FString& EmitterName,
    const FString& StageIndexJson, const FString& StageName, const FString& OptionsJson,
    bool bAllowSourceEdit, bool bSave, bool bRequestCompile)
{
    return Cubeless::Operations::SetNiagaraSimulationStageSettings(SystemPath,
        EmitterIndexJson, EmitterName, StageIndexJson, StageName, OptionsJson,
        bAllowSourceEdit, bSave, bRequestCompile);
}
