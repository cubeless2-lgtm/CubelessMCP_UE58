#include "Registration/CubelessMaterialReferenceLibrary.h"
#include "Compatibility/UE58/CubelessMaterialReferenceApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessMaterialReferenceLibrary::ReplaceMaterialCollectionReferences(const FString& Path,
    const FString& Target, const FString& Source, const FString& Type, bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialReference::ReplaceCollections(Path, Target, Source, Type, Save), TEXT("replace_material_collection_references")); }

FString UCubelessMaterialReferenceLibrary::ReplaceMaterialTextureReferences(const FString& Path,
    const FString& Json, const FString& Type, bool Save)
{ return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::MaterialReference::ReplaceTextures(Path, Json, Type, Save), TEXT("replace_material_texture_references")); }

#include "Registration/CubelessMaterialReferenceDefaults.h"
#include "ToolsetRegistry/UToolsetRegistry.h"
#include "Dom/JsonValue.h"
#include "Serialization/JsonSerializer.h"
FString UCubelessMaterialReferenceLibrary::PrepareMaterialReferenceToolset(UClass* Class)
{
 auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("operation"),TEXT("prepare_material_reference_toolset"));J->SetStringField(TEXT("class_path"),Class?Class->GetPathName():FString());
 for(const TCHAR* K:{TEXT("success"),TEXT("postcondition_passed"),TEXT("default_verified"),TEXT("exact_converter_active"),TEXT("type_schema_verified"),TEXT("metadata_mutation_attempted"),TEXT("metadata_changed"),TEXT("actual_call_packing_verified"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_requested"),TEXT("compile_attempted"),TEXT("save_requested"),TEXT("save_attempted"),TEXT("saved")}) { J->SetBoolField(K,false); }
 J->SetBoolField(TEXT("change_state_known"),true);J->SetBoolField(TEXT("outcome_unknown"),false);J->SetField(TEXT("compiled"),MakeShared<FJsonValueNull>());
 for(const TCHAR* K:{TEXT("metadata_write_attempt_count"),TEXT("metadata_changed_count"),TEXT("verified_property_count"),TEXT("verification_helper_attempt_count"),TEXT("verification_helper_return_count")}) { J->SetNumberField(K,0); }
 FString Error;
 if(!IsInGameThread()) { Error=TEXT("Material reference preparation requires the game thread."); }
 else if(!Class || Class->GetPathName()!=TEXT("/CubelessMCP_UE58/Python/cubeless_mcp_ue58/registration/material_toolset_PY.CubelessMaterialTools") || !Class->IsChildOf(UToolsetDefinition::StaticClass())) { Error=TEXT("Exact canonical MaterialTools class required."); }
 else if(UToolsetRegistry::IsToolsetClassRegistered(Class)) { Error=TEXT("Invalidate only this class before exact preparation; no metadata retry."); }
 else { J->SetNumberField(TEXT("verification_helper_attempt_count"),1);const bool OK=Cubeless::Registration::VerifyMaterialReferenceToolset(Class,J,Error);J->SetNumberField(TEXT("verification_helper_return_count"),1);J->SetBoolField(TEXT("success"),OK);J->SetBoolField(TEXT("postcondition_passed"),OK); }
 J->SetStringField(TEXT("stage"),TEXT("material_reference_schema_preparation"));J->SetStringField(TEXT("error_code"),Error.IsEmpty()?FString():TEXT("material_reference_preparation_failed"));J->SetStringField(TEXT("error"),Error);
 FString Json;FJsonSerializer::Serialize(J,TJsonWriterFactory<>::Create(&Json));return Json;
}
