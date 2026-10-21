// HH: Editor-only ControlRig recipe validation and guarded authoring. Source candidate only.
#include "Compatibility/UE58/CubelessControlRigRecipeAuthoringApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Operations/CubelessControlRigRecipeAuthoringOperations.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Animation/Skeleton.h"
#include "ControlRig.h"
#include "ControlRigBlueprintLegacy.h"
#include "ControlRigEditorAsset.h"
#include "ControlRigAssetReference.h"
#include "ControlRigBlueprintEditorLibrary.h"
#include "ControlRigBlueprintFactory.h"
#include "Dom/JsonObject.h"
#include "Dom/JsonValue.h"
#include "Engine/SkeletalMesh.h"
#include "Engine/World.h"
#include "Editor.h"
#include "FileHelpers.h"
#include "HAL/FileManager.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Misc/PackageName.h"
#include "Misc/SecureHash.h"
#include "RigVMModel/Nodes/RigVMUnitNode.h"
#include "RigVMModel/RigVMController.h"
#include "RigVMModel/RigVMGraph.h"
#include "RigVMModel/RigVMPin.h"
#include "RigVMModel/RigVMLink.h"
#include "Rigs/RigHierarchyController.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Units/Execution/RigUnit_BeginExecution.h"
#include "Units/Execution/RigUnit_InverseExecution.h"
#include "Units/Execution/RigUnit_PrepareForExecution.h"
#include "Units/Hierarchy/RigUnit_SetControlTransform.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/UObjectHash.h"
#include "UObject/StrongObjectPtr.h"
namespace Cubeless::UE58::ControlRigRecipeAuthoring
{
namespace
{
using namespace Cubeless::Operations::ControlRigRecipeAuthoring;
UScriptStruct* GetAllowedUnitScriptStruct(const FString& NormalizedStructName)
{
	if (NormalizedStructName == TEXT("FRigUnit_BeginExecution"))
	{
		return FRigUnit_BeginExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_PreBeginExecution"))
	{
		return FRigUnit_PreBeginExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_PostBeginExecution"))
	{
		return FRigUnit_PostBeginExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_PrepareForExecution"))
	{
		return FRigUnit_PrepareForExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_PostPrepareForExecution"))
	{
		return FRigUnit_PostPrepareForExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_InverseExecution"))
	{
		return FRigUnit_InverseExecution::StaticStruct();
	}
	if (NormalizedStructName == TEXT("FRigUnit_SetControlFloat"))
	{
		return FRigUnit_SetControlFloat::StaticStruct();
	}
	return nullptr;
}



bool Running=false;
constexpr int32 MaxCarrier=1048576,MaxNodes=4096,MaxPins=32768,MaxElements=8192;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool Same(const FString& A,const FString& B){return A.Equals(B,ESearchCase::CaseSensitive);}
bool GlobalsSafe(){return IsInGameThread()&&!IsEngineExitRequested()&&!GExitPurge&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&Live(GEditor);}
bool TextSafe(const FString& S,int32 Limit=MaxCarrier){if(S.Len()>Limit)return false;for(int32 I=0;I<S.Len();++I)if(S[I]==0)return false;return true;}
bool JsonSafe(const TSharedPtr<FJsonValue>& V,int32 Depth,int32& Slots){if(!V||Depth>64||++Slots>262144)return false;if(V->Type==EJson::Number){double N=0;return V->TryGetNumber(N)&&FMath::IsFinite(N)&&FMath::Abs(N)<=static_cast<double>(MAX_flt);}if(V->Type==EJson::String)return TextSafe(V->AsString());if(V->Type==EJson::Array){for(const auto& E:V->AsArray())if(!JsonSafe(E,Depth+1,Slots))return false;}else if(V->Type==EJson::Object){for(const auto& E:V->AsObject()->Values)if(!TextSafe(FString(E.Key))||!JsonSafe(E.Value,Depth+1,Slots))return false;}return V->Type!=EJson::None;}
bool JsonBodySafe(const FString& S){if(!TextSafe(S))return false;int32 Depth=0;bool Q=false,E=false;for(int32 I=0;I<S.Len();++I){TCHAR C=S[I];if(Q){if(E)E=false;else if(C=='\\')E=true;else if(C=='"')Q=false;}else if(C=='"')Q=true;else if(C=='{'||C=='['){if(++Depth>64)return false;}else if(C=='}'||C==']'){if(--Depth<0)return false;}}if(Q||Depth!=0)return true;TArray<TSharedPtr<FJsonValue>> A;if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[")+S+TEXT("]")),A))return true;int32 Slots=0;for(const auto& V:A)if(!JsonSafe(V,0,Slots))return false;return true;}
struct FIdentity
{
 TStrongObjectPtr<UObject> Hold{nullptr};TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package,Outermost;FString Path;
 explicit FIdentity(UObject* O):Hold(O),Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetPackage()),Outermost(O->GetOutermost()),Path(O->GetPathName()){}
 bool Exact()const{auto* O=Object.Get();return Live(O)&&Live(Class.Get())&&Live(Package.Get())&&Live(Outermost.Get())&&O->GetClass()==Class.Get()&&!Class->HasAnyClassFlags(CLASS_NewerVersionExists)&&O->GetOuter()==Outer.Get()&&O->GetPackage()==Package.Get()&&O->GetOutermost()==Outermost.Get()&&Same(O->GetPathName(),Path);}
};
FString ControlValueText(const FRigControlValue& V,const FRigControlSettings& S){if(S.ControlType==ERigControlType::Bool)return V.Get<bool>()?TEXT("true"):TEXT("false");if(S.ControlType==ERigControlType::Float)return FString::SanitizeFloat(V.Get<float>());return V.GetAsTransform(S.ControlType,S.PrimaryAxis).ToString();}
struct FWork
{
 Variable::FResult R;FHHControlRigBuildReport Report;TArray<TUniquePtr<FIdentity>> Objects;TSet<UObject*> Seen;
 TStrongObjectPtr<UEditorEngine> Editor{nullptr};TStrongObjectPtr<UWorld> World{nullptr};FIdentity* WorldId=nullptr;
 UControlRigBlueprint* BP=nullptr;UPackage* Package=nullptr;URigHierarchy* Hierarchy=nullptr;URigHierarchyController* HC=nullptr;URigVMGraph* Graph=nullptr;URigVMController* VC=nullptr;
 TArray<TStrongObjectPtr<URigVMPin>> PinHolds;TSet<URigVMPin*> HeldPins;TStrongObjectPtr<UClass> Generated{nullptr},SkeletonClass{nullptr};TWeakObjectPtr<USkeletalMesh> ExpectedPreview;bool PreviewCaptured=false;
 bool Lost=false,Bound=false,SDKEntered=false,CompileTransition=false,FinalVerified=false;FString TargetPackage,TargetObject,TargetFile,LastHierarchy,LastGraph;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();TArray<TSharedPtr<FJsonValue>> Calls;TSharedPtr<FJsonObject> ActiveCall;
 FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),false);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback"),false);R.Data->SetBoolField(TEXT("cleanup_performed"),false);R.Data->SetBoolField(TEXT("serialized_transitive_runtime_independence_proven"),false);}
 void Error(const TCHAR* Code,const FString& Message){if(R.ErrorCode.IsEmpty()){R.ErrorCode=Code;R.Error=Message;}FinalVerified=false;}
 bool Fail(const TCHAR* Code,const FString& Message){Error(Code,Message);return false;}
 void Count(const TCHAR* Key){double N=0;Counts->TryGetNumberField(Key,N);Counts->SetNumberField(Key,N+1);}
 bool Pin(UObject* O){if(!GlobalsSafe()||Lost||!Live(O))return Fail(TEXT("controlrig_recipe_object_unavailable"),TEXT("Current live object unavailable; no further access"));if(Seen.Contains(O))return true;if(Objects.Num()>=65536)return Fail(TEXT("controlrig_recipe_object_budget"),TEXT("Object pin budget exceeded"));if(!Live(O->GetClass())||!Live(O->GetPackage())||!Live(O->GetOutermost()))return Fail(TEXT("controlrig_recipe_identity_unavailable"),TEXT("Current object class/package is unavailable"));Seen.Add(O);Objects.Add(MakeUnique<FIdentity>(O));return true;}
 bool Init(){if(!GlobalsSafe()||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor)return Fail(TEXT("controlrig_recipe_editor_unavailable"),TEXT("Editor GT/noPIE/noGC/no save context required"));Editor.Reset(GEditor);World.Reset(GEditor->GetEditorWorldContext().World());if(!Live(World.Get())||World->bIsTearingDown)return Fail(TEXT("controlrig_recipe_world_unavailable"),TEXT("Live Editor world required"));if(!Pin(World.Get()))return false;WorldId=Objects.Last().Get();return true;}
 bool Guard(){if(Lost||!R.ErrorCode.IsEmpty())return false;if(!GlobalsSafe()||GEditor!=Editor.Get()||GEditor->PlayWorld||GEditor->bIsSimulatingInEditor||GEditor->GetEditorWorldContext().World()!=World.Get()||!Live(World.Get())||World->bIsTearingDown){Lost=true;return Fail(TEXT("controlrig_recipe_context_lost"),TEXT("GC/exit/save/PIE or exact Editor/world changed; stop without replay"));}for(const auto& I:Objects)if(!I->Exact()){Lost=true;return Fail(TEXT("controlrig_recipe_identity_changed"),TEXT("Captured object class/outer/effective package/path changed"));}if(Bound){if(BP->GetClass()!=UControlRigBlueprint::StaticClass()||BP->GetPackage()!=Package||BP->GetOutermost()!=Package||BP->Hierarchy!=Hierarchy)return Fail(TEXT("controlrig_recipe_owner_changed"),TEXT("Exact stock Blueprint/hierarchy/package owner changed"));if(!CompileTransition&&(BP->GeneratedClass.Get()!=Generated.Get()||BP->SkeletonGeneratedClass.Get()!=SkeletonClass.Get()))return Fail(TEXT("controlrig_recipe_class_changed"),TEXT("Current generated/skeleton class changed outside compile"));if(Graph&&BP->GetDefaultModel()!=Graph)return Fail(TEXT("controlrig_recipe_model_changed"),TEXT("Exact default RigVM model changed"));if(PreviewCaptured){const auto P=BP->GetPreviewSkeletalMesh();if(P.Get()!=ExpectedPreview.Get()||!P.IsNull()&&!P.Get())return Fail(TEXT("controlrig_recipe_preview_changed"),TEXT("Exact preview mesh reference changed"));}}return true;}
 bool Before(const TCHAR* Name,bool Writes){if(!Guard())return false;R.Stage=Name;SDKEntered=true;R.bChangeStateKnown=false;R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);if(Writes)R.bMutationAttempted=true;Count(Name);ActiveCall=MakeShared<FJsonObject>();ActiveCall->SetStringField(TEXT("stage"),Name);ActiveCall->SetBoolField(TEXT("mutation_attempted"),Writes);ActiveCall->SetBoolField(TEXT("SDK_returned"),false);if(Calls.Num()>=4096)return Fail(TEXT("controlrig_recipe_call_budget"),TEXT("SDK call receipt budget exceeded"));Calls.Add(MakeShared<FJsonValueObject>(ActiveCall));return true;}
 bool After(){if(ActiveCall)ActiveCall->SetBoolField(TEXT("SDK_returned"),true);return Guard();}
 template<class F>bool Call(const TCHAR* Name,bool Writes,F&& Fn){if(!Before(Name,Writes))return false;Fn();return After();}
 bool PackageOwned()
 {
  if(!Guard()||!Package||!BP)return false;
  // UClass creates its CDO under the class outer (the package), not under
  // the UClass. Admit only the current exact generated/skeleton role. false
  // forbids constructing a missing CDO and no old CDO pin crosses a compile.
  UClass* G=BP->GeneratedClass.Get();UClass* S=BP->SkeletonGeneratedClass.Get();
  UObject* GD=nullptr;UObject* SD=nullptr;
  auto CurrentDefault=[&](UClass* C,UObject*& D)->bool
  {
   if(!C)return true;
   if(!Live(C)||C->HasAnyClassFlags(CLASS_NewerVersionExists)||C->ClassGeneratedBy!=BP||C->GetOuter()!=Package||C->GetPackage()!=Package||C->GetOutermost()!=Package)return false;
   D=C->GetDefaultObject(false);
   if(!D)return true;
   const FString ExpectedPath=Package->GetPathName()+TEXT(".")+C->GetDefaultObjectName().ToString();
   return Live(D)&&D->HasAnyFlags(RF_ClassDefaultObject)&&D->GetClass()==C&&D->GetOuter()==C->GetOuter()&&D->GetPackage()==Package&&D->GetOutermost()==Package&&D->GetFName()==C->GetDefaultObjectName()&&Same(D->GetPathName(),ExpectedPath);
  };
  const bool GRole=CurrentDefault(G,GD);const bool SRole=CurrentDefault(S,SD);
  auto Snapshot=[&](UObject* O,const TCHAR* Reason)
  {
   auto J=MakeShared<FJsonObject>();J->SetStringField(TEXT("reason"),Reason);
   const bool Available=Live(O);J->SetBoolField(TEXT("export_available"),Available);
   if(Available){J->SetStringField(TEXT("path"),O->GetPathName());J->SetStringField(TEXT("class"),GetPathNameSafe(O->GetClass()));J->SetStringField(TEXT("outer"),GetPathNameSafe(O->GetOuter()));J->SetStringField(TEXT("effective_package"),GetPathNameSafe(O->GetPackage()));J->SetStringField(TEXT("outermost_package"),GetPathNameSafe(O->GetOutermost()));J->SetBoolField(TEXT("class_default_object"),O->HasAnyFlags(RF_ClassDefaultObject));}
   J->SetStringField(TEXT("current_generated_class"),Live(G)?G->GetPathName():FString());J->SetStringField(TEXT("current_skeleton_class"),Live(S)?S->GetPathName():FString());J->SetStringField(TEXT("current_generated_CDO"),Live(GD)?GD->GetPathName():FString());J->SetStringField(TEXT("current_skeleton_CDO"),Live(SD)?SD->GetPathName():FString());J->SetBoolField(TEXT("current_generated_role_valid"),GRole);J->SetBoolField(TEXT("current_skeleton_role_valid"),SRole);R.Data->SetObjectField(TEXT("rejected_package_export"),J);
  };
  if(!GRole||!SRole){Snapshot(!GRole?GD:SD,TEXT("current_class_CDO_role_invalid"));return Fail(TEXT("controlrig_recipe_foreign_package_export"),TEXT("Current generated/skeleton class or CDO identity does not belong to exact Blueprint/package"));}
  // These holds protect only this observation, never the next compile transition.
  TStrongObjectPtr<UObject> CurrentGeneratedDefault(GD),CurrentSkeletonDefault(SD);
  TArray<UObject*> All;GetObjectsWithPackage(Package,All,EGetObjectsFlags::IncludeNestedObjects);
  if(All.Num()>65536)return Fail(TEXT("controlrig_recipe_package_budget"),TEXT("Target package export budget exceeded"));
  for(auto* O:All)
  {
   if(!Live(O)){Snapshot(O,TEXT("export_unavailable"));return Fail(TEXT("controlrig_recipe_package_object_unavailable"),TEXT("Current target package export is unavailable"));}
   const bool Allowed=O==BP||O->IsIn(BP)||O==G||G&&O->IsIn(G)||O==S||S&&O->IsIn(S)||O==GD||GD&&O->IsIn(GD)||O==SD||SD&&O->IsIn(SD);
   if(!Allowed||O->GetPackage()!=Package||O->GetOutermost()!=Package){Snapshot(O,TEXT("foreign_export_or_package_assignment"));return Fail(TEXT("controlrig_recipe_foreign_package_export"),TEXT("Foreign target package export prevents owned package write/audit"));}
  }
  R.Data->SetBoolField(TEXT("current_class_CDO_package_roles_verified"),true);return true;
 }
 bool Bind(UControlRigBlueprint* O){if(!Guard()||!Live(O)||O->GetClass()!=UControlRigBlueprint::StaticClass()||!Same(O->GetPathName(),TargetObject)||!Same(O->GetOutermost()->GetName(),TargetPackage)||O->GetOuter()!=O->GetOutermost()||O->GetPackage()!=O->GetOutermost())return Fail(TEXT("controlrig_recipe_target_mismatch"),TEXT("Factory/load returned a different class/target/package"));BP=O;Package=O->GetOutermost();Hierarchy=O->Hierarchy;if(!Pin(BP)||!Pin(Package)||!Pin(Hierarchy)||!Hierarchy->IsIn(BP))return Fail(TEXT("controlrig_recipe_hierarchy_owner_invalid"),TEXT("Exact owned hierarchy required"));Generated.Reset(BP->GeneratedClass.Get());SkeletonClass.Reset(BP->SkeletonGeneratedClass.Get());Bound=true;return PackageOwned();}
 bool SnapshotHierarchy(FString& Out,const TSet<FRigElementKey>* Only=nullptr){if(!Guard()||!Hierarchy)return false;const TArray<FRigElementKey> Keys=Hierarchy->GetAllKeys();if(Keys.Num()>MaxElements)return Fail(TEXT("controlrig_recipe_hierarchy_budget"),TEXT("Hierarchy element cap exceeded"));Out.Reset();for(const auto& K:Keys){if(Only&&!Only->Contains(K))continue;if(!K.IsValid())return Fail(TEXT("controlrig_recipe_hierarchy_invalid"),TEXT("Invalid current hierarchy key"));Out+=K.ToString()+TEXT("|")+Hierarchy->GetFirstParent(K).ToString()+TEXT("|")+Hierarchy->GetLocalTransform(K,true).ToString()+TEXT(";");if(K.Type==ERigElementType::Curve)Out+=FString::SanitizeFloat(Hierarchy->GetCurveValue(K));if(K.Type==ERigElementType::Control){auto* C=Hierarchy->Find<FRigControlElement>(K);if(!C)return false;Out+=FString::FromInt(static_cast<int32>(C->Settings.ControlType))+TEXT("|")+ControlValueText(Hierarchy->GetControlValue(K,ERigControlValueType::Initial),C->Settings)+TEXT("|")+C->Settings.ShapeName.ToString()+TEXT("|")+C->Settings.ShapeColor.ToString()+TEXT("|")+FString::FromInt(C->Settings.bShapeVisible)+TEXT("|")+Hierarchy->GetControlOffsetTransform(C,ERigTransformType::InitialLocal).ToString()+TEXT("|")+Hierarchy->GetControlShapeTransform(C,ERigTransformType::InitialLocal).ToString();}if(Out.Len()>8*MaxCarrier)return Fail(TEXT("controlrig_recipe_projection_budget"),TEXT("Hierarchy projection cap exceeded"));}return Guard();}
 bool GraphSafe(){if(!Guard()||!Graph)return false;if(!Live(Graph)||!Graph->IsIn(BP)||!Pin(Graph))return Fail(TEXT("controlrig_recipe_graph_owner_invalid"),TEXT("Current RigVM graph must be inside target Blueprint"));auto Nodes=Graph->GetNodes();if(Nodes.Num()>MaxNodes)return Fail(TEXT("controlrig_recipe_graph_budget"),TEXT("RigVM node cap exceeded"));TSet<URigVMPin*> Known;int32 N=0;for(auto* Node:Nodes){if(!Live(Node)||Node->GetGraph()!=Graph||Node->GetOuter()!=Graph||!Pin(Node))return Fail(TEXT("controlrig_recipe_node_owner_invalid"),TEXT("Physical model node ownership mismatch"));TArray<URigVMPin*> Pending;if(!Call(TEXT("get_current_node_pins"),false,[&]{Pending=Node->GetPins();})||!Graph->GetNodes().Contains(Node))return false;while(!Pending.IsEmpty()){auto* P=Pending.Pop(EAllowShrinking::No);if(++N>MaxPins||!Live(P)||P->GetNode()!=Node||!P->IsIn(Node)||Known.Contains(P))return Fail(TEXT("controlrig_recipe_pin_owner_invalid"),TEXT("Physical typed pin ownership/cycle/budget mismatch"));Known.Add(P);if(!HeldPins.Contains(P)){HeldPins.Add(P);PinHolds.Emplace(P);}Pending.Append(P->GetSubPins());}}
 for(auto* L:Graph->GetLinks()){if(!Live(L)||L->GetGraph()!=Graph||!L->IsIn(Graph))return Fail(TEXT("controlrig_recipe_link_owner_invalid"),TEXT("Physical link ownership mismatch"));auto* A=L->GetSourcePin();auto* B=L->GetTargetPin();if(!Known.Contains(A)||!Known.Contains(B))return Fail(TEXT("controlrig_recipe_link_endpoint_invalid"),TEXT("Exact current link endpoints required"));}return true;}
 bool GraphSignature(FString& Out){if(!GraphSafe())return false;Out.Reset();for(auto* N:Graph->GetNodes()){Out+=N->GetName()+TEXT("|")+N->GetClass()->GetPathName()+TEXT(";");TArray<URigVMPin*> Pending;if(!Call(TEXT("get_projection_node_pins"),false,[&]{Pending=N->GetPins();})||!Graph->GetNodes().Contains(N))return false;while(!Pending.IsEmpty()){auto* P=Pending.Pop(EAllowShrinking::No);Out+=P->GetPinPath()+TEXT("|")+P->GetCPPType()+TEXT("|")+P->GetDefaultValue()+TEXT(";");Pending.Append(P->GetSubPins());}}for(auto* L:Graph->GetLinks())Out+=L->GetSourcePinPath()+TEXT("->")+L->GetTargetPinPath()+TEXT(";");if(Out.Len()>8*MaxCarrier)return Fail(TEXT("controlrig_recipe_projection_budget"),TEXT("Graph projection cap exceeded"));return Guard();}
 bool Idle(){if(!Guard()||!BP)return false;if(BP->bIsRegeneratingOnLoad||BP->bBeingCompiled)return Fail(TEXT("controlrig_recipe_compile_pending"),TEXT("Current Blueprint compile/regeneration is active; no forced wait/replay"));return true;}
};
FWork* Current=nullptr;

// Domain-specific bridge around the original build loops. Every invocation admits
// current context first; plain SDK return values are recorded before post-admission.
UControlRigBlueprint* LoadTarget(const FString& Path)
{
 auto& W=*Current;if(!W.Guard())return nullptr;W.TargetPackage=FPackageName::ObjectPathToPackageName(Path);if(W.TargetPackage.IsEmpty())W.TargetPackage=Path;W.TargetObject=MakeObjectPathFromPackagePath(W.TargetPackage);W.TargetFile=FPackageName::LongPackageNameToFilename(W.TargetPackage,FPackageName::GetAssetPackageExtension());
 if(!TextSafe(W.TargetObject,NAME_SIZE-1)){W.Fail(TEXT("controlrig_recipe_expanded_path_bound"),TEXT("Expanded target object path exceeds NAME_SIZE before any lookup/SDK"));return nullptr;}
 const bool Exists=FindObject<UObject>(nullptr,*W.TargetObject)||FPackageName::DoesPackageExist(W.TargetPackage);
 if(!Exists)return nullptr;UObject* O=nullptr;if(!W.Call(TEXT("load_existing_controlrig"),false,[&]{O=StaticLoadObject(UControlRigBlueprint::StaticClass(),nullptr,*W.TargetObject);W.ActiveCall->SetBoolField(TEXT("nonnull_return"),O!=nullptr);}))return nullptr;
 auto* B=Cast<UControlRigBlueprint>(O);if(!B||!W.Bind(B))return nullptr;return B;
}
bool FreshTarget()
{
 auto& W=*Current;if(!W.Guard())return false;bool Exists=false;FAssetRegistryModule* Registry=nullptr;
 if(!W.Call(TEXT("asset_registry_module"),false,[&]{Registry=&FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return false;
 if(!W.Call(TEXT("target_collision_registry"),false,[&]{TArray<FAssetData> Rows;Registry->Get().GetAssetsByPackageName(FName(*W.TargetPackage),Rows,false);Exists=!Rows.IsEmpty();}))return false;
 if(Exists||FindObject<UObject>(nullptr,*W.TargetObject)||FindPackage(nullptr,*W.TargetPackage)||FPackageName::DoesPackageExist(W.TargetPackage)||IFileManager::Get().FileExists(*W.TargetFile))return W.Fail(TEXT("controlrig_recipe_target_collision"),TEXT("Target object/package/file already exists; no overwrite or replay"));return true;
}
UControlRigBlueprint* CreateTarget(const FString&,bool Modular)
{
 auto& W=*Current;if(!W.R.ErrorCode.IsEmpty()||!FreshTarget())return nullptr;UControlRigBlueprint* O=nullptr;W.R.Data->SetBoolField(TEXT("factory_internal_compile_possible"),true);
 if(!W.Call(TEXT("create_controlrig_factory"),true,[&]{O=UControlRigBlueprintFactory::CreateNewControlRigAsset(W.TargetPackage,Modular);W.ActiveCall->SetBoolField(TEXT("nonnull_return"),O!=nullptr);W.R.Data->SetBoolField(TEXT("factory_nonnull_return_observed"),O!=nullptr);}))return nullptr;
 if(!O||!W.Bind(O))return nullptr;W.R.bChanged=true;W.Report.bCreated=true;W.Report.bTargetModified=true;return O;
}
USkeletalMesh* LoadMesh(const FString& Path)
{
 auto& W=*Current;if(!W.Guard())return nullptr;if(!TextSafe(Path,NAME_SIZE-1)){W.Fail(TEXT("controlrig_recipe_expanded_path_bound"),TEXT("Expanded mesh object path exceeds NAME_SIZE before SDK"));return nullptr;}USkeletalMesh* M=nullptr;if(!W.Call(TEXT("load_preview_mesh"),false,[&]{M=Cast<USkeletalMesh>(StaticLoadObject(USkeletalMesh::StaticClass(),nullptr,*Path));W.ActiveCall->SetBoolField(TEXT("nonnull_return"),M!=nullptr);}))return nullptr;
 if(M&&(!W.Pin(M)||M->IsCompiling())){W.Fail(TEXT("controlrig_recipe_mesh_pending"),TEXT("Pending/unavailable mesh reference snapshot is unsupported before getter"));return nullptr;}return M;
}
USkeletalMesh* PreviewMesh(UControlRigBlueprint*)
{
 auto& W=*Current;if(!W.Guard())return nullptr;const auto Ref=W.BP->GetPreviewSkeletalMesh();if(!Ref.IsNull()&&!Ref.Get()){W.Fail(TEXT("controlrig_recipe_preview_unresolved"),TEXT("Non-null unresolved preview reference requires an explicit supported load"));return nullptr;}
 USkeletalMesh* M=nullptr;if(!W.Call(TEXT("get_preview_mesh"),false,[&]{M=UControlRigBlueprintEditorLibrary::GetPreviewMesh(W.BP);}))return nullptr;if(M&&!W.Pin(M))return nullptr;W.ExpectedPreview=M;W.PreviewCaptured=true;return M;
}
void SetPreviewMesh(UControlRigBlueprint*,USkeletalMesh* Mesh,bool Dirty)
{
 auto& W=*Current;if(!W.Guard()||!W.Pin(Mesh))return;W.PreviewCaptured=false;
 if(!W.Call(TEXT("set_preview_mesh"),true,[&]{UControlRigBlueprintEditorLibrary::SetPreviewMesh(W.BP,Mesh,Dirty);}))return;const auto Ref=W.BP->GetPreviewSkeletalMesh();if(Ref.Get()!=Mesh){W.Fail(TEXT("controlrig_recipe_preview_readback_failed"),TEXT("Preview setter current exact reference differs"));return;}W.ExpectedPreview=Mesh;W.PreviewCaptured=true;W.R.bChanged=true;W.Report.bTargetModified=true;
}
FReferenceSkeleton CopyReference(USkeletalMesh* Mesh)
{
 auto& W=*Current;FReferenceSkeleton Copy;if(!W.Guard()||!W.Pin(Mesh)||Mesh->IsCompiling()){W.Fail(TEXT("controlrig_recipe_reference_pending"),TEXT("Mesh reference snapshot unavailable/pending"));return Copy;}
 W.Call(TEXT("copy_mesh_reference_skeleton"),false,[&]{Copy=Mesh->GetRefSkeleton();});if(Copy.GetNum()>65536){W.Fail(TEXT("controlrig_recipe_reference_budget"),TEXT("Reference skeleton capture exceeds cap"));return FReferenceSkeleton();}return Copy;
}
struct FHierarchyFacade
{
 URigHierarchyController* Owner=nullptr;
 bool Admit(){auto& W=*Current;return W.Guard()&&W.HC==Owner&&Live(Owner)&&Owner->GetHierarchy()==W.Hierarchy&&Owner->IsIn(W.Hierarchy);}
 template<class F>FRigElementKey Add(const TCHAR* Stage,const FName& Requested,ERigElementType Type,F&& Fn)
 {
  auto& W=*Current;FRigElementKey Key;FString Before;if(!Admit()||!W.SnapshotHierarchy(Before))return Key;const auto BeforeKeys=W.Hierarchy->GetAllKeys();
  if(!W.Call(Stage,true,[&]{Key=Fn();W.ActiveCall->SetStringField(TEXT("requested_name"),Requested.ToString());W.ActiveCall->SetStringField(TEXT("returned_key"),Key.ToString());if(Key.IsValid()){W.R.bChanged=true;W.Report.bTargetModified=true;++W.Report.CreatedHierarchyElements;}}))return FRigElementKey();
  if(!Admit()||!Key.IsValid()||Key.Type!=Type||!W.Hierarchy->Contains(Key)){W.Fail(TEXT("controlrig_recipe_hierarchy_readback_failed"),TEXT("SDK-created hierarchy key is not currently owned"));return FRigElementKey();}
  const auto AfterKeys=W.Hierarchy->GetAllKeys();if(AfterKeys.Num()!=BeforeKeys.Num()+1||AfterKeys.Last()!=Key){W.Fail(TEXT("controlrig_recipe_hierarchy_transition_unexpected"),TEXT("Expected exactly one appended hierarchy key"));return FRigElementKey();}for(int32 I=0;I<BeforeKeys.Num();++I)if(AfterKeys[I]!=BeforeKeys[I]){W.Fail(TEXT("controlrig_recipe_hierarchy_transition_unexpected"),TEXT("Existing physical hierarchy keys changed"));return FRigElementKey();}
  TSet<FRigElementKey> Previous;for(const auto& Existing:BeforeKeys)Previous.Add(Existing);FString AfterPrevious;if(!W.SnapshotHierarchy(AfterPrevious,&Previous)||!Same(Before,AfterPrevious)){W.Fail(TEXT("controlrig_recipe_existing_hierarchy_changed"),TEXT("Existing hierarchy values changed during append SDK"));return FRigElementKey();}if(!W.SnapshotHierarchy(W.LastHierarchy))return FRigElementKey();return Key;
 }
 FRigElementKey AddCurve(FName Name,float Value,bool Undo,bool Print){auto& W=*Current;auto K=Add(TEXT("add_curve"),Name,ERigElementType::Curve,[&]{return Owner->AddCurve(Name,Value,Undo,Print);});if(K.IsValid()&&W.Hierarchy->GetCurveValue(K)!=Value){W.Fail(TEXT("controlrig_recipe_curve_value_mismatch"),TEXT("Current curve value differs from requested typed value"));return FRigElementKey();}return K;}
 FRigElementKey AddNull(FName Name,FRigElementKey Parent,FTransform Transform,bool Global,bool Undo,bool Print){auto& W=*Current;auto K=Add(TEXT("add_null"),Name,ERigElementType::Null,[&]{return Owner->AddNull(Name,Parent,Transform,Global,Undo,Print);});if(K.IsValid()&&(W.Hierarchy->GetFirstParent(K)!=Parent||!(Global?W.Hierarchy->GetGlobalTransform(K,true):W.Hierarchy->GetLocalTransform(K,true)).Equals(Transform))){W.Fail(TEXT("controlrig_recipe_null_transform_mismatch"),TEXT("Current null parent/initial transform differs"));return FRigElementKey();}return K;}
 FRigElementKey AddControl(FName Name,FRigElementKey Parent,const FRigControlSettings& Settings,const FRigControlValue& Value,const FTransform& Offset,const FTransform& Shape,bool Undo,bool Print)
 {
  auto& W=*Current;auto K=Add(TEXT("add_control"),Name,ERigElementType::Control,[&]{return Owner->AddControl(Name,Parent,Settings,Value,Offset,Shape,Undo,Print);});if(!K.IsValid())return K;auto* E=W.Hierarchy->Find<FRigControlElement>(K);
  if(!E||W.Hierarchy->GetFirstParent(K)!=Parent||E->Settings.ControlType!=Settings.ControlType||E->Settings.PrimaryAxis!=Settings.PrimaryAxis||E->Settings.ShapeName!=Settings.ShapeName||E->Settings.ShapeColor!=Settings.ShapeColor||E->Settings.bShapeVisible!=Settings.bShapeVisible||!W.Hierarchy->GetControlOffsetTransform(E,ERigTransformType::InitialLocal).Equals(Offset)||!W.Hierarchy->GetControlShapeTransform(const_cast<FRigControlElement*>(E),ERigTransformType::InitialLocal).Equals(Shape)||!Same(ControlValueText(W.Hierarchy->GetControlValue(K,ERigControlValueType::Initial),E->Settings),ControlValueText(Value,Settings))){W.Fail(TEXT("controlrig_recipe_control_value_mismatch"),TEXT("Current control settings/parent/typed initial value differ"));return FRigElementKey();}return K;
 }
 TArray<FRigElementKey> ImportBones(const TArray<FMeshBoneInfo>& Infos,const TArray<FTransform>& Poses,FName Namespace,bool Replace,bool Remove,bool Select,bool Undo)
 {
  auto& W=*Current;TArray<FRigElementKey> Keys;if(!Admit()||Infos.Num()!=Poses.Num()||Infos.Num()>MaxElements)return Keys;for(int32 I=0;I<Infos.Num();++I)if(Infos[I].ParentIndex>=I||Infos[I].ParentIndex<INDEX_NONE||Poses[I].ContainsNaN()){W.Fail(TEXT("controlrig_recipe_reference_malformed"),TEXT("Reference parent/transform must be finite and ordered"));return Keys;}
  if(!W.Call(TEXT("import_preview_bones"),true,[&]{Keys=Owner->ImportBones(Infos,Poses,Namespace,Replace,Remove,Select,Undo);W.ActiveCall->SetNumberField(TEXT("returned_key_count"),Keys.Num());if(!Keys.IsEmpty()){W.R.bChanged=true;W.Report.bTargetModified=true;}}))return {};
  if(!Admit())return {};for(const auto& K:Keys)if(!K.IsValid()||K.Type!=ERigElementType::Bone||!W.Hierarchy->Contains(K)){W.Fail(TEXT("controlrig_recipe_import_readback_failed"),TEXT("Imported exact bone key is not in current hierarchy"));return {};}
  if(!W.SnapshotHierarchy(W.LastHierarchy))return {};return Keys;
 }
};
FHierarchyFacade HierarchyFacade;
FHierarchyFacade* GetHierarchy(UControlRigBlueprint*)
{
 auto& W=*Current;if(!W.Guard())return nullptr;URigHierarchyController* O=nullptr;if(!W.Call(TEXT("get_hierarchy_controller"),false,[&]{O=W.BP->GetHierarchyController();}))return nullptr;if(!O||!W.Pin(O)||O->GetHierarchy()!=W.Hierarchy||!O->IsIn(W.Hierarchy)){W.Fail(TEXT("controlrig_recipe_controller_owner_invalid"),TEXT("Exact hierarchy controller owner is required"));return nullptr;}W.HC=O;HierarchyFacade.Owner=O;if(!W.SnapshotHierarchy(W.LastHierarchy))return nullptr;return &HierarchyFacade;
}
struct FVMFacade
{
 URigVMController* Owner=nullptr;
 bool Admit(){auto& W=*Current;return W.GraphSafe()&&W.VC==Owner&&Live(Owner)&&Owner->GetGraph()==W.Graph&&Owner->IsIn(W.BP);}
 URigVMUnitNode* AddUnitNode(UScriptStruct* Struct,FName Method,FVector2D Position,FString Name,bool Undo,bool Print)
 {
  auto& W=*Current;if(!Admit()||!W.Pin(Struct))return nullptr;const auto BeforeNodes=W.Graph->GetNodes();URigVMUnitNode* Out=nullptr;
  if(!W.Call(TEXT("add_unit_node"),true,[&]{Out=Owner->AddUnitNode(Struct,Method,Position,Name,Undo,Print);W.ActiveCall->SetBoolField(TEXT("nonnull_return"),Out!=nullptr);}))return nullptr;
  UScriptStruct* ActualStruct=nullptr;if(!Admit()||!Live(Out)||!W.Graph->GetNodes().Contains(Out)||!W.Pin(Out)||!W.Call(TEXT("get_new_unit_struct"),false,[&]{ActualStruct=Out->GetScriptStruct();})||ActualStruct!=Struct||Out->GetGraph()!=W.Graph){W.Fail(TEXT("controlrig_recipe_unit_readback_failed"),TEXT("Current exact typed unit node was not returned in owned graph"));return nullptr;}
  const auto AfterNodes=W.Graph->GetNodes();if(AfterNodes.Num()!=BeforeNodes.Num()+1){W.Fail(TEXT("controlrig_recipe_unit_transition_unexpected"),TEXT("Expected one new unit node"));return nullptr;}for(int32 I=0;I<BeforeNodes.Num();++I)if(AfterNodes[I]!=BeforeNodes[I]){W.Fail(TEXT("controlrig_recipe_unit_transition_unexpected"),TEXT("Existing node membership/order changed"));return nullptr;}W.R.bChanged=true;W.Report.bTargetModified=true;++W.Report.CreatedRigVMNodes;if(!W.GraphSignature(W.LastGraph))return nullptr;return Out;
 }
 bool SetPinDefaultValue(const FString& Path,const FString& Value,bool Resize,bool Undo,bool Merge,bool Print,bool Linked)
 {
  auto& W=*Current;if(!Admit())return false;auto* P=W.Graph->FindPin(Path);if(!Live(P)||!P->IsIn(W.Graph))return W.Fail(TEXT("controlrig_recipe_pin_missing"),TEXT("Current owned default pin is required"));bool Accepted=false;
  if(!W.Call(TEXT("set_pin_default"),true,[&]{Accepted=Owner->SetPinDefaultValue(Path,Value,Resize,Undo,Merge,Print,Linked);W.ActiveCall->SetBoolField(TEXT("accepted"),Accepted);if(Accepted){W.R.bChanged=true;W.Report.bTargetModified=true;++W.Report.AppliedPinDefaults;}}))return false;
  if(!Admit()||!Accepted)return false;P=W.Graph->FindPin(Path);if(!Live(P)||!P->IsIn(W.Graph)||!Same(P->GetDefaultValue(),Value))return W.Fail(TEXT("controlrig_recipe_pin_default_readback_failed"),TEXT("Reacquired typed pin default differs from requested literal"));return W.GraphSignature(W.LastGraph);
 }
 bool AddLink(URigVMPin* A,URigVMPin* B,bool Undo,ERigVMPinDirection Direction,bool Cast,bool Restore,FString* Failure)
 {
  auto& W=*Current;if(!Admit()||!Live(A)||!Live(B)||!A->IsIn(W.Graph)||!B->IsIn(W.Graph))return false;const FString AP=A->GetPinPath(),BP=B->GetPinPath();bool Accepted=false;
  if(!W.Call(TEXT("add_rigvm_link"),true,[&]{Accepted=Owner->AddLink(A,B,Undo,Direction,Cast,Restore,Failure);W.ActiveCall->SetBoolField(TEXT("accepted"),Accepted);if(Accepted){W.R.bChanged=true;W.Report.bTargetModified=true;++W.Report.AppliedLinks;}}))return false;
  if(!Admit()||!Accepted)return false;auto* NewA=W.Graph->FindPin(AP);auto* NewB=W.Graph->FindPin(BP);if(!Live(NewA)||!Live(NewB)||!NewA->GetLinkedTargetPins().Contains(NewB)||!NewB->GetLinkedSourcePins().Contains(NewA))return W.Fail(TEXT("controlrig_recipe_link_readback_failed"),TEXT("Current reciprocal link endpoints differ"));return W.GraphSignature(W.LastGraph);
 }
};
FVMFacade VMFacade;
URigVMGraph* GetModel(UControlRigBlueprint*){auto& W=*Current;if(!W.Guard())return nullptr;URigVMGraph* G=nullptr;if(!W.Call(TEXT("get_default_model"),false,[&]{G=W.BP->GetDefaultModel();}))return nullptr;if(!Live(G)||!W.Pin(G)||!G->IsIn(W.BP)){W.Fail(TEXT("controlrig_recipe_model_owner_invalid"),TEXT("Exact owned default model required"));return nullptr;}W.Graph=G;return W.GraphSafe()?G:nullptr;}
FVMFacade* GetVMController(UControlRigBlueprint*,URigVMGraph* G){auto& W=*Current;if(!W.GraphSafe()||G!=W.Graph)return nullptr;URigVMController* C=nullptr;if(!W.Call(TEXT("get_rigvm_controller"),false,[&]{C=W.BP->GetOrCreateController(G);}))return nullptr;if(!Live(C)||!W.Pin(C)||C->GetGraph()!=G||!C->IsIn(W.BP)){W.Fail(TEXT("controlrig_recipe_vm_controller_owner_invalid"),TEXT("Exact current RigVM controller required"));return nullptr;}W.VC=C;VMFacade.Owner=C;if(!W.GraphSignature(W.LastGraph))return nullptr;return &VMFacade;}
URigVMUnitNode* FindUnitNodeByScriptStruct(URigVMGraph* G,UScriptStruct* Struct){auto& W=*Current;if(!W.GraphSafe()||G!=W.Graph||!W.Pin(Struct))return nullptr;for(auto* N:G->GetNodes())if(auto* U=Cast<URigVMUnitNode>(N)){UScriptStruct* S=nullptr;if(!W.Call(TEXT("get_unit_script_struct"),false,[&]{S=U->GetScriptStruct();}))return nullptr;if(S==Struct)return U;}return nullptr;}
bool ScanPackageForForbiddenReferences(UPackage* P,FHHControlRigBuildReport& Report)
{
 auto& W=*Current;if(!W.PackageOwned()||P!=W.Package)return false;bool Pass=true;TArray<UObject*> All;GetObjectsWithPackage(P,All,EGetObjectsFlags::IncludeNestedObjects);for(auto* O:All)if(ContainsForbiddenMcpReference(O->GetPathName())||ContainsForbiddenMcpReference(O->GetClass()->GetPathName())){AddIssue(Report,EHHControlRigIssueSeverity::Error,TEXT("forbidden_mcp_object_reference"),TEXT("Target object/class contains an authoring-only reference"),TEXT("dependency_audit.objects"));Pass=false;}
 FAssetRegistryModule* Registry=nullptr;if(!W.Call(TEXT("dependency_registry_module"),false,[&]{Registry=&FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));}))return false;TArray<FName> Dependencies;bool Query=false;
 if(!W.Call(TEXT("dependency_registry_query"),false,[&]{Query=Registry->Get().GetDependencies(FName(*W.TargetPackage),Dependencies);}))return false;W.R.Data->SetBoolField(TEXT("dependency_registry_query_returned"),Query);W.R.Data->SetBoolField(TEXT("dependency_audit_full_serialized_transitive"),false);for(const auto& D:Dependencies)if(ContainsForbiddenMcpReference(D.ToString())){AddIssue(Report,EHHControlRigIssueSeverity::Error,TEXT("forbidden_mcp_package_dependency"),TEXT("Direct package dependency contains authoring-only reference"),TEXT("dependency_audit.package_dependencies"));Pass=false;}return Pass&&W.PackageOwned();
}
void Propagate(UControlRigBlueprint*){auto& W=*Current;W.Call(TEXT("propagate_hierarchy"),true,[&]{static_cast<IControlRigEditorAssetInterface*>(W.BP)->PropagateHierarchyFromBPToInstances();});}
void AutoRecompile(UControlRigBlueprint*){auto& W=*Current;W.Call(TEXT("request_auto_vm_recompilation"),true,[&]{W.BP->RequestAutoVMRecompilation();});}
void MarkDirty(UControlRigBlueprint*){auto& W=*Current;W.Call(TEXT("mark_package_dirty"),true,[&]{W.BP->MarkPackageDirty();});}
void RequestInit(UControlRigBlueprint*){auto& W=*Current;W.Call(TEXT("request_controlrig_init"),true,[&]{UControlRigBlueprintEditorLibrary::RequestControlRigInit(W.BP);});}
void Compile(UControlRigBlueprint*)
{
 auto& W=*Current;if(!W.Idle()||!W.PackageOwned())return;W.CompileTransition=true;FCompilerResultsLog Log;
 const bool Returned=W.Call(TEXT("compile_controlrig_blueprint"),true,[&]{W.R.bCompileAttempted=true;W.R.Compiled.Reset();FKismetEditorUtilities::CompileBlueprint(W.BP,EBlueprintCompileOptions::None,&Log);W.ActiveCall->SetNumberField(TEXT("errors"),Log.NumErrors);W.ActiveCall->SetNumberField(TEXT("warnings"),Log.NumWarnings);});
 if(!Returned)return;auto* G=W.BP->GeneratedClass.Get();auto* S=W.BP->SkeletonGeneratedClass.Get();if(!Live(G)||G->ClassGeneratedBy!=W.BP||!G->IsChildOf(UControlRig::StaticClass())||G->GetPackage()!=W.Package||S&&(!Live(S)||S->ClassGeneratedBy!=W.BP||S->GetPackage()!=W.Package)){W.Fail(TEXT("controlrig_recipe_compiled_class_invalid"),TEXT("Compiled current class must be stock ControlRig-derived and generated by exact Blueprint"));return;}W.Generated.Reset(G);W.SkeletonClass.Reset(S);W.CompileTransition=false;
 if(!W.Guard()||!W.Idle())return;W.R.Compiled=Log.NumErrors==0&&(W.BP->Status==BS_UpToDate||W.BP->Status==BS_UpToDateWithWarnings);W.R.Data->SetStringField(TEXT("actual_compile_scope"),TEXT("Blueprint synchronous compiler return/current status only; no solver execution or GPU proof"));if(!W.R.Compiled.GetValue())W.Fail(TEXT("controlrig_recipe_compile_failed"),TEXT("Actual Blueprint compile status/errors did not pass"));
}
UClass* CurrentRigClass(UControlRigBlueprint*){auto& W=*Current;if(!W.Guard())return nullptr;UClass* C=nullptr;if(!W.Call(TEXT("get_current_controlrig_class"),false,[&]{const auto Ref=static_cast<IControlRigEditorAssetInterface*>(W.BP)->GetControlRigAssetReference();C=Ref.GetBlueprintClass().Get();}))return nullptr;if(C&&(!Live(C)||C!=W.BP->GeneratedClass.Get()||C->ClassGeneratedBy!=W.BP||!C->IsChildOf(UControlRig::StaticClass()))){W.Fail(TEXT("controlrig_recipe_current_class_invalid"),TEXT("Current asset reference/class is not the exact generated ControlRig class"));return nullptr;}return C;}
UControlRig* CreateTransientRig(UControlRigBlueprint*){auto& W=*Current;if(!W.Guard())return nullptr;UControlRig* R=nullptr;if(!W.Call(TEXT("create_transient_controlrig"),true,[&]{R=static_cast<IControlRigEditorAssetInterface*>(W.BP)->CreateControlRig();W.ActiveCall->SetBoolField(TEXT("nonnull_return"),R!=nullptr);}))return nullptr;if(R&&!W.Pin(R))return nullptr;W.R.Data->SetBoolField(TEXT("transient_rig_creation_is_direct_solve_execution"),false);return R;}
bool SaveTarget(const TArray<UPackage*>& Packages,bool OnlyDirty)
{
 auto& W=*Current;if(Packages.Num()!=1||Packages[0]!=W.Package||!W.Idle()||!W.PackageOwned())return false;bool Saved=false;const bool BeforeDirty=W.Package->IsDirty();
 if(!W.Call(TEXT("save_controlrig_package"),true,[&]{W.R.bSaveAttempted=true;Saved=UEditorLoadingAndSavingUtils::SavePackages(Packages,OnlyDirty);W.R.bSaved|=Saved;W.Report.bSaveSucceeded|=Saved;W.ActiveCall->SetBoolField(TEXT("saved_API_true"),Saved);W.R.Data->SetBoolField(TEXT("physical_file_write_proven"),false);W.R.Data->SetBoolField(TEXT("save_only_if_dirty"),OnlyDirty);W.R.Data->SetBoolField(TEXT("clean_package_vacuous_success"),Saved&&OnlyDirty&&!BeforeDirty);}))return false;
 if(!W.PackageOwned())return false;W.R.Data->SetBoolField(TEXT("target_dirty_after_save"),W.Package->IsDirty());if(Saved&&W.Package->IsDirty())return W.Fail(TEXT("controlrig_recipe_save_dirty_after_return"),TEXT("Save API true was returned but target package remains dirty; history preserved"));return Saved;
}

UPackage* OwnedPackage(UControlRigBlueprint*){return Current->Guard()?Current->Package:nullptr;}
FHHControlRigBuildReport BuildRecipeWork(
	const FHHControlRigRecipe& Recipe,
	const FHHControlRigBuildOptions& Options)
{

	FHHControlRigBuildOptions DryRunOptions = Options;
	DryRunOptions.bDryRun = false;

	Current->Report=ValidateRecipe(Recipe,DryRunOptions);
	auto& Report=Current->Report;
	Report.bDryRun = false;
	if (HasError(Report))
	{
		MarkNoMutationStop(
			Report,
			TEXT("BuildRecipe validation failed. No mutation occurred."),
			TEXT("fix_recipe_and_rerun_validate_controlrig_recipe"));
		return Report;
	}

	if (Recipe.AnimBPBinding.bEnabled)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("animbp_binding_requires_external_orchestration"),
			TEXT("BuildRecipe does not mutate AnimBP bindings yet. Build the ControlRig with binding disabled, then run ensure_anim_graph_controlrig_node_sample and set_anim_graph_controlrig_input_defaults on the validated sample target."),
			TEXT("animbp_binding.enabled"));
		MarkNoMutationStop(
			Report,
			TEXT("BuildRecipe blocked before mutation because AnimBP binding requires external orchestration in this phase."),
			TEXT("run_build_controlrig_recipe_with_animbp_binding_for_sample_temp_target"));
		return Report;
	}

	UControlRigBlueprint* TargetControlRigBlueprint=LoadTarget(Recipe.TargetControlRigPath);
	if (TargetControlRigBlueprint)
	{
		Report.bReused = true;
		AddReportEntry(Report.ReusedItems, FormatReportEntry(TEXT("target_control_rig"), Recipe.TargetControlRigPath));
		AddReportEntry(Report.SkippedWrites, FormatReportEntry(TEXT("create_control_rig_existing_target"), Recipe.TargetControlRigPath));
	}
	else
	{
		TargetControlRigBlueprint = CreateTarget(Recipe.TargetControlRigPath,false);
		if (!TargetControlRigBlueprint)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("create_control_rig_failed"),
				FString::Printf(TEXT("Failed to create ControlRig Blueprint at %s."), *Recipe.TargetControlRigPath),
				TEXT("target_control_rig_path"));
			MarkBuildFailure(Report, TEXT("ControlRig asset creation failed."));
			return Report;
		}

		Report.bCreated = true;
		Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("create_control_rig"), Recipe.TargetControlRigPath));
	}

	USkeletalMesh* PreviewSkeletalMesh = nullptr;
	if (!Recipe.PreviewSkeletalMesh.IsEmpty())
	{
		PreviewSkeletalMesh=LoadMesh(Recipe.PreviewSkeletalMesh);
		if (!PreviewSkeletalMesh && FPackageName::IsValidLongPackageName(Recipe.PreviewSkeletalMesh))
		{
			PreviewSkeletalMesh=LoadMesh(MakeObjectPathFromPackagePath(Recipe.PreviewSkeletalMesh));
		}
		if (!PreviewSkeletalMesh)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("preview_mesh_load_failed"),
				FString::Printf(TEXT("Failed to load preview_skeletal_mesh: %s."), *Recipe.PreviewSkeletalMesh),
				TEXT("preview_skeletal_mesh"));
			MarkBuildFailure(Report, TEXT("Preview mesh load failed after target creation/reuse."));
			return Report;
		}

		if (PreviewMesh(TargetControlRigBlueprint) != PreviewSkeletalMesh)
		{
			SetPreviewMesh(TargetControlRigBlueprint, PreviewSkeletalMesh, true);
			Report.bTargetModified = true;
			AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("set_preview_mesh"), Recipe.PreviewSkeletalMesh));
		}
		else
		{
			AddReportEntry(Report.ReusedItems, FormatReportEntry(TEXT("preview_mesh"), Recipe.PreviewSkeletalMesh));
			AddReportEntry(Report.SkippedWrites, FormatReportEntry(TEXT("set_preview_mesh_already_current"), Recipe.PreviewSkeletalMesh));
		}
	}

	FHierarchyFacade* HierarchyController=GetHierarchy(TargetControlRigBlueprint);
	if (!HierarchyController)
	{
		AddIssue(
			Report,
			EHHControlRigIssueSeverity::Error,
			TEXT("missing_hierarchy_controller"),
			TEXT("ControlRig Blueprint has no hierarchy controller."),
			TEXT("hierarchy"));
		MarkBuildFailure(Report, TEXT("ControlRig hierarchy controller was unavailable."));
		return Report;
	}

	TMap<FString, FRigElementKey> AuthoredParentKeys;

	if (Recipe.Hierarchy.bImportPreviewBones)
	{
		if (!PreviewSkeletalMesh)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("preview_mesh_required_for_bone_import"),
				TEXT("preview_skeletal_mesh must resolve before importing preview bones."),
				TEXT("preview_skeletal_mesh"));
			MarkBuildFailure(Report, TEXT("Preview mesh was required for bone import after target preparation."));
			return Report;
		}

		const FReferenceSkeleton RefSkeleton=CopyReference(PreviewSkeletalMesh);
		const int32 RefBoneCount = RefSkeleton.GetNum();
		const int32 ImportBoneCount = FMath::Min(RefBoneCount, Recipe.Hierarchy.MaxImportBones);
		if (ImportBoneCount <= 0)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("preview_mesh_has_no_bones"),
				FString::Printf(TEXT("preview_skeletal_mesh has no importable reference bones: %s."), *Recipe.PreviewSkeletalMesh),
				TEXT("preview_skeletal_mesh"));
			MarkBuildFailure(Report, TEXT("Preview mesh had no importable bones after target preparation."));
			return Report;
		}

		TArray<FMeshBoneInfo> BoneInfos;
		TArray<FTransform> BoneTransforms;
		BoneInfos.Reserve(ImportBoneCount);
		BoneTransforms.Reserve(ImportBoneCount);
		for (int32 BoneIndex = 0; BoneIndex < ImportBoneCount; ++BoneIndex)
		{
			BoneInfos.Add(RefSkeleton.GetRefBoneInfo()[BoneIndex]);
			BoneTransforms.Add(RefSkeleton.GetRefBonePose()[BoneIndex]);
		}

		const TArray<FRigElementKey> ImportedBoneKeys = HierarchyController->ImportBones(
			BoneInfos,
			BoneTransforms,
			NAME_None,
			false,
			false,
			false,
			false);
		if (ImportedBoneKeys.IsEmpty())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("import_preview_bones_failed"),
				FString::Printf(TEXT("Failed to import preview bones from %s."), *Recipe.PreviewSkeletalMesh),
				TEXT("hierarchy.import_preview_bones"));
			MarkBuildFailure(Report, TEXT("Preview bone import failed after target preparation."));
			return Report;
		}

		for (const FRigElementKey& BoneKey : ImportedBoneKeys)
		{
			if (BoneKey.IsValid())
			{
				AuthoredParentKeys.Add(BoneKey.Name.ToString(), BoneKey);
                ++Report.CreatedHierarchyElements;
							}
		}
		Report.bTargetModified = true;
		AddReportEntry(
			Report.PerformedWrites,
			FString::Printf(TEXT("import_preview_bones:%d"), ImportedBoneKeys.Num()));
	}

	for (const FString& CurveName : Recipe.Hierarchy.Curves)
	{
		const FRigElementKey CurveKey = HierarchyController->AddCurve(FName(*CurveName), 0.0f, false, false);
		if (!CurveKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_curve_failed"),
				FString::Printf(TEXT("Failed to add curve: %s."), *CurveName),
				TEXT("hierarchy.curves"));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add curve after partial work: %s."), *CurveName));
			return Report;
		}
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_curve"), CurveName.TrimStartAndEnd()));
	}

	for (int32 Index = 0; Index < Recipe.Hierarchy.CurveSpecs.Num(); ++Index)
	{
		const FHHControlRigCurveSpec& CurveSpec = Recipe.Hierarchy.CurveSpecs[Index];
		const FString CurveName = CurveSpec.Name.TrimStartAndEnd();
		const FRigElementKey CurveKey = HierarchyController->AddCurve(FName(*CurveName), CurveSpec.Value, false, false);
		if (!CurveKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_curve_failed"),
				FString::Printf(TEXT("Failed to add curve: %s."), *CurveName),
				FString::Printf(TEXT("hierarchy.curve_specs[%d]"), Index));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add curve after partial work: %s."), *CurveName));
			return Report;
		}
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_curve"), CurveName));
	}

	for (const FString& NullName : Recipe.Hierarchy.Nulls)
	{
		const FString TrimmedNullName = NullName.TrimStartAndEnd();
		const FRigElementKey NullKey = HierarchyController->AddNull(FName(*TrimmedNullName), FRigElementKey(), FTransform::Identity, true, false, false);
		if (!NullKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_null_failed"),
				FString::Printf(TEXT("Failed to add null: %s."), *TrimmedNullName),
				TEXT("hierarchy.nulls"));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add null after partial work: %s."), *TrimmedNullName));
			return Report;
		}
		AuthoredParentKeys.Add(TrimmedNullName, NullKey);
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_null"), TrimmedNullName));
	}

	for (int32 Index = 0; Index < Recipe.Hierarchy.NullSpecs.Num(); ++Index)
	{
		const FHHControlRigNullSpec& NullSpec = Recipe.Hierarchy.NullSpecs[Index];
		const FString NullName = NullSpec.Name.TrimStartAndEnd();
		FRigElementKey ParentKey;
		if (!ResolveAuthoredParentKey(
				NullSpec.Parent,
				AuthoredParentKeys,
				ParentKey,
				Report,
				FString::Printf(TEXT("hierarchy.null_specs[%d].parent"), Index)))
		{
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to resolve null parent after partial work: %s."), *NullName));
			return Report;
		}

		const FRigElementKey NullKey = HierarchyController->AddNull(
			FName(*NullName),
			ParentKey,
			ToTransform(NullSpec.Transform),
			NullSpec.Transform.bTransformInGlobal,
			false,
			false);
		if (!NullKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_null_failed"),
				FString::Printf(TEXT("Failed to add null: %s."), *NullName),
				FString::Printf(TEXT("hierarchy.null_specs[%d]"), Index));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add null after partial work: %s."), *NullName));
			return Report;
		}
		AuthoredParentKeys.Add(NullName, NullKey);
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_null"), NullName));
	}

	for (const FString& ControlName : Recipe.Hierarchy.Controls)
	{
		const FString TrimmedControlName = ControlName.TrimStartAndEnd();
		FHHControlRigControlSpec DefaultControlSpec;
		DefaultControlSpec.Name = TrimmedControlName;
		const FRigControlSettings Settings = MakeControlSettings(TrimmedControlName, DefaultControlSpec);
		const FRigControlValue ControlValue = MakeControlValue(DefaultControlSpec);
		const FRigElementKey ControlKey = HierarchyController->AddControl(
			FName(*TrimmedControlName),
			FRigElementKey(),
			Settings,
			ControlValue,
			FTransform::Identity,
			FTransform::Identity,
			false,
			false);
		if (!ControlKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_control_failed"),
				FString::Printf(TEXT("Failed to add default float control: %s."), *TrimmedControlName),
				TEXT("hierarchy.controls"));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add control after partial work: %s."), *TrimmedControlName));
			return Report;
		}
		AuthoredParentKeys.Add(TrimmedControlName, ControlKey);
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_control"), TrimmedControlName));
	}

	for (int32 Index = 0; Index < Recipe.Hierarchy.ControlSpecs.Num(); ++Index)
	{
		const FHHControlRigControlSpec& ControlSpec = Recipe.Hierarchy.ControlSpecs[Index];
		const FString ControlName = ControlSpec.Name.TrimStartAndEnd();
		FRigElementKey ParentKey;
		if (!ResolveAuthoredParentKey(
				ControlSpec.Parent,
				AuthoredParentKeys,
				ParentKey,
				Report,
				FString::Printf(TEXT("hierarchy.control_specs[%d].parent"), Index)))
		{
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to resolve control parent after partial work: %s."), *ControlName));
			return Report;
		}

		const FRigControlSettings Settings = MakeControlSettings(ControlName, ControlSpec);
		const FRigControlValue ControlValue = MakeControlValue(ControlSpec);
		const FRigElementKey ControlKey = HierarchyController->AddControl(
			FName(*ControlName),
			ParentKey,
			Settings,
			ControlValue,
			ToTransform(ControlSpec.OffsetTransform),
			ToTransform(ControlSpec.ShapeTransform),
			false,
			false);
		if (!ControlKey.IsValid())
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("add_control_failed"),
				FString::Printf(TEXT("Failed to add control: %s."), *ControlName),
				FString::Printf(TEXT("hierarchy.control_specs[%d]"), Index));
			MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add control after partial work: %s."), *ControlName));
			return Report;
		}
		AuthoredParentKeys.Add(ControlName, ControlKey);
				Report.bTargetModified = true;
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_control"), ControlName));
	}

	if (!Recipe.RigVM.Units.IsEmpty() || !Recipe.RigVM.PinDefaults.IsEmpty() || !Recipe.RigVM.Links.IsEmpty())
	{
		URigVMGraph* RigVMGraph=GetModel(TargetControlRigBlueprint);
		FVMFacade* RigVMController=RigVMGraph?GetVMController(TargetControlRigBlueprint,RigVMGraph):nullptr;
		if (!RigVMGraph || !RigVMController)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("missing_rigvm_controller"),
				TEXT("ControlRig Blueprint default RigVM model/controller was unavailable."),
				TEXT("rigvm"));
			MarkBuildFailure(Report, TEXT("ControlRig Blueprint default RigVM model/controller was unavailable."));
			return Report;
		}

		for (const FHHControlRigUnitSpec& Unit : Recipe.RigVM.Units)
		{
			const FString NormalizedStructName = NormalizeUnitStructName(Unit.StructName);
			UScriptStruct* ScriptStruct=nullptr;Current->Call(TEXT("resolve_allowlisted_unit"),false,[&]{ScriptStruct=GetAllowedUnitScriptStruct(NormalizedStructName);});
			if (!ScriptStruct)
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("unit_struct_not_resolved"),
					FString::Printf(TEXT("Failed to resolve allowlisted unit struct: %s."), *Unit.StructName),
					TEXT("rigvm.units"));
				MarkBuildFailure(Report, FString::Printf(TEXT("Failed to resolve allowlisted unit struct: %s."), *Unit.StructName));
				return Report;
			}

			if (FindUnitNodeByScriptStruct(RigVMGraph, ScriptStruct))
			{
				AddReportEntry(
					Report.ReusedItems,
					FormatReportEntry(TEXT("rigvm_unit"), NormalizedStructName));
				AddReportEntry(
					Report.SkippedWrites,
					FormatReportEntry(TEXT("add_rigvm_unit_existing_struct"), NormalizedStructName));
				continue;
			}

			const FString NodeName = Unit.NodeName.TrimStartAndEnd().IsEmpty()
				? DefaultUnitNodeName(NormalizedStructName)
				: Unit.NodeName.TrimStartAndEnd();
			URigVMUnitNode* UnitNode = RigVMController->AddUnitNode(
				ScriptStruct,
				FName(TEXT("Execute")),
				Unit.Position,
				NodeName,
				false,
				false);
			if (!UnitNode)
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("add_unit_node_failed"),
					FString::Printf(TEXT("Failed to add RigVM unit node: %s."), *Unit.StructName),
					TEXT("rigvm.units"));
				MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add RigVM unit node after partial work: %s."), *Unit.StructName));
				return Report;
			}

						Report.bTargetModified = true;
			AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("add_rigvm_unit"), NodeName));
		}

		for (const FHHControlRigPinDefaultSpec& PinDefault : Recipe.RigVM.PinDefaults)
		{
			if (!RigVMController->SetPinDefaultValue(
				PinDefault.PinPath,
				PinDefault.DefaultValue,
				PinDefault.bAllowArrayResize,
				false,
				false,
				false,
				PinDefault.bAllowLinkedPinPropagation))
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("set_pin_default_failed"),
					FString::Printf(TEXT("Failed to set RigVM pin default: %s."), *PinDefault.PinPath),
					TEXT("rigvm.pin_defaults"));
				MarkBuildFailure(Report, FString::Printf(TEXT("Failed to set RigVM pin default after partial work: %s."), *PinDefault.PinPath));
				return Report;
			}

						Report.bTargetModified = true;
			AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("set_pin_default"), PinDefault.PinPath));
		}

		for (const FHHControlRigLinkSpec& Link : Recipe.RigVM.Links)
		{
			URigVMPin* SourcePin = RigVMGraph->FindPin(Link.SourcePinPath);
			URigVMPin* TargetPin = RigVMGraph->FindPin(Link.TargetPinPath);
			if (!SourcePin || !TargetPin)
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("link_pin_not_found"),
					FString::Printf(TEXT("Failed to resolve RigVM link pins: %s -> %s."), *Link.SourcePinPath, *Link.TargetPinPath),
					TEXT("rigvm.links"));
				MarkBuildFailure(Report, FString::Printf(TEXT("Failed to resolve RigVM link pins after partial work: %s -> %s."), *Link.SourcePinPath, *Link.TargetPinPath));
				return Report;
			}

			FString FailureReason;
			if (!RigVMController->AddLink(
				SourcePin,
				TargetPin,
				false,
				ERigVMPinDirection::Invalid,
				Link.bAllowCastNodeCreation,
				false,
				&FailureReason))
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("add_link_failed"),
					FString::Printf(TEXT("Failed to add RigVM link: %s -> %s. %s"), *Link.SourcePinPath, *Link.TargetPinPath, *FailureReason),
					TEXT("rigvm.links"));
				MarkBuildFailure(Report, FString::Printf(TEXT("Failed to add RigVM link after partial work: %s -> %s."), *Link.SourcePinPath, *Link.TargetPinPath));
				return Report;
			}

						Report.bTargetModified = true;
			AddReportEntry(
				Report.PerformedWrites,
				FString::Printf(TEXT("add_link:%s->%s"), *Link.SourcePinPath, *Link.TargetPinPath));
		}
	}

	if (Report.bTargetModified)
	{
		Propagate(TargetControlRigBlueprint);
		AutoRecompile(TargetControlRigBlueprint);
		MarkDirty(TargetControlRigBlueprint);
	}

	if (Recipe.Verification.bCompile)
	{
		RequestInit(TargetControlRigBlueprint);
		Compile(TargetControlRigBlueprint);
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("compile_control_rig"), Recipe.TargetControlRigPath));
		Report.bCompileSucceeded=Current->R.Compiled.IsSet()&&Current->R.Compiled.GetValue();
		if (!Report.bCompileSucceeded)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("compile_failed"),
				TEXT("ControlRig Blueprint compile reported BS_Error."),
				TEXT("verification.compile"));
		}
	}
	else
	{
		AddReportEntry(Report.SkippedWrites, FormatReportEntry(TEXT("compile_not_requested"), Recipe.TargetControlRigPath));
	}

	if (UClass* GeneratedControlRigClass = CurrentRigClass(TargetControlRigBlueprint))
	{
		UControlRig* TransientControlRig = CreateTransientRig(TargetControlRigBlueprint);
		if (!TransientControlRig || !TransientControlRig->GetClass()->IsChildOf(UControlRig::StaticClass()))
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("transient_control_rig_create_failed"),
				FString::Printf(TEXT("Generated class could not create a transient ControlRig: %s."), *Current->TargetObject),
				TEXT("verification.direct_solve"));
		}
	}

	bool bDependencyAuditPassed = Recipe.Verification.bDependencyAudit
		? ScanPackageForForbiddenReferences(OwnedPackage(TargetControlRigBlueprint), Report)
		: false;
	if (Recipe.Verification.bDependencyAudit)
	{
		AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("dependency_audit"), Recipe.TargetControlRigPath));
	}
	else
	{
		AddReportEntry(Report.SkippedWrites, FormatReportEntry(TEXT("dependency_audit_not_requested"), Recipe.TargetControlRigPath));
	}

	if (Recipe.Verification.bSave && !HasError(Report))
	{
		UPackage* TargetPackage = OwnedPackage(TargetControlRigBlueprint);
		if (!TargetPackage)
		{
			AddIssue(
				Report,
				EHHControlRigIssueSeverity::Error,
				TEXT("save_package_missing"),
				TEXT("ControlRig Blueprint has no outer package to save."),
				TEXT("verification.save"));
		}
		else
		{
			TArray<UPackage*> PackagesToSave;
			PackagesToSave.Add(TargetPackage);
			Report.bSaveSucceeded |= SaveTarget(PackagesToSave,false);
			if (!Report.bSaveSucceeded)
			{
				AddIssue(
					Report,
					EHHControlRigIssueSeverity::Error,
					TEXT("save_failed"),
					FString::Printf(TEXT("Failed to save ControlRig package: %s."), *Current->TargetPackage),
					TEXT("verification.save"));
			}
			else if (Recipe.Verification.bDependencyAudit)
			{
				AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("save_control_rig_package"), Recipe.TargetControlRigPath));
				bDependencyAuditPassed = ScanPackageForForbiddenReferences(TargetPackage, Report) && bDependencyAuditPassed;
			}
			else
			{
				AddReportEntry(Report.PerformedWrites, FormatReportEntry(TEXT("save_control_rig_package"), Recipe.TargetControlRigPath));
			}
		}
	}
	else
	{
		Report.bSaveSucceeded |= false;
		AddReportEntry(
			Report.SkippedWrites,
			Recipe.Verification.bSave
				? FormatReportEntry(TEXT("save_skipped_due_to_errors"), Recipe.TargetControlRigPath)
				: FormatReportEntry(TEXT("save_not_requested"), Recipe.TargetControlRigPath));
	}

	Report.bDependencyAuditPassed = bDependencyAuditPassed;
	Report.bSuccess = !HasError(Report);
	Report.bPartialTargetModified = !Report.bSuccess && Report.bTargetModified;
	if (Report.bSuccess)
	{
		Report.NextAction = Report.bSaveSucceeded
			? TEXT("inspect_saved_sample_temp_control_rig")
			: TEXT("review_unsaved_sample_temp_control_rig_or_enable_save");
		Report.ReadbackSummary = Report.bSaveSucceeded
			? TEXT("BuildRecipe created or updated and saved the sample/temp ControlRig asset.")
			: TEXT("BuildRecipe created or updated the sample/temp ControlRig asset. Save was not requested or performed.");
	}
	else
	{
		Report.NextAction = Report.bPartialTargetModified
			? TEXT("inspect_then_cleanup_or_retry_sample_temp_target")
			: TEXT("fix_recipe_and_rerun_build_controlrig_recipe");
		Report.ReadbackSummary = Report.bPartialTargetModified
			? TEXT("BuildRecipe failed after partial work; inspect partial_target_modified, cleanup_suggestions, and issues.")
			: TEXT("BuildRecipe failed before asset mutation; inspect issues and rerun after fixing the recipe.");
		if (Report.bPartialTargetModified)
		{
			AddTargetCleanupGuidance(Report);
		}
	}
	return Report;
}

