#include "Compatibility/UE58/CubelessAnimationProductionApi.h"
#include "Compatibility/UE58/CubelessAnimationStateEditingApi.h"
#include "Compatibility/UE58/CubelessAnimationEditingApi.h"
#include "Animation/AnimSequence.h"
#include "Animation/AnimMontage.h"
#include "Animation/AnimData/IAnimationDataModel.h"
#include "UObject/UObjectIterator.h"
#include "Editor.h"
#include "Compatibility/UE58/DomainExpansion/CubelessDomainDependencyAudit.h"
#include "Interfaces/IPluginManager.h"
#include "PluginDescriptor.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "Animation/MirrorDataTable.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Misc/PackageName.h"
#include "Misc/CoreMisc.h"
#include "HAL/FileManager.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/ArchiveUObject.h"
#include "UObject/SoftObjectPtr.h"
#include "UObject/Package.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "PoseSearch/PoseSearchSchema.h"
#include "PoseSearch/PoseSearchDatabase.h"
#include "PoseSearch/PoseSearchDerivedData.h"
#include "PoseSearch/PoseSearchFeatureChannel_Position.h"
#include "PoseSearch/PoseSearchFeatureChannel_Velocity.h"
#include "PoseSearch/PoseSearchFeatureChannel_Heading.h"
#include "PoseSearch/PoseSearchFeatureChannel_Trajectory.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimLayerInterface.h"
#include "Animation/AnimClassInterface.h"
#include "AnimGraphNode_UseCachedPose.h"
#include "AnimGraphNode_SaveCachedPose.h"
#include "AnimGraphNode_Slot.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_Root.h"
#include "AnimationGraph.h"
#include "AnimationGraphSchema.h"
#include "K2Node_CallFunction.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionResult.h"
#include "K2Node_Self.h"
#include "K2Node_IfThenElse.h"
#include "EdGraphSchema_K2.h"
#include "AnimExecutionContextLibrary.h"
#include "PoseSearch/MotionMatchingAnimNodeLibrary.h"
#include "AnimGraphNode_LinkedAnimLayer.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/Kismet2NameValidators.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphPin.h"
#include "Chooser.h"
#include "ChooserFunctionLibrary.h"
#include "ChooserPropertyAccess.h"
#include "ObjectChooser_Asset.h"
#include "BoolColumn.h"
#include "FloatRangeColumn.h"
#include "RandomizeColumn.h"
#include "OutputObjectColumn.h"

