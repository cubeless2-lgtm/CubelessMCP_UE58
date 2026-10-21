#include "Compatibility/UE58/CubelessMaterialReferenceApi.h"
#include "Serialization/JsonWriter.h"
#include "Materials/MaterialFunctionInterface.h"
#include "CubelessMaterialPostprocessJob.h"
#include "Operations/CubelessVariableOperations.h"
#include "Serialization/JsonReader.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Compatibility/UE58/CubelessMaterialApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShaderPrecompileMode.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialParameterCollection.h"
#include "Engine/Texture.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"
#include "Materials/MaterialInterface.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"

namespace Cubeless::UE58::MaterialReference
{
namespace
{
using FResult = Variable::FResult;
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.ErrorCode = Code; R.Error = Error; return false; }
void Count(FResult& R, const TCHAR* Key)
{ R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
bool HasNul(const FString& Value)
{ for (int32 Index = 0; Index < Value.Len(); ++Index) { if (Value[Index] == TCHAR('\0')) { return true; } } return false; }
bool Loaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool TextInput(const FString& Value, bool Required = false, int32 Max = 2048)
{ return (!Required || !Value.IsEmpty()) && Value.Len() <= Max && !HasNul(Value); }
bool JsonDepth(const FString& Text)
{
    int32 Depth = 0; bool Quoted = false, Escaped = false;
    for (TCHAR Character : Text)
    {
        if (Quoted) { if (Escaped) { Escaped = false; } else if (Character == TCHAR('\\')) { Escaped = true; } else if (Character == TCHAR('"')) { Quoted = false; } continue; }
        if (Character == TCHAR('"')) { Quoted = true; }
        else if (Character == TCHAR('{') || Character == TCHAR('[')) { if (++Depth > 16) { return false; } }
        else if (Character == TCHAR('}') || Character == TCHAR(']')) { if (--Depth < 0) { return false; } }
    }
    return !Quoted && Depth == 0;
}

bool SameObservationValue(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, int32& Visited, int32 Depth = 0)
{
    if (++Visited > 1048576 || Depth > 64 || !A.IsValid() || !B.IsValid() || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object)
    {
        const auto Left = A->AsObject(), Right = B->AsObject(); if (!Left.IsValid() || !Right.IsValid() || Left->Values.Num() != Right->Values.Num()) { return false; }
        for (const auto& Pair : Left->Values) { if (!SameObservationValue(Pair.Value, Right->TryGetField(Pair.Key.ToView()), Visited, Depth + 1)) { return false; } } return true;
    }
    if (A->Type == EJson::Array)
    {
        const auto& Left = A->AsArray(); const auto& Right = B->AsArray(); if (Left.Num() != Right.Num()) { return false; }
        for (int32 Index = 0; Index < Left.Num(); ++Index) { if (!SameObservationValue(Left[Index], Right[Index], Visited, Depth + 1)) { return false; } } return true;
    }
    return A->Type == EJson::String ? A->AsString().Equals(B->AsString(), ESearchCase::CaseSensitive) : FJsonValue::CompareEqual(*A, *B);
}
bool SameObservation(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ int32 Visited = 0; return SameObservationValue(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B), Visited); }
// HZ Named continuation; JB also reuses this exact concrete scope for synchronous Function postprocessing.
bool DeferredRunning=false,DeferredLease=false,ReferenceCoreEntryActive=false;
struct FDeferredWork;
FDeferredWork* CurrentDeferredWork=nullptr;
bool ContinueDeferred();
void NoteDeferredTargetSDK();
bool OwnDeferredObject(UObject* Object);
bool DeferredGlobals()
{return IsInGameThread()&&GEditor&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext();}
bool DeferredLive(const UObject* Object)
{return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool DeferredNameSafe(const FString& Path)
{
 if(Path.Len()>=NAME_SIZE||HasNul(Path))return false;
 FString ObjectPath=FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd();ObjectPath.TrimQuotesInline();
 if(ObjectPath.StartsWith(TEXT("/"))&&!ObjectPath.Contains(TEXT(".")))ObjectPath+=TEXT(".")+FPackageName::GetShortName(ObjectPath);
 // Short-name resolution uses Registry candidates; each returned path is checked before its explicit load.
 return ObjectPath.Len()<NAME_SIZE;
}

FString NormalizedPath(const FString& Path)
{
    FString Out = FPackageName::ExportTextPathToObjectPath(Path).TrimStartAndEnd(); Out.TrimQuotesInline();
    if ((Out.StartsWith(TEXT("/Game/")) || Out.StartsWith(TEXT("/Engine/"))) && !Out.Contains(TEXT(".")))
    { Out += TEXT(".") + FPackageName::GetShortName(Out); }
    return Out;
}
void Init(FResult& R, bool Save)
{
    R.bSaveRequested = Save;
    for (const TCHAR* Key : { TEXT("outer_transaction_calls"), TEXT("explicit_asset_modify_calls"), TEXT("expression_modify_calls"), TEXT("reference_assignment_calls"), TEXT("parameter_id_assignment_calls"), TEXT("auto_set_sample_type_calls"), TEXT("refresh_node_calls"), TEXT("mark_package_dirty_calls"), TEXT("explicit_pre_edit_change_calls"), TEXT("explicit_post_edit_change_calls"), TEXT("material_capture_calls"), TEXT("material_capture_returns"), TEXT("auxiliary_load_calls"), TEXT("auxiliary_load_returns"), TEXT("material_update_context_calls"), TEXT("force_recompile_background_calls"), TEXT("update_material_function_calls"), TEXT("update_material_function_returns"), TEXT("material_resource_queries"), TEXT("material_compile_status_queries"), TEXT("save_attempt_count"), TEXT("save_return_count"), TEXT("set_dirty_flag_calls"), TEXT("finish_compilation_calls"), TEXT("material_editing_library_recompile_calls"), TEXT("explicit_GC_calls"), TEXT("shader_manager_pump_calls") })
    { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetBoolField(TEXT("saved"), false);
    R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"), false); R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"), false);
    for (const TCHAR* Key : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_errors"), TEXT("compile_error_count") }) { R.Data->SetField(Key, MakeShared<FJsonValueNull>()); }
    R.Data->SetBoolField(TEXT("all_implicit_work_measured"), false); R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"), false);
    R.Data->SetBoolField(TEXT("global_function_fanout_verified"), false); R.Data->SetBoolField(TEXT("runtime_or_cold_verified"), false);
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Original reference iteration and dispatch ordering on a bounded ordinary /Game Material/Function graph. Capture uses the existing public Material resolver and graph observer, which can load assets before its first observation. GraphNode refresh, PostEditChange, compiler/update-context, Function update and forced save have Engine-owned callbacks. Immediate background compilation substitutes for original RecompileMaterial/FinishCompilation; no wait, pump, GC, modal streaming helper, child/editor/viewport parity, rollback or automatic retry."));
    R.Data->SetStringField(TEXT("changed_scope"), TEXT("Selected reference writes, captured selected graph/dirty/file observations. Pending/incomplete Material compiler leaves changed/compiled null. Function target update/selected save can be known independently of shader completion; bounded loaded-interface receipts do not prove global dependent state or all implicit work."));
    R.Data->SetStringField(TEXT("counter_scope"), TEXT("Native explicit call sites only. Assignment counters are logical public-field applications, not Engine method calls. material_capture counters count shared helper attempts/returns, not its internal loads/queries; auxiliary_load counts explicit replacement/collection LoadObject. Original outer transaction began before graph resolution; this finite adapter begins it after Editor/input/graph admission."));
}
struct FContext
{
    TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
    Material::FState Selected;
    UMaterial* MaterialAsset = nullptr;
    UMaterialFunction* FunctionAsset = nullptr;
    TArray<TStrongObjectPtr<UObject>> Aux;
    TArray<TStrongObjectPtr<UMaterialExpression>> Expressions;
    bool WasDirty = false;
    FString BeforeSHA;
};
TConstArrayView<TObjectPtr<UMaterialExpression>> Expressions(const FContext& C)
{ return C.MaterialAsset ? C.MaterialAsset->GetExpressions() : C.FunctionAsset->GetExpressions(); }
bool Disk(UObject* Asset, TSharedRef<FJsonObject> Out)
{
    FString File; if (!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetOutermost()->GetName(), File, FPackageName::GetAssetPackageExtension())) { return false; }
    File = FPaths::ConvertRelativePathToFull(File); const int64 Size = IFileManager::Get().FileSize(*File);
    Out->SetStringField(TEXT("path"), File); Out->SetBoolField(TEXT("exists"), Size >= 0); Out->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); Out->SetStringField(TEXT("sha1"), TEXT(""));
    if (Size < 0) { return true; } TArray<uint8> Bytes;
    if (Size > 64 * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *File) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); Out->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Capture(FContext& C, FResult& R, const TCHAR* Key)
{
    auto Row = MakeShared<FJsonObject>(); R.Data->SetObjectField(Key, Row); Row->SetBoolField(TEXT("complete"), false);
    if(!ContinueDeferred())return false;
    Material::FState Current; FString Error; Count(R, TEXT("material_capture_calls"));
    NoteDeferredTargetSDK();
    const bool OK = Material::CaptureState(Material::EOperation::ListNodes, C.Params, 512, Current, Error); Count(R, TEXT("material_capture_returns"));
    if(!ContinueDeferred())return false;
    if (Current.Snapshot.IsValid()) { Row->SetObjectField(TEXT("graph"), Current.Snapshot.ToSharedRef()); }
    if (Loaded(Current.Asset.Get())) { Row->SetStringField(TEXT("asset_path"), Current.AssetPath); Row->SetBoolField(TEXT("dirty"), Current.Asset->GetOutermost()->IsDirty()); }
    if (!OK || !Loaded(Current.Asset.Get()))
    { R.bChangeStateKnown = !R.bMutationAttempted; return Fail(R, TEXT("material_reference_observation_incomplete"), TEXT("Current graph observation failed; available prefix retained: ") + Error); }
    if (!C.Selected.Asset.IsValid())
    { C.Selected = MoveTemp(Current); C.Params->SetStringField(TEXT("material_path"), C.Selected.AssetPath); C.MaterialAsset = Cast<UMaterial>(C.Selected.Asset.Get()); C.FunctionAsset = Cast<UMaterialFunction>(C.Selected.Asset.Get()); }
    else if (Current.Asset.Get() != C.Selected.Asset.Get() || Current.AssetPath != C.Selected.AssetPath)
    { R.bChangeStateKnown = !R.bMutationAttempted; return Fail(R, TEXT("material_reference_target_relocated"), TEXT("Target identity changed; no retry.")); }
    if(!ContinueDeferred())return false;
    auto File = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"), File);
    if (!Disk(C.Selected.Asset.Get(), File)) { R.bChangeStateKnown = !R.bMutationAttempted; return Fail(R, TEXT("material_reference_disk_incomplete"), TEXT("Bounded disk observation failed; prefix retained.")); }
    Row->SetBoolField(TEXT("complete"), true); return true;
}
bool Prepare(FContext& C, FResult& R, const FString& Path, const FString& Type, bool Save)
{
    if(!ContinueDeferred())return false;
    if(CurrentDeferredWork&&(!DeferredNameSafe(Path)||Type.Len()>=NAME_SIZE))return Fail(R,TEXT("material_reference_async_name_bound"),TEXT("Material/expanded object path must fit the public Engine name bound before resolution."));
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("material_reference_editor_state_invalid"), TEXT("Requires Editor game thread outside PIE/SIE.")); }
    if (!TextInput(Path, true) || !TextInput(Type, false, 64)) { return Fail(R, TEXT("material_reference_input_invalid"), TEXT("Bounded material path/type without interior NUL required.")); }
    C.Params->SetStringField(TEXT("material_path"), Path); C.Params->SetStringField(TEXT("graph_type"), Type);
    if (!Capture(C, R, TEXT("before_observation"))) { return false; } FString Error;
    if (!Material::ValidateMutationTarget(C.Selected, Error)) { return Fail(R, TEXT("material_reference_target_unsupported"), Error); }
    UObject* Asset = C.Selected.Asset.Get();
    if ((!C.MaterialAsset && !C.FunctionAsset) || Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || (Save && !Asset->GetOutermost()->IsFullyLoaded()))
    { return Fail(R, TEXT("material_reference_target_not_ready"), TEXT("Ordinary loaded graph and already fully loaded package for save required.")); }
    for (const auto& Ptr : Expressions(C))
    {
        UMaterialExpression* Expression = Ptr.Get();
        if (!Loaded(Expression) || Expression->GetOuter() != Asset || Expression->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient))
        { return Fail(R, TEXT("material_reference_expression_not_ready"), TEXT("Every graph expression must be an ordinary loaded direct-owned object before any writer.")); }
        C.Expressions.Emplace(Expression);
    }
    C.WasDirty = Asset->GetOutermost()->IsDirty(); C.BeforeSHA = R.Data->GetObjectField(TEXT("before_observation"))->GetObjectField(TEXT("disk"))->GetStringField(TEXT("sha1"));
    R.Data->SetStringField(TEXT("material"), Asset->GetPathName()); R.Data->SetStringField(TEXT("graph_type"), C.MaterialAsset ? TEXT("material") : TEXT("function"));
    R.Data->SetBoolField(TEXT("was_dirty_before_replace"), C.WasDirty); return true;
}
void ReplacementBaseline(FContext& C, FResult& R)
{
    // Original handlers sample this after collection/replacement asset loads.
    C.WasDirty = C.Selected.Asset->GetOutermost()->IsDirty(); R.Data->SetBoolField(TEXT("was_dirty_before_replace"), C.WasDirty);
}
UObject* LoadAux(FContext& C, FResult& R, const FString& Path)
{
    if(!ContinueDeferred()||(CurrentDeferredWork&&!DeferredNameSafe(Path)))return nullptr;
    NoteDeferredTargetSDK();Count(R, TEXT("auxiliary_load_calls")); UObject* Object = LoadObject<UObject>(nullptr, *NormalizedPath(Path)); Count(R, TEXT("auxiliary_load_returns"));
    if(!ContinueDeferred())return nullptr;
    if(Loaded(Object)){if(!OwnDeferredObject(Object))return nullptr;C.Aux.Emplace(Object);return Object;} return nullptr;
}
void Identity(TSharedRef<FJsonObject> Row, const UMaterialExpression* Expression)
{
    const FString Key = TEXT("path:") + Expression->GetPathName(); Row->SetStringField(TEXT("node_id"), Key); Row->SetStringField(TEXT("node_key"), Key);
    Row->SetStringField(TEXT("legacy_node_id"), Expression->MaterialExpressionGuid.IsValid() ? Expression->MaterialExpressionGuid.ToString() : Expression->GetName());
    Row->SetStringField(TEXT("guid"), Expression->MaterialExpressionGuid.ToString()); Row->SetStringField(TEXT("node_name"), Expression->GetName()); Row->SetStringField(TEXT("object_path"), Expression->GetPathName());
}
bool Refresh(FResult& R, UMaterialExpression* Expression)
{
#if WITH_EDITORONLY_DATA
    if(!ContinueDeferred())return false;
    if (Expression->GraphNode) { Count(R, TEXT("refresh_node_calls")); Expression->RefreshNode(true);if(!ContinueDeferred())return false; }
#endif
    return true;
}
bool ModifyAsset(FContext& C, FResult& R)
{ if(!ContinueDeferred())return false;R.bMutationAttempted = true; R.bChangeStateKnown = false; Count(R, TEXT("explicit_asset_modify_calls")); C.Selected.Asset->Modify();return ContinueDeferred(); }
bool MarkChanged(FContext& C, FResult& R)
{
    if(!ModifyAsset(C,R))return false;Count(R,TEXT("mark_package_dirty_calls"));C.Selected.Asset->MarkPackageDirty();if(!ContinueDeferred())return false;
    Count(R,TEXT("explicit_post_edit_change_calls"));C.Selected.Asset->PostEditChange();return ContinueDeferred();
}
struct FExpected
{
    UMaterialExpression* Expression = nullptr;
    UMaterialParameterCollection* Collection = nullptr;
    FGuid ParameterId;
    FName ParameterName;
    UTexture* Texture = nullptr;
    int32 SamplerType = 0;
    bool IsCollection = false;
};

