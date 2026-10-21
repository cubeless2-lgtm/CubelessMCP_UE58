// CV: public cached-data refresh2; private stale clear and blocking wait remain explicit gaps.
#include "Compatibility/UE58/CubelessMaterialCacheApi.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
#include "CubelessMaterialPostprocessJob.h"
#include "Operations/CubelessVariableOperations.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Materials/MaterialFunction.h"
#include "Misc/CoreDelegates.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"

#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessMaterialApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "MaterialCachedData.h"
#include "MaterialShaderPrecompileMode.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialFunctionInterface.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "Modules/ModuleManager.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::MaterialCache
{
namespace
{
using FResult = Variable::FResult;
void Count(FResult& R, const TCHAR* Key) { R.Data->SetNumberField(Key,R.Data->GetNumberField(Key)+1); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error) { R.bSuccess=false;R.ErrorCode=Code;R.Error=Error;return false; }
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed); }
bool HasNul(const FString& S) { for(int32 I=0;I<S.Len();++I) { if(S[I]==TCHAR('\0')) { return true; } }return false; }

bool AsyncRunning=false,AsyncLease=false;
TFunction<bool()>* AsyncReadAdmission=nullptr;
bool ContinueAsyncRead(){return !AsyncReadAdmission||(*AsyncReadAdmission)();}
bool AsyncSafe(){return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage();}
bool AsyncLive(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool AsyncReadable(const UObject* O)
{
 TSet<const UObject*> Seen;for(int32 I=0;O&&I<64;++I){if(!AsyncLive(O)||Seen.Contains(O))return false;Seen.Add(O);O=O->GetOuter();}return O==nullptr;
}
struct FAsyncIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;
 TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,EffectivePackage;FString Path;
 explicit FAsyncIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),EffectivePackage(O->GetPackage()),Path(O->GetPathName()){}
 bool Exact()const
 {
  auto* O=Object.Get();return AsyncReadable(O)&&AsyncLive(Class.Get())&&AsyncReadable(Package.Get())&&AsyncReadable(EffectivePackage.Get())&&
   O->GetClass()==Class.Get()&&O->GetOuter()==Outer.Get()&&O->GetOutermost()==Package.Get()&&O->GetPackage()==EffectivePackage.Get()&&O->GetPathName().Equals(Path,ESearchCase::CaseSensitive);
 }
};

struct FContext
{
    Material::FState Selected;
    TStrongObjectPtr<UMaterial> Asset{nullptr};
    UMaterial* MaterialAsset=nullptr;
    TSharedRef<FJsonObject> Params=MakeShared<FJsonObject>();
    TArray<TStrongObjectPtr<UMaterialExpression>> Expressions;
    TArray<TStrongObjectPtr<UMaterialFunctionInterface>> Functions;
    TSharedPtr<FJsonObject> Before;
    int32 FunctionCallCount=0;
};

struct FDeferredWork
{
 FResult Result;FContext Context;
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
 TArray<TUniquePtr<FAsyncIdentity>> Objects;
 TArray<UMaterialExpression*> Expressions;TMap<UMaterialExpression*,FGuid> ExpressionIds;
 TMap<UMaterialExpressionMaterialFunctionCall*,UMaterialFunctionInterface*> CallFunctions;
 TMap<UMaterialFunctionInterface*,FGuid> FunctionIds;
 TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>(),After=MakeShared<FJsonObject>();
 TSharedRef<FJsonObject> Metrics=MakeShared<FJsonObject>();
 UObject* Root=nullptr;bool Captured=false,Ready=false,CancelRequested=false,GuardFailed=false,Save=true,Unconnected=false;
 FString CancelReason;
 EShaderPlatform ShaderPlatform=GMaxRHIShaderPlatform;
 TOptional<int32> Quality;
 bool PublicationSafe()const
 {
  if(!AsyncSafe()||Editor.Get()!=GEditor)return false;
  UWorld* W=GEditor->GetEditorWorldContext().World();
  return AsyncLive(W)&&!W->bIsTearingDown&&World.Get()==W&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();
 }
 bool Error(const TCHAR* Code,const FString& Message)
 {
  Result.bChangeStateKnown=false;Result.bSuccess=false;Result.bPostconditionPassed=false;return Fail(Result,Code,Message);
 }
 bool Own(UObject* O)
 {
  if(!O)return true;if(!AsyncReadable(O))return false;
  for(const auto& I:Objects)if(I->Object.Get()==O)return true;
  if(Objects.Num()>=8192)return Error(TEXT("material_async_identity_budget"),TEXT("Strong authoring/cache identity budget exceeded; no further SDK."));
  Objects.Add(MakeUnique<FAsyncIdentity>(O));return true;
 }
 bool CaptureRoot()
 {
  UObject* Candidate=Context.Asset.Get();
  if(!Candidate)return true;
  if(!Own(Candidate))return Error(TEXT("material_async_target_unreadable"),TEXT("Current selected target is not live/owned before authoring role capture."));
  Root=Candidate;
  TArray<UMaterialExpression*> CurrentExpressions;
  if(auto* M=Cast<UMaterial>(Root))for(const auto& P:M->GetExpressions())CurrentExpressions.Add(P.Get());
  else if(auto* F=Cast<UMaterialFunction>(Root))for(const auto& P:F->GetExpressions())CurrentExpressions.Add(P.Get());
  else return Error(TEXT("material_async_target_type_unsupported"),TEXT("Exact native Material/Function authoring target required."));
  if(CurrentExpressions.Num()>512)return Error(TEXT("material_async_expression_budget"),TEXT("Current expression roles exceed existing512 bound."));
  for(UMaterialExpression* X:CurrentExpressions)
  {
   if(!X||!Own(X)||X->GetOuter()!=Root)return Error(TEXT("material_async_expression_owner_changed"),TEXT("Every captured expression must be live and directly owned by the selected target."));
   Expressions.Add(X);ExpressionIds.Add(X,X->MaterialExpressionGuid);
   if(auto* Call=Cast<UMaterialExpressionMaterialFunctionCall>(X))
   {
    UMaterialFunctionInterface* F=Call->MaterialFunction;
    if(!Own(F))return Error(TEXT("material_async_function_source_unreadable"),TEXT("Selected expression Function source is unreadable."));
    CallFunctions.Add(Call,F);if(F)FunctionIds.Add(F,F->StateId);
   }
  }
  Captured=true;return true;
 }
 bool Admit()
 {
  if(!PublicationSafe()||CancelRequested||MaterialPostprocessAsync::IsStopping())
  {GuardFailed=true;return Error(TEXT("material_async_context_stopped"),TEXT("Exact Editor/world/GC/exit/owned cancellation admission failed; actual prefix retained."));}
  if(GuardFailed)return false;
  if(ShaderPlatform!=GMaxRHIShaderPlatform){GuardFailed=true;return Error(TEXT("material_async_shader_platform_changed"),TEXT("Current shader platform changed during owned continuation."));}
  if(!Captured&&!CaptureRoot())return false;
  if(!Captured)return true;
  for(const auto& I:Objects)if(!I->Exact()){GuardFailed=true;return Error(TEXT("material_async_identity_changed"),TEXT("Captured class/outer/effective package/path changed; no new compile/save."));}
  if(Context.Asset.Get()!=Root){GuardFailed=true;return Error(TEXT("material_async_target_changed"),TEXT("Selected target changed after a callback."));}
  TArray<UMaterialExpression*> CurrentExpressions;
  if(auto* M=Cast<UMaterial>(Root))for(const auto& P:M->GetExpressions())CurrentExpressions.Add(P.Get());
  else if(auto* F=Cast<UMaterialFunction>(Root))for(const auto& P:F->GetExpressions())CurrentExpressions.Add(P.Get());
  if(CurrentExpressions!=Expressions){GuardFailed=true;return Error(TEXT("material_async_expression_membership_changed"),TEXT("Current owned expression physical order/membership changed."));}
  for(auto* X:Expressions)if(X->MaterialExpressionGuid!=ExpressionIds.FindChecked(X))
  {GuardFailed=true;return Error(TEXT("material_async_expression_guid_changed"),TEXT("Captured authoring expression GUID changed."));}
  for(const auto& P:CallFunctions)if(P.Key->MaterialFunction!=P.Value)
  {GuardFailed=true;return Error(TEXT("material_async_function_route_changed"),TEXT("Captured expression Function source pointer changed."));}
  for(const auto& P:FunctionIds)if(P.Key->StateId!=P.Value)
  {GuardFailed=true;return Error(TEXT("material_async_function_source_changed"),TEXT("Captured external Function source StateId changed."));}
  return true;
 }
 void MergeMetrics(const TSharedRef<FJsonObject>& Point)
 {
  for(const auto& P:Point->Values)
  {
   if(P.Value->Type==EJson::Number){double V=0;Metrics->TryGetNumberField(FString(P.Key.ToView()),V);Metrics->SetNumberField(FString(P.Key.ToView()),V+P.Value->AsNumber());}
   else Metrics->SetField(FString(P.Key.ToView()),P.Value);
  }
  Result.Data->SetObjectField(TEXT("shared_material_read_metrics"),Metrics);
 }
};