namespace Cubeless::UE58::AnimationProduction
{
namespace
{
using J=FJsonObject;using Snap=TMap<FString,FString>;
bool Writing=false;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_WillBeLoaded|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Idle(){return IsInGameThread()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&!GIsImportingT3D&&!IsEngineExitRequested()&&GEditor&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor;}
FString Path(const UObject* O){return O?O->GetPathName():FString();}
FString Encode(TSharedRef<J> R){bool Unobserved=false;if(R->TryGetBoolField(TEXT("input_acquisition_unobserved_load"),Unobserved)&&Unobserved){R->SetBoolField(TEXT("native_change_state_known"),R->GetBoolField(TEXT("change_state_known")));R->SetBoolField(TEXT("change_state_known"),false);bool Changed=false;R->TryGetBoolField(TEXT("changed"),Changed);if(!Changed)R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R->SetStringField(TEXT("retry_risk"),TEXT("inspect_actual_target_after_input_load"));}FString S;FJsonSerializer::Serialize(R,TJsonWriterFactory<>::Create(&S));return S;}
TSharedRef<J> Result(const FString& Action){auto R=MakeShared<J>();R->SetStringField(TEXT("operation"),Action);R->SetStringField(TEXT("stage"),TEXT("preflight"));for(const TCHAR* K:{TEXT("success"),TEXT("mutation_attempted"),TEXT("changed"),TEXT("compile_attempted"),TEXT("save_attempted"),TEXT("saved"),TEXT("automatic_retry"),TEXT("atomic_rollback")})R->SetBoolField(K,false);R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("save_state_known"),true);return R;}
FString Fail(TSharedRef<J> R,const FString& E){R->SetStringField(TEXT("error"),E);if(!R->GetBoolField(TEXT("change_state_known")))R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R->SetStringField(TEXT("retry_risk"),!R->GetBoolField(TEXT("change_state_known"))?TEXT("inspect_actual_target"):R->GetBoolField(TEXT("mutation_attempted"))?TEXT("mutation_attempted"):TEXT("none_observed"));return Encode(R);}
bool Kind(UObject* O){return O&&(O->GetClass()==UPoseSearchSchema::StaticClass()||O->GetClass()==UPoseSearchDatabase::StaticClass()||O->GetClass()==UChooserTable::StaticClass());}
bool RuntimeReference(UObject* O)
{
 if(!Live(O)||IsEditorOnlyObject(O)||O->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly)||DomainExpansion::FDomainDependencyAudit::IsMCPDependencyReference(Path(O)))return false;
 UClass* C=Cast<UClass>(O);if(!C)C=O->GetClass();
 for(;C;C=C->GetSuperClass())
 {
  const FString Package=C->GetOutermost()->GetName();if(DomainExpansion::FDomainDependencyAudit::IsMCPDependencyReference(Package))return false;
  if(auto* Default=C->GetDefaultObject(false);Default&&IsEditorOnlyObject(Default))return false;
  if(Package.StartsWith(TEXT("/Script/"))){const FName Module(*Package.Mid(8));if(Module==TEXT("UnrealEd")||Module==TEXT("BlueprintGraph")||Module==TEXT("AnimGraph")||Module==TEXT("Kismet"))return false;auto Plugin=IPluginManager::Get().GetModuleOwnerPlugin(Module);if(Plugin.IsValid())for(const auto& Desc:Plugin->GetDescriptor().Modules)if(Desc.Name==Module&&Desc.Type!=EHostType::Runtime&&Desc.Type!=EHostType::RuntimeNoCommandlet&&Desc.Type!=EHostType::RuntimeAndProgram&&Desc.Type!=EHostType::CookedOnly&&Desc.Type!=EHostType::ClientOnly&&Desc.Type!=EHostType::ServerOnly)return false;}
 }
 return true;
}
bool RuntimeAudit(UObject* Root,FString& Error,TSharedPtr<J> Observation=nullptr);
UObject* Reference(const FString& P,TSharedPtr<J> Observation=nullptr,bool RuntimeSoft=false)
{
 if(P.Len()>1023||(!RuntimeSoft&&!P.StartsWith(TEXT("/Game/"))&&!P.StartsWith(TEXT("/Script/"))))return nullptr;for(const TCHAR C:P)if(C<32||C=='\''||C=='"'||C=='\\'||C==':')return nullptr;
 FString Canonical=P;if(!P.Contains(TEXT(".")))Canonical+=TEXT(".")+FPackageName::GetLongPackageAssetName(P);if(RuntimeSoft&&!FPackageName::IsValidObjectPath(Canonical))return nullptr;
 const FString Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);TSharedPtr<J> Begin,End;
 const FString BeginText=AnimationEditing::BeginAcquisition(Canonical,Token);
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(BeginText),Begin)||!Begin||!Begin->GetBoolField(TEXT("success"))){if(Observation)Observation->SetStringField(TEXT("input_acquisition_error"),BeginText);return nullptr;}
 if(Observation)Observation->SetBoolField(TEXT("input_lookup_attempted"),true);UObject* Resolved=FindObject<UObject>(nullptr,*Canonical);const bool Pending=Resolved&&!Live(Resolved);const bool Attempt=Resolved==nullptr;
 // Never force a second loader against an already discovered pending object.
 // The owning engine load may finish later; this request has no writer entry.
 if(Pending){if(Observation){Observation->SetBoolField(TEXT("input_readiness_rejected"),true);Observation->SetStringField(TEXT("input_not_ready_path"),Canonical);}Resolved=nullptr;}
 if(Attempt){if(Observation){Observation->SetBoolField(TEXT("input_load_attempted"),true);Observation->SetBoolField(TEXT("input_acquisition_unobserved_load"),true);}Resolved=StaticLoadObject(UObject::StaticClass(),nullptr,*Canonical);}
 const FString EndText=AnimationEditing::EndAcquisition(Token,Attempt);
 if(Observation)Observation->SetStringField(TEXT("input_acquisition_observation"),EndText);
 if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(EndText),End)||!End||!End->GetBoolField(TEXT("success"))){if(Observation){Observation->SetStringField(TEXT("input_acquisition_error"),EndText);Observation->SetBoolField(TEXT("change_state_known"),false);}return nullptr;}
 return Resolved;
}
bool Target(UObject* O){return Live(O)&&Kind(O)&&!O->IsTemplate()&&O->GetOuter()==O->GetOutermost()&&O->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))&&!O->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap);}
struct Lease
{
 TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TWeakObjectPtr<UObject> Serial;FString Name;
 explicit Lease(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Serial(O),Name(Path(O)){}
 bool Current()const{auto* O=Object.Get();return Serial.IsValid()&&Serial.Get()==O&&Live(O)&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&Path(O)==Name&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists);}
};
// Runtime serialization excludes editor-only import/source metadata. Capture
// reference serials/strong holds immediately; resolve runtime soft assets only
// after Serialize returns, never by recursive loading in its collector callback.
bool RuntimeAudit(UObject* Root,FString& Error,TSharedPtr<J> Observation)
{
 struct Archive final:FArchiveUObject
 {
  TArray<Lease> Refs;TArray<FSoftObjectPath> Soft;FString Error;
  Archive(){ArIsObjectReferenceCollector=true;ArIgnoreOuterRef=true;ArIgnoreArchetypeRef=true;ArIgnoreClassRef=false;SetFilterEditorOnly(true);}
  using FArchiveUObject::operator<<;
  virtual FArchive& operator<<(UObject*& O)override{if(O){if(!Live(O)||Refs.Num()>=4096){Error=TEXT("Invalid/excess runtime reference during collection");return *this;}if(!Refs.ContainsByPredicate([&](const Lease& H){return H.Object.Get()==O;}))Refs.Emplace(O);}return *this;}
  virtual FArchive& operator<<(FSoftObjectPath& P)override{if(!P.IsNull()){if(DomainExpansion::FDomainDependencyAudit::IsMCPDependencyReference(P.ToString()))Error=TEXT("MCP soft reference: ")+P.ToString();else if(Soft.Num()>=4096)Error=TEXT("Runtime soft-reference budget exceeded");else Soft.AddUnique(P);}return *this;}
  virtual FArchive& operator<<(FSoftObjectPtr& P)override{FSoftObjectPath Value=P.ToSoftObjectPath();return *this<<Value;}
 };
 if(!Live(Root)||!Idle()){Error=TEXT("Live runtime audit root and idle Editor required");return false;}
 TArray<Lease> Queue;Queue.Emplace(Root);TSet<UObject*> Seen,Scheduled;Scheduled.Add(Root);
 for(int I=0;I<Queue.Num();++I)
 {
  if(!Queue[I].Current()){Error=TEXT("Collected runtime reference identity changed");return false;}
  UObject* O=Queue[I].Object.Get();if(Seen.Contains(O)||O->IsA<UPackage>())continue;if(O!=Root&&IsEditorOnlyObject(O)&&!DomainExpansion::FDomainDependencyAudit::IsMCPDependencyReference(Path(O))&&!DomainExpansion::FDomainDependencyAudit::IsMCPDependencyReference(Path(O->GetClass())))continue;if(Seen.Num()>4096||!RuntimeReference(O)){Error=TEXT("Invalid, Editor-only or MCP runtime reference: ")+Path(O)+FString::Printf(TEXT(" (editor_only=%d)"),int(IsEditorOnlyObject(O)));return false;}Seen.Add(O);
  if(auto* C=Cast<UClass>(O);C&&!C->ClassGeneratedBy)continue;
  Archive Ar;O->Serialize(Ar);
  if(!Ar.Error.IsEmpty()||!Queue[I].Current()||!Idle()){Error=Ar.Error.IsEmpty()?TEXT("Runtime reference/context changed during serialization"):Ar.Error;return false;}
  for(auto& Ref:Ar.Refs){if(!Ref.Current()){Error=TEXT("Child runtime reference changed during collection");return false;}if(!Scheduled.Contains(Ref.Object.Get())){if(Queue.Num()>=4096){Error=TEXT("Runtime reference queue budget exceeded");return false;}Scheduled.Add(Ref.Object.Get());Queue.Add(MoveTemp(Ref));}}
  for(auto& Soft:Ar.Soft)
  {
   if(Soft.ToString().Len()>=NAME_SIZE||!Soft.GetSubPathString().IsEmpty()){Error=TEXT("Bounded top-level runtime soft asset required: ")+Soft.ToString();return false;}
    UObject* Resolved=Soft.ResolveObject();if(!Resolved)Resolved=Reference(Soft.ToString(),Observation,true);
   if(!Idle()||!Live(Resolved)||!Queue[I].Current()){Error=TEXT("Runtime soft reference unresolved or changed: ")+Soft.ToString();return false;}
   if(!RuntimeReference(Resolved)){Error=TEXT("Runtime soft asset is Editor-only/MCP: ")+Soft.ToString();return false;}if(!Scheduled.Contains(Resolved)){if(Queue.Num()>=4096){Error=TEXT("Runtime reference queue budget exceeded");return false;}Scheduled.Add(Resolved);Queue.Emplace(Resolved);}
  }
 }
 return true;
}
bool ExactValue(FProperty* P,const void* Data,const FString& Key,Snap& Out,int Depth=0)
{
 if(Depth>16||Out.Num()>262144)return false;
 if(auto* N=CastField<FNumericProperty>(P);N&&N->IsFloatingPoint()){const double D=N->GetFloatingPointPropertyValue(Data);if(!FMath::IsFinite(D))return false;Out.Add(Key+TEXT(".$exact"),FString::Printf(TEXT("%.17g"),D));}
 else if(auto* A=CastField<FArrayProperty>(P)){FScriptArrayHelper H(A,Data);if(H.Num()>65536)return false;for(int I=0;I<H.Num();++I)if(!ExactValue(A->Inner,H.GetRawPtr(I),Key+FString::Printf(TEXT("[%d]"),I),Out,Depth+1))return false;}
 else if(auto* S=CastField<FStructProperty>(P))
 {
  const UStruct* Type=S->Struct;const void* Ptr=Data;
  if(Type==FInstancedStruct::StaticStruct()){const auto& I=*static_cast<const FInstancedStruct*>(Data);if(!I.IsValid())return true;Type=I.GetScriptStruct();Ptr=I.GetMemory();}
  for(TFieldIterator<FProperty> F(Type);F;++F){if(F->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated))continue;for(int I=0;I<F->ArrayDim;++I)if(!ExactValue(*F,F->ContainerPtrToValuePtr<void>(Ptr,I),Key+TEXT(".")+F->GetName()+FString::FromInt(I),Out,Depth+1))return false;}
 }
 return true;
}
void Props(UObject* O,const FString& Prefix,Snap& Out)
{
 for(TFieldIterator<FProperty> P(O->GetClass());P;++P)
 {
  if(P->HasAnyPropertyFlags(CPF_Transient|CPF_Deprecated|CPF_DuplicateTransient)||P->GetFName()==TEXT("ErrorMsg")||P->GetFName()==TEXT("ErrorType")||P->GetFName()==TEXT("bHasCompilerMessage"))continue;
  if(!P->HasAnyPropertyFlags(CPF_Edit)&&!(Cast<USkeleton>(O)&&(P->GetFName()==TEXT("SlotGroups")||P->GetFName()==TEXT("BoneTree")||P->GetFName()==TEXT("VirtualBones")))&&P->GetFName()!=TEXT("DatabaseAnimationAssets")&&P->GetFName()!=TEXT("ResultsStructs")&&P->GetFName()!=TEXT("ColumnsStructs")&&P->GetFName()!=TEXT("DisabledRows")&&P->GetFName()!=TEXT("FallbackResult"))continue;
  // Schema finalization regenerates role bone references, not authored role identity.
  if(auto* S=Cast<UPoseSearchSchema>(O);S&&P->GetFName()==TEXT("Skeletons"))
  {int I=0;for(const auto& Role:S->GetRoledSkeletons())Out.Add(Prefix+FString::Printf(TEXT(".Skeleton%d"),I++),Path(Role.Skeleton)+TEXT("|")+Path(Role.MirrorDataTable)+TEXT("|")+Role.Role.ToString());continue;}
  for(int I=0;I<P->ArrayDim;++I){const FString Key=Prefix+TEXT(".")+P->GetName()+(I?FString::Printf(TEXT("[%d]"),I):FString());void* Data=P->ContainerPtrToValuePtr<void>(O,I);FString V;P->ExportTextItem_Direct(V,Data,nullptr,O,PPF_None);Out.Add(Key,V);if(!ExactValue(*P,Data,Key,Out)){Out.Add(TEXT("$invalid_precision_snapshot"),TEXT("true"));return;}}
 }
}
bool Capture(UObject* O,Snap& Out,bool Input=false)
{
 Out.Reset();if(!Target(O)){auto* Owner=O?O->GetTypedOuter<UChooserTable>():nullptr;while(Owner&&Owner->GetTypedOuter<UChooserTable>())Owner=Owner->GetTypedOuter<UChooserTable>();if(!Input||!Live(O)||!Kind(O)||O->IsTemplate()||!Target(Owner)||O->GetOutermost()!=Owner->GetOutermost())return false;}Props(O,TEXT("asset"),Out);
 if(auto* S=Cast<UPoseSearchSchema>(O))
 {
  auto* P=FindFProperty<FArrayProperty>(S->GetClass(),TEXT("Channels"));if(!P)return false;FScriptArrayHelper A(P,P->ContainerPtrToValuePtr<void>(S));auto* Inner=CastField<FObjectPropertyBase>(P->Inner);if(!Inner||A.Num()>128)return false;
  for(int I=0;I<A.Num();++I){auto* C=Inner->GetObjectPropertyValue(A.GetRawPtr(I));if(!Live(C)||!C->IsIn(S))return false;Props(C,FString::Printf(TEXT("channel.%d"),I),Out);}
 }
 int64 Bytes=0;for(const auto& V:Out)Bytes+=(V.Key.Len()+V.Value.Len())*2;return !Out.Contains(TEXT("$invalid_precision_snapshot"))&&Bytes<=4*1024*1024;
}
bool Same(const Snap& Expected,const Snap& Actual,TSharedRef<J> R)
{
 TArray<FString> Keys;Expected.GetKeys(Keys);for(const auto& P:Actual)Keys.AddUnique(P.Key);Keys.Sort();for(const auto& K:Keys){auto* E=Expected.Find(K);auto* A=Actual.Find(K);if(!E||!A||*E!=*A){R->SetStringField(TEXT("first_mismatch_property"),K);R->SetStringField(TEXT("expected"),E?E->Left(2048):TEXT("<absent>"));R->SetStringField(TEXT("actual"),A?A->Left(2048):TEXT("<absent>"));return false;}}return true;
}
bool CaptureInput(UObject* O,Snap& Out,FString& Error)
{
 Out.Reset();if(!Live(O)){Error=TEXT("Input lifetime invalid");return false;}
 if(Kind(O))return Capture(O,Out,true);
 if(O->IsA<UAnimSequenceBase>())return AnimationEditing::CaptureReadOnlySource(O,Out,Error);
 if(auto* Skeleton=Cast<USkeleton>(O)){
  Props(Skeleton,TEXT("input"),Out);const auto& Ref=Skeleton->GetReferenceSkeleton();if(Ref.GetRawBoneNum()>4096)return false;
  for(int I=0;I<Ref.GetRawBoneNum();++I){const auto& T=Ref.GetRawRefBonePose()[I];const auto P=T.GetTranslation(),S=T.GetScale3D();const auto Q=T.GetRotation();FString V=Ref.GetBoneName(I).ToString()+TEXT("|")+FString::FromInt(Ref.GetParentIndex(I));for(double N:{P.X,P.Y,P.Z,Q.X,Q.Y,Q.Z,Q.W,S.X,S.Y,S.Z})V+=TEXT("|")+FString::Printf(TEXT("%.17g"),N);Out.Add(FString::Printf(TEXT("bone.%d"),I),V);}
 }else {Props(O,TEXT("input"),Out);if(auto* Table=Cast<UMirrorDataTable>(O)){auto* RowStruct=Table->GetRowStruct();if(!Live(RowStruct)||Table->GetRowMap().Num()>4096)return false;Out.Add(TEXT("mirror.row_struct"),Path(RowStruct));for(const auto& Row:Table->GetRowMap()){if(!Row.Value)return false;const FString Key=TEXT("mirror.row.")+Row.Key.ToString();FString Text;RowStruct->ExportText(Text,Row.Value,nullptr,Table,PPF_None,nullptr);Out.Add(Key,Text);for(TFieldIterator<FProperty> P(RowStruct);P;++P)for(int I=0;I<P->ArrayDim;++I)if(!ExactValue(*P,P->ContainerPtrToValuePtr<void>(Row.Value,I),Key+TEXT(".")+P->GetName()+FString::FromInt(I),Out))return false;}}}
 int64 Bytes=0;for(const auto& V:Out)Bytes+=(V.Key.Len()+V.Value.Len())*2;return !Out.Contains(TEXT("$invalid_precision_snapshot"))&&Bytes<=4*1024*1024;
}
// Freeze authored assets, while holding immediate reference identities. Do not
// traverse class/CDO/import metadata as authored animation inputs.
bool CaptureInputs(UObject* Root,UObject* Skip,TMap<UObject*,Snap>& States,TArray<Lease>& Holds,FString& Error)
{
 TArray<UObject*> Pending{Root};TSet<UObject*> Seen;
 auto Hold=[&](UObject* O){if(Live(O)&&!Holds.ContainsByPredicate([&](const Lease& L){return L.Object.Get()==O;}))Holds.Emplace(O);};
 for(int I=0;I<Pending.Num();++I){auto* O=Pending[I];if(Seen.Contains(O))continue;if(!Live(O)||Seen.Num()>=4096){Error=TEXT("Input dependency lifetime/budget invalid");return false;}Seen.Add(O);Hold(O);if(O!=Skip){Snap Input;if(!CaptureInput(O,Input,Error))return false;if(const auto* Before=States.Find(O)){if(!Before->OrderIndependentCompareEqual(Input)){Error=TEXT("Existing input changed during dependency observation: ")+Path(O);return false;}}else States.Add(O,MoveTemp(Input));}
  TArray<UObject*> Refs;FReferenceFinder Finder(Refs,nullptr,false,true,false,true);Finder.FindReferences(O);if(Refs.Num()>4096)return false;
  for(auto* Ref:Refs){if(!Live(Ref))return false;Hold(Ref);if(Ref!=Skip&&!Seen.Contains(Ref)&&(Kind(Ref)||Ref->IsA<UAnimationAsset>()||Ref->IsA<USkeleton>()||Ref->IsA<UMirrorDataTable>()))Pending.AddUnique(Ref);}
 }return true;
}
bool ChooserCompiled(UChooserTable* C,TSharedRef<J> R)
{
 TArray<TSharedPtr<FJsonValue>> Errors;
 auto Check=[&](const FInstancedStruct& Value,const FString& Label){if(!Value.IsValid())return;
  if(Value.GetScriptStruct()->IsChildOf(FChooserColumnBase::StaticStruct())){FInstancedStruct Copy=Value;auto& Column=Copy.GetMutable<FChooserColumnBase>();bool CompileInput=!Column.bDisabled;if(auto* Random=Copy.GetMutablePtr<FRandomizeColumn>()){const auto* Input=Random->InputValue.GetPtr<FChooserParameterRandomizeBase>();CompileInput=Input&&Input->IsBound();}if(CompileInput)if(const auto* Parameter=Column.GetInputValue()){FText Message;if(Parameter->HasCompileErrors(Message))Errors.Add(MakeShared<FJsonValueString>(Label+TEXT(": ")+Message.ToString()));}if(auto* Output=Copy.GetMutablePtr<FOutputObjectColumn>())for(int I=0;I<Output->RowValues.Num();++I){auto& Row=Output->RowValues[I].Value;if(Row.IsValid()){FText Message;if(Row.GetMutable<FObjectChooserBase>().HasCompileErrors(Message))Errors.Add(MakeShared<FJsonValueString>(Label+FString::Printf(TEXT(".row[%d]: "),I)+Message.ToString()));}}}
  else if(Value.GetScriptStruct()->IsChildOf(FObjectChooserBase::StaticStruct())){FInstancedStruct Copy=Value;FText Message;if(Copy.GetMutable<FObjectChooserBase>().HasCompileErrors(Message))Errors.Add(MakeShared<FJsonValueString>(Label+TEXT(": ")+Message.ToString()));}
 };
 for(int I=0;I<C->ColumnsStructs.Num();++I)Check(C->ColumnsStructs[I],FString::Printf(TEXT("column[%d]"),I));for(int I=0;I<C->ResultsStructs.Num();++I)Check(C->ResultsStructs[I],FString::Printf(TEXT("row[%d]"),I));Check(C->FallbackResult,TEXT("fallback"));
 R->SetArrayField(TEXT("compile_diagnostics"),Errors);R->SetNumberField(TEXT("compile_errors"),Errors.Num());R->SetBoolField(TEXT("compiled"),Errors.IsEmpty());return Errors.IsEmpty();
}
struct Guard
{
 UObject* Target;TSharedRef<J> R;TArray<Lease> Holds;Snap Before,Expected;UEditorEngine* Editor;bool Ready=false;FString* ScriptError=nullptr;UPoseSearchSchema* RelatedSchema=nullptr;Snap RelatedBefore;TMap<UObject*,Snap> InputStates;
 Guard(UObject* O,TSharedRef<J> In):Target(O),R(In),Editor(GEditor){Ready=Capture(O,Before);Expected=Before;Hold(O);if(auto* DB=Cast<UPoseSearchDatabase>(O)){RelatedSchema=const_cast<UPoseSearchSchema*>(DB->Schema.Get());if(RelatedSchema){Hold(RelatedSchema);Ready=Ready&&Capture(RelatedSchema,RelatedBefore);}}FString InputError;Ready=Ready&&CaptureInputs(O,O,InputStates,Holds,InputError);if(!Ready)R->SetStringField(TEXT("input_observation_error"),InputError);}
 void Hold(UObject* O){if(!Live(O))return;if(!Holds.ContainsByPredicate([&](const Lease& H){return H.Object.Get()==O;}))Holds.Emplace(O);}
 void HoldRefs(){TArray<UObject*> Objects;FReferenceFinder Finder(Objects,nullptr,false,true,false,true);Finder.FindReferences(Target);if(Objects.Num()>4096){Ready=false;return;}for(auto* O:Objects)Hold(O);}
 bool Check(const Snap& Want){if((ScriptError&&!ScriptError->IsEmpty())||!Ready||!Idle()||GEditor!=Editor||!Holds.ContainsByPredicate([&](const Lease& L){return L.Object.Get()==Target;})){R->SetBoolField(TEXT("change_state_known"),false);return false;}for(const auto& H:Holds)if(!H.Current()){R->SetStringField(TEXT("first_mismatch_property"),TEXT("object_identity_owner_lifetime:" )+H.Name);R->SetBoolField(TEXT("change_state_known"),false);return false;}Snap Now;FString InputError;for(const auto& Input:InputStates)if(!CaptureInput(Input.Key,Now,InputError)||!Same(Input.Value,Now,R)){R->SetStringField(TEXT("first_mismatch_object"),Path(Input.Key));R->SetStringField(TEXT("input_observation_error"),InputError);R->SetBoolField(TEXT("change_state_known"),false);return false;}if(RelatedSchema&&(!Capture(RelatedSchema,Now)||!Same(RelatedBefore,Now,R))){R->SetBoolField(TEXT("change_state_known"),false);return false;}if(!Capture(Target,Now)||!Same(Want,Now,R)){R->SetBoolField(TEXT("change_state_known"),false);return false;}return true;}
 bool Finish(bool Save)
 {
  R->SetStringField(TEXT("stage"),TEXT("post_edit"));Target->PostEditChange();if(!Check(Expected))return false;
  if(auto* C=Cast<UChooserTable>(Target)){R->SetStringField(TEXT("stage"),TEXT("compile"));R->SetBoolField(TEXT("compile_attempted"),true);C->Compile(true);if(!Check(Expected))return false;if(!ChooserCompiled(C,R))return false;}
  FString DependencyError;if(!RuntimeAudit(Target,DependencyError,R)){R->SetStringField(TEXT("dependency_error"),DependencyError);R->SetBoolField(TEXT("change_state_known"),false);return false;}
  if(!Check(Expected))return false;R->SetStringField(TEXT("stage"),TEXT("mark_dirty"));Target->MarkPackageDirty();if(!Check(Expected))return false;
  if(Save){R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);auto* P=Target->GetOutermost();FSavePackageArgs A;A.TopLevelFlags=RF_Public|RF_Standalone;A.SaveFlags=SAVE_NoError;A.bSlowTask=false;const FString File=FPackageName::LongPackageNameToFilename(P->GetName(),FPackageName::GetAssetPackageExtension());const bool Saved=UPackage::SavePackage(P,Target,*File,A);R->SetBoolField(TEXT("saved"),Saved);if(!Check(Expected)||!Saved)return false;}
  R->SetBoolField(TEXT("dirty_after"),Target->GetOutermost()->IsDirty());return true;
 }
};
bool SaveAdmission(UObject* O,bool Save,bool Allow){if(!Save)return true;const FString F=FPackageName::LongPackageNameToFilename(O->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());return (!O->GetOutermost()->IsDirty()||Allow)&&!IFileManager::Get().IsReadOnly(*F);}
bool Number(TSharedPtr<J> P,const TCHAR* K,double& V,double Min,double Max){return P->TryGetNumberField(K,V)&&FMath::IsFinite(V)&&V>=Min&&V<=Max;}
bool ColumnBinding(UChooserTable* C,int32 Context,const FString& Property,bool Bool)
{
 if(!C->ContextData.IsValidIndex(Context)||C->ContextData[Context].GetScriptStruct()!=FContextObjectTypeClass::StaticStruct()||Property.IsEmpty()||Property.Len()>128)return false;
 auto* Class=C->ContextData[Context].Get<FContextObjectTypeClass>().Class.Get();if(!Live(Class))return false;auto* P=FindFProperty<FProperty>(Class,*Property);if(!P||P->HasAnyPropertyFlags(CPF_EditorOnly))return false;return Bool?CastField<FBoolProperty>(P)!=nullptr:(CastField<FFloatProperty>(P)||CastField<FDoubleProperty>(P)||CastField<FIntProperty>(P));
}
struct IndexInputWatch
{
 TArray<TWeakObjectPtr<UObject>> Roots;FDelegateHandle Modified,PropertyChanged;TAtomic<bool> Invalidated{false};FString FirstObject;
 explicit IndexInputWatch(const TMap<UObject*,Snap>& Inputs){for(const auto& Input:Inputs)Roots.Add(Input.Key);Modified=FCoreUObjectDelegates::OnObjectModified.AddRaw(this,&IndexInputWatch::Observe);PropertyChanged=FCoreUObjectDelegates::OnObjectPropertyChanged.AddLambda([this](UObject* O,FPropertyChangedEvent&){Observe(O);});}
 void Observe(UObject* O){if(!IsInGameThread()){Invalidated.Store(true);return;}if(!Live(O))return;for(const auto& Root:Roots)if(Root.IsValid()&&(O==Root.Get()||O->IsIn(Root.Get()))){if(!Invalidated.Load())FirstObject=Path(O);Invalidated.Store(true);return;}}
 ~IndexInputWatch(){FCoreUObjectDelegates::OnObjectModified.Remove(Modified);FCoreUObjectDelegates::OnObjectPropertyChanged.Remove(PropertyChanged);}
};
struct IndexJob{TWeakObjectPtr<UPoseSearchDatabase> Database;Snap DatabaseAuthored,SchemaAuthored;TStrongObjectPtr<UPoseSearchSchema> Schema;TMap<UObject*,Snap> Inputs;TArray<Lease> Holds;TSharedPtr<IndexInputWatch> Watch;FString Status;bool FreshIssued=false;};
TMap<FString,IndexJob> Jobs;
bool Active(UObject* O){for(auto& J:Jobs)if((J.Value.Status==TEXT("in_progress")||J.Value.Status.StartsWith(TEXT("inputs_changed")))&&(J.Value.Database.Get()==O||J.Value.Schema.Get()==O))return true;return false;}
}
bool AuditRuntimeInput(UObject* Object,FString& Error,TSharedPtr<FJsonObject> Observation){return RuntimeAudit(Object,Error,Observation);}
FString Inspect(UObject* O)
{
 if(Cast<UAnimBlueprint>(O))return InspectBindings(O);
 auto R=Result(TEXT("inspect_animation_production"));if(!Idle()||!Target(O))return Fail(R,TEXT("Idle Editor and exact top-level PoseSearch Schema/Database or Chooser asset required"));Snap S;if(!Capture(O,S))return Fail(R,TEXT("Authored snapshot budget or owned channel state invalid"));auto P=MakeShared<J>();for(const auto& V:S)P->SetStringField(V.Key,V.Value);R->SetObjectField(TEXT("authored_properties"),P);R->SetStringField(TEXT("asset"),Path(O));R->SetStringField(TEXT("class"),Path(O->GetClass()));
 if(auto* D=Cast<UPoseSearchDatabase>(O)){R->SetNumberField(TEXT("animation_count"),D->GetNumAnimationAssets());R->SetStringField(TEXT("schema"),Path(D->Schema));TArray<TSharedPtr<FJsonValue>> A;for(int I=0;I<D->GetNumAnimationAssets();++I)A.Add(MakeShared<FJsonValueString>(Path(D->GetAnimationAsset(I))));R->SetArrayField(TEXT("animations"),A);}
 if(auto* C=Cast<UChooserTable>(O)){R->SetNumberField(TEXT("row_count"),C->ResultsStructs.Num());R->SetNumberField(TEXT("column_count"),C->ColumnsStructs.Num());R->SetNumberField(TEXT("context_count"),C->ContextData.Num());}
 R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}
