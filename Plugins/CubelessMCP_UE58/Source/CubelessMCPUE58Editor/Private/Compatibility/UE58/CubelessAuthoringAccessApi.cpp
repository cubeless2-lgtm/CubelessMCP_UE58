#include "Compatibility/UE58/CubelessAuthoringAccessApi.h"
#include "Editor.h"
#include "Subsystems/AssetEditorSubsystem.h"
#include "Engine/Blueprint.h"
#include "Engine/TimelineTemplate.h"
#include "Engine/Texture2D.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/SkeletalMeshSocket.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "EditorFramework/AssetImportData.h"
#include "Animation/Skeleton.h"
#include "Animation/AnimSequence.h"
#include "Animation/PoseAsset.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "GameFramework/Actor.h"
#include "Curves/CurveFloat.h"
#include "Curves/CurveVector.h"
#include "Curves/CurveLinearColor.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "AnimGraphNode_ControlRig.h"
#include "AnimGraphNode_PoseDriver.h"
#include "ControlRig.h"
#include "OptimusDeformer.h"
#include "OptimusDiagnostic.h"
#include "OptimusNodeGraph.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Misc/PackageName.h"
#include "Misc/ScopeExit.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/SceneComponent.h"
#include "Engine/World.h"
#include "ReferenceSkeleton.h"
#include <limits>
#endif

