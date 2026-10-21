#include "CubelessTextureMaterialAttachmentLibrary.h"
#include "Registration/CubelessTextureMaterialToolset.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetToolsModule.h"
#include "IAssetTools.h"
#include "AssetImportTask.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Editor/EditorEngine.h"
#include "Subsystems/EditorActorSubsystem.h"
#include "Engine/Texture2D.h"
#include "Engine/StaticMesh.h"
#include "Components/StaticMeshComponent.h"
#include "Materials/MaterialInterface.h"
#include "InterchangeManager.h"
#include "HAL/IConsoleManager.h"
#include "HAL/FileManager.h"
#include "Misc/Paths.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"

namespace
{
using FTextureMaterialAttachment = bool (*)(UClass*, const TSharedRef<FJsonObject>&, FString&);
FString TextureMaterialAttachmentReceipt(UClass* ToolsetClass, FTextureMaterialAttachment Function, const TCHAR* Operation)
{
    auto Report = MakeShared<FJsonObject>(); FString Error;
    const bool Ok = Function(ToolsetClass, Report, Error);
    Report->SetStringField(TEXT("operation"), Operation);
    // Admission-only Verify deliberately returns false; do not promote it to general readiness.
    Report->SetBoolField(TEXT("ready"), Ok); Report->SetBoolField(TEXT("verified"), Ok);
    Report->SetStringField(TEXT("error"), Error);
    Report->SetBoolField(TEXT("asset_mutation"), false); Report->SetBoolField(TEXT("metadata_mutation"), false);
    Report->SetBoolField(TEXT("actual_named_wire_verified"), false);
    FString Out; FJsonSerializer::Serialize(Report, TJsonWriterFactory<>::Create(&Out)); return Out;
}
}
FString UCubelessTextureMaterialAttachmentLibrary::InstallTextureMaterialToolset(UClass* ToolsetClass)
{
    return TextureMaterialAttachmentReceipt(ToolsetClass, &Cubeless::Registration::InstallTextureMaterialToolset, TEXT("install_texture_material_authoring_toolset"));
}
FString UCubelessTextureMaterialAttachmentLibrary::VerifyTextureMaterialToolset(UClass* ToolsetClass)
{
    return TextureMaterialAttachmentReceipt(ToolsetClass, &Cubeless::Registration::VerifyTextureMaterialToolset, TEXT("verify_texture_material_authoring_toolset"));
}
FString UCubelessTextureMaterialAttachmentLibrary::RemoveTextureMaterialToolset(UClass* ToolsetClass)
{
    return TextureMaterialAttachmentReceipt(ToolsetClass, &Cubeless::Registration::RemoveTextureMaterialToolset, TEXT("remove_texture_material_authoring_toolset"));
}