FString EvaluateChooser(UObject* Object,UObject* Context)
{
 auto R=Result(TEXT("evaluate_chooser_table"));auto* C=Cast<UChooserTable>(Object);if(!Idle()||Writing||!Target(C)||!Live(Context)||!RuntimeReference(Context)||C->ContextData.Num()!=1||C->ContextData[0].GetScriptStruct()!=FContextObjectTypeClass::StaticStruct()||!Context->IsA(C->ContextData[0].Get<FContextObjectTypeClass>().Class)||C->ResultType!=EObjectChooserResultType::ObjectResult||!C->OutputObjectType)return Fail(R,TEXT("Idle direct-property Chooser with one matching class context and ObjectResult required"));TGuardValue<bool> Lock(Writing,true);Guard G(C,R);G.HoldRefs();G.Hold(Context);Snap BeforeContext;Props(Context,TEXT("context"),BeforeContext);
 for(const auto& Col:C->ColumnsStructs)
 {
  const FChooserPropertyBinding* Binding=nullptr;bool Bool=false;
  if(Col.GetScriptStruct()==FBoolColumn::StaticStruct()){const auto& V=Col.Get<FBoolColumn>();if(V.InputValue.GetScriptStruct()!=FBoolContextProperty::StaticStruct()||V.RowValuesWithAny.Num()!=C->ResultsStructs.Num())return Fail(R,TEXT("Malformed bool column"));Binding=&V.InputValue.Get<FBoolContextProperty>().Binding;Bool=true;}
  else if(Col.GetScriptStruct()==FFloatRangeColumn::StaticStruct()){const auto& V=Col.Get<FFloatRangeColumn>();if(V.InputValue.GetScriptStruct()!=FFloatContextProperty::StaticStruct()||V.RowValues.Num()!=C->ResultsStructs.Num())return Fail(R,TEXT("Malformed float column"));Binding=&V.InputValue.Get<FFloatContextProperty>().Binding;}
  else return Fail(R,TEXT("Evaluation profile admits only direct Bool/FloatRange conditions; arbitrary function bindings not executed"));
  if(!Binding||Binding->ContextIndex!=0||Binding->IsBoundToRoot||Binding->PropertyBindingChain.Num()!=1||!ColumnBinding(C,0,Binding->PropertyBindingChain[0].ToString(),Bool))return Fail(R,TEXT("Direct single-property context binding required"));
 }
 for(const auto& Result:C->ResultsStructs)if(Result.GetScriptStruct()!=FAssetChooser::StaticStruct())return Fail(R,TEXT("Hard asset result rows required for deterministic editor evaluation profile"));if(C->FallbackResult.IsValid()&&C->FallbackResult.GetScriptStruct()!=FAssetChooser::StaticStruct())return Fail(R,TEXT("Hard asset or empty fallback required"));
 R->SetStringField(TEXT("stage"),TEXT("evaluate"));UObject* Chosen=UChooserFunctionLibrary::EvaluateChooser(Context,C,C->OutputObjectType);G.Hold(Chosen);Snap AfterContext;Props(Context,TEXT("context"),AfterContext);if(!G.Check(G.Before)||!Same(BeforeContext,AfterContext,R)){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Chooser/context changed during evaluation"));}if(Chosen&&(!Live(Chosen)||!Chosen->IsA(C->OutputObjectType)||!RuntimeReference(Chosen)))return Fail(R,TEXT("Returned object has unexpected type/lifetime/runtime scope"));R->SetStringField(TEXT("result_asset"),Path(Chosen));R->SetBoolField(TEXT("has_result"),Chosen!=nullptr);R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}
FString Create(const FString& K,const FString& P,UObject* Ref,bool Execute,bool Save)
{
 auto R=Result(TEXT("create_")+K);if(!Idle()||Writing||!P.StartsWith(TEXT("/Game/"))||P.Len()>900||!FPackageName::IsValidLongPackageName(P)||FPackageName::GetLongPackageAssetName(P).Equals(TEXT("None"),ESearchCase::IgnoreCase)||FPackageName::DoesPackageExist(P)||FindPackage(nullptr,*P))return Fail(R,TEXT("Idle Editor, fresh unused project package path required; overwrite is never implicit"));
 if((K==TEXT("pose_search_schema")&&!Cast<USkeleton>(Ref))||(K==TEXT("pose_search_database")&&!Cast<UPoseSearchSchema>(Ref))||(K==TEXT("chooser_table")&&!Cast<UClass>(Ref))||(K!=TEXT("pose_search_schema")&&K!=TEXT("pose_search_database")&&K!=TEXT("chooser_table"))||!RuntimeReference(Ref))return Fail(R,TEXT("Schema requires Skeleton, Database requires Schema, Chooser requires result UObject class"));
 Lease Source(Ref);TGuardValue<bool> Lock(Writing,true);TMap<UObject*,Snap> SourceStates;TArray<Lease> SourceHolds;FString InputError;if(!CaptureInputs(Ref,nullptr,SourceStates,SourceHolds,InputError))return Fail(R,TEXT("Creation source observation failed: ")+InputError);if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("validated"));return Encode(R);}
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("stage"),TEXT("create"));auto* Pkg=CreatePackage(*P);TStrongObjectPtr<UPackage> HoldPackage(Pkg);const FName Name(*FPackageName::GetLongPackageAssetName(P));UObject* O=nullptr;
 if(K==TEXT("pose_search_schema")){auto* S=NewObject<UPoseSearchSchema>(Pkg,Name,RF_Public|RF_Standalone|RF_Transactional);O=S;if(!Source.Current()||!Target(O))return Fail(R,TEXT("Created/source identity changed before skeleton assignment"));S->AddSkeleton(CastChecked<USkeleton>(Ref));}
 else if(K==TEXT("pose_search_database")){auto* D=NewObject<UPoseSearchDatabase>(Pkg,Name,RF_Public|RF_Standalone|RF_Transactional);O=D;if(!Source.Current()||!Target(O))return Fail(R,TEXT("Created/source identity changed before schema assignment"));D->Schema=CastChecked<UPoseSearchSchema>(Ref);}
 else {auto* C=NewObject<UChooserTable>(Pkg,Name,RF_Public|RF_Standalone|RF_Transactional);O=C;if(!Source.Current()||!Target(O))return Fail(R,TEXT("Created/source identity changed before output-class assignment"));C->OutputObjectType=CastChecked<UClass>(Ref);}
 R->SetStringField(TEXT("asset"),Path(O));R->SetBoolField(TEXT("changed"),true);if(!Source.Current()||!Target(O))return Fail(R,TEXT("Source/created object changed during construction; inspect new package"));Guard G(O,R);for(auto& H:SourceHolds)G.Holds.Add(MoveTemp(H));for(auto& Input:SourceStates)G.InputStates.Add(Input.Key,MoveTemp(Input.Value));G.HoldRefs();if(!G.Ready||!G.Check(G.Expected))return Fail(R,TEXT("Created snapshot unavailable"));R->SetBoolField(TEXT("change_state_known"),true);FAssetRegistryModule::AssetCreated(O);if(!G.Check(G.Expected)||!Source.Current()||!G.Finish(Save))return Fail(R,TEXT("Created asset callback/compile/save boundary failed; no rollback or retry"));R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}
