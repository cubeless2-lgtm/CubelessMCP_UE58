#include "Registration/CubelessAIDiscoveryLibrary.h"
#include "Compatibility/UE58/CubelessAIDiscoveryApi.h"
#include "Compatibility/UE58/CubelessAIDiscoveryDefaultsConverter.h"
#include "Operations/CubelessFunctionOperations.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"

FString UCubelessAIDiscoveryLibrary::PrepareAIDiscoveryToolset(UClass* ToolsetClass)
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
    if (!IsInGameThread()) { Error = TEXT("AI discovery schema preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/ai_toolset_PY.CubelessAITools"))
    { Error = TEXT("Only the exact merged mounted CubelessAITools generated class may be prepared."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    { Error = TEXT("AI toolset schema is already cached; invalidate the exact class before preparation and registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless AI discovery default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("A live official registry subsystem is required."); }
        else
        {
            bool Both = true;
            for (const TPair<FName, FName>& Names : {
                TPair<FName, FName>(TEXT("list_ai_assets"), TEXT("name_filter")),
                TPair<FName, FName>(TEXT("search_ai_behavior_tree_node_classes"), TEXT("query")) })
            {
                UFunction* Function = ToolsetClass->FindFunctionByName(Names.Key);
                FProperty* Property = Function ? FindFProperty<FProperty>(Function, Names.Value) : nullptr;
                auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
                Both &= Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.AIDiscoveryEmptyStringDefaults.v1");
            }
            Report->SetBoolField(TEXT("both_exact_converters_active"), Both);
            if (!Both) { Error = TEXT("Both exact AI inputs must be claimed by the registered AI-only converter before schema preparation."); }
            else if (Cubeless::UE58::PrepareAIDiscoveryDefaults(ToolsetClass, Report, Error))
            { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("ai_discovery_defaults_preparation_failed"));
    Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessAIDiscoveryLibrary::SearchAIBehaviorTreeNodeClasses(const FString& Category,
    const FString& Query, bool bIncludeAbstract, int32 MaxResults)
{
    return Cubeless::Operations::EncodeFunctionResult(
        Cubeless::UE58::AIDiscovery::SearchBehaviorTreeNodeClasses(Category, Query, bIncludeAbstract, MaxResults),
        TEXT("search_ai_behavior_tree_node_classes"));
}