struct FDeferredIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;
 FString Path;bool Retired=false;
 explicit FDeferredIdentity(UObject* Value):Hold(Value),Object(Value),Outer(Value->GetOuter()),Class(Value->GetClass()),Package(Value->GetOutermost()),Effective(Value->GetPackage()),Path(Value->GetPathName()){}
 bool Exact()const
 {
  if(Retired)return true;
  UObject* Value=Object.Get();return DeferredLive(Value)&&DeferredLive(Class.Get())&&DeferredLive(Package.Get())&&DeferredLive(Effective.Get())&&
   Value->GetClass()==Class.Get()&&Value->GetOuter()==Outer.Get()&&Value->GetOutermost()==Package.Get()&&Value->GetPackage()==Effective.Get()&&Value->GetPathName().Equals(Path,ESearchCase::CaseSensitive);
 }
};
struct FDeferredWork
{
 FResult Result;FContext Context;TSharedRef<FJsonObject> Metrics=MakeShared<FJsonObject>();
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;
 TArray<TUniquePtr<FDeferredIdentity>> Objects;
 TArray<UMaterialExpression*> Physical;TMap<UMaterialExpression*,FGuid> Guids;
 TMap<UMaterialExpressionMaterialFunctionCall*,UMaterialFunctionInterface*> FunctionRoutes;
 TMap<UMaterialFunctionInterface*,FGuid> FunctionIds;
 TArray<FExpected> Expected;
 TSharedPtr<FJsonObject> Authored;
 EShaderPlatform Platform=GMaxRHIShaderPlatform;TOptional<int32> Quality;
 bool Bound=false,Ready=false,Changed=false,CollectionOperation=false,CancelRequested=false,TargetSDKEntered=false,SaveDone=false,GuardFailed=false,FunctionTargetCompleted=false;
 FString CancelReason;
 bool PublicationSafe()const
 {
  if(!DeferredGlobals()||Editor.Get()!=GEditor)return false;
  UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();return DeferredLive(CurrentWorld)&&!CurrentWorld->bIsTearingDown&&World.Get()==CurrentWorld;
 }
 bool Error(const TCHAR* Code,const FString& Message)
 {GuardFailed=true;Result.bChangeStateKnown=false;Result.bPostconditionPassed=false;if(Context.FunctionAsset){FunctionTargetCompleted=false;Result.Data->SetBoolField(TEXT("function_target_scope_complete"),false);}return Fail(Result,Code,Message);}
 bool Own(UObject* Value)
 {
  if(!Value)return true;if(!DeferredLive(Value))return Error(TEXT("material_reference_async_unreadable"),TEXT("Captured authoring/reference object is not live."));
  for(const auto& Object:Objects)if(Object->Object.Get()==Value&&!Object->Retired)return true;
  Objects.Add(MakeUnique<FDeferredIdentity>(Value));return true;
 }
 bool BindRoot()
 {
  UObject* Root=Context.Selected.Asset.Get();if(!Root)return true;
  if(!Own(Root))return false;
  if(!Context.MaterialAsset&&!Context.FunctionAsset)return true;
  if(Bound)return true;
  for(const auto& Ptr:Expressions(Context))
  {
   UMaterialExpression* Expression=Ptr.Get();if(!Expression||!Own(Expression)||Expression->GetOuter()!=Root)return Error(TEXT("material_reference_async_expression_owner"),TEXT("Current physical expressions must be direct-owned by the exact target."));
   Physical.Add(Expression);Guids.Add(Expression,Expression->MaterialExpressionGuid);
   if(auto* Call=Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
   {UMaterialFunctionInterface* Source=Call->MaterialFunction;if(!Own(Source))return false;FunctionRoutes.Add(Call,Source);if(Source)FunctionIds.Add(Source,Source->StateId);}
  }
  Bound=true;return true;
 }
 bool Admit()
 {
  if(GuardFailed)return false;
  if(!PublicationSafe()||CancelRequested||MaterialPostprocessAsync::IsStopping())return Error(TEXT("material_reference_async_context_stopped"),TEXT("Exact Editor/world/GC/exit/cancel admission failed; no new compiler/save."));
  if(Platform!=GMaxRHIShaderPlatform)return Error(TEXT("material_reference_async_shader_platform_changed"),TEXT("Current shader platform changed during owned continuation."));
  for(const auto& Object:Objects)if(!Object->Exact())return Error(TEXT("material_reference_async_identity_changed"),TEXT("Captured class/outer/effective package/path changed."));
  if(!BindRoot())return false;
  if(!Bound)return true;
  const auto Current=Expressions(Context);if(Current.Num()!=Physical.Num())return Error(TEXT("material_reference_async_expression_membership_changed"),TEXT("Physical expression count changed across callback/tick."));
  for(int32 Index=0;Index<Physical.Num();++Index)
  {auto* Expression=Physical[Index];if(Current[Index].Get()!=Expression||Expression->MaterialExpressionGuid!=Guids.FindChecked(Expression))return Error(TEXT("material_reference_async_expression_membership_changed"),TEXT("Expression order/GUID changed across callback/tick."));}
  for(const auto& Route:FunctionRoutes)if(Route.Key->MaterialFunction!=Route.Value)return Error(TEXT("material_reference_async_function_route_changed"),TEXT("Referenced Function pointer changed."));
  for(const auto& Source:FunctionIds)if(Source.Key->StateId!=Source.Value)return Error(TEXT("material_reference_async_function_source_changed"),TEXT("Referenced Function source StateId changed."));
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
bool ContinueDeferred(){return !CurrentDeferredWork||CurrentDeferredWork->Admit();}
void NoteDeferredTargetSDK(){if(CurrentDeferredWork)CurrentDeferredWork->TargetSDKEntered=true;}
bool OwnDeferredObject(UObject* Object){return !CurrentDeferredWork||CurrentDeferredWork->Own(Object);}

bool Verify(FContext& C, FResult& R, const TArray<FExpected>& Expected)
{
    if(!ContinueDeferred())return false;
    const auto Current = Expressions(C); if (Current.Num() != C.Expressions.Num()) { return Fail(R, TEXT("material_reference_expression_set_changed"), TEXT("Expression count changed after callbacks; raw prefix retained.")); }
    for (int32 Index = 0; Index < Current.Num(); ++Index)
    { if (!Loaded(Current[Index].Get()) || Current[Index].Get() != C.Expressions[Index].Get()) { return Fail(R, TEXT("material_reference_expression_set_changed"), TEXT("Expression identity/order changed after callbacks.")); } }
    for (const auto& Row : Expected)
    {
        if (Row.IsCollection)
        { const auto* Node = Cast<UMaterialExpressionCollectionParameter>(Row.Expression); if (!Node || Node->Collection.Get() != Row.Collection || Node->ParameterId != Row.ParameterId || Node->ParameterName != Row.ParameterName) { return Fail(R, TEXT("material_reference_write_unverified"), TEXT("Collection pointer/name/actual target ID readback differs.")); } }
        else
        { const auto* Node = Cast<UMaterialExpressionTextureBase>(Row.Expression); if (!Node || Node->Texture.Get() != Row.Texture || static_cast<int32>(Node->SamplerType.GetValue()) != Row.SamplerType) { return Fail(R, TEXT("material_reference_write_unverified"), TEXT("Texture pointer/sampler readback differs.")); } }
    }
    R.Data->SetBoolField(TEXT("selected_reference_postcondition_passed"), true); return true;
}
bool ObserveCompile(FContext& C, FResult& R, const TCHAR* Key)
{
    auto Row = MakeShared<FJsonObject>(); R.Data->SetObjectField(Key, Row); Row->SetBoolField(TEXT("complete"), false);
    R.Compiled.Reset(); Row->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    Row->SetNumberField(TEXT("shader_platform"), static_cast<int32>(GMaxRHIShaderPlatform)); Row->SetBoolField(TEXT("null_rhi"), GUsingNullRHI);
    if(!ContinueDeferred())return false;
    Count(R, TEXT("material_resource_queries")); FMaterialResource* Resource = C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;
    TArray<TSharedPtr<FJsonValue>> Errors; Row->SetArrayField(TEXT("compile_errors"), Errors);
    if (!Resource)
    { R.Compiled = false; Row->SetBoolField(TEXT("resource_available"), false); Row->SetStringField(TEXT("status"), TEXT("resource_unavailable")); Errors.Add(MakeShared<FJsonValueString>(TEXT("No material resource for current shader platform."))); Row->SetArrayField(TEXT("compile_errors"), Errors); }
    else
    {
        Row->SetBoolField(TEXT("resource_available"), true); Count(R, TEXT("material_compile_status_queries"));
        const bool Finished=Resource->IsCompilationFinished();if(!ContinueDeferred())return false;
        if(CurrentDeferredWork){Count(R,TEXT("material_resource_queries"));FMaterialResource* CurrentResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentResource!=Resource) return CurrentDeferredWork->Error(TEXT("material_async_resource_replaced"),TEXT("Current resource changed during observation."));UMaterialInterface* ResourceOwner=Resource->GetMaterialInterface();if(!ContinueDeferred())return false;CurrentResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentResource!=Resource||ResourceOwner!=C.MaterialAsset)return CurrentDeferredWork->Error(TEXT("material_reference_async_resource_replaced"),TEXT("Current live owner/resource role changed after completion predicate; old resource is not dereferenced."));const int32 Quality=static_cast<int32>(Resource->GetQualityLevel());if(CurrentDeferredWork->Quality.IsSet()&&CurrentDeferredWork->Quality.GetValue()!=Quality)return CurrentDeferredWork->Error(TEXT("material_reference_async_quality_changed"),TEXT("Current default-quality resource role changed."));CurrentDeferredWork->Quality=Quality;}
        const auto CompileErrors=Resource->GetCompileErrors();int32 Chars=0;
        Row->SetBoolField(TEXT("compilation_finished"), Finished);
        for (const FString& Error : CompileErrors)
        { if (Errors.Num() >= 256 || Error.Len() > 1048576 || Chars > 1048576 - Error.Len()) { return Fail(R, TEXT("material_reference_compile_status_incomplete"), TEXT("Compile error observation bound exceeded.")); } Chars += Error.Len(); Errors.Add(MakeShared<FJsonValueString>(Error)); Row->SetArrayField(TEXT("compile_errors"), Errors); }
        const FMaterialShaderMap* Map = Resource->GetGameThreadShaderMap(); const bool Complete = Resource->IsGameThreadShaderMapComplete(), Valid = Resource->HasValidGameThreadShaderMap();
        Row->SetBoolField(TEXT("game_thread_shader_map_complete"), Complete); Row->SetBoolField(TEXT("has_valid_game_thread_shader_map"), Valid); Row->SetBoolField(TEXT("game_thread_shader_map_present"), Map != nullptr);
        if (Map) { Row->SetBoolField(TEXT("shader_map_compilation_finalized"), Map->IsCompilationFinalized()); Row->SetBoolField(TEXT("shader_map_compiled_successfully"), Map->CompiledSuccessfully()); }
        else { Row->SetField(TEXT("shader_map_compilation_finalized"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("shader_map_compiled_successfully"), MakeShared<FJsonValueNull>()); }
        const bool StrictReady=Complete&&Valid&&Map&&Map->IsCompilationFinalized()&&Map->CompiledSuccessfully();
        const bool HadErrorOrCompiling=C.MaterialAsset->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentDeferredWork){FMaterialResource* FinalResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(FinalResource!=Resource)return CurrentDeferredWork->Error(TEXT("material_async_resource_changed_during_status"),TEXT("Current resource changed during final Editor criterion observation."));}Row->SetBoolField(TEXT("original_compiling_or_had_error"),HadErrorOrCompiling);
        Row->SetBoolField(TEXT("strict_render_map_criterion"),StrictReady);Row->SetBoolField(TEXT("original_Editor_completion_criterion"),Finished&&CompileErrors.IsEmpty()&&!HadErrorOrCompiling);
        if(CurrentDeferredWork&&!Finished){R.Compiled.Reset();Row->SetStringField(TEXT("status"),TEXT("pending"));}
        else if (!CompileErrors.IsEmpty()) { R.Compiled = false; Row->SetStringField(TEXT("status"), TEXT("compile_errors")); }
        else if (Finished && !HadErrorOrCompiling && (CurrentDeferredWork||StrictReady)) { R.Compiled = true; Row->SetStringField(TEXT("status"), TEXT("current_resource_complete")); }
        else if(CurrentDeferredWork&&Finished){R.Compiled=false;Row->SetStringField(TEXT("status"),TEXT("original_Editor_criterion_failed"));}
        else { Row->SetStringField(TEXT("status"), Finished ? TEXT("resource_incomplete") : TEXT("pending")); }
    }
    if (R.Compiled.IsSet()) { Row->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); }
    Row->SetNumberField(TEXT("compile_error_count"), Errors.Num()); Row->SetBoolField(TEXT("complete"), true);
    for (const TCHAR* Field : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_errors"), TEXT("compile_error_count") }) { R.Data->SetField(Field, Row->TryGetField(Field)); }
    return true;
}

bool DeferredPackageOwned(FContext& C,FResult& R)
{
 if(!CurrentDeferredWork)return true;
 if(!ContinueDeferred())return false;
 UObject* Root=C.Selected.Asset.Get();UPackage* Package=Root->GetOutermost();TArray<UObject*> Exports;
 GetObjectsWithPackage(Package,Exports,EGetObjectsFlags::IncludeNestedObjects);if(!ContinueDeferred())return false;
 if(Exports.Num()>4096)return CurrentDeferredWork->Error(TEXT("material_reference_async_package_bound"),TEXT("Current target package export budget exceeded before/after exact save."));
 for(UObject* Export:Exports)if(!DeferredLive(Export)||Export->GetOutermost()!=Package||Export->GetPackage()!=Package||(Export!=Root&&!Export->IsIn(Root)))
  return CurrentDeferredWork->Error(TEXT("material_reference_async_foreign_package_export"),TEXT("Only the exact Material/Function root and current subtree may be saved; actual prior save prefix is retained."));
 return true;
}

// IC policy reused for Reference Function targets: this is a bounded loaded-interface
// receipt, never a dependency closure, shader status or SDK-global preservation claim.
bool ReferenceFunctionFanout(FResult& R,const TCHAR* Name,bool HoldBefore)
{
    if(!ContinueDeferred()||!CurrentDeferredWork)return false;
    auto& W=*CurrentDeferredWork;auto J=MakeShared<FJsonObject>();R.Data->SetObjectField(Name,J);
    J->SetStringField(TEXT("scope"),TEXT("Bounded point sample of loaded Material/Function interfaces. Existing interfaces are strongly held before UpdateMaterialFunction; identity holds do not imply full authored/cache preservation, dependency closure or completion."));
    J->SetBoolField(TEXT("dependency_closure_verified"),false);J->SetBoolField(TEXT("shader_completion_observed"),false);
    J->SetBoolField(TEXT("inventory_scan_complete"),false);
    TArray<TSharedPtr<FJsonValue>> Rows;int32 Seen=0,Chars=0;bool Truncated=false;
    auto Add=[&](UObject* O,UMaterialFunctionInterface* Function)->bool
    {
        if(++Seen>8192)return W.Error(TEXT("material_reference_function_fanout_budget"),TEXT("Loaded-interface inventory exceeds finite8192 bound; actual prefix retained, no further target work."));
        if(!DeferredLive(O)||!DeferredLive(O->GetClass())||!DeferredLive(O->GetOutermost()))
            return W.Error(TEXT("material_reference_function_fanout_unreadable"),TEXT("Current loaded interface is not readable; actual prefix retained."));
        if(HoldBefore&&!W.Own(O))return false;
        const FString Path=O->GetPathName(),ClassPath=O->GetClass()->GetPathName();
        if(Rows.Num()>=512||Path.Len()>2048||ClassPath.Len()>2048||Chars+Path.Len()+ClassPath.Len()>131072){Truncated=true;return true;}
        Chars+=Path.Len()+ClassPath.Len();auto Row=MakeShared<FJsonObject>();
        Row->SetStringField(TEXT("path"),Path);Row->SetStringField(TEXT("class"),ClassPath);Row->SetBoolField(TEXT("dirty"),O->GetOutermost()->IsDirty());
        if(Function)Row->SetStringField(TEXT("StateId"),Function->StateId.ToString(EGuidFormats::DigitsWithHyphens));
        Rows.Add(MakeShared<FJsonValueObject>(Row));return true;
    };
    for(TObjectIterator<UMaterialFunctionInterface> It(RF_ClassDefaultObject,true,EInternalObjectFlags::Garbage);It;++It)if(!Add(*It,*It))return false;
    for(TObjectIterator<UMaterialInterface> It(RF_ClassDefaultObject,true,EInternalObjectFlags::Garbage);It;++It)if(!Add(*It,nullptr))return false;
    J->SetArrayField(TEXT("interfaces"),Rows);J->SetNumberField(TEXT("observed_interface_count"),Seen);
    J->SetNumberField(TEXT("returned_row_count"),Rows.Num());J->SetBoolField(TEXT("rows_truncated"),Truncated);J->SetBoolField(TEXT("inventory_scan_complete"),true);
    return ContinueDeferred();
}
bool VerifyReferenceFunction(FContext& C,FResult& R,const TCHAR* Name)
{
    if(!ContinueDeferred()||!CurrentDeferredWork)return false;
    if(!Capture(C,R,Name)||!Verify(C,R,CurrentDeferredWork->Expected))return false;
    const auto Point=R.Data->GetObjectField(Name);
    if(!CurrentDeferredWork->Authored.IsValid()||!SameObservation(CurrentDeferredWork->Authored.ToSharedRef(),Point->GetObjectField(TEXT("graph")).ToSharedRef()))
        return CurrentDeferredWork->Error(TEXT("material_reference_function_authored_graph_changed"),TEXT("Post-replacement authored graph changed during Function update/save; actual prefix retained without rollback/replay."));
    return ContinueDeferred();
}
bool PrepareReferenceFunctionUpdate(FContext& C,FResult& R,const TArray<FExpected>& Expected)
{
    if(!ContinueDeferred()||!CurrentDeferredWork)return false;
    for(const auto& State:Expected)if(!CurrentDeferredWork->Own(State.Collection)||!CurrentDeferredWork->Own(State.Texture))return false;
    CurrentDeferredWork->Expected=Expected;
    if(!Capture(C,R,TEXT("before_function_update_observation"))||!Verify(C,R,Expected))return false;
    CurrentDeferredWork->Authored=R.Data->GetObjectField(TEXT("before_function_update_observation"))->GetObjectField(TEXT("graph"));
    return ReferenceFunctionFanout(R,TEXT("before_function_fanout_inventory"),true);
}
bool CompleteReferenceFunction(FContext& C,FResult& R,bool Changed,bool CollectionOperation)
{
    R.Compiled.Reset();auto Defaults=MakeShared<FJsonObject>();
    Defaults->SetBoolField(TEXT("compiled"),true);Defaults->SetBoolField(TEXT("compilation_finished"),true);
    Defaults->SetNumberField(TEXT("compile_error_count"),0);Defaults->SetArrayField(TEXT("compile_errors"),{});
    Defaults->SetStringField(TEXT("scope"),TEXT("Original Function response defaults, not inspected shader completion. Typed compiled remains null."));
    R.Data->SetObjectField(TEXT("original_unverified_function_defaults"),Defaults);
    for(const auto& P:Defaults->Values)if(!FString(P.Key.ToView()).Equals(TEXT("scope")))R.Data->SetField(P.Key,P.Value);
    R.Data->SetBoolField(TEXT("function_shader_completion_observed"),false);
    R.Data->SetStringField(TEXT("function_completion_criterion"),TEXT("function_update_and_selected_save_returned"));
    R.Data->SetBoolField(TEXT("global_function_fanout_verified"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
    R.Data->SetStringField(TEXT("direct_compile_status_scope"),TEXT("Original Function defaults are preserved in data; typed compiled is null. Target completion means returned UpdateMaterialFunction and selected changed-only save, not shader/global fanout completion."));
    if(!VerifyReferenceFunction(C,R,TEXT("after_function_update_observation")))return false;
    if(R.bSaveRequested&&Changed)
    {
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;
        if(CurrentDeferredWork->SaveDone)return CurrentDeferredWork->Error(TEXT("material_reference_async_save_replay"),TEXT("Save was already entered; no second save."));
        // Original Texture handler marks changed again only for its selected save;
        // Collection already marked changed before its changed-only Function update.
        if(!CollectionOperation){R.Stage=TEXT("prepare_function_save");if(!MarkChanged(C,R))return false;}
        if(!VerifyReferenceFunction(C,R,TEXT("before_function_save_observation"))||!DeferredPackageOwned(C,R))return false;
        R.Stage=TEXT("save_function");R.bSaveAttempted=true;R.bChangeStateKnown=false;Count(R,TEXT("save_attempt_count"));
        auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),C.Selected.AssetPath);
        Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);Call->SetBoolField(TEXT("only_if_dirty"),false);
        R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
        CurrentDeferredWork->SaveDone=true;
        const bool SavedNow=UEditorAssetLibrary::SaveLoadedAsset(C.Selected.Asset.Get(),false);
        R.bSaved|=SavedNow;Count(R,TEXT("save_return_count"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),SavedNow);R.Data->SetBoolField(TEXT("saved"),R.bSaved);
        // Adopt actual save truth before any post-callback identity/graph/disk getter.
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R)||!VerifyReferenceFunction(C,R,TEXT("after_function_save_observation")))return false;
        const auto File=R.Data->GetObjectField(TEXT("after_function_save_observation"))->GetObjectField(TEXT("disk"));
        const bool DiskOK=R.bSaved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!C.Selected.Asset->GetOutermost()->IsDirty();
        R.Data->SetBoolField(TEXT("save_disk_verified"),DiskOK);
        R.bChanged=Changed||C.BeforeSHA!=File->GetStringField(TEXT("sha1"))||C.WasDirty!=C.Selected.Asset->GetOutermost()->IsDirty();
        if(!DiskOK){R.bChangeStateKnown=true;return Fail(R,TEXT("material_reference_function_save_unverified"),TEXT("Returned save/nonempty disk/clean Function target criterion failed; actual saved prefix retained without retry."));}
    }
    if(!ReferenceFunctionFanout(R,TEXT("after_function_fanout_inventory"),false)||!VerifyReferenceFunction(C,R,TEXT("final_function_target_observation")))return false;
    R.Data->SetBoolField(TEXT("function_target_scope_complete"),true);
    R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Known selected reference values, physical authored graph, identity, dirty/disk observations and bounded loaded-interface receipt. Global dependency fanout, editor/render/cache preservation and shader completion remain unverified."));
    R.Data->SetBoolField(TEXT("dirty_after_replace"),C.Selected.Asset->GetOutermost()->IsDirty());
    R.bChanged|=Changed||C.WasDirty!=C.Selected.Asset->GetOutermost()->IsDirty();
    R.bChangeStateKnown=true;R.bPostconditionPassed=true;R.bSuccess=true;R.Stage=TEXT("complete");CurrentDeferredWork->FunctionTargetCompleted=true;return true;
}
bool ReferenceFunctionTransactionFence(FContext& C,FResult& R)
{
    if(!R.bSuccess||!CurrentDeferredWork||!CurrentDeferredWork->FunctionTargetCompleted)return true;
    const auto Before=R.Data->GetObjectField(TEXT("final_function_target_observation"));
    const bool OK=VerifyReferenceFunction(C,R,TEXT("after_function_transaction_observation"));
    if(!OK){R.bPostconditionPassed=false;CurrentDeferredWork->FunctionTargetCompleted=false;R.Data->SetBoolField(TEXT("function_target_scope_complete"),false);return false;}
    if(!SameObservation(Before.ToSharedRef(),R.Data->GetObjectField(TEXT("after_function_transaction_observation")).ToSharedRef()))
    {
        R.Data->SetBoolField(TEXT("function_target_scope_complete"),false);
        return CurrentDeferredWork->Error(TEXT("material_reference_function_transaction_changed"),TEXT("Selected Function observation changed during transaction finalization; actual replacement/update/save prefix retained, no replay."));
    }
    R.Data->SetBoolField(TEXT("transaction_function_target_fence_passed"),true);return true;
}