FDeferredWork* CurrentDeferredWork=nullptr;
void Init(FResult& R,bool Save)
{
    R.bSaveRequested=Save;R.bCompileRequested=true;
    for(const TCHAR* K:{TEXT("outer_transaction_calls"),TEXT("material_capture_calls"),TEXT("material_capture_returns"),TEXT("cache_observation_calls"),
        TEXT("expression_scan_visits"),TEXT("explicit_asset_modify_calls"),TEXT("update_cached_expression_data_calls"),TEXT("update_cached_expression_data_returns"),
        TEXT("explicit_post_edit_change_calls"),TEXT("explicit_pre_edit_change_calls"),TEXT("material_update_context_calls"),TEXT("force_recompile_background_calls"),
        TEXT("mark_package_dirty_calls"),TEXT("save_prepare_custom_input_visits"),TEXT("save_attempt_count"),TEXT("save_return_count"),
        TEXT("registry_module_query_calls"),TEXT("registry_scan_calls"),TEXT("registry_scan_returns"),TEXT("registry_dependency_query_calls"),TEXT("registry_dependency_query_returns"),
        TEXT("material_resource_queries"),TEXT("material_compile_status_queries"),TEXT("finish_compilation_calls"),TEXT("material_editing_library_recompile_calls"),TEXT("private_function_info_clear_calls"),
        TEXT("explicit_GC_calls"),TEXT("shader_manager_pump_calls")}) { R.Data->SetNumberField(K,0); }
    R.Data->SetArrayField(TEXT("save_calls"),{});R.Data->SetArrayField(TEXT("dependencies"),{});R.Data->SetArrayField(TEXT("source_or_temp_dependencies"),{});
    R.Data->SetBoolField(TEXT("dependencies_complete"),false);R.Data->SetBoolField(TEXT("cached_expression_data_refreshed"),false);
    R.Data->SetBoolField(TEXT("cleared_stale_function_infos"),false);R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),false);R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);
    R.Data->SetBoolField(TEXT("legacy_private_clear_contract_complete"),false);R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false);R.Data->SetBoolField(TEXT("whole_RecompileMaterial_helper_equivalent"),false);
    R.Data->SetBoolField(TEXT("automatic_retry_performed"),false);R.Data->SetBoolField(TEXT("runtime_cold_or_render_verified"),false);
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Selected public graph/expression identity/order, function-info pointer-path/StateId/CRC/layers, dirty/disk from after initial shared resolution. Shader/registry/editor/child/streaming effects and initial loads are not globally measured."));
    R.Data->SetStringField(TEXT("cache_refresh_scope"),TEXT("UpdateCachedExpressionData actual returned public call and bounded current cache fields. Protected serialized-cache flag may make it return early; no direct cache mutation/invalidation or stale FunctionInfos const_cast."));
    R.Data->SetStringField(TEXT("compile_scope"),TEXT("Finite public PostEdit/update-context/Force Background request; immediate original Editor finished/errors/map-presence criterion for requested-save gate, strict full resource diagnostics separate. Original RecompileMaterial GC/modal/streaming/fanout and blocking Finish parity incomplete."));
    R.Data->SetStringField(TEXT("dependency_scope"),TEXT("AssetRegistry ScanModifiedAssetFiles/GetDependencies reads current on-disk package. Unsaved RAM rebuild does not imply changed disk dependencies; registry cache callbacks unmeasured."));
}
bool Disk(const UObject* O,const TSharedRef<FJsonObject>& Out)
{
    FString Path;if(!Loaded(O)||!FPackageName::TryConvertLongPackageNameToFilename(O->GetOutermost()->GetName(),Path,FPackageName::GetAssetPackageExtension())) { return false; }
    Path=FPaths::ConvertRelativePathToFull(Path);const int64 N=IFileManager::Get().FileSize(*Path);
    Out->SetStringField(TEXT("path"),Path);Out->SetBoolField(TEXT("exists"),N>=0);Out->SetNumberField(TEXT("bytes"),FMath::Max<int64>(0,N));Out->SetStringField(TEXT("sha1"),TEXT(""));
    if(N<0) { return true; }if(N>67108864) { return false; }TArray<uint8> Bytes;if(!FFileHelper::LoadFileToArray(Bytes,*Path)||Bytes.Num()!=N) { return false; }
    FSHAHash Hash;FSHA1::HashBuffer(Bytes.GetData(),Bytes.Num(),Hash.Hash);Out->SetStringField(TEXT("sha1"),Hash.ToString());return true;
}
bool Equal(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A.IsValid()||!B.IsValid()||A->Type!=B->Type) { return false; }
    switch(A->Type)
    {
    case EJson::Null:return true;case EJson::Boolean:return A->AsBool()==B->AsBool();case EJson::Number:return A->AsNumber()==B->AsNumber();
    case EJson::String:return A->AsString().Equals(B->AsString(),ESearchCase::CaseSensitive);
    case EJson::Array:{const auto& X=A->AsArray();const auto& Y=B->AsArray();if(X.Num()!=Y.Num()) { return false; }for(int32 I=0;I<X.Num();++I) { if(!Equal(X[I],Y[I])) { return false; } }return true;}
    case EJson::Object:{const auto X=A->AsObject(),Y=B->AsObject();if(!X.IsValid()||!Y.IsValid()||X->Values.Num()!=Y->Values.Num()) { return false; }for(const auto& P:X->Values) { const auto* V=Y->Values.Find(P.Key);if(!V||!Equal(P.Value,*V)) { return false; } }return true;}
    default:return false;
    }
}
bool ObserveCompile(FContext& C, FResult& R, const TCHAR* Key)
{
    R.Compiled.Reset();
    for(const TCHAR* Field:{TEXT("compiled"),TEXT("full_resource_compiled"),TEXT("compilation_finished"),TEXT("compile_errors"),TEXT("compile_error_count")}) { R.Data->SetField(Field,MakeShared<FJsonValueNull>()); }
    TOptional<bool> FullCompiled;
    auto Row = MakeShared<FJsonObject>(); R.Data->SetObjectField(Key, Row); Row->SetBoolField(TEXT("complete"), false);
    FullCompiled.Reset(); Row->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    Row->SetNumberField(TEXT("shader_platform"), static_cast<int32>(GMaxRHIShaderPlatform)); Row->SetBoolField(TEXT("null_rhi"), GUsingNullRHI);
    Count(R, TEXT("material_resource_queries")); FMaterialResource* Resource = C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);
    TArray<TSharedPtr<FJsonValue>> Errors; Row->SetArrayField(TEXT("compile_errors"), Errors);
    if (!Resource)
    { FullCompiled = false; Row->SetBoolField(TEXT("resource_available"), false); Row->SetStringField(TEXT("status"), TEXT("resource_unavailable")); Errors.Add(MakeShared<FJsonValueString>(TEXT("No material resource for current shader platform."))); Row->SetArrayField(TEXT("compile_errors"), Errors); }
    else
    {
        Row->SetBoolField(TEXT("resource_available"), true); Count(R, TEXT("material_compile_status_queries"));
        const bool Finished=Resource->IsCompilationFinished();if(!ContinueAsyncRead())return false;
        if(CurrentDeferredWork)
        {
            Count(R,TEXT("material_resource_queries"));
            if(C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource||Resource->GetMaterialInterface()!=C.MaterialAsset)
                return CurrentDeferredWork->Error(TEXT("material_async_resource_changed"),TEXT("Current resource changed during ready-cache application."));
            const int32 Q=static_cast<int32>(Resource->GetQualityLevel());
            if(CurrentDeferredWork->Quality.IsSet()&&CurrentDeferredWork->Quality.GetValue()!=Q)
                return CurrentDeferredWork->Error(TEXT("material_async_quality_changed"),TEXT("Current default-quality resource role changed."));
            CurrentDeferredWork->Quality=Q;
        }
        const auto CompileErrors=Resource->GetCompileErrors();int32 Chars=0;
        Row->SetBoolField(TEXT("compilation_finished"), Finished);
        for (const FString& Error : CompileErrors)
        { if (Errors.Num() >= 256 || Error.Len() > 1048576 || Chars > 1048576 - Error.Len()) { return Fail(R, TEXT("material_cache_compile_status_incomplete"), TEXT("Compile error observation bound exceeded.")); } Chars += Error.Len(); Errors.Add(MakeShared<FJsonValueString>(Error)); Row->SetArrayField(TEXT("compile_errors"), Errors); }
        const FMaterialShaderMap* Map = Resource->GetGameThreadShaderMap(); const bool Complete = Resource->IsGameThreadShaderMapComplete(), Valid = Resource->HasValidGameThreadShaderMap();
        Row->SetBoolField(TEXT("game_thread_shader_map_complete"), Complete); Row->SetBoolField(TEXT("has_valid_game_thread_shader_map"), Valid); Row->SetBoolField(TEXT("game_thread_shader_map_present"), Map != nullptr);
        if (Map) { Row->SetBoolField(TEXT("shader_map_compilation_finalized"), Map->IsCompilationFinalized()); Row->SetBoolField(TEXT("shader_map_compiled_successfully"), Map->CompiledSuccessfully()); }
        else { Row->SetField(TEXT("shader_map_compilation_finalized"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("shader_map_compiled_successfully"), MakeShared<FJsonValueNull>()); }
        const bool HadErrorOrCompiling=C.MaterialAsset->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform);
        if(!ContinueAsyncRead())return false;
        if(CurrentDeferredWork){Count(R,TEXT("material_resource_queries"));if(C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource)return CurrentDeferredWork->Error(TEXT("material_async_resource_changed"),TEXT("Current resource changed after Editor criterion query."));}
        Row->SetBoolField(TEXT("original_compiling_or_had_error"), HadErrorOrCompiling);
        if (!CompileErrors.IsEmpty()) { FullCompiled = false; Row->SetStringField(TEXT("status"), TEXT("compile_errors")); }
        else if (Finished && !HadErrorOrCompiling && Complete && Valid && Map && Map->IsCompilationFinalized() && Map->CompiledSuccessfully()) { FullCompiled = true; Row->SetStringField(TEXT("status"), TEXT("current_resource_complete")); }
        else { Row->SetStringField(TEXT("status"), Finished ? TEXT("resource_incomplete") : TEXT("pending")); }
    }
    if (FullCompiled.IsSet()) { Row->SetBoolField(TEXT("compiled"), FullCompiled.GetValue()); }
    Row->SetNumberField(TEXT("compile_error_count"), Errors.Num()); Row->SetBoolField(TEXT("complete"), true);
    for (const TCHAR* Field : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_errors"), TEXT("compile_error_count") }) { R.Data->SetField(Field, Row->TryGetField(Field)); }
    // Original Editor criterion is distinct from the strict full-resource row.
    R.Compiled.Reset();
    if (!Row->GetBoolField(TEXT("resource_available"))) { R.Compiled = false; }
    else if (Row->GetBoolField(TEXT("compilation_finished")))
    { R.Compiled = Row->GetArrayField(TEXT("compile_errors")).IsEmpty() && !Row->GetBoolField(TEXT("original_compiling_or_had_error")); }
    Row->SetField(TEXT("editor_compiled"), R.Compiled.IsSet() ? TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(R.Compiled.GetValue())) : TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
    R.Data->SetField(TEXT("full_resource_compiled"), Row->TryGetField(TEXT("compiled")));
    R.Data->SetField(TEXT("compiled"), Row->TryGetField(TEXT("editor_compiled")));
    R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("Immediate original Editor finished/errors/current-map-presence criterion; no blocking Finish. Full resource flags remain separate actual observations."));
    return true;
}