// Fixed Editor leaves. Import requires the owned Named ticker; Apply uses the idle Editor context.
namespace
{
using FRPResult=Cubeless::UE58::Variable::FResult;
bool RPLive(const UObject* O){return O&&IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad);}
struct FRPIdentity
{
 TStrongObjectPtr<UObject> Object,Class,Outer,Package,ActorWorld,ComponentOwner;FString Path;bool ActorRole=false,ComponentRole=false;
 bool Current()const
 {
  if(!RPLive(Object.Get())||!RPLive(Class.Get())||static_cast<UClass*>(Class.Get())->HasAnyClassFlags(CLASS_NewerVersionExists)||(Outer.Get()&&!RPLive(Outer.Get()))||!RPLive(Package.Get())||Object->GetClass()!=Class.Get()||Object->GetOuter()!=Outer.Get()||Object->GetOutermost()!=Package.Get()||Object->GetPathName()!=Path)return false;
  if(ActorRole&&((ActorWorld.Get()&&!RPLive(ActorWorld.Get()))||static_cast<AActor*>(Object.Get())->GetWorld()!=ActorWorld.Get()))return false;
  if(ComponentRole&&((ComponentOwner.Get()&&!RPLive(ComponentOwner.Get()))||static_cast<UActorComponent*>(Object.Get())->GetOwner()!=ComponentOwner.Get()))return false;
  return true;
 }
};
struct FRPWork
{
 const bool RequireStandaloneImport;FRPResult R;bool Stopped=false;TStrongObjectPtr<UEditorEngine> Editor;TStrongObjectPtr<UWorld> World;TArray<TUniquePtr<FRPIdentity>> Held;TArray<TSharedPtr<FJsonValue>> Calls;
 explicit FRPWork(bool InRequireStandaloneImport=true):RequireStandaloneImport(InRequireStandaloneImport){R.Data->SetBoolField(TEXT("fixed_texture_import_apply_leaf"),true);R.Data->SetBoolField(TEXT("full_effect_state_known"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);}
 bool Fail(const TCHAR* Code,const FString& Message,bool Unknown=false){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Message;}Stopped=true;R.bSuccess=false;R.bPostconditionPassed=false;if(Unknown)R.bChangeStateKnown=false;return false;}
 bool Plain()
 {
  if(Stopped)return false;
  if(!IsInGameThread()||IsGarbageCollecting()||IsEngineExitRequested()||UE::IsSavingPackage()||!GEditor||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return Fail(TEXT("texture_import_editor_context_unavailable"),TEXT("Idle live Editor outside PIE/GC/exit/save required"),true);
  if(RequireStandaloneImport&&!Cubeless::Registration::TextureMaterialStandaloneImportAdmitted())return Fail(TEXT("python_import_requires_standalone_ticker"),TEXT("Only the exact owned Named texture import next-tick delegation may import; no Batch/direct fallback"),R.bMutationAttempted);
  if(!Editor.IsValid()){Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());}
  if(!RPLive(Editor.Get())||GEditor!=Editor.Get()||!RPLive(World.Get())||World->bIsTearingDown||GEditor->GetEditorWorldContext().World()!=World.Get())return Fail(TEXT("texture_import_editor_context_changed"),TEXT("Captured Editor/world identity changed"),true);
  return true;
 }
 bool Current(){if(!Plain())return false;for(const auto& H:Held)if(!H->Current())return Fail(TEXT("texture_import_held_role_changed"),TEXT("Held object class/outer/package/path or liveness changed"),true);return true;}
 bool Hold(UObject* O)
 {
  if(!O)return true;for(const auto& H:Held)if(H->Object.Get()==O)return Current();
  TStrongObjectPtr<UObject> Immediate(O);if(!Current()||!RPLive(O))return Fail(TEXT("texture_import_returned_object_unavailable"),TEXT("Returned object is not current and complete"),true);
  auto H=MakeUnique<FRPIdentity>();H->Object.Reset(O);H->Class.Reset(O->GetClass());H->Outer.Reset(O->GetOuter());H->Package.Reset(O->GetOutermost());
  if(!RPLive(H->Class.Get())||(H->Outer.Get()&&!RPLive(H->Outer.Get()))||!RPLive(H->Package.Get()))return Fail(TEXT("texture_import_returned_binding_unavailable"),TEXT("Actual returned class/outer/package unavailable"),true);
  if(O->IsA<AActor>()){H->ActorRole=true;H->ActorWorld.Reset(static_cast<AActor*>(O)->GetWorld());if(H->ActorWorld.Get()&&!RPLive(H->ActorWorld.Get()))return Fail(TEXT("texture_material_actor_world_unavailable"),TEXT("Actual actor world is unavailable"),true);}
  if(O->IsA<UActorComponent>()){H->ComponentRole=true;H->ComponentOwner.Reset(static_cast<UActorComponent*>(O)->GetOwner());if(H->ComponentOwner.Get()&&!RPLive(H->ComponentOwner.Get()))return Fail(TEXT("texture_material_component_owner_unavailable"),TEXT("Actual component owner is unavailable"),true);}
  H->Path=O->GetPathName();Held.Add(MoveTemp(H));return Current();
 }
 template<typename F>bool Call(const TCHAR* Stage,bool Mutation,F&& Invoke)
 {
  if(!Current())return false;R.Stage=Stage;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("stage"),Stage);Row->SetBoolField(TEXT("entered"),true);Row->SetBoolField(TEXT("mutation_requested"),Mutation);Calls.Add(MakeShared<FJsonValueObject>(Row));R.bMutationAttempted|=Mutation;R.bChangeStateKnown=false;
  Invoke();Row->SetBoolField(TEXT("returned"),true);if(!Current())return false;R.bChangeStateKnown=true;Row->SetBoolField(TEXT("continuation_boundary_current"),true);return true;
 }
 FString Finish(const TCHAR* Operation,const TSharedRef<FJsonObject>& Legacy,bool IncludeLegacyErrorCode=true)
 {
  Legacy->SetBoolField(TEXT("success"),R.bSuccess);if(!R.bSuccess){Legacy->SetStringField(TEXT("message"),R.Error);if(IncludeLegacyErrorCode)Legacy->SetStringField(TEXT("error_code"),R.ErrorCode);}
  R.Data->SetObjectField(TEXT("legacy_result"),Legacy);R.Data->SetArrayField(TEXT("sdk_calls"),Calls);R.Data->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted&&!R.bSuccess);R.Data->SetBoolField(TEXT("mutation_state_unknown"),!R.bChangeStateKnown);R.Data->SetBoolField(TEXT("standalone_import_dispatch_only"),RequireStandaloneImport);
  return Cubeless::Operations::EncodeVariableResult(R,Operation);
 }
};
struct FRPCVar
{
 FString Name,Value;IConsoleVariable* Variable=nullptr;EConsoleVariableFlags Flags=ECVF_Default;bool Required=false,Disable=false,Captured=false;
};
struct FRPLegacyImport
{
 FRPWork& W;TArray<FRPCVar> Vars;bool RestoreAttempted=false;TArray<TSharedPtr<FJsonValue>> Rows;
 explicit FRPLegacyImport(FRPWork& In):W(In)
 {
  auto Add=[&](const TCHAR* N,bool Required,bool Disable){FRPCVar V;V.Name=N;V.Required=Required;V.Disable=Disable;Vars.Add(MoveTemp(V));};
  Add(TEXT("Interchange.FeatureFlags.Import.Enable"),true,true);Add(TEXT("Interchange.FeatureFlags.Import.FBX"),false,true);Add(TEXT("Interchange.FeatureFlags.Import.FBX.ToLevel"),false,true);Add(TEXT("Interchange.FeatureFlags.Import.OBJ"),false,true);
  for(const TCHAR* S:{TEXT("BMP"),TEXT("DDS"),TEXT("EXR"),TEXT("HDR"),TEXT("JPG"),TEXT("PNG"),TEXT("PSD"),TEXT("TGA"),TEXT("TIFF"),TEXT("UEJPEG")})Add(*(FString(TEXT("Interchange.FeatureFlags.Import."))+S),false,false);
 }
 ~FRPLegacyImport(){if(!RestoreAttempted)Restore();}
 bool Activate()
 {
  bool Importing=false;if(!W.Call(TEXT("Interchange_IsImporting_before"),false,[&]{Importing=UInterchangeManager::IsImporting();}))return false;
  if(Importing)return W.Fail(TEXT("interchange_import_in_progress"),TEXT("Another Interchange import is active; no writer"));
  for(auto& V:Vars)
  {
   V.Variable=IConsoleManager::Get().FindConsoleVariable(*V.Name);auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("name"),V.Name);Row->SetBoolField(TEXT("available"),V.Variable!=nullptr);Rows.Add(MakeShared<FJsonValueObject>(Row));
   if(!V.Variable){if(V.Required)return W.Fail(TEXT("legacy_import_cvar_missing"),V.Name);continue;}
   V.Value=V.Variable->GetString();V.Flags=V.Variable->GetFlags();V.Captured=true;Row->SetStringField(TEXT("value_before"),V.Value);Row->SetNumberField(TEXT("flags_before"),static_cast<uint32>(V.Flags));
  }
  for(auto& V:Vars)if(V.Captured&&V.Disable)
  {
   if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable)return W.Fail(TEXT("legacy_import_cvar_registry_changed"),V.Name,true);
   // UE58 SetWithCurrentPriority's default MinPriority excludes Constructor.
   // Explicit captured priority preserves the original value/priority scope there too.
   const auto CapturedPriority=static_cast<EConsoleVariableFlags>(V.Flags&ECVF_SetByMask);
   if(!W.Call(TEXT("Interchange_CVar_disable_captured_priority"),false,[&]{V.Variable->Set(false,CapturedPriority);}))return false;
   if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable||V.Variable->GetBool()||V.Variable->GetFlags()!=V.Flags)return W.Fail(TEXT("legacy_import_cvar_disable_failed"),V.Name,true);
  }
  return true;
 }
 bool Restore()
 {
  if(RestoreAttempted)return false;RestoreAttempted=true;bool Ok=true;
  // Mandatory native console scope cleanup, even after an asset boundary became unknown.
  // No asset/object observers or later import/apply calls are made by this path.
  for(int32 I=Vars.Num()-1;I>=0;--I)
  {
   auto& V=Vars[I];if(!V.Captured)continue;auto Row=Rows[I]->AsObject();
   if(!IsInGameThread()||IsEngineExitRequested()||IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable){Ok=false;Row->SetBoolField(TEXT("restore_skipped_unsafe"),true);continue;}
   if(V.Variable->GetString()!=V.Value||V.Variable->GetFlags()!=V.Flags)
   {
    const auto OriginalSetBy=static_cast<EConsoleVariableFlags>(V.Flags&ECVF_SetByMask);
    for(int32 Attempt=0;Attempt<16;++Attempt){const auto SetBy=static_cast<EConsoleVariableFlags>(V.Variable->GetFlags()&ECVF_SetByMask);if(SetBy==OriginalSetBy)break;V.Variable->Unset(SetBy);if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable){Ok=false;break;}if((V.Variable->GetFlags()&ECVF_SetByMask)==SetBy){Ok=false;break;}}
    if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable){Row->SetBoolField(TEXT("restore_registry_changed"),true);continue;}
    if((V.Variable->GetFlags()&ECVF_SetByMask)!=OriginalSetBy)Ok=false;
    V.Variable->SetFlags(V.Flags);if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable){Ok=false;Row->SetBoolField(TEXT("restore_registry_changed"),true);continue;}V.Variable->Set(*V.Value,OriginalSetBy);
    if(IConsoleManager::Get().FindConsoleVariable(*V.Name)!=V.Variable){Ok=false;Row->SetBoolField(TEXT("restore_registry_changed"),true);continue;}
   }
   const bool Same=V.Variable->GetString()==V.Value&&V.Variable->GetFlags()==V.Flags;Ok&=Same;Row->SetBoolField(TEXT("restored"),Same);Row->SetStringField(TEXT("value_after"),V.Variable->GetString());Row->SetNumberField(TEXT("flags_after"),static_cast<uint32>(V.Variable->GetFlags()));
  }
  W.R.Data->SetArrayField(TEXT("interchange_cvar_scope"),Rows);W.R.Data->SetBoolField(TEXT("interchange_restore_verified"),Ok);if(!Ok)W.Fail(TEXT("legacy_import_restore_failed"),TEXT("Original CVar value/priority/flags could not be restored; no retry"),true);return Ok;
 }
};
bool RPString(const FString& S){if(S.Len()>=NAME_SIZE)return false;for(TCHAR C:S)if(C==0)return false;return true;}
bool RPActorLabel(FRPWork& W,AActor* Actor,FString& Label)
{
 if(!W.Current())return false;const FString Before=Actor->GetActorLabel(false);
 // bCreateIfNone=True is the original getter. A missing label is a real lazy
 // writer and broadcasts OnActorLabelChanged; preserve that order and fence.
 if(!W.Call(TEXT("GetActorLabel_original_lazy_default"),Before.IsEmpty(),[&]{Label=Actor->GetActorLabel();}))return false;
 W.R.bChanged|=Before!=Label;return true;
}
}