bool FinalReadback(FWork& W)
{
 if(!W.Guard()||!W.Bound||!W.Idle()||!W.PackageOwned())return false;FString H;if(!W.SnapshotHierarchy(H)||!Same(H,W.LastHierarchy))return W.Fail(TEXT("controlrig_recipe_hierarchy_final_mismatch"),TEXT("Current hierarchy changed after its verified authoring step"));
 if(W.Graph){FString G;if(!W.GraphSignature(G)||!Same(G,W.LastGraph))return W.Fail(TEXT("controlrig_recipe_graph_final_mismatch"),TEXT("Current typed RigVM node/pin/link projection changed after its verified step"));}
 W.R.Data->SetStringField(TEXT("current_target_path"),W.TargetObject);W.R.Data->SetBoolField(TEXT("current_target_dirty"),W.Package->IsDirty());W.R.Data->SetStringField(TEXT("hierarchy_projection_hash"),FMD5::HashAnsiString(*H));if(W.Graph)W.R.Data->SetStringField(TEXT("graph_projection_hash"),FMD5::HashAnsiString(*W.LastGraph));
 W.R.Data->SetBoolField(TEXT("typed_target_postcondition_passed"),true);return W.Guard();
}
FString Finish(FWork& W,const TCHAR* Operation,bool Pure)
{
 if(!W.R.ErrorCode.IsEmpty())
 {
  AddIssue(W.Report,EHHControlRigIssueSeverity::Error,W.R.ErrorCode,W.R.Error,TEXT("actual_operation"));W.Report.bSuccess=false;W.Report.bPartialTargetModified=W.Report.bTargetModified;
  if(W.Report.bPartialTargetModified)AddTargetCleanupGuidance(W.Report);
 }
 W.R.bSuccess=W.Report.bSuccess&&W.R.ErrorCode.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess&&(Pure||W.FinalVerified);
 if(W.R.bSuccess&&!W.R.bPostconditionPassed){W.Error(TEXT("controlrig_recipe_final_unverified"),TEXT("Current typed target postcondition was not verified"));W.R.bSuccess=false;}
 if(W.R.bPostconditionPassed)W.R.bChangeStateKnown=true;else if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;
 if(!W.R.bSuccess&&W.R.ErrorCode.IsEmpty()){W.R.ErrorCode=TEXT("controlrig_recipe_rejected");W.R.Error=TEXT("Recipe validation/build failed; original issues and actual partial result are preserved");}
 W.Report.bSaveSucceeded|=W.R.bSaved;W.R.Data->SetObjectField(TEXT("legacy_result"),ReportToObject(W.Report));W.R.Data->SetArrayField(TEXT("actual_call_prefix"),W.Calls);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("scoped_target_change_state_known"),W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("post_failure_live_observation_skipped"),W.Lost||!W.R.ErrorCode.IsEmpty());W.R.Data->SetBoolField(TEXT("mutation_state_unknown"),!W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("do_not_retry"),W.R.bMutationAttempted&&!W.R.bSuccess);W.R.Data->SetBoolField(TEXT("cleanup_guidance_is_automatic_rollback"),false);
 W.R.Data->SetBoolField(TEXT("explicit_compile_requested"),W.R.bCompileRequested);W.R.Data->SetBoolField(TEXT("explicit_compile_attempted"),W.R.bCompileAttempted);W.R.Data->SetBoolField(TEXT("explicit_save_requested"),W.R.bSaveRequested);W.R.Data->SetBoolField(TEXT("explicit_save_attempted"),W.R.bSaveAttempted);W.R.Data->SetBoolField(TEXT("actual_saved_API_true"),W.R.bSaved);W.R.Data->SetStringField(TEXT("actual_compile_save_scope"),TEXT("Intent, SDK attempt, current synchronous Blueprint result and actual Save API true are separate; no solver execution, cold reload or atomic package persistence proof"));
 if(W.R.bMutationAttempted&&!W.R.bSuccess&&!W.R.bChangeStateKnown){W.R.Data->SetBoolField(TEXT("uncertain_writer_effect"),true);W.R.Data->SetBoolField(TEXT("legacy_partial_false_is_no_mutation_proof"),false);W.R.Data->SetStringField(TEXT("unknown_writer_guidance"),TEXT("Do not replay. Inspect the exact requested target and recorded SDK return prefix. No automatic deletion, overwrite or rollback is performed."));W.Report.NextAction=TEXT("inspect_recorded_unknown_writer_before_new_operation");AddReportEntry(W.Report.CleanupSuggestions,TEXT("Inspect the exact requested target and SDK return prefix before any new operation; uncertain ownership is not deletion authority."));W.R.Data->SetObjectField(TEXT("legacy_result"),ReportToObject(W.Report));}if(W.R.bSuccess)W.R.Stage=TEXT("complete");FString Encoded=Cubeless::Operations::EncodeVariableResult(W.R,Operation);const FTCHARToUTF8 OriginalBytes(*Encoded);
 if(OriginalBytes.Length()>4*1024*1024)
 {
  const FString FullHash=FMD5::HashAnsiString(*Encoded);const int32 OriginalSize=OriginalBytes.Length();TArray<TSharedPtr<FJsonValue>> Prefix;
  for(int32 I=0;I<W.Calls.Num();++I)if(I<16||I>=W.Calls.Num()-16)Prefix.Add(W.Calls[I]);
  W.R.Data->SetArrayField(TEXT("actual_call_prefix"),Prefix);W.R.Data->SetBoolField(TEXT("diagnostic_call_prefix_omitted"),true);W.R.Data->SetNumberField(TEXT("original_call_prefix_rows"),W.Calls.Num());
  Encoded=Cubeless::Operations::EncodeVariableResult(W.R,Operation);
  if(FTCHARToUTF8(*Encoded).Length()>4*1024*1024)
  {
   W.R.Data->SetBoolField(TEXT("actual_Engine_work_completed_before_receipt_failure"),W.R.bSuccess&&W.R.bPostconditionPassed);W.R.Data->SetStringField(TEXT("original_error_code_before_response_budget"),W.R.ErrorCode);
   W.R.bSuccess=false;W.R.bPostconditionPassed=false;W.R.ErrorCode=TEXT("controlrig_recipe_response_budget_exceeded");W.R.Error=TEXT("Canonical legacy output exceeds4MiB receipt budget; exact actual scalar effects and bounded original result prefix are preserved. Do not replay.");
   auto Summary=W.Report;Summary.bSuccess=false;Summary.ReadbackSummary=W.R.Error;Summary.NextAction=TEXT("inspect_recorded_actual_effects_before_new_operation");
   auto Bound=[](auto& A){if(A.Num()>16)A.SetNum(16,EAllowShrinking::No);};Bound(Summary.Issues);Bound(Summary.Warnings);Bound(Summary.PlannedWrites);Bound(Summary.PerformedWrites);Bound(Summary.SkippedWrites);Bound(Summary.ReusedItems);Bound(Summary.CleanupSuggestions);
   W.R.Data->SetObjectField(TEXT("legacy_result"),ReportToObject(Summary));W.R.Data->SetBoolField(TEXT("public_legacy_arrays_truncated"),true);W.R.Data->SetNumberField(TEXT("public_legacy_array_prefix_limit"),16);W.R.Data->SetBoolField(TEXT("do_not_retry"),W.R.bMutationAttempted);Encoded=Cubeless::Operations::EncodeVariableResult(W.R,Operation);
  }
  W.R.Data->SetNumberField(TEXT("original_encoded_response_UTF8_bytes"),OriginalSize);W.R.Data->SetStringField(TEXT("original_encoded_response_MD5"),FullHash);W.R.Data->SetBoolField(TEXT("original_encoded_response_preserved"),false);Encoded=Cubeless::Operations::EncodeVariableResult(W.R,Operation);
 }
 return Encoded;
}
bool RecipeNamesSafe(const FHHControlRigRecipe& Recipe)
{
 auto Name=[](const FString& S){return TextSafe(S,NAME_SIZE-1);};
 auto Asset=[&](const FString& S){return Name(S)&&(!FPackageName::IsValidLongPackageName(S)||Name(MakeObjectPathFromPackagePath(S)));};
 if(!Asset(Recipe.TargetControlRigPath)||!Asset(Recipe.SourceSkeletalMesh)||!Asset(Recipe.PreviewSkeletalMesh))return false;
 for(const auto& S:Recipe.Hierarchy.Nulls)if(!Name(S))return false;for(const auto& S:Recipe.Hierarchy.Controls)if(!Name(S))return false;for(const auto& S:Recipe.Hierarchy.Curves)if(!Name(S))return false;
 for(const auto& S:Recipe.Hierarchy.NullSpecs)if(!Name(S.Name)||!Name(S.Parent))return false;
 for(const auto& S:Recipe.Hierarchy.ControlSpecs)if(!Name(S.Name)||!Name(S.Parent)||!Name(S.ShapeName))return false;
 for(const auto& S:Recipe.Hierarchy.CurveSpecs)if(!Name(S.Name))return false;
 for(const auto& S:Recipe.RigVM.Units)if(!Name(S.NodeName)||!Name(S.StructName))return false;
 for(const auto& S:Recipe.RigVM.PinDefaults)if(!Name(S.NodeName)||!Name(S.PinPath))return false;
 for(const auto& S:Recipe.RigVM.Links)if(!Name(S.SourcePinPath)||!Name(S.TargetPinPath))return false;
 return true;
}
FString Run(const FString& ParamsJson,int32 Mode)
{
 const TCHAR* Operation=Mode==0?TEXT("validate_controlrig_recipe"):Mode==1?TEXT("dry_run_controlrig_recipe"):TEXT("build_controlrig_recipe");FWork W;
 if(!IsInGameThread()||Running){W.Error(TEXT("controlrig_recipe_reentry_or_thread"),TEXT("Native recipe entry requires GT and rejects nested direct calls"));return Finish(W,Operation,true);}TGuardValue<bool> Scope(Running,true);TGuardValue<FWork*> CurrentScope(Current,&W);
 W.R.Data->SetStringField(TEXT("workflow_scope"),Mode==2?TEXT("Editor_only_recipe_build"):TEXT("pure_recipe_validation_no_Engine_work"));
 TSharedPtr<FJsonObject> Params;if(!JsonBodySafe(ParamsJson)||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ParamsJson),Params)||!Params){W.Error(TEXT("controlrig_recipe_params_invalid"),TEXT("Finite bounded ParamsJson envelope required"));return Finish(W,Operation,true);}
 FString RecipeJson,OptionsJson;if(!Params->TryGetStringField(TEXT("recipe"),RecipeJson)){W.Error(TEXT("controlrig_recipe_json_required"),TEXT("recipe must be the original _json_payload JSON body string"));return Finish(W,Operation,true);}if(Mode==2&&Params->HasField(TEXT("options"))&&!Params->TryGetStringField(TEXT("options"),OptionsJson)){W.Error(TEXT("controlrig_recipe_options_invalid"),TEXT("Optional options must be the original JSON body string or omitted"));return Finish(W,Operation,true);}
 if(!JsonBodySafe(RecipeJson)||!JsonBodySafe(OptionsJson)){W.Error(TEXT("controlrig_recipe_numeric_or_capture_bound"),TEXT("JSON body exceeds finite float-storage/depth64/262144slot/1MiUTF16 bound; no writer"));return Finish(W,Operation,true);}
 FHHControlRigRecipe Recipe;FHHControlRigBuildOptions Options;FString ParseError;if(!ParseRecipeJson(RecipeJson,Recipe,ParseError)){AddIssue(W.Report,EHHControlRigIssueSeverity::Error,TEXT("invalid_json"),ParseError,TEXT("recipe_json"));W.Report.bDryRun=Mode!=2;MarkNoMutationStop(W.Report,TEXT("Recipe JSON parse failed. No mutation occurred."),Mode==0?TEXT("fix_recipe_json_and_rerun_validate_controlrig_recipe"):Mode==1?TEXT("fix_recipe_json_and_rerun_dry_run_controlrig_recipe"):TEXT("fix_recipe_json_and_rerun_build_controlrig_recipe"));return Finish(W,Operation,true);}
 W.R.bCompileRequested=Recipe.Verification.bCompile;W.R.bSaveRequested=Recipe.Verification.bSave;W.R.Data->SetStringField(TEXT("requested_target_path"),Recipe.TargetControlRigPath);W.R.Data->SetBoolField(TEXT("recipe_requested_intent_is_not_dispatch"),true);
 if(!RecipeNamesSafe(Recipe)){W.Report=MakeBaseReport(Recipe,Options);W.Error(TEXT("controlrig_recipe_name_bound"),TEXT("Recipe identifier/path exceeds NAME_SIZE or contains NUL; no writer dispatched"));return Finish(W,Operation,true);}
 if(Mode==2&&!ParseBuildOptionsJson(OptionsJson,Options,ParseError)){W.Report=MakeBaseReport(Recipe,Options);AddIssue(W.Report,EHHControlRigIssueSeverity::Error,TEXT("invalid_options_json"),ParseError,TEXT("options_json"));MarkNoMutationStop(W.Report,TEXT("Options JSON parse failed. No mutation occurred."),TEXT("fix_options_json_and_rerun_build_controlrig_recipe"));return Finish(W,Operation,true);}
 if(Mode<2){W.Report=Mode==0?Cubeless::Operations::ControlRigRecipeAuthoring::ValidateRecipe(Recipe,Options):Cubeless::Operations::ControlRigRecipeAuthoring::DryRunRecipe(Recipe,Options);W.FinalVerified=true;return Finish(W,Operation,true);}
 W.Report=MakeBaseReport(Recipe,Options);W.Report.bDryRun=false;
 if(!W.Init())return Finish(W,Operation,false);
 BuildRecipeWork(Recipe,Options);
 if(W.R.ErrorCode.IsEmpty()&&W.Report.bSuccess)W.FinalVerified=FinalReadback(W);
 return Finish(W,Operation,false);
}
}
FString ValidateRecipe(const FString& ParamsJson){return Run(ParamsJson,0);}
FString DryRunRecipe(const FString& ParamsJson){return Run(ParamsJson,1);}
FString BuildRecipe(const FString& ParamsJson){return Run(ParamsJson,2);}
}