bool CompleteReferenceSave(FContext& C,FResult& R,const TArray<FExpected>& Expected,bool Changed,bool CollectionOperation)
{
    if (R.bSaveRequested && Changed)
    {
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;
        if(CurrentDeferredWork&&CurrentDeferredWork->SaveDone)return CurrentDeferredWork->Error(TEXT("material_reference_async_save_replay"),TEXT("Save SDK was already entered; no second save."));
        R.Compiled.Reset();
        for (const TCHAR* Key : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_errors"), TEXT("compile_error_count") }) { R.Data->SetField(Key, MakeShared<FJsonValueNull>()); }
        R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("Final status unobserved after save preparation; initial_compile_status is immutable prefix."));
        if (!CollectionOperation) { R.Stage = TEXT("prepare_save"); if(!MarkChanged(C,R))return false; }
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;
        R.Stage = TEXT("save"); R.bSaveAttempted = true; R.bChangeStateKnown = false; Count(R, TEXT("save_attempt_count")); auto Call = MakeShared<FJsonObject>();
        Call->SetStringField(TEXT("asset_path"), C.Selected.AssetPath); Call->SetBoolField(TEXT("attempted"), true); Call->SetBoolField(TEXT("returned"), false); Call->SetBoolField(TEXT("only_if_dirty"), false);
        R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Call) });
        if(CurrentDeferredWork)CurrentDeferredWork->SaveDone=true;const bool SavedNow=UEditorAssetLibrary::SaveLoadedAsset(C.Selected.Asset.Get(),false);R.bSaved|=SavedNow;Count(R,TEXT("save_return_count"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),SavedNow);R.Data->SetBoolField(TEXT("saved"),R.bSaved);if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;R.Data->SetBoolField(TEXT("dirty_after_replace"),C.Selected.Asset->GetOutermost()->IsDirty());
        R.Stage = TEXT("observe_save"); if (!Capture(C, R, TEXT("after_save_observation")) || !Verify(C, R, Expected)) { return false; }
        const auto File = R.Data->GetObjectField(TEXT("after_save_observation"))->GetObjectField(TEXT("disk"));
        const bool DiskOK = R.bSaved && File->GetBoolField(TEXT("exists")) && File->GetNumberField(TEXT("bytes")) > 0 && !C.Selected.Asset->GetOutermost()->IsDirty(); R.Data->SetBoolField(TEXT("save_disk_verified"), DiskOK);
        if (!ObserveCompile(C, R, TEXT("final_compile_status"))) { return false; }
        if(!R.Compiled.IsSet()&&CurrentDeferredWork){R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Stage=TEXT("await_resource_after_saved_true");return true;}
        if (!R.Compiled.IsSet()) { return Fail(R, TEXT("material_reference_pending_after_save"), TEXT("Real save prefix retained, final resource pending/incomplete; no save or compile retry.")); }
        R.bChangeStateKnown = true; R.bChanged = Changed || C.BeforeSHA != File->GetStringField(TEXT("sha1")) || C.WasDirty != C.Selected.Asset->GetOutermost()->IsDirty();
        if (!DiskOK || !R.Compiled.GetValue()) { return Fail(R, TEXT("material_reference_save_or_final_compile_failed"), TEXT("Returned save/disk/current compile criterion failed; actual saved prefix retained.")); }
    }
    R.bChangeStateKnown = true; R.bChanged = Changed || C.WasDirty != C.Selected.Asset->GetOutermost()->IsDirty(); R.bPostconditionPassed = true; R.bSuccess = true; R.Stage = TEXT("complete"); return true;
}