FString UCubelessTextureMaterialAttachmentLibrary::ImportTextureFile(const FString& ImagePath,const FString& UnrealFolder,const FString& AssetName)
{
 FRPWork W;auto L=MakeShared<FJsonObject>();W.R.bSaveRequested=true;L->SetBoolField(TEXT("import_attempted"),false);
 bool LegacyErrorCode=true,ValidationError=false;
 auto Finish=[&]()
 {
  // Original leaf fields remain distinct from the enclosing Native failure code.
  // Copy only already-observed values here, including after an unknown boundary.
  L->SetBoolField(TEXT("mutation_attempted"),W.R.bMutationAttempted);
  if(!L->HasField(TEXT("mutation_performed"))){if(W.R.bChanged)L->SetBoolField(TEXT("mutation_performed"),true);else if(W.R.bMutationAttempted&&!W.R.bChangeStateKnown)L->SetField(TEXT("mutation_performed"),MakeShared<FJsonValueNull>());else L->SetBoolField(TEXT("mutation_performed"),false);}
  L->SetBoolField(TEXT("mutation_state_unknown"),!W.R.bChangeStateKnown);
  if(!W.R.bSuccess){L->SetBoolField(TEXT("do_not_retry"),W.R.bMutationAttempted||!W.R.bChangeStateKnown);if(W.R.bMutationAttempted&&!L->HasField(TEXT("partial_mutation")))L->SetBoolField(TEXT("partial_mutation"),W.R.bChanged);if(ValidationError)L->SetStringField(TEXT("error"),W.R.Error);}
  return W.Finish(TEXT("import_texture_file"),L,LegacyErrorCode);
 };
 if(!W.Plain())return Finish();
 if(AssetName.IsEmpty()){ValidationError=true;W.Fail(TEXT("invalid_texture_asset_name"),TEXT("asset_name must be a non-empty string"));return Finish();}
 // Windows FString::Len is the original Unreal UTF-16 code-unit count.
 if(AssetName.Len()>=NAME_SIZE){ValidationError=true;L->SetNumberField(TEXT("max_length"),NAME_SIZE-1);L->SetNumberField(TEXT("actual_tchar_length"),AssetName.Len());W.Fail(TEXT("texture_asset_name_too_long"),TEXT("asset_name exceeds the Unreal FName UTF-16 code-unit limit"));return Finish();}
 if(!(UnrealFolder.Equals(TEXT("/Game"),ESearchCase::CaseSensitive)||UnrealFolder.StartsWith(TEXT("/Game/"),ESearchCase::CaseSensitive))){ValidationError=true;W.Fail(TEXT("invalid_texture_unreal_folder"),TEXT("unreal_folder must be /Game or a child of /Game"));return Finish();}
 FString Folder=UnrealFolder;while(Folder.EndsWith(TEXT("/")))Folder.LeftChopInline(1);
 const FString Expected=Folder+TEXT("/")+AssetName;L->SetStringField(TEXT("expected_asset_path"),Expected);
 if(Expected.Len()>=NAME_SIZE){ValidationError=true;L->SetNumberField(TEXT("max_length"),NAME_SIZE-1);L->SetNumberField(TEXT("actual_tchar_length"),Expected.Len());W.Fail(TEXT("texture_asset_path_too_long"),TEXT("The destination asset path exceeds the Unreal FName UTF-16 code-unit limit"));return Finish();}
 if(!RPString(AssetName)||!RPString(Expected)){ValidationError=true;W.Fail(TEXT("invalid_texture_asset_name"),TEXT("Embedded NUL is not admitted by the native texture destination guard"));return Finish();}
 FString File=FPaths::ConvertRelativePathToFull(ImagePath);FPaths::NormalizeFilename(File);
 if(!IFileManager::Get().FileExists(*File)){LegacyErrorCode=false;W.Fail(TEXT("texture_image_missing"),TEXT("Image file does not exist: ")+File);return Finish();}
 const FMD5Hash InputHash=FMD5Hash::HashFile(*File);const int64 InputSize=IFileManager::Get().FileSize(*File);
 auto FileCurrent=[&](){if(!W.Current())return false;if(IFileManager::Get().FileSize(*File)!=InputSize||FMD5Hash::HashFile(*File)!=InputHash)return W.Fail(TEXT("texture_source_file_changed"),TEXT("Input image bytes changed across SDK callback"),true);return true;};
 auto Scope=MakeUnique<FRPLegacyImport>(W);if(!Scope->Activate()){Scope->Restore();return Finish();}
 bool Directory=false;if(!W.Call(TEXT("MakeDirectory"),true,[&]{Directory=UEditorAssetLibrary::MakeDirectory(Folder);L->SetBoolField(TEXT("directory_call_returned"),true);L->SetBoolField(TEXT("directory_result"),Directory);})){Scope->Restore();return Finish();}
 bool Existed=false;if(!W.Call(TEXT("DoesAssetExist"),false,[&]{Existed=UEditorAssetLibrary::DoesAssetExist(Expected);L->SetBoolField(TEXT("target_existed_before"),Existed);})){Scope->Restore();return Finish();}
 TStrongObjectPtr<UAssetImportTask> Task;if(!W.Call(TEXT("NewObject_AssetImportTask"),false,[&]{Task.Reset(NewObject<UAssetImportTask>());} )||!W.Hold(Task.Get())){Scope->Restore();return Finish();}if(!Task.IsValid()){W.Fail(TEXT("texture_import_task_missing"),TEXT("AssetImportTask allocation returned null"));Scope->Restore();return Finish();}
 Task->Filename=File;Task->DestinationPath=Folder;Task->DestinationName=AssetName;Task->bAutomated=true;Task->bSave=true;Task->bReplaceExisting=true;Task->bReplaceExistingSettings=true;Task->bAsync=false;
 auto TaskCurrent=[&](){if(!W.Current())return false;if(Task->Filename!=File||Task->DestinationPath!=Folder||Task->DestinationName!=AssetName||!Task->bAutomated||!Task->bSave||!Task->bReplaceExisting||!Task->bReplaceExistingSettings||Task->bAsync)return W.Fail(TEXT("texture_import_task_configuration_changed"),TEXT("Original synchronous automated/save/replace task configuration changed"),true);return true;};
 FAssetToolsModule* Tools=nullptr;if(!W.Call(TEXT("Load_AssetTools"),false,[&]{Tools=&FModuleManager::LoadModuleChecked<FAssetToolsModule>(TEXT("AssetTools"));})||!FileCurrent()){Scope->Restore();return Finish();}
 L->SetBoolField(TEXT("import_attempted"),true);W.R.Data->SetBoolField(TEXT("task_save_requested"),true);
 if(!W.Call(TEXT("ImportAssetTasks"),true,[&]{Tools->Get().ImportAssetTasks({Task.Get()});L->SetBoolField(TEXT("import_call_returned"),true);})){Scope->Restore();return Finish();}
 if(!FileCurrent()||!TaskCurrent()){Scope->Restore();return Finish();}
 bool Importing=false;if(!W.Call(TEXT("Interchange_IsImporting_after"),false,[&]{Importing=UInterchangeManager::IsImporting();})){Scope->Restore();return Finish();}
 W.R.Data->SetBoolField(TEXT("interchange_import_active_at_return"),Importing);
 TArray<TSharedPtr<FJsonValue>> Paths;FString Reported;
 for(const auto& P:Task->ImportedObjectPaths){Paths.Add(MakeShared<FJsonValueString>(P));if(Reported.IsEmpty()&&FPackageName::ObjectPathToPackageName(P).Equals(Expected,ESearchCase::CaseSensitive))Reported=P;}
 L->SetArrayField(TEXT("imported_paths"),Paths);L->SetStringField(TEXT("asset_path"),Reported.IsEmpty()?Expected:FPackageName::ObjectPathToPackageName(Reported));W.R.bChanged=!Paths.IsEmpty();
 if(Importing){W.Fail(TEXT("interchange_async_outcome_unknown"),TEXT("Import did not prove a completed expected asset; actual paths retained"),true);Scope->Restore();return Finish();}
 if(Reported.IsEmpty()){LegacyErrorCode=false;L->SetField(TEXT("mutation_performed"),MakeShared<FJsonValueNull>());L->SetBoolField(TEXT("partial_mutation"),!Paths.IsEmpty());W.Fail(TEXT("texture_import_expected_path_missing"),TEXT("Texture import did not report the expected asset path"),true);Scope->Restore();return Finish();}
 TStrongObjectPtr<UObject> Loaded;if(!W.Call(TEXT("LoadImportedTexture"),false,[&]{Loaded.Reset(UEditorAssetLibrary::LoadAsset(Reported));} )||!W.Hold(Loaded.Get())){Scope->Restore();return Finish();}
 if(!Loaded.IsValid()){LegacyErrorCode=false;L->SetBoolField(TEXT("mutation_performed"),true);L->SetBoolField(TEXT("partial_mutation"),true);W.Fail(TEXT("texture_import_expected_class_failed"),TEXT("Texture import reported a path that is not loadable"),true);Scope->Restore();return Finish();}
 if(Loaded->GetClass()!=UTexture2D::StaticClass())
 {
  LegacyErrorCode=false;L->SetStringField(TEXT("asset_path"),FPackageName::ObjectPathToPackageName(Loaded->GetPathName()));L->SetStringField(TEXT("object_path"),Loaded->GetPathName());L->SetStringField(TEXT("class_name"),Loaded->GetClass()->GetName());L->SetBoolField(TEXT("mutation_performed"),true);L->SetBoolField(TEXT("partial_target_modified"),true);L->SetBoolField(TEXT("partial_mutation"),true);L->SetStringField(TEXT("failed_stage"),TEXT("verify_imported_texture_class"));L->SetStringField(TEXT("failed_stage_effect"),TEXT("mutation"));L->SetBoolField(TEXT("failed_stage_mutation_performed"),true);L->SetBoolField(TEXT("workflow_recovery_required"),true);L->SetStringField(TEXT("recovery_guidance"),TEXT("Inspect or remove the unexpected imported asset manually before retrying."));W.Fail(TEXT("texture_import_expected_class_failed"),TEXT("Texture import reported the expected path but loaded a non-Texture2D asset"),true);Scope->Restore();return Finish();
 }
 if(FPackageName::ObjectPathToPackageName(Loaded->GetPathName())!=Expected){W.Fail(TEXT("texture_import_expected_class_failed"),TEXT("Reported object is not the expected exact Texture2D"),true);Scope->Restore();return Finish();}
 L->SetStringField(TEXT("object_path"),Loaded->GetPathName());L->SetStringField(TEXT("class_name"),Loaded->GetClass()->GetName());W.R.bChanged=true;
 auto SaveFailure=[&](){LegacyErrorCode=false;L->SetStringField(TEXT("save_error"),TEXT("save_loaded_asset returned false"));L->SetBoolField(TEXT("mutation_performed"),true);L->SetBoolField(TEXT("partial_target_modified"),true);L->SetBoolField(TEXT("partial_mutation"),true);L->SetStringField(TEXT("failed_stage"),TEXT("save_imported_texture"));L->SetStringField(TEXT("failed_stage_effect"),TEXT("mutation"));L->SetBoolField(TEXT("failed_stage_mutation_performed"),true);L->SetBoolField(TEXT("workflow_recovery_required"),true);L->SetStringField(TEXT("recovery_guidance"),TEXT("Resolve source-control or package write access, then save or revert the dirty texture manually."));};
 bool Saved=false;W.R.bSaveAttempted=true;
 if(!W.Call(TEXT("SaveImportedTexture"),true,[&]{Saved=UEditorAssetLibrary::SaveLoadedAsset(Loaded.Get());W.R.bSaved|=Saved;L->SetBoolField(TEXT("save_call_returned"),true);L->SetBoolField(TEXT("saved"),Saved);if(!Saved)SaveFailure();})){Scope->Restore();return Finish();}
 if(!Saved)W.Fail(TEXT("texture_import_save_failed"),TEXT("Texture imported in memory but could not be saved"),true);
 if(!Scope->Restore()||!FileCurrent())return Finish();
 if(!W.Stopped){W.R.bSuccess=true;W.R.bPostconditionPassed=true;W.R.Stage=TEXT("complete");auto Verification=MakeShared<FJsonObject>();Verification->SetStringField(TEXT("status"),TEXT("passed"));Verification->SetStringField(TEXT("method"),TEXT("asset_import_task_path_and_save_readback"));L->SetObjectField(TEXT("verification"),Verification);}return Finish();
}

