#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "ScopedTransaction.h"
#if !UE_VERSION_OLDER_THAN(5,8,0)
#include "PCGComponent.h"
#include "PCGPin.h"
#include "PCGGraphExecutionInspection.h"
#include "Data/PCGBasePointData.h"
#include "Data/PCGPointArrayData.h"
#include "Graph/DataOverride/PCGDataOverride.h"
#include "Graph/DataOverride/PCGDataOverrideHelpers.h"
#include "Graph/DataOverride/PCGDataOverridePoints.h"
#endif

namespace Cubeless::UE58::DomainExpansion
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
namespace
{
TSet<TWeakObjectPtr<UPCGComponent>> OwnedInspection;
struct FTarget
{
	FPCGSourceDataStorageKey Key;
	FPCGStack Stack;
	const UPCGNode* Node=nullptr;
	const UPCGPin* Pin=nullptr;
	TSharedPtr<FPCGInspectionData> Inspection;
};
FString KeyText(uint64 Key) { return FString::Printf(TEXT("%016llx"),Key); }
FPCGDeltaKey MatchingKey(const FPCGDeltaSettings& Settings,const FTransform& Transform,const FBox& Bounds)
{
	switch(Settings.KeyTarget)
	{
	case EPCGSpatialKeyTarget::Position: return ::PCG::DataOverride::Keys::FPCGLocationDeltaKey(Transform.GetLocation(),Settings.SpatialTolerance);
	case EPCGSpatialKeyTarget::Transform: return ::PCG::DataOverride::Keys::FPCGTransformDeltaKey(Transform,Settings.SpatialTolerance);
	case EPCGSpatialKeyTarget::AABB: return ::PCG::DataOverride::Keys::FPCGAABBDeltaKey(Bounds,Settings.SpatialTolerance);
	default: return {};
	}
}
TArray<FTarget> Targets(UPCGComponent* C)
{
	TArray<FTarget> Rows;
	const auto& Inspection=C->GetExecutionState().GetInspection();
	Inspection.ForEachExecutedNodeStack([&](const UPCGNode* Node,const auto& Stacks)
	{
		if(!Node || Rows.Num()>=256) { return false; }
		const auto* Pins=::PCG::DataOverride::Helpers::GetOverridePins(Node); if(!Pins) { return true; }
		for(const auto& Entry:Stacks) { for(const UPCGPin* Pin:*Pins)
		{
			if(!Pin || Rows.Num()>=256) { break; }
			FTarget T; T.Node=Node; T.Pin=Pin; T.Stack=Entry.Stack;
			T.Key=FPCGSourceDataStorageKey(::PCG::DataOverride::Constants::DefaultOverrideLabel,::PCG::DataOverride::Helpers::ComputePinOverrideKey(T.Stack,Node,Pin->Properties.Label).Hash);
			FPCGStack DataStack=T.Stack; DataStack.PushFrame(Node); DataStack.PushFrame(Pin);
			T.Inspection=Inspection.GetInspectionDataPtr(DataStack); Rows.Add(MoveTemp(T));
		} }
		return Rows.Num()<256;
	});
	return Rows;
}
FString CollectionHash(const FPCGDeltaCollection* Collection)
{
	return Collection ? StructBinaryHash(FPCGDeltaCollection::StaticStruct(),const_cast<FPCGDeltaCollection*>(Collection)) : TEXT("absent");
}
FObject DeltaRows(const FPCGDeltaCollection* Collection)
{
	FObject R=MakeShared<FJsonObject>();
	if(Collection) { Collection->ForEachDelta([&](const FPCGDeltaKey& Key,const TInstancedStruct<FPCGDeltaBase>& Delta)
	{
		R->SetStringField(KeyText(Key.Hash),Delta.GetScriptStruct()->GetPathName()); return R->Values.Num()<256;
	}); }
	return R;
}
FString TargetHash(UPCGComponent* C,const FTarget& T,const FPCGDeltaCollection* Collection)
{
	FString Value=C->GetPathName()+TEXT(":")+KeyText(T.Key.Hash)+TEXT(":")+CollectionHash(Collection)+
		FString::Printf(TEXT(":%llu:%u"),C->GetExecutionState().GetInspection().GetExecutedStacksGeneration(),C->GetExecutionState().GetSourceDataContainer()->GetDirtyGeneration());
	if(T.Inspection && T.Inspection->Data) { for(const auto& Data:T.Inspection->Data->TaggedData) { if(Data.Data) { Value+=Data.Data->GetPathName()+FString::Printf(TEXT(":%u"),Data.Data->GetOrComputeCrc(true).GetValue()); } } }
	return Hash(Value);
}
bool ReadTransform(const FObject& P,const TCHAR* Key,FTransform& Transform)
{
	const FString Text=String(P,Key); if(Text.IsEmpty() || Text.Len()>1024) { return false; }
	FStringOutputDevice Errors; const TCHAR* End=TBaseStructure<FTransform>::Get()->ImportText(*Text,&Transform,nullptr,PPF_None,&Errors,TEXT("Transform"));
	return End && FString(End).TrimStartAndEnd().IsEmpty() && Errors.IsEmpty() && !Transform.ContainsNaN() &&
		Transform.GetTranslation().GetAbsMax()<1.e9 && Transform.GetScale3D().GetAbsMax()<1.e6 && Transform.GetRotation().IsNormalized();
}
FObject Manual(const FObject& P)
{
	UPCGComponent* C=Component<UPCGComponent>(P); if(!C) { return Error(TEXT("Exact editor PCG component required")); }
	if(C->IsGenerating() || C->IsCleaningUp()) { return Error(TEXT("PCG execution is in flight; wait for completion")); }
	const FString Op=String(P,TEXT("operation"));
	if(Op==TEXT("authoring.pcg.inspection"))
	{
		FString Why; if(!Writable(C,Why)) { return Error(Why); }
		bool Enabled; if(!P->TryGetBoolField(TEXT("enabled"),Enabled)) { return Error(TEXT("enabled must be boolean")); }
		for(auto It=OwnedInspection.CreateIterator();It;++It) { if(!It->IsValid()) { It.RemoveCurrent(); } }
		FObject R=Result(); R->SetBoolField(TEXT("dry_run"),DryRun(P)); R->SetBoolField(TEXT("owned_reference_before"),OwnedInspection.Contains(C));
		if(!DryRun(P))
		{
			if(Enabled && !OwnedInspection.Contains(C)) { C->GetExecutionState().GetInspection().EnableInspection(); OwnedInspection.Add(C); }
			else if(!Enabled && OwnedInspection.Contains(C)) { C->GetExecutionState().GetInspection().DisableInspection(); OwnedInspection.Remove(C); }
			R->SetBoolField(TEXT("read_only"),false); R->SetBoolField(TEXT("mutation_performed"),R->GetBoolField(TEXT("owned_reference_before"))!=OwnedInspection.Contains(C));
		}
		R->SetBoolField(TEXT("inspection_enabled"),C->GetExecutionState().GetInspection().IsInspecting()); R->SetBoolField(TEXT("owned_reference_after"),OwnedInspection.Contains(C)); return R;
	}
	FPCGSourceDataContainer* Container=C->GetExecutionState().GetSourceDataContainer(); if(!Container) { return Error(TEXT("PCG source container unavailable")); }
	const auto All=Targets(C); const FString TargetKey=String(P,TEXT("target_key"));
    if(Op==TEXT("authoring.pcg.edit") && TargetKey.IsEmpty()) return Error(TEXT("Editing requires an exact nonempty target_key; no delta was written"));
	if(TargetKey.IsEmpty())
	{
		FObject R=Result(); TArray<TSharedPtr<FJsonValue>> Rows;
		for(const auto& T:All)
		{
			auto Stored=Container->Get<FPCGDeltaCollection>(T.Key); const auto* Collection=Stored.IsValid() ? Stored.GetPtr<FPCGDeltaCollection>() : nullptr;
			FObject Row=MakeShared<FJsonObject>(); Row->SetStringField(TEXT("target_key"),KeyText(T.Key.Hash)); Row->SetStringField(TEXT("node_path"),T.Node->GetPathName()); Row->SetStringField(TEXT("pin"),T.Pin->Properties.Label.ToString());
			Row->SetStringField(TEXT("state_hash"),TargetHash(C,T,Collection)); Row->SetObjectField(TEXT("deltas"),DeltaRows(Collection));
			TArray<TSharedPtr<FJsonValue>> DataRows;
			if(T.Inspection && T.Inspection->Data) { for(int32 D=0;D<T.Inspection->Data->TaggedData.Num() && D<16;++D)
			{
				const auto* Points=Cast<UPCGBasePointData>(T.Inspection->Data->TaggedData[D].Data);
				if(!Points) { continue; }
				FObject DataRow=MakeShared<FJsonObject>(); DataRow->SetNumberField(TEXT("data_index"),D); DataRow->SetNumberField(TEXT("point_count"),Points->GetNumPoints());
				TArray<TSharedPtr<FJsonValue>> Samples; for(int32 I=0;I<Points->GetNumPoints() && I<8;++I) { FObject Point=MakeShared<FJsonObject>(); Point->SetNumberField(TEXT("point_index"),I); Point->SetStringField(TEXT("transform"),StructText(TBaseStructure<FTransform>::Get(),&Points->GetTransform(I))); Samples.Add(MakeShared<FJsonValueObject>(Point)); }
				DataRow->SetArrayField(TEXT("point_samples"),Samples); DataRows.Add(MakeShared<FJsonValueObject>(DataRow));
			} }
			Row->SetArrayField(TEXT("data"),DataRows); Rows.Add(MakeShared<FJsonValueObject>(Row));
		}
		R->SetArrayField(TEXT("targets"),Rows); R->SetBoolField(TEXT("inspection_enabled"),C->GetExecutionState().GetInspection().IsInspecting()); R->SetBoolField(TEXT("truncated"),All.Num()==256); return R;
	}
	FString Why; if(!Writable(C,Why)) { return Error(Why); }
	const FTarget* T=nullptr; for(const auto& Row:All) { if(KeyText(Row.Key.Hash)==TargetKey) { if(T) { return Error(TEXT("Ambiguous storage key")); } T=&Row; } }
	if(!T) { return Error(TEXT("Exact target_key from current inspection required")); }
	auto Stored=Container->Get<FPCGDeltaCollection>(T->Key); const auto* Current=Stored.IsValid() ? Stored.GetPtr<FPCGDeltaCollection>() : nullptr;
	const FString BeforeHash=TargetHash(C,*T,Current); FPCGDeltaCollection Proposed=Current ? *Current : FPCGDeltaCollection();
	if(!Current) { Proposed.Settings.KeyPolicy=EPCGDataOverrideKeyPolicy::Spatial; Proposed.Settings.KeyTarget=EPCGSpatialKeyTarget::Position; Proposed.Settings.SpatialTolerance=10.0; }
    if(Proposed.Settings.KeyPolicy!=EPCGDataOverrideKeyPolicy::Spatial) { return Error(TEXT("Only spatial point override collections are supported")); }
	bool Supported=true; Proposed.ForEachDelta([&](const auto&,const TInstancedStruct<FPCGDeltaBase>& Delta) { const auto* Type=Delta.GetScriptStruct(); Supported=Type==FPCGPointTransformDelta::StaticStruct() || Type==FPCGPointTransformOffsetDelta::StaticStruct() || Type==FPCGPointDeletionDelta::StaticStruct() || Type==FPCGPointInsertionDelta::StaticStruct(); return Supported; });
	if(!Supported || Proposed.Num()>256) { return Error(TEXT("Only bounded engine point deltas can be edited")); }
	const FString Action=String(P,TEXT("action"));
    if(!TSet<FString>{TEXT("transform"),TEXT("offset"),TEXT("delete"),TEXT("insert"),TEXT("remove"),TEXT("clear")}.Contains(Action)) return Error(TEXT("Unsupported point override action; no delta was written"));
	if(Action==TEXT("clear")) { Proposed.Empty(); }
	else if(Action==TEXT("remove"))
	{
		FPCGDeltaKey Key; bool Found=false; Proposed.ForEachDelta([&](const FPCGDeltaKey& K,const auto&) { if(KeyText(K.Hash)==String(P,TEXT("delta_key"))) { Key=K; Found=true; return false; } return true; });
		if(!Found || !Proposed.Remove(Key)) { return Error(TEXT("Existing exact delta_key required")); }
	}
	else
	{
		FTransform Desired;
		if(Action!=TEXT("delete") && !ReadTransform(P,TEXT("transform"),Desired)) { return Error(TEXT("Canonical finite Unreal transform export text required")); }
		if(Action==TEXT("insert"))
		{
			if(!T->Inspection || !T->Inspection->Data || T->Inspection->Data->TaggedData.Num()!=1 || !Cast<UPCGPointArrayData>(T->Inspection->Data->TaggedData[0].Data)) { return Error(TEXT("Insertion requires exactly one inspected engine point-array data entry")); }
			const FString Label=String(P,TEXT("delta_key")); if(Label.IsEmpty() || Label.Len()>96) { return Error(TEXT("Insert requires a unique bounded delta_key label")); }
			const FPCGDeltaKey Key=::PCG::DataOverride::Keys::FPCGLabelDeltaKey(*Label);
			if(Proposed.Contains(Key)) { return Error(TEXT("Delta key exists; remove it explicitly before replacement")); }
			FPCGPointInsertionDelta Delta; Delta.InsertedPoints.Add(FPCGPoint(Desired,1.f,42)); Proposed.Add(Key,TInstancedStruct<FPCGPointInsertionDelta>::Make(Delta));
		}
		else
		{
			if(Action!=TEXT("transform") && Action!=TEXT("offset") && Action!=TEXT("delete")) { return Error(TEXT("Unknown manual point operation")); }
			double DataIndex=-1,PointIndex=-1;
			if(!P->TryGetNumberField(TEXT("data_index"),DataIndex) || !P->TryGetNumberField(TEXT("point_index"),PointIndex) || DataIndex<0 || DataIndex>=16 || FMath::FloorToDouble(DataIndex)!=DataIndex || PointIndex<0 || FMath::FloorToDouble(PointIndex)!=PointIndex || !T->Inspection || !T->Inspection->Data || !T->Inspection->Data->TaggedData.IsValidIndex(static_cast<int32>(DataIndex))) { return Error(TEXT("Exact inspected data_index/point_index required")); }
			const auto* Points=Cast<UPCGPointArrayData>(T->Inspection->Data->TaggedData[static_cast<int32>(DataIndex)].Data);
			int32 PointSources=0; for(const auto& Tagged:T->Inspection->Data->TaggedData) { PointSources+=Cast<UPCGBasePointData>(Tagged.Data)!=nullptr; }
			if(PointSources!=1) { return Error(TEXT("A single point-data source is required to avoid cross-data spatial ambiguity")); }
			if(!Points || PointIndex>=Points->GetNumPoints() || Points->GetNumPoints()>100000) { return Error(TEXT("Point index/data outside bounded budget")); }
			const FTransform Original=Points->GetTransform(static_cast<int32>(PointIndex)); const FBox Bounds=Points->GetLocalBounds(static_cast<int32>(PointIndex)).TransformBy(Original);
            const auto MatchKey=MatchingKey(Proposed.Settings,Original,Bounds); int32 Matches=0;
            for(int32 I=0;I<Points->GetNumPoints();++I) { if(MatchingKey(Proposed.Settings,Points->GetTransform(I),Points->GetLocalBounds(I).TransformBy(Points->GetTransform(I)))==MatchKey) { ++Matches; } }
			if(Matches!=1) { return Error(TEXT("Spatial key is ambiguous; no override was written")); }
			bool AlreadyEdited=false;
			Proposed.ForEachDelta([&](const auto&,const TInstancedStruct<FPCGDeltaBase>& Entry)
			{
				if(const auto* Delta=Entry.GetPtr<FPCGPointDeltaBase>()) { AlreadyEdited|=Delta->OriginalTransform.Equals(Original); }
				if(const auto* Delta=Entry.GetPtr<FPCGPointTransformDelta>()) { AlreadyEdited|=Delta->TransformOverride.Equals(Original); }
				if(Entry.GetPtr<FPCGPointTransformOffsetDelta>()) { AlreadyEdited=true; }
				if(const auto* Delta=Entry.GetPtr<FPCGPointInsertionDelta>()) { for(const auto& Point:Delta->InsertedPoints) { AlreadyEdited|=Point.Transform.Equals(Original); } }
				return !AlreadyEdited;
			});
			if(AlreadyEdited) { return Error(TEXT("Point already has an override; remove its delta and regenerate before reauthoring")); }
			const FName Label=Action==TEXT("transform") ? FPCGPointTransformDelta::GetDeltaNameStatic() : (Action==TEXT("offset") ? FPCGPointTransformOffsetDelta::GetDeltaNameStatic() : ::PCG::DataOverride::Points::Constants::PointDeleteDeltaName);
			const FPCGDeltaKey Key=::PCG::DataOverride::Keys::FPCGCompositeTransformDeltaKey(Original,Proposed.Settings.SpatialTolerance,true,true,true,Label);
			if(Proposed.Contains(Key)) { return Error(TEXT("Delta key collision; explicit removal required")); }
			if(Action==TEXT("transform")) { FPCGPointTransformDelta Delta; Delta.OriginalTransform=Original; Delta.Bounds=Bounds; Delta.TransformOverride=Desired; Proposed.Add(Key,TInstancedStruct<FPCGPointTransformDelta>::Make(Delta)); }
			else if(Action==TEXT("offset")) { FPCGPointTransformOffsetDelta Delta; Delta.OriginalTransform=Original; Delta.Bounds=Bounds; Delta.TransformOffset=Desired; Proposed.Add(Key,TInstancedStruct<FPCGPointTransformOffsetDelta>::Make(Delta)); }
			else { FPCGPointDeletionDelta Delta; Delta.OriginalTransform=Original; Delta.Bounds=Bounds; Proposed.Add(Key,TInstancedStruct<FPCGPointDeletionDelta>::Make(Delta)); }
		}
	}
	if(Proposed.Num()>256) { return Error(TEXT("Delta count exceeds the bounded budget")); }
	FObject R=Result(); R->SetStringField(TEXT("state_hash"),BeforeHash); R->SetObjectField(TEXT("proposed_deltas"),DeltaRows(&Proposed)); R->SetBoolField(TEXT("dry_run"),DryRun(P));
	if(DryRun(P)) { return R; }
	if(!Expected(P,BeforeHash)) { return Error(TEXT("Source generation/data/container state changed; inspect again")); }
	const FPCGSourceDataContainer Before=*Container; const bool DirtyBefore=C->GetOutermost()->IsDirty();
    TStrongObjectPtr<UPCGComponent> ComponentHold(C);TStrongObjectPtr<AActor> ActorHold(C->GetOwner());TStrongObjectPtr<UPackage> PackageHold(C->GetPackage());
    const FString ComponentPath=C->GetPathName();const auto InspectionGeneration=C->GetExecutionState().GetInspection().GetExecutedStacksGeneration();
    auto CurrentOwner=[&]{return SafeContext()&&Live(ComponentHold.Get())&&Live(ActorHold.Get())&&Live(PackageHold.Get())&&C->GetOwner()==ActorHold.Get()&&C->GetPackage()==PackageHold.Get()&&C->GetPathName()==ComponentPath&&!C->IsGenerating()&&!C->IsCleaningUp()&&C->GetExecutionState().GetSourceDataContainer()==Container&&C->GetExecutionState().GetInspection().GetExecutedStacksGeneration()==InspectionGeneration;};
    auto Unknown=[&]{auto X=Error(TEXT("PCG component/container/inspection changed during callback; stop without follow-up writer"));X->SetBoolField(TEXT("change_state_known"),false);X->SetBoolField(TEXT("mutation_performed"),true);X->SetBoolField(TEXT("read_only"),false);X->SetBoolField(TEXT("do_not_retry"),true);X->SetStringField(TEXT("stage"),TEXT("pcg_mutation_callback"));return X;};
	FScopedTransaction Transaction(NSLOCTEXT("CubelessDomainExpansion","UE58PCGManual","Edit PCG Manual Point Override")); C->Modify();if(!CurrentOwner())return Unknown();C->GetOwner()->Modify();if(!CurrentOwner())return Unknown();
	Container->Store(T->Key,Proposed); Container->MarkDirty();if(!CurrentOwner())return Unknown();
	auto After=Container->Get<FPCGDeltaCollection>(T->Key); const bool Verified=After.IsValid() && CollectionHash(After.GetPtr<FPCGDeltaCollection>())==CollectionHash(&Proposed);
	bool Restored=false;
	if(!Verified) { *Container=Before; Container->MarkDirty(); auto Back=Container->Get<FPCGDeltaCollection>(T->Key); Restored=CollectionHash(Back.IsValid() ? Back.GetPtr<FPCGDeltaCollection>() : nullptr)==CollectionHash(Current); if(Restored) { C->GetOutermost()->SetDirtyFlag(DirtyBefore); Transaction.Cancel(); } }
	else { C->MarkPackageDirty();if(!CurrentOwner())return Unknown(); }
	R->SetBoolField(TEXT("read_only"),false); R->SetBoolField(TEXT("mutation_performed"),true); R->SetBoolField(TEXT("postcondition_verified"),Verified); R->SetBoolField(TEXT("requires_regeneration"),true); R->SetBoolField(TEXT("runtime_application_confirmed"),false);
	auto Observed=Container->Get<FPCGDeltaCollection>(T->Key);
    R->SetBoolField(TEXT("package_dirty_before"),DirtyBefore); R->SetBoolField(TEXT("package_dirty_after"),C->GetOutermost()->IsDirty()); R->SetStringField(TEXT("state_hash"),TargetHash(C,*T,Observed.IsValid()?Observed.GetPtr<FPCGDeltaCollection>():nullptr));
	if(!Verified) { R->SetBoolField(TEXT("success"),false); R->SetStringField(TEXT("error"),TEXT("Readback failed; cache generation invalidated during rollback")); R->SetBoolField(TEXT("do_not_retry"),true); R->SetBoolField(TEXT("rollback_verified"),Restored); }
	return R;
}
}
#endif

void Shutdown()
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
	for(const auto& Weak:OwnedInspection) { if(UPCGComponent* C=Weak.Get()) { if(C->GetExecutionState().GetInspection().IsInspecting()) { C->GetExecutionState().GetInspection().DisableInspection(); } } }
	OwnedInspection.Empty();
#endif
}
FObject PCG(const FObject& P)
{
#if !UE_VERSION_OLDER_THAN(5,8,0)
	return Manual(P);
#else
	return Error(TEXT("PCG data override API requires UE5.8"));
#endif
}
}
