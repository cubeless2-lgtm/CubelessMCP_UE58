#include "Compatibility/UE58/CubelessMaterialCollectionParametersApi.h"
#include "Serialization/JsonWriter.h"
#include "Materials/MaterialFunctionInterface.h"
#include "CubelessMaterialPostprocessJob.h"
#include "Operations/CubelessVariableOperations.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonReader.h"
#include "Misc/CoreDelegates.h"
#include "Containers/Ticker.h"
#include "Compatibility/UE58/CubelessMaterialApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/World.h"
#include "HAL/FileManager.h"
#include "MaterialEditingLibrary.h"
#include "MaterialShaderPrecompileMode.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpressionCollectionParameter.h"
#include "Materials/MaterialExpressionParameter.h"
#include "Materials/MaterialExpressionScalarParameter.h"
#include "Materials/MaterialExpressionVectorParameter.h"
#include "Materials/MaterialParameterCollection.h"
#include "Materials/MaterialParameterCollectionInstance.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "RHIGlobals.h"
#include "RHIShaderPlatform.h"
#include "ScopedTransaction.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Compatibility/UE58/CubelessMaterialPostprocessAsyncApi.h"

namespace Cubeless::UE58::MaterialCollectionParameters
{
namespace
{
using FResult = Variable::FResult;
bool Fail(FResult& R, const TCHAR* Code, const FString& Error)
{ R.bSuccess = false; R.ErrorCode = Code; R.Error = Error; return false; }
void Count(FResult& R, const TCHAR* Key)
{ R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
bool Loaded(const UObject* Object)
{ return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool HasNul(const FString& Text)
{ for (int32 Index=0; Index<Text.Len(); ++Index) { if (Text[Index]==TCHAR('\0')) { return true; } } return false; }
bool Finite(const FLinearColor& Value)
{ return FMath::IsFinite(Value.R) && FMath::IsFinite(Value.G) && FMath::IsFinite(Value.B) && FMath::IsFinite(Value.A); }
TSharedRef<FJsonObject> Color(const FLinearColor& Value)
{ auto Row=MakeShared<FJsonObject>(); Row->SetNumberField(TEXT("r"),Value.R); Row->SetNumberField(TEXT("g"),Value.G); Row->SetNumberField(TEXT("b"),Value.B); Row->SetNumberField(TEXT("a"),Value.A); return Row; }

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
// HZ: these concrete continuation checks are inactive on the existing synchronous routes.
bool DeferredRunning=false,DeferredLease=false;
struct FDeferredWork;
FDeferredWork* CurrentDeferredWork=nullptr;
bool ContinueDeferred();
void NoteDeferredTargetSDK();
bool OwnDeferredObject(UObject* Value);
bool RetireDeferredObject(UObject* Value);
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

struct FContext
{
    TSharedRef<FJsonObject> Params=MakeShared<FJsonObject>();
    Material::FState Selected;
    UMaterial* MaterialAsset=nullptr;
    TStrongObjectPtr<UWorld> World;
    TArray<TStrongObjectPtr<UMaterialExpression>> Original, Created;
    TArray<UMaterialExpression*> OriginalPlain;
    TArray<TStrongObjectPtr<UMaterialParameterCollection>> Collections;
    TArray<TStrongObjectPtr<UMaterialParameterCollectionInstance>> Instances;
    bool WasDirty=false;
};
bool Disk(UObject* Asset,TSharedRef<FJsonObject> Row)
{
    FString File; if (!FPackageName::TryConvertLongPackageNameToFilename(Asset->GetOutermost()->GetName(),File,FPackageName::GetAssetPackageExtension())) { return false; }
    File=FPaths::ConvertRelativePathToFull(File); const int64 Bytes=IFileManager::Get().FileSize(*File);
    Row->SetStringField(TEXT("path"),File); Row->SetBoolField(TEXT("exists"),Bytes>=0); Row->SetNumberField(TEXT("bytes"),FMath::Max<int64>(0,Bytes)); Row->SetStringField(TEXT("sha1"),TEXT(""));
    if (Bytes<0) { return true; } TArray<uint8> Data;
    if (Bytes>64*1024*1024 || !FFileHelper::LoadFileToArray(Data,*File) || Data.Num()!=Bytes) { return false; }
    uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Data.GetData(),Data.Num(),Hash); Row->SetStringField(TEXT("sha1"),BytesToHex(Hash,FSHA1::DigestSize)); return true;
}
bool Capture(FContext& C,FResult& R,const TCHAR* Key)
{
    auto Row=MakeShared<FJsonObject>(); R.Data->SetObjectField(Key,Row); Row->SetBoolField(TEXT("complete"),false);
    if(!ContinueDeferred())return false;
    Material::FState Current; FString Error; Count(R,TEXT("material_capture_calls"));NoteDeferredTargetSDK();
    const bool OK=Material::CaptureState(Material::EOperation::ListNodes,C.Params,512,Current,Error);Count(R,TEXT("material_capture_returns"));if(!ContinueDeferred())return false;
    if (Current.Snapshot.IsValid()) { Row->SetObjectField(TEXT("graph"),Current.Snapshot.ToSharedRef()); }
    if (Loaded(Current.Asset.Get())) { Row->SetStringField(TEXT("asset_path"),Current.AssetPath); Row->SetBoolField(TEXT("dirty"),Current.Asset->GetOutermost()->IsDirty()); }
    if (!OK || !Loaded(Current.Asset.Get())) { R.bChangeStateKnown=!R.bMutationAttempted; return Fail(R,TEXT("collection_parameters_observation_incomplete"),Error); }
    if (!C.Selected.Asset.IsValid()) { C.Selected=MoveTemp(Current); C.MaterialAsset=Cast<UMaterial>(C.Selected.Asset.Get()); C.Params->SetStringField(TEXT("material_path"),C.Selected.AssetPath); }
    else if (Current.Asset.Get()!=C.Selected.Asset.Get() || Current.AssetPath!=C.Selected.AssetPath) { R.bChangeStateKnown=!R.bMutationAttempted; return Fail(R,TEXT("collection_parameters_target_relocated"),TEXT("Current target differs; no retry.")); }
    if(!ContinueDeferred())return false;
    auto File=MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"),File);
    if (!Disk(C.Selected.Asset.Get(),File)) { R.bChangeStateKnown=!R.bMutationAttempted; return Fail(R,TEXT("collection_parameters_disk_incomplete"),TEXT("Bounded file observation failed; available prefix retained.")); }
    Row->SetBoolField(TEXT("complete"),true); return true;
}
// Original helpers: GUID first; name fallback only without opposite-type ambiguity.
const FCollectionScalarParameter* Scalar(const UMaterialExpressionCollectionParameter* Node)
{
    for (const auto& Parameter:Node->Collection->ScalarParameters) { if (Parameter.Id==Node->ParameterId) { return &Parameter; } }
    const FCollectionScalarParameter* Found=nullptr;
    for (const auto& Parameter:Node->Collection->ScalarParameters) { if (Parameter.ParameterName==Node->ParameterName) { Found=&Parameter; break; } }
    for (const auto& Parameter:Node->Collection->VectorParameters) { if (Parameter.ParameterName==Node->ParameterName) { return nullptr; } }
    return Found;
}
const FCollectionVectorParameter* Vector(const UMaterialExpressionCollectionParameter* Node)
{
    for (const auto& Parameter:Node->Collection->VectorParameters) { if (Parameter.Id==Node->ParameterId) { return &Parameter; } }
    const FCollectionVectorParameter* Found=nullptr;
    for (const auto& Parameter:Node->Collection->VectorParameters) { if (Parameter.ParameterName==Node->ParameterName) { Found=&Parameter; break; } }
    for (const auto& Parameter:Node->Collection->ScalarParameters) { if (Parameter.ParameterName==Node->ParameterName) { return nullptr; } }
    return Found;
}
struct FPlan
{
    UMaterialExpressionCollectionParameter* Source=nullptr;
    UMaterialExpression* Replacement=nullptr;
    FName Name;
    FString Description;
    float ScalarValue=0;
    FLinearColor VectorValue=FLinearColor::Black;
    bool IsScalar=false,RuntimeResolved=false;
    TSharedRef<FJsonObject> Row=MakeShared<FJsonObject>();
};
struct FLink
{
    UMaterialExpression* Owner=nullptr;
    UMaterialExpression* From=nullptr;
    int32 Index=0,Output=0,Mask=0,R=0,G=0,B=0,A=0;
    FName Name;
    bool Property=false;
};
FLink Link(UMaterialExpression* Owner,int32 Index,bool Property,const FExpressionInput* Input)
{ return {Owner,Input->Expression,Index,Input->OutputIndex,Input->Mask,Input->MaskR,Input->MaskG,Input->MaskB,Input->MaskA,Input->InputName,Property}; }
bool ObserveLinks(FContext& C,FResult& R,TArray<FLink>& Links)
{
    for (const auto& Node:C.Original)
    {
        for (FExpressionInputIterator It(Node.Get());It;++It)
        { Count(R,TEXT("expression_input_iterator_visits")); if (Links.Num()>=8192) { return Fail(R,TEXT("collection_parameters_input_budget"),TEXT("Input bound exceeded before graph writer.")); } Links.Add(Link(Node.Get(),It.Index,false,It.Input)); }
    }
    for (int32 Index=0;Index<static_cast<int32>(MP_MAX);++Index)
    { Count(R,TEXT("material_property_input_getter_calls")); if (auto* Input=C.MaterialAsset->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Index))) { Links.Add(Link(nullptr,Index,true,Input)); } }
    return true;
}

struct FDeferredIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Effective;
 FString Path;EObjectFlags OriginalFlags=RF_NoFlags;bool Retired=false;
 explicit FDeferredIdentity(UObject* Value):Hold(Value),Object(Value),Outer(Value->GetOuter()),Class(Value->GetClass()),Package(Value->GetOutermost()),Effective(Value->GetPackage()),Path(Value->GetPathName()),OriginalFlags(Value->GetFlags()){}
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
 TArray<FPlan> Plans;TArray<FLink> Links;TMap<UMaterialExpression*,UMaterialExpression*> Replacements;
 TSharedPtr<FJsonObject> Authored;
 EShaderPlatform Platform=GMaxRHIShaderPlatform;TOptional<int32> Quality;
 bool Bound=false,Ready=false,Changed=false,TopologyWriting=true,CancelRequested=false,TargetSDKEntered=false,SaveDone=false,GuardFailed=false;
 FString CancelReason;
 bool PublicationSafe()const
 {
  if(!DeferredGlobals()||Editor.Get()!=GEditor)return false;
  UWorld* CurrentWorld=GEditor->GetEditorWorldContext().World();return DeferredLive(CurrentWorld)&&!CurrentWorld->bIsTearingDown&&World.Get()==CurrentWorld;
 }
 bool Error(const TCHAR* Code,const FString& Message)
 {GuardFailed=true;Result.bChangeStateKnown=false;Result.bPostconditionPassed=false;return Fail(Result,Code,Message);}
 bool Own(UObject* Value)
 {
  if(!Value)return true;if(!DeferredLive(Value))return Error(TEXT("collection_parameters_async_unreadable"),TEXT("Captured authoring/reference object is not live."));
  for(const auto& Object:Objects)if(Object->Object.Get()==Value&&!Object->Retired)return true;
  Objects.Add(MakeUnique<FDeferredIdentity>(Value));return true;
 }
 bool BindRoot()
 {
  UObject* Root=Context.Selected.Asset.Get();if(!Root)return true;
  if(!Own(Root))return false;
  if(!Context.MaterialAsset)return true;
  if(Bound)return true;
  for(const auto& Ptr:Context.MaterialAsset->GetExpressions())
  {
   UMaterialExpression* Expression=Ptr.Get();if(!Expression||!Own(Expression)||Expression->GetOuter()!=Root)return Error(TEXT("collection_parameters_async_expression_owner"),TEXT("Current physical expressions must be direct-owned by the exact target."));
   Physical.Add(Expression);Guids.Add(Expression,Expression->MaterialExpressionGuid);
   if(auto* Call=Cast<UMaterialExpressionMaterialFunctionCall>(Expression))
   {UMaterialFunctionInterface* Source=Call->MaterialFunction;if(!Own(Source))return false;FunctionRoutes.Add(Call,Source);if(Source)FunctionIds.Add(Source,Source->StateId);}
  }
  Bound=true;return true;
 }

