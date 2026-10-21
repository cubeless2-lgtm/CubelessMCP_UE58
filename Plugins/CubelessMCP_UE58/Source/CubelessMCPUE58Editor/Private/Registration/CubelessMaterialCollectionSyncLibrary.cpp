#include "Registration/CubelessMaterialCollectionSyncLibrary.h"
#include "Compatibility/UE58/CubelessMaterialCollectionSyncApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Registration/CubelessMaterialReferenceDefaults.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"

FString UCubelessMaterialCollectionSyncLibrary::SetMaterialParameterCollectionSync(const FString& Action,const FString& SourceCollectionPath,
    const FString& TargetCollectionPath,const FString& ParameterNamesJson,double IntervalSeconds,bool bSetAssetDefaultsOnEnable,bool bSaveOnEnable)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialCollectionSync::Control(Action,SourceCollectionPath,TargetCollectionPath,
        ParameterNamesJson,IntervalSeconds,bSetAssetDefaultsOnEnable,bSaveOnEnable),TEXT("set_material_parameter_collection_sync"));
}

FString UCubelessMaterialCollectionSyncLibrary::PrepareMaterialCollectionSyncToolset(UClass* Class)
{
    auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("operation"),TEXT("prepare_material_collection_sync_toolset"));J->SetStringField(TEXT("class_path"),Class?Class->GetPathName():FString());
    for(const TCHAR* K:{TEXT("success"),TEXT("postcondition_passed"),TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("nullable_schema_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed"),TEXT("actual_call_packing_verified"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("asset_mutation_attempted"),TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_requested"),TEXT("save_attempted"),TEXT("saved")}) { J->SetBoolField(K,false); }
    J->SetBoolField(TEXT("change_state_known"),true);J->SetBoolField(TEXT("outcome_unknown"),false);J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());
    for(const TCHAR* K:{TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("property_count"),TEXT("verified_property_count"),TEXT("verified_parameter_count"),TEXT("verification_helper_attempt_count"),TEXT("verification_helper_return_count")}) { J->SetNumberField(K,0); }
    FString Error;
    if(!IsInGameThread()) { Error=TEXT("Material collection sync preparation requires the game thread."); }
    else if(!Class||Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools")||!Class->IsChildOf(UToolsetDefinition::StaticClass())) { Error=TEXT("Exact canonical MaterialTools class required."); }
    else if(UToolsetRegistry::IsToolsetClassRegistered(Class)) { Error=TEXT("Invalidate only this class before exact preparation; no metadata retry."); }
    else { J->SetNumberField(TEXT("verification_helper_attempt_count"),1);const bool OK=Cubeless::Registration::VerifyMaterialCollectionSyncToolset(Class,J,Error);J->SetNumberField(TEXT("verification_helper_return_count"),1);J->SetBoolField(TEXT("success"),OK);J->SetBoolField(TEXT("postcondition_passed"),OK); }
    J->SetStringField(TEXT("stage"),TEXT("material_collection_sync_schema_preparation"));J->SetStringField(TEXT("error_code"),Error.IsEmpty()?FString():TEXT("material_collection_sync_preparation_failed"));J->SetStringField(TEXT("error"),Error);
    FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;
}
