#include "Registration/CubelessVariableLibrary.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessJsonConverters.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
using namespace Cubeless;
FString UCubelessVariableLibrary::AddBlueprintVariable(const FString& Path, const FString& Name, const FString& Type,
    bool Exposed, const FString& Default, const FString& Category, const FString& Tooltip, const FString& Friendly,
    const FString& TypeObject, bool Array, const FString& Metadata, const FString& Guid, bool Editable, bool Transient)
{ return Operations::EncodeVariableResult(UE58::Variable::Add(Path, Name, Type, Exposed, Default, Category, Tooltip, Friendly, TypeObject, Array, Metadata, Guid, Editable, Transient), TEXT("add_blueprint_variable")); }
FString UCubelessVariableLibrary::InspectBlueprintVariables(const FString& Path)
{ return Operations::EncodeVariableResult(UE58::Variable::Inspect(Path, TEXT(""), false), TEXT("inspect_blueprint_variables")); }
FString UCubelessVariableLibrary::InspectBlueprintVariableMetadata(const FString& Path, const FString& Name)
{ return Operations::EncodeVariableResult(UE58::Variable::Inspect(Path, Name, true), TEXT("inspect_blueprint_variable_metadata")); }
FString UCubelessVariableLibrary::SetBlueprintVariableDefault(const FString& Path, const FString& Name, const FString& Json, bool Compile, bool Save, bool DryRun)
{ return Operations::EncodeVariableResult(UE58::Variable::SetDefault(Path, Name, Json, Compile, Save, DryRun), TEXT("set_blueprint_variable_default")); }

FString UCubelessVariableLibrary::SetBlueprintVariableMetadata(const FString& Path, const FString& Name, const FString& Meta, const FString& Editable, const FString& Instance, const FString& Spawn)
{ return Operations::EncodeVariableResult(UE58::Variable::SetMetadata(Path, Name, Meta, Editable, Instance, Spawn), TEXT("set_blueprint_variable_metadata")); }
FString UCubelessVariableLibrary::SetBlueprintVariableOrder(const FString& Path, const FString& Order, bool Strict, bool Compile, bool Save)
{ return Operations::EncodeVariableResult(UE58::Variable::SetOrder(Path, Order, Strict, Compile, Save), TEXT("set_blueprint_variable_order")); }