 bool Retire(UObject* Value)
 {
  if(!PublicationSafe())return Error(TEXT("collection_parameters_async_retirement_unsafe"),TEXT("Returned deletion lost Editor/world; removed object is not observed again."));
  // Match captured addresses only; no old UObject method is called after DeleteMaterialExpression.
  for(auto& Object:Objects)if(Object->Hold.Get()==Value){Object->Retired=true;Object->Hold.Reset();}
  for(int32 Index=0;Index<Context.OriginalPlain.Num();++Index)if(Context.OriginalPlain[Index]==Value)Context.Original[Index].Reset();
  return true;
 }
 bool FreezeTopology()
 {
  if(!Admit())return false;
  Physical.Reset();Guids.Reset();
  for(const auto& Ptr:Context.MaterialAsset->GetExpressions())
  {auto* Expression=Ptr.Get();if(!Own(Expression)||Expression->GetOuter()!=Context.Selected.Asset.Get())return Error(TEXT("collection_parameters_async_expression_owner"),TEXT("Current authored expression owner changed."));Physical.Add(Expression);Guids.Add(Expression,Expression->MaterialExpressionGuid);}
  TopologyWriting=false;return Admit();
 }
 bool Admit()
 {
  if(GuardFailed)return false;
  if(!PublicationSafe()||CancelRequested||MaterialPostprocessAsync::IsStopping())return Error(TEXT("collection_parameters_async_context_stopped"),TEXT("Exact Editor/world/GC/exit/cancel admission failed; no new compiler/save."));
  if(Platform!=GMaxRHIShaderPlatform)return Error(TEXT("collection_parameters_async_shader_platform_changed"),TEXT("Current shader platform changed during owned continuation."));
  for(const auto& Object:Objects)if(!Object->Exact())return Error(TEXT("collection_parameters_async_identity_changed"),TEXT("Captured class/outer/effective package/path changed."));
  if(!BindRoot())return false;
  if(!Bound)return true;
  if(TopologyWriting)return true;
  const auto Current=Context.MaterialAsset->GetExpressions();if(Current.Num()!=Physical.Num())return Error(TEXT("collection_parameters_async_expression_membership_changed"),TEXT("Physical expression count changed across callback/tick."));
  for(int32 Index=0;Index<Physical.Num();++Index)
  {auto* Expression=Physical[Index];if(Current[Index].Get()!=Expression||Expression->MaterialExpressionGuid!=Guids.FindChecked(Expression))return Error(TEXT("collection_parameters_async_expression_membership_changed"),TEXT("Expression order/GUID changed across callback/tick."));}
  for(const auto& Route:FunctionRoutes)if(Route.Key->MaterialFunction!=Route.Value)return Error(TEXT("collection_parameters_async_function_route_changed"),TEXT("Referenced Function pointer changed."));
  for(const auto& Source:FunctionIds)if(Source.Key->StateId!=Source.Value)return Error(TEXT("collection_parameters_async_function_source_changed"),TEXT("Referenced Function source StateId changed."));
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
bool OwnDeferredObject(UObject* Value){return !CurrentDeferredWork||CurrentDeferredWork->Own(Value);}
bool RetireDeferredObject(UObject* Value){return !CurrentDeferredWork||CurrentDeferredWork->Retire(Value);}

bool Verify(FContext& C,FResult& R,const TArray<FPlan>& Plans,const TArray<FLink>& Links,const TMap<UMaterialExpression*,UMaterialExpression*>& Replacements)
{
    if(!ContinueDeferred())return false;
    const auto Current=C.MaterialAsset->GetExpressions();int32 Index=0;
    for(int32 OldIndex=0;OldIndex<C.Original.Num();++OldIndex)
    {UMaterialExpression* Old=C.OriginalPlain.IsValidIndex(OldIndex)?C.OriginalPlain[OldIndex]:C.Original[OldIndex].Get();if(Replacements.Contains(Old)){continue;}if(!Current.IsValidIndex(Index)||Current[Index++].Get()!=Old) { return Fail(R,TEXT("collection_parameters_topology_unverified"),TEXT("Unreplaced expression identity/order differs.")); } }
    for (const auto& New:C.Created)
    { if (!Current.IsValidIndex(Index) || Current[Index++].Get()!=New.Get()) { return Fail(R,TEXT("collection_parameters_topology_unverified"),TEXT("Created expression identity/order differs.")); } }
    if (Index!=Current.Num()) { return Fail(R,TEXT("collection_parameters_topology_unverified"),TEXT("Unexpected extra/missing expression after callbacks.")); }
    for (const auto& Plan:Plans)
    {
        if (!Plan.Replacement) { continue; }
        auto* Parameter=Cast<UMaterialExpressionParameter>(Plan.Replacement);
        if (!Loaded(Parameter) || Parameter->GetOuter()!=C.MaterialAsset || Parameter->ParameterName!=Plan.Name || Parameter->Group!=FName(TEXT("Baked MPC")) || Parameter->Desc!=Plan.Description)
        { return Fail(R,TEXT("collection_parameters_value_unverified"),TEXT("Current generated parameter identity/name/group/description differs.")); }
        if (Plan.IsScalar)
        { const auto* Node=Cast<UMaterialExpressionScalarParameter>(Parameter); if (!Node || Node->DefaultValue!=Plan.ScalarValue) { return Fail(R,TEXT("collection_parameters_value_unverified"),TEXT("Scalar default differs.")); } }
        else
        { const auto* Node=Cast<UMaterialExpressionVectorParameter>(Parameter); if (!Node || Node->DefaultValue!=Plan.VectorValue) { return Fail(R,TEXT("collection_parameters_value_unverified"),TEXT("Vector default differs.")); } }
    }
    for (const auto& Old:Links)
    {
        if (Old.Owner && Replacements.Contains(Old.Owner)) { continue; }
        FExpressionInput* Input=nullptr;
        if (Old.Property) { Count(R,TEXT("material_property_input_getter_calls")); Input=C.MaterialAsset->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Old.Index)); }
        else { Count(R,TEXT("verification_input_getter_calls")); Input=Old.Owner->GetInput(Old.Index); }
        UMaterialExpression* Wanted=Old.From; int32 Output=Old.Output;
        if (auto* Replacement=Replacements.Find(Wanted)) { Wanted=*Replacement; Output=0; }
        if (!Input || Input->Expression!=Wanted || Input->OutputIndex!=Output || Input->InputName!=Old.Name || Input->Mask!=Old.Mask || Input->MaskR!=Old.R || Input->MaskG!=Old.G || Input->MaskB!=Old.B || Input->MaskA!=Old.A)
        { return Fail(R,TEXT("collection_parameters_links_unverified"),TEXT("Current input link/output/name/masks differ; available prefix retained.")); }
    }
    R.Data->SetBoolField(TEXT("selected_topology_values_links_verified"),true); return true;
}
// CQ uses only the small public resource observer derived from frozen CP, not its operation framework.
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
        if(CurrentDeferredWork){Count(R,TEXT("material_resource_queries"));FMaterialResource* CurrentResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentResource!=Resource) return CurrentDeferredWork->Error(TEXT("material_async_resource_replaced"),TEXT("Current resource changed during observation."));UMaterialInterface* ResourceOwner=Resource->GetMaterialInterface();if(!ContinueDeferred())return false;CurrentResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentResource!=Resource||ResourceOwner!=C.MaterialAsset)return CurrentDeferredWork->Error(TEXT("collection_parameters_async_resource_replaced"),TEXT("Current owner/resource role changed; no old resource dereference."));const int32 Quality=static_cast<int32>(Resource->GetQualityLevel());if(CurrentDeferredWork->Quality.IsSet()&&CurrentDeferredWork->Quality.GetValue()!=Quality)return CurrentDeferredWork->Error(TEXT("collection_parameters_async_quality_changed"),TEXT("Current default-quality resource changed."));CurrentDeferredWork->Quality=Quality;}
        const auto CompileErrors=Resource->GetCompileErrors();int32 Chars=0;
        Row->SetBoolField(TEXT("compilation_finished"), Finished);
        for (const FString& Error : CompileErrors)
        { if (Errors.Num() >= 256 || Error.Len() > 1048576 || Chars > 1048576 - Error.Len()) { return Fail(R, TEXT("collection_parameters_compile_status_incomplete"), TEXT("Compile error observation bound exceeded.")); } Chars += Error.Len(); Errors.Add(MakeShared<FJsonValueString>(Error)); Row->SetArrayField(TEXT("compile_errors"), Errors); }
        const FMaterialShaderMap* Map = Resource->GetGameThreadShaderMap(); const bool Complete = Resource->IsGameThreadShaderMapComplete(), Valid = Resource->HasValidGameThreadShaderMap();
        Row->SetBoolField(TEXT("game_thread_shader_map_complete"), Complete); Row->SetBoolField(TEXT("has_valid_game_thread_shader_map"), Valid); Row->SetBoolField(TEXT("game_thread_shader_map_present"), Map != nullptr);
        if (Map) { Row->SetBoolField(TEXT("shader_map_compilation_finalized"), Map->IsCompilationFinalized()); Row->SetBoolField(TEXT("shader_map_compiled_successfully"), Map->CompiledSuccessfully()); }
        else { Row->SetField(TEXT("shader_map_compilation_finalized"), MakeShared<FJsonValueNull>()); Row->SetField(TEXT("shader_map_compiled_successfully"), MakeShared<FJsonValueNull>()); }
        const bool StrictReady=Complete&&Valid&&Map&&Map->IsCompilationFinalized()&&Map->CompiledSuccessfully();const bool HadErrorOrCompiling=C.MaterialAsset->IsCompilingOrHadCompileError(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(CurrentDeferredWork){FMaterialResource* FinalResource=C.MaterialAsset->GetMaterialResource(GMaxRHIShaderPlatform);if(!ContinueDeferred())return false;if(FinalResource!=Resource)return CurrentDeferredWork->Error(TEXT("material_async_resource_changed_during_status"),TEXT("Current resource changed during final Editor criterion observation."));}Row->SetBoolField(TEXT("original_compiling_or_had_error"),HadErrorOrCompiling);Row->SetBoolField(TEXT("strict_render_map_criterion"),StrictReady);Row->SetBoolField(TEXT("original_Editor_completion_criterion"),Finished&&CompileErrors.IsEmpty()&&!HadErrorOrCompiling);
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
}

