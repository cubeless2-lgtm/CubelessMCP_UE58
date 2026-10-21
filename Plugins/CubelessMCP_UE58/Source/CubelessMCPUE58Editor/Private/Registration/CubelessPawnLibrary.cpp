#include "Registration/CubelessPawnLibrary.h"
#include "Compatibility/UE58/CubelessPawnApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessJsonConverters.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

FString UCubelessPawnLibrary::SetPawnProperties(const FString& Path, const FString& Auto,
    const FString& Yaw, const FString& Pitch, const FString& Roll, const FString& Damage)
{
    return Cubeless::Operations::EncodeVariableResult(
        Cubeless::UE58::Pawn::SetProperties(Path, Auto, Yaw, Pitch, Roll, Damage), TEXT("set_pawn_properties"));
}

FString UCubelessPawnLibrary::PreparePawnPropertiesToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("operation"), TEXT("prepare_pawn_properties_toolset"));
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Key : { TEXT("success"), TEXT("postcondition_passed"), TEXT("default_verified"), TEXT("exact_converter_active"), TEXT("nullable_schema_verified"),
        TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"),
        TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved"), TEXT("metadata_mutation_attempted"), TEXT("metadata_changed"), TEXT("actual_call_packing_verified") }) { Report->SetBoolField(Key, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("outcome_unknown"), false);
    Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("verified_property_count"), 0);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verification_helper_attempt_count"), 0); Report->SetNumberField(TEXT("verification_helper_return_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Exact unregistered canonical Blueprint Pawn method: original auto-possession empty string and four optional boolean unset defaults, field-attached converters/nullable schemas. Helper proves all five before at most one RAM-only auto CPP_Default quoted-empty attachment; separate metadata flags/counts and raw prefixes retained. No asset/load/compile/save; official schema/call packing are separate gates."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("Pawn properties default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools")) { Error = TEXT("Exact current canonical CubelessBlueprintTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate only this canonical Blueprint class before metadata preparation; no retry."); }
    else
    {
        Report->SetNumberField(TEXT("verification_helper_attempt_count"), 1);
        const bool Verified = Cubeless::Registration::VerifyPawnPropertiesToolset(ToolsetClass, Report, Error);
        Report->SetNumberField(TEXT("verification_helper_return_count"), 1);
        if (Verified) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        else if (Error.IsEmpty()) { Error = TEXT("Pawn properties default preparation did not complete; raw RAM prefix retained."); }
    }
    Report->SetStringField(TEXT("stage"), Report->GetNumberField(TEXT("verification_helper_attempt_count")) == 1 ? TEXT("pawn_properties_schema_preparation") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("pawn_properties_preparation_failed")); Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}
