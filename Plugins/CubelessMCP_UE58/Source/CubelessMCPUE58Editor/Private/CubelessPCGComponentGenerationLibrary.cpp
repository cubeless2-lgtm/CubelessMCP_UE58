#include "CubelessPCGComponentGenerationLibrary.h"
#include "Compatibility/UE58/CubelessPCGComponentGenerationApi.h"

FString UCubelessPCGComponentGenerationLibrary::RefreshPCGComponents(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGComponentGeneration::RefreshComponents(ParamsJson);
}
FString UCubelessPCGComponentGenerationLibrary::BindPCGGenerationSettingsToBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::PCGComponentGeneration::BindGenerationSettings(ParamsJson);
}

// QU: internal Editor compatibility seam. Exact public C++ noarg operations preserve local delayed semantics.
#include "PCGComponent.h"
#include "Editor.h"
#include "Engine/World.h"
#include "GameFramework/Actor.h"
#include "CoreGlobals.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/SavePackage.h"
#include "UObject/StrongObjectPtr.h"
namespace
{
bool PCGSmokeNoargRunning=false;
bool PCGSmokeLive(const UObject* Object)
{return IsValid(Object)&&!Object->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
bool PCGSmokeSafe()
{return IsInGameThread()&&!GExitPurge&&!IsEngineExitRequested()&&!IsGarbageCollecting()&&!UE::IsSavingPackage()&&PCGSmokeLive(GEditor)&&!GEditor->PlayWorld&&!GEditor->bIsSimulatingInEditor;}
}
FString UCubelessPCGComponentGenerationLibrary::InvokePCGComponentNoarg(UPCGComponent* Component,const FString& MethodName)
{
    auto Receipt=MakeShared<FJsonObject>();Receipt->SetStringField(TEXT("operation"),TEXT("invoke_pcg_component_noarg"));Receipt->SetStringField(TEXT("method_name"),MethodName);
    Receipt->SetBoolField(TEXT("success"),false);Receipt->SetBoolField(TEXT("dispatch_entered"),false);Receipt->SetBoolField(TEXT("dispatch_returned"),false);Receipt->SetBoolField(TEXT("continuation_safe"),false);
    Receipt->SetBoolField(TEXT("target_alive_after_return"),false);Receipt->SetBoolField(TEXT("target_alive_observed_after_return"),false);Receipt->SetBoolField(TEXT("mutation_attempted"),false);
    Receipt->SetBoolField(TEXT("completion_proven"),false);Receipt->SetBoolField(TEXT("task_acceptance_proven"),false);Receipt->SetBoolField(TEXT("full_effect_state_known"),false);Receipt->SetBoolField(TEXT("noarg_public_cpp"),true);
    Receipt->SetBoolField(TEXT("explicit_compile_save"),false);Receipt->SetBoolField(TEXT("immediate_cleanup_substituted"),false);Receipt->SetBoolField(TEXT("forced_generation_substituted"),false);
    Receipt->SetStringField(TEXT("observation_scope"),TEXT("Exact public noarg dispatch/return and retained Editor-world/target liveness only. Python closes its actor/component/graph anchors before continuation. Void return may be a no-op; task acceptance, completion, resource/output changes and persistence are unproved."));
    Receipt->SetStringField(TEXT("error_code"),TEXT(""));Receipt->SetStringField(TEXT("error"),TEXT(""));
    const auto Encode=[&](){FString Out;FJsonSerializer::Serialize(Receipt,TJsonWriterFactory<>::Create(&Out));return Out;};
    const auto Fail=[&](const TCHAR* Code,const TCHAR* Why){Receipt->SetStringField(TEXT("error_code"),Code);Receipt->SetStringField(TEXT("error"),Why);return Encode();};
    if(!MethodName.Equals(TEXT("cleanup"),ESearchCase::CaseSensitive)&&!MethodName.Equals(TEXT("generate"),ESearchCase::CaseSensitive))return Fail(TEXT("pcg_smoke_noarg_method_unsupported"),TEXT("Only exact cleanup/generate are internal compatibility operations."));
    if(!PCGSmokeSafe()||PCGSmokeNoargRunning)return Fail(TEXT("pcg_smoke_noarg_context_unavailable"),TEXT("Idle nonreentrant Editor game thread outside GC/save/exit is required; no dispatch."));
    TGuardValue<bool> Active(PCGSmokeNoargRunning,true);TStrongObjectPtr<UEditorEngine> Editor(GEditor);TStrongObjectPtr<UPCGComponent> HeldComponent(Component);
    TStrongObjectPtr<UWorld> World(Editor->GetEditorWorldContext().World());if(!PCGSmokeLive(World.Get())||World->WorldType!=EWorldType::Editor||World->bIsTearingDown||!PCGSmokeLive(HeldComponent.Get()))return Fail(TEXT("pcg_smoke_noarg_target_unavailable"),TEXT("Existing live component in the retained Editor world required."));
    TStrongObjectPtr<AActor> Owner(HeldComponent->GetOwner());
    if(!PCGSmokeLive(Owner.Get())||Owner->GetWorld()!=World.Get()||HeldComponent->GetWorld()!=World.Get())return Fail(TEXT("pcg_smoke_noarg_world_changed"),TEXT("Retained component/owner must belong to the current Editor world."));
    // Copy diagnostic paths before the public writer. No getter on a lost target after return.
    Receipt->SetStringField(TEXT("component_path_before"),HeldComponent->GetPathName());Receipt->SetStringField(TEXT("actor_path_before"),Owner->GetPathName());
    Receipt->SetBoolField(TEXT("mutation_attempted"),true);Receipt->SetBoolField(TEXT("dispatch_entered"),true);
    if(MethodName.Equals(TEXT("cleanup"),ESearchCase::CaseSensitive))HeldComponent->Cleanup();else HeldComponent->Generate();
    Receipt->SetBoolField(TEXT("dispatch_returned"),true);
    // Cleanup's internal scoped transaction has also ended before this fence. Do not poll, retry or force completion.
    if(!PCGSmokeSafe()||GEditor!=Editor.Get()||!PCGSmokeLive(World.Get())||World->bIsTearingDown||Editor->GetEditorWorldContext().World()!=World.Get())return Fail(TEXT("pcg_smoke_noarg_return_context_unknown"),TEXT("Editor/world changed after public noarg dispatch; no more target observation or SDK."));
    const bool TargetAlive=PCGSmokeLive(HeldComponent.Get())&&PCGSmokeLive(Owner.Get());Receipt->SetBoolField(TEXT("target_alive_observed_after_return"),true);Receipt->SetBoolField(TEXT("target_alive_after_return"),TargetAlive);
    if(!TargetAlive)return Fail(TEXT("pcg_smoke_noarg_return_target_unknown"),TEXT("Retained component/owner is no longer live after dispatch; no further getter or SDK."));
    Receipt->SetBoolField(TEXT("continuation_safe"),true);Receipt->SetBoolField(TEXT("success"),true);return Encode();
}