// IA actual followup: DeleteMaterialExpression removes the physical expression and
// marks it Garbage without renaming it out of its package. The package hash can
// therefore still return that exact deleted object before GC. Only this request's
// already verified source retirement can be separated from live ownership; no
// class-wide, name-prefix or general Garbage exemption is permitted.
const FDeferredIdentity* ExactRetiredPackageExport(UObject* Export,UObject* Root,UPackage* Package)
{
 auto& W=*CurrentDeferredWork;
 if(!W.Ready||W.TopologyWriting||!W.Authored.IsValid()||!Export||W.Context.MaterialAsset!=Root)return nullptr;
 const FDeferredIdentity* Captured=nullptr;
 for(const auto& Identity:W.Objects)
  if(Identity->Retired&&Identity->Object.Get(true)==Export){Captured=Identity.Get();break;}
 // Get(true) checks the original object index/serial even when Garbage. Never
 // dereference the raw Source retained by Plans/Replacements after the callback.
 if(!Captured||Captured->Class.Get()!=UMaterialExpressionCollectionParameter::StaticClass()||Captured->Outer.Get()!=Root||
    Captured->Package.Get()!=Package||Captured->Effective.Get()!=Package)return nullptr;
 if(Export->GetClass()!=Captured->Class.Get()||Export->GetOuter()!=Root||Export->GetOutermost()!=Package||Export->GetPackage()!=Package||
    Export->GetFlags()!=(Captured->OriginalFlags|RF_MirroredGarbage)||!Export->HasAnyInternalFlags(EInternalObjectFlags::Garbage)||
    !Export->GetPathName().Equals(Captured->Path,ESearchCase::CaseSensitive))return nullptr;
 bool Planned=false;
 for(const auto& Plan:W.Plans)
 {
  const auto* Replacement=W.Replacements.Find(Plan.Source);
  if(static_cast<UObject*>(Plan.Source)==Export&&Plan.Replacement&&Replacement&&*Replacement==Plan.Replacement&&W.Physical.Contains(Plan.Replacement))
  {Planned=true;break;}
 }
 if(!Planned)return nullptr;
 for(const auto& CurrentExpression:W.Context.MaterialAsset->GetExpressions())if(CurrentExpression.Get()==Export)return nullptr;
 return Captured;
}
bool ExportDiagnosticReadable(UObject* Value)
{
 if(!Value)return false;
 TWeakObjectPtr<UObject> Witness(Value);
 if(Witness.Get(true)!=Value||Value->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed))return false;
 // A current hash member may be Garbage, but every parent must remain live.
 TSet<const UObject*> Seen;Seen.Add(Value);const UObject* Parent=Value->GetOuter();
 for(int32 Depth=0;Parent&&Depth<64;++Depth)
 {if(!DeferredLive(Parent)||Seen.Contains(Parent))return false;Seen.Add(Parent);Parent=Parent->GetOuter();}
 return Parent==nullptr;
}
void FirstPackageExportFailure(FContext& C,FResult& R,UObject* Export,int32 Index,int32 Count)
{
 if(R.Data->HasField(TEXT("first_package_export_failure")))return;
 auto Row=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("first_package_export_failure"),Row);
 bool TextTruncated=false;auto PutPath=[&](const TCHAR* Key,const FString& Value){TextTruncated|=Value.Len()>1024;Row->SetStringField(Key,Value.Left(1024));Row->SetBoolField(TEXT("path_text_truncated"),TextTruncated);};
 Row->SetStringField(TEXT("stage"),R.Stage.Left(256));Row->SetNumberField(TEXT("export_index"),Index);Row->SetNumberField(TEXT("export_count"),Count);
 Row->SetBoolField(TEXT("stage_truncated"),R.Stage.Len()>256);PutPath(TEXT("expected_root"),C.Selected.AssetPath);
 Row->SetStringField(TEXT("reason"),TEXT("Current export is neither a live exact target/subtree member nor this request's exact verified deleted CollectionParameter Garbage."));
 Row->SetBoolField(TEXT("current_identity_available"),false);Row->SetBoolField(TEXT("captured_retired_serial_match"),false);
 if(!CurrentDeferredWork->PublicationSafe()||!Export)return;
 TWeakObjectPtr<UObject> Witness(Export);if(Witness.Get(true)!=Export)return;
 Row->SetNumberField(TEXT("flags"),static_cast<uint32>(Export->GetFlags()));
 Row->SetBoolField(TEXT("garbage"),Export->HasAnyInternalFlags(EInternalObjectFlags::Garbage));
 for(const auto& Identity:CurrentDeferredWork->Objects)if(Identity->Retired&&Identity->Object.Get(true)==Export)
 {Row->SetBoolField(TEXT("captured_retired_serial_match"),true);PutPath(TEXT("captured_path"),Identity->Path);Row->SetNumberField(TEXT("captured_flags"),static_cast<uint32>(Identity->OriginalFlags));break;}
 if(!ExportDiagnosticReadable(Export))return;
 UClass* CurrentClass=Export->GetClass();UObject* CurrentOuter=Export->GetOuter();UPackage* CurrentPackage=Export->GetOutermost();UPackage* EffectivePackage=Export->GetPackage();
 if(!DeferredLive(CurrentClass)||!ExportDiagnosticReadable(CurrentClass)||!ExportDiagnosticReadable(CurrentPackage)||!ExportDiagnosticReadable(EffectivePackage))return;
 Row->SetBoolField(TEXT("current_identity_available"),true);PutPath(TEXT("path"),Export->GetPathName());
 PutPath(TEXT("class"),CurrentClass->GetPathName());PutPath(TEXT("outer"),CurrentOuter?CurrentOuter->GetPathName():FString());
 PutPath(TEXT("outermost_package"),CurrentPackage->GetPathName());PutPath(TEXT("effective_package"),EffectivePackage->GetPathName());
 Row->SetBoolField(TEXT("live"),DeferredLive(Export));Row->SetBoolField(TEXT("in_exact_target"),Export==C.Selected.Asset.Get()||Export->IsIn(C.Selected.Asset.Get()));
}
bool DeferredPackageOwned(FContext& C,FResult& R)
{
 if(!CurrentDeferredWork)return true;
 if(!ContinueDeferred())return false;
 UObject* Root=C.Selected.Asset.Get();UPackage* Package=Root->GetOutermost();TArray<UObject*> Exports;
 GetObjectsWithPackage(Package,Exports,EGetObjectsFlags::IncludeNestedObjects);if(!ContinueDeferred())return false;
 if(Exports.Num()>4096)return CurrentDeferredWork->Error(TEXT("collection_parameters_async_package_bound"),TEXT("Current target package export budget exceeded before/after exact save."));
 TArray<TSharedPtr<FJsonValue>> RetiredRows;
 for(int32 Index=0;Index<Exports.Num();++Index)
 {
  UObject* Export=Exports[Index];
  if(DeferredLive(Export)&&Export->GetOutermost()==Package&&Export->GetPackage()==Package&&(Export==Root||Export->IsIn(Root)))continue;
  if(const FDeferredIdentity* Retired=ExactRetiredPackageExport(Export,Root,Package))
  {
   auto Row=MakeShared<FJsonObject>();Row->SetStringField(TEXT("captured_path"),Retired->Path.Left(1024));Row->SetStringField(TEXT("class"),TEXT("/Script/Engine.MaterialExpressionCollectionParameter"));
   Row->SetBoolField(TEXT("captured_path_truncated"),Retired->Path.Len()>1024);
   Row->SetBoolField(TEXT("captured_index_serial_and_identity_match"),true);Row->SetBoolField(TEXT("deleted_source_absent_from_current_expressions"),true);Row->SetBoolField(TEXT("garbage_only_flag_added"),true);
   RetiredRows.Add(MakeShared<FJsonValueObject>(Row));continue;
  }
  FirstPackageExportFailure(C,R,Export,Index,Exports.Num());
  return CurrentDeferredWork->Error(TEXT("collection_parameters_async_foreign_package_export"),TEXT("Only exact live target/subtree or this request's verified deleted CollectionParameter Garbage may pass ownership; actual mutation/save prefix is retained."));
 }
 R.Data->SetArrayField(TEXT("verified_deleted_expression_package_exports"),RetiredRows);
 R.Data->SetNumberField(TEXT("verified_deleted_expression_package_export_count"),RetiredRows.Num());
 R.Data->SetBoolField(TEXT("general_garbage_or_foreign_export_allowed"),false);
 return true;
}