bool Capture(FContext& C,FResult& R,const TCHAR* Key,TSharedPtr<FJsonObject>& Out)
{
    Count(R,TEXT("material_capture_calls"));Out=MakeShared<FJsonObject>();R.Data->SetObjectField(Key,Out);Out->SetBoolField(TEXT("complete"),false);
    FString Error;Material::FState State;
    const bool Captured=Material::CaptureState(Material::EOperation::ResolveGraph,C.Params,512,State,Error);Count(R,TEXT("material_capture_returns"));
    if(!ContinueAsyncRead())return false;
    if(!Captured)
    { if(State.Snapshot.IsValid()) { Out->SetObjectField(TEXT("graph"),State.Snapshot); }R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_observation_incomplete"),Error); }
    Out->SetObjectField(TEXT("graph"),State.Snapshot);Out->SetStringField(TEXT("material"),State.AssetPath);Out->SetBoolField(TEXT("dirty"),State.bDirty);
    auto* A=Cast<UMaterial>(State.Asset.Get());
    if(!Loaded(A)||(C.Asset.IsValid()&&A!=C.Asset.Get())) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_target_changed"),TEXT("Current resolved Material identity/lifetime differs.")); }
    if(!C.Asset.IsValid()) { C.Asset.Reset(A);C.MaterialAsset=A; }
    C.Selected=MoveTemp(State);if(!ContinueAsyncRead())return false;Out->SetStringField(TEXT("class"),A->GetClass()->GetPathName());Out->SetStringField(TEXT("package"),A->GetOutermost()->GetName());
    auto File=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("disk"),File);if(!Disk(A,File)) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_disk_incomplete"),TEXT("Selected disk proof exceeded64MiB or failed; prefix retained.")); }
    const auto Current=A->GetExpressions();TArray<TSharedPtr<FJsonValue>> Identity;Out->SetArrayField(TEXT("current_expressions"),Identity);
    if(!C.Expressions.IsEmpty()&&Current.Num()!=C.Expressions.Num()) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_expression_set_changed"),TEXT("Current expression count differs after callbacks.")); }
    for(int32 I=0;I<Current.Num();++I)
    {
        auto Row=MakeShared<FJsonObject>();Identity.Add(MakeShared<FJsonValueObject>(Row));Out->SetArrayField(TEXT("current_expressions"),Identity);auto* X=Current[I].Get();
        if(!Loaded(X)||X->GetOuter()!=A||(!C.Expressions.IsEmpty()&&X!=C.Expressions[I].Get())) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_expression_set_changed"),TEXT("Current expression ownership/order/lifetime differs.")); }
        Row->SetNumberField(TEXT("index"),I);Row->SetStringField(TEXT("path"),X->GetPathName());Row->SetStringField(TEXT("class"),X->GetClass()->GetPathName());Row->SetStringField(TEXT("guid"),X->MaterialExpressionGuid.ToString());
    }
    Count(R,TEXT("cache_observation_calls"));auto Cache=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("cache"),Cache);Cache->SetBoolField(TEXT("complete"),false);
    const auto& Data=A->GetCachedExpressionData();Cache->SetNumberField(TEXT("function_info_count"),Data.FunctionInfos.Num());Cache->SetNumberField(TEXT("function_infos_state_crc"),Data.FunctionInfosStateCRC);Cache->SetBoolField(TEXT("has_material_layers"),Data.bHasMaterialLayers);
    TArray<TSharedPtr<FJsonValue>> Functions;Cache->SetArrayField(TEXT("function_infos"),Functions);
    if(Data.FunctionInfos.Num()>4096) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_observation_budget"),TEXT("Function info4096 budget exceeded; count/prefix retained.")); }
    for(const auto& Info:Data.FunctionInfos)
    {
        auto Row=MakeShared<FJsonObject>();Functions.Add(MakeShared<FJsonValueObject>(Row));Cache->SetArrayField(TEXT("function_infos"),Functions);Row->SetStringField(TEXT("state_id"),Info.StateId.ToString());auto* F=Info.Function.Get();Row->SetBoolField(TEXT("function_present"),F!=nullptr);
        if(F) { if(!Loaded(F)||F->GetPathName().Len()>2048) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_function_info_not_ready"),TEXT("Cached referenced function not loaded/bounded; available row retained.")); }if(CurrentDeferredWork){if(!CurrentDeferredWork->Own(F))return false;}else C.Functions.Emplace(F);Row->SetStringField(TEXT("function"),F->GetPathName());Row->SetStringField(TEXT("class"),F->GetClass()->GetPathName()); }
        else { Row->SetStringField(TEXT("function"),TEXT(""));Row->SetStringField(TEXT("class"),TEXT("")); }
    }
    Cache->SetBoolField(TEXT("complete"),true);Out->SetBoolField(TEXT("complete"),true);return true;
}
bool VerifyGraph(FContext& C,FResult& R,const TSharedPtr<FJsonObject>& After)
{
    const bool Same=Equal(MakeShared<FJsonValueObject>(C.Before->GetObjectField(TEXT("graph"))),MakeShared<FJsonValueObject>(After->GetObjectField(TEXT("graph"))));
    R.Data->SetBoolField(TEXT("selected_graph_preserved"),Same);
    if(!Same) { R.bChangeStateKnown=false;return Fail(R,TEXT("material_cache_graph_changed"),TEXT("Captured graph/links/fields changed during public callbacks; no repair/retry.")); }
    return true;
}
bool StaleResolved(FContext& C,FResult& R,const TSharedPtr<FJsonObject>& Point)
{
    const auto Cache=Point->GetObjectField(TEXT("cache"));const bool OK=C.FunctionCallCount!=0||Cache->GetNumberField(TEXT("function_info_count"))==0;
    R.Data->SetBoolField(TEXT("no_call_stale_function_infos_resolved"),OK);
    R.Data->SetNumberField(TEXT("function_info_count_after_clear"),Cache->GetNumberField(TEXT("function_info_count")));
    if(!OK) { R.bChangeStateKnown=true;R.bChanged=!Equal(MakeShared<FJsonValueObject>(C.Before),MakeShared<FJsonValueObject>(Point));return Fail(R,TEXT("material_cache_private_clear_unavailable"),TEXT("No direct function-call nodes but public rebuild retained function infos; original const_cast clear/CRC invalidation unavailable. Actual public-call prefix retained, no private write or compile/save continuation.")); }
    return true;
}
void MarkChanged(FContext& C,FResult& R)
{
    if(!ContinueAsyncRead())return;
    R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("explicit_asset_modify_calls"));C.Asset->Modify();if(!ContinueAsyncRead())return;
    Count(R,TEXT("mark_package_dirty_calls"));C.Asset->MarkPackageDirty();if(!ContinueAsyncRead())return;
    bool Unconnected=false;for(const auto& P:C.Asset->GetExpressions())
    { if(const auto* Custom=Cast<UMaterialExpressionCustom>(P.Get())) { for(const auto& Input:Custom->Inputs) { Count(R,TEXT("save_prepare_custom_input_visits"));if(!Input.Input.Expression) { Unconnected=true; } } } }
    R.Data->SetBoolField(TEXT("save_prepare_PostEdit_skipped_for_unconnected_custom"),Unconnected);
    if(!Unconnected) { Count(R,TEXT("explicit_post_edit_change_calls"));C.Asset->PostEditChange(); }
    ContinueAsyncRead();
}
bool Dependencies(FContext& C,FResult& R)
{
    R.Stage=TEXT("registry_dependencies");Count(R,TEXT("registry_module_query_calls"));auto* Module=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if(!Module||!Module->TryGet()) { return Fail(R,TEXT("material_cache_registry_unavailable"),TEXT("Already-loaded AssetRegistry required; original module-loading variant remains incomplete.")); }
    const auto File=R.Data->GetObjectField(TEXT("last_observation"))->GetObjectField(TEXT("disk"));TArray<FString> Files{File->GetStringField(TEXT("path"))};
    R.Data->SetStringField(TEXT("dependency_scan_filename"),Files[0]);R.Data->SetBoolField(TEXT("dependency_scan_disk_exists"),File->GetBoolField(TEXT("exists")));
    // Scan callbacks can invalidate prior current-status claims; nested phase receipts stay intact.
    R.Compiled.Reset();for(const TCHAR* Key:{TEXT("compiled"),TEXT("full_resource_compiled"),TEXT("compilation_finished"),TEXT("compile_errors"),TEXT("compile_error_count")}) { R.Data->SetField(Key,MakeShared<FJsonValueNull>()); }
    Count(R,TEXT("registry_scan_calls"));Module->Get().ScanModifiedAssetFiles(Files);Count(R,TEXT("registry_scan_returns"));if(!ContinueAsyncRead())return false;Count(R,TEXT("registry_module_query_calls"));auto* CurrentRegistryModule=FModuleManager::GetModulePtr<FAssetRegistryModule>(TEXT("AssetRegistry"));if(CurrentRegistryModule!=Module||!CurrentRegistryModule||!CurrentRegistryModule->TryGet())return Fail(R,TEXT("material_cache_registry_changed"),TEXT("AssetRegistry module changed during scan; no dependency-query replay."));
    TArray<FName> Dependencies;Count(R,TEXT("registry_dependency_query_calls"));const bool Found=Module->Get().GetDependencies(FName(*C.Asset->GetOutermost()->GetName()),Dependencies);Count(R,TEXT("registry_dependency_query_returns"));
    R.Data->SetBoolField(TEXT("dependency_query_succeeded"),Found);R.Data->SetNumberField(TEXT("dependency_total_count"),Dependencies.Num());if(!ContinueAsyncRead())return false;
    TArray<TSharedPtr<FJsonValue>> Rows,Source;int32 Chars=0;
    for(int32 I=0;I<FMath::Min(Dependencies.Num(),4096);++I)
    {
        const FString Path=Dependencies[I].ToString();if(Path.Len()>2048||Chars>1048576-Path.Len()) { R.Data->SetArrayField(TEXT("dependencies"),Rows);R.Data->SetArrayField(TEXT("source_or_temp_dependencies"),Source);return Fail(R,TEXT("material_cache_dependencies_incomplete"),TEXT("Dependency string budget exceeded; returned prefix retained.")); }Chars+=Path.Len();Rows.Add(MakeShared<FJsonValueString>(Path));
        if(Path.StartsWith(TEXT("/Game/UltraDynamicSky"))||Path.StartsWith(TEXT("/Game/_MCP_Temp"))) { Source.Add(MakeShared<FJsonValueString>(Path)); }
    }
    R.Data->SetArrayField(TEXT("dependencies"),Rows);R.Data->SetArrayField(TEXT("source_or_temp_dependencies"),Source);R.Data->SetNumberField(TEXT("source_or_temp_dependency_count"),Source.Num());
    const bool Complete=Found&&Dependencies.Num()<=4096;R.Data->SetBoolField(TEXT("dependencies_complete"),Complete);
    if(!Complete) { return Fail(R,TEXT("material_cache_dependencies_incomplete"),TEXT("Current on-disk dependency query incomplete; empty/prefix is not absence proof.")); }return true;
}
}