FString UCubelessVariableLibrary::PrepareBlueprintVariableLifecycleToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false);
    Report->SetStringField(TEXT("operation"), TEXT("prepare_blueprint_variable_lifecycle_toolset"));
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("default_verified"), TEXT("exact_converter_active"), TEXT("nullable_schema_verified"),
        TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"),
        TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") })
    { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("outcome_unknown"), false);
    Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verification_helper_attempt_count"), 0); Report->SetNumberField(TEXT("verification_helper_return_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Read-only verification of exact canonical metadata method's four optional object/boolean fields, active attached converter, actual unset defaults and explicit nullable type schema; no metadata preparation writes, asset load/edit/compile/save, global converter or old method changes. Actual public registration schema is verified separately."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("Blueprint variable lifecycle schema verification requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools"))
    { Error = TEXT("Exact mounted current CubelessBlueprintTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    { Error = TEXT("Invalidate only the exact cached Blueprint class before schema verification and registration."); }
    else
    {
        Report->SetNumberField(TEXT("verification_helper_attempt_count"), 1);
        const bool Verified = Cubeless::Registration::VerifyBlueprintVariableLifecycleToolset(ToolsetClass, Report, Error);
        Report->SetNumberField(TEXT("verification_helper_return_count"), 1);
        if (Verified) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        else if (Error.IsEmpty()) { Error = TEXT("Exact variable lifecycle schema verification did not complete."); }
    }
    Report->SetStringField(TEXT("stage"), Report->GetNumberField(TEXT("verification_helper_attempt_count")) == 1 ? TEXT("nullable_schema_verification") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("blueprint_variable_lifecycle_preparation_failed"));
    Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessVariableLibrary::RemoveBlueprintVariable(const FString& Path, const FString& Name)
{ return Operations::EncodeVariableResult(UE58::Variable::Remove(Path, Name), TEXT("remove_blueprint_variable")); }

FString UCubelessVariableLibrary::SetBlueprintCategorySorting(const FString& Path, const FString& Order, const FString& Front, bool Append, bool Save, bool Compile)
{ return Operations::EncodeVariableResult(UE58::Variable::SetCategorySorting(Path, Order, Front, Append, Save, Compile), TEXT("set_blueprint_category_sorting")); }

FString UCubelessVariableLibrary::PrepareBlueprintCategorySortingToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false);
    Report->SetStringField(TEXT("operation"), TEXT("prepare_blueprint_category_sorting_toolset"));
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("default_verified"), TEXT("exact_converter_active"), TEXT("nullable_schema_verified"), TEXT("front_default_verified"),
        TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("actual_call_packing_verified"),
        TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") })
    { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("outcome_unknown"), false);
    Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verification_helper_attempt_count"), 0); Report->SetNumberField(TEXT("verification_helper_return_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Exact canonical category sorting direct string-array/public nullable default and empty front defaults only. Helper prevalidates both properties, actual and canonical empty defaults, converters and schemas before at most two ordered current UFunction CPP_Default array/front metadata writes; actual RAM metadata observations/counts are retained separately from asset flags. No asset load/edit/fullcompile/save, global converter or old method changes. Actual public schema and call packing are verified separately."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("Blueprint category sorting schema preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/toolsets_PY.CubelessBlueprintTools"))
    { Error = TEXT("Exact mounted current CubelessBlueprintTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass))
    { Error = TEXT("Invalidate only the exact cached Blueprint class before schema preparation and registration."); }
    else
    {
        Report->SetNumberField(TEXT("verification_helper_attempt_count"), 1);
        const bool Verified = Cubeless::Registration::VerifyBlueprintCategorySortingToolset(ToolsetClass, Report, Error);
        Report->SetNumberField(TEXT("verification_helper_return_count"), 1);
        if (Verified) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        else if (Error.IsEmpty()) { Error = TEXT("Exact category sorting schema preparation did not complete."); }
    }
    Report->SetStringField(TEXT("stage"), Report->GetNumberField(TEXT("verification_helper_attempt_count")) == 1 ? TEXT("category_schema_preparation") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("blueprint_category_sorting_preparation_failed"));
    Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessVariableLibrary::SetBlueprintCustomEventMetadata(const FString& Path, const FString& Name, const FString& Description,
    const FString& NodeId, const FString& EventName, const FString& Tooltip, const FString& CallInEditorJson, bool Comment, bool Compile, bool Save)
{ return Operations::EncodeVariableResult(UE58::Variable::SetCustomEventMetadata(Path, Name, Description, NodeId, EventName, Tooltip, CallInEditorJson, Comment, Compile, Save), TEXT("set_blueprint_custom_event_metadata")); }
FString UCubelessVariableLibrary::PrepareBlueprintCustomEventMetadataToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>();
    Report->SetStringField(TEXT("operation"), TEXT("prepare_blueprint_custom_event_metadata_toolset"));
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Key : {TEXT("success"), TEXT("postcondition_passed"), TEXT("default_verified"), TEXT("exact_converter_active"), TEXT("nullable_schema_verified"),
        TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"),
        TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved"), TEXT("metadata_mutation_attempted"), TEXT("metadata_changed"), TEXT("actual_call_packing_verified")}) { Report->SetBoolField(Key, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetBoolField(TEXT("outcome_unknown"), false);
    Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Report->SetNumberField(TEXT("verified_property_count"), 0);
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetNumberField(TEXT("verification_helper_attempt_count"), 0); Report->SetNumberField(TEXT("verification_helper_return_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Exact unregistered canonical Graph custom-event metadata method: five empty string defaults and one optional boolean unset default, selected converters/parsed defaults/schema proofs. Helper prevalidates all six targets before at most five ordered RAM-only quoted-empty metadata attachments; separate RAM flags/counts and raw partial rows retained. No asset/load/compile/save or arbitrary global converter edits. Official schema and actual call packing are separate gates."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("Custom-event metadata default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/graph_toolset_PY.CubelessBlueprintGraphTools"))
    { Error = TEXT("Exact current canonical CubelessBlueprintGraphTools generated class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate only this canonical Graph class before metadata preparation; no retry."); }
    else
    {
        Report->SetNumberField(TEXT("verification_helper_attempt_count"), 1);
        const bool Verified = Cubeless::Registration::VerifyBlueprintCustomEventMetadataToolset(ToolsetClass, Report, Error);
        Report->SetNumberField(TEXT("verification_helper_return_count"), 1);
        if (Verified) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        else if (Error.IsEmpty()) { Error = TEXT("Custom-event metadata preparation did not complete; raw RAM prefix retained."); }
    }
    Report->SetStringField(TEXT("stage"), Report->GetNumberField(TEXT("verification_helper_attempt_count")) == 1 ? TEXT("custom_event_metadata_schema_preparation") : TEXT("schema_preflight"));
    Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("custom_event_metadata_preparation_failed")); Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessVariableLibrary::AddBlueprintLocalVariable(const FString& Path, const FString& Name, const FString& Type,
    const FString& DefaultJson, const FString& Category, const FString& Tooltip, const FString& TypeObject, bool Array,
    const FString& GraphName, const FString& GraphId, const FString& GraphType, bool CreateGraph)
{ return Operations::EncodeVariableResult(UE58::Variable::AddLocal(Path, Name, Type, DefaultJson, Category, Tooltip, TypeObject, Array, GraphName, GraphId, GraphType, CreateGraph), TEXT("add_blueprint_local_variable")); }
