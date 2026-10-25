#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "ScopedTransaction.h"
#include "Components/PrimitiveComponent.h"
#include "Components/SkinnedMeshComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "PhysicsEngine/BodyInstance.h"
#include "PhysicsEngine/PhysicsConstraintComponent.h"
#include "PhysicsEngine/PhysicsConstraintTemplate.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "Engine/CollisionProfile.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
#if WITH_DEV_AUTOMATION_TESTS
#include "Misc/AutomationTest.h"
#include "Components/StaticMeshComponent.h"
#endif

namespace Cubeless::UE58::DomainExpansion
{
namespace
{
bool PhysicsContext()
{return IsInGameThread()&&IsValid(GEditor)&&!GEditor->PlayWorld&&!GEditor->GetPIEWorldContext()&&!GEditor->bIsSimulatingInEditor&&!IsGarbageCollecting()&&!GExitPurge&&!IsEngineExitRequested()&&!UE::IsSavingPackage();}
bool PhysicsPath(const FString& Path)
{if(Path.IsEmpty()||Path.Len()>=NAME_SIZE||!Path.StartsWith(TEXT("/")))return false;for(TCHAR C:Path)if(C==0)return false;return true;}
struct FPhysicsOwnerLease
{
 TStrongObjectPtr<UObject> Object,Outer;TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UPackage> Package;TStrongObjectPtr<AActor> Actor;
 TWeakObjectPtr<UEditorEngine> Editor;TWeakObjectPtr<UWorld> World;FString Path;
 explicit FPhysicsOwnerLease(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetOutermost()),Editor(GEditor),World(GEditor->GetEditorWorldContext().World()),Path(O->GetPathName())
 {if(auto* C=Cast<UActorComponent>(O))Actor.Reset(C->GetOwner());}
 bool Current()const
 {
  if(!PhysicsContext()||GEditor!=Editor.Get()||GEditor->GetEditorWorldContext().World()!=World.Get()||!IsValid(World.Get())||World->bIsTearingDown||!IsValid(Object.Get())||Object->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed)||Object->GetOuter()!=Outer.Get()||Object->GetClass()!=Class.Get()||Object->GetOutermost()!=Package.Get()||Object->GetPathName()!=Path)return false;
  if(auto* C=Cast<UActorComponent>(Object.Get()))return IsValid(Actor.Get())&&C->GetOwner()==Actor.Get()&&C->GetWorld()==World.Get();return true;
 }
};
FObject PhysicsUnknown(const FObject& R,const TCHAR* Stage)
{R->SetBoolField(TEXT("success"),false);R->SetBoolField(TEXT("read_only"),false);R->SetBoolField(TEXT("mutation_performed"),true);R->SetBoolField(TEXT("postcondition_verified"),false);R->SetBoolField(TEXT("change_state_known"),false);R->SetBoolField(TEXT("outcome_unknown"),true);R->SetBoolField(TEXT("do_not_retry"),true);R->SetStringField(TEXT("stage"),Stage);R->SetStringField(TEXT("error"),TEXT("Captured owner/body/constraint changed at an engine callback; no further SDK, rollback or retry."));return R;}
FObject Constraint(const FObject& P)
{
	const bool IsAsset=String(P,TEXT("operation")).EndsWith(TEXT("asset_constraint"));
	UObject* Owner=nullptr; FConstraintInstance* Instance=nullptr;TStrongObjectPtr<UPhysicsAsset> KeepAsset;
	if(IsAsset)
	{
		const FString Path=String(P,TEXT("asset_path")), Joint=String(P,TEXT("joint_name"));
		UPhysicsAsset* Asset=PhysicsPath(Path) ? LoadObject<UPhysicsAsset>(nullptr,*Path) : nullptr;KeepAsset.Reset(Asset);
		if(!Asset || Joint.IsEmpty()) { return Error(TEXT("PhysicsAsset and exact joint_name required")); }
		for(UPhysicsConstraintTemplate* Template:Asset->ConstraintSetup)
		{
			if(!Template || Template->DefaultInstance.JointName.ToString()!=Joint) { continue; }
			if(Instance) { return Error(TEXT("Ambiguous joint name")); }
			if(!Template->IsIn(Asset) || Template->GetOutermost()!=Asset->GetOutermost()) { return Error(TEXT("Externally owned constraint template refused")); }
			Owner=Template; Instance=&Template->DefaultInstance;
		}
	}
	else
	{
		auto* C=Component<UPhysicsConstraintComponent>(P); if(!C) { return Error(TEXT("Exact editor PhysicsConstraintComponent required")); }
		UPrimitiveComponent *First=nullptr,*Second=nullptr; FName FirstBone,SecondBone;
		C->GetConstrainedComponents(First,FirstBone,Second,SecondBone);
		for(UPrimitiveComponent* Body:{First,Second}) { if(Body && (Body->GetWorld()!=C->GetWorld() || Body->GetOutermost()!=C->GetOutermost())) { return Error(TEXT("Constraint references an external world/package body")); } }
		Owner=C; Instance=&C->ConstraintInstance;
	}
	if(!Owner || !Instance) { return Error(TEXT("Exact constraint not found")); }
	FPhysicsOwnerLease Lease(Owner);
	const TFunction<bool()> Current=[&](){if(!Lease.Current())return false;if(auto* C=Cast<UPhysicsConstraintComponent>(Owner))return &C->ConstraintInstance==Instance;auto* T=Cast<UPhysicsConstraintTemplate>(Owner);return T&&KeepAsset.IsValid()&&KeepAsset->ConstraintSetup.Contains(T)&&&T->DefaultInstance==Instance;};
	if(!Current())return Error(TEXT("Current exact constraint owner required"));
	const FConstraintProfileProperties BeforeProfile=Instance->ProfileInstance;
	const FString Before=StructText(FConstraintProfileProperties::StaticStruct(),&BeforeProfile);
	FObject R=Result(), Fields=MakeShared<FJsonObject>(); Describe(FConstraintProfileProperties::StaticStruct(),&BeforeProfile,Fields);
	R->SetObjectField(TEXT("properties"),Fields); R->SetStringField(TEXT("state_hash"),Hash(Before));
	R->SetStringField(TEXT("target"),Owner->GetPathName()); R->SetBoolField(TEXT("constraint_instance_valid"),Instance->IsValidConstraintInstance());
	R->SetBoolField(TEXT("runtime_application_confirmed"),false);
	if(!P->HasField(TEXT("settings"))) { return R; }
	FString Why; if(!Writable(Owner,Why)) { return Error(Why); }
	const TSharedPtr<FJsonObject>* Changes=nullptr;
	if(!P->TryGetObjectField(TEXT("settings"),Changes) || !Changes) { return Error(TEXT("settings object required")); }
	FConstraintProfileProperties Proposed=BeforeProfile;
	if(!Patch(FConstraintProfileProperties::StaticStruct(),&Proposed,*Changes,Why)) { return Error(Why); }
	FObject ProposedFields=MakeShared<FJsonObject>(); Describe(FConstraintProfileProperties::StaticStruct(),&Proposed,ProposedFields);
	R->SetObjectField(TEXT("proposed"),ProposedFields); R->SetBoolField(TEXT("dry_run"),DryRun(P));
	if(DryRun(P)) { return R; }
	if(!Expected(P,Hash(Before))) { return Error(TEXT("state_hash mismatch; inspect again")); }
	const FString Desired=StructText(FConstraintProfileProperties::StaticStruct(),&Proposed);
	if(Desired==Before) { R->SetBoolField(TEXT("no_op"),true); R->SetBoolField(TEXT("postcondition_verified"),true); return R; }
	UPackage* Package=Owner->GetOutermost(); const bool DirtyBefore=Package->IsDirty();
	FScopedTransaction Transaction(NSLOCTEXT("UnrealMCP","UE58ConstraintProfile","Configure Constraint Profile"));
	Owner->Modify();if(!Current())return PhysicsUnknown(R,TEXT("owner_modify"));if(auto* C=Cast<UActorComponent>(Owner)){C->GetOwner()->Modify();if(!Current())return PhysicsUnknown(R,TEXT("actor_modify"));}
	Instance->CopyProfilePropertiesFrom(Proposed);if(!Current())return PhysicsUnknown(R,TEXT("copy_constraint_profile"));
	const bool Verified=StructText(FConstraintProfileProperties::StaticStruct(),&Instance->ProfileInstance)==Desired;
	bool RolledBack=false;
	if(!Verified)
	{
		Instance->CopyProfilePropertiesFrom(BeforeProfile);if(!Current())return PhysicsUnknown(R,TEXT("restore_constraint_profile"));
		RolledBack=StructText(FConstraintProfileProperties::StaticStruct(),&Instance->ProfileInstance)==Before;
		if(RolledBack) { Package->SetDirtyFlag(DirtyBefore);if(!Current())return PhysicsUnknown(R,TEXT("restore_dirty"));Transaction.Cancel(); }
	}
	else { Owner->MarkPackageDirty();if(!Current())return PhysicsUnknown(R,TEXT("mark_dirty")); }
	Fields=MakeShared<FJsonObject>(); Describe(FConstraintProfileProperties::StaticStruct(),&Instance->ProfileInstance,Fields); R->SetObjectField(TEXT("properties"),Fields);
	R->SetStringField(TEXT("rollback_scope"),TEXT("Selected authored constraint profile and package dirty bit only; global callbacks and runtime physics are unverified"));MutationResult(R,Owner,Before,StructText(FConstraintProfileProperties::StaticStruct(),&Instance->ProfileInstance),DirtyBefore,Verified,RolledBack);
	return R;
}

struct FBodyFields
{
	ECollisionEnabled::Type Collision;
	FName Profile;
	FCollisionResponse Responses;
	bool CCD,Gravity;
	float Linear,Angular,Mass;
};
FBodyFields ReadBody(UPrimitiveComponent* C,FBodyInstance* B)
{
	const auto* Field=FindFProperty<FStructProperty>(FBodyInstance::StaticStruct(),TEXT("CollisionResponses"));
	FCollisionResponse Responses; if(Field) { Field->CopyCompleteValue(&Responses,Field->ContainerPtrToValuePtr<void>(B)); }
	return {B->GetCollisionEnabled(false),B->GetCollisionProfileName(),Responses,B->bUseCCD!=0,C->IsGravityEnabled(),B->LinearDamping,B->AngularDamping,B->MassScale};
}
FString BodyText(const FBodyFields& V)
{
	return V.Profile.ToString()+StructText(FCollisionResponse::StaticStruct(),&V.Responses)+FString::Printf(TEXT(":%d:%d:%d:%.9g:%.9g:%.9g"),static_cast<int32>(V.Collision),V.CCD,V.Gravity,V.Linear,V.Angular,V.Mass);
}
FObject BodyFields(const FBodyFields& V)
{
	FObject R=MakeShared<FJsonObject>();
	R->SetStringField(TEXT("collision_enabled"),StaticEnum<ECollisionEnabled::Type>()->GetNameStringByValue(V.Collision));
	R->SetStringField(TEXT("collision_profile"),V.Profile.ToString());
	R->SetBoolField(TEXT("use_ccd"),V.CCD); R->SetBoolField(TEXT("enable_gravity"),V.Gravity);
	R->SetNumberField(TEXT("linear_damping"),V.Linear); R->SetNumberField(TEXT("angular_damping"),V.Angular); R->SetNumberField(TEXT("mass_scale"),V.Mass); return R;
}
bool WriteBody(UPrimitiveComponent* C,FBodyInstance* B,const FBodyFields& V,const TFunction<bool()>& Current={})
{
	if(Current&&!Current())return false;
	// CopyRuntimeBodyInstancePropertiesFrom changes only responses/profile/mode and updates filters.
	// Unlike SetCollisionEnabled on the live body, it preserves external-profile/pending state and never recreates a body.
	if(B->GetCollisionEnabled(false)!=V.Collision)
	{
		FBodyInstance Defaults; Defaults.SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName); Defaults.SetCollisionEnabled(V.Collision,false);
		const auto* Field=FindFProperty<FStructProperty>(FBodyInstance::StaticStruct(),TEXT("CollisionResponses"));
		if(Field) { Field->CopyCompleteValue(Field->ContainerPtrToValuePtr<void>(&Defaults),&V.Responses); }
		B->CopyRuntimeBodyInstancePropertiesFrom(&Defaults);if(Current&&!Current())return false;C->UpdateOverlaps();if(Current&&!Current())return false;
	}
	B->SetUseCCD(V.CCD);if(Current&&!Current())return false;C->SetEnableGravity(V.Gravity);if(Current&&!Current())return false;
	C->SetLinearDamping(V.Linear);if(Current&&!Current())return false;C->SetAngularDamping(V.Angular);if(Current&&!Current())return false;B->SetMassScale(V.Mass);return !Current||Current();
}
FObject Body(const FObject& P)
{
	UPrimitiveComponent* C=Component<UPrimitiveComponent>(P);
	if(!C || C->IsA<USkinnedMeshComponent>() || C->IsA<UInstancedStaticMeshComponent>()) { return Error(TEXT("Exact non-skinned, non-instanced editor component required")); }
	FBodyInstance* B=C->GetBodyInstance(NAME_None,false);
	if(!B || B->WeldParent || B->OwnerComponent.Get()!=C) { return Error(TEXT("One component-owned unwelded body required")); }
	FPhysicsOwnerLease Lease(C);const TFunction<bool()> Current=[&](){return Lease.Current()&&C->GetBodyInstance(NAME_None,false)==B&&!B->WeldParent&&B->OwnerComponent.Get()==C;};
	const auto* ResponseField=FindFProperty<FStructProperty>(FBodyInstance::StaticStruct(),TEXT("CollisionResponses"));if(!Current()||!ResponseField||ResponseField->Struct!=FCollisionResponse::StaticStruct())return Error(TEXT("Current owned body and native collision-response descriptor required"));
	const FBodyFields Before=ReadBody(C,B); const FString BeforeText=BodyText(Before);
	FObject R=Result(); R->SetObjectField(TEXT("properties"),BodyFields(Before)); R->SetStringField(TEXT("state_hash"),Hash(BeforeText)); R->SetBoolField(TEXT("runtime_application_confirmed"),false);
	if(!P->HasField(TEXT("settings"))) { return R; }
	FString Why; if(!Writable(C,Why)) { return Error(Why); }
	const TSharedPtr<FJsonObject>* Settings=nullptr;
	if(!P->TryGetObjectField(TEXT("settings"),Settings) || !Settings || (*Settings)->Values.IsEmpty()) { return Error(TEXT("Nonempty settings required")); }
	FBodyFields Proposed=Before;
	for(const auto& Pair:(*Settings)->Values)
	{
		const FString Key(Pair.Key.ToView());
		if(Key==TEXT("collision_enabled"))
		{
			FString Value; if(!Pair.Value->TryGetString(Value)) { return Error(TEXT("collision_enabled requires an enum name")); }
			int64 Enum=StaticEnum<ECollisionEnabled::Type>()->GetValueByNameString(Value);
            if(Enum<0 || Enum>static_cast<int64>(ECollisionEnabled::QueryAndProbe)) { return Error(TEXT("A valid collision enabled mode is required, including ProbeOnly/QueryAndProbe")); }
			Proposed.Collision=static_cast<ECollisionEnabled::Type>(Enum);
		}
		else if(Key==TEXT("use_ccd") || Key==TEXT("enable_gravity"))
		{
			bool Value; if(Pair.Value->Type!=EJson::Boolean || !Pair.Value->TryGetBool(Value)) { return Error(TEXT("CCD/gravity require JSON booleans")); }
			if(Key==TEXT("use_ccd")) { Proposed.CCD=Value; } else { Proposed.Gravity=Value; }
		}
		else if(Key==TEXT("linear_damping") || Key==TEXT("angular_damping") || Key==TEXT("mass_scale"))
		{
			double Value; if(Pair.Value->Type!=EJson::Number || !Pair.Value->TryGetNumber(Value) || !FMath::IsFinite(Value) || Value<0 || Value>1.e6 || (Key==TEXT("mass_scale") && Value<0.001)) { return Error(TEXT("Damping must be 0..1e6; mass_scale must be 0.001..1e6")); }
			if(Key==TEXT("linear_damping")) { Proposed.Linear=static_cast<float>(Value); } else if(Key==TEXT("angular_damping")) { Proposed.Angular=static_cast<float>(Value); } else { Proposed.Mass=static_cast<float>(Value); }
		}
		else { return Error(TEXT("Unknown body setting")); }
	}
	if(Proposed.Collision!=Before.Collision)
	{
		const auto* RawProfile=FindFProperty<FNameProperty>(FBodyInstance::StaticStruct(),TEXT("CollisionProfileName"));
		const auto* Responses=FindFProperty<FStructProperty>(FBodyInstance::StaticStruct(),TEXT("CollisionResponses"));
		if(Before.Profile!=UCollisionProfile::CustomCollisionProfileName || !RawProfile || RawProfile->GetPropertyValue_InContainer(B)!=Before.Profile || !Responses || Responses->Struct!=FCollisionResponse::StaticStruct() || CollisionEnabledHasPhysics(Before.Collision)!=CollisionEnabledHasPhysics(Proposed.Collision))
		{ return Error(TEXT("Collision-mode changes require an existing custom local profile and must preserve physical-body activation; profile conversion/body recreation refused")); }
	}
	R->SetObjectField(TEXT("proposed"),BodyFields(Proposed)); R->SetBoolField(TEXT("dry_run"),DryRun(P));
	if(DryRun(P)) { return R; }
	if(!Expected(P,Hash(BeforeText))) { return Error(TEXT("state_hash mismatch; inspect again")); }
	if(BodyText(Proposed)==BeforeText) { R->SetBoolField(TEXT("no_op"),true); R->SetBoolField(TEXT("postcondition_verified"),true); return R; }
	UPackage* Package=C->GetOutermost(); const bool DirtyBefore=Package->IsDirty();
	FScopedTransaction Transaction(NSLOCTEXT("UnrealMCP","UE58BodyCollision","Configure Body Collision")); C->Modify();if(!Current())return PhysicsUnknown(R,TEXT("component_modify"));C->GetOwner()->Modify();if(!Current())return PhysicsUnknown(R,TEXT("actor_modify"));
	if(!WriteBody(C,B,Proposed,Current))return PhysicsUnknown(R,TEXT("write_body"));const bool Verified=BodyText(ReadBody(C,B))==BodyText(Proposed); bool RolledBack=false;
	if(!Verified) { if(!WriteBody(C,B,Before,Current))return PhysicsUnknown(R,TEXT("restore_body"));RolledBack=BodyText(ReadBody(C,B))==BeforeText; if(RolledBack) { Package->SetDirtyFlag(DirtyBefore);if(!Current())return PhysicsUnknown(R,TEXT("restore_body_dirty"));Transaction.Cancel(); } }
	else { C->MarkPackageDirty();if(!Current())return PhysicsUnknown(R,TEXT("mark_body_dirty")); }
	R->SetObjectField(TEXT("properties"),BodyFields(ReadBody(C,B))); R->SetStringField(TEXT("rollback_scope"),TEXT("Selected authored body fields/profile/responses and package dirty bit only; global callbacks and runtime physics are unverified"));MutationResult(R,C,BeforeText,BodyText(ReadBody(C,B)),DirtyBefore,Verified,RolledBack); return R;
}
#if WITH_DEV_AUTOMATION_TESTS
IMPLEMENT_SIMPLE_AUTOMATION_TEST(FUE58CustomBodyCollisionTest,"CubelessMCP.Domain.Authoring.CustomCollisionRoundTrip",EAutomationTestFlags::EditorContext|EAutomationTestFlags::EngineFilter)
bool FUE58CustomBodyCollisionTest::RunTest(const FString& Parameters)
{
 auto* Component=NewObject<UStaticMeshComponent>(GetTransientPackage()); auto* Body=&Component->BodyInstance;
 Body->SetCollisionProfileName(UCollisionProfile::CustomCollisionProfileName); Body->SetCollisionEnabled(ECollisionEnabled::QueryAndPhysics);
 Body->SetResponseToChannel(ECC_Camera,ECR_Ignore); const auto Before=ReadBody(Component,Body); auto Desired=Before; Desired.Collision=ECollisionEnabled::PhysicsOnly;
 WriteBody(Component,Body,Desired); TestEqual(TEXT("Custom mode applied"),BodyText(ReadBody(Component,Body)),BodyText(Desired));
 TestEqual(TEXT("Custom name preserved"),Body->GetCollisionProfileName(),UCollisionProfile::CustomCollisionProfileName); TestEqual(TEXT("Camera response preserved"),Body->GetResponseToChannel(ECC_Camera),ECR_Ignore);
 WriteBody(Component,Body,Before); TestEqual(TEXT("Complete authored collision/profile/response state restored"),BodyText(ReadBody(Component,Body)),BodyText(Before)); return true;
}
#endif
}

FObject Physics(const FObject& P)
{
	if(!PhysicsContext())return Error(TEXT("Idle Editor game thread outside PIE/GC/save required"));
	const FString Op=String(P,TEXT("operation"));
	if(Op.EndsWith(TEXT("body"))) { return Body(P); }
	if(Op.EndsWith(TEXT("constraint"))) { return Constraint(P); }
	return Error(TEXT("Unsupported physics authoring operation"));
}
}