FString UCubelessTextureMaterialAttachmentLibrary::ApplyTextureMaterial(const FString& Target,const FString& MaterialAssetPath,int32 MaterialSlot)
{
 FRPWork W(false);auto L=MakeShared<FJsonObject>();L->SetStringField(TEXT("material_asset_path"),MaterialAssetPath);TArray<TSharedPtr<FJsonValue>> Applied;
 auto AppliedPrefix=[&](){L->SetArrayField(TEXT("applied"),Applied);L->SetNumberField(TEXT("count"),Applied.Num());};AppliedPrefix();
 if(!W.Plain())return W.Finish(TEXT("apply_texture_material"),L);
 TStrongObjectPtr<UObject> Material;if(!W.Call(TEXT("LoadMaterial"),false,[&]{Material.Reset(UEditorAssetLibrary::LoadAsset(MaterialAssetPath));})||!W.Hold(Material.Get()))return W.Finish(TEXT("apply_texture_material"),L);
 if(!Material.IsValid()){W.Fail(TEXT("material_asset_not_found"),TEXT("Material asset not found: ")+MaterialAssetPath);return W.Finish(TEXT("apply_texture_material"),L);}
 if(!Material->IsA<UMaterialInterface>()){W.Fail(TEXT("material_asset_not_found"),TEXT("Material asset not found or not a MaterialInterface"));return W.Finish(TEXT("apply_texture_material"),L);}
 TStrongObjectPtr<UEditorActorSubsystem> Actors(GEditor->GetEditorSubsystem<UEditorActorSubsystem>());if(!W.Hold(Actors.Get()))return W.Finish(TEXT("apply_texture_material"),L);if(!Actors.IsValid()){W.Fail(TEXT("material_actor_subsystem_unavailable"),TEXT("Editor Actor subsystem unavailable"));return W.Finish(TEXT("apply_texture_material"),L);}
 TStrongObjectPtr<UObject> Asset;TArray<AActor*> Candidates,Selected;TArray<TStrongObjectPtr<AActor>> CandidateHolds;
 auto HoldCandidates=[&](){for(auto* A:Candidates)CandidateHolds.Emplace(A);};
 if(Target==TEXT("selected"))
 {
  if(!W.Call(TEXT("GetSelectedLevelActors"),false,[&]{Candidates=Actors->GetSelectedLevelActors();HoldCandidates();}))return W.Finish(TEXT("apply_texture_material"),L);
  if(Candidates.IsEmpty()){W.Fail(TEXT("material_target_not_found"),TEXT("No selected actors. Select a Static Mesh actor or pass an actor name/path."));return W.Finish(TEXT("apply_texture_material"),L);}
 }
 else
 {
  if(!W.Call(TEXT("LoadTargetAsset"),false,[&]{Asset.Reset(UEditorAssetLibrary::LoadAsset(Target));}))return W.Finish(TEXT("apply_texture_material"),L);
  if(Asset.IsValid()&&!W.Hold(Asset.Get()))return W.Finish(TEXT("apply_texture_material"),L);
  if(!Asset.IsValid()||Asset->GetClass()!=UStaticMesh::StaticClass())if(!W.Call(TEXT("GetAllLevelActors"),false,[&]{Candidates=Actors->GetAllLevelActors();HoldCandidates();}))return W.Finish(TEXT("apply_texture_material"),L);
 }
 // Only selected/matching roles survive writers. Unselected world objects are not
 // required to remain physically alive as an accidental global invariant.
 for(AActor* A:Candidates)
 {
  TStrongObjectPtr<AActor> Temporary(A);if(!W.Current()||!RPLive(A)){W.Fail(TEXT("material_actor_observation_unavailable"),TEXT("Actor observation unavailable"),true);return W.Finish(TEXT("apply_texture_material"),L);}
  const int32 PreviousHeld=W.Held.Num();if(!W.Hold(A))return W.Finish(TEXT("apply_texture_material"),L);
  FString Label;if(Target!=TEXT("selected")&&!RPActorLabel(W,A,Label))return W.Finish(TEXT("apply_texture_material"),L);
  const bool Match=Target==TEXT("selected")||Label.Contains(Target,ESearchCase::CaseSensitive)||A->GetPathName().Contains(Target,ESearchCase::CaseSensitive)||A->GetName()==Target;
  if(Match){if(A->GetWorld()!=W.World.Get()){W.Fail(TEXT("material_actor_world_changed"),TEXT("Matched actor is outside the captured Editor world"),true);return W.Finish(TEXT("apply_texture_material"),L);}Selected.Add(A);}
  else W.Held.SetNum(PreviousHeld);
 }
 CandidateHolds.Reset();if(!W.Current())return W.Finish(TEXT("apply_texture_material"),L);
 if(Target!=TEXT("selected")&&(!Asset.IsValid()||Asset->GetClass()!=UStaticMesh::StaticClass())&&Selected.IsEmpty())
 {W.Fail(TEXT("material_target_not_found"),TEXT("No actor or StaticMesh asset matched target: ")+Target);return W.Finish(TEXT("apply_texture_material"),L);}
 UMaterialInterface* M=static_cast<UMaterialInterface*>(Material.Get());
 if(Asset.IsValid()&&Asset->GetClass()==UStaticMesh::StaticClass())
 {
  auto* Mesh=static_cast<UStaticMesh*>(Asset.Get());const bool ValidSlot=Mesh->GetStaticMaterials().IsValidIndex(MaterialSlot);UMaterialInterface* Before=nullptr;FName BeforeSlot,BeforeImported;const bool BeforeDirty=Mesh->GetOutermost()->IsDirty();
  if(ValidSlot){const auto& Slot=Mesh->GetStaticMaterials()[MaterialSlot];if(!Slot.MaterialInterface.IsResolved()){W.Fail(TEXT("material_slot_unresolved"),TEXT("Unresolved existing material handle requires separate preparation; no writer"));return W.Finish(TEXT("apply_texture_material"),L);}Before=Slot.MaterialInterface.Get();BeforeSlot=Slot.MaterialSlotName;BeforeImported=Slot.ImportedMaterialSlotName;if(!W.Hold(Before))return W.Finish(TEXT("apply_texture_material"),L);}
  if(!W.Call(TEXT("StaticMesh_SetMaterial"),true,[&]{Mesh->SetMaterial(MaterialSlot,M);}))return W.Finish(TEXT("apply_texture_material"),L);
  if(ValidSlot&&(!Mesh->GetStaticMaterials().IsValidIndex(MaterialSlot)||!Mesh->GetStaticMaterials()[MaterialSlot].MaterialInterface.IsResolved()||Mesh->GetStaticMaterials()[MaterialSlot].MaterialInterface.Get()!=M)){W.Fail(TEXT("static_mesh_material_readback_failed"),TEXT("Requested valid material slot differs"),true);return W.Finish(TEXT("apply_texture_material"),L);}
  W.R.bChanged|=(ValidSlot&&(Before!=M||BeforeSlot!=Mesh->GetStaticMaterials()[MaterialSlot].MaterialSlotName||BeforeImported!=Mesh->GetStaticMaterials()[MaterialSlot].ImportedMaterialSlotName))||(!BeforeDirty&&Mesh->GetOutermost()->IsDirty());auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("target"),Mesh->GetPathName());Row->SetStringField(TEXT("type"),TEXT("StaticMesh"));Row->SetNumberField(TEXT("slot"),MaterialSlot);Applied.Add(MakeShared<FJsonValueObject>(Row));AppliedPrefix();bool Saved=false;W.R.bSaveRequested=true;W.R.bSaveAttempted=true;
  if(!W.Call(TEXT("SaveStaticMesh"),true,[&]{Saved=UEditorAssetLibrary::SaveLoadedAsset(Mesh);W.R.bSaved|=Saved;L->SetBoolField(TEXT("save_returned"),true);L->SetBoolField(TEXT("save_result"),Saved);} ))return W.Finish(TEXT("apply_texture_material"),L);
  // The original ignores this save bool for apply success. Retain the actual bool.
 }
 for(AActor* A:Selected)
 {
  TArray<UStaticMeshComponent*> Components;TArray<TStrongObjectPtr<UStaticMeshComponent>> ComponentHolds;if(!W.Call(TEXT("GetStaticMeshComponents"),false,[&]{A->GetComponents<UStaticMeshComponent>(Components);for(auto* C:Components)ComponentHolds.Emplace(C);}))return W.Finish(TEXT("apply_texture_material"),L);
  for(auto* C:Components)
  {
   if(!W.Hold(C)||C->GetOwner()!=A){W.Fail(TEXT("material_component_owner_changed"),TEXT("Component owner differs"),true);return W.Finish(TEXT("apply_texture_material"),L);}
   // The SDK computes ElementIndex+1 in int32. Reject only that overflow,
   // without imposing a new arbitrary material-slot count or allocating a plan array.
   if(MaterialSlot==MAX_int32){W.Fail(TEXT("material_component_slot_overflow"),TEXT("SDK int32 override-array growth would overflow"));return W.Finish(TEXT("apply_texture_material"),L);}
   TArray<UMaterialInterface*> Before;for(const auto& Slot:C->OverrideMaterials){if(!Slot.IsResolved()){W.Fail(TEXT("material_override_unresolved"),TEXT("Unresolved existing override handle requires separate preparation; no writer"));return W.Finish(TEXT("apply_texture_material"),L);}Before.Add(Slot.Get());}
   for(auto* Previous:Before)if(!W.Hold(Previous))return W.Finish(TEXT("apply_texture_material"),L);
   const int32 ExpectedNum=MaterialSlot<0?Before.Num():FMath::Max(Before.Num(),MaterialSlot+1);
   if(!W.Call(TEXT("StaticMeshComponent_SetMaterial"),true,[&]{C->SetMaterial(MaterialSlot,M);}))return W.Finish(TEXT("apply_texture_material"),L);
   if(C->GetOwner()!=A||C->OverrideMaterials.Num()!=ExpectedNum){W.Fail(TEXT("material_component_slots_changed"),TEXT("Component owner/override array differs from SDK plan"),true);return W.Finish(TEXT("apply_texture_material"),L);}
   bool SlotChanged=Before.Num()!=ExpectedNum;for(int32 I=0;I<ExpectedNum;++I){UMaterialInterface* Expected=I==MaterialSlot?M:(Before.IsValidIndex(I)?Before[I]:nullptr);if(!C->OverrideMaterials[I].IsResolved()||C->OverrideMaterials[I].Get()!=Expected){W.Fail(TEXT("material_component_readback_failed"),TEXT("Component override slot differs"),true);return W.Finish(TEXT("apply_texture_material"),L);}SlotChanged|=Before.IsValidIndex(I)&&Before[I]!=Expected;}
   W.R.bChanged|=SlotChanged;auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("actor_path"),A->GetPathName());Row->SetStringField(TEXT("component"),C->GetName());Row->SetNumberField(TEXT("slot"),MaterialSlot);Applied.Add(MakeShared<FJsonValueObject>(Row));AppliedPrefix();
   FString Label;if(!RPActorLabel(W,A,Label))return W.Finish(TEXT("apply_texture_material"),L);Row->SetStringField(TEXT("target"),Label);
  }
 }
 L->SetArrayField(TEXT("applied"),Applied);L->SetNumberField(TEXT("count"),Applied.Num());
 if(Applied.IsEmpty())W.Fail(TEXT("material_target_not_found"),TEXT("Target did not contain a StaticMeshComponent and was not a StaticMesh asset."));
 else if(W.Current()){W.R.bSuccess=true;W.R.bPostconditionPassed=true;W.R.Stage=TEXT("complete");}return W.Finish(TEXT("apply_texture_material"),L);
}