bool CompleteParameterSave(FContext& C,FResult& R,const TArray<FPlan>& Plans,const TArray<FLink>& Links,const TMap<UMaterialExpression*,UMaterialExpression*>& Replacements,bool Save,bool Changed)
{
    if(Save&&Changed)
    {
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;
        if(CurrentDeferredWork&&CurrentDeferredWork->SaveDone)return CurrentDeferredWork->Error(TEXT("collection_parameters_async_save_replay"),TEXT("Save SDK was already entered; no second save."));
        R.Stage=TEXT("prepare_save");R.bChangeStateKnown=false;R.Compiled.Reset();
        for(const TCHAR* Key:{TEXT("compiled"),TEXT("compilation_finished"),TEXT("compile_errors"),TEXT("compile_error_count")})R.Data->SetField(Key,MakeShared<FJsonValueNull>());
        Count(R,TEXT("explicit_asset_modify_calls"));C.Selected.Asset->Modify();if(!ContinueDeferred())return false;
        Count(R,TEXT("mark_package_dirty_calls"));C.Selected.Asset->MarkPackageDirty();if(!ContinueDeferred())return false;
        Count(R,TEXT("explicit_post_edit_change_calls"));C.Selected.Asset->PostEditChange();if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;
        R.Stage=TEXT("save");R.bSaveAttempted=true;auto Call=MakeShared<FJsonObject>();Call->SetStringField(TEXT("asset_path"),C.Selected.AssetPath);Call->SetBoolField(TEXT("attempted"),true);Call->SetBoolField(TEXT("returned"),false);Call->SetBoolField(TEXT("only_if_dirty"),false);R.Data->SetArrayField(TEXT("save_calls"),{MakeShared<FJsonValueObject>(Call)});
        if(CurrentDeferredWork)CurrentDeferredWork->SaveDone=true;Count(R,TEXT("save_attempt_count"));
        const bool SavedNow=UEditorAssetLibrary::SaveLoadedAsset(C.Selected.Asset.Get(),false);R.bSaved|=SavedNow;Count(R,TEXT("save_return_count"));Call->SetBoolField(TEXT("returned"),true);Call->SetBoolField(TEXT("saved"),SavedNow);R.Data->SetBoolField(TEXT("saved"),R.bSaved);
        if(!ContinueDeferred()||!DeferredPackageOwned(C,R))return false;R.Data->SetBoolField(TEXT("dirty_after_replace"),C.Selected.Asset->GetOutermost()->IsDirty());
        R.Stage=TEXT("observe_save");if(!Capture(C,R,TEXT("after_save_observation"))||!Verify(C,R,Plans,Links,Replacements))return false;
        const auto File=R.Data->GetObjectField(TEXT("after_save_observation"))->GetObjectField(TEXT("disk"));const bool DiskOK=R.bSaved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!C.Selected.Asset->GetOutermost()->IsDirty();R.Data->SetBoolField(TEXT("save_disk_verified"),DiskOK);
        if(!ObserveCompile(C,R,TEXT("final_compile_status")))return false;
        if(!R.Compiled.IsSet()&&CurrentDeferredWork){R.bSuccess=false;R.bPostconditionPassed=false;R.bChangeStateKnown=false;R.Stage=TEXT("await_resource_after_saved_true");return true;}
        if(!R.Compiled.IsSet())return Fail(R,TEXT("collection_parameters_pending_after_save"),TEXT("Real saved prefix retained; final resource pending/incomplete, no retry."));
        R.bChangeStateKnown=true;if(!DiskOK||!R.Compiled.GetValue())return Fail(R,TEXT("collection_parameters_save_or_final_compile_failed"),TEXT("Actual save/disk/final compile criterion failed; prefix retained."));
    }
    R.bChangeStateKnown=true;R.bChanged=Changed||C.WasDirty!=C.Selected.Asset->GetOutermost()->IsDirty();R.bPostconditionPassed=true;R.bSuccess=true;R.Stage=TEXT("complete");return true;
}