namespace Cubeless::UE58::AuthoringAccess
{
namespace
{
using J=FJsonObject;
bool Writing=false;
FString Path(const UObject* O){return IsValid(O)?O->GetPathName():FString();}
FString Encode(const TSharedRef<J>& V)
{
 FString S;if(FJsonSerializer::Serialize(V,TJsonWriterFactory<>::Create(&S)))return S;
 // A response error must retain observed side effects, including a completed save.
 auto Failure=MakeShared<J>();
 for(const TCHAR* Key:{TEXT("operation"),TEXT("stage"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("change_state_known"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("package_dirty_after"),TEXT("retry_risk")})
  if(const auto* Value=V->Values.Find(Key))Failure->SetField(Key,*Value);
 Failure->SetBoolField(TEXT("success"),false);Failure->SetStringField(TEXT("error_code"),TEXT("response_serialization_failed"));Failure->SetStringField(TEXT("error"),TEXT("Response serialization failed; observed mutation/save state is retained. Do not replay the operation."));
 S.Reset();FJsonSerializer::Serialize(Failure,TJsonWriterFactory<>::Create(&S));return S;
}
TSharedPtr<FJsonValue> Row(const TSharedRef<J>& V){return MakeShared<FJsonValueObject>(V);}
TArray<TSharedPtr<FJsonValue>> Strings(const TArray<FString>& V){TArray<TSharedPtr<FJsonValue>> R;for(const auto& S:V)R.Add(MakeShared<FJsonValueString>(S));return R;}
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed|RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WillBeLoaded);}
bool Idle(){return IsInGameThread()&&GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!IsEngineExitRequested();}
struct FContext
{
 TSharedRef<J> R=MakeShared<J>();TStrongObjectPtr<UObject> Object;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UClass> Class;
 UEditorEngine* Editor=GEditor;TWeakObjectPtr<UWorld> World;FString ObjectPath,PackageName;bool Ready=false,DirtyBefore=false;
 FContext(const TCHAR* Op,UObject* O)
 {
  R->SetStringField(TEXT("operation"),Op);R->SetStringField(TEXT("stage"),TEXT("preflight"));
  for(const TCHAR* K:{TEXT("success"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("automatic_retry"),TEXT("atomic_rollback")})R->SetBoolField(K,false);
  R->SetBoolField(TEXT("change_state_known"),true);R->SetStringField(TEXT("retry_risk"),TEXT("none_observed"));
  if(!Idle()||!Live(O)||!Live(O->GetPackage())){Error(TEXT("ready_editor_object_required"),TEXT("Ready object on the idle Editor game thread required; no PIE/SIE, GC, save, exit or pending load."));return;}
  Object.Reset(O);Package.Reset(O->GetPackage());Outer.Reset(O->GetOuter());Class.Reset(O->GetClass());ObjectPath=Path(O);PackageName=Package->GetName();World=Editor->GetEditorWorldContext().World();DirtyBefore=Package->IsDirty();
  R->SetStringField(TEXT("object"),ObjectPath);R->SetStringField(TEXT("package"),PackageName);R->SetBoolField(TEXT("package_dirty_before"),DirtyBefore);Ready=true;
 }
 void Error(const TCHAR* Code,const FString& Message){R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("error_code"),Code);R->SetStringField(TEXT("error"),Message);}
 FString Fail(const TCHAR* Code,const FString& Message){Error(Code,Message);return Finish(false);}
 FString Finish(bool Success){R->SetBoolField(TEXT("success"),Success);if(Package.IsValid())R->SetBoolField(TEXT("package_dirty_after"),Package->IsDirty());return Encode(R);}
 bool Current()
 {
  if(!Idle()||GEditor!=Editor||Editor->GetEditorWorldContext().World()!=World.Get()||!Live(Object.Get())||!Live(Package.Get())||Object->GetOuter()!=Outer.Get()||Object->GetClass()!=Class.Get()||Path(Object.Get())!=ObjectPath||Object->GetPackage()!=Package.Get()||Package->GetName()!=PackageName)
  {Error(TEXT("context_changed"),TEXT("Object/class/outer/package or Editor context changed at a callback boundary; inspect actual state before another request."));return false;}return true;
 }
 bool Admit(bool Save,bool AllowDirty)
 {
  if(!Object->IsAsset()||Object->IsTemplate()||!PackageName.StartsWith(TEXT("/Game/"))||Package->HasAnyPackageFlags(PKG_ContainsMap)||Writing){Error(TEXT("editable_game_asset_required"),TEXT("Standalone /Game asset and an available mutation slot required; no templates, map packages or reentry."));return false;}
  if(Save&&DirtyBefore&&!AllowDirty){Error(TEXT("dirty_package_save_requires_approval"),TEXT("Saving an already dirty package also persists other work in that exact package."));return false;}return true;
 }
 void Begin(const TCHAR* Stage){R->SetStringField(TEXT("stage"),Stage);R->SetBoolField(TEXT("mutation_attempted"),true);R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target"));}
 void Known(bool Changed){R->SetBoolField(TEXT("changed"),Changed);R->SetBoolField(TEXT("change_state_known"),true);R->SetStringField(TEXT("retry_risk"),Changed?TEXT("mutation_attempted"):TEXT("none_observed"));}
 bool SaveExact(bool Requested,TFunctionRef<bool()> Stable)
 {
  if(!Requested)return true;
  if(!Current()||!Stable())return false;
  FString Filename;const bool Existing=FPackageName::DoesPackageExist(PackageName,&Filename);
  if(!Existing)Filename=FPackageName::LongPackageNameToFilename(PackageName,FPackageName::GetAssetPackageExtension());
  R->SetStringField(TEXT("stage"),TEXT("save_exact_package"));R->SetStringField(TEXT("save_filename"),Filename);R->SetBoolField(TEXT("save_attempted"),true);
  FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;
  const bool Saved=UPackage::SavePackage(Package.Get(),Object.Get(),*Filename,Args);R->SetBoolField(TEXT("saved"),Saved);
  if(!Current()||!Stable()){R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target"));return false;}
  if(!Saved){Error(TEXT("save_failed"),TEXT("Exact package save failed; memory changes remain and are not rolled back."));return false;}return true;
 }
};

// Local authored-property copy, not a serialized text/hash approximation. Classes
// and owners stay alive; Identical compares complete array/struct/reference values.
struct FValue
{
 TStrongObjectPtr<UObject> Owner;TStrongObjectPtr<UObject> Outer;TStrongObjectPtr<UClass> Type;FString ObjectPath;FProperty* Property;void* Copy;
 FValue(UObject* O,FProperty* P):Owner(O),Outer(O->GetOuter()),Type(O->GetClass()),ObjectPath(O->GetPathName()),Property(P){Copy=FMemory::Malloc(P->GetSize(),P->GetMinAlignment());P->InitializeValue(Copy);P->CopyCompleteValue(Copy,P->ContainerPtrToValuePtr<void>(O));}
 ~FValue(){Property->DestroyValue(Copy);FMemory::Free(Copy);}
 bool Same()const{return Live(Owner.Get())&&Owner->GetClass()==Type.Get()&&Owner->GetOuter()==Outer.Get()&&Owner->GetPathName()==ObjectPath&&Property->Identical(Copy,Property->ContainerPtrToValuePtr<void>(Owner.Get()),PPF_None);}
};
struct FSnapshot
{
 TArray<TUniquePtr<FValue>> Values;
 TArray<TStrongObjectPtr<UObject>> ReferencedObjects;int32 ReferenceVisits=0;
 bool HoldReferences(FProperty* P,const void* Data,int32 Depth=0)
 {
  if(!P||!Data||Depth>16||++ReferenceVisits>131072||P->ArrayDim>4096)return false;
  for(int32 D=0;D<P->ArrayDim;++D)
  {
   const void* V=static_cast<const uint8*>(Data)+D*P->GetElementSize();
   if(auto* O=CastField<FObjectProperty>(P)){UObject* Ref=O->GetObjectPropertyValue(V);if(Ref){if(!Live(Ref))return false;ReferencedObjects.Emplace(Ref);}}
   else if(auto* I=CastField<FInterfaceProperty>(P)){UObject* Ref=static_cast<const FScriptInterface*>(V)->GetObject();if(Ref){if(!Live(Ref))return false;ReferencedObjects.Emplace(Ref);}}
   else if(auto* S=CastField<FStructProperty>(P)){for(TFieldIterator<FProperty> F(S->Struct);F;++F)if(!F->HasAnyPropertyFlags(CPF_Transient)&&!HoldReferences(*F,F->ContainerPtrToValuePtr<void>(V),Depth+1))return false;}
   else if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,V);if(H.Num()>16384)return false;for(int32 N=0;N<H.Num();++N)if(!HoldReferences(A->Inner,H.GetRawPtr(N),Depth+1))return false;}
   else if(auto* M=CastField<FMapProperty>(P)){FScriptMapHelper H(M,V);if(H.GetMaxIndex()>32768)return false;for(int32 N=0;N<H.GetMaxIndex();++N)if(H.IsValidIndex(N)&&(!HoldReferences(M->KeyProp,H.GetKeyPtr(N),Depth+1)||!HoldReferences(M->ValueProp,H.GetValuePtr(N),Depth+1)))return false;}
   else if(auto* Set=CastField<FSetProperty>(P)){FScriptSetHelper H(Set,V);if(H.GetMaxIndex()>32768)return false;for(int32 N=0;N<H.GetMaxIndex();++N)if(H.IsValidIndex(N)&&!HoldReferences(Set->ElementProp,H.GetElementPtr(N),Depth+1))return false;}
  }return true;
 }
 bool Capture(UObject* O,const TSet<FName>& Skip={},bool IncludeOwned=false)
 {
  TArray<UObject*> Objects{O};if(IncludeOwned)GetObjectsWithOuter(O,Objects,EGetObjectsFlags::IncludeNestedObjects);
  Objects.AddUnique(O);if(Objects.Num()>4096)return false;
  for(UObject* V:Objects)
  {
   if(!Live(V))return false;
   // Optimus generated compute graphs and action history are not authored inputs.
   bool Generated=false;if(IncludeOwned)for(const UObject* Parent=V;Parent&&Parent!=O;Parent=Parent->GetOuter())Generated|=Parent->GetClass()->GetName().Contains(TEXT("ComputeGraph"))||Parent->GetClass()->GetName().Contains(TEXT("ActionStack"));
   if(Generated)continue;
   for(TFieldIterator<FProperty> I(V->GetClass());I;++I)
   {
    FProperty* P=*I;if(P->HasAnyPropertyFlags(CPF_Transient|CPF_DuplicateTransient)||Skip.Contains(P->GetFName()))continue;
    // Optimus graph/pin/value signatures are persistent but often not CPF_Edit.
    // Exclude the installed compiler's generated maps/status, retain all other
    // persistent input properties including pin direction/type/domain/subpins.
    const bool CompileOutput=IncludeOwned&&(P->GetName()==TEXT("ComputeGraphs")||P->GetName()==TEXT("DataInterfacePropertyOverrideMap")||P->GetName()==TEXT("ValueMap")||P->GetName()==TEXT("Status")||P->GetName()==TEXT("DiagnosticLevel"));
    const bool Authored=!CompileOutput&&(P->HasAnyPropertyFlags(CPF_Edit)||IncludeOwned);
    if(!Authored)continue;
    if(Values.Num()>=32768||P->GetSize()>1048576)return false;
    if(auto* A=CastField<FArrayProperty>(P))if(FScriptArrayHelper(A,A->ContainerPtrToValuePtr<void>(V)).Num()>16384)return false;
    if(!HoldReferences(P,P->ContainerPtrToValuePtr<void>(V)))return false;
    Values.Add(MakeUnique<FValue>(V,P));
   }
  }return true;
 }
 bool Same(FContext& C,const TCHAR* Boundary)const
 {
  if(!C.Current())return false;
  for(const auto& O:ReferencedObjects)if(!Live(O.Get())){C.Error(TEXT("authored_reference_not_live"),TEXT("An authored hard reference was destroyed or became unready during a callback."));return false;}
  for(const auto& V:Values)if(!V->Same())
  {
   auto M=MakeShared<J>();M->SetStringField(TEXT("object"),Path(V->Owner.Get()));M->SetStringField(TEXT("property"),V->Property->GetName());M->SetStringField(TEXT("boundary"),Boundary);M->SetStringField(TEXT("expected"),TEXT("captured authored value/reference"));M->SetStringField(TEXT("actual"),TEXT("changed"));C.R->SetObjectField(TEXT("first_mismatch"),M);C.Error(TEXT("authored_input_changed"),TEXT("Authored input changed during an engine callback; actual change/save state is retained. No rollback or retry."));return false;
  }return true;
 }
};
bool FiniteCurve(const FRichCurve& Curve)
{
 if(!FMath::IsFinite(Curve.DefaultValue))return false;
 for(const auto& K:Curve.GetConstRefOfKeys())
  if(!FMath::IsFinite(K.Time)||!FMath::IsFinite(K.Value)||!FMath::IsFinite(K.ArriveTangent)||!FMath::IsFinite(K.LeaveTangent)||!FMath::IsFinite(K.ArriveTangentWeight)||!FMath::IsFinite(K.LeaveTangentWeight))return false;
 return true;
}
TSharedRef<J> CurveData(const FRichCurve& Curve)
{
 auto O=MakeShared<J>();TArray<TSharedPtr<FJsonValue>> Keys;
 for(const auto& K:Curve.GetConstRefOfKeys()){auto V=MakeShared<J>();V->SetNumberField(TEXT("time"),K.Time);V->SetNumberField(TEXT("value"),K.Value);V->SetNumberField(TEXT("interp_mode"),K.InterpMode);V->SetNumberField(TEXT("tangent_mode"),K.TangentMode);V->SetNumberField(TEXT("tangent_weight_mode"),K.TangentWeightMode);V->SetNumberField(TEXT("arrive_tangent"),K.ArriveTangent);V->SetNumberField(TEXT("leave_tangent"),K.LeaveTangent);V->SetNumberField(TEXT("arrive_tangent_weight"),K.ArriveTangentWeight);V->SetNumberField(TEXT("leave_tangent_weight"),K.LeaveTangentWeight);Keys.Add(Row(V));}
 O->SetArrayField(TEXT("keys"),Keys);O->SetNumberField(TEXT("default_value"),Curve.DefaultValue);O->SetNumberField(TEXT("pre_infinity"),Curve.PreInfinityExtrap);O->SetNumberField(TEXT("post_infinity"),Curve.PostInfinityExtrap);return O;
}
bool SameCurve(const FRichCurve& A,const FRichCurve& B)
{
 // Installed RichCurve equality omits DefaultValue and tangent weights; key
 // equality also ignores non-cubic tangent values. Preserve all authored fields.
 if(A.DefaultValue!=B.DefaultValue||A.PreInfinityExtrap!=B.PreInfinityExtrap||A.PostInfinityExtrap!=B.PostInfinityExtrap||A.GetNumKeys()!=B.GetNumKeys())return false;
 const auto& X=A.GetConstRefOfKeys();const auto& Y=B.GetConstRefOfKeys();
 for(int32 I=0;I<X.Num();++I){const auto& L=X[I];const auto& R=Y[I];if(L.Time!=R.Time||L.Value!=R.Value||L.InterpMode!=R.InterpMode||L.TangentMode!=R.TangentMode||L.TangentWeightMode!=R.TangentWeightMode||L.ArriveTangent!=R.ArriveTangent||L.LeaveTangent!=R.LeaveTangent||L.ArriveTangentWeight!=R.ArriveTangentWeight||L.LeaveTangentWeight!=R.LeaveTangentWeight)return false;}return true;
}
FString BonesState(USkeleton* S)
{
 FString V;const auto& Ref=S->GetReferenceSkeleton();for(int32 I=0;I<Ref.GetRawBoneNum();++I){V+=Ref.GetRawRefBoneInfo()[I].Name.ToString()+TEXT(":")+FString::FromInt(Ref.GetRawParentIndex(I))+TEXT(";");}
 return V;
}
}

FString EditorState(UObject* Object)
{
 FContext C(TEXT("inspect_asset_editing_state"),Object);if(!C.Ready)return C.Finish(false);
 auto* Editors=GEditor->GetEditorSubsystem<UAssetEditorSubsystem>();if(!Editors)return C.Fail(TEXT("asset_editor_subsystem_unavailable"),TEXT("AssetEditorSubsystem unavailable."));
 TArray<TSharedPtr<FJsonValue>> Rows;bool SamePackageOpen=false;
 const auto Assets=Editors->GetAllEditedAssets();if(Assets.Num()>16384)return C.Fail(TEXT("open_asset_bound"),TEXT("Open asset inventory exceeds16384."));
 for(UObject* A:Assets)if(Live(A)){auto O=MakeShared<J>();O->SetStringField(TEXT("object"),Path(A));O->SetStringField(TEXT("package"),A->GetPackage()->GetName());O->SetBoolField(TEXT("dirty"),A->GetPackage()->IsDirty());Rows.Add(Row(O));SamePackageOpen|=A->GetPackage()==C.Package.Get();}
 C.R->SetBoolField(TEXT("package_dirty"),C.Package->IsDirty());C.R->SetBoolField(TEXT("same_package_open"),SamePackageOpen);C.R->SetArrayField(TEXT("open_assets"),Rows);C.R->SetStringField(TEXT("scope"),TEXT("Observation only, not a save lock or permission to overwrite. No focus/open/close/save."));return C.Finish(C.Current());
}
FString Curve(UObject* Object)
{
 FContext C(TEXT("inspect_float_curve"),Object);auto* F=Cast<UCurveFloat>(Object);if(!C.Ready)return C.Finish(false);if(!F||F->FloatCurve.GetNumKeys()>4096)return C.Fail(TEXT("bounded_float_curve_required"),TEXT("CurveFloat with at most4096 authored keys required."));if(!FiniteCurve(F->FloatCurve))return C.Fail(TEXT("non_finite_curve_data"),TEXT("Curve default/key/time/tangent/weight contains NaN or infinity; no data was changed."));C.R->SetObjectField(TEXT("curve"),CurveData(F->FloatCurve));C.R->SetBoolField(TEXT("event_curve"),F->bIsEventCurve);return C.Finish(C.Current());
}
FString SetCurve(UObject* Object,const FString& Keys,bool AllowReplace,bool Execute,bool Save,bool AllowDirty)
{
 FContext C(TEXT("set_float_curve_keys"),Object);auto* F=Cast<UCurveFloat>(Object);if(!C.Ready)return C.Finish(false);if(!F)return C.Fail(TEXT("editable_float_curve_required"),TEXT("Standalone CurveFloat required."));if(F->FloatCurve.GetNumKeys()>4096)return C.Fail(TEXT("existing_curve_key_bound"),TEXT("Existing key preservation snapshot exceeds4096 keys."));if(!C.Admit(Save,AllowDirty))return C.Finish(false);if(!FiniteCurve(F->FloatCurve))return C.Fail(TEXT("non_finite_curve_data"),TEXT("Existing curve contains NaN or infinity; refusing replacement before Modify/save."));
 TArray<TSharedPtr<FJsonValue>> Input;if(Keys.Len()>1048576||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Keys),Input)||Input.Num()>4096)return C.Fail(TEXT("invalid_curve_keys"),TEXT("JSON array of at most4096 keys required."));
 FRichCurve Wanted=F->FloatCurve;TArray<FRichCurveKey> Parsed;float Last=-TNumericLimits<float>::Max();
 const TSet<FString> Fields{TEXT("time"),TEXT("value"),TEXT("interp_mode"),TEXT("tangent_mode"),TEXT("tangent_weight_mode"),TEXT("arrive_tangent"),TEXT("leave_tangent"),TEXT("arrive_tangent_weight"),TEXT("leave_tangent_weight")};
 for(const auto& V:Input)
 {
  const TSharedPtr<J>* K=nullptr;if(!V->TryGetObject(K)||!K->IsValid())return C.Fail(TEXT("invalid_key"),TEXT("Each key must be an object."));for(const auto& Pair:(*K)->Values){const FString KeyName(Pair.Key.ToView());if(!Fields.Contains(KeyName))return C.Fail(TEXT("unknown_key_field"),KeyName);}
  double T=0,N=0;if(!(*K)->TryGetNumberField(TEXT("time"),T)||!(*K)->TryGetNumberField(TEXT("value"),N)||!FMath::IsFinite(T)||!FMath::IsFinite(N)||FMath::Abs(T)>1e9||FMath::Abs(N)>1e30||float(T)<=Last)return C.Fail(TEXT("invalid_key_order_or_value"),TEXT("Finite unique increasing float times and finite values required."));
  FRichCurveKey Key{float(T),float(N)};Last=Key.Time;
  auto Number=[&](const TCHAR* Name,float& Out){if(!(*K)->HasField(Name))return true;double X=0;if(!(*K)->TryGetNumberField(Name,X)||!FMath::IsFinite(X)||FMath::Abs(X)>1e30)return false;Out=float(X);return true;};
  if(!Number(TEXT("arrive_tangent"),Key.ArriveTangent)||!Number(TEXT("leave_tangent"),Key.LeaveTangent)||!Number(TEXT("arrive_tangent_weight"),Key.ArriveTangentWeight)||!Number(TEXT("leave_tangent_weight"),Key.LeaveTangentWeight)||Key.ArriveTangentWeight<0||Key.LeaveTangentWeight<0)return C.Fail(TEXT("invalid_tangent"),TEXT("Finite tangents and nonnegative tangent weights required."));
  auto Mode=[&](const TCHAR* Name,int32 Maximum,int32& Out){if(!(*K)->HasField(Name))return true;double X=0;if(!(*K)->TryGetNumberField(Name,X)||X<0||X>Maximum||FMath::TruncToDouble(X)!=X)return false;Out=int32(X);return true;};int32 I=RCIM_Linear,M=RCTM_Auto,W=RCTWM_WeightedNone;
  if(!Mode(TEXT("interp_mode"),2,I)||!Mode(TEXT("tangent_mode"),2,M)||!Mode(TEXT("tangent_weight_mode"),3,W))return C.Fail(TEXT("invalid_curve_mode"),TEXT("interp/tangent mode0..2, weight mode0..3 required."));Key.InterpMode=ERichCurveInterpMode(I);Key.TangentMode=ERichCurveTangentMode(M);Key.TangentWeightMode=ERichCurveTangentWeightMode(W);Parsed.Add(Key);
 }
 // SetKeys calls AutoSetTangents, including for explicitly supplied key values.
 // AddKey creates valid handles without recomputing tangents; copy every field.
 Wanted.Reset();Wanted.ReserveKeys(Parsed.Num());for(const FRichCurveKey& Key:Parsed){const FKeyHandle Handle=Wanted.AddKey(Key.Time,Key.Value);Wanted.GetKey(Handle)=Key;}const FRichCurve Before=F->FloatCurve;
 const bool Same=SameCurve(Before,Wanted);if(!Same&&!AllowReplace&&Before.GetNumKeys()>0)return C.Fail(TEXT("replace_requires_approval"),TEXT("Existing keys need allow_replace=true, including replacement with an empty array."));
 C.R->SetNumberField(TEXT("keys_before"),Before.GetNumKeys());C.R->SetNumberField(TEXT("keys_after_requested"),Parsed.Num());if(!Execute||Same)return C.Finish(true);
 const bool EventBefore=F->bIsEventCurve;FSnapshot Preserved;if(!Preserved.Capture(F,{TEXT("FloatCurve")}))return C.Fail(TEXT("snapshot_bound"),TEXT("Authored preservation snapshot bound exceeded."));TGuardValue<bool> Busy(Writing,true);C.Begin(TEXT("modify_curve"));F->Modify();if(!C.Current()||!SameCurve(F->FloatCurve,Before)||F->bIsEventCurve!=EventBefore||!Preserved.Same(C,TEXT("modify")))return C.Fail(TEXT("curve_changed_after_modify"),TEXT("Curve/authored inputs changed in Modify callback; writer/save not dispatched."));
 F->FloatCurve=Wanted;C.R->SetStringField(TEXT("stage"),TEXT("post_edit_curve"));F->PostEditChange();
 auto Stable=[&](){if(!Preserved.Same(C,TEXT("post_edit_or_save")))return false;if(!SameCurve(F->FloatCurve,Wanted)||F->bIsEventCurve!=EventBefore){C.Error(TEXT("curve_keys_changed"),TEXT("Actual curve/event flag differs from requested complete key values after callback."));return false;}return true;};
 if(!Stable())return C.Finish(false);F->MarkPackageDirty();if(!Stable())return C.Finish(false);C.Known(true);if(!C.SaveExact(Save,Stable))return C.Finish(false);C.R->SetStringField(TEXT("stage"),TEXT("complete"));return C.Finish(true);
}
FString Timelines(UObject* Object)
{
 FContext C(TEXT("inspect_blueprint_timelines"),Object);if(!C.Ready)return C.Finish(false);auto* BP=Cast<UBlueprint>(Object);if(!BP||BP->Timelines.Num()>1024)return C.Fail(TEXT("blueprint_required"),TEXT("Blueprint with at most1024 source-local Timelines required."));TArray<TSharedPtr<FJsonValue>> Rows;
 for(UTimelineTemplate* T:BP->Timelines)
 {
  if(!Live(T))return C.Fail(TEXT("timeline_not_ready"),TEXT("Timeline is missing or pending load."));if(T->FloatTracks.Num()+T->VectorTracks.Num()+T->LinearColorTracks.Num()+T->EventTracks.Num()>4096)return C.Fail(TEXT("track_bound"),TEXT("At most4096 tracks per Timeline."));
  auto O=MakeShared<J>();O->SetStringField(TEXT("object"),Path(T));O->SetNumberField(TEXT("length"),T->TimelineLength);O->SetNumberField(TEXT("length_mode"),T->LengthMode);TArray<TSharedPtr<FJsonValue>> Tracks;
  auto Track=[&](const FTTTrackBase& B,const TCHAR* Kind,UObject* CurveObject){auto V=MakeShared<J>();V->SetStringField(TEXT("name"),B.GetTrackName().ToString());V->SetStringField(TEXT("kind"),Kind);V->SetBoolField(TEXT("external"),B.bIsExternalCurve);V->SetStringField(TEXT("curve"),Path(CurveObject));Tracks.Add(Row(V));};
  for(const auto& V:T->FloatTracks)Track(V,TEXT("float"),V.CurveFloat);for(const auto& V:T->VectorTracks)Track(V,TEXT("vector"),V.CurveVector);for(const auto& V:T->LinearColorTracks)Track(V,TEXT("linear_color"),V.CurveLinearColor);for(const auto& V:T->EventTracks)Track(V,TEXT("event"),V.CurveKeys);
  O->SetArrayField(TEXT("tracks"),Tracks);Rows.Add(Row(O));
 }C.R->SetArrayField(TEXT("timelines"),Rows);C.R->SetStringField(TEXT("scope"),TEXT("Source-local templates; curve object paths can be inspected separately. No compile/open/save."));return C.Finish(C.Current());
}
FString ImportSources(UObject* Object)
{
 FContext C(TEXT("inspect_asset_import_sources"),Object);if(!C.Ready)return C.Finish(false);TArray<TSharedPtr<FJsonValue>> Rows;TSet<UAssetImportData*> Seen;
 for(TFieldIterator<FObjectPropertyBase> I(Object->GetClass());I;++I)if(auto* Data=Cast<UAssetImportData>(I->GetObjectPropertyValue_InContainer(Object)))
 {
  if(Seen.Contains(Data))continue;Seen.Add(Data);if(!Live(Data)||Data->GetSourceData().SourceFiles.Num()>4096)return C.Fail(TEXT("import_data_bound"),TEXT("Ready import data with at most4096 source files required."));
  for(const auto& V:Data->GetSourceData().SourceFiles){auto O=MakeShared<J>();O->SetStringField(TEXT("import_data"),Path(Data));O->SetStringField(TEXT("relative_filename"),V.RelativeFilename);O->SetStringField(TEXT("resolved_filename"),UAssetImportData::ResolveImportFilename(V.RelativeFilename,Data->GetOutermost()));O->SetStringField(TEXT("timestamp"),V.Timestamp.ToIso8601());O->SetStringField(TEXT("file_hash"),LexToString(V.FileHash));Rows.Add(Row(O));}
 }C.R->SetArrayField(TEXT("source_files"),Rows);C.R->SetBoolField(TEXT("has_import_data"),!Seen.IsEmpty());return C.Finish(C.Current());
}
FString AnimNodes(UObject* Object)
{
 FContext C(TEXT("inspect_specialized_anim_graph"),Object);if(!C.Ready)return C.Finish(false);TArray<UEdGraph*> Graphs;if(auto* BP=Cast<UBlueprint>(Object))BP->GetAllGraphs(Graphs);else if(auto* G=Cast<UEdGraph>(Object))Graphs.Add(G);else return C.Fail(TEXT("blueprint_or_graph_required"),TEXT("Loaded Blueprint/AnimBlueprint or exact EdGraph required."));
 if(Graphs.Num()>4096)return C.Fail(TEXT("graph_bound"),TEXT("Graph inventory bound4096."));TArray<TSharedPtr<FJsonValue>> Rows;int32 Count=0;
 for(UEdGraph* G:Graphs){if(!Live(G))return C.Fail(TEXT("graph_not_ready"),TEXT("Graph pending load or unavailable."));for(UEdGraphNode* N:G->Nodes){if(++Count>16384||!Live(N))return C.Fail(TEXT("node_bound_or_not_ready"),TEXT("At most16384 ready nodes."));auto O=MakeShared<J>();O->SetStringField(TEXT("graph"),Path(G));O->SetStringField(TEXT("object"),Path(N));O->SetStringField(TEXT("class"),Path(N->GetClass()));O->SetStringField(TEXT("node_guid"),N->NodeGuid.ToString());O->SetStringField(TEXT("comment"),N->NodeComment);
  if(auto* Rig=Cast<UAnimGraphNode_ControlRig>(N))O->SetStringField(TEXT("control_rig_class"),Path(Rig->Node.GetControlRigAssetReference().GetBlueprintClass()));
  if(auto* Driver=Cast<UAnimGraphNode_PoseDriver>(N)){TArray<FString> Names;if(Driver->Node.PoseTargets.Num()>4096)return C.Fail(TEXT("pose_target_bound"),TEXT("PoseDriver target bound4096."));for(const auto& V:Driver->Node.PoseTargets)Names.Add(V.DrivenName.ToString());O->SetArrayField(TEXT("pose_driver_driven_names"),Strings(Names));}
  Rows.Add(Row(O));}}
 C.R->SetArrayField(TEXT("nodes"),Rows);C.R->SetStringField(TEXT("scope"),TEXT("Native source node inventory/comment; ControlRig.GetControlRigClass and PoseDriver.DrivenName (not nonexistent target_name). No compile/save."));return C.Finish(C.Current());
}
FString Animation(UObject* Object)
{
 FContext C(TEXT("inspect_animation_native_data"),Object);if(!C.Ready)return C.Finish(false);
 if(auto* Sequence=Cast<UAnimSequence>(Object)){auto Rate=Sequence->GetSamplingFrameRate();auto R=MakeShared<J>();R->SetNumberField(TEXT("numerator"),Rate.Numerator);R->SetNumberField(TEXT("denominator"),Rate.Denominator);C.R->SetObjectField(TEXT("target_sampling_frame_rate"),R);}
 else if(auto* Pose=Cast<UPoseAsset>(Object))C.R->SetBoolField(TEXT("is_valid_additive"),Pose->IsValidAdditive());
 else if(auto* P=FindFProperty<FSoftClassProperty>(Object->GetClass(),TEXT("BoundActorClass"))){C.R->SetStringField(TEXT("bound_actor_class"),P->GetPropertyValue_InContainer(Object).ToSoftObjectPath().ToString());}
 else return C.Fail(TEXT("animation_type_required"),TEXT("AnimSequence, PoseAsset or Camera/TemplateSequence with BoundActorClass required. Montage sections/rates use existing inspect_animation_asset_metadata."));return C.Finish(C.Current());
}
FString TextureSource(UObject* Object,int32 Mip,int32 Start,int32 Count)
{
 FContext C(TEXT("inspect_texture_source_pixels"),Object);if(!C.Ready)return C.Finish(false);auto* T=Cast<UTexture2D>(Object);if(!T||!T->Source.IsValid()||T->Source.GetNumBlocks()!=1||T->Source.GetNumLayers()!=1||T->Source.GetNumSlices()!=1||Mip<0||Mip>=T->Source.GetNumMips()||Start<0||Count<0||Count>4096)return C.Fail(TEXT("source_or_page_invalid"),TEXT("Texture2D with one source block/layer/slice, existing mip and page0..4096 required."));
 const int64 Width=FMath::Max<int64>(1,T->Source.GetSizeX()>>Mip),Height=FMath::Max<int64>(1,T->Source.GetSizeY()>>Mip),Bpp=T->Source.GetBytesPerPixel();const int64 Size=T->Source.CalcMipSize(Mip);if(Size<0||Size>256*1024*1024||Bpp<=0||int64(Start)>Width*Height)return C.Fail(TEXT("source_size_bound"),TEXT("Source mip bound256MiB and valid pixel offset required."));
 const FGuid Id=T->Source.GetId();C.R->SetNumberField(TEXT("width"),double(Width));C.R->SetNumberField(TEXT("height"),double(Height));C.R->SetNumberField(TEXT("format"),T->Source.GetFormat());C.R->SetNumberField(TEXT("bytes_per_pixel"),double(Bpp));C.R->SetNumberField(TEXT("mip"),Mip);C.R->SetNumberField(TEXT("start_pixel"),Start);C.R->SetStringField(TEXT("encoding"),TEXT("Native source texel bytes in source format; no RGBA/gamma conversion, not GPU readback."));
 if(Count){TArray64<uint8> Bytes;if(!T->Source.GetMipData(Bytes,Mip)||Bytes.Num()!=Size||Size!=Width*Height*Bpp)return C.Fail(TEXT("source_payload_unavailable"),TEXT("Source mip could not be decoded to expected native texel bytes."));if(!C.Current()||T->Source.GetId()!=Id)return C.Fail(TEXT("source_changed"),TEXT("Texture source changed during payload access."));TArray<TSharedPtr<FJsonValue>> Rows;const int64 End=FMath::Min<int64>(Width*Height,int64(Start)+Count);for(int64 P=Start;P<End;++P){TArray<TSharedPtr<FJsonValue>> V;for(int64 B=0;B<Bpp;++B)V.Add(MakeShared<FJsonValueNumber>(Bytes[P*Bpp+B]));Rows.Add(MakeShared<FJsonValueArray>(V));}C.R->SetArrayField(TEXT("pixels"),Rows);}
 return C.Finish(C.Current());
}
FString RefreshTexture(UObject* Object,bool Execute)
{
 FContext C(TEXT("refresh_texture_editor_data"),Object);if(!C.Ready)return C.Finish(false);auto* T=Cast<UTexture2D>(Object);if(!T||!C.Admit(false,false))return C.Fail(TEXT("editable_texture_required"),TEXT("Standalone /Game Texture2D required."));if(!Execute)return C.Finish(true);
 FSnapshot Input;if(!Input.Capture(T))return C.Fail(TEXT("snapshot_bound"),TEXT("Texture property snapshot exceeded."));const FGuid SourceId=T->Source.GetId();TGuardValue<bool> Busy(Writing,true);C.Begin(TEXT("post_edit_texture"));T->PostEditChange();if(!Input.Same(C,TEXT("post_edit_texture"))||T->Source.GetId()!=SourceId)return C.Fail(TEXT("texture_authored_state_changed"),TEXT("Authored texture property/source identity changed at callback. No save/retry."));C.R->SetBoolField(TEXT("resource_update_requested"),true);C.R->SetBoolField(TEXT("gpu_completion_verified"),false);C.R->SetStringField(TEXT("scope"),TEXT("PostEditChange returned; resource/derived-data work may still be pending. No automatic save."));C.Known(false);return C.Finish(true);
}
FString SkeletalComponent(UObject* Object,const FString& Action,bool Disabled,double Delta,bool Refresh,bool Execute)
{
 FContext C(TEXT("skeletal_component_authoring_access"),Object);if(!C.Ready)return C.Finish(false);auto* S=Cast<USkeletalMeshComponent>(Object);if(!S)return C.Fail(TEXT("skeletal_component_required"),TEXT("SkeletalMeshComponent required."));C.R->SetStringField(TEXT("physics_asset"),Path(S->GetPhysicsAsset()));C.R->SetBoolField(TEXT("disable_post_process_blueprint"),S->GetDisablePostProcessBlueprint());
 if(Action==TEXT("inspect"))return C.Finish(C.Current());
 auto* Owner=S->GetOwner();if(!C.World.IsValid()||!Live(Owner)||Owner->IsActorBeingDestroyed()||S->IsTemplate()||!S->IsRegistered()||S->GetWorld()!=C.World.Get()||Writing||!FMath::IsFinite(Delta)||Delta<0||Delta>1)return C.Fail(TEXT("placed_editor_component_required"),TEXT("Ready registered component in current Editor world, no reentry; delta0..1 seconds required."));if(!Execute)return C.Finish(true);
 TStrongObjectPtr<AActor> KeepOwner(Owner);TStrongObjectPtr<UObject> Mesh(S->GetSkeletalMeshAsset());TStrongObjectPtr<UAnimInstance> Anim(S->GetAnimInstance());const FTransform Transform=S->GetRelativeTransform(),ActorTransform=Owner->GetActorTransform();
 FSnapshot Input;if(!Input.Capture(S,{TEXT("bDisablePostProcessBlueprint")}))return C.Fail(TEXT("snapshot_bound"),TEXT("Component authored-property snapshot bound exceeded."));const bool Before=S->GetDisablePostProcessBlueprint();if(Action==TEXT("post_process")&&Before==Disabled)return C.Finish(true);
 auto ContextStable=[&](){if(!C.Current())return false;if(!Live(KeepOwner.Get())||S->GetOwner()!=KeepOwner.Get()||KeepOwner->IsActorBeingDestroyed()||KeepOwner->GetWorld()!=C.World.Get()||S->GetWorld()!=C.World.Get()||!KeepOwner->GetActorTransform().Equals(ActorTransform,0)||S->GetSkeletalMeshAsset()!=Mesh.Get()||!S->GetRelativeTransform().Equals(Transform,0)||!S->IsRegistered()){C.Error(TEXT("component_context_changed"),TEXT("Owner/world/transform/mesh/registration changed at callback boundary; no subsequent animation writer dispatched. Inspect actual state before another request."));return false;}return true;};
 TGuardValue<bool> Busy(Writing,true);C.Begin(Action==TEXT("tick")?TEXT("tick_animation"):TEXT("set_post_process"));
 if(Action==TEXT("post_process")){S->Modify();if(!Input.Same(C,TEXT("modify_component"))||!ContextStable())return C.Finish(false);if(S->GetDisablePostProcessBlueprint()!=Before)return C.Fail(TEXT("post_process_changed_after_modify"),TEXT("Post-process flag changed in Modify callback; setter not dispatched."));S->SetDisablePostProcessBlueprint(Disabled);}
 else S->TickAnimation(float(Delta),false);
 auto Stable=[&](){return Input.Same(C,TEXT("animation_callback"))&&ContextStable()&&(Action!=TEXT("post_process")||S->GetDisablePostProcessBlueprint()==Disabled);};
 if(!Stable())return C.Fail(TEXT("component_changed_after_callback"),TEXT("Owner/mesh/authored component state changed after engine callback. Inspect actual animation state; no replay."));if(Action==TEXT("tick")&&Refresh){S->RefreshBoneTransforms();if(!Stable())return C.Fail(TEXT("component_changed_after_refresh"),TEXT("Component changed after RefreshBoneTransforms."));}
 C.R->SetBoolField(TEXT("disable_post_process_blueprint_after"),S->GetDisablePostProcessBlueprint());C.R->SetStringField(TEXT("anim_instance_after"),Path(S->GetAnimInstance()));C.R->SetBoolField(TEXT("pose_tick_returned"),Action==TEXT("tick"));C.R->SetBoolField(TEXT("render_or_gpu_completion_verified"),false);C.Known(true);return C.Finish(true);
}
FString RemoveBones(UObject* Object,const FString& Names,bool Children,bool AllowRemove,bool AllowDependentUpdates,bool Execute,bool Save,bool AllowDirty)
{
 FContext C(TEXT("remove_skeleton_bones"),Object);if(!C.Ready)return C.Finish(false);auto* S=Cast<USkeleton>(Object);if(!S)return C.Fail(TEXT("editable_skeleton_required"),TEXT("Standalone /Game Skeleton required."));if(!C.Admit(Save,AllowDirty))return C.Finish(false);
 const auto& Ref=S->GetReferenceSkeleton();if(Ref.GetRawBoneNum()>16384||Ref.GetRawRefBonePose().Num()!=Ref.GetRawBoneNum())return C.Fail(TEXT("bone_bound"),TEXT("At most16384 raw bones with matching raw poses."));for(int32 I=0;I<Ref.GetRawBoneNum();++I)if(Ref.GetRawParentIndex(I)<INDEX_NONE||Ref.GetRawParentIndex(I)>=I)return C.Fail(TEXT("invalid_raw_bone_hierarchy"),TEXT("Raw bone parent must be an earlier bone or INDEX_NONE."));C.R->SetNumberField(TEXT("bones_before"),Ref.GetRawBoneNum());TArray<TSharedPtr<FJsonValue>> Values;if(Names.Len()>65536||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Names),Values)||Values.IsEmpty()||Values.Num()>4096)return C.Fail(TEXT("bone_names_invalid"),TEXT("JSON array of1..4096 unique existing raw bone names required."));
 TSet<FName> Removal;for(const auto& V:Values){FString N;if(!V->TryGetString(N)||N.IsEmpty()||N.Len()>128)return C.Fail(TEXT("invalid_bone_name"),TEXT("Existing bone names1..128 characters required."));const FName Name(*N);const int32 Index=Ref.FindRawBoneIndex(Name);if(Removal.Contains(Name)||Index<0||Ref.GetRawParentIndex(Index)<0)return C.Fail(TEXT("missing_root_or_duplicate_bone"),TEXT("Unknown/duplicate/root bones are refused before Modify."));Removal.Add(Name);}
 if(Children)for(int32 I=1;I<Ref.GetRawBoneNum();++I){const int32 P=Ref.GetRawParentIndex(I);if(P>=0&&Removal.Contains(Ref.GetRawRefBoneInfo()[P].Name))Removal.Add(Ref.GetRawRefBoneInfo()[I].Name);}
 for(int32 I=1;I<Ref.GetRawBoneNum();++I){const int32 P=Ref.GetRawParentIndex(I);if(P>=0&&!Removal.Contains(Ref.GetRawRefBoneInfo()[I].Name)&&Removal.Contains(Ref.GetRawRefBoneInfo()[P].Name))return C.Fail(TEXT("retained_child_bone"),TEXT("Removing a parent requires its complete descendant set or remove_children=true. Engine only removes leaf bones."));}
 TArray<FName> ToRemove;TArray<FString> Removed,ExpectedNames,ExpectedParents;TArray<FTransform> ExpectedPose;TArray<int32> ExpectedRetargetModes;for(int32 I=0;I<Ref.GetRawBoneNum();++I){const FName N=Ref.GetRawRefBoneInfo()[I].Name;if(Removal.Contains(N)){ToRemove.Add(N);Removed.Add(N.ToString());}else{ExpectedNames.Add(N.ToString());const int32 Parent=Ref.GetRawParentIndex(I);ExpectedParents.Add(Parent<0?FString():Ref.GetRawRefBoneInfo()[Parent].Name.ToString());ExpectedPose.Add(Ref.GetRawRefBonePose()[I]);ExpectedRetargetModes.Add(int32(S->GetBoneTranslationRetargetingMode(I)));}}
 const auto SocketsBefore=S->Sockets;FSnapshot Retained;if(SocketsBefore.Num()>4096||!Retained.Capture(S,{TEXT("BoneTree")}))return C.Fail(TEXT("snapshot_bound"),TEXT("Skeleton preservation snapshot bound exceeded."));TArray<FString> OrphanedSockets;for(USkeletalMeshSocket* Socket:SocketsBefore){if(!Socket)continue;if(!Live(Socket)||!Retained.Capture(Socket))return C.Fail(TEXT("socket_not_ready_or_bound"),TEXT("Ready bounded Skeleton sockets required."));if(Removal.Contains(Socket->BoneName))OrphanedSockets.Add(Socket->SocketName.ToString());}C.R->SetArrayField(TEXT("sockets_referencing_removed_bones"),Strings(OrphanedSockets));
 TArray<FString> Loaded;for(TObjectIterator<UAnimationAsset> I;I;++I)if(Live(*I)&&I->GetSkeleton()==S)Loaded.Add(Path(*I));C.R->SetArrayField(TEXT("planned_removed_bones"),Strings(Removed));C.R->SetArrayField(TEXT("loaded_animations_subject_to_validation"),Strings(Loaded));C.R->SetNumberField(TEXT("virtual_bones_before"),S->GetVirtualBones().Num());
 C.R->SetStringField(TEXT("side_effects"),TEXT("Engine hierarchy change refreshes Skeleton GUID/remapping/retarget data/blend profiles and validates all loaded animations. Invalid virtual bones may be removed. Only target package may be saved; dependent assets are never auto-saved. No atomic rollback."));
 if(Execute&&(!AllowRemove||!AllowDependentUpdates))return C.Fail(TEXT("destructive_update_requires_approval"),TEXT("execute=true requires allow_remove=true and allow_dependent_updates=true for hierarchy and loaded-animation side effects."));if(!Execute)return C.Finish(true);
 auto SocketsStable=[&](const TCHAR* Boundary){if(!Retained.Same(C,Boundary))return false;if(S->Sockets!=SocketsBefore){C.Error(TEXT("socket_inventory_changed"),TEXT("Skeleton socket references/order changed during callback; no subsequent hierarchy writer/save dispatched."));return false;}return true;};
 const FString Before=BonesState(S);const auto BeforePose=Ref.GetRawRefBonePose();const FGuid Guid=S->GetGuid();FSnapshot BeforeAuthored;if(!BeforeAuthored.Capture(S))return C.Fail(TEXT("snapshot_bound"),TEXT("Skeleton authored property snapshot exceeded."));TGuardValue<bool> Busy(Writing,true);C.Begin(TEXT("modify_skeleton"));S->Modify();
 bool PoseSame=C.Current()&&S->GetReferenceSkeleton().GetRawRefBonePose().Num()==BeforePose.Num();for(int32 I=0;PoseSame&&I<BeforePose.Num();++I)PoseSame=S->GetReferenceSkeleton().GetRawRefBonePose()[I].Equals(BeforePose[I],0);
 if(!PoseSame||Before!=BonesState(S)||Guid!=S->GetGuid()||!BeforeAuthored.Same(C,TEXT("modify_skeleton")))return C.Fail(TEXT("skeleton_changed_after_modify"),TEXT("Skeleton bone/authored values changed in Modify callback; hierarchy writer/save not dispatched."));
 if(!SocketsStable(TEXT("modify_skeleton")))return C.Finish(false);
 C.R->SetStringField(TEXT("stage"),TEXT("remove_bones_engine"));S->RemoveBonesFromSkeleton(ToRemove,Children);
 auto Stable=[&](){if(!SocketsStable(TEXT("hierarchy_or_save_callback")))return false;const auto& Now=S->GetReferenceSkeleton();if(Now.GetRawBoneNum()!=ExpectedNames.Num()){C.Error(TEXT("bone_inventory_mismatch"),TEXT("Actual remaining bone count differs from request."));return false;}for(int32 I=0;I<Now.GetRawBoneNum();++I){const int32 Parent=Now.GetRawParentIndex(I);const FString ParentName=Parent<0?FString():Now.GetRawRefBoneInfo()[Parent].Name.ToString();if(Now.GetRawRefBoneInfo()[I].Name.ToString()!=ExpectedNames[I]||ParentName!=ExpectedParents[I]||!Now.GetRawRefBonePose()[I].Equals(ExpectedPose[I],0)||int32(S->GetBoneTranslationRetargetingMode(I))!=ExpectedRetargetModes[I]){auto M=MakeShared<J>();M->SetNumberField(TEXT("bone_index"),I);M->SetStringField(TEXT("expected"),ExpectedNames[I]+TEXT(":")+ExpectedParents[I]);M->SetStringField(TEXT("actual"),Now.GetRawRefBoneInfo()[I].Name.ToString()+TEXT(":")+ParentName);M->SetStringField(TEXT("expected_transform"),ExpectedPose[I].ToString());M->SetStringField(TEXT("actual_transform"),Now.GetRawRefBonePose()[I].ToString());C.R->SetObjectField(TEXT("first_mismatch"),M);C.Error(TEXT("remaining_bone_values_changed"),TEXT("Remaining bone name/parent/reference transform/retarget mode changed."));return false;}}return true;};
 if(!Stable())return C.Finish(false);C.R->SetNumberField(TEXT("bones_after"),S->GetReferenceSkeleton().GetRawBoneNum());C.R->SetNumberField(TEXT("virtual_bones_after"),S->GetVirtualBones().Num());C.Known(true);
 if(!C.SaveExact(Save,Stable))return C.Finish(false);C.R->SetStringField(TEXT("stage"),TEXT("complete"));return C.Finish(true);
}
FString Optimus(UObject* Object,bool Compile,bool Execute)
{
 FContext C(Compile?TEXT("compile_optimus_deformer"):TEXT("inspect_optimus_deformer"),Object);if(!C.Ready)return C.Finish(false);auto* D=Cast<UOptimusDeformer>(Object);if(!D)return C.Fail(TEXT("optimus_deformer_required"),TEXT("OptimusDeformer required; prerequisite plugin DeformerGraph must be enabled/loaded."));
 C.R->SetStringField(TEXT("graph_compile_status"),StaticEnum<EOptimusDeformerStatus>()->GetNameStringByValue(int64(D->GetStatus())));C.R->SetBoolField(TEXT("shader_completion_verified"),false);C.R->SetField(TEXT("full_compile_complete"),MakeShared<FJsonValueNull>());TArray<FString> Graphs;for(auto* G:D->GetGraphs())Graphs.Add(Path(G));C.R->SetArrayField(TEXT("graphs"),Strings(Graphs));C.R->SetStringField(TEXT("scope"),TEXT("GetStatus describes graph compilation. CompileEnd precedes asynchronous shader completion; this API never claims GPU/resource readiness or saves the asset."));
 if(!Compile||!Execute)return C.Finish(C.Current());if(!C.Admit(false,false))return C.Finish(false);
 FSnapshot Input;if(!Input.Capture(D,{},true))return C.Fail(TEXT("snapshot_bound"),TEXT("Optimus authored-property snapshot exceeds bounded scope."));
 TArray<TSharedPtr<FJsonValue>> Messages;int32 Total=0;const FDelegateHandle Handle=D->GetCompileMessageDelegate().AddLambda([&](const FOptimusCompilerDiagnostic& V){++Total;if(Messages.Num()>=256)return;auto O=MakeShared<J>();O->SetNumberField(TEXT("level"),int32(V.Level));O->SetStringField(TEXT("message"),V.Message.ToString());O->SetStringField(TEXT("object"),Path(V.Object.Get()));O->SetNumberField(TEXT("line"),V.Line);Messages.Add(Row(O));});
 ON_SCOPE_EXIT{D->GetCompileMessageDelegate().Remove(Handle);};
 TGuardValue<bool> Busy(Writing,true);C.Begin(TEXT("compile_optimus_graph"));C.R->SetBoolField(TEXT("compile_attempted"),true);const bool EngineReturn=D->Compile();C.R->SetBoolField(TEXT("engine_return"),EngineReturn);C.R->SetArrayField(TEXT("diagnostics"),Messages);C.R->SetNumberField(TEXT("diagnostic_count_during_call"),Total);
 if(!Input.Same(C,TEXT("compile_return")))return C.Finish(false);const auto Status=D->GetStatus();const bool Good=EngineReturn&&(Status==EOptimusDeformerStatus::Compiled||Status==EOptimusDeformerStatus::CompiledWithWarnings);C.R->SetStringField(TEXT("graph_compile_status"),StaticEnum<EOptimusDeformerStatus>()->GetNameStringByValue(int64(Status)));C.R->SetBoolField(TEXT("graph_compile_succeeded"),Good);C.Known(true);
 if(!Good)return C.Fail(TEXT("optimus_graph_compile_failed"),TEXT("Engine graph compile failed or ended in Modified/HasErrors. Generated data may have been cleared/changed; no save or automatic retry."));C.R->SetStringField(TEXT("stage"),TEXT("graph_compiled_shader_completion_not_verified"));return C.Finish(true);
}
#if WITH_DEV_AUTOMATION_TESTS
namespace
{
// In-memory, owned /Game fixtures only. No disk writes or existing asset edits.
struct FAuthoringRegressionFixture
{
 TStrongObjectPtr<UPackage> Package;
 TStrongObjectPtr<UObject> Asset;
 explicit FAuthoringRegressionFixture(UClass* Type)
 {
  const FString Name=TEXT("/Game/_MCP_Temp/AuthoringAccess/CubelessTests/Callback_")+FGuid::NewGuid().ToString(EGuidFormats::Digits);
  Package.Reset(CreatePackage(*Name));Asset.Reset(NewObject<UObject>(Package.Get(),Type,TEXT("Fixture"),RF_Public|RF_Standalone|RF_Transactional));
 }
 ~FAuthoringRegressionFixture(){Asset->ClearFlags(RF_Public|RF_Standalone);Package->SetDirtyFlag(false);}
};
TSharedPtr<J> RegressionResult(const FString& Text)
{
 TSharedPtr<J> Result;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text),Result);return Result;
}
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringExplicitTangentsTest,"CubelessMCP.AuthoringAccess.ExplicitTangents",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringExplicitTangentsTest::RunTest(const FString& Parameters)
{
 FAuthoringRegressionFixture Fixture(UCurveFloat::StaticClass());auto* CurveAsset=CastChecked<UCurveFloat>(Fixture.Asset.Get());
 CurveAsset->FloatCurve.DefaultValue=17;CurveAsset->FloatCurve.PreInfinityExtrap=RCCE_Linear;
 const FString Input=TEXT("[{\"time\":0,\"value\":2,\"interp_mode\":2,\"arrive_tangent\":3,\"leave_tangent\":4},{\"time\":1,\"value\":5,\"interp_mode\":2,\"arrive_tangent\":6,\"leave_tangent\":7},{\"time\":2,\"value\":1,\"interp_mode\":0,\"tangent_mode\":1,\"arrive_tangent\":8,\"leave_tangent\":9,\"tangent_weight_mode\":3,\"arrive_tangent_weight\":0.75,\"leave_tangent_weight\":1.25}]");
 auto Result=RegressionResult(SetCurve(CurveAsset,Input,false,true,false,false));
 if(!TestTrue(TEXT("Curve edit returned JSON"),Result.IsValid()))return false;
 TestTrue(TEXT("Explicit key edit succeeds"),Result->GetBoolField(TEXT("success")));
 const auto& Keys=CurveAsset->FloatCurve.GetConstRefOfKeys();if(!TestEqual(TEXT("All keys retained"),Keys.Num(),3))return false;
 TestEqual(TEXT("First auto arrive tangent"),Keys[0].ArriveTangent,3.f);TestEqual(TEXT("Middle auto arrive tangent"),Keys[1].ArriveTangent,6.f);
 TestEqual(TEXT("First auto leave tangent"),Keys[0].LeaveTangent,4.f);TestEqual(TEXT("Middle auto leave tangent"),Keys[1].LeaveTangent,7.f);
 TestEqual(TEXT("Linear/manual arrive tangent"),Keys[2].ArriveTangent,8.f);TestEqual(TEXT("Linear/manual leave tangent"),Keys[2].LeaveTangent,9.f);
 TestEqual(TEXT("Weight retained"),Keys[2].LeaveTangentWeight,1.25f);TestEqual(TEXT("Default retained"),CurveAsset->FloatCurve.DefaultValue,17.f);
 TestEqual(TEXT("Infinity retained"),int32(CurveAsset->FloatCurve.PreInfinityExtrap),int32(RCCE_Linear));
 auto Noop=RegressionResult(SetCurve(CurveAsset,Input,false,true,false,false));TestTrue(TEXT("Identical request is a no-op"),Noop.IsValid()&&Noop->GetBoolField(TEXT("success"))&&!Noop->GetBoolField(TEXT("mutation_attempted")));
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringSocketCallbackTest,"CubelessMCP.AuthoringAccess.SocketModifyBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringSocketCallbackTest::RunTest(const FString& Parameters)
{
 for(int32 Case=0;Case<2;++Case)
 {
  FAuthoringRegressionFixture Fixture(USkeleton::StaticClass());auto* Skeleton=CastChecked<USkeleton>(Fixture.Asset.Get());
  {FReferenceSkeletonModifier Bones(Skeleton);Bones.Add(FMeshBoneInfo(TEXT("root"),TEXT("root"),INDEX_NONE),FTransform::Identity);Bones.Add(FMeshBoneInfo(TEXT("leaf"),TEXT("leaf"),0),FTransform::Identity);}
  auto* BoneTree=FindFProperty<FArrayProperty>(Skeleton->GetClass(),TEXT("BoneTree"));if(!TestNotNull(TEXT("Reflected bone tree"),BoneTree))return false;FScriptArrayHelper(BoneTree,BoneTree->ContainerPtrToValuePtr<void>(Skeleton)).Resize(2);
  auto* Socket=NewObject<USkeletalMeshSocket>(Skeleton);Socket->SocketName=TEXT("KeptSocket");Socket->BoneName=TEXT("root");Skeleton->Sockets.Add(Socket);
  const FGuid BeforeGuid=Skeleton->GetGuid();bool Fired=false;
  const FDelegateHandle Hook=FCoreUObjectDelegates::OnObjectModified.AddLambda([&](UObject* O){if(O!=Skeleton||Fired)return;Fired=true;if(Case==0)Socket->RelativeLocation.X=23;else Skeleton->Sockets.Reset();});
  ON_SCOPE_EXIT{FCoreUObjectDelegates::OnObjectModified.Remove(Hook);};
  auto Result=RegressionResult(RemoveBones(Skeleton,TEXT("[\"leaf\"]"),false,true,true,true,true,true));
  if(!TestTrue(TEXT("Removal returned JSON"),Result.IsValid()))return false;
  TestTrue(TEXT("Modify callback ran"),Fired);TestFalse(TEXT("Callback conflict fails"),Result->GetBoolField(TEXT("success")));
  TestEqual(TEXT("Stopped before hierarchy writer"),Result->GetStringField(TEXT("stage")),FString(TEXT("modify_skeleton")));
  TestEqual(TEXT("Bones not removed"),Skeleton->GetReferenceSkeleton().GetRawBoneNum(),2);TestTrue(TEXT("Hierarchy GUID unchanged"),Skeleton->GetGuid()==BeforeGuid);
  TestFalse(TEXT("Unknown callback state is not asserted known"),Result->GetBoolField(TEXT("change_state_known")));
  TestFalse(TEXT("Save never attempted"),Result->GetBoolField(TEXT("save_attempted")));TestFalse(TEXT("Nothing saved"),Result->GetBoolField(TEXT("saved")));
  TestTrue(TEXT("Callback effect is retained, not rolled back"),Case==0?Socket->RelativeLocation.X==23:Skeleton->Sockets.IsEmpty());
 }
 return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringComponentCallbackTest,"CubelessMCP.AuthoringAccess.ComponentModifyBoundary",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringComponentCallbackTest::RunTest(const FString& Parameters)
{
 if(!TestTrue(TEXT("Idle Editor world required"),Idle()))return false;UWorld* World=GEditor->GetEditorWorldContext().World();if(!TestNotNull(TEXT("Editor world"),World))return false;
 const bool DirtyBefore=World->GetPackage()->IsDirty();
 for(int32 Case=0;Case<2;++Case)
 {
  FActorSpawnParameters Spawn;Spawn.ObjectFlags=RF_Transient|RF_Transactional;Spawn.Name=MakeUniqueObjectName(World,AActor::StaticClass(),TEXT("CubelessAuthoringCallback"));AActor* Actor=World->SpawnActor<AActor>(Spawn);if(!TestNotNull(TEXT("Owned fixture Actor"),Actor))return false;
  ON_SCOPE_EXIT{World->DestroyActor(Actor);World->GetPackage()->SetDirtyFlag(DirtyBefore);};
  auto* Root=NewObject<USceneComponent>(Actor,NAME_None,RF_Transient);Actor->AddInstanceComponent(Root);Actor->SetRootComponent(Root);Root->RegisterComponent();
  auto* Component=NewObject<USkeletalMeshComponent>(Actor,NAME_None,RF_Transient|RF_Transactional);Actor->AddInstanceComponent(Component);Component->SetupAttachment(Root);Component->RegisterComponent();
  const bool Before=Component->GetDisablePostProcessBlueprint();bool Fired=false;
  const FDelegateHandle Hook=FCoreUObjectDelegates::OnObjectModified.AddLambda([&](UObject* O){if(O!=Component||Fired)return;Fired=true;if(Case==0)Component->UnregisterComponent();else Actor->SetActorLocation(FVector(17,31,47));});
  ON_SCOPE_EXIT{FCoreUObjectDelegates::OnObjectModified.Remove(Hook);};
  auto Result=RegressionResult(SkeletalComponent(Component,TEXT("post_process"),!Before,0,false,true));
  if(!TestTrue(TEXT("Component edit returned JSON"),Result.IsValid()))return false;
  TestTrue(TEXT("Modify callback ran"),Fired);TestFalse(TEXT("Context conflict fails"),Result->GetBoolField(TEXT("success")));
  TestEqual(TEXT("Post-process setter not dispatched"),Component->GetDisablePostProcessBlueprint(),Before);
  TestEqual(TEXT("Context failure reported"),Result->GetStringField(TEXT("error_code")),FString(TEXT("component_context_changed")));
  TestFalse(TEXT("Callback state remains unknown"),Result->GetBoolField(TEXT("change_state_known")));TestFalse(TEXT("No save attempted"),Result->GetBoolField(TEXT("save_attempted")));
  TestTrue(TEXT("Callback effect retained"),Case==0?!Component->IsRegistered():Actor->GetActorLocation().Equals(FVector(17,31,47),0));
 }
 return true;
}


IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringNonFiniteCurveTest,"CubelessMCP.AuthoringAccess.NonFiniteCurve",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringNonFiniteCurveTest::RunTest(const FString& Parameters)
{
 FAuthoringRegressionFixture Fixture(UCurveFloat::StaticClass());auto* F=CastChecked<UCurveFloat>(Fixture.Asset.Get());
 const FKeyHandle H=F->FloatCurve.AddKey(0,2);const float BadValues[]={std::numeric_limits<float>::quiet_NaN(),std::numeric_limits<float>::infinity(),-std::numeric_limits<float>::infinity()};
 for(float Bad:BadValues)for(int32 Field=0;Field<7;++Field)
 {
  F->FloatCurve.DefaultValue=17;F->FloatCurve.GetKey(H)=FRichCurveKey(0,2);auto& K=F->FloatCurve.GetKey(H);
  float* Fields[]={&F->FloatCurve.DefaultValue,&K.Time,&K.Value,&K.ArriveTangent,&K.LeaveTangent,&K.ArriveTangentWeight,&K.LeaveTangentWeight};*Fields[Field]=Bad;
  for(bool Write:{false,true})
  {
   auto Result=RegressionResult(Write?SetCurve(F,TEXT("[{\"time\":0,\"value\":9}]"),true,true,true,true):Curve(F));
   if(!TestTrue(TEXT("Non-finite data returns valid JSON"),Result.IsValid()))return false;
   TestFalse(TEXT("Non-finite data is refused"),Result->GetBoolField(TEXT("success")));
   TestEqual(TEXT("Specific invalid-data diagnosis"),Result->GetStringField(TEXT("error_code")),FString(TEXT("non_finite_curve_data")));
   TestFalse(TEXT("No writer entered"),Result->GetBoolField(TEXT("mutation_attempted")));TestFalse(TEXT("No save attempted"),Result->GetBoolField(TEXT("save_attempted")));
   TestTrue(TEXT("Malformed data retained without repair"),FMath::IsNaN(Bad)?FMath::IsNaN(*Fields[Field]):*Fields[Field]==Bad);
  }
 }
 F->FloatCurve.DefaultValue=TNumericLimits<float>::Max();F->FloatCurve.GetKey(H)=FRichCurveKey(0,2);
 auto Normal=RegressionResult(Curve(F));if(!TestTrue(TEXT("Finite sentinel returns JSON"),Normal.IsValid()))return false;
 TestTrue(TEXT("Normal FLT_MAX default is valid"),Normal->GetBoolField(TEXT("success")));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringNonemptyTimelinesTest,"CubelessMCP.AuthoringAccess.NonemptyTimelines",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringNonemptyTimelinesTest::RunTest(const FString& Parameters)
{
 FAuthoringRegressionFixture Fixture(UBlueprint::StaticClass());auto* BP=CastChecked<UBlueprint>(Fixture.Asset.Get());
 auto* T=NewObject<UTimelineTemplate>(BP,TEXT("MotionTimeline"));BP->Timelines.Add(T);T->TimelineLength=3.5f;
 FTTFloatTrack Float;Float.SetTrackName(TEXT("Strength"),T);Float.CurveFloat=NewObject<UCurveFloat>(T,TEXT("StrengthCurve"));Float.CurveFloat->FloatCurve.AddKey(0,1);T->FloatTracks.Add(Float);
 FTTVectorTrack Vector;Vector.SetTrackName(TEXT("Offset"),T);Vector.bIsExternalCurve=true;Vector.CurveVector=NewObject<UCurveVector>(BP,TEXT("OffsetCurve"));T->VectorTracks.Add(Vector);
 FTTLinearColorTrack Color;Color.SetTrackName(TEXT("Tint"),T);Color.CurveLinearColor=NewObject<UCurveLinearColor>(T,TEXT("TintCurve"));T->LinearColorTracks.Add(Color);
 FTTEventTrack Event;Event.SetTrackName(TEXT("Trigger"),T);Event.CurveKeys=NewObject<UCurveFloat>(T,TEXT("TriggerCurve"));T->EventTracks.Add(Event);
 const bool Dirty=Fixture.Package->IsDirty();auto Result=RegressionResult(Timelines(BP));if(!TestTrue(TEXT("Timeline response JSON"),Result.IsValid()))return false;
 TestTrue(TEXT("Timeline read succeeds"),Result->GetBoolField(TEXT("success")));const auto& Rows=Result->GetArrayField(TEXT("timelines"));if(!TestEqual(TEXT("Nonempty timeline present"),Rows.Num(),1))return false;
 auto Row=Rows[0]->AsObject();TestEqual(TEXT("Timeline length"),Row->GetNumberField(TEXT("length")),3.5);const auto& Tracks=Row->GetArrayField(TEXT("tracks"));if(!TestEqual(TEXT("All four track kinds"),Tracks.Num(),4))return false;
 const FString Kinds[]={TEXT("float"),TEXT("vector"),TEXT("linear_color"),TEXT("event")};const FString Names[]={TEXT("Strength"),TEXT("Offset"),TEXT("Tint"),TEXT("Trigger")};UObject* Curves[]={Float.CurveFloat,Vector.CurveVector,Color.CurveLinearColor,Event.CurveKeys};
 for(int32 I=0;I<4;++I){auto Track=Tracks[I]->AsObject();TestEqual(TEXT("Track kind"),Track->GetStringField(TEXT("kind")),Kinds[I]);TestEqual(TEXT("Track name"),Track->GetStringField(TEXT("name")),Names[I]);TestEqual(TEXT("Exact curve reference"),Track->GetStringField(TEXT("curve")),Path(Curves[I]));TestEqual(TEXT("External flag"),Track->GetBoolField(TEXT("external")),I==1);}
 TestEqual(TEXT("Read preserves dirty state"),Fixture.Package->IsDirty(),Dirty);TestFalse(TEXT("No mutation"),Result->GetBoolField(TEXT("mutation_attempted")));return true;
}

IMPLEMENT_SIMPLE_AUTOMATION_TEST(FAuthoringSpecializedNodesTest,"CubelessMCP.AuthoringAccess.SpecializedAnimNodes",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FAuthoringSpecializedNodesTest::RunTest(const FString& Parameters)
{
 FAuthoringRegressionFixture Fixture(UBlueprint::StaticClass());auto* BP=CastChecked<UBlueprint>(Fixture.Asset.Get());
 auto* Graph=NewObject<UEdGraph>(BP,TEXT("OwnedAnimGraph"));BP->FunctionGraphs.Add(Graph);
 auto* Rig=NewObject<UAnimGraphNode_ControlRig>(Graph);Rig->CreateNewGuid();Rig->NodeComment=TEXT("Rig inspection fixture");Rig->Node.SetControlRigClass(UControlRig::StaticClass());Graph->Nodes.Add(Rig);
 auto* Driver=NewObject<UAnimGraphNode_PoseDriver>(Graph);Driver->CreateNewGuid();Driver->NodeComment=TEXT("Pose driver fixture");FPoseDriverTarget A;A.DrivenName=TEXT("Smile_L");FPoseDriverTarget B;B.DrivenName=TEXT("Jaw_Open");Driver->Node.PoseTargets={A,B};Graph->Nodes.Add(Driver);
 const bool Dirty=Fixture.Package->IsDirty();
 for(UObject* Target:{static_cast<UObject*>(BP),static_cast<UObject*>(Graph)})
 {
  auto Result=RegressionResult(AnimNodes(Target));if(!TestTrue(TEXT("Specialized response JSON"),Result.IsValid()))return false;TestTrue(TEXT("Specialized lookup succeeds"),Result->GetBoolField(TEXT("success")));
  const auto& Rows=Result->GetArrayField(TEXT("nodes"));if(!TestEqual(TEXT("Two specialized nodes"),Rows.Num(),2))return false;
  auto RigRow=Rows[0]->AsObject();TestEqual(TEXT("Actual ControlRig class"),RigRow->GetStringField(TEXT("control_rig_class")),Path(UControlRig::StaticClass()));TestEqual(TEXT("Rig comment"),RigRow->GetStringField(TEXT("comment")),Rig->NodeComment);
  auto DriverRow=Rows[1]->AsObject();const auto& Names=DriverRow->GetArrayField(TEXT("pose_driver_driven_names"));if(!TestEqual(TEXT("Pose target count"),Names.Num(),2))return false;TestEqual(TEXT("First driven name"),Names[0]->AsString(),FString(TEXT("Smile_L")));TestEqual(TEXT("Second driven name"),Names[1]->AsString(),FString(TEXT("Jaw_Open")));TestEqual(TEXT("Driver comment"),DriverRow->GetStringField(TEXT("comment")),Driver->NodeComment);TestFalse(TEXT("Read-only inspection"),Result->GetBoolField(TEXT("mutation_attempted")));
 }
 TestEqual(TEXT("Source Blueprint unchanged"),Fixture.Package->IsDirty(),Dirty);return true;
}

#endif
}
