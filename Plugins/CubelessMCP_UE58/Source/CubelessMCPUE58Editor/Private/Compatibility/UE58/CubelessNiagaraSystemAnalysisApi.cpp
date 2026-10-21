// HB: count-only Editor metadata and original SDK argument normalization.
// The six-section analysis, summary and partial-failure contract remain Python composition.
#include "Compatibility/UE58/CubelessNiagaraSystemAnalysisApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "NiagaraSystem.h"
#include "Misc/PackageName.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
namespace Cubeless::UE58::NiagaraSystemAnalysis
{
namespace
{
constexpr const TCHAR* Operation=TEXT("inspect_niagara_system_analysis_metadata");
constexpr int32 MaxCarrier=1024*1024,MaxSlots=65536,MaxDepth=32;
bool Running=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool TextSafe(const FString& S,int32 Limit){if(S.Len()>Limit)return false;for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
bool GlobalsSafe(){return IsEditorGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!GIsSavingPackage&&!IsGarbageCollecting()&&Live(GEditor)&&GEngine==GEditor;}
struct FContext
{
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;TWeakObjectPtr<UClass> WorldClass;TWeakObjectPtr<UObject> WorldOuter;TWeakObjectPtr<UPackage> WorldPackage,WorldOutermost;FString WorldPath;bool HadWorld=false;
 TStrongObjectPtr<UObject> EditorHold,WorldHold;
 bool Capture(){if(!GlobalsSafe())return false;Editor=GEditor;EditorHold.Reset(GEditor);UWorld* W=GEditor->GetEditorWorldContext().World();HadWorld=W!=nullptr;if(W){if(!Live(W)||W->bIsTearingDown)return false;World=W;WorldHold.Reset(W);WorldClass=W->GetClass();WorldOuter=W->GetOuter();WorldPackage=W->GetPackage();WorldOutermost=W->GetOutermost();WorldPath=W->GetPathName();}return Same();}
 bool Same()const{if(!GlobalsSafe()||Editor.Get()!=GEditor)return false;UWorld* Current=GEditor->GetEditorWorldContext().World();if(!HadWorld)return Current==nullptr;UWorld* W=World.Get();return Live(W)&&!W->bIsTearingDown&&Current==W&&W->GetClass()==WorldClass.Get()&&W->GetOuter()==WorldOuter.Get()&&W->GetPackage()==WorldPackage.Get()&&W->GetOutermost()==WorldOutermost.Get()&&W->GetPathName()==WorldPath;}
};
struct FSystemIdentity
{
 TWeakObjectPtr<UNiagaraSystem> System;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UObject> Outer;TWeakObjectPtr<UPackage> Package,Outermost;FString Path;TStrongObjectPtr<UNiagaraSystem> Hold;
 void Capture(UNiagaraSystem* S){System=S;Hold.Reset(S);Class=S->GetClass();Outer=S->GetOuter();Package=S->GetPackage();Outermost=S->GetOutermost();Path=S->GetPathName();}
 bool Same()const{auto* S=System.Get();return Live(S)&&Live(Class.Get())&&Live(Outer.Get())&&Live(Package.Get())&&Live(Outermost.Get())&&S->GetClass()==Class.Get()&&S->GetOuter()==Outer.Get()&&S->GetPackage()==Package.Get()&&S->GetOutermost()==Outermost.Get()&&S->GetPathName()==Path;}
};
struct FWork
{
 Variable::FResult R;TSharedRef<FJsonObject> Metadata=MakeShared<FJsonObject>(),Prefix=MakeShared<FJsonObject>();TSharedPtr<FJsonObject> Params;FContext Context;FSystemIdentity Identity;bool ContextCaptured=false,IdentityCaptured=false,Lost=false;
 FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("legacy_result"),Metadata);R.Data->SetObjectField(TEXT("actual_partial_state"),Prefix);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("aggregate_implemented_in_this_leaf"),false);R.Data->SetBoolField(TEXT("game_runtime_dependency_added"),false);Metadata->SetBoolField(TEXT("success"),false);}
 bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;Metadata->SetBoolField(TEXT("success"),false);Metadata->SetStringField(TEXT("error"),R.Error);return false;}
 bool Admit(){if(Lost)return false;if(!ContextCaptured||!Context.Same()){Lost=true;return Fail(TEXT("niagara_analysis_metadata_context_lost"),TEXT("Captured Editor context changed or GC/exit/save/teardown became unsafe; no further object reads"));}if(IdentityCaptured&&!Identity.Same()){Lost=true;return Fail(TEXT("niagara_analysis_metadata_system_changed"),TEXT("Captured loaded System class/outer/package/path identity changed; no reload or retarget"));}return true;}
 bool Parse(const FString& Text)
 {
  if(!TextSafe(Text,MaxCarrier)||Text.IsEmpty())return Fail(TEXT("niagara_analysis_metadata_carrier_invalid"),TEXT("One bounded JSON object without embedded NUL required"));
  auto Reader=TJsonReaderFactory<>::Create(Text);EJsonNotation Note;int32 Depth=0,Slots=0;bool Valid=true;
  while(Reader->ReadNext(Note)){if(++Slots>MaxSlots){Valid=false;break;}if(Note==EJsonNotation::ObjectStart||Note==EJsonNotation::ArrayStart){if(++Depth>MaxDepth){Valid=false;break;}}else if(Note==EJsonNotation::ObjectEnd||Note==EJsonNotation::ArrayEnd){if(--Depth<0){Valid=false;break;}}else if(Note==EJsonNotation::Number){if(!FMath::IsFinite(Reader->GetValueAsNumber())){Valid=false;break;}}else if(Note==EJsonNotation::String){if(!TextSafe(Reader->GetValueAsString(),MaxCarrier)){Valid=false;break;}}else if(Note==EJsonNotation::Error){Valid=false;break;}}
  if(!Valid||Depth!=0||!Reader->GetErrorMessage().IsEmpty()){R.Data->SetStringField(TEXT("json_reader_error"),Reader->GetErrorMessage());return Fail(TEXT("niagara_analysis_metadata_json_bound"),TEXT("Finite JSON depth/slot/text bound or reader validation failed"));}
  auto ObjectReader=TJsonReaderFactory<>::Create(Text);if(!FJsonSerializer::Deserialize(ObjectReader,Params)||!Params.IsValid()){R.Data->SetStringField(TEXT("json_reader_error"),ObjectReader->GetErrorMessage());return Fail(TEXT("niagara_analysis_metadata_params_invalid"),TEXT("A JSON object required"));}return true;
 }
 bool Run(const FString& Text)
 {
  if(!Parse(Text))return false;FString Requested;if(!Params->TryGetStringField(TEXT("system_path"),Requested))return Fail(TEXT("niagara_analysis_metadata_system_path_missing"),TEXT("Missing 'system_path' parameter"));if(!TextSafe(Requested,32768))return Fail(TEXT("niagara_analysis_metadata_system_path_bound"),TEXT("Bounded System path without embedded NUL required"));
  FString Path=FPackageName::ExportTextPathToObjectPath(Requested).TrimStartAndEnd();Path.TrimQuotesInline();if(Path.StartsWith(TEXT("/"))&&!Path.StartsWith(TEXT("/Script/"))&&!Path.Contains(TEXT(".")))Path+=TEXT(".")+FPackageName::GetShortName(Path);
  R.Stage=TEXT("capture_context");if(!Context.Capture())return Fail(TEXT("niagara_analysis_metadata_editor_unsafe"),TEXT("Live Editor game thread outside GC/exit/save/world teardown required"));ContextCaptured=true;if(!Admit())return false;
  Prefix->SetStringField(TEXT("requested_system_path"),Requested);Prefix->SetStringField(TEXT("normalized_load_path"),Path);R.Stage=TEXT("load_system");R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);Prefix->SetBoolField(TEXT("LoadObject_attempted"),true);
  UNiagaraSystem* S=LoadObject<UNiagaraSystem>(nullptr,*Path);Prefix->SetBoolField(TEXT("LoadObject_returned"),true);Prefix->SetBoolField(TEXT("LoadObject_returned_nonnull"),S!=nullptr); // Plain return prefix is recorded before post-SDK admission.
  if(!Admit())return false;if(!Live(S)||!S->IsA<UNiagaraSystem>())return Fail(TEXT("niagara_analysis_metadata_system_missing"),FString(TEXT("Failed to load Niagara system: "))+Requested);
  Identity.Capture(S);IdentityCaptured=true;if(!Admit())return false;Prefix->SetStringField(TEXT("loaded_system_path"),Identity.Path);Prefix->SetBoolField(TEXT("live_identity_verified"),true);
  // Keep the original load-before-option-normalization order and real FJsonObject SDK coercion/default behavior.
  R.Stage=TEXT("normalize_options");auto N=MakeShared<FJsonObject>();N->SetStringField(TEXT("system_path"),Identity.Path);
  struct FBoolOption{const TCHAR* Key;bool Default;};const FBoolOption Booleans[]={{TEXT("include_renderers"),true},{TEXT("include_user_parameters"),true},{TEXT("include_stack"),true},{TEXT("include_graph"),true},{TEXT("include_module_inputs"),true},{TEXT("include_compile_status"),true},{TEXT("include_pins"),false},{TEXT("include_links"),false},{TEXT("include_scratch_pads"),true},{TEXT("include_resolved_stack_inputs"),false}};
  for(const FBoolOption& B:Booleans){bool Value=B.Default;Params->TryGetBoolField(B.Key,Value);N->SetBoolField(B.Key,Value);}
  struct FIntOption{const TCHAR* Key;int32 Default;};const FIntOption Integers[]={{TEXT("max_function_calls"),200},{TEXT("max_nodes_per_graph"),300},{TEXT("max_links_per_graph"),0},{TEXT("max_modules"),200},{TEXT("max_candidates_per_module"),24},{TEXT("max_resolved_inputs_per_module"),8},{TEXT("max_top_candidates"),80}};
  for(const FIntOption& I:Integers){int32 Value=I.Default;Params->TryGetNumberField(I.Key,Value);N->SetNumberField(I.Key,Value);}R.Data->SetObjectField(TEXT("normalized_parameters"),N);
  R.Stage=TEXT("read_metadata");if(!Admit())return false;const int32 EmitterCount=static_cast<const UNiagaraSystem*>(S)->GetNumEmitters();Prefix->SetNumberField(TEXT("physical_emitter_handle_count"),EmitterCount);if(!Admit())return false;
  Metadata->SetBoolField(TEXT("success"),true);Metadata->SetStringField(TEXT("system_path"),Identity.Path);Metadata->SetNumberField(TEXT("emitter_count"),EmitterCount);Metadata->SetBoolField(TEXT("read_only"),true);Metadata->SetStringField(TEXT("source_policy"),TEXT("read_only_analysis_no_save_no_compile_request"));R.Data->SetBoolField(TEXT("metadata_projection_complete"),true);R.Data->SetBoolField(TEXT("renderer_virtual_getters_called"),false);R.Data->SetStringField(TEXT("known_scope"),TEXT("Current captured System path/physical emitter count and SDK-normalized options only. Multi-call aggregate atomicity, loader/cache/global purity and shader/GPU completion are unproven."));return Admit();
 }
 FString Finish(){if(ContextCaptured&&!Lost)Admit();R.bSuccess=R.Error.IsEmpty();R.bPostconditionPassed=R.bSuccess;if(R.bSuccess){R.Stage=TEXT("complete");R.bChangeStateKnown=true;}Metadata->SetBoolField(TEXT("success"),R.bSuccess);R.Data->SetBoolField(TEXT("change_state_known"),R.bChangeStateKnown);R.Data->SetBoolField(TEXT("mutation_state_unknown"),!R.bChangeStateKnown);return Operations::EncodeVariableResult(R,Operation);}
};
}
FString InspectMetadata(const FString& ParamsJson)
{
 FWork W;if(!IsInGameThread()){W.Fail(TEXT("niagara_analysis_metadata_GT_required"),TEXT("Metadata leaf must execute on the Editor game thread"));return W.Finish();}if(Running){W.Fail(TEXT("niagara_analysis_metadata_reentrant_call"),TEXT("Nested metadata load refused; no replay"));return W.Finish();}TGuardValue<bool> Scope(Running,true);W.Run(ParamsJson);return W.Finish();
}
}