Variable::FResult Replace(const FString& Path,bool Runtime,bool Save)
{
    if((DeferredLease||DeferredRunning)&&!CurrentDeferredWork){FResult Denied;Denied.bSaveRequested=Save;Fail(Denied,TEXT("collection_parameters_async_lease_active"),TEXT("This reference core is owned by an in-flight request; no synchronous replay."));return Denied;}TGuardValue<bool> RunningScope(DeferredRunning,true);
    FResult LocalResult;FContext LocalContext;FResult& R=CurrentDeferredWork?CurrentDeferredWork->Result:LocalResult;FContext& C=CurrentDeferredWork?CurrentDeferredWork->Context:LocalContext;R.bSaveRequested=Save;R.bCompileRequested=true;
    for (const TCHAR* Key:{TEXT("outer_transaction_calls"),TEXT("material_capture_calls"),TEXT("material_capture_returns"),TEXT("editor_world_query_calls"),TEXT("runtime_instance_lookup_calls"),TEXT("runtime_instance_lookup_returns"),TEXT("runtime_scalar_getter_calls"),TEXT("runtime_vector_getter_calls"),TEXT("explicit_asset_modify_calls"),TEXT("create_expression_calls"),TEXT("create_expression_returns"),TEXT("created_expression_count"),TEXT("configured_parameter_field_applications"),TEXT("delete_expression_calls"),TEXT("delete_expression_returns"),TEXT("actual_deleted_collection_parameter_count"),TEXT("expression_input_iterator_visits"),TEXT("verification_input_getter_calls"),TEXT("material_property_input_getter_calls"),TEXT("material_update_context_calls"),TEXT("explicit_pre_edit_change_calls"),TEXT("explicit_post_edit_change_calls"),TEXT("force_recompile_background_calls"),TEXT("mark_package_dirty_calls"),TEXT("material_resource_queries"),TEXT("material_compile_status_queries"),TEXT("save_attempt_count"),TEXT("save_return_count"),TEXT("set_dirty_flag_calls"),TEXT("finish_compilation_calls"),TEXT("shader_manager_pump_calls"),TEXT("explicit_GC_calls"),TEXT("material_editing_library_recompile_calls")}) { R.Data->SetNumberField(Key,0); }
    for (const TCHAR* Key:{TEXT("replaced_nodes"),TEXT("missing_parameters"),TEXT("creation_calls"),TEXT("save_calls"),TEXT("runtime_value_observations")}) { R.Data->SetArrayField(Key,{}); }
    for (const TCHAR* Key:{TEXT("initial_collection_parameter_count"),TEXT("replacement_count"),TEXT("deleted_collection_parameter_count"),TEXT("rewired_expression_input_count"),TEXT("rewired_material_property_count"),TEXT("missing_parameter_count")}) { R.Data->SetNumberField(Key,0); }
    R.Data->SetBoolField(TEXT("use_runtime_values"),Runtime); R.Data->SetBoolField(TEXT("saved"),false); R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),false); R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);
    R.Data->SetBoolField(TEXT("runtime_instance_lazy_creation_possible"),false); R.Data->SetBoolField(TEXT("runtime_world_effects_exhaustively_measured"),false); R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false); R.Data->SetBoolField(TEXT("runtime_or_cold_verified"),false);
    R.Data->SetStringField(TEXT("effect_scope"),TEXT("Original Material-only collection→ordinary Scalar/VectorParameter topology bake on bounded owned /Game graph, including GUID/name ambiguity and Editor-world/default value fallback. Runtime instance lookup can lazily allocate/update scene resources; create/delete/PostEdit/compiler/save have Engine-owned callbacks. No original PIE selection, blocking FinishCompilation, manager pump, GC, full RecompileMaterial editor/child/streaming parity, rollback or automatic retry."));
    R.Data->SetStringField(TEXT("changed_scope"),TEXT("Selected Material topology/parameter values/input links/dirty/disk observations; global runtime instance/render/editor fanout not proven. Pending resource leaves typed changed/compiled null. Logical field/rewire counts are applications, not estimated internal method calls; shared captures can load/query before first capture."));
    if(!ContinueDeferred())return R;
    if(CurrentDeferredWork&&!DeferredNameSafe(Path)){Fail(R,TEXT("collection_parameters_async_name_bound"),TEXT("Material/expanded path must fit public Engine name bound."));return R;}
    if (!IsEditorGameThread() || IsPlaySessionActive() || Path.IsEmpty() || Path.Len()>2048 || HasNul(Path)) { Fail(R,TEXT("collection_parameters_input_or_editor_invalid"),TEXT("Bounded Material path and idle Editor game thread required.")); return R; }
    C.Params->SetStringField(TEXT("material_path"),Path); C.Params->SetStringField(TEXT("graph_type"),TEXT("material"));
    if (!Capture(C,R,TEXT("before_observation"))) { return R; } FString Error;
    if (!C.MaterialAsset || !Material::ValidateMutationTarget(C.Selected,Error) || C.MaterialAsset->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient) || (Save&&!C.MaterialAsset->GetOutermost()->IsFullyLoaded()))
    { Fail(R,TEXT("collection_parameters_material_target_required"),TEXT("Ordinary loaded /Game Material only, fully loaded package for save: ")+Error); return R; }
    for (const auto& Ptr:C.MaterialAsset->GetExpressions())
    { if (!Loaded(Ptr.Get()) || Ptr->GetOuter()!=C.MaterialAsset || Ptr->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)) { Fail(R,TEXT("collection_parameters_expression_not_ready"),TEXT("Ordinary direct-owned loaded expressions required.")); return R; } C.Original.Emplace(Ptr.Get());C.OriginalPlain.Add(Ptr.Get()); }
    TArray<FLink> Links; if (!ObserveLinks(C,R,Links)) { return R; }
    C.WasDirty=C.MaterialAsset->GetOutermost()->IsDirty(); R.Data->SetBoolField(TEXT("was_dirty_before_replace"),C.WasDirty); R.Data->SetStringField(TEXT("material"),C.Selected.AssetPath);
    Count(R,TEXT("outer_transaction_calls")); FScopedTransaction Transaction(NSLOCTEXT("CubelessMCP","BakeCollectionParameters","Replace Material Collection Parameters"));
    if (Runtime) { Count(R,TEXT("editor_world_query_calls")); C.World.Reset(GEditor?GEditor->GetEditorWorldContext().World():nullptr); }
    if (C.World.IsValid()&&(!Loaded(C.World.Get()) || C.World->WorldType!=EWorldType::Editor)) { Fail(R,TEXT("collection_parameters_editor_world_required"),TEXT("Only current idle Editor world admitted; original PIE choice remains outside this slice.")); return R; }
    R.Data->SetStringField(TEXT("runtime_world"),C.World.IsValid()?C.World->GetPathName():FString());
    for (const auto& Ptr:C.Original) { if (Cast<UMaterialExpressionCollectionParameter>(Ptr.Get())) { R.Data->SetNumberField(TEXT("initial_collection_parameter_count"),R.Data->GetNumberField(TEXT("initial_collection_parameter_count"))+1); } }
    TArray<FPlan> Plans; TArray<TSharedPtr<FJsonValue>> Missing,ValueRows;
    TMap<UMaterialExpression*,UMaterialExpression*> Replacements; TArray<TSharedPtr<FJsonValue>> Replaced,Creations; bool Modified=false;
    for (const auto& Ptr:C.Original)
    {
        auto* Node=Cast<UMaterialExpressionCollectionParameter>(Ptr.Get()); if (!Node) { continue; }
        if (!Node->Collection) { continue; }
        auto* Collection=Node->Collection.Get();
        if (!Loaded(Collection)||Collection->GetClass()!=UMaterialParameterCollection::StaticClass()||Collection->HasAnyFlags(RF_ClassDefaultObject|RF_ArchetypeObject|RF_Transient)||Collection->ScalarParameters.Num()+Collection->VectorParameters.Num()>4096)
        { Fail(R,TEXT("collection_parameters_collection_not_ready"),TEXT("Loaded ordinary native collection with bounded declarations required; retained runtime prefix may have work.")); return R; }
        if(!OwnDeferredObject(Collection))return R;C.Collections.Emplace(Collection);UMaterialParameterCollectionInstance* Instance=nullptr;
        if (C.World.IsValid())
        { R.bMutationAttempted=true; R.bChangeStateKnown=false; R.Data->SetBoolField(TEXT("runtime_instance_lazy_creation_possible"),true); Count(R,TEXT("runtime_instance_lookup_calls")); Instance=C.World->GetParameterCollectionInstance(Collection); Count(R,TEXT("runtime_instance_lookup_returns"));if(!ContinueDeferred())return R;if(Instance&&!OwnDeferredObject(Instance))return R; if (Instance) { C.Instances.Emplace(Instance); if (!Loaded(Instance)||Instance->GetCollection()!=Collection) { Fail(R,TEXT("collection_parameters_runtime_identity_invalid"),TEXT("Returned runtime instance identity is unavailable; actual lookup prefix retained.")); return R; } } }
        FPlan Plan; Plan.Source=Node; Plan.Name=Node->ParameterName; Plan.Description=FString::Printf(TEXT("Baked from %s.%s"),*Collection->GetPathName(),*Plan.Name.ToString());
        if (const auto* Parameter=Scalar(Node))
        { Plan.IsScalar=true; Plan.ScalarValue=Parameter->DefaultValue; if (Instance) { Count(R,TEXT("runtime_scalar_getter_calls")); float Value=Plan.ScalarValue; Plan.RuntimeResolved=Instance->GetScalarParameterValue(Plan.Name,Value);if(!ContinueDeferred())return R; if (Plan.RuntimeResolved) { Plan.ScalarValue=Value; } } if (!FMath::IsFinite(Plan.ScalarValue)) { Fail(R,TEXT("collection_parameters_value_nonfinite"),TEXT("Scalar value must be finite before graph creation.")); return R; } }
        else if (const auto* VectorParameter=Vector(Node))
        { Plan.VectorValue=VectorParameter->DefaultValue; if (Instance) { Count(R,TEXT("runtime_vector_getter_calls")); FLinearColor Value=Plan.VectorValue; Plan.RuntimeResolved=Instance->GetVectorParameterValue(Plan.Name,Value);if(!ContinueDeferred())return R; if (Plan.RuntimeResolved) { Plan.VectorValue=Value; } } if (!Finite(Plan.VectorValue)) { Fail(R,TEXT("collection_parameters_value_nonfinite"),TEXT("Vector value must be finite before graph creation.")); return R; } }
        else { Missing.Add(MakeShared<FJsonValueString>(Plan.Name.ToString())); R.Data->SetArrayField(TEXT("missing_parameters"),Missing); R.Data->SetNumberField(TEXT("missing_parameter_count"),Missing.Num()); continue; }
        Plan.Row->SetStringField(TEXT("source_node"),Node->GetName()); Plan.Row->SetStringField(TEXT("source_node_path"),Node->GetPathName()); Plan.Row->SetStringField(TEXT("parameter_name"),Plan.Name.ToString()); Plan.Row->SetStringField(TEXT("parameter_type"),Plan.IsScalar?TEXT("scalar"):TEXT("vector")); Plan.Row->SetBoolField(TEXT("used_runtime_value"),Plan.RuntimeResolved);
        if (Plan.IsScalar) { Plan.Row->SetNumberField(TEXT("baked_value"),Plan.ScalarValue); } else { Plan.Row->SetObjectField(TEXT("baked_value"),Color(Plan.VectorValue)); }
        ValueRows.Add(MakeShared<FJsonValueObject>(Plan.Row)); R.Data->SetArrayField(TEXT("runtime_value_observations"),ValueRows);
        R.Stage=TEXT("create_parameter_nodes"); R.bMutationAttempted=true; R.bChangeStateKnown=false;
        if (!Modified) { Count(R,TEXT("explicit_asset_modify_calls")); C.Selected.Asset->Modify();if(!ContinueDeferred())return R; Modified=true; }
        auto Call=MakeShared<FJsonObject>(); Call->SetStringField(TEXT("source_node_path"),Plan.Source->GetPathName()); Call->SetBoolField(TEXT("attempted"),true); Call->SetBoolField(TEXT("returned"),false); Creations.Add(MakeShared<FJsonValueObject>(Call)); R.Data->SetArrayField(TEXT("creation_calls"),Creations);
        Count(R,TEXT("create_expression_calls")); UMaterialExpression* Created=UMaterialEditingLibrary::CreateMaterialExpression(C.MaterialAsset,Plan.IsScalar?UMaterialExpressionScalarParameter::StaticClass():UMaterialExpressionVectorParameter::StaticClass(),Plan.Source->MaterialExpressionEditorX,Plan.Source->MaterialExpressionEditorY);
        Count(R,TEXT("create_expression_returns")); Call->SetBoolField(TEXT("returned"),true); Call->SetBoolField(TEXT("nonnull"),Created!=nullptr);if(!ContinueDeferred())return R;if(Created&&!OwnDeferredObject(Created))return R;
        if (Created) { C.Created.Emplace(Created); Count(R,TEXT("created_expression_count")); Call->SetStringField(TEXT("created_path"),Created->GetPathName()); Call->SetStringField(TEXT("created_class"),Created->GetClass()->GetPathName()); }
        if (!Created) { Missing.Add(MakeShared<FJsonValueString>(Plan.Name.ToString())); R.Data->SetArrayField(TEXT("missing_parameters"),Missing); R.Data->SetNumberField(TEXT("missing_parameter_count"),Missing.Num()); continue; }
        auto* Parameter=Cast<UMaterialExpressionParameter>(Created);
        if (!Loaded(Parameter)||Parameter->GetOuter()!=C.MaterialAsset||Created->GetClass()!=(Plan.IsScalar?UMaterialExpressionScalarParameter::StaticClass():UMaterialExpressionVectorParameter::StaticClass())) { Fail(R,TEXT("collection_parameters_created_node_unverified"),TEXT("Returned allocation retained but current parameter identity/type differs; no cleanup/retry.")); return R; }
        Parameter->ParameterName=Plan.Name; Parameter->Group=TEXT("Baked MPC"); Parameter->Desc=Plan.Description;
        if (Plan.IsScalar) { CastChecked<UMaterialExpressionScalarParameter>(Parameter)->DefaultValue=Plan.ScalarValue; } else { CastChecked<UMaterialExpressionVectorParameter>(Parameter)->DefaultValue=Plan.VectorValue; }
        R.Data->SetNumberField(TEXT("configured_parameter_field_applications"),R.Data->GetNumberField(TEXT("configured_parameter_field_applications"))+4);
        Plan.Replacement=Created; Replacements.Add(Plan.Source,Created); Plan.Row->SetStringField(TEXT("replacement_node"),Created->GetName()); Plan.Row->SetStringField(TEXT("replacement_node_path"),Created->GetPathName()); Replaced.Add(MakeShared<FJsonValueObject>(Plan.Row)); R.Data->SetArrayField(TEXT("replaced_nodes"),Replaced); R.Data->SetNumberField(TEXT("replacement_count"),Replacements.Num()); R.Data->SetNumberField(TEXT("deleted_collection_parameter_count"),Replacements.Num()); Plans.Add(MoveTemp(Plan));
    }
    R.Stage=TEXT("rewire_links"); int32 RewireVisits=0;
    for (const auto& Ptr:C.MaterialAsset->GetExpressions())
    {
        if (!Loaded(Ptr.Get())||Ptr->GetOuter()!=C.MaterialAsset) { Fail(R,TEXT("collection_parameters_post_create_identity_invalid"),TEXT("Current expression identity/readiness changed after public creation; no retry.")); return R; }
        for (FExpressionInputIterator It(Ptr.Get());It;++It)
        { Count(R,TEXT("expression_input_iterator_visits")); if (++RewireVisits>8192) { Fail(R,TEXT("collection_parameters_input_budget"),TEXT("Current input bound exceeded after creation; actual prefix retained.")); return R; } if (auto* Replacement=Replacements.Find(It.Input->Expression)) { It.Input->Expression=*Replacement; It.Input->OutputIndex=0; R.Data->SetNumberField(TEXT("rewired_expression_input_count"),R.Data->GetNumberField(TEXT("rewired_expression_input_count"))+1); } }
    }
    for (int32 Index=0;Index<static_cast<int32>(MP_MAX);++Index)
    { Count(R,TEXT("material_property_input_getter_calls")); if (auto* Input=C.MaterialAsset->GetExpressionInputForProperty(static_cast<EMaterialProperty>(Index))) { if (auto* Replacement=Replacements.Find(Input->Expression)) { Input->Expression=*Replacement; Input->OutputIndex=0; R.Data->SetNumberField(TEXT("rewired_material_property_count"),R.Data->GetNumberField(TEXT("rewired_material_property_count"))+1); } } }
    R.Stage=TEXT("retire_collection_nodes");
    for (const auto& Plan:Plans)
    { if (Plan.Replacement) { Count(R,TEXT("delete_expression_calls")); UMaterialEditingLibrary::DeleteMaterialExpression(C.MaterialAsset,Plan.Source); Count(R,TEXT("delete_expression_returns"));if(!RetireDeferredObject(Plan.Source)||!ContinueDeferred())return R;bool Exists=false; for (const auto& Ptr:C.MaterialAsset->GetExpressions()) { Exists|=Ptr.Get()==Plan.Source; } if (Exists) { Fail(R,TEXT("collection_parameters_retirement_unverified"),TEXT("Original collection node remains after returned delete; prefix retained.")); return R; } Count(R,TEXT("actual_deleted_collection_parameter_count")); } }
    R.Stage=TEXT("observe_topology"); if (!Capture(C,R,TEXT("after_topology_observation"))||!Verify(C,R,Plans,Links,Replacements)) { return R; }
    if(CurrentDeferredWork&&!CurrentDeferredWork->FreezeTopology())return R;
    // Original always recompiles, including zero replacements. No blocking finish or resource pump.
    R.Stage=TEXT("request_compile"); R.bMutationAttempted=true; R.bChangeStateKnown=false; R.bCompileAttempted=true;
    { Count(R,TEXT("material_update_context_calls")); FMaterialUpdateContext Update; Update.AddMaterial(C.MaterialAsset);if(!ContinueDeferred())return R;Count(R,TEXT("explicit_pre_edit_change_calls"));C.Selected.Asset->PreEditChange(nullptr);if(!ContinueDeferred())return R;Count(R,TEXT("explicit_post_edit_change_calls"));C.Selected.Asset->PostEditChange();if(!ContinueDeferred())return R;Count(R,TEXT("force_recompile_background_calls"));C.MaterialAsset->ForceRecompileForRendering(EMaterialShaderPrecompileMode::Background);if(!ContinueDeferred())return R;}
    if(!ContinueDeferred())return R;
    Count(R,TEXT("mark_package_dirty_calls")); C.Selected.Asset->MarkPackageDirty();if(!ContinueDeferred())return R;const bool Changed=!Replacements.IsEmpty();
    if (!Changed&&!C.WasDirty&&C.Selected.Asset->GetOutermost()->IsDirty()) { Count(R,TEXT("set_dirty_flag_calls")); C.Selected.Asset->GetOutermost()->SetDirtyFlag(false);if(!ContinueDeferred())return R; }
    R.Stage=TEXT("observe_compile"); if (!Capture(C,R,TEXT("after_compile_observation"))||!Verify(C,R,Plans,Links,Replacements) ) { return R; }
    if(CurrentDeferredWork){CurrentDeferredWork->Plans=Plans;CurrentDeferredWork->Links=Links;CurrentDeferredWork->Replacements=Replacements;CurrentDeferredWork->Changed=Changed;CurrentDeferredWork->Authored=R.Data->GetObjectField(TEXT("after_compile_observation"))->GetObjectField(TEXT("graph"));CurrentDeferredWork->Ready=true;R.Stage=TEXT("await_current_resource");return R;}
    if(!ObserveCompile(C,R,TEXT("initial_compile_status")))return R;
    R.Data->SetBoolField(TEXT("dirty_after_replace"),C.Selected.Asset->GetOutermost()->IsDirty());
    if (!R.Compiled.IsSet()) { R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),Save&&Changed); Fail(R,TEXT("collection_parameters_compile_pending_or_incomplete"),TEXT("Current resource pending/incomplete; topology/counters retained, no save/wait/retry.")); return R; }
    R.bChangeStateKnown=true; R.bChanged=Changed||C.WasDirty!=C.Selected.Asset->GetOutermost()->IsDirty();
    if (!R.Compiled.GetValue()) { R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&Changed); Fail(R,TEXT("collection_parameters_compile_failed"),TEXT("Observed current resource/error criterion failed; no save.")); return R; }
    CompleteParameterSave(C,R,Plans,Links,Replacements,Save,Changed);return R;
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