bool CompleteCacheSave(FContext& C,FResult& R,bool Save,bool Ready,TSharedPtr<FJsonObject>& After)
{
    if(Save&&Ready)
    {
        R.Stage=TEXT("prepare_save");R.Compiled.Reset();for(const TCHAR* Key:{TEXT("compiled"),TEXT("full_resource_compiled"),TEXT("compilation_finished"),TEXT("compile_errors"),TEXT("compile_error_count")}) { R.Data->SetField(Key,MakeShared<FJsonValueNull>()); }
        MarkChanged(C,R);if(!ContinueAsyncRead())return false;auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),C.Asset->GetPathName());Call->SetBoolField(TEXT("only_if_dirty"),false);Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
        R.Stage=TEXT("save");R.bSaveAttempted=true;Count(R,TEXT("save_attempt_count"));const bool SavedReturn=UEditorAssetLibrary::SaveLoadedAsset(C.Asset.Get(),false);R.bSaved|=SavedReturn;Count(R,TEXT("save_return_count"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),SavedReturn);R.Data->SetBoolField(TEXT("saved"),R.bSaved);if(!ContinueAsyncRead())return false;
        if(!R.bSaved) { R.Data->SetBoolField(TEXT("legacy_returned_saved_false"),true); }
        R.Stage=TEXT("after_save_observation");if(!Capture(C,R,TEXT("after_save_observation"),After)) { return false; }R.Data->SetObjectField(TEXT("last_observation"),After);if(!VerifyGraph(C,R,After)||!StaleResolved(C,R,After)||!ObserveCompile(C,R,TEXT("final_compile_status"))) { return false; }
        const auto File=After->GetObjectField(TEXT("disk"));R.Data->SetBoolField(TEXT("save_disk_verified"),R.bSaved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!After->GetBoolField(TEXT("dirty")));
    }
    return true;
}

