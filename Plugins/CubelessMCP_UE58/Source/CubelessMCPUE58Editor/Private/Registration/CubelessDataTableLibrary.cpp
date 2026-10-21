#include "Registration/CubelessDataTableLibrary.h"
#include "Compatibility/UE58/CubelessDataTableApi.h"
#include "Compatibility/UE58/CubelessDataTableDefaultsConverter.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "UObject/UnrealType.h"

FString UCubelessDataTableLibrary::GetDataTableSchema(const FString& Path) { return Cubeless::UE58::DataTable::GetSchema(Path); }
FString UCubelessDataTableLibrary::ListDataTableRows(const FString& Path, const FString& Contains, int32 MaxRows, bool IncludeValues) { return Cubeless::UE58::DataTable::ListRows(Path, Contains, MaxRows, IncludeValues); }
FString UCubelessDataTableLibrary::GetDataTableRow(const FString& Path, const FString& Name) { return Cubeless::UE58::DataTable::GetRow(Path, Name); }
FString UCubelessDataTableLibrary::AddOrUpdateDataTableRow(const FString& Path, const FString& Name, const FString& Json, bool Save) { return Cubeless::UE58::DataTable::AddOrUpdateRow(Path, Name, Json, Save); }
FString UCubelessDataTableLibrary::RemoveDataTableRow(const FString& Path, const FString& Name, bool Save) { return Cubeless::UE58::DataTable::RemoveRow(Path, Name, Save); }
FString UCubelessDataTableLibrary::CreateDataTable(const FString& Path, const FString& Type, bool Save) { return Cubeless::UE58::DataTable::Create(Path, Type, Save); }

FString UCubelessDataTableLibrary::PrepareDataTableToolset(UClass* ToolsetClass)
{
    auto Report = MakeShared<FJsonObject>(); Report->SetBoolField(TEXT("success"), false); Report->SetBoolField(TEXT("postcondition_passed"), false);
    Report->SetStringField(TEXT("class_path"), ToolsetClass ? ToolsetClass->GetPathName() : FString());
    for (const TCHAR* Field : { TEXT("exact_converter_active"), TEXT("default_verified"), TEXT("mutation_attempted"), TEXT("changed"), TEXT("asset_mutation_attempted"), TEXT("compile_requested"), TEXT("compile_attempted"), TEXT("save_requested"), TEXT("save_attempted"), TEXT("saved") }) { Report->SetBoolField(Field, false); }
    Report->SetBoolField(TEXT("change_state_known"), true); Report->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>());
    Report->SetNumberField(TEXT("metadata_write_attempt_count"), 0); Report->SetNumberField(TEXT("metadata_changed_count"), 0);
    Report->SetStringField(TEXT("scope"), TEXT("Only exact generated DataTable List row_name_contains CPP_Default empty-string metadata; no asset/compile/save/global string converter or Python body changes."));
    FString Error;
    if (!IsInGameThread()) { Error = TEXT("DataTable default preparation requires the game thread."); }
    else if (!ToolsetClass || ToolsetClass->GetPathName() != TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/datatable_toolset_PY.CubelessDataTableTools")) { Error = TEXT("Exact mounted CubelessDataTableTools class required."); }
    else if (UToolsetRegistry::IsToolsetClassRegistered(ToolsetClass)) { Error = TEXT("Invalidate the exact cached toolset before preparation and registration."); }
    else
    {
        auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless DataTable empty default preparation"));
        if (!Subsystem.HasValue()) { Error = TEXT("Live official registry subsystem required."); }
        else
        {
            UFunction* Function = ToolsetClass->FindFunctionByName(TEXT("list_datatable_rows")); FProperty* Property = Function ? FindFProperty<FProperty>(Function, TEXT("row_name_contains")) : nullptr;
            auto Converter = Property ? Subsystem.GetValue()->ToolsetRegistry.GetConverterForProperty(Property) : nullptr;
            const bool Active = Converter.IsValid() && Converter->GetName() == TEXT("CubelessMCP_UE58.DataTableEmptyStringDefaults.v1"); Report->SetBoolField(TEXT("exact_converter_active"), Active);
            if (!Active) { Error = TEXT("Exact DataTable List FString converter must be active before schema preparation."); }
            else if (Cubeless::UE58::PrepareDataTableDefaults(ToolsetClass, Report, Error)) { Report->SetBoolField(TEXT("success"), true); Report->SetBoolField(TEXT("postcondition_passed"), true); }
        }
    }
    Report->SetStringField(TEXT("stage"), Report->GetBoolField(TEXT("mutation_attempted")) ? TEXT("metadata_preparation") : TEXT("schema_preflight")); Report->SetStringField(TEXT("error_code"), Error.IsEmpty() ? FString() : TEXT("datatable_default_preparation_failed")); Report->SetStringField(TEXT("error"), Error);
    FString Json; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Json)); return Json;
}

FString UCubelessDataTableLibrary::AddBlueprintGetDataTableRowNode(const FString& B,const FString& U,const FString& P,const FString& N,const FString& I,const FString& T,bool C)
{return Cubeless::UE58::DataTable::AddGraphRowNode(B,U,P,N,I,T,C);}
FString UCubelessDataTableLibrary::AddBlueprintBreakStringStructNode(const FString& B,const FString& U,const FString& P,const FString& N,const FString& I,const FString& T,bool C)
{return Cubeless::UE58::DataTable::AddGraphStringBreakNode(B,U,P,N,I,T,C);}