enum class EParameterPhase:uint8 {Dispatch,WaitInitial,Save,WaitSaved,Final,Terminal,Quarantined};
struct FParameterJobState:public TSharedFromThis<FParameterJobState>
{
 FDeferredWork Work;TStrongObjectPtr<UCubelessMaterialPostprocessJob> Job{nullptr};TUniquePtr<FDeferredIdentity> JobIdentity;
 UCubelessMaterialPostprocessJob* RawJob=nullptr;FGuid Id=FGuid::NewGuid();FString LastSnapshot,MaterialPath;
 bool Runtime=true,Save=true,Executing=false,Dispatched=false,FinalEncoded=false,FrameMetricsMerged=false;Material::FReadMetricsScope* FrameMetrics=nullptr;
 int64 Revision=0;uint64 LastFrame=MAX_uint64;EParameterPhase Phase=EParameterPhase::Dispatch;FTSTicker::FDelegateHandle Ticker;
 const TCHAR* Operation()const{return TEXT("replace_material_collection_parameters");}
 bool PublicationSafe()const{return Work.PublicationSafe()&&(!JobIdentity||JobIdentity->Exact());}
 void Unhook(){if(Ticker.IsValid()){FTSTicker::GetCoreTicker().RemoveTicker(Ticker);Ticker.Reset();}}
 void Metadata(bool Terminal)
 {
  if(FrameMetrics&&!FrameMetricsMerged){Work.MergeMetrics(FrameMetrics->ToJson());FrameMetricsMerged=true;}
  auto& R=Work.Result;R.Data->SetStringField(TEXT("native_job_id"),Id.ToString(EGuidFormats::DigitsWithHyphens));R.Data->SetNumberField(TEXT("native_checkpoint_revision"),++Revision);
  R.Data->SetBoolField(TEXT("native_job_terminal"),Terminal);R.Data->SetBoolField(TEXT("operation_complete"),Terminal);
  R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("do_not_retry"),true);
  R.Data->SetBoolField(TEXT("async_core_dispatched"),Dispatched);R.Data->SetBoolField(TEXT("SDK_calls_guaranteed_nonblocking"),false);
  double Assignments=0;R.Data->TryGetNumberField(TEXT("configured_parameter_field_applications"),Assignments);const bool Persistent=R.bCompileAttempted||R.bSaveAttempted||Assignments>0;
  R.Data->SetBoolField(TEXT("explicit_persistent_asset_mutation_attempted"),Persistent);R.Data->SetBoolField(TEXT("transient_creation_only"),!Persistent);
  R.Data->SetBoolField(TEXT("legacy_wait_contract_complete"),false);R.Data->SetBoolField(TEXT("current_resource_completion_continuation"),true);
  R.Data->SetStringField(TEXT("continuation_scope"),TEXT("Same parameter bake writer once, finite GT completion/save phases on the exact current-platform/default-quality resource. Editor criterion is separate from strict shader-map diagnostics. No invented timeout, global compiler cancellation/pump, Function/streaming fanout equivalence or replay."));
  R.Data->SetObjectField(TEXT("shared_material_read_metrics"),Work.Metrics);
 }
 void Cache(bool Terminal,bool Safe){LastSnapshot=Operations::EncodeVariableResult(Work.Result,Operation());MaterialPostprocessAsync::Publish(RawJob,LastSnapshot,Terminal,Safe);}
 void Quarantine(const FString& Why)
 {
  if(Phase==EParameterPhase::Quarantined||Phase==EParameterPhase::Terminal)return;Unhook();Work.CancelRequested=true;Work.CancelReason=Why;
  Work.Error(TEXT("collection_parameters_async_context_quarantined"),Why);Work.Result.Data->SetBoolField(TEXT("unsafe_job_publication_quarantined"),true);Work.Result.Data->SetBoolField(TEXT("native_writers_stopped"),true);
  Metadata(false);Phase=EParameterPhase::Quarantined;Cache(false,false);
 }
 void Pending()
 {
  if(!PublicationSafe()){Quarantine(TEXT("Parameter bake pending publication lost exact Editor/world/Job; retained state is not released."));return;}
  Work.Result.bSuccess=false;Work.Result.bPostconditionPassed=false;Metadata(false);Cache(false,true);
 }
 bool Observe()
 {
  auto& R=Work.Result;if(!Work.Admit()||!Capture(Work.Context,R,TEXT("async_current_observation"))||!Verify(Work.Context,R,Work.Plans,Work.Links,Work.Replacements))return false;
  const auto Point=R.Data->GetObjectField(TEXT("async_current_observation"));
  if(!Work.Authored.IsValid()||!SameObservation(Work.Authored.ToSharedRef(),Point->GetObjectField(TEXT("graph")).ToSharedRef()))return Work.Error(TEXT("collection_parameters_async_authored_graph_changed"),TEXT("Authored values/ordered inputs changed across callback/tick; no new compiler/save."));
  R.bChanged|=Work.Changed||Work.Context.WasDirty!=Work.Context.Selected.Asset->GetOutermost()->IsDirty();return true;
 }
 bool Point(const TCHAR* Name){return Work.Admit()&&Observe()&&ObserveCompile(Work.Context,Work.Result,Name);}
 bool DiskProof()
 {
  auto& R=Work.Result;if(!Save||!Work.Changed)return true;
  if(!DeferredPackageOwned(Work.Context,R))return false;
  const auto File=R.Data->GetObjectField(TEXT("async_current_observation"))->GetObjectField(TEXT("disk"));
  const bool OK=R.bSaved&&File->GetBoolField(TEXT("exists"))&&File->GetNumberField(TEXT("bytes"))>0&&!Work.Context.Selected.Asset->GetOutermost()->IsDirty();
  R.Data->SetBoolField(TEXT("save_disk_verified"),OK);
  return OK||Fail(R,TEXT("collection_parameters_async_final_disk_failed"),TEXT("Actual save history retained; current package/disk proof failed, no second save."));
 }
 void Success(){auto& R=Work.Result;R.bChangeStateKnown=true;R.bSuccess=true;R.bPostconditionPassed=true;R.Stage=TEXT("complete");Phase=EParameterPhase::Final;}
 void Finalize()
 {
  if(FinalEncoded||Phase==EParameterPhase::Terminal||Phase==EParameterPhase::Quarantined)return;
  if(!PublicationSafe()){Quarantine(TEXT("Parameter bake final publication lost exact Editor/world/Job."));return;}
  auto& R=Work.Result;if(R.Error.IsEmpty()&&!Work.Admit()){}
  if(!R.Error.IsEmpty())
  {
   R.bSuccess=false;R.bPostconditionPassed=false;
   if(!Work.TargetSDKEntered&&!R.bCompileAttempted&&!R.bSaveAttempted){R.bChangeStateKnown=true;R.bChanged=false;R.Compiled=false;R.Data->SetBoolField(TEXT("target_SDK_entered"),false);}
  }
  Metadata(true);FinalEncoded=true;Cache(true,true);Phase=EParameterPhase::Terminal;Unhook();
 }
 void Execute()
 {
  auto& R=Work.Result;
  if(Work.CancelRequested||MaterialPostprocessAsync::IsStopping())Work.Error(TEXT("collection_parameters_async_cancelled"),Work.CancelReason.IsEmpty()?TEXT("Parameter bake continuation stopped without compiler cancellation/recovery/replay."):Work.CancelReason);
  if(!R.Error.IsEmpty()||!Work.Admit()){Phase=EParameterPhase::Final;Finalize();return;}
  if(Phase==EParameterPhase::Dispatch)
  {
   if(Dispatched){Work.Error(TEXT("collection_parameters_async_replay_forbidden"),TEXT("Original writer already entered."));Phase=EParameterPhase::Final;Finalize();return;}
   Dispatched=true;Replace(MaterialPath,Runtime,Save);
   R.Data->SetBoolField(TEXT("transaction_closed_before_yield"),true);
   Phase=Work.Ready&&R.Error.IsEmpty()?EParameterPhase::WaitInitial:EParameterPhase::Final;
  }
  else if(Phase==EParameterPhase::WaitInitial)
  {
   if(!Point(TEXT("initial_compile_status")))Phase=EParameterPhase::Final;
   else if(R.Compiled.IsSet())
   {
    R.Data->SetBoolField(TEXT("save_skipped_due_to_pending_compile"),false);R.Data->SetBoolField(TEXT("save_skipped_due_to_compile_errors"),Save&&Work.Changed&&!R.Compiled.GetValue());
    if(!R.Compiled.GetValue()){R.bChangeStateKnown=true;Fail(R,TEXT("collection_parameters_compile_failed"),TEXT("Actual original Editor compile criterion failed; actual replacements retained, save suppressed."));Phase=EParameterPhase::Final;}
    else Phase=EParameterPhase::Save;
   }
  }
  else if(Phase==EParameterPhase::Save)
  {
   if(!Observe()||!CompleteParameterSave(Work.Context,R,Work.Plans,Work.Links,Work.Replacements,Save,Work.Changed))Phase=EParameterPhase::Final;
   else if(Save&&Work.Changed&&!R.Compiled.IsSet())Phase=EParameterPhase::WaitSaved;
   else if(!Observe()||!DiskProof())Phase=EParameterPhase::Final;else Success();
  }
  else if(Phase==EParameterPhase::WaitSaved)
  {
   if(!Point(TEXT("final_compile_status")))Phase=EParameterPhase::Final;
   else if(R.Compiled.IsSet())
   {if(!R.Compiled.GetValue()){R.bChangeStateKnown=true;Fail(R,TEXT("collection_parameters_compile_failed_after_save"),TEXT("Final Editor criterion failed; savedTrue history retained."));Phase=EParameterPhase::Final;}else if(!DiskProof())Phase=EParameterPhase::Final;else Success();}
  }
  if(!R.Error.IsEmpty())Phase=EParameterPhase::Final;
  if(Phase==EParameterPhase::Final){Finalize();return;}Pending();
 }
 void ReleaseSafe();
 bool Tick(float)
 {
  TSharedRef<FParameterJobState> Hold=AsShared();if(Phase==EParameterPhase::Terminal||Phase==EParameterPhase::Quarantined)return false;
  if(Executing||DeferredRunning||LastFrame==GFrameCounter)return true;LastFrame=GFrameCounter;
  if(!PublicationSafe()){Quarantine(TEXT("Parameter bake tick lost exact Editor/world/global safety."));return false;}
  {TGuardValue<bool> Busy(Executing,true);TGuardValue<bool> Running(DeferredRunning,true);TGuardValue<FDeferredWork*> Current(CurrentDeferredWork,&Work);Material::FReadMetricsScope Metrics;FrameMetrics=&Metrics;FrameMetricsMerged=false;Execute();FrameMetrics=nullptr;}
  ReleaseSafe();return Phase!=EParameterPhase::Terminal&&Phase!=EParameterPhase::Quarantined;
 }
 void Hook(){const TWeakPtr<FParameterJobState> Weak=AsShared();Ticker=FTSTicker::GetCoreTicker().AddTicker(FTickerDelegate::CreateLambda([Weak](float Delta){auto State=Weak.Pin();return State&&State->Tick(Delta);}),0.0f);}
};
TSharedPtr<FParameterJobState>& ActiveParameterJob(){static auto* State=new TSharedPtr<FParameterJobState>;return *State;}
void FParameterJobState::ReleaseSafe()
{
 if(Executing||Phase!=EParameterPhase::Terminal||!PublicationSafe())return;
 Work.Objects.Reset();Work.Context.Original.Reset();Work.Context.Created.Reset();Work.Context.Collections.Reset();Work.Context.Instances.Reset();Work.Context.World.Reset();Work.Context.Selected.Asset.Reset();Work.Context.Selected.SelectedExpression.Reset();JobIdentity.Reset();Job.Reset();
 if(ActiveParameterJob().Get()==this){ActiveParameterJob().Reset();DeferredLease=false;}
}
UCubelessMaterialPostprocessJob* StartParameterJob(const FString& Text)
{
 if(!DeferredGlobals()||DeferredRunning||DeferredLease||MaterialPostprocessAsync::IsStopping())return nullptr;
 UWorld* World=GEditor->GetEditorWorldContext().World();if(!DeferredLive(World)||World->bIsTearingDown)return nullptr;
 auto State=MakeShared<FParameterJobState>();State->Work.Editor=GEditor;State->Work.World=World;ActiveParameterJob()=State;DeferredLease=true;UCubelessMaterialPostprocessJob* Created=nullptr;
 {
  TGuardValue<bool> Busy(State->Executing,true);TGuardValue<bool> Running(DeferredRunning,true);State->Work.Result.bMutationAttempted=true;State->Work.Result.Data->SetNumberField(TEXT("NewObject_MaterialPostprocessJob"),1);
  Created=NewObject<UCubelessMaterialPostprocessJob>(GetTransientPackage());State->RawJob=Created;State->Work.Result.Data->SetBoolField(TEXT("job_allocation_returned"),true);State->Work.Result.Data->SetBoolField(TEXT("job_allocation_nonnull"),Created!=nullptr);
  if(!State->PublicationSafe()||State->Work.CancelRequested||MaterialPostprocessAsync::IsStopping()){State->Quarantine(TEXT("Parameter bake Job allocation interrupted before original writer dispatch."));return nullptr;}
  if(!Created||!DeferredLive(Created)||Created->GetClass()!=UCubelessMaterialPostprocessJob::StaticClass()||Created->GetOuter()!=GetTransientPackage()||Created->GetPackage()!=GetTransientPackage()){ActiveParameterJob().Reset();DeferredLease=false;return nullptr;}State->Job.Reset(Created);State->JobIdentity=MakeUnique<FDeferredIdentity>(Created);
  MaterialPostprocessAsync::RegisterJob(Created,MaterialPostprocessAsync::EJobKind::ReplaceCollectionParameters,State->Id);
  TSharedPtr<FJsonObject> Params;bool Valid=ParseReferenceJobParams(Text,Params,3);
  if(Valid)Valid=Params->TryGetStringField(TEXT("material_path"),State->MaterialPath)&&Params->TryGetBoolField(TEXT("use_runtime_values"),State->Runtime)&&Params->TryGetBoolField(TEXT("save"),State->Save);
  State->Work.Result.bSaveRequested=State->Save;
  if(!Valid){Fail(State->Work.Result,TEXT("collection_parameters_async_params_invalid"),TEXT("Exact normalized material_path/use_runtime_values/save fields required."));State->Phase=EParameterPhase::Final;State->Finalize();}
  else State->Pending();
 }
 if(State->Phase==EParameterPhase::Terminal){State->ReleaseSafe();return Created;}if(State->Phase!=EParameterPhase::Quarantined)State->Hook();return Created;
}
bool CancelParameterJob(UCubelessMaterialPostprocessJob* Job,const FString& Reason)
{
 if(!IsInGameThread()||!ActiveParameterJob()||ActiveParameterJob()->RawJob!=Job)return false;
 auto State=ActiveParameterJob();if(State->Phase==EParameterPhase::Terminal||State->Phase==EParameterPhase::Quarantined)return false;
 State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();if(State->Executing)return true;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Parameter bake cancellation retained unsafe state without new SDK."));return true;}
 {TGuardValue<bool> Busy(State->Executing,true);State->Work.Error(TEXT("collection_parameters_async_cancelled"),State->Work.CancelReason);State->Phase=EParameterPhase::Final;State->Finalize();}State->ReleaseSafe();return true;
}
void ShutdownParameterJob(const FString& Reason)
{
 auto State=ActiveParameterJob();if(!State)return;State->Work.CancelRequested=true;State->Work.CancelReason=Reason.Left(1024);State->Unhook();if(State->Executing)return;
 if(!State->PublicationSafe()){State->Quarantine(TEXT("Module shutdown retained unsafe reference state."));return;}CancelParameterJob(State->RawJob,State->Work.CancelReason);
}

}
UCubelessMaterialPostprocessJob* StartReplaceCollectionParametersContinuation(const FString& Params){return StartParameterJob(Params);}
bool CancelReplaceCollectionParametersContinuation(UCubelessMaterialPostprocessJob* Job,const FString& Reason){return CancelParameterJob(Job,Reason);}
void ShutdownReplaceCollectionParametersContinuation(const FString& Reason){ShutdownParameterJob(Reason);}
bool HasReplaceCollectionParametersContinuationLease(){return DeferredLease||DeferredRunning;}
}
