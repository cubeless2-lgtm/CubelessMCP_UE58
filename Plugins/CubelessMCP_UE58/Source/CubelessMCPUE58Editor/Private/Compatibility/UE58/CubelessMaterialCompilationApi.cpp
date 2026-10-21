#include "Compatibility/UE58/CubelessMaterialCompilationApi.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"
#include "CubelessMaterialPostprocessJob.h"
#include "Containers/Ticker.h"
#include "CoreGlobals.h"
#include "Editor.h"
#include "Engine/World.h"
#include "Misc/CoreDelegates.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectGlobals.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"

#include "Compatibility/UE58/CubelessMaterialApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "HAL/PlatformTime.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShaderPrecompileMode.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialExpressionCustom.h"
#include "Materials/MaterialFunctionInterface.h"
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
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::MaterialCompilation
{
namespace
{

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

struct FResult
{
    FString Stage = TEXT("preflight"), Code, Error;
    bool Success = false, Attempted = false, Known = true, Changed = false, Post = false;
    bool CompileAttempted = false, SaveRequested = false, SaveAttempted = false, Saved = false;
    TOptional<bool> Compiled;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
bool HasNul(const FString& Value)
{ for (int32 Index = 0; Index < Value.Len(); ++Index) { if (Value[Index] == TCHAR('\0')) { return true; } } return false; }
bool Loaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.Success = false; R.Code = Code; R.Error = Error; return false; }
FString Text(const TSharedRef<FJsonObject>& Object)
{ FString Value; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Value)); return Value; }
FString Encode(FResult& R, Material::FReadMetricsScope& Metrics)
{
    R.Data->SetObjectField(TEXT("shared_material_read_metrics"), Metrics.ToJson());
    auto Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("operation"), TEXT("compile_and_save_material"));
    Out->SetBoolField(TEXT("success"), R.Success); Out->SetStringField(TEXT("stage"), R.Stage);
    Out->SetStringField(TEXT("error_code"), R.Code); Out->SetStringField(TEXT("error"), R.Error);
    Out->SetBoolField(TEXT("mutation_attempted"), R.Attempted); Out->SetBoolField(TEXT("change_state_known"), R.Known);
    if (R.Known) { Out->SetBoolField(TEXT("changed"), R.Changed); } else { Out->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    Out->SetBoolField(TEXT("postcondition_passed"), R.Post); Out->SetBoolField(TEXT("compile_requested"), true);
    Out->SetBoolField(TEXT("compile_attempted"), R.CompileAttempted);
    if (R.Compiled.IsSet()) { Out->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); } else { Out->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); }
    Out->SetBoolField(TEXT("save_requested"), R.SaveRequested); Out->SetBoolField(TEXT("save_attempted"), R.SaveAttempted); Out->SetBoolField(TEXT("saved"), R.Saved);
    Out->SetBoolField(TEXT("outcome_unknown"), !R.Known); Out->SetBoolField(TEXT("atomic_rollback_available"), false); Out->SetBoolField(TEXT("auto_retry_safe"), false);
    Out->SetObjectField(TEXT("data"), R.Data); return Text(Out);
}
bool ObservationBudget(const TSharedPtr<FJsonValue>& Value, int32& Values, int32& Chars, int32 Depth = 0)
{
    if (!Value.IsValid() || ++Values > 1048576 || Depth > 32) { return false; }
    if (Value->Type == EJson::Object)
    { for (const auto& Pair : Value->AsObject()->Values) { if (!ObservationBudget(Pair.Value, Values, Chars, Depth + 1)) { return false; } } }
    else if (Value->Type == EJson::Array)
    { for (const auto& Row : Value->AsArray()) { if (!ObservationBudget(Row, Values, Chars, Depth + 1)) { return false; } } }
    else if (Value->Type == EJson::String)
    { const int32 Length = Value->AsString().Len(); if (Length > 4194304 || Chars > 4194304 - Length) { return false; } Chars += Length; }
    return true;
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
void Count(FResult& R, const TCHAR* Field)
{ R.Data->SetNumberField(Field, R.Data->GetNumberField(Field) + 1); }
TSharedRef<FJsonObject> Identity(const UObject* Object)
{
    auto Out = MakeShared<FJsonObject>(); Out->SetStringField(TEXT("path"), Object->GetPathName()); Out->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
    Out->SetStringField(TEXT("outer"), Object->GetOuter() ? Object->GetOuter()->GetPathName() : FString());
    Out->SetStringField(TEXT("object_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Object->GetFlags()))); return Out;
}
bool Fields(UObject* Object, TSharedRef<FJsonObject>& Out)
{
    int32 CountFields = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(Object->GetClass(), EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* Property = *It; if (++CountFields > 256 || Property->ArrayDim < 1 || Property->ArrayDim > 128) { return false; }
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 Index = 0; Index < Property->ArrayDim; ++Index)
        {
            FString Value; const void* Address = Property->ContainerPtrToValuePtr<void>(Object, Index);
            if (!Property->ExportText_Direct(Value, Address, Address, Object, PPF_None) || !ContinueAsyncRead() || Value.Len() > 1048576 || (Chars += Value.Len()) > 1048576) { return false; }
            Values.Add(MakeShared<FJsonValueString>(Value));
        }
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), Property->GetOwnerStruct()->GetPathName());
        Row->SetStringField(TEXT("property_class"), Property->GetClass()->GetName()); Row->SetNumberField(TEXT("array_dim"), Property->ArrayDim);
        Row->SetStringField(TEXT("flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Property->GetPropertyFlags()))); Row->SetArrayField(TEXT("elements"), Values);
        Out->SetObjectField(Property->GetName(), Row);
    }
    return true;
}
bool Disk(UObject* Asset, TSharedRef<FJsonObject>& Out)
{
    FString Filename; if (!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetOutermost()->GetName(), Filename, FPackageName::GetAssetPackageExtension())) { return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); const int64 Size = IFileManager::Get().FileSize(*Filename);
    Out->SetStringField(TEXT("path"), Filename); Out->SetBoolField(TEXT("exists"), Size >= 0); Out->SetNumberField(TEXT("bytes"), FMath::Max<int64>(Size, 0)); Out->SetStringField(TEXT("sha1"), TEXT(""));
    if (Size < 0) { return true; } TArray<uint8> Bytes;
    if (Size > 64 * 1024 * 1024 || !FFileHelper::LoadFileToArray(Bytes, *Filename) || Bytes.Num() != Size) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); Out->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Forbidden(const FString& Value)
{
    for (const TCHAR* Token : { TEXT("/Script/CubelessMCP"), TEXT("/CubelessMCP_UE58/"), TEXT("/Script/UnrealMCP"), TEXT("/UnrealMCP/"), TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Engine/PythonTypes") })
    { if (Value.Contains(Token, ESearchCase::IgnoreCase)) { return true; } }
    return false;
}
bool ForbiddenSelectedObservation(const TSharedRef<FJsonObject>& Observation)
{
    // Unrelated loaded-package names are observations, not selected runtime references.
    auto Selected = MakeShared<FJsonObject>();
    for (const TCHAR* Key : { TEXT("asset"), TEXT("asset_fields"), TEXT("graph") })
    {
        const auto Value = Observation->TryGetField(Key);
        if (!Value.IsValid() || Value->Type != EJson::Object || !Value->AsObject().IsValid()) { return true; }
        Selected->SetField(Key, Value);
    }
    return Forbidden(Text(Selected));
}
struct FContext
{
    TSharedRef<FJsonObject> Params = MakeShared<FJsonObject>();
    Material::FState Selected;
    UMaterial* MaterialAsset = nullptr; UMaterialFunction* FunctionAsset = nullptr;
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
  Result.Known=false;Result.Success=false;Result.Post=false;return Fail(Result,Code,Message);
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
  UObject* Candidate=Context.Selected.Asset.Get();
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
  if(Context.Selected.Asset.Get()!=Root){GuardFailed=true;return Error(TEXT("material_async_target_changed"),TEXT("Selected target changed after a callback."));}
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
bool Capture(FContext& C, FResult& R, const TCHAR* Name, TSharedRef<FJsonObject>& Out)
{
    Out = MakeShared<FJsonObject>(); Out->SetBoolField(TEXT("complete"), false); R.Data->SetObjectField(Name, Out);
    auto AssetFields = MakeShared<FJsonObject>(); auto File = MakeShared<FJsonObject>(); auto Dirty = MakeShared<FJsonObject>(); auto Sources = MakeShared<FJsonObject>();
    Out->SetObjectField(TEXT("asset_fields"), AssetFields); Out->SetObjectField(TEXT("disk"), File); Out->SetObjectField(TEXT("other_package_dirty"), Dirty); Out->SetObjectField(TEXT("native_source_classes"), Sources);
    Material::FState Current; FString Error; Count(R, TEXT("material_capture_calls"));
    const bool Captured = Material::CaptureState(Material::EOperation::ListNodes, C.Params, 512, Current, Error);
    if(!ContinueAsyncRead())return false;
    if (Current.Snapshot.IsValid()) { Out->SetObjectField(TEXT("graph"), Current.Snapshot.ToSharedRef()); }
    if (Loaded(Current.Asset.Get())) { Out->SetObjectField(TEXT("asset"), Identity(Current.Asset.Get())); Out->SetBoolField(TEXT("dirty"), Current.Asset->GetOutermost()->IsDirty()); }
    if (!Captured || !Loaded(Current.Asset.Get()))
    { Out->SetStringField(TEXT("failure"), Error); if (R.Attempted) { R.Known = false; } return Fail(R, TEXT("material_compilation_observation_incomplete"), TEXT("Material graph capture failed; available prefix retained: ") + Error); }
    if (!C.Selected.Asset.IsValid()) { C.Selected = MoveTemp(Current); C.Params->SetStringField(TEXT("material_path"), C.Selected.AssetPath); }
    else if (Current.Asset.Get() != C.Selected.Asset.Get() || Current.AssetPath != C.Selected.AssetPath)
    { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_target_relocated"), TEXT("Resolved current target differs from retained original asset; prefix retained, no retry.")); }
    if(!ContinueAsyncRead())return false;
    UObject* Asset = C.Selected.Asset.Get();
    for (UClass* Class : { Asset->GetClass(), UMaterialEditingLibrary::StaticClass() })
    {
        if (!Loaded(Class)) { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_source_incomplete"), TEXT("Source class identity is unavailable.")); }
        auto Row = Identity(Class); Row->SetStringField(TEXT("class_flags"), FString::Printf(TEXT("%llu"), static_cast<uint64>(Class->GetClassFlags()))); Row->SetBoolField(TEXT("dirty"), Class->GetOutermost()->IsDirty());
        Sources->SetObjectField(Class->GetPathName(), Row);
        UObject* Default = Class->GetDefaultObject(false); Row->SetBoolField(TEXT("CDO_exists"), Default != nullptr);
        if (Default && Loaded(Default)) { Row->SetObjectField(TEXT("CDO"), Identity(Default)); }
        else if (Default) { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_source_incomplete"), TEXT("Existing source CDO is not readable; no CDO creation is requested.")); }
    }
    if (!Fields(Asset, AssetFields) || !Disk(Asset, File))
    { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_observation_incomplete"), TEXT("Bounded reflected asset/file observation is incomplete; raw available maps retained.")); }
    int32 Packages = 0;
    for (TObjectIterator<UPackage> It; It; ++It)
    {
        if (!Loaded(*It) || *It == Asset->GetOutermost()) { continue; }
        if (++Packages > 8192) { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_package_budget"), TEXT("Loaded-package dirty observation exceeded its finite bound.")); }
        Dirty->SetBoolField(It->GetPathName(), It->IsDirty());
    }
    int32 JSONValues = 0, TextChars = 0;
    if (!ObservationBudget(MakeShared<FJsonValueObject>(Out), JSONValues, TextChars))
    { R.Known = !R.Attempted; return Fail(R, TEXT("material_compilation_observation_budget"), TEXT("Observation JSON/string budget exceeded; available raw prefix retained.")); }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
bool ObserveCompile(UMaterial* MaterialAsset, FResult& R, const TCHAR* Name, TOptional<bool>& Compiled)
{
    auto Out = MakeShared<FJsonObject>(); R.Data->SetObjectField(Name, Out); Out->SetBoolField(TEXT("complete"), false);
    Out->SetNumberField(TEXT("shader_platform"), static_cast<int32>(GMaxRHIShaderPlatform)); Out->SetBoolField(TEXT("null_rhi"), GUsingNullRHI);
    Out->SetBoolField(TEXT("resource_available"), false); Out->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); Out->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    for (const TCHAR* Key : { TEXT("game_thread_shader_map_complete"), TEXT("game_thread_shader_map_present"), TEXT("has_valid_game_thread_shader_map"), TEXT("shader_map_compilation_finalized"), TEXT("shader_map_compiled_successfully") }) { Out->SetField(Key, MakeShared<FJsonValueNull>()); }
    Out->SetStringField(TEXT("scope"), TEXT("Current-platform/default-quality resource point samples after one IsCompilationFinished call. That getter can finalize ready cache work; no explicit blocking finish/manager pump. Cached completeness, map presence, finalization and success are distinct observed fields; no Engine cause or other target/render/cook proof is inferred."));
    if (!Loaded(MaterialAsset)) { return false; } Count(R, TEXT("material_resource_queries")); FMaterialResource* Resource = MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);
    if (!Resource)
    {
        Out->SetStringField(TEXT("status"), TEXT("resource_unavailable")); Compiled = false; Out->SetBoolField(TEXT("compiled"), false);
        Out->SetNumberField(TEXT("compile_error_count"), 1); Out->SetArrayField(TEXT("compile_errors"), { MakeShared<FJsonValueString>(TEXT("No material resource is available for the current shader platform.")) });
        Out->SetBoolField(TEXT("complete"), true); return true;
    }
    Out->SetBoolField(TEXT("resource_available"), true); Count(R, TEXT("material_compile_status_queries"));
    const bool Finished = Resource->IsCompilationFinished();
    if(!ContinueAsyncRead())return false;
    if(CurrentDeferredWork)
    {
        Count(R,TEXT("material_resource_queries"));
        if(MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource||Resource->GetMaterialInterface()!=MaterialAsset)
        {R.Known=false;return Fail(R,TEXT("material_async_resource_changed"),TEXT("Current material resource changed during ready-cache application; no stale resource access."));}
        const int32 Q=static_cast<int32>(Resource->GetQualityLevel());
        if(CurrentDeferredWork->Quality.IsSet()&&CurrentDeferredWork->Quality.GetValue()!=Q)
        {R.Known=false;return Fail(R,TEXT("material_async_quality_changed"),TEXT("Default-quality resource role changed during continuation."));}
        CurrentDeferredWork->Quality=Q;
    }
    const TArray<FString> Errors = Resource->GetCompileErrors();
    const bool CompleteMap = Resource->IsGameThreadShaderMapComplete(); const bool HasValidMap = Resource->HasValidGameThreadShaderMap();
    const FMaterialShaderMap* ShaderMap = Resource->GetGameThreadShaderMap(); const bool HasMap = ShaderMap != nullptr;
    const bool Finalized = ShaderMap && ShaderMap->IsCompilationFinalized(); const bool ShaderSuccess = ShaderMap && ShaderMap->CompiledSuccessfully();
    Out->SetBoolField(TEXT("game_thread_shader_map_complete"), CompleteMap); Out->SetBoolField(TEXT("game_thread_shader_map_present"), HasMap);
    Out->SetBoolField(TEXT("has_valid_game_thread_shader_map"), HasValidMap);
    if (ShaderMap)
    {
        Out->SetBoolField(TEXT("shader_map_compilation_finalized"), Finalized);
        Out->SetBoolField(TEXT("shader_map_compiled_successfully"), ShaderSuccess);
    }
    Out->SetBoolField(TEXT("compilation_finished"), Finished); TArray<TSharedPtr<FJsonValue>> Rows; Out->SetArrayField(TEXT("compile_errors"), Rows); int32 Chars = 0;
    for (const FString& Error : Errors)
    {
        if (Rows.Num() >= 256 || Error.Len() > 1048576 || (Chars += Error.Len()) > 1048576) { return false; }
        Rows.Add(MakeShared<FJsonValueString>(Error)); Out->SetArrayField(TEXT("compile_errors"), Rows);
    }
    Out->SetNumberField(TEXT("compile_error_count"), Errors.Num());
    if (!Finished) { Compiled.Reset(); Out->SetStringField(TEXT("status"), TEXT("pending")); }
    else
    {
        const bool HadError = MaterialAsset->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform);
        // Preserve the original helper result as data; pointer-only validity cannot establish completion.
        const bool Strict=Finished&&CompleteMap&&HasValidMap&&HasMap&&Finalized&&ShaderSuccess&&Errors.IsEmpty();
        Out->SetBoolField(TEXT("strict_render_map_ready"),Strict);
        if(!ContinueAsyncRead())return false;
        if(CurrentDeferredWork)
        {
            Count(R,TEXT("material_resource_queries"));
            if(MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform)!=Resource)
            {R.Known=false;return Fail(R,TEXT("material_async_resource_changed"),TEXT("Resource role changed after original Editor compiler predicate."));}
        }
        Compiled = CurrentDeferredWork ? Finished&&Errors.IsEmpty()&&!HadError : Strict;
        Out->SetBoolField(TEXT("compiling_or_had_error"), HadError); Out->SetBoolField(TEXT("compiled"), Compiled.GetValue());
        Out->SetStringField(TEXT("status"), Compiled.GetValue() ? TEXT("current_resource_complete") : Errors.IsEmpty() ? TEXT("resource_incomplete") : TEXT("compile_errors"));
    }
    Out->SetBoolField(TEXT("complete"), true); return true;
}
}
bool CompleteCompileSave(FContext& C,FResult& R,const TSharedRef<FJsonObject>& Before,bool UnconnectedCustomInput,bool bSave,bool Deferred)
{
    UObject* Asset=C.Selected.Asset.Get();
    if (bSave)
    {
        R.Stage = TEXT("prepare_save"); R.Data->SetBoolField(TEXT("observations_complete"), false); R.Known = false; R.Compiled.Reset();
        R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("final status unobserved after save preparation; initial_compile_status remains an unchanged prefix"));
        for (const TCHAR* Key : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_error_count"), TEXT("compile_errors") })
        { R.Data->SetField(Key, MakeShared<FJsonValueNull>()); }
        if(!ContinueAsyncRead())return false;
        Count(R,TEXT("explicit_modify_calls"));Asset->Modify();if(!ContinueAsyncRead())return false;
        Count(R,TEXT("mark_package_dirty_calls"));Asset->MarkPackageDirty();if(!ContinueAsyncRead())return false;
        // Preserve the original MarkChanged conditional, using public Custom input fields.
        R.Data->SetBoolField(TEXT("save_post_edit_change_skipped_unconnected_custom"), UnconnectedCustomInput);
        if (!UnconnectedCustomInput) { Count(R, TEXT("explicit_post_edit_change_calls")); Asset->PostEditChange(); }
        if(!ContinueAsyncRead())return false;
        TSharedRef<FJsonObject> Prepared = MakeShared<FJsonObject>(); if (!Capture(C, R, TEXT("before_save_observation"), Prepared)) { return false; }
        if (ForbiddenSelectedObservation(Prepared)) { Fail(R, TEXT("material_compilation_reference_unsupported_after_work"), TEXT("Prepared selected asset identity, reflected fields or graph contain tool references after retained work; unrelated loaded-package names remain observed separately. Save suppressed, prefix unknown and no retry.")); return false; }
        R.Stage = TEXT("save"); R.SaveAttempted = true; Count(R, TEXT("save_attempt_count")); auto Call = MakeShared<FJsonObject>();
        Call->SetStringField(TEXT("asset_path"), Asset->GetPathName()); Call->SetStringField(TEXT("route"), TEXT("UEditorAssetLibrary::SaveLoadedAsset")); Call->SetBoolField(TEXT("only_if_dirty"), false);
        R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Call) });
        const bool SavedReturn=UEditorAssetLibrary::SaveLoadedAsset(Asset,false);R.Saved|=SavedReturn;Call->SetBoolField(TEXT("saved"),SavedReturn);R.Data->SetBoolField(TEXT("saved"),R.Saved);
        if(!ContinueAsyncRead())return false;
        R.Stage = TEXT("observe_save"); TSharedRef<FJsonObject> Saved = MakeShared<FJsonObject>(); if (!Capture(C, R, TEXT("after_save_observation"), Saved)) { return false; }
        R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("observed_selected_changed"), !SameObservation(Before, Saved));
        const bool DiskOK = R.Saved && Saved->GetObjectField(TEXT("disk"))->GetBoolField(TEXT("exists")) && Saved->GetObjectField(TEXT("disk"))->GetNumberField(TEXT("bytes")) > 0 && !Asset->GetOutermost()->IsDirty();
        R.Data->SetBoolField(TEXT("save_disk_verified"), DiskOK); R.Data->SetBoolField(TEXT("dirty_after_compile"), Asset->GetOutermost()->IsDirty());
        if (!R.Saved || !DiskOK) { Fail(R, TEXT("material_compilation_save_unverified"), TEXT("Forced save return/disk/clean criterion failed; actual prefix retained and no save retry.")); return false; }
        TOptional<bool> FinalCompiled;
        if (!ObserveCompile(C.MaterialAsset, R, TEXT("final_compile_status"), FinalCompiled))
        { R.Compiled.Reset(); Fail(R, TEXT("material_compilation_status_incomplete"), TEXT("Final current resource status is incomplete after real save; saved prefix retained.")); return false; }
        R.Compiled = FinalCompiled; const auto FinalStatus = R.Data->GetObjectField(TEXT("final_compile_status"));
        R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("final_compile_status after real save; initial_compile_status remains an unchanged prefix"));
        for (const TCHAR* Key : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_error_count"), TEXT("compile_errors") })
        { const auto Value = FinalStatus->TryGetField(Key); R.Data->SetField(Key, Value.IsValid() ? Value : MakeShared<FJsonValueNull>()); }
        if (!R.Compiled.IsSet() && !Deferred) { Fail(R, TEXT("material_compilation_pending_after_save"), TEXT("Original save preparation scheduled pending current shader work; real saved1 retained, compiled null/no retry. Initial gate remains prefix-scoped.")); return false; }
        if (R.Compiled.IsSet() && !R.Compiled.GetValue()) { R.Known = true; R.Changed = !SameObservation(Before, Saved); Fail(R, TEXT("material_compilation_failed_after_save"), TEXT("Final resource fails current compile criterion after real save; prefix retained, no retry.")); return false; }
        R.Known = true; R.Changed = !SameObservation(Before, Saved);
    }
    return true;
}


namespace
{
// IC: this is an observed loaded-interface inventory, not a dependency-closure or
// global shader completion proof. The public Function SDK itself owns the fanout.
bool CaptureFunctionFanout(FDeferredWork& W,const TCHAR* Name,bool HoldBefore)
{
 if(!W.Admit())return false;
 auto J=MakeShared<FJsonObject>();W.Result.Data->SetObjectField(Name,J);
 J->SetStringField(TEXT("scope"),TEXT("Bounded point sample of loaded Material/Function interfaces; membership does not assert dependency or completion. Existing interfaces are strongly held across the Function SDK call."));
 J->SetBoolField(TEXT("dependency_closure_verified"),false);J->SetBoolField(TEXT("shader_completion_observed"),false);
 TArray<TSharedPtr<FJsonValue>> Rows;int32 Seen=0,Chars=0;bool Truncated=false;
 auto Add=[&](UObject* O,UMaterialFunctionInterface* Function)->bool
 {
  if(++Seen>8192)return W.Error(TEXT("material_function_fanout_budget"),TEXT("Loaded interface inventory exceeds finite8192 bound; no further target work."));
  if(!AsyncReadable(O)||!AsyncReadable(O->GetOutermost()))return W.Error(TEXT("material_function_fanout_unreadable"),TEXT("A current loaded interface is not readable; bounded prefix retained."));
  if(HoldBefore&&!W.Own(O))return false;
  const FString P=O->GetPathName();const FString C=O->GetClass()->GetPathName();
  if(Rows.Num()>=512||P.Len()>2048||C.Len()>2048||Chars+P.Len()+C.Len()>131072){Truncated=true;return true;}
  Chars+=P.Len()+C.Len();auto Row=MakeShared<FJsonObject>();
  Row->SetStringField(TEXT("path"),P);Row->SetStringField(TEXT("class"),C);
  Row->SetBoolField(TEXT("dirty"),O->GetOutermost()->IsDirty());
  if(Function)Row->SetStringField(TEXT("StateId"),Function->StateId.ToString(EGuidFormats::DigitsWithHyphens));
  Rows.Add(MakeShared<FJsonValueObject>(Row));return true;
 };
 for(TObjectIterator<UMaterialFunctionInterface> It(RF_ClassDefaultObject,true,EInternalObjectFlags::Garbage);It;++It)
  if(!Add(*It,*It))return false;
 for(TObjectIterator<UMaterialInterface> It(RF_ClassDefaultObject,true,EInternalObjectFlags::Garbage);It;++It)
  if(!Add(*It,nullptr))return false;
 J->SetArrayField(TEXT("interfaces"),Rows);J->SetNumberField(TEXT("observed_interface_count"),Seen);
 J->SetNumberField(TEXT("returned_row_count"),Rows.Num());J->SetBoolField(TEXT("rows_truncated"),Truncated);
 J->SetBoolField(TEXT("inventory_scan_complete"),true);return W.Admit();
}
bool VerifyFunctionTarget(FDeferredWork& W,const TCHAR* Name)
{
 if(!W.Admit())return false;
 auto Point=MakeShared<FJsonObject>();if(!Capture(W.Context,W.Result,Name,Point))return false;
 W.After=Point;
 if(!SameObservation(W.Before->GetObjectField(TEXT("graph")).ToSharedRef(),Point->GetObjectField(TEXT("graph")).ToSharedRef()))
  return W.Error(TEXT("material_function_authored_graph_changed"),TEXT("Selected authored graph projection changed during Function update/save; actual prefix retained, no rollback or resubmit."));
 W.Result.Changed|=!SameObservation(W.Before,Point);
 return W.Admit();
}
bool CompleteFunctionUpdate(FDeferredWork& W,bool Save,bool UnconnectedCustomInput)
{
 auto& R=W.Result;auto& C=W.Context;UObject* Asset=C.Selected.Asset.Get();
 R.Compiled.Reset();
 auto Defaults=MakeShared<FJsonObject>();Defaults->SetBoolField(TEXT("compiled"),true);Defaults->SetBoolField(TEXT("compilation_finished"),true);
 Defaults->SetNumberField(TEXT("compile_error_count"),0);Defaults->SetArrayField(TEXT("compile_errors"),{});
 Defaults->SetStringField(TEXT("scope"),TEXT("Original Function handler initialized these fields; UpdateMaterialFunction does not inspect a Function render resource. These are legacy response defaults, not actual shader completion."));
 R.Data->SetObjectField(TEXT("original_unverified_function_defaults"),Defaults);
 for(const auto& P:Defaults->Values)if(!FString(P.Key.ToView()).Equals(TEXT("scope")))R.Data->SetField(P.Key,P.Value);
 R.Data->SetStringField(TEXT("material"),Asset->GetPathName());R.Data->SetStringField(TEXT("graph_type"),TEXT("function"));
 R.Data->SetBoolField(TEXT("was_dirty_before_compile"),W.Before->GetBoolField(TEXT("dirty")));
 R.Data->SetBoolField(TEXT("function_shader_completion_observed"),false);
 R.Data->SetStringField(TEXT("function_completion_criterion"),TEXT("function_update_and_selected_save_returned"));
 R.Data->SetBoolField(TEXT("global_function_fanout_verified"),false);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);
 R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),false);R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);
 if(!VerifyFunctionTarget(W,TEXT("after_function_update_observation")))return false;
 if(Save)
 {
  R.Stage=TEXT("prepare_function_save");R.Known=false;
  if(!W.Admit())return false;
  Count(R,TEXT("explicit_modify_calls"));Asset->Modify();if(!W.Admit())return false;
  Count(R,TEXT("mark_package_dirty_calls"));Asset->MarkPackageDirty();if(!W.Admit())return false;
  R.Data->SetBoolField(TEXT("save_post_edit_change_skipped_unconnected_custom"),UnconnectedCustomInput);
  if(!UnconnectedCustomInput){Count(R,TEXT("explicit_post_edit_change_calls"));Asset->PostEditChange();if(!W.Admit())return false;}
  if(!VerifyFunctionTarget(W,TEXT("before_function_save_observation")))return false;
  if(ForbiddenSelectedObservation(W.After))
   return W.Error(TEXT("material_compilation_reference_unsupported_after_work"),TEXT("Prepared Function contains unsupported runtime references; actual update retained, no save/replay."));
  R.Stage=TEXT("save_function");R.SaveAttempted=true;Count(R,TEXT("save_attempt_count"));
  auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),Asset->GetPathName());
  Call->SetStringField(TEXT("route"),TEXT("UEditorAssetLibrary::SaveLoadedAsset"));Call->SetBoolField(TEXT("only_if_dirty"),false);
  Call->SetBoolField(TEXT("returned"),false);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
  const bool SavedReturn=UEditorAssetLibrary::SaveLoadedAsset(Asset,false);
  R.Saved|=SavedReturn;R.Data->SetBoolField(TEXT("saved"),R.Saved);Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),SavedReturn);
  // Save truth is adopted before any callback/lifetime/postcondition observation.
  if(!W.Admit())return false;
  if(!VerifyFunctionTarget(W,TEXT("after_function_save_observation")))return false;
  const auto File=W.After->GetObjectField(TEXT("disk"));
  const bool DiskOK=R.Saved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!Asset->GetOutermost()->IsDirty();
  R.Data->SetBoolField(TEXT("save_disk_verified"),DiskOK);
  if(!DiskOK){R.Known=true;return Fail(R,TEXT("material_function_save_unverified"),TEXT("Function save return/nonempty disk/clean target criterion failed; actual saved prefix retained without retry."));}
 }
 if(!CaptureFunctionFanout(W,TEXT("after_function_fanout_inventory"),false))return false;
 if(!VerifyFunctionTarget(W,TEXT("final_function_target_observation")))return false;
 R.Data->SetBoolField(TEXT("observations_complete"),true);R.Data->SetBoolField(TEXT("function_target_scope_complete"),true);
 R.Data->SetBoolField(TEXT("dirty_after_compile"),Asset->GetOutermost()->IsDirty());
 R.Data->SetStringField(TEXT("change_state_scope"),TEXT("Complete selected Function authored graph/identity/fields/disk plus bounded loaded-interface receipt; global dependency fanout/shader/render/cache preservation is explicitly unverified."));
 R.Known=true;R.Success=true;R.Post=true;R.Stage=TEXT("complete");return true;
}
}

