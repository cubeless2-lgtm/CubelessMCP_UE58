#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#if !UE_VERSION_OLDER_THAN(5,8,0)
#include "ChaosClothAsset/ClothAsset.h"
#include "ChaosClothAsset/ClothAssetFactory.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowInputOutput.h"
#include "Dataflow/DataflowEdNode.h"
#include "Dataflow/DataflowInstance.h"
#include "Engine/StaticMesh.h"
#include "Engine/SkeletalMesh.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
#include "Modules/ModuleManager.h"
#include "UObject/UObjectGlobals.h"
#include "Misc/AutomationTest.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "EdGraphNode_Comment.h"
#endif
#endif

namespace Cubeless::UE58::DomainExpansion
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
namespace
{
bool ClothGraphContext()
{return IsInGameThread()&&IsValid(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage();}
bool ClothGraphPath(const FString& Path,bool AllowEmpty=false)
{if(Path.IsEmpty())return AllowEmpty;if(Path.Len()>=NAME_SIZE||!Path.StartsWith(TEXT("/")))return false;for(TCHAR C:Path)if(C==0)return false;return true;}
bool ClothGraphName(const FString& Name)
{if(Name.IsEmpty()||Name.Len()>=NAME_SIZE)return false;for(TCHAR C:Name)if(C==0)return false;return true;}
FObject ClothGraphUnknown(const FString& Target,const TCHAR* Stage)
{auto R=Error(TEXT("Candidate/source/context changed at an engine callback; retained target package requires inspection, no cleanup or retry."));R->SetStringField(TEXT("target_package"),Target);R->SetStringField(TEXT("stage"),Stage);R->SetBoolField(TEXT("read_only"),false);R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("candidate_retained"),true);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("outcome_unknown"),true);R->SetBoolField(TEXT("do_not_retry"),true);return R;}
bool ClothNode(const UE::Dataflow::FFactoryParameters& F)
{
 return !F.bIsDeprecated && F.DefaultNodeObject && F.DefaultNodeObject->TypedScriptStruct() &&
  F.DefaultNodeObject->TypedScriptStruct()->GetOutermost()->GetName()==TEXT("/Script/ChaosClothAssetDataflowNodes");
}
FObject NodeRow(const TSharedPtr<FDataflowNode>& Node)
{
 auto R=MakeShared<FJsonObject>(); R->SetStringField(TEXT("name"),Node->GetName().ToString());
 R->SetStringField(TEXT("type"),Node->GetType().ToString());
 TArray<TSharedPtr<FJsonValue>> Inputs,Outputs;
 for(const auto* Pin:Node->GetInputs()) { auto V=MakeShared<FJsonObject>(); V->SetStringField(TEXT("name"),Pin->GetName().ToString()); V->SetStringField(TEXT("type"),Pin->GetType().ToString()); Inputs.Add(MakeShared<FJsonValueObject>(V)); }
 for(const auto* Pin:Node->GetOutputs()) { auto V=MakeShared<FJsonObject>(); V->SetStringField(TEXT("name"),Pin->GetName().ToString()); V->SetStringField(TEXT("type"),Pin->GetType().ToString()); Outputs.Add(MakeShared<FJsonValueObject>(V)); }
 R->SetArrayField(TEXT("inputs"),Inputs); R->SetArrayField(TEXT("outputs"),Outputs);
 FObject Properties=MakeShared<FJsonObject>(); Describe(const_cast<UScriptStruct*>(Node->TypedScriptStruct()),Node.Get(),Properties); R->SetObjectField(TEXT("properties"),Properties); return R;
}
bool Identifier(const FString& Name)
{
 if(Name.IsEmpty() || Name.Len()>64 || HasMCPReference(Name) || FChar::IsDigit(Name[0])) { return false; }
 for(TCHAR C:Name) { if(C>127 || (!FChar::IsAlnum(C) && C!=TEXT('_'))) { return false; } } return true;
}
// UObject names and native Dataflow names use FName semantics, including case folding.
// Comments and other graph children occupy the same UObject namespace as editor nodes.
bool ClothRecipeChildNameAvailable(UDataflow* Owner,FName Name)
{
 return !Name.IsNone() && (!Owner || !FindObjectFast<UObject>(Owner,Name));
}
bool ClothRecipeNameAvailable(UDataflow* Owner,FName Name)
{
 if(!ClothRecipeChildNameAvailable(Owner,Name))return false;
 if(Owner)
 {
  const auto Graph=Owner->GetDataflowGraph();if(!Graph)return false;
  for(const auto& Node:Graph->GetNodes())if(!Node || Node->GetName()==Name)return false;
 }
 return true;
}
bool ReserveClothRecipeName(UDataflow* Owner,FName Name,TSet<FName>& Reserved)
{
 if(Reserved.Contains(Name)||!ClothRecipeNameAvailable(Owner,Name))return false;
 Reserved.Add(Name);return true;
}
FObject GraphRecipe(const FObject& P)
{
 FString Why; const FString Target=String(P,TEXT("target_path")),Source=String(P,TEXT("source_path"));
 if(!ClothGraphContext()||!ClothGraphPath(Target))return Error(TEXT("Idle Editor and bounded target asset path required"));
 if(!UnusedAssetPath(Target,Why)) { return Error(Why); }
 if(!ClothGraphPath(Source,true))return Error(TEXT("Bounded source Dataflow path required"));
 TStrongObjectPtr<UDataflow> SourceHold(Source.IsEmpty()?nullptr:LoadObject<UDataflow>(nullptr,*Source));UDataflow* Original=SourceHold.Get();
 if(!Source.IsEmpty() && (!Original || !IndependentSavedSource(Original,Why))) { return Error(Why.IsEmpty() ? TEXT("Saved Dataflow source required") : Why); }
 if(!GEditor || GEditor->PlayWorld) { return Error(TEXT("Author outside PIE")); }
 const TArray<TSharedPtr<FJsonValue>> *Nodes=nullptr,*Links=nullptr;
 if(!P->TryGetArrayField(TEXT("nodes"),Nodes) || Nodes->IsEmpty() || Nodes->Num()>32 || !P->TryGetArrayField(TEXT("links"),Links) || Links->Num()>128) { return Error(TEXT("1..32 nodes and 0..128 links required")); }
 auto* Factory=UE::Dataflow::FNodeFactory::GetInstance(); TSet<FString> Names;TSet<FName> ReservedNames;
 for(const auto& Value:*Nodes)
 {
  if(Value->Type!=EJson::Object) { return Error(TEXT("Node specification object required")); }
  auto Spec=Value->AsObject(); const FString Name=String(Spec,TEXT("name"));
  if(!Identifier(Name) || !ClothGraphName(String(Spec,TEXT("type"))) || !ClothNode(Factory->GetParameters(*String(Spec,TEXT("type"))))) { return Error(TEXT("Unique English name and registered current Cloth node type required")); }
  if(!ReserveClothRecipeName(Original,FName(*Name),ReservedNames))return Error(TEXT("Node name conflicts with a requested FName, existing native node or graph child object; no candidate created"));
  Names.Add(Name);
 }
 auto R=Result(); R->SetBoolField(TEXT("dry_run"),DryRun(P));
 if(DryRun(P)) { R->SetStringField(TEXT("validation_level"),TEXT("source/path/node-type preflight; native connections validate candidate")); return R; }
 UPackage* Package=CreatePackage(*Target);
 TStrongObjectPtr<UDataflow> Candidate(Original ? DuplicateObject<UDataflow>(Original,Package,*FPackageName::GetLongPackageAssetName(Target)) : NewObject<UDataflow>(Package,*FPackageName::GetLongPackageAssetName(Target),RF_Public|RF_Standalone|RF_Transactional));
 if(!Candidate.IsValid()) { return Error(TEXT("Unable to create Dataflow candidate")); } Candidate->SetFlags(RF_Public|RF_Standalone|RF_Transactional);
 const FString CandidatePath=Candidate->GetPathName();TStrongObjectPtr<UPackage> KeepPackage(Package);TWeakObjectPtr<UEditorEngine> Editor(GEditor);TWeakObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());
 auto Graph=Candidate->GetDataflowGraph();if(!Graph || Graph->GetNodes().Num()>512) { return CandidateFailure(Candidate.Get(),TEXT("Bounded candidate graph required")); }
 auto Current=[&](){return ClothGraphContext()&&GEditor==Editor.Get()&&GEditor->GetEditorWorldContext().World()==World.Get()&&IsValid(World.Get())&&!World->bIsTearingDown&&Candidate.IsValid()&&!Candidate->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)&&Candidate->GetOutermost()==KeepPackage.Get()&&Candidate->GetPathName()==CandidatePath&&Candidate->GetDataflowGraph()==Graph&&(!Original||(IsValid(Original)&&!Original->GetOutermost()->IsDirty()));};
 if(!Current())return ClothGraphUnknown(Target,TEXT("candidate_creation"));
 Candidate->Schema=LoadClass<UEdGraphSchema>(nullptr,TEXT("/Script/DataflowEditor.DataflowSchema"));if(!Current())return ClothGraphUnknown(Target,TEXT("schema_load"));
 if(!Candidate->Schema) { return CandidateFailure(Candidate.Get(),TEXT("Stock Dataflow editor schema unavailable")); }
 TArray<TStrongObjectPtr<UObject>> BoundSources;TMap<FString,TSharedPtr<FDataflowNode>> ByName;
 for(const auto& N:Graph->GetNodes()) { ByName.Add(N->GetName().ToString(),N); }
 for(int32 I=0;I<Nodes->Num();++I)
 {
  auto Spec=(*Nodes)[I]->AsObject(); const FString Name=String(Spec,TEXT("name"));
  if(!Current())return ClothGraphUnknown(Target,TEXT("node_name_preflight"));
  if(!ClothRecipeNameAvailable(Candidate.Get(),FName(*Name))) { return CandidateFailure(Candidate.Get(),TEXT("Existing native node or graph child object replacement refused")); }
  auto Node=Factory->NewNodeFromRegisteredType(*Graph,{FGuid::NewGuid(),*String(Spec,TEXT("type")),*Name,Candidate.Get()});
  if(!Current())return ClothGraphUnknown(Target,TEXT("dataflow_node_creation"));if(!Node) { return CandidateFailure(Candidate.Get(),TEXT("Native node creation failed")); }
  auto* Struct=const_cast<UScriptStruct*>(Node->TypedScriptStruct());
  const TSharedPtr<FJsonObject>* Settings=nullptr;
  if(Spec->HasField(TEXT("settings")) && (!Spec->TryGetObjectField(TEXT("settings"),Settings) || !Patch(Struct,Node.Get(),*Settings,Why))) { return CandidateFailure(Candidate.Get(),Why.IsEmpty() ? TEXT("Invalid settings object") : Why); }
  const TSharedPtr<FJsonObject>* Bindings=nullptr;
  if(Spec->HasField(TEXT("asset_bindings")))
  {
   if(!Spec->TryGetObjectField(TEXT("asset_bindings"),Bindings) || (*Bindings)->Values.Num()>8) { return CandidateFailure(Candidate.Get(),TEXT("Bounded asset bindings required")); }
   for(const auto& Pair:(*Bindings)->Values)
   {
    FString Path;const FString FieldName(Pair.Key.ToView());if(!ClothGraphName(FieldName))return CandidateFailure(Candidate.Get(),TEXT("Bounded asset field name required"));auto* Field=FindFProperty<FObjectPropertyBase>(Struct,*FieldName);
    UObject* Object=Pair.Value->TryGetString(Path)&&ClothGraphPath(Path)&&!HasMCPReference(Path)?LoadObject<UObject>(nullptr,*Path):nullptr;TStrongObjectPtr<UObject> HoldSource(Object);if(!Current())return ClothGraphUnknown(Target,TEXT("binding_asset_load"));
    if(!Field || !Field->HasAnyPropertyFlags(CPF_Edit) || Field->HasAnyPropertyFlags(CPF_Transient|CPF_EditConst) || !Object ||
      (!Object->IsA<UStaticMesh>() && !Object->IsA<USkeletalMesh>() && !Object->IsA<UPhysicsAsset>()) || !Object->IsA(Field->PropertyClass) || !IndependentSavedSource(Object,Why))
    { return CandidateFailure(Candidate.Get(),TEXT("Saved independent mesh/PhysicsAsset binding with exact editable object field required")); }
    BoundSources.Emplace(Object);Field->SetObjectPropertyValue_InContainer(Node.Get(),Object);if(!Current())return ClothGraphUnknown(Target,TEXT("binding_assignment"));
   }
  }
  Node->Invalidate();if(!Current())return ClothGraphUnknown(Target,TEXT("node_invalidate"));ByName.Add(Name,Node);
  // Native creation/patch/load/invalidation can notify listeners. Recheck the child
  // namespace immediately before NewObject, which otherwise replaces or fatals.
  if(!ClothRecipeChildNameAvailable(Candidate.Get(),FName(*Name)))return CandidateFailure(Candidate.Get(),TEXT("Graph child name became occupied before editor node creation"));
  auto* EditorNode=NewObject<UDataflowEdNode>(Candidate.Get(),*Name,RF_Transactional);
  TStrongObjectPtr<UDataflowEdNode> HoldEditorNode(EditorNode);if(!Current()||!IsValid(EditorNode))return ClothGraphUnknown(Target,TEXT("editor_node_creation"));
  EditorNode->SetDataflowGraph(Graph);EditorNode->SetDataflowNodeGuid(Node->GetGuid());Candidate->AddNode(EditorNode,true,false);if(!Current()||!IsValid(EditorNode)||!Candidate->Nodes.Contains(EditorNode))return ClothGraphUnknown(Target,TEXT("editor_add_node"));
  EditorNode->CreateNewGuid();EditorNode->PostPlacedNewNode();if(!Current()||!IsValid(EditorNode)||!Candidate->Nodes.Contains(EditorNode))return ClothGraphUnknown(Target,TEXT("editor_post_placed"));EditorNode->AllocateDefaultPins();if(!Current()||!IsValid(EditorNode)||!Candidate->Nodes.Contains(EditorNode))return ClothGraphUnknown(Target,TEXT("editor_allocate_pins"));EditorNode->NodePosX=I*300;
 }
 for(const auto& Value:*Links)
 {
  if(Value->Type!=EJson::Object) { return CandidateFailure(Candidate.Get(),TEXT("Link object required")); }
  auto Link=Value->AsObject();if(!ClothGraphName(String(Link,TEXT("from_pin")))||!ClothGraphName(String(Link,TEXT("to_pin"))))return CandidateFailure(Candidate.Get(),TEXT("Bounded exact link pin names required"));auto From=ByName.FindRef(String(Link,TEXT("from_node"))),To=ByName.FindRef(String(Link,TEXT("to_node")));
  auto* Output=From ? From->FindOutput(FName(*String(Link,TEXT("from_pin")))) : nullptr;
  auto* Input=To ? To->FindInput(FName(*String(Link,TEXT("to_pin")))) : nullptr;
  TArray<const FDataflowNode*> Queue; TSet<const FDataflowNode*> Seen; if(From) { Queue.Add(From.Get()); }
  bool Cycle=false;
  for(int32 I=0;I<Queue.Num() && !Cycle;++I) { const auto* N=Queue[I]; if(N==To.Get()) { Cycle=true; break; } if(Seen.Contains(N)) { continue; } Seen.Add(N); for(const auto* Pin:N->GetInputs()) { if(Pin->GetConnection()) { Queue.AddUnique(Pin->GetConnection()->GetOwningNode()); } } }
  if(!Output||!Input||Cycle||Input->GetConnection()||!Graph->CanConnect(*Output,*Input))return CandidateFailure(Candidate.Get(),TEXT("Exact unoccupied input and acyclic native type-approved connection required"));
  if(!Current())return ClothGraphUnknown(Target,TEXT("can_connect"));const bool Connected=Graph->Connect(*Output,*Input);if(!Current())return ClothGraphUnknown(Target,TEXT("connect"));if(!Connected)return CandidateFailure(Candidate.Get(),TEXT("Native Dataflow connection refused"));
 }
 const auto EditorNodes=Candidate->Nodes;for(UEdGraphNode* Node:EditorNodes){if(!Current()||!IsValid(Node)||!Candidate->Nodes.Contains(Node))return ClothGraphUnknown(Target,TEXT("editor_pin_membership"));if(auto* Ed=Cast<UDataflowEdNode>(Node)){TStrongObjectPtr<UDataflowEdNode> Hold(Ed);Ed->UpdatePinsConnectionsFromDataflowNode();if(!Current()||!IsValid(Ed)||!Candidate->Nodes.Contains(Ed))return ClothGraphUnknown(Target,TEXT("update_editor_pins"));}}
 TArray<TSharedPtr<FJsonValue>> Rows; for(const FString& Name:Names) { Rows.Add(MakeShared<FJsonValueObject>(NodeRow(ByName[Name]))); }
 if(!Current())return ClothGraphUnknown(Target,TEXT("node_readback"));R->SetArrayField(TEXT("created_nodes"),Rows); R->SetBoolField(TEXT("source_unchanged"),true); R->SetBoolField(TEXT("evaluated"),false); PublishCandidate(Candidate.Get(),R); return R;
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FCubelessClothRecipeNamePreflight,
 "CubelessMCP.Domain.Authoring.ClothRecipeNamePreflight",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FCubelessClothRecipeNamePreflight::RunTest(const FString& Parameters)
{
 TSet<FName> Names;
 TestTrue(TEXT("First name reserved"),ReserveClothRecipeName(nullptr,FName(TEXT("Weights")),Names));
 TestFalse(TEXT("Case variant rejected before writer"),ReserveClothRecipeName(nullptr,FName(TEXT("weights")),Names));
 TestFalse(TEXT("NAME_None cannot become an automatic object name"),ReserveClothRecipeName(nullptr,NAME_None,Names));
 TStrongObjectPtr<UDataflow> Source(NewObject<UDataflow>(GetTransientPackage(),NAME_None,RF_Transient));
 auto* Comment=NewObject<UEdGraphNode_Comment>(Source.Get(),TEXT("ExistingComment"),RF_Transactional);
 Source->Nodes.Add(Comment);
 TestFalse(TEXT("Source comment occupies child namespace"),ClothRecipeNameAvailable(Source.Get(),FName(TEXT("existingcomment"))));
 TStrongObjectPtr<UDataflow> Clone(DuplicateObject<UDataflow>(Source.Get(),GetTransientPackage(),MakeUniqueObjectName(GetTransientPackage(),UDataflow::StaticClass(),TEXT("ClothNameClone"))));
 if(!TestNotNull(TEXT("Source clone"),Clone.Get()))return false;
 TestFalse(TEXT("Cloned comment rejected before native writer"),ClothRecipeNameAvailable(Clone.Get(),FName(TEXT("ExistingComment"))));
 TestFalse(TEXT("Cloned comment rejected before editor writer"),ClothRecipeChildNameAvailable(Clone.Get(),FName(TEXT("ExistingComment"))));
 TestTrue(TEXT("Unoccupied clone name remains valid"),ClothRecipeNameAvailable(Clone.Get(),FName(TEXT("NewWeights"))));
 TestEqual(TEXT("Source still has its original comment"),Source->Nodes.Num(),1);
 TestTrue(TEXT("Source comment identity preserved"),Source->Nodes[0]==Comment);
 return true;
}
#endif
FObject CreateCloth(const FObject& P)
{
 FString Why; const FString Target=String(P,TEXT("target_path")),Path=String(P,TEXT("template_path"));
 if(!ClothGraphContext()||!ClothGraphPath(Target))return Error(TEXT("Idle Editor and bounded target asset path required"));
 if(!UnusedAssetPath(Target,Why)) { return Error(Why); }
 if(!ClothGraphPath(Path))return Error(TEXT("Bounded template Dataflow path required"));TStrongObjectPtr<UDataflow> KeepTemplate(LoadObject<UDataflow>(nullptr,*Path));UDataflow* Template=KeepTemplate.Get();
 if(!Template || !IndependentSavedSource(Template,Why)) { return Error(Why.IsEmpty() ? TEXT("Saved independent Dataflow template required") : Why); }
 if(!Template->GetDataflowGraph() || Template->GetDataflowGraph()->GetNodes().Num()>512) { return Error(TEXT("Bounded template graph required")); }
 bool Evaluate=false; if(P->HasField(TEXT("evaluate")) && !P->TryGetBoolField(TEXT("evaluate"),Evaluate)) { return Error(TEXT("evaluate must be boolean")); }
 if(!GEditor || GEditor->PlayWorld) { return Error(TEXT("Create outside PIE")); }
 auto R=Result(); R->SetBoolField(TEXT("dry_run"),DryRun(P)); if(DryRun(P)) { return R; }
 UPackage* Package=CreatePackage(*Target);
 TStrongObjectPtr<UChaosClothAsset> Asset(Cast<UChaosClothAsset>(UChaosClothAssetFactory::CreateClothAssetFromTemplate(UChaosClothAsset::StaticClass(),Package,*FPackageName::GetLongPackageAssetName(Target),RF_Public|RF_Standalone|RF_Transactional,&Path,true)));
 if(!Asset.IsValid()) { return CandidateFailure(nullptr,TEXT("Native cloth template creation failed; inspect target package")); }
 TStrongObjectPtr<UPackage> KeepPackage(Package);const FString AssetPath=Asset->GetPathName();TWeakObjectPtr<UEditorEngine> Editor(GEditor);TWeakObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());
 auto Current=[&](){return ClothGraphContext()&&GEditor==Editor.Get()&&GEditor->GetEditorWorldContext().World()==World.Get()&&IsValid(World.Get())&&!World->bIsTearingDown&&Asset.IsValid()&&!Asset->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)&&Asset->GetOutermost()==KeepPackage.Get()&&Asset->GetPathName()==AssetPath&&IsValid(Template)&&!Template->GetOutermost()->IsDirty();};
 if(!Current())return ClothGraphUnknown(Target,TEXT("cloth_factory"));UDataflow* Embedded=Asset->GetDataflowInstance().GetDataflowAsset();TStrongObjectPtr<UDataflow> KeepEmbedded(Embedded);
 if(!Embedded || !Embedded->IsIn(Asset.Get()) || Embedded->GetOutermost()!=Package) { return CandidateFailure(Asset.Get(),TEXT("Native factory did not create an owned embedded Dataflow")); }
 if(Evaluate){const bool Updated=Asset->GetDataflowInstance().UpdateOwnerAsset(false);if(!Current()||!IsValid(Embedded)||Asset->GetDataflowInstance().GetDataflowAsset()!=Embedded)return ClothGraphUnknown(Target,TEXT("cloth_evaluation"));if(!Updated)return CandidateFailure(Asset.Get(),TEXT("Native embedded Dataflow evaluation failed; no save attempted"));}
 R->SetBoolField(TEXT("evaluated"),Evaluate); TArray<TSharedPtr<FJsonValue>> Lods;
 for(const auto& Collection:static_cast<const UChaosClothAsset*>(Asset.Get())->GetClothCollections())
 {
  UE::Chaos::ClothAsset::FCollectionClothConstFacade Facade(Collection); FObject Row=MakeShared<FJsonObject>();
  Row->SetNumberField(TEXT("sim_vertices"),Facade.IsValid() ? Facade.GetNumSimVertices3D() : 0); Row->SetNumberField(TEXT("sim_faces"),Facade.IsValid() ? Facade.GetNumSimFaces() : 0); Lods.Add(MakeShared<FJsonValueObject>(Row));
 }
 if(!Current()||!IsValid(Embedded)||Asset->GetDataflowInstance().GetDataflowAsset()!=Embedded)return ClothGraphUnknown(Target,TEXT("cloth_readback"));R->SetArrayField(TEXT("lods"),Lods);
 R->SetStringField(TEXT("dataflow_path"),Embedded->GetPathName()); R->SetStringField(TEXT("terminal"),Asset->GetDataflowInstance().GetDataflowTerminal().ToString());
 R->SetBoolField(TEXT("embedded_dataflow"),true); R->SetBoolField(TEXT("simulation_verified"),false); PublishCandidate(Asset.Get(),R); return R;
}
}
#endif
FObject ClothGraph(const FObject& P)
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
 if(!ClothGraphContext())return Error(TEXT("Idle Editor game thread outside PIE/GC/save required"));
 if(!FModuleManager::LoadModulePtr<IModuleInterface>(TEXT("ChaosClothAssetDataflowNodes")))return Error(TEXT("ChaosClothAssetDataflowNodes module unavailable"));
 const FString Op=String(P,TEXT("operation"));
 if(Op==TEXT("authoring.cloth.graph_catalog"))
 {
  auto R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
  for(const auto& F:UE::Dataflow::FNodeFactory::GetInstance()->RegisteredParameters()) { if(ClothNode(F) && Rows.Num()<256) { Rows.Add(MakeShared<FJsonValueObject>(NodeRow(F.DefaultNodeObject))); } }
  R->SetArrayField(TEXT("nodes"),Rows); return R;
 }
 if(Op==TEXT("authoring.cloth.graph_recipe")) { return GraphRecipe(P); }
 if(Op==TEXT("authoring.cloth.create_asset")) { return CreateCloth(P); }
#endif
 return Error(TEXT("Unsupported cloth graph authoring operation"));
}
}
