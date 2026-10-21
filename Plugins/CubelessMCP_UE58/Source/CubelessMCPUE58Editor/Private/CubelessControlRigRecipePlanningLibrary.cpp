#include "CubelessControlRigRecipePlanningLibrary.h"
#include "Compatibility/UE58/CubelessControlRigRecipePlanningApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessControlRigPresetRecipes.h"
#include "Operations/CubelessVariableOperations.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace
{
bool HGPresetTextSafe(const FString& Text,int32 Limit){if(Text.Len()>Limit)return false;for(int32 I=0;I<Text.Len();++I)if(Text[I]==0)return false;return true;}
bool HGPresetInputBound(const FString& Text)
{
 if(!HGPresetTextSafe(Text,1048576))return false;auto Reader=TJsonReaderFactory<>::Create(Text);EJsonNotation Note;int32 Depth=0,Slots=0;while(Reader->ReadNext(Note)){if(++Slots>262144)return false;if(Note==EJsonNotation::ObjectStart||Note==EJsonNotation::ArrayStart){if(++Depth>64)return false;}else if(Note==EJsonNotation::ObjectEnd||Note==EJsonNotation::ArrayEnd){if(--Depth<0)break;}else if(Note==EJsonNotation::Number){if(!FMath::IsFinite(Reader->GetValueAsNumber()))return false;}else if(Note==EJsonNotation::String){if(!HGPresetTextSafe(Reader->GetValueAsString(),1048576))return false;}else if(Note==EJsonNotation::Error)break;}
 // Syntax/type errors remain the original pure Operations invalid_json result; only unsafe bounds are rejected here.
 return true;
}
bool HGPresetResponse(const FString& Text,TSharedPtr<FJsonObject>& Out)
{
 if(Text.IsEmpty()||!HGPresetTextSafe(Text,32*1024*1024))return false;int32 Depth=0;bool Quoted=false,Escape=false;
 for(int32 I=0;I<Text.Len();++I){const TCHAR C=Text[I];if(Quoted){if(Escape)Escape=false;else if(C=='\\')Escape=true;else if(C=='"')Quoted=false;}else if(C=='"')Quoted=true;else if(C=='{'||C=='['){if(++Depth>64)return false;}else if(C=='}'||C==']'){if(--Depth<0)return false;}}if(Quoted||Depth!=0)return false;
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out)||!Out)return false;TArray<TSharedPtr<FJsonValue>> Pending{MakeShared<FJsonValueObject>(Out)};int32 Slots=0;
 while(!Pending.IsEmpty()){const auto V=Pending.Pop(EAllowShrinking::No);if(!V||++Slots>262144)return false;if(V->Type==EJson::Number&&!FMath::IsFinite(V->AsNumber()))return false;if(V->Type==EJson::String&&!HGPresetTextSafe(V->AsString(),32*1024*1024))return false;if(V->Type==EJson::Array)Pending.Append(V->AsArray());else if(V->Type==EJson::Object){for(const auto& E:V->AsObject()->Values){if(!HGPresetTextSafe(FString(E.Key),32*1024*1024))return false;Pending.Add(E.Value);}}if(Pending.Num()>262144)return false;}
 const auto Success=Out->TryGetField(TEXT("success"));return Success&&Success->Type==EJson::Boolean;
}
}
FString UCubelessControlRigRecipePlanningLibrary::ProfileControlRigSkeleton(const FString& RequestJson)
{
    return Cubeless::UE58::ControlRigRecipePlanning::ProfileSkeleton(RequestJson);
}
FString UCubelessControlRigRecipePlanningLibrary::MakeControlRigPresetRecipe(const FString& PresetJson)
{
 using Cubeless::UE58::Variable::FResult;constexpr const TCHAR* Operation=TEXT("make_controlrig_preset_recipe_data");FResult R;R.Compiled=false;R.Data->SetBoolField(TEXT("pure_data"),true);R.Data->SetBoolField(TEXT("operation_complete"),true);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("explicit_compile_requested"),false);R.Data->SetBoolField(TEXT("explicit_compile_attempted"),false);R.Data->SetBoolField(TEXT("explicit_save_requested"),false);R.Data->SetBoolField(TEXT("explicit_save_attempted"),false);R.Data->SetBoolField(TEXT("recipe_compile_save_policy_is_data_only"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);auto Prefix=MakeShared<FJsonObject>();Prefix->SetBoolField(TEXT("attempted"),false);Prefix->SetBoolField(TEXT("returned"),false);R.Data->SetObjectField(TEXT("actual_pure_data_call_prefix"),Prefix);
 if(!HGPresetInputBound(PresetJson)){R.ErrorCode=TEXT("controlrig_preset_recipe_carrier_invalid");R.Error=TEXT("Preset JSON carrier must be finite within 1Mi UTF16/depth64/262144 tokens without embedded NUL");R.Stage=TEXT("parse_preset_carrier");return Cubeless::Operations::EncodeVariableResult(R,Operation);}
 R.Stage=TEXT("make_preset_recipe_data");Prefix->SetBoolField(TEXT("attempted"),true);const FString Plain=Cubeless::Operations::ControlRigPresetRecipes::MakePresetRecipeJson(PresetJson);Prefix->SetBoolField(TEXT("returned"),true);Prefix->SetNumberField(TEXT("response_characters"),Plain.Len());TSharedPtr<FJsonObject> Legacy;
 if(!HGPresetResponse(Plain,Legacy)){R.ErrorCode=TEXT("controlrig_preset_recipe_response_invalid");R.Error=TEXT("Pure preset result is not a finite bounded original legacy JSON object with Boolean success");R.Data->SetStringField(TEXT("preset_response_raw_prefix"),Plain.Left(4096));R.Data->SetBoolField(TEXT("preset_response_raw_prefix_truncated"),Plain.Len()>4096);R.Data->SetBoolField(TEXT("recipe_result_unavailable"),true);return Cubeless::Operations::EncodeVariableResult(R,Operation);}
 R.Data->SetObjectField(TEXT("legacy_result"),Legacy);const bool Success=Legacy->GetBoolField(TEXT("success"));R.bSuccess=Success;R.bPostconditionPassed=Success;R.bChangeStateKnown=true;if(Success)R.Stage=TEXT("complete");else{R.ErrorCode=TEXT("controlrig_preset_recipe_rejected");R.Error=TEXT("Pure preset request was rejected; original issues and recipe result are preserved");}return Cubeless::Operations::EncodeVariableResult(R,Operation);
}