bool Finish(FContext& C, FResult& R, const TArray<FExpected>& Expected, bool Changed, bool CollectionOperation)
{
    R.Data->SetBoolField(TEXT("reference_values_changed"), Changed); R.bCompileRequested = Changed || !CollectionOperation;
    if (Changed && CollectionOperation) { R.Stage = TEXT("mark_changed"); if(!MarkChanged(C,R))return false; }
    if (R.bCompileRequested)
    {
        R.Stage = TEXT("request_compile"); R.bMutationAttempted = true; R.bChangeStateKnown = false; R.bCompileAttempted = true;
        if (C.MaterialAsset)
        {
            { Count(R, TEXT("material_update_context_calls")); FMaterialUpdateContext Update;
              Update.AddMaterial(C.MaterialAsset);if(!ContinueDeferred())return false;Count(R,TEXT("explicit_pre_edit_change_calls"));C.Selected.Asset->PreEditChange(nullptr);if(!ContinueDeferred())return false;
              Count(R,TEXT("explicit_post_edit_change_calls"));C.Selected.Asset->PostEditChange();if(!ContinueDeferred())return false;
              Count(R,TEXT("force_recompile_background_calls"));C.MaterialAsset->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Background);if(!ContinueDeferred())return false; }
            if(!ContinueDeferred())return false;
            Count(R,TEXT("mark_package_dirty_calls"));C.Selected.Asset->MarkPackageDirty();if(!ContinueDeferred())return false;
        }
        else { if(!PrepareReferenceFunctionUpdate(C,R,Expected))return false;Count(R, TEXT("update_material_function_calls")); UMaterialEditingLibrary::UpdateMaterialFunction(C.FunctionAsset, nullptr); Count(R,TEXT("update_material_function_returns"));if(!ContinueDeferred())return false; }
    }
    if (!Changed && !C.WasDirty && C.Selected.Asset->GetOutermost()->IsDirty())
    { R.bMutationAttempted = true; R.bChangeStateKnown = false; Count(R, TEXT("set_dirty_flag_calls")); C.Selected.Asset->GetOutermost()->SetDirtyFlag(false);if(!ContinueDeferred())return false; }
    R.Stage = TEXT("observe_replacement"); if (!Capture(C, R, TEXT("after_replace_observation")) || !Verify(C, R, Expected)) { return false; }
    R.Data->SetBoolField(TEXT("dirty_after_replace"), C.Selected.Asset->GetOutermost()->IsDirty());
    if (R.bCompileRequested)
    {
        if (C.FunctionAsset)
        { return CompleteReferenceFunction(C,R,Changed,CollectionOperation); }
        if(CurrentDeferredWork){for(const auto& State:Expected)if(!CurrentDeferredWork->Own(State.Collection)||!CurrentDeferredWork->Own(State.Texture))return false;CurrentDeferredWork->Expected=Expected;CurrentDeferredWork->Changed=Changed;CurrentDeferredWork->CollectionOperation=CollectionOperation;CurrentDeferredWork->Authored=R.Data->GetObjectField(TEXT("after_replace_observation"))->GetObjectField(TEXT("graph"));CurrentDeferredWork->Ready=true;R.Stage=TEXT("await_current_resource");return true;}
        R.Stage = TEXT("observe_compile"); if (!ObserveCompile(C, R, TEXT("initial_compile_status"))) { return false; }
        if (!R.Compiled.IsSet()) { R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"), R.bSaveRequested && Changed); return Fail(R, TEXT("material_reference_compile_pending_or_incomplete"), TEXT("Current resource is pending/incomplete; compile dispatch and replacement prefix retained, save suppressed, no blocking finish/pump/retry.")); }
        if (!R.Compiled.GetValue())
        { R.bChangeStateKnown = true; R.bChanged = Changed || C.WasDirty != C.Selected.Asset->GetOutermost()->IsDirty(); R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"), R.bSaveRequested && Changed); return Fail(R, TEXT("material_reference_compile_failed"), TEXT("Current compile resource/error criterion failed; actual replacement prefix retained, no save.")); }
    }
    else
    { R.Data->SetBoolField(TEXT("compiled"), true); R.Data->SetBoolField(TEXT("compilation_finished"), true); R.Data->SetNumberField(TEXT("compile_error_count"), 0); R.Data->SetArrayField(TEXT("compile_errors"), {}); R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("Original unchanged collection handler defaults; no compile requested. Typed compiled remains null, current resource completion not measured.")); }
    return CompleteReferenceSave(C,R,Expected,Changed,CollectionOperation);
}
}