FString Edit(UObject* O,const FString& A,const FString& Args,bool Execute,bool Save,bool AllowDirty,bool AllowRemove)
{
 auto R=Result(A);if(!Idle()||Writing||!Target(O)||Active(O))return Fail(R,TEXT("Idle exact asset required; owned active index jobs block conflicting edits"));R->SetStringField(TEXT("asset"),Path(O));if(!SaveAdmission(O,Save,AllowDirty))return Fail(R,TEXT("Saving preexisting dirty package requires approval and writable target file"));TSharedPtr<J> P;if(Args.Len()>65536||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Args),P)||!P)return Fail(R,TEXT("Bounded JSON object required"));
 auto* S=Cast<UPoseSearchSchema>(O);auto* D=Cast<UPoseSearchDatabase>(O);auto* C=Cast<UChooserTable>(O);double V=0,W=0;int32 I=INDEX_NONE;double IndexValue;if(P->TryGetNumberField(TEXT("index"),IndexValue)){if(!FMath::IsFinite(IndexValue)||IndexValue<0||IndexValue>4096||IndexValue!=FMath::FloorToDouble(IndexValue))return Fail(R,TEXT("Integer index0..4096 required"));I=int32(IndexValue);}FString Text;P->TryGetStringField(TEXT("value"),Text);UObject* Ref=nullptr;UClass* ChannelClass=nullptr;TFunction<void()> Apply;TFunction<bool()> Verify;
 TGuardValue<bool> Lock(Writing,true);Guard G(O,R);G.HoldRefs();if(!G.Ready||!G.Check(G.Expected))return Fail(R,TEXT("Authored snapshot/reference budget exceeded"));
 FString ScriptError;G.ScriptError=&ScriptError;FScopedScriptExceptionHandler Handler([&](ELogVerbosity::Type Severity,const TCHAR* Message,const TCHAR* Stack){if(Severity==ELogVerbosity::Error)ScriptError=FString(Message).Left(2048);FScriptExceptionHandler::LoggingExceptionHandler(Severity,Message,Stack);});
 auto LoadRef=[&](const FString& Value)->UObject*{R->SetBoolField(TEXT("input_lookup_attempted"),true);auto* Obj=Reference(Value,R);FString DependencyError;if(!G.Check(G.Before))return nullptr;if(!RuntimeReference(Obj)){R->SetStringField(TEXT("reference_admission_error"),TEXT("Editor-only/MCP/invalid runtime reference:" )+Path(Obj));return nullptr;}if(!CaptureInputs(Obj,O,G.InputStates,G.Holds,DependencyError)){R->SetStringField(TEXT("input_observation_error"),DependencyError);return nullptr;}if(!RuntimeAudit(Obj,DependencyError,R)){R->SetStringField(TEXT("dependency_error"),DependencyError);return nullptr;}if(!G.Check(G.Before))return nullptr;return Obj;};
 bool Applied=true;
 if(C){for(auto& Column:C->ColumnsStructs){if(!Column.IsValid()||!Column.GetScriptStruct()->IsChildOf(FChooserColumnBase::StaticStruct()))return Fail(R,TEXT("Existing invalid Chooser column"));auto& Base=Column.GetMutable<FChooserColumnBase>();const FName Rows=Base.RowValuesPropertyName();if(!Rows.IsNone()){auto* Arr=FindFProperty<FArrayProperty>(Column.GetScriptStruct(),Rows);if(!Arr||FScriptArrayHelper(Arr,Arr->ContainerPtrToValuePtr<void>(Column.GetMutableMemory())).Num()!=C->ResultsStructs.Num())return Fail(R,TEXT("Existing Chooser column/row count mismatch"));}}}

 if(A==TEXT("set_pose_search_sample_rate")&&S&&Number(P,TEXT("rate"),V,1,240)&&V==FMath::FloorToDouble(V)){Apply=[&]{S->SampleRate=int32(V);};Verify=[&]{return S->SampleRate==int32(V);};}
 else if(A==TEXT("add_pose_search_channel")&&S)
 {
  if(Text==TEXT("Position"))ChannelClass=UPoseSearchFeatureChannel_Position::StaticClass();else if(Text==TEXT("Velocity"))ChannelClass=UPoseSearchFeatureChannel_Velocity::StaticClass();else if(Text==TEXT("Heading"))ChannelClass=UPoseSearchFeatureChannel_Heading::StaticClass();else if(Text==TEXT("Trajectory"))ChannelClass=UPoseSearchFeatureChannel_Trajectory::StaticClass();if(!ChannelClass)return Fail(R,TEXT("Channel type Position/Velocity/Heading/Trajectory required"));
  auto* AP=FindFProperty<FArrayProperty>(S->GetClass(),TEXT("Channels"));FScriptArrayHelper Before(AP,AP->ContainerPtrToValuePtr<void>(S));if(Before.Num()>=128)return Fail(R,TEXT("128 authored channel limit"));
  const int32 OldCount=Before.Num();Apply=[&]{auto* New=NewObject<UPoseSearchFeatureChannel>(S,ChannelClass,NAME_None,RF_Transactional);G.Hold(New);if(!G.Check(G.Before)){Applied=false;return;}S->AddChannel(New);R->SetStringField(TEXT("channel"),Path(New));};Verify=[AP,S,OldCount]{FScriptArrayHelper Now(AP,AP->ContainerPtrToValuePtr<void>(S));return Now.Num()==OldCount+1;};
 }
 else if(A==TEXT("set_pose_search_channel_settings")&&S)
 {
  auto* AP=FindFProperty<FArrayProperty>(S->GetClass(),TEXT("Channels"));FScriptArrayHelper Array(AP,AP->ContainerPtrToValuePtr<void>(S));if(!Array.IsValidIndex(I))return Fail(R,TEXT("Existing authored channel index required"));auto* Channel=CastFieldChecked<FObjectPropertyBase>(AP->Inner)->GetObjectPropertyValue(Array.GetRawPtr(I));if(!Live(Channel))return Fail(R,TEXT("Live owned channel required"));G.Hold(Channel);
  const TSharedPtr<J>* Settings=nullptr;if(!P->TryGetObjectField(TEXT("settings"),Settings)||(*Settings)->Values.IsEmpty()||(*Settings)->Values.Num()>16)return Fail(R,TEXT("1..16 explicit channel settings required"));
  const TSet<FName> Allowed={TEXT("Weight"),TEXT("SampleTimeOffset"),TEXT("OriginTimeOffset"),TEXT("bDefaultWithRootBone"),TEXT("bNormalizeDisplacement"),TEXT("SampleRole"),TEXT("OriginRole")};
  TArray<TPair<FProperty*,TSharedPtr<FJsonValue>>> Fields;
  for(const auto& Field:(*Settings)->Values){auto* Property=FindFProperty<FProperty>(Channel->GetClass(),*Field.Key);if(!Property||!Allowed.Contains(Property->GetFName())||!Property->HasAnyPropertyFlags(CPF_Edit)||Property->HasAnyPropertyFlags(CPF_Transient))return Fail(R,TEXT("Only supported authored scalar channel settings accepted"));double N;if(auto* Numeric=CastField<FNumericProperty>(Property);Numeric&&Numeric->IsFloatingPoint()){if(!Field.Value->TryGetNumber(N)||!FMath::IsFinite(N)||FMath::Abs(N)>1e6)return Fail(R,TEXT("Finite bounded float channel setting required"));}else if(CastField<FBoolProperty>(Property)){bool B;if(!Field.Value->TryGetBool(B))return Fail(R,TEXT("Boolean setting required"));}else if(CastField<FNameProperty>(Property)){FString Role;if(!Field.Value->TryGetString(Role)||Role.Len()>128||!S->GetRoledSkeleton(FName(*Role)))return Fail(R,TEXT("Existing Schema role name required"));}else return Fail(R,TEXT("Unsupported channel setting type"));Fields.Emplace(Property,Field.Value);}
  Apply=[&,Channel,Fields]{Channel->Modify(false);if(!G.Check(G.Before)){Applied=false;return;}for(const auto& F:Fields){void* Address=F.Key->ContainerPtrToValuePtr<void>(Channel);if(auto* N=CastField<FNumericProperty>(F.Key))N->SetFloatingPointPropertyValue(Address,F.Value->AsNumber());else if(auto* B=CastField<FBoolProperty>(F.Key))B->SetPropertyValue(Address,F.Value->AsBool());else CastFieldChecked<FNameProperty>(F.Key)->SetPropertyValue(Address,FName(*F.Value->AsString()));}};
 }
 else if(A==TEXT("remove_pose_search_channel")&&S&&AllowRemove)
 {auto* AP=FindFProperty<FArrayProperty>(S->GetClass(),TEXT("Channels"));FScriptArrayHelper Array(AP,AP->ContainerPtrToValuePtr<void>(S));if(!Array.IsValidIndex(I))return Fail(R,TEXT("Existing channel index required"));Apply=[=]{FScriptArrayHelper Now(AP,AP->ContainerPtrToValuePtr<void>(S));Now.RemoveValues(I);};}
 else if(A==TEXT("add_pose_search_animation")&&D)
 {Ref=LoadRef(Text);auto* Anim=Cast<UAnimationAsset>(Ref);if(!Live(Anim)||!Live(const_cast<UPoseSearchSchema*>(D->Schema.Get()))||D->GetNumAnimationAssets()>=4096)return Fail(R,TEXT("Animation asset and live schema required;4096 entries max"));FPoseSearchDatabaseAnimationAsset Entry;Entry.AnimAsset=Anim;if(!Entry.IsSkeletonCompatible(D->Schema))return Fail(R,TEXT("Animation skeleton incompatible with Schema"));for(int K=0;K<D->GetNumAnimationAssets();++K)if(D->GetAnimationAsset(K)==Anim)return Fail(R,TEXT("Existing animation entry: choose explicit edit/removal; duplicate not implicit"));Apply=[=]{D->AddAnimationAsset(Entry);};}
 else if(A==TEXT("set_pose_search_animation_settings")&&D&&I>=0&&I<D->GetNumAnimationAssets())
 {
  bool Enabled;double Begin,End;if(!P->TryGetBoolField(TEXT("enabled"),Enabled)||!Number(P,TEXT("start"),Begin,0,86400)||!Number(P,TEXT("end"),End,0,86400)||Begin>End||(Text!=TEXT("UnmirroredOnly")&&Text!=TEXT("MirroredOnly")&&Text!=TEXT("UnmirroredAndMirrored")))return Fail(R,TEXT("Enabled flag, valid sampling interval and mirror option required"));auto* Existing=D->GetDatabaseAnimationAsset(I);if(!Existing)return Fail(R,TEXT("Entry unavailable"));FPoseSearchDatabaseAnimationAsset Entry=*Existing;Entry.bEnabled=Enabled;Entry.MirrorOption=Text==TEXT("UnmirroredOnly")?EPoseSearchMirrorOption::UnmirroredOnly:Text==TEXT("MirroredOnly")?EPoseSearchMirrorOption::MirroredOnly:EPoseSearchMirrorOption::UnmirroredAndMirrored;Entry.SamplingRange=FFloatInterval(Begin,End);if(!Entry.IsMirrorOptionCompatible(D->Schema)||(End>0&&End>Entry.GetPlayLength(FVector::ZeroVector)+KINDA_SMALL_NUMBER))return Fail(R,TEXT("Mirror table or sampling interval incompatible with entry"));Apply=[=]{D->SetAnimationAssetAt(Entry,I);};
 }
 else if(A==TEXT("set_pose_search_database_schema")&&D)
 {Ref=LoadRef(Text);auto* Schema=Cast<UPoseSearchSchema>(Ref);if(!Schema)return Fail(R,TEXT("Compatible live Schema required"));if(!AllowRemove)return Fail(R,TEXT("Replacing Database schema requires allow_replace=true"));for(int K=0;K<D->GetNumAnimationAssets();++K)if(!D->GetDatabaseAnimationAsset(K)->IsSkeletonCompatible(Schema))return Fail(R,TEXT("Existing entry incompatible with replacement Schema"));Apply=[=]{D->Schema=Schema;};}
 else if(A==TEXT("remove_pose_search_animation")&&D&&AllowRemove&&I>=0&&I<D->GetNumAnimationAssets()){Apply=[=]{D->RemoveAnimationAssetAt(I);};}
 else if(A==TEXT("set_pose_search_search_settings")&&D)
 {if(!Number(P,TEXT("neighbors"),V,1,600)||V!=FMath::FloorToDouble(V)||!Number(P,TEXT("principal_components"),W,1,64)||W!=FMath::FloorToDouble(W)||(Text!=TEXT("BruteForce")&&Text!=TEXT("PCAKDTree")&&Text!=TEXT("VPTree")))return Fail(R,TEXT("mode BruteForce/PCAKDTree/VPTree, neighbors1..600 and principal_components1..64 required"));Apply=[&]{D->PoseSearchMode=Text==TEXT("BruteForce")?EPoseSearchMode::BruteForce:Text==TEXT("PCAKDTree")?EPoseSearchMode::PCAKDTree:EPoseSearchMode::VPTree;D->KDTreeQueryNumNeighbors=int32(V);D->NumberOfPrincipalComponents=int32(W);};}
 else if(A==TEXT("set_chooser_context")&&C)
 {Ref=LoadRef(Text);auto* Class=Cast<UClass>(Ref);if(!Live(Class)||I<0||I>C->ContextData.Num()||C->ContextData.Num()>=64)return Fail(R,TEXT("Live explicit context class and existing/append index required"));if(I<C->ContextData.Num()&&!AllowRemove)return Fail(R,TEXT("Replacing existing context requires allow_replace=true"));Apply=[=]{FInstancedStruct X;X.InitializeAs<FContextObjectTypeClass>();X.GetMutable<FContextObjectTypeClass>().Class=Class;if(I==C->ContextData.Num())C->ContextData.Add(X);else C->ContextData[I]=X;};}
 else if(A==TEXT("add_chooser_column")&&C)
 {FString Property;double Context;if(!P->TryGetStringField(TEXT("property"),Property)||!Number(P,TEXT("context"),Context,0,63)||Context!=FMath::FloorToDouble(Context)||(Text!=TEXT("Bool")&&Text!=TEXT("FloatRange"))||C->ColumnsStructs.Num()>=128||!ColumnBinding(C,int32(Context),Property,Text==TEXT("Bool")))return Fail(R,TEXT("Bool/FloatRange column requires matching existing context property, context0..63, max128 columns"));Apply=[=]{FInstancedStruct Col;if(Text==TEXT("Bool")){Col.InitializeAs<FBoolColumn>();auto& T=Col.GetMutable<FBoolColumn>();T.InputValue.InitializeAs<FBoolContextProperty>();auto& B=T.InputValue.GetMutable<FBoolContextProperty>().Binding;B.ContextIndex=int32(Context);B.PropertyBindingChain={FName(*Property)};T.SetNumRows(C->ResultsStructs.Num());}else{Col.InitializeAs<FFloatRangeColumn>();auto& T=Col.GetMutable<FFloatRangeColumn>();T.InputValue.InitializeAs<FFloatContextProperty>();auto& B=T.InputValue.GetMutable<FFloatContextProperty>().Binding;B.ContextIndex=int32(Context);B.PropertyBindingChain={FName(*Property)};T.SetNumRows(C->ResultsStructs.Num());}C->ColumnsStructs.Add(MoveTemp(Col));};}
 else if((A==TEXT("add_chooser_row")||A==TEXT("set_chooser_result")||A==TEXT("set_chooser_fallback"))&&C)
 {Ref=LoadRef(Text);if(!Live(Ref)||!C->OutputObjectType||!Ref->IsA(C->OutputObjectType)||C->ResultType!=EObjectChooserResultType::ObjectResult||C->ResultsStructs.Num()>=4096)return Fail(R,TEXT("Result asset must match ObjectResult class;4096 rows max"));if(A==TEXT("set_chooser_result")&&(!C->ResultsStructs.IsValidIndex(I)||!AllowRemove))return Fail(R,TEXT("Existing row and allow_replace=true required"));if(A==TEXT("set_chooser_fallback")&&C->FallbackResult.IsValid()&&!AllowRemove)return Fail(R,TEXT("Existing fallback replacement requires allow_replace=true"));Apply=[=]{FInstancedStruct X;X.InitializeAs<FAssetChooser>();X.GetMutable<FAssetChooser>().Asset=Ref;if(A==TEXT("add_chooser_row")){C->ResultsStructs.Add(X);C->DisabledRows.SetNumZeroed(C->ResultsStructs.Num());for(auto& Col:C->ColumnsStructs)Col.GetMutable<FChooserColumnBase>().SetNumRows(C->ResultsStructs.Num());}else if(A==TEXT("set_chooser_result"))C->ResultsStructs[I]=X;else C->FallbackResult=X;};}
 else if(A==TEXT("set_chooser_cell")&&C)
 {double Row;if(!C->ColumnsStructs.IsValidIndex(I)||!Number(P,TEXT("row"),Row,0,4095)||Row!=FMath::FloorToDouble(Row)||!C->ResultsStructs.IsValidIndex(int32(Row)))return Fail(R,TEXT("Existing column/row required"));auto& Col=C->ColumnsStructs[I];if(Col.GetScriptStruct()==FBoolColumn::StaticStruct()&&(Text==TEXT("True")||Text==TEXT("False")||Text==TEXT("Any"))){Apply=[&,Row]{auto& B=C->ColumnsStructs[I].GetMutable<FBoolColumn>();B.RowValuesWithAny[int32(Row)]=Text==TEXT("True")?EBoolColumnCellValue::MatchTrue:Text==TEXT("False")?EBoolColumnCellValue::MatchFalse:EBoolColumnCellValue::MatchAny;};}else if(Col.GetScriptStruct()==FFloatRangeColumn::StaticStruct()&&Number(P,TEXT("min"),V,-1e12,1e12)&&Number(P,TEXT("max"),W,-1e12,1e12)&&V<=W){Apply=[&,Row]{auto& B=C->ColumnsStructs[I].GetMutable<FFloatRangeColumn>();auto& Cell=B.RowValues[int32(Row)];Cell.Min=V;Cell.Max=W;Cell.bNoMin=false;Cell.bNoMax=false;};}else return Fail(R,TEXT("Bool cell True/False/Any or finite FloatRange min<=max required"));}
 else if(A==TEXT("remove_chooser_row")&&C&&AllowRemove&&C->ResultsStructs.IsValidIndex(I)){Apply=[=]{for(auto& Col:C->ColumnsStructs){int Row=I;Col.GetMutable<FChooserColumnBase>().DeleteRows(MakeArrayView(&Row,1));}C->ResultsStructs.RemoveAt(I);if(C->DisabledRows.IsValidIndex(I))C->DisabledRows.RemoveAt(I);};}
 else if(A==TEXT("remove_chooser_column")&&C&&AllowRemove&&C->ColumnsStructs.IsValidIndex(I)){Apply=[=]{C->ColumnsStructs.RemoveAt(I);};}
 else return Fail(R,TEXT("Action/type/arguments invalid; destructive removals require explicit approval"));
 if(!SaveAdmission(O,Save,AllowDirty))return Fail(R,TEXT("Reference loading dirtied target; saving requires renewed explicit dirty approval"));
 G.Hold(Ref);if(!G.Ready)return Fail(R,TEXT("Authored snapshot/reference budget exceeded"));R->SetBoolField(TEXT("dirty_before"),O->GetOutermost()->IsDirty());if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("validated"));return Encode(R);}
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("modify"));O->Modify(false);if(!G.Check(G.Before)||!ScriptError.IsEmpty())return Fail(R,TEXT("Modify callback changed context, identity or authored values"));
 R->SetStringField(TEXT("stage"),TEXT("apply"));Apply();if(!Applied||!ScriptError.IsEmpty()){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,ScriptError);}if(Verify&&!Verify()){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Engine did not apply expected collection change"));}if(!Capture(O,G.Expected)){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Post-apply snapshot failed"));}G.HoldRefs();R->SetBoolField(TEXT("changed"),!Same(G.Before,G.Expected,MakeShared<J>()));if(!G.Finish(Save)||!ScriptError.IsEmpty())return Fail(R,TEXT("Post-edit/compile/save boundary failed; inspect actual target and save flags"));R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}