static Variable::FResult RunRefresh(const FString& Path,bool Save,FDeferredWork* Deferred)
{
    FResult LocalR;FResult& R=Deferred?Deferred->Result:LocalR;Init(R,Save);Material::FReadMetricsScope Metrics;auto Finish=[&](){R.Data->SetObjectField(TEXT("shared_material_read_metrics"),Metrics.ToJson());return R;};
    if(!IsEditorGameThread()||IsPlaySessionActive()||Path.IsEmpty()||Path.Len()>2048||HasNul(Path)) { Fail(R,TEXT("material_cache_input_or_editor_invalid"),TEXT("Bounded nonempty Material path and idle Editor game thread required."));return Finish(); }
    Count(R,TEXT("outer_transaction_calls"));TUniquePtr<FScopedTransaction> Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP","RefreshMaterialCache","Refresh Material Cached Expression Data"));FContext LocalC;FContext& C=Deferred?Deferred->Context:LocalC;C.Params->SetStringField(TEXT("material_path"),Path);C.Params->SetStringField(TEXT("graph_type"),TEXT("material"));
    R.Stage=TEXT("before_observation");if(!Capture(C,R,TEXT("before_observation"),C.Before)) { return Finish(); }FString Error;
    if(!Material::ValidateMutationTarget(C.Selected,Error)||C.Asset->GetClass()!=UMaterial::StaticClass()||!C.Asset->IsAsset()||C.Asset->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)
        ||C.Asset->GetOutermost()->ContainsMap()||C.Asset->GetOutermost()->HasAnyPackageFlags(PKG_Cooked)||(Save&&!C.Asset->GetOutermost()->IsFullyLoaded()))
    { Fail(R,TEXT("material_cache_target_unsupported"),TEXT("Finite loaded ordinary /Game native uncooked Material, fully loaded package for save required: ")+Error);return Finish(); }
    for(const auto& P:C.Asset->GetExpressions())
    { Count(R,TEXT("expression_scan_visits"));if(!Loaded(P.Get())||P->GetOuter()!=C.Asset.Get()||P->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)) { Fail(R,TEXT("material_cache_expression_not_ready"),TEXT("Loaded ordinary direct-owned expressions required."));return Finish(); }C.Expressions.Emplace(P.Get());if(Cast<UMaterialExpressionMaterialFunctionCall>(P.Get())) { ++C.FunctionCallCount; } }
    R.Data->SetStringField(TEXT("material"),C.Selected.AssetPath);R.Data->SetStringField(TEXT("graph_type"),TEXT("material"));R.Data->SetNumberField(TEXT("material_function_call_count"),C.FunctionCallCount);R.Data->SetBoolField(TEXT("was_dirty_before_refresh"),C.Before->GetBoolField(TEXT("dirty")));
    R.Stage=TEXT("public_cache_update");R.bMutationAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("explicit_asset_modify_calls"));C.Asset->Modify();if(!ContinueAsyncRead())return Finish();Count(R,TEXT("update_cached_expression_data_calls"));C.Asset->UpdateCachedExpressionData();Count(R,TEXT("update_cached_expression_data_returns"));if(!ContinueAsyncRead())return Finish();R.Data->SetBoolField(TEXT("cached_expression_data_refreshed"),true);R.Data->SetBoolField(TEXT("public_cache_update_returned"),true);
    TSharedPtr<FJsonObject> After;R.Stage=TEXT("after_update_observation");if(!Capture(C,R,TEXT("after_update_observation"),After)) { return Finish(); }R.Data->SetObjectField(TEXT("last_observation"),After);R.Data->SetNumberField(TEXT("function_info_count_before_clear"),After->GetObjectField(TEXT("cache"))->GetNumberField(TEXT("function_info_count")));
    R.Data->SetBoolField(TEXT("public_update_removed_function_infos"),C.Before->GetObjectField(TEXT("cache"))->GetNumberField(TEXT("function_info_count"))>After->GetObjectField(TEXT("cache"))->GetNumberField(TEXT("function_info_count")));
    if(!VerifyGraph(C,R,After)||!StaleResolved(C,R,After)) { return Finish(); }
    R.Stage=TEXT("post_edit_change");Count(R,TEXT("explicit_post_edit_change_calls"));C.Asset->PostEditChange();
    if(!ContinueAsyncRead())return Finish();
    R.Stage=TEXT("after_post_edit_observation");if(!Capture(C,R,TEXT("after_post_edit_observation"),After)) { return Finish(); }R.Data->SetObjectField(TEXT("last_observation"),After);if(!VerifyGraph(C,R,After)||!StaleResolved(C,R,After)) { return Finish(); }
    R.Stage=TEXT("compile_dispatch");R.bCompileAttempted=true;
    {
        Count(R,TEXT("material_update_context_calls"));FMaterialUpdateContext Update;Update.AddMaterial(C.Asset.Get());if(!ContinueAsyncRead())return Finish();
        Count(R,TEXT("explicit_pre_edit_change_calls"));C.Asset->PreEditChange(nullptr);if(!ContinueAsyncRead())return Finish();
        Count(R,TEXT("explicit_post_edit_change_calls"));C.Asset->PostEditChange();if(!ContinueAsyncRead())return Finish();
        Count(R,TEXT("force_recompile_background_calls"));C.Asset->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Background);if(!ContinueAsyncRead())return Finish();
    }
    if(!ContinueAsyncRead())return Finish();
    Count(R,TEXT("mark_package_dirty_calls"));C.Asset->MarkPackageDirty();
    if(!ContinueAsyncRead())return Finish();
    R.Stage=TEXT("after_compile_observation");if(!Capture(C,R,TEXT("after_compile_observation"),After)) { return Finish(); }R.Data->SetObjectField(TEXT("last_observation"),After);if(!VerifyGraph(C,R,After)||!StaleResolved(C,R,After)||!ObserveCompile(C,R,TEXT("initial_compile_status"))) { return Finish(); }
    if(Deferred)
    {
        Deferred->Before=C.Before.ToSharedRef();Deferred->After=After.ToSharedRef();Deferred->Save=Save;
        Transaction.Reset();R.Data->SetBoolField(TEXT("transaction_closed_before_yield"),true);
        if(!ContinueAsyncRead())return Finish();Deferred->Ready=true;R.Stage=TEXT("cache_compile_dispatched");return Finish();
    }
    const bool Ready=R.Compiled.IsSet()&&R.Compiled.GetValue();R.Data->SetBoolField(TEXT("initial_editor_compile_gate_passed"),Ready);R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),Save&&!R.Compiled.IsSet());R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&R.Compiled.IsSet()&&!R.Compiled.GetValue());
    if(!CompleteCacheSave(C,R,Save,Ready,After))return Finish();
    R.Data->SetBoolField(TEXT("saved"),R.bSaved);R.Data->SetBoolField(TEXT("dirty_after_refresh"),After->GetBoolField(TEXT("dirty")));R.bChanged=!Equal(MakeShared<FJsonValueObject>(C.Before),MakeShared<FJsonValueObject>(After));
    if(!Dependencies(C,R)) { R.bChangeStateKnown=false;return Finish(); }
    R.Stage=TEXT("after_registry_observation");if(!Capture(C,R,TEXT("after_registry_observation"),After)) { return Finish(); }R.Data->SetObjectField(TEXT("last_observation"),After);
    if(!VerifyGraph(C,R,After)||!StaleResolved(C,R,After)||!ObserveCompile(C,R,TEXT("last_compile_status"))) { return Finish(); }
    R.Data->SetBoolField(TEXT("dirty_after_refresh"),After->GetBoolField(TEXT("dirty")));R.bChanged=!Equal(MakeShared<FJsonValueObject>(C.Before),MakeShared<FJsonValueObject>(After));
    if(!R.Compiled.IsSet()) { R.bChangeStateKnown=false;Fail(R,TEXT("material_cache_compile_pending"),TEXT("Immediate Editor criterion pending; actual refresh/compile/save/dependency prefix retained, no blocking Finish or automatic retry."));return Finish(); }
    R.bChangeStateKnown=true;
    if(!R.Compiled.GetValue()) { Fail(R,TEXT("material_cache_compile_failed"),TEXT("Current original Editor error/presence criterion failed; actual prefix retained."));return Finish(); }
    if(Save&&!R.bSaveAttempted) { R.Stage=TEXT("requested_save_unperformed");Fail(R,TEXT("material_cache_requested_save_unperformed"),TEXT("Requested save was skipped at the initial Editor compile gate; a later current compiled result does not complete that save boundary. Actual cache/compile/dependency prefix and current status retained; no automatic save or retry."));return Finish(); }
    if(R.bSaveAttempted&&(!R.bSaved||!R.Data->GetBoolField(TEXT("save_disk_verified")))) { Fail(R,TEXT("material_cache_save_failed"),TEXT("Returned save/disk criterion failed; saved prefix retained, no second save."));return Finish(); }
    R.bSuccess=R.bPostconditionPassed=true;R.Stage=TEXT("complete");return Finish();
}
Variable::FResult Refresh(const FString& Path,bool Save)
{
 if(AsyncRunning||AsyncLease){FResult R;R.bSaveRequested=Save;R.bCompileRequested=true;Fail(R,TEXT("material_async_lease_active"),TEXT("An owned Material continuation holds this cache core; no synchronous replay."));return R;}
 return RunRefresh(Path,Save,nullptr);
}

