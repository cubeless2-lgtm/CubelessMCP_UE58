#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "Animation/AnimBlueprint.h"
#include "Animation/AnimationAsset.h"
#include "Animation/Skeleton.h"
#include "AnimGraphNode_Base.h"
#include "AnimGraphNode_Root.h"
#include "AnimGraphNode_AssetPlayerBase.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphSchema.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "KismetCompiler.h"
#include "UObject/StrongObjectPtr.h"

namespace Cubeless::UE58::DomainExpansion
{
namespace
{
const TSet<FString> NodeClasses{
	TEXT("AnimGraphNode_Inertialization"),TEXT("AnimGraphNode_DeadBlending"),TEXT("AnimGraphNode_TwoWayBlend"),
	TEXT("AnimGraphNode_BlendListByBool"),TEXT("AnimGraphNode_LayeredBoneBlend"),TEXT("AnimGraphNode_SequencePlayer"),
	TEXT("AnimGraphNode_BlendSpacePlayer"),TEXT("AnimGraphNode_ModifyBone"),TEXT("AnimGraphNode_CopyBone"),
	TEXT("AnimGraphNode_SpringBone"),TEXT("AnimGraphNode_AnimDynamics"),TEXT("AnimGraphNode_RigidBody"),
	TEXT("AnimGraphNode_Trail"),TEXT("AnimGraphNode_LocalToComponentSpace"),TEXT("AnimGraphNode_ComponentToLocalSpace")};
UEdGraph* FindAnimGraph(UAnimBlueprint* BP,const FString& Name)
{
	TArray<UEdGraph*> Graphs; BP->GetAllGraphs(Graphs); UEdGraph* Match=nullptr;
	for(UEdGraph* G:Graphs) { if(G && G->GetName()==Name && G->GetSchema() && G->GetSchema()->GetClass()->GetPathName()==TEXT("/Script/AnimGraph.AnimationGraphSchema")) { if(Match) { return nullptr; } Match=G; } }
	return Match;
}
bool Acyclic(UEdGraph* Graph)
{
	TMap<const UEdGraphNode*,uint8> States;
	TFunction<bool(const UEdGraphNode*)> Visit=[&](const UEdGraphNode* Node)
	{
		const uint8 State=States.FindRef(Node); if(State==1) { return false; } if(State==2) { return true; }
		States.Add(Node,1);
		for(const UEdGraphPin* P:Node->Pins) { if(P && P->Direction==EGPD_Output) { for(const UEdGraphPin* Other:P->LinkedTo) { if(Other && Other->GetOwningNode()->GetGraph()==Graph && !Visit(Other->GetOwningNode())) { return false; } } } }
		States.Add(Node,2); return true;
	};
	for(const UEdGraphNode* Node:Graph->Nodes) { if(Node && !Visit(Node)) { return false; } } return true;
}
FObject Catalog()
{
	FObject R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
	for(const FString& Name:NodeClasses)
	{
		UClass* Class=LoadClass<UAnimGraphNode_Base>(nullptr,*(TEXT("/Script/AnimGraph.")+Name)); if(!Class || Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated)) { continue; }
		auto* Default=Cast<UAnimGraphNode_Base>(Class->GetDefaultObject()); FObject Row=MakeShared<FJsonObject>(),Fields=MakeShared<FJsonObject>();
		Row->SetStringField(TEXT("class_path"),Class->GetPathName());
		if(auto* P=Default->GetFNodeProperty()) { Describe(P->Struct,P->ContainerPtrToValuePtr<void>(Default),Fields); }
		Row->SetObjectField(TEXT("runtime_properties"),Fields); Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	R->SetArrayField(TEXT("node_classes"),Rows); return R;
}
FObject Recipe(const FObject& P)
{
	const FString Source=FPackageName::ExportTextPathToObjectPath(String(P,TEXT("source_path"))), Target=String(P,TEXT("target_path")); FString Why;
	if(!BoundedObjectPath(Source)) { return Error(TEXT("Bounded canonical source object/package path required after export-text normalization")); }
	if(!UnusedAssetPath(Target,Why)) { return Error(Why); }
	UAnimBlueprint* SourceBP=LoadObject<UAnimBlueprint>(nullptr,*Source);
    TStrongObjectPtr<UAnimBlueprint> SourceHold(SourceBP);
	if(!IndependentSavedSource(SourceBP,Why)) { return Error(Why); }
	if(!SourceBP->TargetSkeleton || !SourceBP->GeneratedClass || GEditor->PlayWorld) { return Error(TEXT("Compiled source AnimBP with skeleton outside PIE required")); }
	TStrongObjectPtr<UPackage> SourcePackage(SourceBP->GetOutermost());
	TStrongObjectPtr<UClass> SourceClass(SourceBP->GetClass()),SourceGeneratedClass(SourceBP->GeneratedClass);
	TStrongObjectPtr<USkeleton> Skeleton(SourceBP->TargetSkeleton);
	const FString SourceIdentity=SourceBP->GetPathName(),SourcePackageName=SourcePackage->GetName();
	auto SourceCurrent=[&]{return SafeContext()&&Live(SourceHold.Get())&&Live(SourcePackage.Get())&&Live(SourceClass.Get())&&Live(SourceGeneratedClass.Get())&&Live(Skeleton.Get())&&SourceBP->GetClass()==SourceClass.Get()&&SourceBP->GetOutermost()==SourcePackage.Get()&&SourcePackage->GetName()==SourcePackageName&&SourceBP->GetPathName()==SourceIdentity&&!SourcePackage->IsDirty()&&SourceBP->TargetSkeleton==Skeleton.Get()&&SourceBP->GeneratedClass==SourceGeneratedClass.Get();};
	const TArray<TSharedPtr<FJsonValue>> *Nodes=nullptr,*Links=nullptr;
	if(!P->TryGetArrayField(TEXT("nodes"),Nodes) || !Nodes || Nodes->IsEmpty() || Nodes->Num()>32 || !P->TryGetArrayField(TEXT("links"),Links) || !Links || Links->Num()>128) { return Error(TEXT("1..32 node specs and 0..128 links required")); }
	TSet<FString> IDs;
	for(const auto& Value:*Nodes)
	{
		if(!Value || Value->Type!=EJson::Object) { return Error(TEXT("Node spec object required")); }
		const auto Spec=Value->AsObject(); const FString ID=String(Spec,TEXT("id")), ClassPath=String(Spec,TEXT("class_path"));
		FString ClassName; ClassPath.Split(TEXT("."),nullptr,&ClassName);
		if(ID.IsEmpty() || ID.Len()>64 || ID==TEXT("root") || ID==TEXT("source") || IDs.Contains(ID) || !ClassPath.StartsWith(TEXT("/Script/AnimGraph.")) || !NodeClasses.Contains(ClassName)) { return Error(TEXT("Unique bounded node id and catalog class_path required")); }
		for(TCHAR C:ID) { if(C>127 || (!FChar::IsAlnum(C) && C!=TEXT('_'))) { return Error(TEXT("Node ids must use English identifiers")); } }
		const FString AssetPath=FPackageName::ExportTextPathToObjectPath(String(Spec,TEXT("animation_asset")));
		if(!AssetPath.IsEmpty()&&!BoundedObjectPath(AssetPath)){return Error(TEXT("Bounded canonical animation asset path required after export-text normalization"));}
		IDs.Add(ID);
	}
	FObject R=Result(); R->SetBoolField(TEXT("dry_run"),DryRun(P)); R->SetStringField(TEXT("target_path"),Target);
	R->SetField(TEXT("source_unchanged"),MakeShared<FJsonValueNull>());
	R->SetStringField(TEXT("source_preservation_scope"),TEXT("Exact object/package/class/generated-class/skeleton identity and clean package; arbitrary unmarked memory changes not proven"));
	if(DryRun(P)) { R->SetStringField(TEXT("validation_level"),TEXT("path/source/class preflight; candidate compile validates full recipe")); return R; }
	TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<UAnimBlueprint> BP;TStrongObjectPtr<UEdGraph> GraphHold;
	TArray<TSharedPtr<FJsonValue>> Prefix;FString Stage;bool DirtyLast=false,DirtyObserved=false;
	auto Enter=[&](const TCHAR* InStage){Stage=InStage;Prefix.Add(MakeShared<FJsonValueString>(Stage));R->SetBoolField(TEXT("read_only"),false);R->SetBoolField(TEXT("mutation_performed"),true);};
	auto Fail=[&](const FString& Message){R->SetBoolField(TEXT("success"),false);R->SetStringField(TEXT("error"),Message);R->SetStringField(TEXT("stage"),Stage);R->SetStringField(TEXT("target_path"),Target);R->SetBoolField(TEXT("candidate_retained"),BP.Get()!=nullptr);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("do_not_retry"),true);R->SetBoolField(TEXT("postcondition_verified"),false);R->SetArrayField(TEXT("actual_call_prefix"),Prefix);R->SetBoolField(TEXT("package_dirty_last_observed_valid"),DirtyObserved);if(DirtyObserved)R->SetBoolField(TEXT("package_dirty_last_observed"),DirtyLast);return R;};
	auto CandidateCurrent=[&]{return SourceCurrent()&&Live(Package.Get())&&Live(BP.Get())&&Package->GetName()==Target&&BP->GetOutermost()==Package.Get()&&BP->GetClass()==SourceClass.Get()&&BP->GetPathName()==Target+TEXT(".")+FPackageName::GetLongPackageAssetName(Target)&&BP->TargetSkeleton==Skeleton.Get();};
	// Loading/auditing sources can broadcast editor callbacks. Check vacancy again before writing.
	if(!SourceCurrent()||!UnusedAssetPath(Target,Why)||!SourceCurrent()){return Error(Why.IsEmpty()?TEXT("Source changed before duplication"):Why);}
	Enter(TEXT("create_candidate_package"));Package.Reset(CreatePackage(*Target));
	if(!SourceCurrent()||!Live(Package.Get())||Package->GetName()!=Target){return Fail(TEXT("Source/package changed during candidate creation"));}
	Enter(TEXT("duplicate_candidate"));BP.Reset(DuplicateObject<UAnimBlueprint>(SourceBP,Package.Get(),*FPackageName::GetLongPackageAssetName(Target)));
	if(!CandidateCurrent()) { return Fail(TEXT("Candidate duplication failed or source/candidate identity changed")); } BP->SetFlags(RF_Public|RF_Standalone|RF_Transactional);
	FString GraphName=String(P,TEXT("graph_name")); if(GraphName.IsEmpty()) { GraphName=TEXT("AnimGraph"); }
	UEdGraph* Graph=FindAnimGraph(BP.Get(),GraphName); if(!CandidateCurrent()||!Live(Graph) || !Graph->IsIn(BP.Get()) || Graph->Nodes.Num()>512) { return Fail(TEXT("Unique candidate-owned bounded AnimGraph required")); }
	GraphHold.Reset(Graph);
    const FGuid GraphGuid=Graph->GraphGuid;
	const int32 MaxGraphNodes=Graph->Nodes.Num()+Nodes->Num()+Links->Num()*2;
	TMap<FString,TStrongObjectPtr<UAnimationAsset>> AuthoredAssets;
	TMap<FString,TStrongObjectPtr<UPackage>> AssetPackages;
	TMap<FString,TStrongObjectPtr<UClass>> AssetClasses;
	TMap<FString,FString> AssetIdentities;
	auto Current=[&](bool ReacquireGraph=false){if(!CandidateCurrent())return false;for(const auto& A:AuthoredAssets){UPackage* AP=AssetPackages.FindChecked(A.Key).Get();if(!Live(A.Value.Get())||!Live(AP)||A.Value->GetClass()!=AssetClasses.FindChecked(A.Key).Get()||A.Value->GetOutermost()!=AP||A.Value->GetPathName()!=AssetIdentities.FindChecked(A.Key)||AP->IsDirty()||A.Value->GetSkeleton()!=Skeleton.Get())return false;}UEdGraph* Actual=FindAnimGraph(BP.Get(),GraphName);if(!CandidateCurrent()||!Live(Actual)||!Actual->IsIn(BP.Get())||Actual->GraphGuid!=GraphGuid||Actual->Nodes.Num()>MaxGraphNodes||(!ReacquireGraph&&Actual!=GraphHold.Get()))return false;for(UEdGraphNode* Node:Actual->Nodes)if(Node&&(!Live(Node)||Node->GetGraph()!=Actual))return false;if(ReacquireGraph){GraphHold.Reset(Actual);Graph=Actual;}DirtyLast=Package->IsDirty();DirtyObserved=true;return true;};
	auto FindAnyNode=[&](const FGuid& Id)->UEdGraphNode*{UEdGraphNode* Found=nullptr;for(UEdGraphNode* Node:Graph->Nodes){if(Live(Node)&&Node->NodeGuid==Id){if(Found)return nullptr;Found=Node;}}return Found;};
	auto FindNode=[&](const FGuid& Id){return Cast<UAnimGraphNode_Base>(FindAnyNode(Id));};
    TMap<FString,FGuid> Aliases; UEdGraphPin* RootInput=nullptr;
	for(UEdGraphNode* Node:Graph->Nodes)
	{
		if(!Node) { continue; }
		Aliases.Add(TEXT("existing:")+Node->NodeGuid.ToString(EGuidFormats::DigitsWithHyphens),Node->NodeGuid);
		if(Node->IsA<UAnimGraphNode_Root>()) { if(RootInput) { return Fail(TEXT("Ambiguous root")); } RootInput=Node->FindPin(TEXT("Result"),EGPD_Input); Aliases.Add(TEXT("root"),Node->NodeGuid); }
	}
	if(!RootInput) { return Fail(TEXT("Root Result pin required")); }
	if(RootInput->LinkedTo.Num()==1) { Aliases.Add(TEXT("source"),RootInput->LinkedTo[0]->GetOwningNode()->NodeGuid); }
	TMap<FString,FGuid> Created;
	TMap<FString,TStrongObjectPtr<UClass>> CreatedClasses;
	TMap<FString,TMap<FString,FString>> AuthoredSettings;
	TMap<FString,FString> AssetFields;
	for(int32 I=0;I<Nodes->Num();++I)
	{
		const auto Spec=(*Nodes)[I]->AsObject(); const FString ID=String(Spec,TEXT("id")), ClassPath=String(Spec,TEXT("class_path"));
		Enter(TEXT("load_node_class"));TStrongObjectPtr<UClass> Class(LoadClass<UAnimGraphNode_Base>(nullptr,*ClassPath));if(!Current())return Fail(TEXT("Identity changed loading node class"));if(!Live(Class.Get())||Class->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated))return Fail(TEXT("Catalog node class unavailable"));
		// Same native construction sequence as FGraphNodeCreator, with checks between callback boundaries.
		Enter(TEXT("create_node"));TStrongObjectPtr<UAnimGraphNode_Base> NodeHold(NewObject<UAnimGraphNode_Base>(Graph,Class.Get(),NAME_None,RF_Transactional));
		if(!Current()||!Live(NodeHold.Get())||NodeHold->GetGraph()!=Graph||NodeHold->GetClass()!=Class.Get())return Fail(TEXT("Node/candidate identity changed during node creation"));
		if(Graph->HasAnyFlags(RF_Transient))NodeHold->SetFlags(RF_Transient);
		Enter(TEXT("add_node"));Graph->AddNode(NodeHold.Get(),false,true);
		if(!Current()||!Live(NodeHold.Get())||NodeHold->GetGraph()!=Graph||NodeHold->GetClass()!=Class.Get()||!Graph->Nodes.Contains(NodeHold.Get()))return Fail(TEXT("Node/candidate identity changed during node insertion"));
		auto* Node=NodeHold.Get();Node->NodePosX=I*240;Node->NodePosY=300;Node->CreateNewGuid();const FGuid NodeGuid=Node->NodeGuid;
		Enter(TEXT("post_place_node"));Node->PostPlacedNewNode();if(!Current())return Fail(TEXT("Identity changed placing node"));Node=FindNode(NodeGuid);if(!Node||Node!=NodeHold.Get()||Node->GetClass()!=Class.Get())return Fail(TEXT("Placed node identity changed"));
		if(Node->Pins.IsEmpty()){Enter(TEXT("allocate_pins"));Node->AllocateDefaultPins();if(!Current())return Fail(TEXT("Identity changed allocating pins"));Node=FindNode(NodeGuid);if(!Node||Node!=NodeHold.Get())return Fail(TEXT("Node retired allocating pins"));}
		const FString AssetPath=FPackageName::ExportTextPathToObjectPath(String(Spec,TEXT("animation_asset")));
		if(!AssetPath.IsEmpty())
		{
			Enter(TEXT("load_animation_asset"));TStrongObjectPtr<UAnimationAsset> Asset(LoadObject<UAnimationAsset>(nullptr,*AssetPath));if(!Current())return Fail(TEXT("Identity changed loading animation asset"));
			if(!Asset.IsValid()||!IndependentSavedSource(Asset.Get(),Why)){return Fail(TEXT("Saved independent animation asset required"));}if(!Current())return Fail(TEXT("Identity changed auditing animation asset"));Node=FindNode(NodeGuid);
			if(!Node||Asset->GetSkeleton()!=Skeleton.Get()||!Node->IsA<UAnimGraphNode_AssetPlayerBase>()||!SupportNodeClassForAsset(Asset->GetClass(),Class.Get()))return Fail(TEXT("Compatible saved independent animation asset required"));
			AssetPackages.Add(ID,TStrongObjectPtr<UPackage>(Asset->GetOutermost()));AssetClasses.Add(ID,TStrongObjectPtr<UClass>(Asset->GetClass()));AssetIdentities.Add(ID,Asset->GetPathName());AuthoredAssets.Add(ID,TStrongObjectPtr<UAnimationAsset>(Asset.Get()));
			Enter(TEXT("set_animation_asset"));CastChecked<UAnimGraphNode_AssetPlayerBase>(Node)->SetAnimationAsset(Asset.Get());if(!Current())return Fail(TEXT("Identity changed setting animation asset"));Node=FindNode(NodeGuid);if(!Node)return Fail(TEXT("Animation node retired"));
			AssetFields.Add(ID,Class->GetName()==TEXT("AnimGraphNode_SequencePlayer")?TEXT("Sequence"):TEXT("BlendSpace"));
		}
		if(Spec->HasField(TEXT("settings")))
		{
			const TSharedPtr<FJsonObject>* Changes=nullptr; auto* Property=Node->GetFNodeProperty();
			Enter(TEXT("patch_settings"));if(!Property || !Spec->TryGetObjectField(TEXT("settings"),Changes) || !Patch(Property->Struct,Property->ContainerPtrToValuePtr<void>(Node),*Changes,Why)) { return Fail(Why.IsEmpty() ? TEXT("Invalid runtime node settings") : Why); }
			if(!Current())return Fail(TEXT("Identity changed importing settings"));Node=FindNode(NodeGuid);if(!Node)return Fail(TEXT("Settings node retired"));Property=Node->GetFNodeProperty();if(!Property)return Fail(TEXT("Runtime node layout unavailable"));
			for(const auto& Pair:(*Changes)->Values){const FString Key(Pair.Key.ToView());FProperty* Leaf=nullptr;void* Address=nullptr;if(!ResolveLeaf(Property->Struct,Property->ContainerPtrToValuePtr<void>(Node),Key,Leaf,Address))return Fail(TEXT("Authored setting disappeared"));FString Text;Leaf->ExportTextItem_Direct(Text,Address,nullptr,nullptr,PPF_None);AuthoredSettings.FindOrAdd(ID).Add(Key,Text);}
			Enter(TEXT("reconstruct_node"));Node->ReconstructNode();if(!Current())return Fail(TEXT("Identity changed reconstructing node"));Node=FindNode(NodeGuid);if(!Node||Node->GetClass()!=Class.Get())return Fail(TEXT("Reconstructed node identity lost"));
		}
		Aliases.Add(ID,NodeGuid); Created.Add(ID,NodeGuid);CreatedClasses.Add(ID,MoveTemp(Class));
	}
	bool ReplaceRoot=false; P->TryGetBoolField(TEXT("replace_root"),ReplaceRoot);
	for(const auto& Value:*Links)
	{
		if(!Value || Value->Type!=EJson::Object) { return Fail(TEXT("Link object required")); }
		const auto Link=Value->AsObject(); const FString From=String(Link,TEXT("from_node")), To=String(Link,TEXT("to_node"));
		UEdGraphNode* First=FindAnyNode(Aliases.FindRef(From)); UEdGraphNode* Second=FindAnyNode(Aliases.FindRef(To));
		if(!First || !Second || First==Second) { return Fail(TEXT("Exact distinct recipe node aliases required")); }
		UEdGraphPin* Out=First->FindPin(*String(Link,TEXT("from_pin")),EGPD_Output); UEdGraphPin* In=Second->FindPin(*String(Link,TEXT("to_pin")),EGPD_Input);
		if(!Out || !In) { return Fail(TEXT("Exact direction-matched graph pins required")); }
		if(!In->LinkedTo.IsEmpty())
		{
			if(Second->NodeGuid!=Aliases.FindRef(TEXT("root"))||In->PinName!=TEXT("Result") || !ReplaceRoot) { return Fail(TEXT("Occupied input pin refused; root replacement requires replace_root=true")); }
			Enter(TEXT("break_root_links"));Graph->GetSchema()->BreakPinLinks(*In,true);if(!Current())return Fail(TEXT("Identity changed breaking root links"));
			First=FindAnyNode(Aliases.FindRef(From));Second=FindAnyNode(Aliases.FindRef(To));Out=First?First->FindPin(*String(Link,TEXT("from_pin")),EGPD_Output):nullptr;In=Second?Second->FindPin(*String(Link,TEXT("to_pin")),EGPD_Input):nullptr;if(!Out||!In||!In->LinkedTo.IsEmpty())return Fail(TEXT("Selected pins changed breaking root links"));
		}
		TSet<FGuid> NodesBeforeConnection;for(UEdGraphNode* N:Graph->Nodes)if(N)NodesBeforeConnection.Add(N->NodeGuid);
		Enter(TEXT("connect_pins"));const bool Connected=Graph->GetSchema()->TryCreateConnection(Out,In);if(!Current())return Fail(TEXT("Identity changed creating connection"));
		if(!Connected || !Acyclic(Graph)) { return Fail(TEXT("Schema refused connection or recipe introduces a cycle")); }
		First=FindAnyNode(Aliases.FindRef(From));Second=FindAnyNode(Aliases.FindRef(To));Out=First?First->FindPin(*String(Link,TEXT("from_pin")),EGPD_Output):nullptr;In=Second?Second->FindPin(*String(Link,TEXT("to_pin")),EGPD_Input):nullptr;
		if(!Out||!In)return Fail(TEXT("Selected connection pins retired"));
		// Preserve schema-created conversion nodes rather than demanding a direct edge.
		TArray<const UEdGraphPin*> PendingOutputs{Out};TSet<const UEdGraphPin*> VisitedOutputs;bool Reached=false;
		for(int32 Q=0;Q<PendingOutputs.Num()&&VisitedOutputs.Num()<8192&&!Reached;++Q){const UEdGraphPin* Output=PendingOutputs[Q];if(VisitedOutputs.Contains(Output))continue;VisitedOutputs.Add(Output);for(const UEdGraphPin* Input:Output->LinkedTo){if(!Input||!Input->LinkedTo.Contains(const_cast<UEdGraphPin*>(Output)))return Fail(TEXT("Connection lacks reciprocal edge"));if(Input==In){Reached=true;break;}UEdGraphNode* Conversion=Input->GetOwningNode();if(Live(Conversion)&&Conversion->GetGraph()==Graph&&!NodesBeforeConnection.Contains(Conversion->NodeGuid))for(const UEdGraphPin* Pin:Conversion->Pins)if(Pin&&Pin->Direction==EGPD_Output)PendingOutputs.Add(Pin);}}
		if(!Reached)return Fail(TEXT("Schema returned success without the requested direct/conversion connection"));
	}
	// Logical authored edges survive pin reallocation and include native automatic converters.
	auto CaptureEdges=[&](TArray<FString>& Edges)->bool{if(!Current())return false;Edges.Reset();for(UEdGraphNode* Node:Graph->Nodes){if(!Node)continue;for(const UEdGraphPin* Pin:Node->Pins){if(!Pin||Pin->Direction!=EGPD_Output)continue;for(UEdGraphPin* Other:Pin->LinkedTo){if(!Other||!Live(Other->GetOwningNode())||Other->Direction!=EGPD_Input||Other->GetOwningNode()->GetGraph()!=Graph||!Other->LinkedTo.Contains(const_cast<UEdGraphPin*>(Pin))||Edges.Num()>=8192)return false;Edges.Add(Node->NodeGuid.ToString()+TEXT("|")+Pin->PinName.ToString()+TEXT("->")+Other->GetOwningNode()->NodeGuid.ToString()+TEXT("|")+Other->PinName.ToString());}}}Edges.Sort();return Current();};
	TArray<FString> AuthoredEdges;if(!CaptureEdges(AuthoredEdges))return Fail(TEXT("Unable to capture reciprocal authored graph edges"));
	Enter(TEXT("mark_structurally_modified"));R->SetBoolField(TEXT("structural_compile_entered"),true);FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(BP.Get());R->SetBoolField(TEXT("structural_compile_call_returned"),true);if(!Current(true))return Fail(TEXT("Source/candidate/graph changed during structural compile callback; full compile not dispatched"));
	FCompilerResultsLog Log;Enter(TEXT("compile_blueprint"));R->SetBoolField(TEXT("compile_attempted"),true);FKismetEditorUtilities::CompileBlueprint(BP.Get(),EBlueprintCompileOptions::SkipGarbageCollection,&Log);R->SetBoolField(TEXT("compile_call_returned"),true);
	if(!Current(true))return Fail(TEXT("Source/candidate/graph changed during full compile callback"));
	R->SetNumberField(TEXT("compile_errors"),Log.NumErrors);if(Log.NumErrors>0 || BP->Status==BS_Error || !Live(BP->GeneratedClass)) { return Fail(TEXT("Candidate AnimBP compilation/skeleton verification failed")); }
	auto VerifyAuthored=[&]()->bool
	{
		if(!Current())return false;
		for(const auto& Pair:Created){auto* Node=FindNode(Pair.Value);if(!Node||Node->GetClass()!=CreatedClasses.FindChecked(Pair.Key).Get())return false;
			if(const auto* Fields=AuthoredSettings.Find(Pair.Key)){auto* Property=Node->GetFNodeProperty();if(!Property)return false;for(const auto& Field:*Fields){FProperty* Leaf=nullptr;void* Address=nullptr;if(!ResolveLeaf(Property->Struct,Property->ContainerPtrToValuePtr<void>(Node),Field.Key,Leaf,Address))return false;FString Text;Leaf->ExportTextItem_Direct(Text,Address,nullptr,nullptr,PPF_None);if(Text!=Field.Value)return false;}}
			if(const auto* Asset=AuthoredAssets.Find(Pair.Key)){if(!Live(Asset->Get()))return false;auto* Property=Node->GetFNodeProperty();if(!Property)return false;auto* AssetProperty=FindFProperty<FObjectPropertyBase>(Property->Struct,*AssetFields.FindChecked(Pair.Key));if(!AssetProperty||AssetProperty->GetObjectPropertyValue(AssetProperty->ContainerPtrToValuePtr<void>(Property->ContainerPtrToValuePtr<void>(Node)))!=Asset->Get())return false;}
		}
		TArray<FString> Edges;if(!CaptureEdges(Edges)||Edges!=AuthoredEdges)return false;
		return Current()&&Acyclic(Graph);
	};
	Enter(TEXT("verify_authored_after_compile"));if(!VerifyAuthored())return Fail(TEXT("Authored node class/settings/asset/link differs after compilation"));
	TArray<TSharedPtr<FJsonValue>> Rows;
	for(const auto& Pair:Created)
	{
		UEdGraphNode* Found=FindNode(Pair.Value);
		if(!Found) { return Fail(TEXT("Authored node identity lost during compilation")); }
		FObject Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("id"),Pair.Key); Row->SetStringField(TEXT("node_guid"),Pair.Value.ToString(EGuidFormats::DigitsWithHyphens)); Row->SetStringField(TEXT("class"),Found->GetClass()->GetPathName()); Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	R->SetArrayField(TEXT("created_nodes"),Rows); R->SetBoolField(TEXT("compile_succeeded"),true);Enter(TEXT("publish_candidate"));PublishCandidate(BP.Get(),R);
	if(!Current())return Fail(TEXT("Source/candidate changed during publication"));if(!R->GetBoolField(TEXT("success")))return Fail(String(R,TEXT("error")));
	if(!VerifyAuthored())return Fail(TEXT("Authored state changed during publication callbacks"));R->SetBoolField(TEXT("authored_state_verified"),true);R->SetBoolField(TEXT("source_identity_unchanged"),true);R->SetBoolField(TEXT("source_package_remained_clean"),true);R->SetArrayField(TEXT("actual_call_prefix"),Prefix);R->SetStringField(TEXT("stage"),TEXT("complete"));return R;
}
}

FObject Animation(const FObject& P)
{
	return String(P,TEXT("operation"))==TEXT("authoring.animbp.catalog") ? Catalog() : Recipe(P);
}
}