FString Index(UObject* O,const FString& Id,bool Start)
{
 auto R=Result(Start?TEXT("request_pose_search_index"):TEXT("get_pose_search_index_status"));auto* D=Cast<UPoseSearchDatabase>(O);if(!Idle()||Writing||!Target(O)||!D)return Fail(R,TEXT("Idle exact PoseSearch Database required"));TGuardValue<bool> Lock(Writing,true);Lease Identity(D);FString Token=Id;IndexJob* Job=nullptr;
 if(Start){if(Active(D)||!Live(const_cast<UPoseSearchSchema*>(D->Schema.Get()))||D->GetNumAnimationAssets()==0||Jobs.Num()>=64)return Fail(R,TEXT("Nonempty Database/live Schema required, no duplicate active request;max64 recorded jobs"));Token=FGuid::NewGuid().ToString(EGuidFormats::Digits);IndexJob New;New.Database=D;New.Schema.Reset(const_cast<UPoseSearchSchema*>(D->Schema.Get()));if(!Capture(D,New.DatabaseAuthored)||!Capture(New.Schema.Get(),New.SchemaAuthored))return Fail(R,TEXT("Index input snapshot failed"));FString Error;if(!CaptureInputs(D,nullptr,New.Inputs,New.Holds,Error))return Fail(R,TEXT("Index dependency observation failed: ")+Error);New.Watch=MakeShared<IndexInputWatch>(New.Inputs);New.Status=TEXT("in_progress");Job=&Jobs.Add(Token,MoveTemp(New));}
 else {Job=Jobs.Find(Token);if(!Job||Job->Database.Get()!=D)return Fail(R,TEXT("Exact owned job id and same live Database required;status does not start unowned jobs"));}
 R->SetStringField(TEXT("job_id"),Token);Snap Now;
 auto InputsCurrent=[&]{if(Job->Watch&&Job->Watch->Invalidated.Load()){R->SetStringField(TEXT("first_mismatch_object"),Job->Watch->FirstObject);R->SetStringField(TEXT("first_mismatch_property"),TEXT("index_input_modified_generation"));return false;}for(const auto& H:Job->Holds)if(!H.Current()){R->SetStringField(TEXT("first_mismatch_object"),H.Name);R->SetStringField(TEXT("first_mismatch_property"),TEXT("index_input_identity_owner_lifetime"));return false;}FString Error;Snap Current;for(const auto& Input:Job->Inputs)if(!CaptureInput(Input.Key,Current,Error)||!Same(Input.Value,Current,R)){R->SetStringField(TEXT("first_mismatch_object"),Path(Input.Key));R->SetStringField(TEXT("input_observation_error"),Error);return false;}return true;};
if(!InputsCurrent()||!Live(Job->Schema.Get())||D->Schema!=Job->Schema.Get()||!Capture(D,Now)||!Same(Job->DatabaseAuthored,Now,R)||!Capture(Job->Schema.Get(),Now)||!Same(Job->SchemaAuthored,Now,R)){Job->Status=TEXT("inputs_changed");R->SetStringField(TEXT("status"),Job->Status);R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Index inputs changed; engine work may continue; no automatic restart or cancellation"));}
 if(Job->Status==TEXT("in_progress")){using namespace UE::PoseSearch;R->SetStringField(TEXT("stage"),TEXT("index_request"));R->SetBoolField(TEXT("index_engine_call_attempted"),true);R->SetBoolField(TEXT("compile_attempted"),true);R->SetBoolField(TEXT("mutation_attempted"),true);R->SetBoolField(TEXT("change_state_known"),false);auto State=FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(D,ERequestAsyncBuildFlag::ContinueRequest);
 // NewRequest cancels a Prestarted task in this engine. Observe/finish any
 // existing task first; a terminal probe alone is not our fresh-build result.
 if(State!=EAsyncBuildIndexResult::InProgress&&!Job->FreshIssued){
  if(!InputsCurrent()||!Identity.Current()||!Idle()||D->Schema!=Job->Schema.Get()||!Capture(D,Now)||!Same(Job->DatabaseAuthored,Now,R)||!Capture(Job->Schema.Get(),Now)||!Same(Job->SchemaAuthored,Now,R)){Job->Status=TEXT("inputs_changed_after_engine_call");return Fail(R,TEXT("Index probe callback changed inputs; no fresh request issued"));}
  Job->FreshIssued=true;State=FAsyncPoseSearchDatabasesManagement::RequestAsyncBuildIndex(D,ERequestAsyncBuildFlag::NewRequest);
 }R->SetBoolField(TEXT("fresh_request_issued"),Job->FreshIssued);R->SetBoolField(TEXT("mutation_attempted"),true);if(!InputsCurrent()||!Identity.Current()||!Idle()||D->Schema!=Job->Schema.Get()||!Capture(D,Now)||!Same(Job->DatabaseAuthored,Now,R)||!Capture(Job->Schema.Get(),Now)||!Same(Job->SchemaAuthored,Now,R)){Job->Status=TEXT("inputs_changed_after_engine_call");R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("status"),Job->Status);return Fail(R,TEXT("Index callback changed identity/input; engine task may continue; no retry"));}Job->Status=State==EAsyncBuildIndexResult::InProgress?TEXT("in_progress"):State==EAsyncBuildIndexResult::Success?TEXT("complete"):TEXT("failed");if(State==EAsyncBuildIndexResult::Success){const int Poses=D->GetSearchIndex().GetNumPoses();R->SetNumberField(TEXT("pose_count"),Poses);if(Poses<=0)Job->Status=TEXT("empty_index");}}
 if(Job->Status!=TEXT("in_progress"))Job->Watch.Reset();R->SetBoolField(TEXT("fresh_request_issued"),Job->FreshIssued);R->SetStringField(TEXT("change_scope"),TEXT("authored inputs; derived-data state is reported separately by status"));R->SetBoolField(TEXT("change_state_known"),true);R->SetBoolField(TEXT("changed"),false);R->SetField(TEXT("derived_data_changed"),MakeShared<FJsonValueNull>());R->SetStringField(TEXT("status"),Job->Status);R->SetBoolField(TEXT("completed"),Job->Status==TEXT("complete"));R->SetBoolField(TEXT("compiled"),Job->Status==TEXT("complete"));R->SetBoolField(TEXT("authored_changed"),false);if(!R->GetBoolField(TEXT("change_state_known")))R->SetField(TEXT("changed"),MakeShared<FJsonValueNull>());R->SetBoolField(TEXT("success"),Job->Status==TEXT("complete")||Job->Status==TEXT("in_progress"));R->SetStringField(TEXT("stage"),TEXT("index_status"));R->SetStringField(TEXT("scope"),TEXT("Engine async derived-data request; observes existing work to a terminal boundary before requesting a fresh key. No game-thread wait, asset save, task cancellation or automatic retry."));return Encode(R);
}
namespace
{
bool GraphState(UAnimBlueprint* B,UEdGraphNode* Binding,Snap& Out,bool MaskBinding,bool MaskUpdate=false,UEdGraph* Ignored=nullptr,UEdGraph* Included=nullptr)
{
 Out.Reset();if(!Live(B)||!Live(B->TargetSkeleton))return false;Out.Add(TEXT("skeleton"),Path(B->TargetSkeleton));TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);if(Included)Graphs.AddUnique(Included);if(Graphs.Num()>256)return false;int Nodes=0,Pins=0;
 for(auto* G:Graphs){if(G==Ignored)continue;if(!Live(G)||!G->IsIn(B)||G->GetOutermost()!=B->GetOutermost())return false;const FString GK=G->GraphGuid.ToString();Out.Add(GK+TEXT(".graph"),Path(G)+TEXT("|")+Path(G->GetClass())+TEXT("|")+Path(G->GetSchema()));
  for(UEdGraphNode* N:G->Nodes){if(!Live(N)||++Nodes>4096||!N->NodeGuid.IsValid())return false;const FString K=GK+TEXT(".")+N->NodeGuid.ToString();Out.Add(K+TEXT(".identity"),Path(N)+TEXT("|")+Path(N->GetClass())+TEXT("|")+Path(N->GetOuter()));Snap Values;Props(N,K,Values);if(Values.Contains(TEXT("$invalid_precision_snapshot"))||!AnimationStateEditing::CaptureNodeBindings(N,K+TEXT(".binding."),Values))return false;
   for(const TCHAR* Name:{TEXT("SaveCachedPoseNode"),TEXT("NameOfCache"),TEXT("InterfaceGuid"),TEXT("FunctionReference")})if(auto* P=FindFProperty<FProperty>(N->GetClass(),Name)){FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(N),nullptr,N,PPF_None);Values.Add(K+TEXT(".")+Name,V);}
   if(MaskUpdate&&N==Binding)for(auto It=Values.CreateIterator();It;++It)if(It.Key()==K+TEXT(".UpdateFunction")||It.Key().StartsWith(K+TEXT(".UpdateFunction.")))It.RemoveCurrent();
   if(MaskBinding&&N==Binding){for(auto It=Values.CreateIterator();It;++It)if(It.Key().StartsWith(K+TEXT(".SaveCachedPoseNode"))||It.Key().StartsWith(K+TEXT(".NameOfCache"))||It.Key().StartsWith(K+TEXT(".InterfaceGuid"))||It.Key().StartsWith(K+TEXT(".FunctionReference"))||((Cast<UAnimGraphNode_LinkedAnimLayer>(N)||N->GetClass()->GetName()==TEXT("AnimGraphNode_MotionMatching"))&&(It.Key()==K+TEXT(".Node")||It.Key().StartsWith(K+TEXT(".Node.")))))It.RemoveCurrent();
    auto* Runtime=FindFProperty<FStructProperty>(N->GetClass(),TEXT("Node"));if(Runtime&&(Cast<UAnimGraphNode_LinkedAnimLayer>(N)||N->GetClass()->GetName()==TEXT("AnimGraphNode_MotionMatching"))){void* Memory=Runtime->ContainerPtrToValuePtr<void>(N);for(TFieldIterator<FProperty> P(Runtime->Struct);P;++P){if(P->HasAnyPropertyFlags(CPF_Transient)||(Cast<UAnimGraphNode_LinkedAnimLayer>(N)&&(P->GetFName()==TEXT("Layer")||P->GetFName()==TEXT("Interface")||P->GetFName()==TEXT("InstanceClass")))||(N->GetClass()->GetName()==TEXT("AnimGraphNode_MotionMatching")&&P->GetFName()==TEXT("Database")))continue;FString V;P->ExportTextItem_Direct(V,P->ContainerPtrToValuePtr<void>(Memory),nullptr,N,PPF_None);Values.Add(K+TEXT(".otherRuntime.")+P->GetName(),V);if(!ExactValue(*P,P->ContainerPtrToValuePtr<void>(Memory),K+TEXT(".otherRuntime.")+P->GetName(),Values))return false;}}
   }
   for(const auto& V:Values)Out.Add(V.Key,V.Value);
   for(auto* P:N->Pins){if(!P||++Pins>16384)return false;const FString PK=K+TEXT(".pin.")+P->PinName.ToString()+TEXT(".")+FString::FromInt(P->Direction);FString Type;FEdGraphPinType::StaticStruct()->ExportText(Type,&P->PinType,nullptr,nullptr,PPF_None,nullptr);Out.Add(PK+TEXT(".type"),Type);FString Localized;FTextStringHelper::WriteToBuffer(Localized,P->DefaultTextValue);Out.Add(PK+TEXT(".default"),P->DefaultValue+TEXT("|")+Path(P->DefaultObject)+TEXT("|")+Localized);TArray<FString> Links;for(auto* Link:P->LinkedTo){if(!Link||!Live(Link->GetOwningNode()))return false;Links.Add(Link->GetOwningNode()->NodeGuid.ToString()+TEXT(".")+Link->PinName.ToString()+TEXT(".")+FString::FromInt(Link->Direction));}Links.Sort();Out.Add(PK+TEXT(".links"),FString::Join(Links,TEXT("|")));}
  }
 }
 int64 Bytes=0;for(const auto& V:Out)Bytes+=(V.Key.Len()+V.Value.Len())*2;return !Out.Contains(TEXT("$invalid_precision_snapshot"))&&Bytes<=8*1024*1024;
}
UEdGraph* TopGraph(UEdGraph* G){for(int I=0;G&&I<64;++I){auto* Next=G->GetTypedOuter<UEdGraph>();if(!Next)return G;G=Next;}return nullptr;}
}
FString BindNode(UObject* Object,const FString& Action,const FString& GraphName,const FString& Id,const FString& Source,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowReplace)
{
 auto R=Result(Action);auto* B=Cast<UAnimBlueprint>(Object);if(!Idle()||Writing||!Live(B)||!Live(B->TargetSkeleton)||B->IsTemplate()||B->GetOuter()!=B->GetOutermost()||B->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap)||!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||!SaveAdmission(B,Save,AllowDirty))return Fail(R,TEXT("Idle project AnimBlueprint with live Skeleton and explicit dirty save approval required"));TGuardValue<bool> Lock(Writing,true);Lease Blueprint(B),Skeleton(B->TargetSkeleton);TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);UEdGraph* G=nullptr;for(auto* Item:Graphs)if(Item&&Item->GetName()==GraphName){if(G)return Fail(R,TEXT("Ambiguous graph name"));G=Item;}FGuid Guid;if(!G||!FGuid::Parse(Id,Guid))return Fail(R,TEXT("Exact graph name and node GUID required"));UEdGraphNode* N=nullptr;for(UEdGraphNode* Item:G->Nodes)if(Item&&Item->NodeGuid==Guid)N=Item;if(!Live(N))return Fail(R,TEXT("Node not found"));Lease Node(N);Snap Before,Protected,Now;if(!GraphState(B,N,Before,false)||!GraphState(B,N,Protected,true))return Fail(R,TEXT("Graph snapshot budget exceeded or graph invalid"));Snap SkeletonState;Props(B->TargetSkeleton,TEXT("skeleton"),SkeletonState);if(SkeletonState.Contains(TEXT("$invalid_precision_snapshot")))return Fail(R,TEXT("Skeleton snapshot precision/budget invalid"));
 if(Compile)for(auto* Graph:Graphs)for(UEdGraphNode* Other:Graph->Nodes)if(auto* Slot=Cast<UAnimGraphNode_Slot>(Other);Slot&&!B->TargetSkeleton->ContainsSlotName(Slot->Node.SlotName))return Fail(R,TEXT("Compilation would register an unapproved new Skeleton slot; register that slot explicitly first or compile=false"));
 TArray<Lease> GraphHolds;for(auto* Graph:Graphs){GraphHolds.Emplace(Graph);for(UEdGraphNode* Other:Graph->Nodes)if(Live(Other))GraphHolds.Emplace(Other);}
 FString ScriptError;FScopedScriptExceptionHandler Handler([&](ELogVerbosity::Type V,const TCHAR* M,const TCHAR* Stack){if(V==ELogVerbosity::Error)ScriptError=FString(M).Left(2048);FScriptExceptionHandler::LoggingExceptionHandler(V,M,Stack);});
 TArray<Lease> Sources;TMap<UObject*,Snap> SourceStates;
 auto Check=[&](const Snap& Wanted,bool Mask){if(!Idle()||!ScriptError.IsEmpty()||!Blueprint.Current()||!Skeleton.Current()||!Node.Current()){R->SetBoolField(TEXT("change_state_known"),false);return false;}Snap CurrentSkeleton;Props(B->TargetSkeleton,TEXT("skeleton"),CurrentSkeleton);if(!Same(SkeletonState,CurrentSkeleton,R)){R->SetBoolField(TEXT("change_state_known"),false);return false;}for(const auto& Source:Sources)if(!Source.Current()){R->SetBoolField(TEXT("change_state_known"),false);return false;}for(const auto& Input:SourceStates){Snap Current;FString InputError;if(!CaptureInput(Input.Key,Current,InputError)||!Same(Input.Value,Current,R)){R->SetStringField(TEXT("first_mismatch_object"),Path(Input.Key));R->SetStringField(TEXT("input_observation_error"),InputError);R->SetBoolField(TEXT("change_state_known"),false);return false;}}for(const auto& H:GraphHolds)if(!H.Current()){R->SetBoolField(TEXT("change_state_known"),false);return false;}if(!GraphState(B,N,Now,Mask)||!Same(Wanted,Now,R)){R->SetBoolField(TEXT("change_state_known"),false);return false;}return true;};
 TFunction<void()> Apply;TFunction<bool()> Bound;
 if(Action==TEXT("bind_cached_pose"))
 {
  auto* Use=Cast<UAnimGraphNode_UseCachedPose>(N);UAnimGraphNode_SaveCachedPose* Cached=nullptr;FGuid SourceGuid;if(!Use||!FGuid::Parse(Source,SourceGuid))return Fail(R,TEXT("UseCachedPose node and exact SaveCachedPose GUID required"));for(auto* Graph:Graphs)for(UEdGraphNode* Item:Graph->Nodes)if(auto* SaveNode=Cast<UAnimGraphNode_SaveCachedPose>(Item);SaveNode&&SaveNode->NodeGuid==SourceGuid)Cached=SaveNode;
  auto* Name=FindFProperty<FStrProperty>(Use->GetClass(),TEXT("NameOfCache"));if(!Cached||!Name||TopGraph(Cached->GetGraph())!=TopGraph(G)||Cached->CacheName.IsEmpty())return Fail(R,TEXT("Named SaveCachedPose in the same AnimGraph/layer scope required"));if(Use->SaveCachedPoseNode.IsValid()&&Use->SaveCachedPoseNode.Get()!=Cached&&!AllowReplace)return Fail(R,TEXT("Existing cache replacement requires allow_replace=true"));Sources.Emplace(Cached);const FString CacheName=Cached->CacheName;Apply=[=]{Use->SaveCachedPoseNode=Cached;Name->SetPropertyValue_InContainer(Use,CacheName);};Bound=[=]{return Use->SaveCachedPoseNode.Get()==Cached&&Name->GetPropertyValue_InContainer(Use)==CacheName&&Cached->CacheName==CacheName;};
 }
 else if(Action==TEXT("bind_linked_anim_layer"))
 {
  auto* L=Cast<UAnimGraphNode_LinkedAnimLayer>(N);if(!L||Source.IsEmpty()||Source.Len()>128||Source.Equals(TEXT("None"),ESearchCase::IgnoreCase))return Fail(R,TEXT("LinkedAnimLayer node and existing layer name required"));const FName Layer(*Source);auto* AnimClass=IAnimClassInterface::GetFromClass(B->SkeletonGeneratedClass);bool AnimationLayer=false;if(AnimClass)for(const auto& Function:AnimClass->GetAnimBlueprintFunctions())if(Function.Name==Layer&&Function.Name!=TEXT("AnimGraph"))AnimationLayer=true;if(!AnimationLayer)return Fail(R,TEXT("Compiled animation layer required; ordinary K2 functions are not layer targets"));if(L->Node.Layer!=Layer&&!L->Node.Layer.IsNone()&&!AllowReplace)return Fail(R,TEXT("Existing layer replacement requires allow_replace=true"));if(G->GetFName()==Layer)return Fail(R,TEXT("Recursive self layer binding refused"));
  UClass* Interface=nullptr;FGuid InterfaceGuid;bool Found=false;for(const auto& I:B->ImplementedInterfaces)for(UEdGraph* IG:I.Graphs)if(IG&&IG->GetFName()==Layer){if(Found)return Fail(R,TEXT("Ambiguous implemented layer"));Found=true;Interface=I.Interface.Get();InterfaceGuid=IG->InterfaceGuid;}
  if(!Found)for(UEdGraph* FG:B->FunctionGraphs)if(FG&&FG->GetFName()==Layer){Found=true;break;}if(!Found)return Fail(R,TEXT("Existing implemented interface layer or self layer graph required"));if(Interface&&L->Node.InstanceClass&&!L->Node.InstanceClass->ImplementsInterface(Interface))return Fail(R,TEXT("Existing linked instance does not implement the requested interface"));if(Interface&&(!Interface->IsChildOf(UAnimLayerInterface::StaticClass())||!RuntimeReference(Interface)))return Fail(R,TEXT("Runtime AnimLayer interface required"));auto* Member=FindFProperty<FStructProperty>(L->GetClass(),TEXT("FunctionReference"));if(!Member)return Fail(R,TEXT("Engine FunctionReference property unavailable"));FMemberReference Reference;if(Interface){FGuid FunctionGuid;FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(Interface,Layer,FunctionGuid);Reference.SetExternalMember(Layer,Interface,FunctionGuid);Sources.Emplace(Interface);}else {FGuid FunctionGuid;FBlueprintEditorUtils::GetFunctionGuidFromClassByFieldName(B->SkeletonGeneratedClass,Layer,FunctionGuid);Reference.SetSelfMember(Layer,FunctionGuid);}
  auto* OldClass=L->Node.Interface?L->Node.Interface.Get():B->SkeletonGeneratedClass.Get();auto* NewClass=Interface?Interface:B->SkeletonGeneratedClass.Get();auto* OldFunction=OldClass?OldClass->FindFunctionByName(L->Node.Layer):nullptr;auto* NewFunction=NewClass?NewClass->FindFunctionByName(Layer):nullptr;
  auto Parameters=[](UFunction* F){TArray<FProperty*> P;if(F)for(TFieldIterator<FProperty> It(F);It;++It)if(It->HasAnyPropertyFlags(CPF_Parm))P.Add(*It);return P;};auto OldParams=Parameters(OldFunction),NewParams=Parameters(NewFunction);bool SameSignature=OldFunction&&NewFunction&&OldParams.Num()==NewParams.Num();for(int I=0;SameSignature&&I<OldParams.Num();++I){auto* Old=OldParams[I];auto* New=NewParams[I];auto* OP=CastField<FStructProperty>(Old);auto* NP=CastField<FStructProperty>(New);const bool GeneratedPoseResult=OP&&NP&&OP->Struct==FPoseLink::StaticStruct()&&NP->Struct==FPoseLink::StaticStruct()&&Old->HasAnyPropertyFlags(CPF_OutParm)&&!Old->HasAnyPropertyFlags(CPF_ReferenceParm)&&New->HasAnyPropertyFlags(CPF_OutParm)&&!New->HasAnyPropertyFlags(CPF_ReferenceParm)&&Old->GetFName()==L->Node.Layer&&New->GetFName()==Layer;SameSignature=(Old->GetFName()==New->GetFName()||GeneratedPoseResult)&&Old->SameType(New)&&((Old->GetPropertyFlags()^New->GetPropertyFlags())&(CPF_OutParm|CPF_ReferenceParm|CPF_ConstParm|CPF_ReturnParm))==0;}
  // AnimBlueprintCompiler::CreateAnimGraphStubFunctions names its generated
  // pose output after each graph. That output label is not an authored input.
  R->SetStringField(TEXT("old_layer_function"),Path(OldFunction));R->SetStringField(TEXT("new_layer_function"),Path(NewFunction));if(!SameSignature)return Fail(R,TEXT("Existing/new compiled layer signatures must match; changed ports require explicit graph authoring rather than an implicit destructive rebind"));
  UClass* PlannedInstance=Interface?L->Node.InstanceClass.Get():nullptr;if(PlannedInstance){if(!RuntimeReference(PlannedInstance))return Fail(R,TEXT("Runtime linked instance class required"));Sources.Emplace(PlannedInstance);}Apply=[=]{L->Node.Layer=Layer;L->Node.Interface=Interface;L->InterfaceGuid=InterfaceGuid;L->Node.InstanceClass=PlannedInstance;*Member->ContainerPtrToValuePtr<FMemberReference>(L)=Reference;};Bound=[=]{const auto& M=*Member->ContainerPtrToValuePtr<FMemberReference>(L);return L->Node.Layer==Layer&&L->Node.Interface.Get()==Interface&&L->InterfaceGuid==InterfaceGuid&&L->Node.InstanceClass.Get()==PlannedInstance&&FMemberReference::StaticStruct()->CompareScriptStruct(&Reference,&M,PPF_None);};
 }
 else if(Action==TEXT("bind_motion_matching_database"))
 {
  if(N->GetClass()->GetName()!=TEXT("AnimGraphNode_MotionMatching"))return Fail(R,TEXT("Exact MotionMatching node required"));R->SetBoolField(TEXT("input_lookup_attempted"),true);auto* Database=Cast<UPoseSearchDatabase>(Reference(Source,R));if(!Check(Before,false)||!Target(Database)||!RuntimeReference(Database)||!Live(const_cast<UPoseSearchSchema*>(Database->Schema.Get()))||Database->Schema->GetSkeleton(UE::PoseSearch::DefaultRole)!=B->TargetSkeleton)return Fail(R,TEXT("Database requires matching default-role Skeleton"));Sources.Emplace(Database);Sources.Emplace(const_cast<UPoseSearchSchema*>(Database->Schema.Get()));Snap DS,SS;if(!Capture(Database,DS)||!Capture(const_cast<UPoseSearchSchema*>(Database->Schema.Get()),SS))return Fail(R,TEXT("Database input snapshot failed"));SourceStates.Add(Database,DS);SourceStates.Add(const_cast<UPoseSearchSchema*>(Database->Schema.Get()),SS);FString InputError;if(!CaptureInputs(Database,nullptr,SourceStates,Sources,InputError))return Fail(R,TEXT("Database authored input observation failed: ")+InputError);auto* Runtime=FindFProperty<FStructProperty>(N->GetClass(),TEXT("Node"));auto* Property=Runtime?FindFProperty<FObjectPropertyBase>(Runtime->Struct,TEXT("Database")):nullptr;if(!Property)return Fail(R,TEXT("Installed MotionMatching Database property unavailable"));void* Memory=Runtime->ContainerPtrToValuePtr<void>(N);auto* Existing=Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Memory));if(Existing&&Existing!=Database&&!AllowReplace)return Fail(R,TEXT("Replacing Database requires allow_replace=true"));Apply=[=]{Property->SetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Memory),Database);};Bound=[=]{return Property->GetObjectPropertyValue(Property->ContainerPtrToValuePtr<void>(Runtime->ContainerPtrToValuePtr<void>(N)))==Database;};
 }
 else return Fail(R,TEXT("Fixed CachedPose/LinkedLayer/MotionMatching binding operation required"));
 if(!Check(Before,false)||!SaveAdmission(B,Save,AllowDirty))return Fail(R,TEXT("Reference resolution changed target or dirty save admission"));R->SetStringField(TEXT("asset"),Path(B));R->SetStringField(TEXT("node_id"),Guid.ToString());if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("validated"));return Encode(R);}R->SetBoolField(TEXT("mutation_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("modify"));B->Modify(false);if(!Check(Before,false))return Fail(R,TEXT("Blueprint Modify callback changed authored graph"));N->Modify(false);if(!Check(Before,false))return Fail(R,TEXT("Node Modify callback changed authored graph"));for(const auto& S:Sources)if(!S.Current()){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Binding source identity changed"));}
 R->SetStringField(TEXT("stage"),TEXT("bind"));Apply();if(Cast<UAnimGraphNode_LinkedAnimLayer>(N))N->ReconstructNode();if(!Check(Protected,true)||!Bound()){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Binding or unrelated graph preservation failed"));}Snap Expected;if(!GraphState(B,N,Expected,false)){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Post-bind snapshot failed"));}R->SetBoolField(TEXT("changed"),!Same(Before,Expected,MakeShared<J>()));
 N->PostEditChange();if(!Check(Expected,false)||!Bound())return Fail(R,TEXT("PostEdit callback changed binding or graph"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);if(!Check(Expected,false)||!Bound())return Fail(R,TEXT("Structural mark callback changed binding or graph"));
 if(Compile){R->SetStringField(TEXT("stage"),TEXT("compile"));R->SetBoolField(TEXT("compile_attempted"),true);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Log);R->SetNumberField(TEXT("compile_errors"),Log.NumErrors);R->SetBoolField(TEXT("compiled"),Log.NumErrors==0);if(!Check(Expected,false)||!Bound()||Log.NumErrors)return Fail(R,TEXT("Compile/preservation failed; source changes remain, no save"));}
 if(Save){R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const FString File=FPackageName::LongPackageNameToFilename(B->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());R->SetBoolField(TEXT("saved"),UPackage::SavePackage(B->GetOutermost(),B,*File,Args));if(!Check(Expected,false)||!Bound()||!R->GetBoolField(TEXT("saved")))return Fail(R,TEXT("Save/post-save boundary failed; actual saved flag retained"));}
 R->SetBoolField(TEXT("dirty_after"),B->GetOutermost()->IsDirty());R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}