Variable::FResult ReplaceCollections(const FString& MaterialPath, const FString& TargetPath,
    const FString& SourcePath, const FString& GraphType, bool Save)
{
    if(ReferenceCoreEntryActive||((DeferredLease||DeferredRunning)&&!CurrentDeferredWork)){FResult Denied;Denied.bSaveRequested=Save;Fail(Denied,TEXT("material_reference_async_lease_active"),TEXT("This reference core is owned by an in-flight request; no synchronous replay."));return Denied;}TGuardValue<bool> RunningScope(DeferredRunning,true);TGuardValue<bool> CoreEntryScope(ReferenceCoreEntryActive,true);
    FDeferredWork LocalFunctionWork;FResult& R=CurrentDeferredWork?CurrentDeferredWork->Result:LocalFunctionWork.Result;FContext& C=CurrentDeferredWork?CurrentDeferredWork->Context:LocalFunctionWork.Context;Init(R,Save);
    if (!TextInput(TargetPath, true) || !TextInput(SourcePath)) { Fail(R, TEXT("material_reference_collection_input_invalid"), TEXT("Bounded collection paths without interior NUL required.")); return R; }
    if (!Prepare(C, R, MaterialPath, GraphType, Save)) { return R; }
    if(C.FunctionAsset&&!CurrentDeferredWork)
    {
        if(!DeferredGlobals()){Fail(R,TEXT("material_reference_function_context_invalid"),TEXT("Exact Editor admission failed before Function work."));return R;}
        LocalFunctionWork.Editor=GEditor;LocalFunctionWork.World=GEditor->GetEditorWorldContext().World();
    }
    TGuardValue<FDeferredWork*> FunctionScope(CurrentDeferredWork,CurrentDeferredWork?CurrentDeferredWork:(C.FunctionAsset?&LocalFunctionWork:nullptr));
    if(!ContinueDeferred())return R;
    Count(R, TEXT("outer_transaction_calls")); auto Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP", "ReplaceCollectionReferences", "Replace Material Collection References"));
    UMaterialParameterCollection* Target = Cast<UMaterialParameterCollection>(LoadAux(C, R, TargetPath));
    if (!Target || Target->GetClass() != UMaterialParameterCollection::StaticClass() || Target->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || Target->ScalarParameters.Num() + Target->VectorParameters.Num() > 4096)
    { Fail(R, TEXT("material_reference_target_collection_invalid"), TEXT("Loaded ordinary target collection with at most 4096 parameters required.")); return R; }
    UMaterialParameterCollection* Source = nullptr;
    if (!SourcePath.TrimStartAndEnd().IsEmpty())
    { Source = Cast<UMaterialParameterCollection>(LoadAux(C, R, SourcePath)); if (!Source || Source->GetClass() != UMaterialParameterCollection::StaticClass() || Source->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient)) { Fail(R, TEXT("material_reference_source_collection_not_found"), TEXT("Ordinary native source Material Parameter Collection could not be loaded.")); return R; } }
    R.Data->SetStringField(TEXT("source_collection"), Source ? Source->GetPathName() : FString()); R.Data->SetStringField(TEXT("target_collection"), Target->GetPathName());
    ReplacementBaseline(C, R);
    TArray<TSharedPtr<FJsonValue>> Replaced, Skipped; TArray<FExpected> Expected; int32 Nodes = 0;
    R.Data->SetArrayField(TEXT("replaced_nodes"), Replaced); R.Data->SetArrayField(TEXT("skipped_nodes"), Skipped);
    R.Data->SetNumberField(TEXT("collection_node_count"), 0); R.Data->SetNumberField(TEXT("replaced_node_count"), 0); R.Data->SetNumberField(TEXT("skipped_node_count"), 0);
    for (const auto& Ptr : C.Expressions)
    {
        auto* Node = Cast<UMaterialExpressionCollectionParameter>(Ptr.Get()); if (!Node) { continue; } ++Nodes; R.Data->SetNumberField(TEXT("collection_node_count"), Nodes);
        FExpected State; State.Expression = Node; State.Collection = Node->Collection.Get(); State.ParameterId = Node->ParameterId; State.ParameterName = Node->ParameterName; State.IsCollection = true;
        auto Row = MakeShared<FJsonObject>(); Identity(Row, Node); FString Reason, Type; FGuid Id; bool Found = false;
        if (Source && Node->Collection.Get() != Source) { Reason = TEXT("source_collection_mismatch"); Row->SetStringField(TEXT("collection"), Node->Collection ? Node->Collection->GetPathName() : FString()); }
        else
        {
            for (const auto& Parameter : Target->ScalarParameters) { if (Parameter.ParameterName == Node->ParameterName) { Id = Parameter.Id; Type = TEXT("scalar"); Found = true; break; } }
            if (!Found) { for (const auto& Parameter : Target->VectorParameters) { if (Parameter.ParameterName == Node->ParameterName) { Id = Parameter.Id; Type = TEXT("vector"); Found = true; break; } } }
            if (!Found) { Reason = TEXT("target_collection_missing_parameter"); Row->SetStringField(TEXT("parameter_name"), Node->ParameterName.ToString()); }
            else if (Node->Collection.Get() == Target && Node->ParameterId == Id) { Reason = TEXT("already_target_collection"); }
        }
        if (!Reason.IsEmpty())
        { Row->SetStringField(TEXT("reason"), Reason); Skipped.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("skipped_nodes"), Skipped); R.Data->SetNumberField(TEXT("skipped_node_count"), Skipped.Num()); Expected.Add(State); continue; }
        Row->SetStringField(TEXT("old_collection"), Node->Collection ? Node->Collection->GetPathName() : FString()); Row->SetStringField(TEXT("old_parameter_id"), Node->ParameterId.ToString(EGuidFormats::DigitsWithHyphens));
        if(Replaced.IsEmpty()&&!ModifyAsset(C,R))return R;Count(R,TEXT("expression_modify_calls"));Node->Modify();if(!ContinueDeferred())return R;
        Count(R, TEXT("reference_assignment_calls")); Node->Collection = Target; Count(R, TEXT("parameter_id_assignment_calls")); Node->ParameterId = Id;
        State.Collection = Target; State.ParameterId = Id; Expected.Add(State);
        Row->SetStringField(TEXT("parameter_name"), Node->ParameterName.ToString()); Row->SetStringField(TEXT("parameter_type"), Type); Row->SetStringField(TEXT("new_collection"), Target->GetPathName()); Row->SetStringField(TEXT("new_parameter_id"), Id.ToString(EGuidFormats::DigitsWithHyphens));
        Replaced.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("replaced_nodes"), Replaced); R.Data->SetNumberField(TEXT("replaced_node_count"), Replaced.Num()); if(!Refresh(R,Node))return R;
    }
    Finish(C, R, Expected, !Replaced.IsEmpty(), true);
    if(C.FunctionAsset){Transaction.Reset();ReferenceFunctionTransactionFence(C,R);}
    return R;
}