static FString RunCompileAndSave(const FString& MaterialPath, const FString& GraphType, bool bSave,FDeferredWork* Deferred)
{
    FDeferredWork LocalWork;FResult& R=Deferred?Deferred->Result:LocalWork.Result;R.SaveRequested=bSave;Material::FReadMetricsScope Metrics;FContext& C=Deferred?Deferred->Context:LocalWork.Context;
    for (const TCHAR* Key : { TEXT("material_capture_calls"), TEXT("outer_transaction_calls"), TEXT("force_recompile_background_calls"), TEXT("update_material_function_calls"), TEXT("material_update_context_calls"), TEXT("explicit_modify_calls"), TEXT("explicit_pre_edit_change_calls"), TEXT("explicit_post_edit_change_calls"), TEXT("mark_package_dirty_calls"), TEXT("material_resource_queries"), TEXT("material_compile_status_queries"), TEXT("save_attempt_count"), TEXT("finish_compilation_calls"), TEXT("explicit_GC_calls"), TEXT("shader_manager_pump_calls"), TEXT("material_editing_library_recompile_calls") }) { R.Data->SetNumberField(Key, 0); }
    R.Data->SetArrayField(TEXT("save_calls"), TArray<TSharedPtr<FJsonValue>>()); R.Data->SetBoolField(TEXT("saved"), false); R.Data->SetBoolField(TEXT("save_requested"), bSave);
    R.Data->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); R.Data->SetField(TEXT("compilation_finished"), MakeShared<FJsonValueNull>());
    R.Data->SetBoolField(TEXT("observations_complete"), false); R.Data->SetBoolField(TEXT("all_implicit_work_measured"), false); R.Data->SetBoolField(TEXT("global_function_fanout_verified"), false);
    R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"), false); R.Data->SetBoolField(TEXT("material_editing_library_recompile_equivalent"), false);
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("Finite immediate public background compiler slice. Material Pre/PostEditChange, FMaterialUpdateContext and ForceRecompileForRendering(Background) can schedule shader/resource/render work; Function Update can mutate dependent loaded materials/functions/instances. Native does not call RecompileMaterial's GC/modal texture-streaming helper, explicit FinishCompilation, manager pump, playback or GC. Save/PreSave and callbacks remain Engine-owned and may perform other work."));
    R.Data->SetStringField(TEXT("changed_scope"), TEXT("Raw scoped current target graph/reflected fields/dirty/disk and loaded-package dirty observations. Pending shader completion or unverified Function fanout leaves typed changed=null/outcome_unknown=true even when an immediate bounded prefix is complete. No automatic resubmit/rollback."));
    R.Data->SetStringField(TEXT("read_only_pending_observer"), TEXT("Existing analyze_material_graph -> ue58_compile_observation; no new compile submission or explicit blocking finish. Do not rerun this writer to poll."));
    if (!IsEditorGameThread() || IsPlaySessionActive()) { Fail(R, TEXT("material_compilation_editor_state_invalid"), TEXT("Requires the Editor game thread outside PIE/SIE.")); return Encode(R, Metrics); }
    if (MaterialPath.IsEmpty() || MaterialPath.Len() > 2048 || GraphType.Len() > 64 || HasNul(MaterialPath) || HasNul(GraphType) || Forbidden(MaterialPath))
    { Fail(R, TEXT("material_compilation_input_invalid"), TEXT("Bounded original material path/type without tool dependency references required.")); return Encode(R, Metrics); }
    C.Params->SetStringField(TEXT("material_path"), MaterialPath); C.Params->SetStringField(TEXT("graph_type"), GraphType);
    TSharedRef<FJsonObject>& Before=Deferred?Deferred->Before:LocalWork.Before;
    if (!Capture(C, R, TEXT("before_observation"), Before)) { return Encode(R, Metrics); }
    FString Error; if (!Material::ValidateMutationTarget(C.Selected, Error)) { Fail(R, TEXT("material_compilation_target_unsupported"), Error); return Encode(R, Metrics); }
    if (ForbiddenSelectedObservation(Before)) { Fail(R, TEXT("material_compilation_reference_unsupported"), TEXT("Captured selected asset identity, reflected fields or graph contain tool runtime references; unrelated loaded-package names are outside this reference check. Arbitrary/transitive dependency parity remains separate.")); return Encode(R, Metrics); }
    UObject* Asset = C.Selected.Asset.Get(); C.MaterialAsset = Cast<UMaterial>(Asset); C.FunctionAsset = Cast<UMaterialFunction>(Asset);
    if (!Loaded(Asset) || Asset->HasAnyFlags(RF_ClassDefaultObject | RF_ArchetypeObject | RF_Transient) || (bSave && !Asset->GetOutermost()->IsFullyLoaded()))
    { Fail(R, TEXT("material_compilation_target_not_ready"), TEXT("Loaded non-template target and already fully loaded package for requested save required.")); return Encode(R, Metrics); }
    R.Data->SetStringField(TEXT("material"), Asset->GetPathName()); R.Data->SetStringField(TEXT("graph_type"), C.MaterialAsset ? TEXT("material") : TEXT("function"));
    R.Data->SetBoolField(TEXT("was_dirty_before_compile"), Before->GetBoolField(TEXT("dirty")));
    bool UnconnectedCustomInput = false; int32 CustomInputs = 0;
    const auto Expressions = C.MaterialAsset ? C.MaterialAsset->GetExpressions() : C.FunctionAsset->GetExpressions();
    for (const auto& Expression : Expressions)
    {
        if (const UMaterialExpressionCustom* Custom = Cast<UMaterialExpressionCustom>(Expression.Get()))
        {
            for (const FCustomInput& Input : Custom->Inputs)
            {
                if (++CustomInputs > 8192) { Fail(R, TEXT("material_compilation_custom_input_budget"), TEXT("Custom input read budget exceeded before compile dispatch.")); return Encode(R, Metrics); }
                if (!Input.Input.Expression) { UnconnectedCustomInput = true; }
            }
        }
    }
    R.Data->SetBoolField(TEXT("unconnected_custom_inputs"), UnconnectedCustomInput);

    TFunction<bool()> FunctionAdmission;
    TUniquePtr<TGuardValue<TFunction<bool()>*>> FunctionReadScope;
    FDeferredWork* FunctionWork=nullptr;
    if(C.FunctionAsset)
    {
        FunctionWork=Deferred?Deferred:&LocalWork;
        if(!Deferred)
        {
            if(!AsyncSafe()){Fail(R,TEXT("material_function_context_invalid"),TEXT("Safe exact Editor context required before Function SDK dispatch."));return Encode(R,Metrics);}
            FunctionWork->Editor=GEditor;FunctionWork->World=GEditor->GetEditorWorldContext().World();
            FunctionAdmission=[FunctionWork](){return FunctionWork->Admit();};
            FunctionReadScope=MakeUnique<TGuardValue<TFunction<bool()>*>>(AsyncReadAdmission,&FunctionAdmission);
        }
        if(!FunctionWork->Admit()||!CaptureFunctionFanout(*FunctionWork,TEXT("before_function_fanout_inventory"),true))return Encode(R,Metrics);
    }
    if(!ContinueAsyncRead())return Encode(R,Metrics);
    R.Stage = TEXT("request_compile"); Count(R, TEXT("outer_transaction_calls")); TUniquePtr<FScopedTransaction> Transaction=MakeUnique<FScopedTransaction>(NSLOCTEXT("CubelessMCP", "CompileMaterialImmediate", "Compile Material and Save"));
    if(!ContinueAsyncRead())return Encode(R,Metrics);
    R.Attempted = true; R.Known = false; R.CompileAttempted = true;
    if (C.MaterialAsset)
    {
        {
          Count(R,TEXT("material_update_context_calls"));FMaterialUpdateContext Update;Update.AddMaterial(C.MaterialAsset);
          if(!ContinueAsyncRead())return Encode(R,Metrics);
          Count(R,TEXT("explicit_pre_edit_change_calls"));Asset->PreEditChange(nullptr);
          if(!ContinueAsyncRead())return Encode(R,Metrics);
          Count(R,TEXT("explicit_post_edit_change_calls"));Asset->PostEditChange();
          if(!ContinueAsyncRead())return Encode(R,Metrics);
          Count(R,TEXT("force_recompile_background_calls"));C.MaterialAsset->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Background);
          if(!ContinueAsyncRead())return Encode(R,Metrics);
        }
        // The Engine-owned update-context destructor is part of the in-flight SDK scope.
        if(!ContinueAsyncRead())return Encode(R,Metrics);
        Count(R,TEXT("mark_package_dirty_calls"));Asset->MarkPackageDirty();
        if(!ContinueAsyncRead())return Encode(R,Metrics);
    }
    else
    { Count(R, TEXT("update_material_function_calls")); UMaterialEditingLibrary::UpdateMaterialFunction(C.FunctionAsset, nullptr); R.Data->SetBoolField(TEXT("function_update_returned"), true); }
    if(!ContinueAsyncRead())return Encode(R,Metrics);
    R.Stage = TEXT("observe_compile"); TSharedRef<FJsonObject> After = MakeShared<FJsonObject>();
    if (!Capture(C, R, TEXT("after_compile_observation"), After)) { return Encode(R, Metrics); }
    R.Data->SetBoolField(TEXT("observations_complete"), true); R.Data->SetBoolField(TEXT("observed_selected_changed"), !SameObservation(Before, After));
    if (C.FunctionAsset)
    {
        const bool Completed=CompleteFunctionUpdate(*FunctionWork,bSave,UnconnectedCustomInput);
        Transaction.Reset();R.Data->SetBoolField(TEXT("transaction_closed_before_final_publication"),true);
        if(!ContinueAsyncRead())return Encode(R,Metrics);
        if(!Completed){R.Success=false;R.Post=false;}
        return Encode(R,Metrics);
    }
    if (!ObserveCompile(C.MaterialAsset, R, TEXT("initial_compile_status"), R.Compiled))
    { Fail(R, TEXT("material_compilation_status_incomplete"), TEXT("Current resource/error prefix is incomplete after dispatch; no save/retry.")); return Encode(R, Metrics); }
    const auto Initial = R.Data->GetObjectField(TEXT("initial_compile_status"));
    R.Data->SetStringField(TEXT("direct_compile_status_scope"), TEXT("initial_compile_status; no later save preparation has begun"));
    for (const TCHAR* Key : { TEXT("compiled"), TEXT("compilation_finished"), TEXT("compile_error_count"), TEXT("compile_errors") })
    { const auto Value = Initial->TryGetField(Key); R.Data->SetField(Key, Value.IsValid() ? Value : MakeShared<FJsonValueNull>()); }
    R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"), bSave && !R.Compiled.IsSet());
    R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"), bSave && R.Compiled.IsSet() && !R.Compiled.GetValue());
    R.Data->SetBoolField(TEXT("dirty_after_compile"), Asset->GetOutermost()->IsDirty());
    if(Deferred)
    {
        Deferred->After=After;Deferred->Save=bSave;Deferred->Unconnected=UnconnectedCustomInput;
        Transaction.Reset();R.Data->SetBoolField(TEXT("transaction_closed_before_yield"),true);
        if(!ContinueAsyncRead())return Encode(R,Metrics);
        Deferred->Ready=true;R.Stage=TEXT("compile_dispatched");
        return Encode(R,Metrics);
    }
    if (!R.Compiled.IsSet())
    { Fail(R, TEXT("material_compilation_pending"), TEXT("Background compile is pending; compiled remains null, no save/blocking wait/manager pump or automatic resubmit. Observe with the existing status reader.")); return Encode(R, Metrics); }
    R.Known = true; R.Changed = !SameObservation(Before, After);
    if (!R.Compiled.GetValue()) { Fail(R, TEXT("material_compilation_failed"), TEXT("Observed current resource fails original finished/error/status criterion; save suppressed.")); return Encode(R, Metrics); }
    R.Data->SetBoolField(TEXT("initial_compile_gate_passed"), true);
    if(!CompleteCompileSave(C,R,Before,UnconnectedCustomInput,bSave,false))return Encode(R,Metrics);
    R.Post = true; R.Success = true; R.Stage = TEXT("complete"); return Encode(R, Metrics);
}
FString CompileAndSave(const FString& MaterialPath,const FString& GraphType,bool Save)
{
 if(AsyncRunning||AsyncLease){FResult R;Material::FReadMetricsScope Metrics;R.SaveRequested=Save;Fail(R,TEXT("material_async_lease_active"),TEXT("An owned Material continuation holds this core; no synchronous replay."));return Encode(R,Metrics);}
 return RunCompileAndSave(MaterialPath,GraphType,Save,nullptr);
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

enum class EAsyncPhase:uint8 { Dispatch,WaitInitial,Save,WaitSaved,Final,Terminal,Quarantined };
struct FCompileJobState:public TSharedFromThis<FCompileJobState>
{
 FDeferredWork Work;
 TStrongObjectPtr<UCubelessMaterialPostprocessJob> Job{nullptr};
 TUniquePtr<FAsyncIdentity> JobIdentity;
 UCubelessMaterialPostprocessJob* RawJob=nullptr;
 FGuid Id=FGuid::NewGuid();FString LastSnapshot,MaterialPath,GraphType=TEXT("auto");
 bool Save=true,Executing=false,Dispatched=false,FinalEncoded=false,FrameMetricsMerged=false;Material::FReadMetricsScope* FrameMetrics=nullptr;
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
  R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),R.CompileAttempted||R.SaveAttempted);R.Data->SetBoolField(TEXT("transient_creation_only"),!R.CompileAttempted&&!R.SaveAttempted);
  const bool FunctionTarget=Work.Context.FunctionAsset!=nullptr;
  R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false);R.Data->SetBoolField(TEXT("current_resource_completion_continuation"),!FunctionTarget);
  if(FunctionTarget)R.Data->SetStringField(TEXT("continuation_scope"),TEXT("Original Function update/optional target save completed within the existing Job; legacy compiler defaults are not actual shader completion and global fanout remains unverified."));
  if(!FunctionTarget)R.Data->SetStringField(TEXT("continuation_scope"),TEXT("Owned current-platform/default-quality Material completion and save-resume, same original core. Editor completion differs from strict shader-map/render diagnostics. No FinishCompilation/global pump/streaming-equivalence claim, no source writer replay."));
  R.Data->SetObjectField(TEXT("shared_material_read_metrics"),Work.Metrics);
 }
 FString Encoded()
 {
  // Keep the original compiler envelope, including outcome_unknown and its flat data.
  auto& R=Work.Result;auto Out=MakeShared<FJsonObject>();Out->SetStringField(TEXT("operation"),TEXT("compile_and_save_material"));
  Out->SetBoolField(TEXT("success"),R.Success);Out->SetStringField(TEXT("stage"),R.Stage);Out->SetStringField(TEXT("error_code"),R.Code);Out->SetStringField(TEXT("error"),R.Error);
  Out->SetBoolField(TEXT("mutation_attempted"),R.Attempted);Out->SetBoolField(TEXT("change_state_known"),R.Known);Out->SetField(TEXT("changed"),R.Known?TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(R.Changed)):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
  Out->SetBoolField(TEXT("postcondition_passed"),R.Post);Out->SetBoolField(TEXT("compile_requested"),true);Out->SetBoolField(TEXT("compile_attempted"),R.CompileAttempted);
  Out->SetField(TEXT("compiled"),R.Compiled.IsSet()?TSharedPtr<FJsonValue>(MakeShared<FJsonValueBoolean>(R.Compiled.GetValue())):TSharedPtr<FJsonValue>(MakeShared<FJsonValueNull>()));
  Out->SetBoolField(TEXT("save_requested"),R.SaveRequested);Out->SetBoolField(TEXT("save_attempted"),R.SaveAttempted);Out->SetBoolField(TEXT("saved"),R.Saved);Out->SetBoolField(TEXT("outcome_unknown"),!R.Known);
  Out->SetBoolField(TEXT("atomic_rollback_available"),false);Out->SetBoolField(TEXT("auto_retry_safe"),false);Out->SetObjectField(TEXT("data"),R.Data);return Text(Out);
 }
 void Cache(bool Terminal,bool Safe)
 {LastSnapshot=Encoded();MaterialPostprocessAsync::Publish(RawJob,LastSnapshot,Terminal,Safe);}
 void Quarantine(const FString& Reason)
 {
  if(Phase==EAsyncPhase::Quarantined)return;Unhook();Work.CancelRequested=true;Work.CancelReason=Reason;
  Work.Error(TEXT("material_async_context_quarantined"),Reason);Work.Result.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Work.Result.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Metadata(false);Phase=EAsyncPhase::Quarantined;Cache(false,false);
 }
 void Pending()
 {if(!PublicationSafe()){Quarantine(TEXT("Material pending publication lost exact Editor/world/Job."));return;}Work.Result.Success=false;Work.Result.Post=false;Metadata(false);Cache(false,true);}
 bool Observe(const TCHAR* Name)
 {
  if(!Work.Admit())return false;
  auto Point=MakeShared<FJsonObject>();if(!Capture(Work.Context,Work.Result,Name,Point))return false;
  Work.After=Point;
  if(!SameObservation(Work.Before->GetObjectField(TEXT("graph")).ToSharedRef(),Point->GetObjectField(TEXT("graph")).ToSharedRef()))
   return Work.Error(TEXT("material_async_authored_graph_changed"),TEXT("Selected authored graph/expression/input projection changed; no further compiler/save."));
  Work.Result.Changed|=!SameObservation(Work.Before,Point);return true;
 }
 bool Point(const TCHAR* Name)
 {
  if(!Work.Admit()||!Observe(TEXT("async_current_observation")))return false;
  if(!ObserveCompile(Work.Context.MaterialAsset,Work.Result,Name,Work.Result.Compiled))return false;
  const auto Status=Work.Result.Data->GetObjectField(Name);
  for(const TCHAR* Key:{TEXT("compiled"),TEXT("compilation_finished"),TEXT("compile_error_count"),TEXT("compile_errors")})
   {const auto V=Status->TryGetField(Key);Work.Result.Data->SetField(Key,V.IsValid()?V:MakeShared<FJsonValueNull>());}
  Work.Result.Data->SetStringField(TEXT("direct_compile_status_scope"),Name);
  return true;
 }
 void FinishSuccess()
 {Work.Result.Known=true;Work.Result.Success=true;Work.Result.Post=true;Work.Result.Stage=TEXT("complete");Phase=EAsyncPhase::Final;}
 void Finalize()
 {
  if(FinalEncoded||Phase==EAsyncPhase::Terminal||Phase==EAsyncPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("Material final publication lost Editor/world/Job context."));return;}
  if(Work.Result.Error.IsEmpty()&&!Work.Admit()){}
  if(!Work.Result.Error.IsEmpty()){Work.Result.Success=false;Work.Result.Post=false;}
  Metadata(true);FinalEncoded=true;Cache(true,true);Phase=EAsyncPhase::Terminal;Unhook();
 }
 void Execute()
 {
  if(Work.CancelRequested||MaterialPostprocessAsync::IsStopping())Work.Error(TEXT("material_async_cancelled"),Work.CancelReason.IsEmpty()?TEXT("Material continuation stopped; no save/recovery/replay."):Work.CancelReason);
  if(!Work.Result.Error.IsEmpty()){Phase=EAsyncPhase::Final;Finalize();return;}
  if(!Work.Admit()){Phase=EAsyncPhase::Final;Finalize();return;}
  if(Phase==EAsyncPhase::Dispatch)
  {
   if(Dispatched){Work.Error(TEXT("material_async_replay_forbidden"),TEXT("Original compiler already dispatched."));Phase=EAsyncPhase::Final;Finalize();return;}
   Dispatched=true;RunCompileAndSave(MaterialPath,GraphType,Save,&Work);
   const TSharedPtr<FJsonObject>* M=nullptr;if(Work.Result.Data->TryGetObjectField(TEXT("shared_material_read_metrics"),M)&&M&&M->IsValid()&&M->Get()!=&Work.Metrics.Get())Work.MergeMetrics((*M).ToSharedRef());
   Phase=Work.Ready&&Work.Result.Error.IsEmpty()?EAsyncPhase::WaitInitial:EAsyncPhase::Final;
  }
  else if(Phase==EAsyncPhase::WaitInitial)
  {
   if(!Point(TEXT("initial_compile_status")))Phase=EAsyncPhase::Final;
   else if(Work.Result.Compiled.IsSet())
   {
    Work.Result.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);
    Work.Result.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&!Work.Result.Compiled.GetValue());
    if(!Work.Result.Compiled.GetValue()){Work.Result.Known=true;Fail(Work.Result,TEXT("material_compilation_failed"),TEXT("Actual original Editor compiler criterion failed; save suppressed."));Phase=EAsyncPhase::Final;}
    else {Work.Result.Data->SetBoolField(TEXT("initial_compile_gate_passed"),true);Phase=EAsyncPhase::Save;}
   }
  }
  else if(Phase==EAsyncPhase::Save)
  {
   if(!Observe(TEXT("async_before_save")))Phase=EAsyncPhase::Final;
   else if(!CompleteCompileSave(Work.Context,Work.Result,Work.Before,Work.Unconnected,Save,true))Phase=EAsyncPhase::Final;
   else if(Save&&!Work.Result.Compiled.IsSet())Phase=EAsyncPhase::WaitSaved;
   else FinishSuccess();
  }
  else if(Phase==EAsyncPhase::WaitSaved)
  {
   if(!Point(TEXT("final_compile_status")))Phase=EAsyncPhase::Final;
   else if(Work.Result.Compiled.IsSet())
   {
    if(!Work.Result.Compiled.GetValue()){Work.Result.Known=true;Fail(Work.Result,TEXT("material_compilation_failed_after_save"),TEXT("Actual final Editor compiler criterion failed; saved prefix retained."));Phase=EAsyncPhase::Final;}
    else FinishSuccess();
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
  TSharedRef<FCompileJobState> Hold=AsShared();if(Phase==EAsyncPhase::Terminal||Phase==EAsyncPhase::Quarantined)return false;
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
  const TWeakPtr<FCompileJobState> Weak=AsShared();
  Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto S=Weak.Pin();return S&&S->Tick(Delta);}),0.0f);
  PreExit=FCoreDelegates::OnEnginePreExit.AddLambda([](){MaterialPostprocessAsync::Shutdown(TEXT("engine_pre_exit"));});
 }
};
TSharedPtr<FCompileJobState>& ActiveCompileJob(){static auto* P=new TSharedPtr<FCompileJobState>;return *P;}
void FCompileJobState::ReleaseSafe()
{
 if(Executing||Phase!=EAsyncPhase::Terminal||!PublicationSafe())return;
 Work.Objects.Reset();Work.Context.Selected.Asset.Reset();Work.Context.Selected.SelectedExpression.Reset();JobIdentity.Reset();Job.Reset();
 if(ActiveCompileJob().Get()==this){ActiveCompileJob().Reset();AsyncLease=false;}
}
}
UCubelessMaterialPostprocessJob* StartCompileAndSaveContinuation(const FString& ParamsJson)
{
 if(!AsyncSafe()||AsyncRunning||AsyncLease||MaterialPostprocessAsync::IsStopping())return nullptr;
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!AsyncLive(World)||World->bIsTearingDown||GEditor->PlayWorld||GEditor->GetPIEWorldContext())return nullptr;
 auto S=MakeShared<FCompileJobState>();S->Work.Editor=GEditor;S->Work.World=World;ActiveCompileJob()=S;AsyncLease=true;
 UCubelessMaterialPostprocessJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(S->Executing,true);TGuardValue<bool> Lease(AsyncRunning,true);
  S->Work.Result.Attempted=true;S->Work.Result.Data->SetNumberField(TEXT("NewObject_MaterialPostprocessJob"),1);
  Created=NewObject<UCubelessMaterialPostprocessJob>(GetTransientPackage());S->RawJob=Created;
  S->Work.Result.Data->SetBoolField(TEXT("job_allocation_returned"),true);S->Work.Result.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
  if(!S->PublicationSafe()||S->Work.CancelRequested||MaterialPostprocessAsync::IsStopping()){S->Quarantine(TEXT("Material Job allocation interrupted before target dispatch."));return nullptr;}
  if(!Created||!AsyncReadable(Created)){ActiveCompileJob().Reset();AsyncLease=false;return nullptr;}
  S->Job.Reset(Created);S->JobIdentity=MakeUnique<FAsyncIdentity>(Created);
  MaterialPostprocessAsync::RegisterJob(Created,MaterialPostprocessAsync::EJobKind::CompileAndSave,S->Id);
  TSharedPtr<FJsonObject> Params;
  if(!ParseMaterialJobParams(ParamsJson,Params,3)||
     !Params->TryGetStringField(TEXT("material_path"),S->MaterialPath)||!Params->TryGetStringField(TEXT("graph_type"),S->GraphType)||!Params->TryGetBoolField(TEXT("save"),S->Save))
  {Fail(S->Work.Result,TEXT("material_async_params_invalid"),TEXT("Normalized material_path/graph_type/save JSON object required."));S->Phase=EAsyncPhase::Final;S->Finalize();}
  else {S->Work.Result.SaveRequested=S->Save;S->Pending();}
 }
 if(S->Phase==EAsyncPhase::Terminal){S->ReleaseSafe();return Created;}if(S->Phase!=EAsyncPhase::Quarantined)S->Hook();return Created;
}
bool CancelCompileAndSaveContinuation(UCubelessMaterialPostprocessJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!ActiveCompileJob()||ActiveCompileJob()->RawJob!=Job)return false;
 auto S=ActiveCompileJob();if(S->Phase==EAsyncPhase::Terminal||S->Phase==EAsyncPhase::Quarantined)return false;
 S->Work.CancelRequested=true;S->Work.CancelReason=Reason.Left(1024);S->Unhook();if(S->Executing)return true;
 if(!S->PublicationSafe()){S->Quarantine(TEXT("Material cancellation retained unsafe state without new SDK/publication."));return true;}
 {TGuardValue<bool> Busy(S->Executing,true);S->Work.Error(TEXT("material_async_cancelled"),S->Work.CancelReason);S->Phase=EAsyncPhase::Final;S->Finalize();}
 S->ReleaseSafe();return true;
}
void ShutdownCompileAndSaveContinuation(const FString& Reason)
{
 auto S=ActiveCompileJob();if(!S)return;S->Work.CancelRequested=true;S->Work.CancelReason=Reason.Left(1024);S->Unhook();if(S->Executing)return;
 if(!S->PublicationSafe()){S->Quarantine(TEXT("Module shutdown retained unsafe Material context."));return;}
 CancelCompileAndSaveContinuation(S->RawJob,S->Work.CancelReason);
}
bool HasCompileAndSaveContinuationLease(){return AsyncLease;}

}