FString BindChooser(UObject* Object,const FString& GraphName,const FString& Id,const FString& ChooserPath,const FString& FunctionName,bool Execute,bool Compile,bool Save,bool AllowDirty,bool AllowReplace)
{
 auto R=Result(TEXT("bind_motion_matching_chooser"));auto* B=Cast<UAnimBlueprint>(Object);
 if(!Idle()||Writing||!Live(B)||!Live(B->TargetSkeleton)||!Live(B->GeneratedClass)||!Live(B->SkeletonGeneratedClass)||B->IsTemplate()||B->GetOuter()!=B->GetOutermost()||!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||B->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap)||!SaveAdmission(B,Save,AllowDirty))return Fail(R,TEXT("Idle compiled project AnimBlueprint and explicit dirty-save approval required"));
 if(FunctionName.IsEmpty()||FunctionName.Equals(TEXT("None"),ESearchCase::IgnoreCase)||FunctionName.Len()>128||!FChar::IsAlpha(FunctionName[0]))return Fail(R,TEXT("Fresh function identifier required"));for(TCHAR C:FunctionName)if(!FChar::IsAlnum(C)&&C!=TEXT('_'))return Fail(R,TEXT("Function identifier accepts letters, digits and underscore only"));
 FKismetNameValidator FunctionValidator(B);if(FunctionValidator.IsValid(FunctionName)!=EValidatorResult::Ok||FindObject<UObject>(B,*FunctionName))return Fail(R,TEXT("Fresh, nonconflicting engine-valid function name required"));
 TGuardValue<bool> Lock(Writing,true);Lease Blueprint(B),Skeleton(B->TargetSkeleton);TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);UEdGraph* G=nullptr;for(auto* X:Graphs){if(X->GetName()==FunctionName)return Fail(R,TEXT("Function/graph name already exists; existing function is never overwritten"));if(X->GetName()==GraphName){if(G)return Fail(R,TEXT("Ambiguous graph name"));G=X;}}
 if(B->GeneratedClass->FindFunctionByName(*FunctionName)||B->SkeletonGeneratedClass->FindFunctionByName(*FunctionName))return Fail(R,TEXT("Function conflicts with an existing or inherited member"));
 FGuid Guid;if(!G||!FGuid::Parse(Id,Guid))return Fail(R,TEXT("Exact graph and MotionMatching GUID required"));UAnimGraphNode_Base* N=nullptr;for(UEdGraphNode* X:G->Nodes)if(X&&X->NodeGuid==Guid&&X->GetClass()->GetName()==TEXT("AnimGraphNode_MotionMatching"))N=Cast<UAnimGraphNode_Base>(X);if(!N)return Fail(R,TEXT("MotionMatching node not found"));if(!N->UpdateFunction.GetMemberName().IsNone()&&!AllowReplace)return Fail(R,TEXT("Existing OnUpdate replacement requires allow_replace=true; old function is preserved"));
 Lease Node(N);TArray<Lease> Holds;for(auto* X:Graphs){Holds.Emplace(X);for(UEdGraphNode* Y:X->Nodes)if(Live(Y))Holds.Emplace(Y);}Snap Original,Protected,Now,SkeletonState;Props(B->TargetSkeleton,TEXT("skeleton"),SkeletonState);if(SkeletonState.Contains(TEXT("$invalid_precision_snapshot")))return Fail(R,TEXT("Skeleton snapshot precision/budget invalid"));if(!GraphState(B,N,Original,false)||!GraphState(B,N,Protected,false,true))return Fail(R,TEXT("Graph snapshot budget exceeded"));
 if(Compile)for(auto* X:Graphs)for(UEdGraphNode* Y:X->Nodes)if(auto* Slot=Cast<UAnimGraphNode_Slot>(Y);Slot&&!B->TargetSkeleton->ContainsSlotName(Slot->Node.SlotName))return Fail(R,TEXT("Compilation would register an unapproved Skeleton slot"));
 FString ScriptError;FScopedScriptExceptionHandler Handler([&](ELogVerbosity::Type V,const TCHAR* M,const TCHAR* Stack){if(V==ELogVerbosity::Error)ScriptError=FString(M).Left(2048);FScriptExceptionHandler::LoggingExceptionHandler(V,M,Stack);});
 UEdGraph* Created=nullptr;TStrongObjectPtr<UEdGraph> CreatedHold;TArray<Lease> NewNodes;TArray<Lease> Sources;TMap<UObject*,Snap> SourceStates;
 auto Check=[&](bool Mask){if(!Idle()||!ScriptError.IsEmpty()||!Blueprint.Current()||!Skeleton.Current()||!Node.Current())return false;for(const auto& H:Holds)if(!H.Current())return false;for(const auto& H:NewNodes)if(!H.Current()||H.Object->GetOuter()!=Created)return false;if(Created&&(!Live(Created)||Created->GetOuter()!=B))return false;for(const auto& H:Sources)if(!H.Current())return false;for(const auto& X:SourceStates){Snap S;FString Error;if(!CaptureInput(X.Key,S,Error)||!Same(X.Value,S,R)){R->SetStringField(TEXT("first_mismatch_object"),Path(X.Key));R->SetStringField(TEXT("input_observation_error"),Error);return false;}}Snap S;Props(B->TargetSkeleton,TEXT("skeleton"),S);return Same(SkeletonState,S,R)&&GraphState(B,N,Now,false,Mask,Created)&&Same(Mask?Protected:Original,Now,R);};
 auto Stop=[&](const FString& Error){R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("created_function"),Path(Created));return Fail(R,Error);};
 auto* C=Cast<UChooserTable>(Reference(ChooserPath,R));if(!Check(false))return Stop(TEXT("Chooser resolution changed target"));
 if(!Target(C)||C->ResultType!=EObjectChooserResultType::ObjectResult||C->OutputObjectType!=UPoseSearchDatabase::StaticClass()||C->ContextData.Num()!=1||C->ContextData[0].GetScriptStruct()!=FContextObjectTypeClass::StaticStruct()||!Live(C->ContextData[0].Get<FContextObjectTypeClass>().Class)||!B->GeneratedClass->IsChildOf(C->ContextData[0].Get<FContextObjectTypeClass>().Class))return Fail(R,TEXT("Chooser must return PoseSearchDatabase and accept this AnimInstance as its sole class context"));
 Snap CS;if(!Capture(C,CS))return Fail(R,TEXT("Chooser snapshot failed"));Sources.Emplace(C);SourceStates.Add(C,CS);Sources.Emplace(C->ContextData[0].Get<FContextObjectTypeClass>().Class);
 TArray<UObject*> Results;for(const auto& X:C->ResultsStructs){if(X.GetScriptStruct()!=FAssetChooser::StaticStruct())return Fail(R,TEXT("Direct Database result rows required"));Results.Add(X.Get<FAssetChooser>().Asset);}if(C->FallbackResult.IsValid()){if(C->FallbackResult.GetScriptStruct()!=FAssetChooser::StaticStruct())return Fail(R,TEXT("Direct Database fallback required"));Results.Add(C->FallbackResult.Get<FAssetChooser>().Asset);}
 if(Results.IsEmpty())return Fail(R,TEXT("At least one Database result required"));for(UObject* X:Results){auto* D=Cast<UPoseSearchDatabase>(X);if(!Target(D)||!Live(const_cast<UPoseSearchSchema*>(D->Schema.Get()))||D->Schema->GetSkeleton(UE::PoseSearch::DefaultRole)!=B->TargetSkeleton)return Fail(R,TEXT("Every result requires the same default-role Skeleton"));for(UObject* S:{X,static_cast<UObject*>(const_cast<UPoseSearchSchema*>(D->Schema.Get()))}){Snap V;if(!Capture(S,V))return Fail(R,TEXT("Database input snapshot failed"));Sources.Emplace(S);SourceStates.Add(S,V);}}
 FString Error;if(!CaptureInputs(C,nullptr,SourceStates,Sources,Error)||!RuntimeAudit(C,Error,R)||!Check(false)||!SaveAdmission(B,Save,AllowDirty))return Stop(Error.IsEmpty()?TEXT("Runtime audit/target preservation/save admission failed"):Error);
 UFunction* Prototype=UAnimExecutionContextLibrary::StaticClass()->FindFunctionByName(TEXT("Prototype_ThreadSafeAnimUpdateCall"));UFunction* Evaluate=UChooserFunctionLibrary::StaticClass()->FindFunctionByName(TEXT("EvaluateChooserMulti"));UFunction* Convert=UMotionMatchingAnimNodeLibrary::StaticClass()->FindFunctionByName(TEXT("ConvertToMotionMatchingNodePure"));UFunction* Set=UMotionMatchingAnimNodeLibrary::StaticClass()->FindFunctionByName(TEXT("SetDatabasesToSearch"));if(!Prototype||!Evaluate||!Convert||!Set)return Fail(R,TEXT("Installed runtime function signatures unavailable"));
 R->SetStringField(TEXT("asset"),Path(B));R->SetStringField(TEXT("function_name"),FunctionName);if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("validated"));return Encode(R);}
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("modify"));B->Modify(false);if(!Check(false))return Stop(TEXT("Blueprint Modify changed protected state"));N->Modify(false);if(!Check(false))return Stop(TEXT("Node Modify changed protected state"));
 R->SetStringField(TEXT("stage"),TEXT("create_update_function"));Created=FBlueprintEditorUtils::CreateNewGraph(B,FName(*FunctionName),UEdGraph::StaticClass(),UEdGraphSchema_K2::StaticClass());CreatedHold.Reset(Created);if(!Live(Created)||Created->GetName()!=FunctionName||Created->GetOuter()!=B||!Check(false))return Stop(TEXT("Function creation callback changed requested name/owner or protected state"));FBlueprintEditorUtils::CreateFunctionGraph(B,Created,true,Prototype);if(!Check(false))return Stop(TEXT("Function terminator callback changed protected state"));
 UK2Node_FunctionEntry* Entry=nullptr;UK2Node_FunctionResult* Exit=nullptr;for(UEdGraphNode* X:Created->Nodes){if(auto* E=Cast<UK2Node_FunctionEntry>(X)){if(Entry)return Stop(TEXT("Multiple entry nodes"));Entry=E;}else if(auto* Return=Cast<UK2Node_FunctionResult>(X))Exit=Return;else return Stop(TEXT("Unexpected node in fresh function"));NewNodes.Emplace(X);}if(!Entry||!Entry->FindPin(TEXT("Node"))||!Entry->FindPin(UEdGraphSchema_K2::PN_Then))return Stop(TEXT("Update signature pins missing"));Entry->MetaData.bThreadSafe=true;
 const UEdGraphSchema* Schema=Created->GetSchema();auto Call=[&](UFunction* F){FGraphNodeCreator<UK2Node_CallFunction> Creator(*Created);auto* V=Creator.CreateNode(false);NewNodes.Emplace(V);V->SetFromFunction(F);Creator.Finalize();return V;};
 auto* Eval=Call(Evaluate);if(!Check(false))return Stop(TEXT("Evaluate node callback changed protected state"));auto* CastNode=Call(Convert);if(!Check(false))return Stop(TEXT("Convert node callback changed protected state"));auto* SetNode=Call(Set);if(!Check(false))return Stop(TEXT("Set node callback changed protected state"));FGraphNodeCreator<UK2Node_Self> SelfCreator(*Created);auto* Self=SelfCreator.CreateNode(false);NewNodes.Emplace(Self);SelfCreator.Finalize();if(!Check(false))return Stop(TEXT("Self node callback changed protected state"));FGraphNodeCreator<UK2Node_IfThenElse> BranchCreator(*Created);auto* Branch=BranchCreator.CreateNode(false);NewNodes.Emplace(Branch);BranchCreator.Finalize();if(!Check(false))return Stop(TEXT("Branch node callback changed protected state"));
 if(Created->Nodes.Num()!=NewNodes.Num())return Stop(TEXT("Unexpected node inventory during function construction"));
 auto SetObject=[&](UEdGraphNode* X,const TCHAR* Pin,UObject* Value){auto* P=X->FindPin(Pin);if(!P)return false;Schema->TrySetDefaultObject(*P,Value);if(!Check(false))return false;P=X->FindPin(Pin);return P&&P->DefaultObject==Value;};if(!SetObject(Eval,TEXT("ChooserTable"),C)||!SetObject(Eval,TEXT("ObjectClass"),UPoseSearchDatabase::StaticClass()))return Stop(TEXT("Chooser defaults could not be set"));Eval->PinDefaultValueChanged(Eval->FindPin(TEXT("ObjectClass")));if(!Check(false))return Stop(TEXT("Chooser type callback changed protected state"));
 auto Wire=[&](UEdGraphNode* A,FName AP,UEdGraphNode* Z,FName ZP){auto* P=A->FindPin(AP);auto* Q=Z->FindPin(ZP);if(!P||!Q||!Schema->TryCreateConnection(P,Q)||!Check(false))return false;P=A->FindPin(AP);Q=Z->FindPin(ZP);return P&&Q&&P->LinkedTo.Contains(Q);};
 Schema->BreakPinLinks(*Entry->FindPin(UEdGraphSchema_K2::PN_Then),true);if(!Check(false))return Stop(TEXT("Fresh entry-link callback changed protected state"));
 if(!Wire(Entry,TEXT("Node"),CastNode,TEXT("Node"))||!Wire(Self,UEdGraphSchema_K2::PN_Self,Eval,TEXT("ContextObject"))||!Wire(Eval,UEdGraphSchema_K2::PN_ReturnValue,SetNode,TEXT("Databases"))||!Wire(CastNode,TEXT("MotionMatchingNode"),SetNode,TEXT("MotionMatchingNode"))||!Wire(CastNode,TEXT("Result"),Branch,UEdGraphSchema_K2::PN_Condition)||!Wire(Entry,UEdGraphSchema_K2::PN_Then,Branch,UEdGraphSchema_K2::PN_Execute)||!Wire(Branch,UEdGraphSchema_K2::PN_Then,SetNode,UEdGraphSchema_K2::PN_Execute))return Stop(TEXT("Typed update-function wiring failed; partial function retained for inspection"));
 if(Exit&&!Wire(SetNode,UEdGraphSchema_K2::PN_Then,Exit,UEdGraphSchema_K2::PN_Execute))return Stop(TEXT("Update return wiring failed"));
 if(Created->Nodes.Num()!=NewNodes.Num()||Eval->GetTargetFunction()!=Evaluate||CastNode->GetTargetFunction()!=Convert||SetNode->GetTargetFunction()!=Set)return Stop(TEXT("New graph node inventory changed"));for(const auto& H:NewNodes)if(!H.Current())return Stop(TEXT("New node lifetime changed"));
 auto BodyValid=[&]{
  if(!Check(true)||Created->Nodes.Num()!=NewNodes.Num()||!Entry->MetaData.bThreadSafe||Eval->GetTargetFunction()!=Evaluate||CastNode->GetTargetFunction()!=Convert||SetNode->GetTargetFunction()!=Set)return false;
  auto* TablePin=Eval->FindPin(TEXT("ChooserTable"));auto* ClassPin=Eval->FindPin(TEXT("ObjectClass"));if(!TablePin||!ClassPin||TablePin->DefaultObject!=C||ClassPin->DefaultObject!=UPoseSearchDatabase::StaticClass())return false;
  TMap<UEdGraphPin*,TSet<UEdGraphPin*>> Required;
  auto Pair=[&](UEdGraphNode* A,FName AP,UEdGraphNode* Z,FName ZP){auto* P=A->FindPin(AP);auto* Q=Z->FindPin(ZP);if(!P||!Q)return false;Required.FindOrAdd(P).Add(Q);Required.FindOrAdd(Q).Add(P);return true;};
  if(!Pair(Entry,TEXT("Node"),CastNode,TEXT("Node"))||!Pair(Self,UEdGraphSchema_K2::PN_Self,Eval,TEXT("ContextObject"))||!Pair(Eval,UEdGraphSchema_K2::PN_ReturnValue,SetNode,TEXT("Databases"))||!Pair(CastNode,TEXT("MotionMatchingNode"),SetNode,TEXT("MotionMatchingNode"))||!Pair(CastNode,TEXT("Result"),Branch,UEdGraphSchema_K2::PN_Condition)||!Pair(Entry,UEdGraphSchema_K2::PN_Then,Branch,UEdGraphSchema_K2::PN_Execute)||!Pair(Branch,UEdGraphSchema_K2::PN_Then,SetNode,UEdGraphSchema_K2::PN_Execute)||(Exit&&!Pair(SetNode,UEdGraphSchema_K2::PN_Then,Exit,UEdGraphSchema_K2::PN_Execute)))return false;
  for(UEdGraphNode* X:Created->Nodes)for(auto* P:X->Pins){if(!P)return false;const auto* Links=Required.Find(P);if(P->LinkedTo.Num()!=(Links?Links->Num():0))return false;for(auto* Q:P->LinkedTo)if(!Links||!Links->Contains(Q))return false;}return true;
 };
 Snap FrozenBody;if(!BodyValid()||!GraphState(B,N,FrozenBody,false,true,nullptr,Created))return Stop(TEXT("Authored function contract failed before publication"));
 FMemberReference PlannedUpdate;PlannedUpdate.SetSelfMember(FName(*FunctionName));bool BindingWritten=false;auto Bound=[&]{return FMemberReference::StaticStruct()->CompareScriptStruct(&PlannedUpdate,&N->UpdateFunction,PPF_None);};
 auto PublishedValid=[&]{return BodyValid()&&(!BindingWritten||Bound())&&GraphState(B,N,Now,false,true)&&Same(FrozenBody,Now,R);};
 B->FunctionGraphs.Add(Created);if(!PublishedValid())return Stop(TEXT("Function publication changed protected graph"));N->UpdateFunction=PlannedUpdate;BindingWritten=true;if(!PublishedValid())return Stop(TEXT("Update binding changed unrelated graph"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);if(!PublishedValid())return Stop(TEXT("Structural callback changed protected graph"));
 Snap Expected;if(!GraphState(B,N,Expected,false))return Stop(TEXT("Post-publication snapshot failed"));R->SetBoolField(TEXT("changed"),true);R->SetStringField(TEXT("created_function"),Path(Created));
 auto Final=[&]{if(!PublishedValid()||!Entry->MetaData.bThreadSafe||!Bound()||!GraphState(B,N,Now,false)||!Same(Expected,Now,R))return false;for(const auto& H:NewNodes)if(!H.Current())return false;return true;};
 if(Compile){R->SetStringField(TEXT("stage"),TEXT("compile"));R->SetBoolField(TEXT("compile_attempted"),true);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Log);R->SetNumberField(TEXT("compile_errors"),Log.NumErrors);R->SetBoolField(TEXT("compiled"),Log.NumErrors==0);if(!Final()||Log.NumErrors)return Stop(TEXT("Compile/preservation failed; no save"));}
 if(Save){R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;const FString File=FPackageName::LongPackageNameToFilename(B->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());R->SetBoolField(TEXT("saved"),UPackage::SavePackage(B->GetOutermost(),B,*File,Args));if(!Final()||!R->GetBoolField(TEXT("saved")))return Stop(TEXT("Save/post-save failed; actual saved result retained"));}
 R->SetStringField(TEXT("runtime_functions"),TEXT("Chooser.EvaluateChooserMulti;PoseSearch.ConvertToMotionMatchingNodePure;PoseSearch.SetDatabasesToSearch"));R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}


