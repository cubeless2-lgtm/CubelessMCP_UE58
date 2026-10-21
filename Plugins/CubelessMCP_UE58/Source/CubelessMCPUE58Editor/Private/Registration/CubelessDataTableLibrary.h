#pragma once
#include "CoreMinimal.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "CubelessDataTableLibrary.generated.h"

UCLASS()
class UCubelessDataTableLibrary : public UBlueprintFunctionLibrary
{
    GENERATED_BODY()
public:
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="get_datatable_schema"))
    static FString GetDataTableSchema(const FString& DatatablePath);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="list_datatable_rows"))
    static FString ListDataTableRows(const FString& DatatablePath, const FString& RowNameContains, int32 MaxRows, bool bIncludeValues);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="get_datatable_row"))
    static FString GetDataTableRow(const FString& DatatablePath, const FString& RowName);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="add_or_update_datatable_row"))
    static FString AddOrUpdateDataTableRow(const FString& DatatablePath, const FString& RowName, const FString& RowValuesJson, bool bSave);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="remove_datatable_row"))
    static FString RemoveDataTableRow(const FString& DatatablePath, const FString& RowName, bool bSave);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="create_datatable"))
    static FString CreateDataTable(const FString& AssetPath, const FString& RowStructPath, bool bSave);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="prepare_datatable_toolset"))
    static FString PrepareDataTableToolset(UClass* ToolsetClass);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="add_blueprint_get_data_table_row_node"))
    static FString AddBlueprintGetDataTableRowNode(const FString& BlueprintPath,const FString& RowStructPath,const FString& NodePositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
    UFUNCTION(BlueprintCallable, meta=(AIIgnore, ScriptName="add_blueprint_break_string_struct_node"))
    static FString AddBlueprintBreakStringStructNode(const FString& BlueprintPath,const FString& RowStructPath,const FString& NodePositionJson,const FString& GraphName,const FString& GraphId,const FString& GraphType,bool bCreateGraphIfMissing);
};
