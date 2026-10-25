#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "Misc/AutomationTest.h"
#include "ScopedTransaction.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
#if !UE_VERSION_OLDER_THAN(5,8,0)
#include "Dataflow/DataflowObject.h"
#include "Dataflow/DataflowGraph.h"
#include "Dataflow/DataflowNodeFactory.h"
#include "Dataflow/DataflowVertexAttributeEditableNode.h"
#include "InteractiveToolChange.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Dataflow/DataflowEngine.h"
#include "ChaosClothAsset/CollectionClothFacade.h"
#endif
#endif

namespace Cubeless::UE58::DomainExpansion
{
FObject ClothGraph(const FObject& P);
#if !UE_VERSION_OLDER_THAN(5,8,0)
namespace
{
FObject SnapshotRows(FDataflowVertexAttributeEditableNode* Node,FDataflowToolNodeSnapshotSet* Set)
{
	auto R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
	const auto Snapshots=Set->GetSnapshots(); const auto* Active=Set->GetActiveSnapshot();
	for(int32 I=0;I<Snapshots.Num();++I)
	{
		const auto& S=Snapshots[I]; FObject Row=MakeShared<FJsonObject>();
		Row->SetNumberField(TEXT("index"),I); Row->SetStringField(TEXT("name"),S.Name); Row->SetStringField(TEXT("description"),S.Description);
		Row->SetBoolField(TEXT("locked"),S.IsLocked()); Row->SetBoolField(TEXT("active"),&S==Active); Row->SetStringField(TEXT("date"),S.GetDate().ToIso8601());
        Row->SetNumberField(TEXT("allocated_bytes"),static_cast<double>(S.Data.GetAllocatedSize())); Rows.Add(MakeShared<FJsonValueObject>(Row));
	}
	R->SetArrayField(TEXT("snapshots"),Rows); R->SetNumberField(TEXT("active_index"),Active ? Active-Snapshots.GetData() : INDEX_NONE);
	R->SetStringField(TEXT("state_hash"),StructBinaryHash(FDataflowToolNodeSnapshotSet::StaticStruct(),Set)); return R;
}
bool EnglishSnapshotName(const FString& Name)
{
	if(Name.IsEmpty() || Name.Len()>96) { return false; }
	for(TCHAR C:Name) { if(C>127 || (!FChar::IsAlnum(C) && C!=TEXT('_') && C!=TEXT(' ') && C!=TEXT('-'))) { return false; } }
	return !HasMCPReference(Name);
}
bool ClothHistoryContext()
{return IsInGameThread()&&IsValid(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage();}
FObject ClothHistoryUnknown(const FObject& R,const TCHAR* Stage)
{R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("read_only"),false);R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("outcome_unknown"),true);R->SetBoolField(TEXT("postcondition_verified"),false);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("stage"),Stage);R->SetStringField(TEXT("error"),TEXT("Captured Dataflow/node/snapshot role changed; no further SDK, rollback or retry."));return R;}
FObject Snapshots(const FObject& P,bool ForceFailure=false)
{
	const FString Path=String(P,TEXT("asset_path")), Name=String(P,TEXT("node_name"));
	if(!ClothHistoryContext()||Path.Len()>=NAME_SIZE)return Error(TEXT("Idle Editor and bounded asset path required"));
	for(TCHAR C:Path)if(C==0)return Error(TEXT("NUL asset path refused"));
	UDataflow* Asset=Path.StartsWith(TEXT("/")) ? LoadObject<UDataflow>(nullptr,*Path) : nullptr;TStrongObjectPtr<UDataflow> KeepAsset(Asset);
	if(!Asset || !Asset->GetDataflowGraph() || Name.IsEmpty()) { return Error(TEXT("Dataflow asset and exact node_name required")); }
	TSharedPtr<FDataflowNode> Match;
	for(const auto& N:Asset->GetDataflowGraph()->GetNodes()) { if(N && N->GetName().ToString()==Name) { if(Match) { return Error(TEXT("Ambiguous node")); } Match=N; } }
	if(!Match || !Match->TypedScriptStruct() || !Match->TypedScriptStruct()->IsChildOf(FDataflowVertexAttributeEditableNode::StaticStruct())) { return Error(TEXT("An editable vertex-attribute node is required")); }
	auto* Editable=static_cast<FDataflowVertexAttributeEditableNode*>(Match.Get());
	FStructProperty* Property=FindFProperty<FStructProperty>(Match->TypedScriptStruct(),TEXT("Snapshots"));
	if(!Property || Property->Struct!=FDataflowToolNodeSnapshotSet::StaticStruct()) { return Error(TEXT("Native snapshot-set contract unavailable")); }
	auto* Set=Property->ContainerPtrToValuePtr<FDataflowToolNodeSnapshotSet>(Match.Get());
    const auto Graph=Asset->GetDataflowGraph();const FGuid NodeGuid=Match->GetGuid();const UScriptStruct* NodeType=Match->TypedScriptStruct();
    TStrongObjectPtr<UPackage> KeepPackage(Asset->GetOutermost());TStrongObjectPtr<UClass> KeepClass(Asset->GetClass());TWeakObjectPtr<UEditorEngine> Editor(GEditor);TWeakObjectPtr<UWorld> World(GEditor->GetEditorWorldContext().World());const FString AssetPath=Asset->GetPathName();
    auto Current=[&]()
    {
        if(!ClothHistoryContext()||GEditor!=Editor.Get()||GEditor->GetEditorWorldContext().World()!=World.Get()||!IsValid(World.Get())||World->bIsTearingDown||!IsValid(Asset)||Asset->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Asset->GetClass()!=KeepClass.Get()||Asset->GetOutermost()!=KeepPackage.Get()||Asset->GetPathName()!=AssetPath||Asset->GetDataflowGraph()!=Graph||!Graph->GetNodes().Contains(Match)||Match->GetGuid()!=NodeGuid||Match->TypedScriptStruct()!=NodeType)return false;
        return FindFProperty<FStructProperty>(NodeType,TEXT("Snapshots"))==Property&&Property->ContainerPtrToValuePtr<FDataflowToolNodeSnapshotSet>(Match.Get())==Set;
    };
    if(!Current())return Error(TEXT("Current Dataflow/node/snapshot ownership required"));
    uint64 Bytes=0; for(const auto& S:Set->GetSnapshots()) { Bytes+=S.Data.GetAllocatedSize(); }
	if(Set->GetSnapshots().Num()>128 || Bytes>64u*1024u*1024u) { return Error(TEXT("Snapshot history exceeds bounded authoring budget")); }
	FObject R=SnapshotRows(Editable,Set); const FString BeforeHash=String(R,TEXT("state_hash"));
	if(BeforeHash.IsEmpty()) { return Error(TEXT("Unable to fingerprint snapshot data")); }
	const FString Action=String(P,TEXT("action")); if(Action.IsEmpty() || Action==TEXT("inspect")) { return R; }
	FString Why; if(!Writable(Asset,Why)) { return Error(Why); }
	double RawIndex=-1;
	if(!P->TryGetNumberField(TEXT("index"),RawIndex) || !FMath::IsFinite(RawIndex) || FMath::FloorToDouble(RawIndex)!=RawIndex || RawIndex<0 || RawIndex>=Set->GetSnapshots().Num()) { return Error(TEXT("Valid exact snapshot index required")); }
	const int32 Index=static_cast<int32>(RawIndex);
	const auto& Original=Set->GetSnapshots()[Index];
	if(Action!=TEXT("select") && Action!=TEXT("delete") && Action!=TEXT("rename") && Action!=TEXT("set_lock") && Action!=TEXT("duplicate")) { return Error(TEXT("Unknown snapshot action")); }
	if(Original.IsLocked() && (Action==TEXT("delete") || Action==TEXT("rename"))) { return Error(TEXT("Locked snapshots cannot be deleted or renamed")); }
	const FString NewName=String(P,TEXT("name"));
	if((Action==TEXT("rename") || Action==TEXT("duplicate")) && !EnglishSnapshotName(NewName)) { return Error(TEXT("A bounded English snapshot name is required")); }
	bool Locked=false; if(Action==TEXT("set_lock") && !P->TryGetBoolField(TEXT("locked"),Locked)) { return Error(TEXT("locked must be a boolean")); }
	int32 Evicted=INDEX_NONE;
	if(Action==TEXT("duplicate") && Set->GetSnapshots().Num()>=5)
	{
		Evicted=Set->FindOldestUnlockedSnapShot(); if(Evicted==INDEX_NONE) { return Error(TEXT("All snapshots are locked; history growth is refused")); }
	}
	R->SetStringField(TEXT("action"),Action); R->SetNumberField(TEXT("index"),Index); R->SetNumberField(TEXT("would_remove_index"),Action==TEXT("delete") ? Index : Evicted);
	R->SetBoolField(TEXT("dry_run"),DryRun(P));
	if(DryRun(P)) { return R; }
	if(!Expected(P,BeforeHash)) { return Error(TEXT("state_hash mismatch; inspect history again")); }
	const bool DirtyBefore=Asset->GetOutermost()->IsDirty(); const int32 BeforeCount=Set->GetSnapshots().Num();
	TUniquePtr<FToolCommandChange> Rollback=Editable->MakeEditNodeToolChange();if(!Current())return ClothHistoryUnknown(R,TEXT("make_native_tool_change"));
	if(!Rollback) { return Error(TEXT("Native rollback capture unavailable")); }
	FScopedTransaction Transaction(NSLOCTEXT("UnrealMCP","UE58ClothSnapshot","Edit Cloth Snapshot History"));Asset->Modify();
    if(!Current()||StructBinaryHash(FDataflowToolNodeSnapshotSet::StaticStruct(),Set)!=BeforeHash)return ClothHistoryUnknown(R,TEXT("asset_modify"));
	bool Verified=false;
	if(Action==TEXT("select")) { Set->SetActiveSnapshot(Index,true);if(!Current())return ClothHistoryUnknown(R,TEXT("select_snapshot"));Verified=Set->GetSnapshots().IsValidIndex(Index)&&Set->GetActiveSnapshot()==&Set->GetSnapshots()[Index]; }
	else if(Action==TEXT("delete")) { const bool Removed=Set->RemoveSnapshot(Index,true);if(!Current())return ClothHistoryUnknown(R,TEXT("delete_snapshot"));Verified=Removed&&Set->GetSnapshots().Num()==BeforeCount-1; }
	else if(Action==TEXT("rename")) { auto& S=const_cast<FDataflowToolNodeSnapshot&>(Set->GetSnapshots()[Index]); S.Name=NewName;Editable->Invalidate();if(!Current())return ClothHistoryUnknown(R,TEXT("rename_invalidate"));Verified=Set->GetSnapshots().IsValidIndex(Index)&&Set->GetSnapshots()[Index].Name==NewName; }
	else if(Action==TEXT("set_lock"))
	{
		auto* Field=FindFProperty<FBoolProperty>(FDataflowToolNodeSnapshot::StaticStruct(),TEXT("bLocked"));
		if(Field) { auto& S=const_cast<FDataflowToolNodeSnapshot&>(Set->GetSnapshots()[Index]); Field->SetPropertyValue_InContainer(&S,Locked);Editable->Invalidate();if(!Current())return ClothHistoryUnknown(R,TEXT("lock_invalidate"));Verified=Set->GetSnapshots().IsValidIndex(Index)&&Set->GetSnapshots()[Index].IsLocked()==Locked; }
	}
	else if(Action==TEXT("duplicate"))
	{
		// Copy before AddSnapshot can evict this very source snapshot.
		const FManagedArrayCollection Data=Set->GetSnapshots()[Index].Data;const FString Description=Set->GetSnapshots()[Index].Description;
		auto& New=Set->AddSnapshot(false);New.Data=Data;New.Name=NewName;New.Description=Description;Editable->Invalidate();if(!Current())return ClothHistoryUnknown(R,TEXT("duplicate_invalidate"));
		const auto* Actual=Set->GetActiveSnapshot();Verified=Actual&&Actual->Name==NewName&&Set->GetSnapshots().Num()==(Evicted==INDEX_NONE?BeforeCount+1:BeforeCount);
	}
	if(Verified && (Action==TEXT("rename") || Action==TEXT("set_lock") || Action==TEXT("duplicate"))) { Set->OnSnapshotSetChanged.Broadcast(*Set);if(!Current())return ClothHistoryUnknown(R,TEXT("snapshot_broadcast")); }
	Verified&=!ForceFailure; bool Restored=false;
	if(!Verified) { Rollback->Revert(Asset);if(!Current())return ClothHistoryUnknown(R,TEXT("native_revert"));Restored=StructBinaryHash(FDataflowToolNodeSnapshotSet::StaticStruct(),Set)==BeforeHash; if(Restored) { Asset->GetOutermost()->SetDirtyFlag(DirtyBefore);if(!Current())return ClothHistoryUnknown(R,TEXT("restore_dirty"));Transaction.Cancel(); } }
	else { Asset->MarkPackageDirty();if(!Current())return ClothHistoryUnknown(R,TEXT("mark_dirty")); }
	FObject After=SnapshotRows(Editable,Set);if(!Current())return ClothHistoryUnknown(R,TEXT("snapshot_readback"));After->SetBoolField(TEXT("change_state_known"),true);After->SetStringField(TEXT("rollback_scope"),TEXT("Native tool change restores the snapshot set/hash and observed package dirty bit only; global notifications and unrelated payloads are not atomic rollback")); After->SetBoolField(TEXT("read_only"),false); After->SetBoolField(TEXT("mutation_performed"),true); After->SetBoolField(TEXT("dry_run"),false); After->SetBoolField(TEXT("postcondition_verified"),Verified);
	After->SetBoolField(TEXT("package_dirty_before"),DirtyBefore); After->SetBoolField(TEXT("package_dirty_after"),Asset->GetOutermost()->IsDirty());
	if(!Verified) { After->SetBoolField(TEXT("success"),false); After->SetStringField(TEXT("error"),TEXT("Snapshot readback failed; inspect rollback evidence")); After->SetBoolField(TEXT("do_not_retry"),true); After->SetBoolField(TEXT("rollback_verified"),Restored && Asset->GetOutermost()->IsDirty()==DirtyBefore); }
	return After;
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUE58ClothHistoryAuthoringTest,"CubelessMCP.Domain.Authoring.ClothHistoryRollback",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUE58ClothHistoryAuthoringTest::RunTest(const FString& Parameters)
{
 FModuleManager::LoadModuleChecked<IModuleInterface>(TEXT("ChaosClothAssetDataflowNodes"));
 UPackage* Package=CreatePackage(*FString::Printf(TEXT("/Game/_MCP_Temp/Automation/ClothHistory_%s"),*FGuid::NewGuid().ToString(EGuidFormats::Digits)));
 UDataflow* Asset=NewObject<UDataflow>(Package,TEXT("History"),RF_Transactional);
 auto Node=UE::Dataflow::FNodeFactory::GetInstance()->NewNodeFromRegisteredType(*Asset->GetDataflowGraph(),{FGuid::NewGuid(),TEXT("FChaosClothAssetWeightMapNode"),TEXT("Weights"),Asset});
 if(!TestTrue(TEXT("Engine weight node"),Node.IsValid())) { return false; }
 TSharedRef<FManagedArrayCollection> Collection=MakeShared<FManagedArrayCollection>(); UE::Chaos::ClothAsset::FCollectionClothFacade Cloth(Collection); Cloth.DefineSchema();
 Collection->AddElements(3,TEXT("SimVertices3D")); Collection->AddElements(1,TEXT("SimFaces"));
 Cloth.GetSimPosition3D()[0]=FVector3f(0,0,0); Cloth.GetSimPosition3D()[1]=FVector3f(1,0,0); Cloth.GetSimPosition3D()[2]=FVector3f(0,1,0); Cloth.GetSimIndices3D()[0]=FIntVector(0,1,2);
 auto* Field=FindFProperty<FStructProperty>(Node->TypedScriptStruct(),TEXT("Collection")); if(!Field) { return false; }
 *Field->ContainerPtrToValuePtr<FManagedArrayCollection>(Node.Get())=*Collection;
 auto* Editable=static_cast<FDataflowVertexAttributeEditableNode*>(Node.Get()); UE::Dataflow::FEngineContext Context(Asset);
 for(int32 I=0;I<5;++I) { Editable->SetVertexAttributeValues(Context,TArray<float>{0.1f*I,0.2f,0.3f},{}); }
 FObject P=MakeShared<FJsonObject>(); P->SetStringField(TEXT("asset_path"),Asset->GetPathName()); P->SetStringField(TEXT("node_name"),TEXT("Weights"));
 auto R=Snapshots(P); if(!TestTrue(TEXT("History read"),R->GetBoolField(TEXT("success")))) { return false; }
 TestEqual(TEXT("Five bounded entries"),R->GetArrayField(TEXT("snapshots")).Num(),5);
 const FString OriginalHash=String(R,TEXT("state_hash")); Package->SetDirtyFlag(false);
 P->SetStringField(TEXT("action"),TEXT("rename")); P->SetNumberField(TEXT("index"),4); P->SetStringField(TEXT("name"),TEXT("RollbackProbe")); P->SetBoolField(TEXT("dry_run"),false); P->SetStringField(TEXT("expected_state_hash"),OriginalHash);
 R=Snapshots(P,true); TestFalse(TEXT("Injected failure reported"),R->GetBoolField(TEXT("success"))); TestTrue(TEXT("Full history restored"),R->GetBoolField(TEXT("rollback_verified"))); TestFalse(TEXT("Clean dirty flag restored"),Package->IsDirty());
 P->SetStringField(TEXT("action"),TEXT("duplicate")); P->SetBoolField(TEXT("dry_run"),true); R=Snapshots(P); TestTrue(TEXT("Dry-run exposes eviction"),R->GetIntegerField(TEXT("would_remove_index"))>=0); TestEqual(TEXT("Dry-run preserves binary state"),String(R,TEXT("state_hash")),OriginalHash);
 P->SetBoolField(TEXT("dry_run"),false); R=Snapshots(P); TestTrue(TEXT("Duplicate succeeded"),R->GetBoolField(TEXT("success"))); TestEqual(TEXT("Eviction retains five"),R->GetArrayField(TEXT("snapshots")).Num(),5);
 P->SetStringField(TEXT("action"),TEXT("set_lock")); P->SetBoolField(TEXT("locked"),true);
 for(int32 I=0;I<5;++I) { P->SetNumberField(TEXT("index"),I); P->SetStringField(TEXT("expected_state_hash"),String(R,TEXT("state_hash"))); R=Snapshots(P); if(!TestTrue(TEXT("Lock readback"),R->GetBoolField(TEXT("success")))) { return false; } }
 P->SetStringField(TEXT("action"),TEXT("duplicate")); P->SetBoolField(TEXT("dry_run"),true); TestFalse(TEXT("All-locked growth refused"),Snapshots(P)->GetBoolField(TEXT("success")));
 P->SetStringField(TEXT("action"),TEXT("delete")); TestFalse(TEXT("Locked deletion refused"),Snapshots(P)->GetBoolField(TEXT("success"))); Package->SetDirtyFlag(false); return true;
}
#endif
}
#endif

FObject Cloth(const FObject& P)
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
	if(String(P,TEXT("operation"))==TEXT("authoring.cloth.snapshots")) { return Snapshots(P); }
	return ClothGraph(P);
#else
	return Error(TEXT("Unsupported cloth authoring operation"));
#endif
}
}