FString CreateLayer(UObject* Object,const FString& Name,bool Execute,bool Compile,bool Save,bool AllowDirty)
{
 auto R=Result(TEXT("create_anim_layer"));auto* B=Cast<UAnimBlueprint>(Object);if(!Idle()||Writing||!Live(B)||!Live(B->TargetSkeleton)||!Live(B->GeneratedClass)||!Live(B->SkeletonGeneratedClass)||B->IsTemplate()||B->GetOuter()!=B->GetOutermost()||!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/"))||B->GetOutermost()->HasAnyPackageFlags(PKG_ContainsMap)||!SaveAdmission(B,Save,AllowDirty)||UAnimBlueprint::FindRootAnimBlueprint(B))return Fail(R,TEXT("Idle root project AnimBlueprint with live Skeleton and explicit dirty save admission required; inherited layers must be edited through their owning Blueprint"));
 if(Name.IsEmpty()||Name.Equals(TEXT("None"),ESearchCase::IgnoreCase)||Name.Len()>128||!FChar::IsAlpha(Name[0]))return Fail(R,TEXT("Fresh layer identifier required"));for(TCHAR C:Name)if(!FChar::IsAlnum(C)&&C!=TEXT('_'))return Fail(R,TEXT("Layer name accepts identifier characters only"));
 FKismetNameValidator LayerValidator(B);if(LayerValidator.IsValid(Name)!=EValidatorResult::Ok||FindObject<UObject>(B,*Name))return Fail(R,TEXT("Fresh, nonconflicting engine-valid layer name required"));
 TGuardValue<bool> Lock(Writing,true);TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);for(auto* G:Graphs)if(G->GetName().Equals(Name,ESearchCase::IgnoreCase))return Fail(R,TEXT("Graph name already exists"));if(B->GeneratedClass->FindFunctionByName(*Name)||B->SkeletonGeneratedClass->FindFunctionByName(*Name))return Fail(R,TEXT("Inherited or existing function conflict"));
 Lease Blueprint(B),Skeleton(B->TargetSkeleton);TArray<Lease> Holds;for(auto* G:Graphs){Holds.Emplace(G);for(UEdGraphNode* X:G->Nodes)if(Live(X))Holds.Emplace(X);}Snap Before,Now,Expected,SS;Props(B->TargetSkeleton,TEXT("skeleton"),SS);if(SS.Contains(TEXT("$invalid_precision_snapshot")))return Fail(R,TEXT("Skeleton snapshot invalid"));if(!GraphState(B,nullptr,Before,false))return Fail(R,TEXT("Graph snapshot budget exceeded"));if(Compile)for(auto* G:Graphs)for(UEdGraphNode* X:G->Nodes)if(auto* Slot=Cast<UAnimGraphNode_Slot>(X);Slot&&!B->TargetSkeleton->ContainsSlotName(Slot->Node.SlotName))return Fail(R,TEXT("Compilation would register an unapproved Skeleton slot"));
 FString ScriptError;FScopedScriptExceptionHandler Handler([&](ELogVerbosity::Type V,const TCHAR* M,const TCHAR* Stack){if(V==ELogVerbosity::Error)ScriptError=FString(M).Left(2048);FScriptExceptionHandler::LoggingExceptionHandler(V,M,Stack);});
 UAnimationGraph* New=nullptr;TStrongObjectPtr<UAnimationGraph> Hold;TArray<Lease> NewNodes;
 auto Check=[&]{if(!Idle()||!ScriptError.IsEmpty()||!Blueprint.Current()||!Skeleton.Current()||(New&&(!Live(New)||New->GetOuter()!=B)))return false;for(const auto& H:Holds)if(!H.Current())return false;for(const auto& H:NewNodes)if(!H.Current())return false;Snap S;Props(B->TargetSkeleton,TEXT("skeleton"),S);return Same(SS,S,R)&&GraphState(B,nullptr,Now,false,false,New)&&Same(Before,Now,R);};
 auto Stop=[&](const FString& Error){R->SetBoolField(TEXT("change_state_known"),false);R->SetStringField(TEXT("created_graph"),Path(New));return Fail(R,Error);};R->SetStringField(TEXT("asset"),Path(B));if(!Execute){R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("validated"));return Encode(R);}
 R->SetBoolField(TEXT("mutation_attempted"),true);R->SetStringField(TEXT("stage"),TEXT("modify"));B->Modify(false);if(!Check())return Stop(TEXT("Modify callback changed protected state"));R->SetStringField(TEXT("stage"),TEXT("create_layer"));New=Cast<UAnimationGraph>(FBlueprintEditorUtils::CreateNewGraph(B,*Name,UAnimationGraph::StaticClass(),UAnimationGraphSchema::StaticClass()));Hold.Reset(New);if(!New||New->GetName()!=Name||!Check())return Stop(TEXT("Layer graph creation failed"));New->GetSchema()->CreateDefaultNodesForGraph(*New);if(!Check()||New->Nodes.Num()!=1||!Cast<UAnimGraphNode_Root>(New->Nodes[0]))return Stop(TEXT("Fresh layer must contain exactly its standard pose result"));NewNodes.Emplace(New->Nodes[0]);for(auto* P:New->Nodes[0]->Pins)if(!P||!P->LinkedTo.IsEmpty())return Stop(TEXT("Unexpected links in fresh layer"));if(!GraphState(B,nullptr,Expected,false,false,nullptr,New))return Stop(TEXT("Layer snapshot failed before publication"));
 B->FunctionGraphs.Add(New);auto Final=[&]{return Check()&&GraphState(B,nullptr,Now,false)&&Same(Expected,Now,R);};if(!Final())return Stop(TEXT("Layer publication changed authored state"));FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(B);if(!Final())return Stop(TEXT("Structural callback changed authored state"));R->SetBoolField(TEXT("changed"),true);R->SetStringField(TEXT("created_graph"),Path(New));
 if(Compile){R->SetStringField(TEXT("stage"),TEXT("compile"));R->SetBoolField(TEXT("compile_attempted"),true);FCompilerResultsLog Log;FKismetEditorUtilities::CompileBlueprint(B,EBlueprintCompileOptions::None,&Log);R->SetNumberField(TEXT("compile_errors"),Log.NumErrors);R->SetBoolField(TEXT("compiled"),Log.NumErrors==0);if(!Final()||Log.NumErrors)return Stop(TEXT("Compile/preservation failed; no save"));}
 if(Save){R->SetStringField(TEXT("stage"),TEXT("save"));R->SetBoolField(TEXT("save_attempted"),true);FSavePackageArgs Args;Args.TopLevelFlags=RF_Public|RF_Standalone;Args.SaveFlags=SAVE_NoError;Args.bSlowTask=false;FString File=FPackageName::LongPackageNameToFilename(B->GetOutermost()->GetName(),FPackageName::GetAssetPackageExtension());R->SetBoolField(TEXT("saved"),UPackage::SavePackage(B->GetOutermost(),B,*File,Args));if(!Final()||!R->GetBoolField(TEXT("saved")))return Stop(TEXT("Save/post-save failed; actual saved result retained"));}
 R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));R->SetStringField(TEXT("scope"),TEXT("Creates a self animation layer with a standard pose output; interface creation and typed input signature editing are separate operations. Connect its body with graph tools."));return Encode(R);
}