Variable::FResult ReplaceTextures(const FString& MaterialPath, const FString& Json, const FString& GraphType, bool Save)
{
    if(ReferenceCoreEntryActive||((DeferredLease||DeferredRunning)&&!CurrentDeferredWork)){FResult Denied;Denied.bSaveRequested=Save;Fail(Denied,TEXT("material_reference_async_lease_active"),TEXT("This reference core is owned by an in-flight request; no synchronous replay."));return Denied;}TGuardValue<bool> RunningScope(DeferredRunning,true);TGuardValue<bool> CoreEntryScope(ReferenceCoreEntryActive,true);
    FDeferredWork LocalFunctionWork;FResult& R=CurrentDeferredWork?CurrentDeferredWork->Result:LocalFunctionWork.Result;FContext& C=CurrentDeferredWork?CurrentDeferredWork->Context:LocalFunctionWork.Context;Init(R,Save);TSharedPtr<FJsonObject> Input;
    if (Json.IsEmpty() || Json.Len() > 262144 || HasNul(Json) || !JsonDepth(Json) || !FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Json), Input) || !Input.IsValid() || Input->Values.Num() > 512)
    { Fail(R, TEXT("material_reference_replacements_invalid"), TEXT("Bounded JSON replacements object required.")); return R; }
    if (!Prepare(C, R, MaterialPath, GraphType, Save)) { return R; }
    if(C.FunctionAsset&&!CurrentDeferredWork)
    {
        if(!DeferredGlobals()){Fail(R,TEXT("material_reference_function_context_invalid"),TEXT("Exact Editor admission failed before Function work."));return R;}
        LocalFunctionWork.Editor=GEditor;LocalFunctionWork.World=GEditor->GetEditorWorldContext().World();
    }
    TGuardValue<FDeferredWork*> FunctionScope(CurrentDeferredWork,CurrentDeferredWork?CurrentDeferredWork:(C.FunctionAsset?&LocalFunctionWork:nullptr));
    if(!ContinueDeferred())return R;
    Count(R, TEXT("outer_transaction_calls")); auto Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP", "ReplaceTextureReferences", "Replace Material Texture References"));
    TMap<FString, UTexture*> Lookup; TArray<TSharedPtr<FJsonValue>> Pairs, Invalid, Replaced; TSet<FString> Unmatched; TArray<FExpected> Expected;
    R.Data->SetArrayField(TEXT("replacement_pairs"), Pairs); R.Data->SetArrayField(TEXT("invalid_replacements"), Invalid); R.Data->SetArrayField(TEXT("replaced_nodes"), Replaced);
    for (const TCHAR* Key : { TEXT("replacement_pair_count"), TEXT("invalid_replacement_count"), TEXT("texture_expression_count"), TEXT("replaced_texture_reference_count"), TEXT("unmatched_texture_count") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("unmatched_textures"), {});
    for (const auto& Pair : Input->Values)
    {
        const FString Source(Pair.Key.ToView()); auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("source"), Source); FString Replacement;
        if (!Pair.Value.IsValid() || Pair.Value->Type != EJson::String)
        { Row->SetStringField(TEXT("reason"), TEXT("replacement value must be a texture asset path string")); }
        else
        {
            Replacement = Pair.Value->AsString(); Row->SetStringField(TEXT("replacement"), Replacement);
            UTexture* Texture = TextInput(Source) && TextInput(Replacement, true) ? Cast<UTexture>(LoadAux(C, R, Replacement)) : nullptr;if(!ContinueDeferred())return R;
            if (Texture && (Texture->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || Texture->GetClass()->GetOutermost()->GetName() != TEXT("/Script/Engine"))) { Texture = nullptr; }
            if (!Texture) { Row->SetStringField(TEXT("reason"), TEXT("replacement texture could not be loaded")); }
            else
            {
                FString ObjectPath = FPackageName::ExportTextPathToObjectPath(Source).TrimStartAndEnd(); TArray<FString> Keys = { Source, ObjectPath, NormalizedPath(Source), NormalizedPath(ObjectPath) };
                FString Package, Object; if (ObjectPath.Split(TEXT("."), &Package, &Object, ESearchCase::CaseSensitive, ESearchDir::FromEnd)) { Keys.Add(Package); Keys.Add(NormalizedPath(Package)); }
                for (FString Key : Keys) { Key.TrimQuotesInline(); Key = Key.TrimStartAndEnd(); if (!Key.IsEmpty()) { Lookup.Add(Key, Texture); } }
                Row->SetStringField(TEXT("replacement"), Texture->GetPathName()); Pairs.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("replacement_pairs"), Pairs); R.Data->SetNumberField(TEXT("replacement_pair_count"), Pairs.Num()); continue;
            }
        }
        Invalid.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("invalid_replacements"), Invalid); R.Data->SetNumberField(TEXT("invalid_replacement_count"), Invalid.Num());
    }
    if (Lookup.IsEmpty()) { Fail(R, TEXT("material_reference_no_valid_replacements"), TEXT("No valid texture replacements were provided")); return R; }
    ReplacementBaseline(C, R);
    int32 Nodes = 0;
    for (const auto& Ptr : C.Expressions)
    {
        auto* Node = Cast<UMaterialExpressionTextureBase>(Ptr.Get()); if (!Node || !Node->Texture) { continue; } ++Nodes; R.Data->SetNumberField(TEXT("texture_expression_count"), Nodes);
        FExpected State; State.Expression = Node; State.Texture = Node->Texture.Get(); State.SamplerType = static_cast<int32>(Node->SamplerType.GetValue()); UTexture* Replacement = nullptr;
        for (FString Key : { Node->Texture->GetPathName(), Node->Texture->GetOutermost()->GetName(), NormalizedPath(Node->Texture->GetPathName()) })
        { Key.TrimQuotesInline(); Key = Key.TrimStartAndEnd(); if (UTexture** Found = Lookup.Find(Key)) { Replacement = *Found; break; } }
        if (!Replacement)
        { Unmatched.Add(Node->Texture->GetPathName()); TArray<TSharedPtr<FJsonValue>> Rows; for (const FString& Path : Unmatched) { Rows.Add(MakeShared<FJsonValueString>(Path)); } R.Data->SetArrayField(TEXT("unmatched_textures"), Rows); R.Data->SetNumberField(TEXT("unmatched_texture_count"), Unmatched.Num()); Expected.Add(State); continue; }
        if (Node->Texture.Get() == Replacement) { Expected.Add(State); continue; }
        auto Row = MakeShared<FJsonObject>(); Identity(Row, Node); Row->SetStringField(TEXT("old_texture"), Node->Texture->GetPathName()); Row->SetStringField(TEXT("new_texture"), Replacement->GetPathName());
        if(Replaced.IsEmpty()&&!ModifyAsset(C,R))return R;Count(R,TEXT("expression_modify_calls"));Node->Modify();if(!ContinueDeferred())return R;Count(R,TEXT("reference_assignment_calls"));Node->Texture=Replacement;
        Replaced.Add(MakeShared<FJsonValueObject>(Row)); R.Data->SetArrayField(TEXT("replaced_nodes"), Replaced); R.Data->SetNumberField(TEXT("replaced_texture_reference_count"), Replaced.Num());
        Count(R, TEXT("auto_set_sample_type_calls")); Node->AutoSetSampleType();if(!ContinueDeferred())return R;State.Texture = Replacement; State.SamplerType = static_cast<int32>(Node->SamplerType.GetValue()); Expected.Add(State);if(!Refresh(R,Node))return R;
    }
    Finish(C, R, Expected, !Replaced.IsEmpty(), false);
    if(C.FunctionAsset){Transaction.Reset();ReferenceFunctionTransactionFence(C,R);}
    return R;
}