namespace
{

bool ParseMaterialJobParams(const FString& ParamsText,TSharedPtr<FJsonObject>& Params,int32 Fields)
{
 if(ParamsText.IsEmpty()||ParamsText.Len()>16384)return false;
 bool InString=false,Escaped=false;int32 Depth=0;
 for(TCHAR Ch:ParamsText)
 {
  if(Ch==0)return false;
  if(InString){if(Escaped)Escaped=false;else if(Ch=='\\')Escaped=true;else if(Ch=='"')InString=false;}
  else if(Ch=='"')InString=true;else if(Ch=='{'||Ch=='['){if(++Depth>32)return false;}
  else if(Ch=='}'||Ch==']'){if(--Depth<0)return false;}
 }
 if(Depth||InString)return false;
 auto Reader=TJsonReaderFactory<>::Create(ParamsText);EJsonNotation Token;int32 Tokens=0;
 while(Reader->ReadNext(Token))if(Token==EJsonNotation::Error||++Tokens>128)return false;
 return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ParamsText),Params)&&Params.IsValid()&&Params->Values.Num()==Fields;
}

enum class EAsyncPhase:uint8 { Dispatch,WaitInitial,Save,WaitSaved,Audit,WaitFinal,Final,Terminal,Quarantined };
struct FCacheJobState:public TSharedFromThis<FCacheJobState>
{
 FDeferredWork Work;
 TStrongObjectPtr<UCubelessMaterialPostprocessJob> Job{nullptr};
 TUniquePtr<FAsyncIdentity> JobIdentity;
 UCubelessMaterialPostprocessJob* RawJob=nullptr;
 FGuid Id=FGuid::NewGuid();FString LastSnapshot,MaterialPath;
 bool Save=true,Executing=false,Dispatched=false,FinalEncoded=false,FrameMetricsMerged=false,InitialCompileFailed=false,PostSaveCompileFailed=false,AuditDispatched=false;Material::FReadMetricsScope* FrameMetrics=nullptr;
 int64 Revision=0;uint64 LastFrame=MAX_uint64;
 EAsyncPhase Phase=EAsyncPhase::Dispatch;FTSTicker::FDelegateHandle Ticker;FDelegateHandle PreExit;
 bool PublicationSafe()const{return Work.PublicationSafe()&&(!JobIdentity||JobIdentity->Exact());}
 void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}if(PreExit.IsValid()){FCoreDelegates::OnEnginePreExit.Remove(PreExit);PreExit.Reset();}}
 void Metadata(bool Terminal)
 {
  if(FrameMetrics&&!FrameMetricsMerged){Work.MergeMetrics(FrameMetrics->ToJson());FrameMetricsMerged=true;}
  auto& R=Work.Result;R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::DigitsWithHyphens));
  R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);R.Data->SetBoolField(TEXT("native_job_terminal"),Terminal);R.Data->SetBoolField(TEXT("operation_complete"),Terminal);
  R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("do_not_retry"),true);
  R.Data->SetBoolField(TEXT("async_core_dispatched"),Dispatched);R.Data->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  double Modifies=0;R.Data->TryGetNumberField(TEXT("explicit_asset_modify_calls"),Modifies);const bool Persistent=Modifies>0||R.bCompileAttempted||R.bSaveAttempted;
  R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Persistent);R.Data->SetBoolField(TEXT("transient_creation_only"),!Persistent);
  R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false);R.Data->SetBoolField(TEXT("current_resource_completion_continuation"),true);
  R.Data->SetStringField(TEXT("continuation_scope"),TEXT("Owned current-platform/default-quality Material completion and save-resume, same original core. Editor completion differs from strict shader-map/render diagnostics. No FinishCompilation/global pump/streaming-equivalence claim, no source writer replay."));
  R.Data->SetObjectField(TEXT("shared_material_read_metrics"),Work.Metrics);
 }
 FString Encoded()
 {return Operations::EncodeVariableResult(Work.Result,TEXT("refresh_material_cached_expression_data"));}
 void Cache(bool Terminal,bool Safe)
 {LastSnapshot=Encoded();MaterialPostprocessAsync::Publish(RawJob,LastSnapshot,Terminal,Safe);}
 void Quarantine(const FString& Reason)
 {
  if(Phase==EAsyncPhase::Quarantined)return;Unhook();Work.CancelRequested=true;Work.CancelReason=Reason;
  Work.Error(TEXT("material_async_context_quarantined"),Reason);Work.Result.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Work.Result.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Metadata(false);Phase=EAsyncPhase::Quarantined;Cache(false,false);
 }
 void Pending()
 {if(!PublicationSafe()){Quarantine(TEXT("Material pending publication lost exact Editor/world/Job."));return;}Work.Result.bSuccess=false;Work.Result.bPostconditionPassed=false;Metadata(false);Cache(false,true);}
 bool Observe(const TCHAR* Name)
 {
  if(!Work.Admit())return false;
  TSharedPtr<FJsonObject> Point;
  if(!Capture(Work.Context,Work.Result,Name,Point))return false;
  Work.After=Point.ToSharedRef();Work.Result.Data->SetObjectField(TEXT("last_observation"),Point);
  if(!VerifyGraph(Work.Context,Work.Result,Point)||!StaleResolved(Work.Context,Work.Result,Point))return false;
  Work.Result.bChanged|=!Equal(MakeShared<FJsonValueObject>(Work.Before),MakeShared<FJsonValueObject>(Point));
  return true;
 }
 bool Point(const TCHAR* Name)
 {
  return Work.Admit()&&Observe(TEXT("async_current_observation"))&&ObserveCompile(Work.Context,Work.Result,Name);
 }
 void FinishSuccess()
 {Work.Result.bChangeStateKnown=true;Work.Result.bSuccess=true;Work.Result.bPostconditionPassed=true;Work.Result.Stage=TEXT("complete");Phase=EAsyncPhase::Final;}
 void Finalize()
 {
  if(FinalEncoded||Phase==EAsyncPhase::Terminal||Phase==EAsyncPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("Material final publication lost Editor/world/Job context."));return;}
  if(Work.Result.Error.IsEmpty()&&!Work.Admit()){}
  if(!Work.Result.Error.IsEmpty()){Work.Result.bSuccess=false;Work.Result.bPostconditionPassed=false;}
  Metadata(true);FinalEncoded=true;Cache(true,true);Phase=EAsyncPhase::Terminal;Unhook();
 }
 void Execute()
 {
  if(Work.CancelRequested||MaterialPostprocessAsync::IsStopping())Work.Error(TEXT("material_async_cancelled"),Work.CancelReason.IsEmpty()?TEXT("Material cache continuation stopped; no save/recovery/replay."):Work.CancelReason);
  if(!Work.Result.Error.IsEmpty()){Phase=EAsyncPhase::Final;Finalize();return;}
  if(!Work.Admit()){Phase=EAsyncPhase::Final;Finalize();return;}
  if(Phase==EAsyncPhase::Dispatch)
  {
   if(Dispatched){Work.Error(TEXT("material_async_replay_forbidden"),TEXT("Original cache refresh already dispatched."));Phase=EAsyncPhase::Final;Finalize();return;}
   Dispatched=true;RunRefresh(MaterialPath,Save,&Work);
   const TSharedPtr<FJsonObject>* M=nullptr;
   if(Work.Result.Data->TryGetObjectField(TEXT("shared_material_read_metrics"),M)&&M&&M->IsValid()&&M->Get()!=&Work.Metrics.Get())Work.MergeMetrics((*M).ToSharedRef());
   Phase=Work.Ready&&Work.Result.Error.IsEmpty()?EAsyncPhase::WaitInitial:EAsyncPhase::Final;
  }
  else if(Phase==EAsyncPhase::WaitInitial)
  {
   if(!Point(TEXT("initial_compile_status")))Phase=EAsyncPhase::Final;
   else if(Work.Result.Compiled.IsSet())
   {
    InitialCompileFailed=!Work.Result.Compiled.GetValue();Work.Result.bChangeStateKnown=true;
    Work.Result.Data->SetBoolField(TEXT("initial_editor_compile_gate_passed"),!InitialCompileFailed);
    Work.Result.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);
    Work.Result.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&InitialCompileFailed);
    Phase=InitialCompileFailed?EAsyncPhase::Audit:EAsyncPhase::Save;
   }
  }
  else if(Phase==EAsyncPhase::Save)
  {
   if(!Observe(TEXT("async_before_save")))Phase=EAsyncPhase::Final;
   else
   {
    TSharedPtr<FJsonObject> After=Work.After;
    if(!CompleteCacheSave(Work.Context,Work.Result,Save,true,After))Phase=EAsyncPhase::Final;
    else {if(After)Work.After=After.ToSharedRef();Phase=Save&&!Work.Result.Compiled.IsSet()?EAsyncPhase::WaitSaved:EAsyncPhase::Audit;}
   }
  }
  else if(Phase==EAsyncPhase::WaitSaved)
  {
   if(!Point(TEXT("final_compile_status")))Phase=EAsyncPhase::Final;
   else if(Work.Result.Compiled.IsSet()){PostSaveCompileFailed=!Work.Result.Compiled.GetValue();Phase=EAsyncPhase::Audit;}
  }
  else if(Phase==EAsyncPhase::Audit)
  {
   if(AuditDispatched){Work.Error(TEXT("material_async_audit_replay_forbidden"),TEXT("Registry audit already dispatched."));Phase=EAsyncPhase::Final;}
   else
   {
    AuditDispatched=true;
    if(!Observe(TEXT("async_before_registry")))Phase=EAsyncPhase::Final;
     else if(!Dependencies(Work.Context,Work.Result)){Work.Result.bChangeStateKnown=false;Phase=EAsyncPhase::Final;}
    else Phase=EAsyncPhase::WaitFinal;
   }
  }
  else if(Phase==EAsyncPhase::WaitFinal)
  {
   if(!Point(TEXT("last_compile_status")))Phase=EAsyncPhase::Final;
   else if(Work.Result.Compiled.IsSet())
   {
    Work.Result.bChangeStateKnown=true;
    if(InitialCompileFailed||PostSaveCompileFailed||!Work.Result.Compiled.GetValue())
    {Fail(Work.Result,TEXT("material_cache_compile_failed"),TEXT("Actual Editor compile criterion failed; returned cache/compiler/save/audit prefix retained."));Phase=EAsyncPhase::Final;}
    else if(Save&&!Work.Result.bSaveAttempted)
    {Fail(Work.Result,TEXT("material_cache_requested_save_unperformed"),TEXT("Requested save was not executed; no inferred or repeated save."));Phase=EAsyncPhase::Final;}
    else if(Work.Result.bSaveAttempted&&(!Work.Result.bSaved||!Work.Result.Data->GetBoolField(TEXT("save_disk_verified"))))
    {Fail(Work.Result,TEXT("material_cache_save_failed"),TEXT("Actual save/disk/clean criterion failed; retained prefix and no second save."));Phase=EAsyncPhase::Final;}
    else {Work.Result.Data->SetBoolField(TEXT("saved"),Work.Result.bSaved);Work.Result.Data->SetBoolField(TEXT("dirty_after_refresh"),Work.After->GetBoolField(TEXT("dirty")));FinishSuccess();}
   }
  }
  else if(Phase==EAsyncPhase::Final){Finalize();return;}
  if(!Work.Result.Error.IsEmpty()){Phase=EAsyncPhase::Final;Finalize();return;}
  if(Phase==EAsyncPhase::Final){Finalize();return;}
  Pending();
 }
 void ReleaseSafe();
 bool Tick(float)
 {
  TSharedRef<FCacheJobState> Hold=AsShared();if(Phase==EAsyncPhase::Terminal||Phase==EAsyncPhase::Quarantined)return false;
  if(Executing||AsyncRunning||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("Material ticker lost its exact Editor/world/GC/exit context."));return false;}
  {
   TGuardValue<bool> Busy(Executing,true);TGuardValue<bool> Lease(AsyncRunning,true);
   TFunction<bool()> Admission=[this](){return Work.Admit();};TGuardValue<TFunction<bool()>*> Read(AsyncReadAdmission,&Admission);TGuardValue<FDeferredWork*> Scope(CurrentDeferredWork,&Work);
   Material::FReadMetricsScope Metrics;FrameMetrics=&Metrics;FrameMetricsMerged=false;Execute();FrameMetrics=nullptr;
  }
  ReleaseSafe();return Phase!=EAsyncPhase::Terminal&&Phase!=EAsyncPhase::Quarantined;
 }
 void Hook()
 {
  const TWeakPtr<FCacheJobState> Weak=AsShared();
  Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto S=Weak.Pin();return S&&S->Tick(Delta);}),0.0f);
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([](){MaterialPostprocessAsync::Shutdown(TEXT("engine_pre_exit"));});
 }
};
TSharedPtr<FCacheJobState>& ActiveCacheJob(){static auto* P=new TSharedPtr<FCacheJobState>;return *P;}
void FCacheJobState::ReleaseSafe()
{
 if(Executing||Phase!=EAsyncPhase::Terminal||!PublicationSafe())return;
 Work.Objects.Reset();Work.Context.Selected.Asset.Reset();Work.Context.Selected.SelectedExpression.Reset();JobIdentity.Reset();Job.Reset();
 if(ActiveCacheJob().Get()==this){ActiveCacheJob().Reset();AsyncLease=false;}
}
}
UCubelessMaterialPostprocessJob* StartCacheRefreshContinuation(const FString& ParamsJson)
{
 if(!AsyncSafe()||AsyncRunning||AsyncLease||MaterialPostprocessAsync::IsStopping())return nullptr;
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!AsyncLive(World)||World->bIsTearingDown||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return nullptr;
 auto S=MakeShared<FCacheJobState>();S->Work.Editor=GEditor;S->Work.World=World;ActiveCacheJob()=S;AsyncLease=true;
 UCubelessMaterialPostprocessJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(S->Executing,true);TGuardValue<bool> Lease(AsyncRunning,true);
  S->Work.Result.bCompileRequested=true;S->Work.Result.bSaveRequested=true;
  S->Work.Result.bMutationAttempted=true;S->Work.Result.Data->SetNumberField(TEXT("NewObject_MaterialPostprocessJob"),1);
  Created=NewObject<UCubelessMaterialPostprocessJob>(GetTransientPackage());S->RawJob=Created;
  S->Work.Result.Data->SetBoolField(TEXT("job_allocation_returned"),true);S->Work.Result.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
  if(!S->PublicationSafe()||S->Work.CancelRequested||MaterialPostprocessAsync::IsStopping()){S->Quarantine(TEXT("Material Job allocation interrupted before target dispatch."));return nullptr;}
  if(!Created||!AsyncReadable(Created)){ActiveCacheJob().Reset();AsyncLease=false;return nullptr;}
  S->Job.Reset(Created);S->JobIdentity=MakeUnique<FAsyncIdentity>(Created);
  MaterialPostprocessAsync::RegisterJob(Created,MaterialPostprocessAsync::EJobKind::CacheRefresh,S->Id);
  TSharedPtr<FJsonObject> Params;
  if(!ParseMaterialJobParams(ParamsJson,Params,2)||
     !Params->TryGetStringField(TEXT("material_path"),S->MaterialPath)||!Params->TryGetBoolField(TEXT("save"),S->Save))
  {Fail(S->Work.Result,TEXT("material_async_params_invalid"),TEXT("Normalized material_path/save JSON object required."));S->Phase=EAsyncPhase::Final;S->Finalize();}
  else {S->Work.Result.bSaveRequested=S->Save;S->Pending();}
 }
 if(S->Phase==EAsyncPhase::Terminal){S->ReleaseSafe();return Created;}if(S->Phase!=EAsyncPhase::Quarantined)S->Hook();return Created;
}
bool CancelCacheRefreshContinuation(UCubelessMaterialPostprocessJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!ActiveCacheJob()||ActiveCacheJob()->RawJob!=Job)return false;
 auto S=ActiveCacheJob();if(S->Phase==EAsyncPhase::Terminal||S->Phase==EAsyncPhase::Quarantined)return false;
 S->Work.CancelRequested=true;S->Work.CancelReason=Reason.Left(1024);S->Unhook();if(S->Executing)return true;
 if(!S->PublicationSafe()){S->Quarantine(TEXT("Material cancellation retained unsafe state without new SDK/publication."));return true;}
 {TGuardValue<bool> Busy(S->Executing,true);S->Work.Error(TEXT("material_async_cancelled"),S->Work.CancelReason);S->Phase=EAsyncPhase::Final;S->Finalize();}
 S->ReleaseSafe();return true;
}
void ShutdownCacheRefreshContinuation(const FString& Reason)
{
 auto S=ActiveCacheJob();if(!S)return;S->Work.CancelRequested=true;S->Work.CancelReason=Reason.Left(1024);S->Unhook();if(S->Executing)return;
 if(!S->PublicationSafe()){S->Quarantine(TEXT("Module shutdown retained unsafe Material context."));return;}
 CancelCacheRefreshContinuation(S->RawJob,S->Work.CancelReason);
}
bool HasCacheRefreshContinuationLease(){return AsyncLease;}

}