FString InspectBindings(UObject* Object)
{
 auto R=Result(TEXT("inspect_animation_blueprint_bindings"));auto* B=Cast<UAnimBlueprint>(Object);if(!Idle()||Writing||!Live(B)||!Live(B->TargetSkeleton)||B->IsTemplate()||B->GetOuter()!=B->GetOutermost()||!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/")))return Fail(R,TEXT("Idle explicit project AnimBlueprint required"));Snap Before,After;if(!GraphState(B,nullptr,Before,false))return Fail(R,TEXT("Authored graph snapshot invalid/budget exceeded"));Lease Blueprint(B),Skeleton(B->TargetSkeleton);TArray<UEdGraph*> Graphs;B->GetAllGraphs(Graphs);TArray<TSharedPtr<FJsonValue>> Rows;
 for(auto* G:Graphs)for(UEdGraphNode* N:G->Nodes){auto Row=MakeShared<J>();Row->SetStringField(TEXT("graph"),G->GetName());Row->SetStringField(TEXT("graph_path"),Path(G));Row->SetStringField(TEXT("node_id"),N->NodeGuid.ToString());Row->SetStringField(TEXT("class"),Path(N->GetClass()));Row->SetStringField(TEXT("path"),Path(N));for(const TCHAR* Field:{TEXT("FunctionReference"),TEXT("UpdateFunction"),TEXT("InterfaceGuid"),TEXT("SaveCachedPoseNode"),TEXT("NameOfCache")})if(auto* P=FindFProperty<FProperty>(N->GetClass(),Field)){FString Text;P->ExportTextItem_Direct(Text,P->ContainerPtrToValuePtr<void>(N),nullptr,N,PPF_None);Row->SetStringField(Field,Text);}if(auto* L=Cast<UAnimGraphNode_LinkedAnimLayer>(N)){Row->SetStringField(TEXT("layer"),L->Node.Layer.ToString());Row->SetStringField(TEXT("interface"),Path(L->Node.Interface));Row->SetStringField(TEXT("instance_class"),Path(L->Node.InstanceClass));}Rows.Add(MakeShared<FJsonValueObject>(Row));}
 if(!Blueprint.Current()||!Skeleton.Current()||!Idle()||!GraphState(B,nullptr,After,false)||!Same(Before,After,R)){R->SetBoolField(TEXT("change_state_known"),false);return Fail(R,TEXT("Inspection boundary changed authored graph"));}R->SetArrayField(TEXT("nodes"),Rows);R->SetStringField(TEXT("asset"),Path(B));R->SetNumberField(TEXT("blueprint_status"),B->Status);R->SetBoolField(TEXT("dirty_after"),B->GetOutermost()->IsDirty());R->SetBoolField(TEXT("success"),true);R->SetStringField(TEXT("stage"),TEXT("complete"));return Encode(R);
}

}