namespace
{

bool ParseReferenceJobParams(const FString& Text,TSharedPtr<FJsonObject>& Out,int32 FieldCount)
{
 if(Text.IsEmpty()||Text.Len()>2*262144+16384||HasNul(Text))return false;
 bool String=false,Escape=false;int32 Depth=0;
 for(TCHAR Ch:Text){if(String){if(Escape)Escape=false;else if(Ch=='\\')Escape=true;else if(Ch=='"')String=false;}else if(Ch=='"')String=true;else if(Ch=='{'||Ch=='['){if(++Depth>32)return false;}else if(Ch=='}'||Ch==']'){if(--Depth<0)return false;}}
 if(String||Depth)return false;auto Reader=TJsonReaderFactory<>::Create(Text);EJsonNotation Token;int32 Slots=0;
 while(Reader->ReadNext(Token))if(Token==EJsonNotation::Error||++Slots>4096)return false;
 return FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Out)&&Out.IsValid()&&Out->Values.Num()==FieldCount;
}

enum class EReferencePhase:uint8 {Dispatch,WaitInitial,Save,WaitSaved,Final,Terminal,Quarantined};
struct FReferenceJobState:public TSharedFromThis<FReferenceJobState>
{
 FDeferredWork Work;TStrongObjectPtr<UCubelessMaterialPostprocessJob> Job{nullptr};TUniquePtr<FDeferredIdentity> JobIdentity;
 UCubelessMaterialPostprocessJob* RawJob=nullptr;FGuid Id=FGuid::NewGuid();FString LastSnapshot,MaterialPath,GraphType=TEXT("auto"),TargetPath,SourcePath,ReplacementsJson;
 bool Collections=true,Save=true,Executing=false,Dispatched=false,FinalEncoded=false,FrameMetricsMerged=false;Material::FReadMetricsScope* FrameMetrics=nullptr;
 int64 Revision=0;uint64 LastFrame=MAX_uint64;EReferencePhase Phase=EReferencePhase::Dispatch;FTSTicker::FDelegateHandle Ticker;
 const TCHAR* Operation()const{return Collections?TEXT("replace_material_collection_references"):TEXT("replace_material_texture_references");}
 bool PublicationSafe()const{return Work.PublicationSafe()&&(!JobIdentity||JobIdentity->Exact());}
 void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
 void Metadata(bool Terminal)
 {
  if(FrameMetrics&&!FrameMetricsMerged){Work.MergeMetrics(FrameMetrics->ToJson());FrameMetricsMerged=true;}
  auto& R=Work.Result;R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::DigitsWithHyphens));R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  R.Data->SetBoolField(TEXT("native_job_terminal"),Terminal);R.Data->SetBoolField(TEXT("operation_complete"),Terminal);
  R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("do_not_retry"),true);
  R.Data->SetBoolField(TEXT("async_core_dispatched"),Dispatched);R.Data->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  double Assignments=0;R.Data->TryGetNumberField(TEXT("reference_assignment_calls"),Assignments);const bool Persistent=R.bCompileAttempted||R.bSaveAttempted||Assignments>0;
  R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Persistent);R.Data->SetBoolField(TEXT("transient_creation_only"),!Persistent);
  R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false);R.Data->SetBoolField(TEXT("current_resource_completion_continuation"),Work.Context.MaterialAsset!=nullptr);R.Data->SetBoolField(TEXT("function_target_update_continuation"),Work.Context.FunctionAsset!=nullptr);
  R.Data->SetStringField(TEXT("continuation_scope"),TEXT("Same reference writer once, finite GT completion/save phases on the exact current-platform/default-quality resource. Editor criterion is separate from strict shader-map diagnostics. No invented timeout, global compiler cancellation/pump, Function/streaming fanout equivalence or replay."));
  R.Data->SetObjectField(TEXT("shared_material_read_metrics"),Work.Metrics);
 }
 void Cache(bool Terminal,bool Safe){LastSnapshot=Operations::EncodeVariableResult(Work.Result,Operation());MaterialPostprocessAsync::Publish(RawJob,LastSnapshot,Terminal,Safe);}
 void Quarantine(const FString& Why)
 {
  if(Phase==EReferencePhase::Quarantined||Phase==EReferencePhase::Terminal)return;Unhook();Work.CancelRequested=true;Work.CancelReason=Why;
  Work.Error(TEXT("material_reference_async_context_quarantined"),Why);Work.Result.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Work.Result.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Metadata(false);Phase=EReferencePhase::Quarantined;Cache(false,false);
 }
 void Pending()
 {
  if(!PublicationSafe()){Quarantine(TEXT("Reference pending publication lost exact Editor/world/Job; retained state is not released."));return;}
  Work.Result.bSuccess=false;Work.Result.bPostconditionPassed=false;Metadata(false);Cache(false,true);
 }
 bool Observe()
 {
  auto& R=Work.Result;if(!Work.Admit()||!Capture(Work.Context,R,TEXT("async_current_observation"))||!Verify(Work.Context,R,Work.Expected))return false;
  const auto Point=R.Data->GetObjectField(TEXT("async_current_observation"));
  if(!Work.Authored.IsValid()||!SameObservation(Work.Authored.ToSharedRef(),Point->GetObjectField(TEXT("graph")).ToSharedRef()))return Work.Error(TEXT("material_reference_async_authored_graph_changed"),TEXT("Authored values/ordered inputs changed across callback/tick; no new compiler/save."));
  R.bChanged|=Work.Changed||Work.Context.WasDirty!=Work.Context.Selected.Asset->GetOutermost()->IsDirty();return true;
 }
 bool Point(const TCHAR* Name){return Work.Admit()&&Observe()&&ObserveCompile(Work.Context,Work.Result,Name);}
 bool DiskProof()
 {
  auto& R=Work.Result;if(!Save||!Work.Changed)return true;
  if(!DeferredPackageOwned(Work.Context,R))return false;
  const auto File=R.Data->GetObjectField(TEXT("async_current_observation"))->GetObjectField(TEXT("disk"));
  const bool OK=R.bSaved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!Work.Context.Selected.Asset->GetOutermost()->IsDirty();
  R.Data->SetBoolField(TEXT("save_disk_verified"),OK);R.bChanged|=Work.Context.BeforeSHA!=File->GetStringField(TEXT("sha1"));
  return OK||Fail(R,TEXT("material_reference_async_final_disk_failed"),TEXT("Actual save history retained; current package/disk proof failed, no second save."));
 }
 void Success(){auto& R=Work.Result;R.bChangeStateKnown=true;R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("complete");Phase=EReferencePhase::Final;}
 void Finalize()
 {
  if(FinalEncoded||Phase==EReferencePhase::Terminal||Phase==EReferencePhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("Reference final publication lost exact Editor/world/Job."));return;}
  auto& R=Work.Result;if(R.Error.IsEmpty()&&!Work.Admit()){}
  if(!R.Error.IsEmpty())
  {
   R.bSuccess=false;R.bPostconditionPassed=false;
   if(!Work.TargetSDKEntered&&!R.bCompileAttempted&&!R.bSaveAttempted){R.bChangeStateKnown=true;R.bChanged=false;R.Compiled=false;R.Data->SetBoolField(TEXT("target_SDK_entered"),false);}
  }
  Metadata(true);FinalEncoded=true;Cache(true,true);Phase=EReferencePhase::Terminal;Unhook();
 }
 void Execute()
 {
  auto& R=Work.Result;
  if(Work.CancelRequested||MaterialPostprocessAsync::IsStopping())Work.Error(TEXT("material_reference_async_cancelled"),Work.CancelReason.IsEmpty()?TEXT("Reference continuation stopped without compiler cancellation/recovery/replay."):Work.CancelReason);
  if(!R.Error.IsEmpty()||!Work.Admit()){Phase=EReferencePhase::Final;Finalize();return;}
  if(Phase==EReferencePhase::Dispatch)
  {
   if(Dispatched){Work.Error(TEXT("material_reference_async_replay_forbidden"),TEXT("Original writer already entered."));Phase=EReferencePhase::Final;Finalize();return;}
   Dispatched=true;if(Collections)ReplaceCollections(MaterialPath,TargetPath,SourcePath,GraphType,Save);else ReplaceTextures(MaterialPath,ReplacementsJson,GraphType,Save);
   R.Data->SetBoolField(TEXT("transaction_closed_before_yield"),true);
   Phase=Work.Ready&&R.Error.IsEmpty()?EReferencePhase::WaitInitial:EReferencePhase::Final;
  }
  else if(Phase==EReferencePhase::WaitInitial)
  {
   if(!Point(TEXT("initial_compile_status")))Phase=EReferencePhase::Final;
   else if(R.Compiled.IsSet())
   {
    R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&Work.Changed&&!R.Compiled.GetValue());
    if(!R.Compiled.GetValue()){R.bChangeStateKnown=true;Fail(R,TEXT("material_reference_compile_failed"),TEXT("Actual original Editor compile criterion failed; actual replacements retained, save suppressed."));Phase=EReferencePhase::Final;}
    else Phase=EReferencePhase::Save;
   }
  }
  else if(Phase==EReferencePhase::Save)
  {
   if(!Observe()||!CompleteReferenceSave(Work.Context,R,Work.Expected,Work.Changed,Collections))Phase=EReferencePhase::Final;
   else if(Save&&Work.Changed&&!R.Compiled.IsSet())Phase=EReferencePhase::WaitSaved;
   else if(!Observe()||!DiskProof())Phase=EReferencePhase::Final;else Success();
  }
  else if(Phase==EReferencePhase::WaitSaved)
  {
   if(!Point(TEXT("final_compile_status")))Phase=EReferencePhase::Final;
   else if(R.Compiled.IsSet())
   {if(!R.Compiled.GetValue()){R.bChangeStateKnown=true;Fail(R,TEXT("material_reference_compile_failed_after_save"),TEXT("Final Editor criterion failed; savedTrue history retained."));Phase=EReferencePhase::Final;}else if(!DiskProof())Phase=EReferencePhase::Final;else Success();}
  }
  if(!R.Error.IsEmpty())Phase=EReferencePhase::Final;
  if(Phase==EReferencePhase::Final){Finalize();return;}Pending();
 }
 void ReleaseSafe();
 bool Tick(float)
 {
  TSharedRef<FReferenceJobState> Hold=AsShared();if(Phase==EReferencePhase::Terminal||Phase==EReferencePhase::Quarantined)return false;
  if(Executing||DeferredRunning||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("Reference tick lost exact Editor/world/global safety."));return false;}
  {TGuardValue<bool> Busy(Executing,true);TGuardValue<bool> Running(DeferredRunning,true);TGuardValue<FDeferredWork*> Current(CurrentDeferredWork,&Work);Material::FReadMetricsScope Metrics;FrameMetrics=&Metrics;FrameMetricsMerged=false;Execute();FrameMetrics=nullptr;}
  ReleaseSafe();return Phase!=EReferencePhase::Terminal&&Phase!=EReferencePhase::Quarantined;
 }
 void Hook(){const TWeakPtr<FReferenceJobState> Weak=AsShared();Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto State=Weak.Pin();return State&&State->Tick(Delta);}),0.0f);}
};
TSharedPtr<FReferenceJobState>& ActiveReferenceJob(){static auto* State=new TSharedPtr<FReferenceJobState>;return *State;}
void FReferenceJobState::ReleaseSafe()
{
 if(Executing||Phase!=EReferencePhase::Terminal||!PublicationSafe())return;
 Work.Objects.Reset();Work.Context.Aux.Reset();Work.Context.Expressions.Reset();Work.Context.Selected.Asset.Reset();Work.Context.Selected.SelectedExpression.Reset();JobIdentity.Reset();Job.Reset();
 if(ActiveReferenceJob().Get()==this){ActiveReferenceJob().Reset();DeferredLease=false;}
}
UCubelessMaterialPostprocessJob* StartReferenceJob(const FString& Text,bool Collections)
{
 if(!DeferredGlobals()||DeferredRunning||DeferredLease||MaterialPostprocessAsync::IsStopping())return nullptr;
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!DeferredLive(World)||World->bIsTearingDown)return nullptr;
 auto State=MakeShared<FReferenceJobState>();State->Collections=Collections;State->Work.Editor=GEditor;State->Work.World=World;ActiveReferenceJob()=State;DeferredLease=true;UCubelessMaterialPostprocessJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> Running(DeferredRunning,true);State->Work.Result.bMutationAttempted=true;State->Work.Result.Data->SetNumberField(TEXT("NewObject_MaterialPostprocessJob"),1);
  Created=NewObject<UCubelessMaterialPostprocessJob>(GetTransientPackage());State->RawJob=Created;State->Work.Result.Data->SetBoolField(TEXT("job_allocation_returned"),true);State->Work.Result.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
  if(!State->PublicationSafe()||State->Work.CancelRequested||MaterialPostprocessAsync::IsStopping()){State->Quarantine(TEXT("Reference Job allocation interrupted before original writer dispatch."));return nullptr;}
  if(!Created||!DeferredLive(Created)||Created->GetClass()!=UCubelessMaterialPostprocessJob::StaticClass()||Created->GetOuter()!=GetTransientPackage()||Created->GetPackage()!=GetTransientPackage()){ActiveReferenceJob().Reset();DeferredLease=false;return nullptr;}State->Job.Reset(Created);State->JobIdentity=MakeUnique<FDeferredIdentity>(Created);
  MaterialPostprocessAsync::RegisterJob(Created,Collections?MaterialPostprocessAsync::EJobKind::ReplaceCollections:MaterialPostprocessAsync::EJobKind::ReplaceTextures,State->Id);
  TSharedPtr<FJsonObject> Params;bool Valid=ParseReferenceJobParams(Text,Params,Collections?5:4);
  if(Valid)Valid=Params->TryGetStringField(TEXT("material_path"),State->MaterialPath)&&Params->TryGetStringField(TEXT("graph_type"),State->GraphType)&&Params->TryGetBoolField(TEXT("save"),State->Save);
  if(Valid&&Collections)Valid=Params->TryGetStringField(TEXT("target_collection_path"),State->TargetPath)&&Params->TryGetStringField(TEXT("source_collection_path"),State->SourcePath);
  if(Valid&&!Collections)
  {
   const TSharedPtr<FJsonObject>* Pairs=nullptr;Valid=Params->TryGetObjectField(TEXT("replacements"),Pairs)&&Pairs&&Pairs->IsValid()&&(*Pairs)->Values.Num()<=512;
   if(Valid){for(const auto& Pair:(*Pairs)->Values)if(!Pair.Value.IsValid()||Pair.Value->Type!=EJson::String){Valid=false;break;}}
   if(Valid){auto Writer=TJsonWriterFactory<>::Create(&State->ReplacementsJson);FJsonSerializer::Serialize((*Pairs).ToSharedRef(),Writer);Valid=State->ReplacementsJson.Len()<=262144;}
  }
  State->Work.Result.bSaveRequested=State->Save;
  if(!Valid){Fail(State->Work.Result,TEXT("material_reference_async_params_invalid"),TEXT("Exact normalized reference5/texture4 fields required, replacements is an object of path strings."));State->Phase=EReferencePhase::Final;State->Finalize();}
  else State->Pending();
 }
 if(State->Phase==EReferencePhase::Terminal){State->ReleaseSafe();return Created;}if(State->Phase!=EReferencePhase::Quarantined)State->Hook();return Created;
}
bool CancelReferenceJob(UCubelessMaterialPostprocessJob* Job,const FString& Reason,bool Collections)
{
 if(!IsInGameThread()||!ActiveReferenceJob()||ActiveReferenceJob()->RawJob!=Job||ActiveReferenceJob()->Collections!=Collections)return false;
 auto State=ActiveReferenceJob();if(State->Phase==EReferencePhase::Terminal||State->Phase==EReferencePhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Reference cancellation retained unsafe state without new SDK."));return true;}
 {TGuardValue<bool> Busy(State->Executing,true);State->Work.Error(TEXT("material_reference_async_cancelled"),State->Work.CancelReason);State->Phase=EReferencePhase::Final;State->Finalize();}State->ReleaseSafe();return true;
}
void ShutdownReferenceJob(const FString& Reason,bool Collections)
{
 auto State=ActiveReferenceJob();if(!State||State->Collections!=Collections)return;State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();if(State->Executing)return;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe reference state."));return;}CancelReferenceJob(State->RawJob,State->Work.CancelReason,Collections);
}

}
UCubelessMaterialPostprocessJob* StartReplaceCollectionsContinuation(const FString& Params){return StartReferenceJob(Params,true);}
UCubelessMaterialPostprocessJob* StartReplaceTexturesContinuation(const FString& Params){return StartReferenceJob(Params,false);}
bool CancelReplaceCollectionsContinuation(UCubelessMaterialPostprocessJob* Job,const FString& Reason){return CancelReferenceJob(Job,Reason,true);}
bool CancelReplaceTexturesContinuation(UCubelessMaterialPostprocessJob* Job,const FString& Reason){return CancelReferenceJob(Job,Reason,false);}
void ShutdownReplaceCollectionsContinuation(const FString& Reason){ShutdownReferenceJob(Reason,true);}
void ShutdownReplaceTexturesContinuation(const FString& Reason){ShutdownReferenceJob(Reason,false);}
bool HasReplaceCollectionsContinuationLease(){return (DeferredLease&&ActiveReferenceJob()&&ActiveReferenceJob()->Collections)||DeferredRunning;}
bool HasReplaceTexturesContinuationLease(){return (DeferredLease&&ActiveReferenceJob()&&!ActiveReferenceJob()->Collections)||DeferredRunning;}
}
