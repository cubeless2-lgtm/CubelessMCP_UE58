#include "Compatibility/UE58/CubelessPIEInputApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Editor.h"
#include "Engine/Engine.h"
#include "Engine/World.h"
#include "Engine/GameInstance.h"
#include "Engine/LocalPlayer.h"
#include "GameFramework/PlayerController.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/InputDeviceLibrary.h"
#include "InputKeyEventArgs.h"
#include "UnrealEngine.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "Dom/JsonObject.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
namespace Cubeless::UE58::PIEInput {
namespace {
using Variable::FResult;
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_BeginDestroyed|RF_FinishDestroyed);}
struct FIdentity {
 TWeakObjectPtr<UObject> Object,Outer;TWeakObjectPtr<UClass> Class;TWeakObjectPtr<UPackage> Package;FString Path;
 explicit FIdentity(UObject* O):Object(O),Outer(O->GetOuter()),Class(O->GetClass()),Package(O->GetPackage()),Path(O->GetPathName()){}
 bool Exact()const{auto* O=Object.Get();return Live(O)&&O->GetOuter()==Outer.Get()&&O->GetClass()==Class.Get()&&O->GetPackage()==Package.Get()&&O->GetPathName()==Path;}
};
struct FWork {
 FResult R;UEditorEngine* Editor=GEditor;UEngine* Engine=GEngine;TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>();
 FWork(){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("gameplay_effect_verified"),false);R.Data->SetBoolField(TEXT("OS_input_injected"),false);R.Data->SetStringField(TEXT("known_scope"),TEXT("One simulated controller InputKey dispatch and immediate scoped metadata; not next-tick gameplay, multi-player isolation, or packaged execution proof."));}
 bool Safe()const{return IsInGameThread()&&!IsGarbageCollecting()&&!IsEngineExitRequested()&&!GExitPurge&&!GIsSavingPackage&&GEditor==Editor&&GEngine==Engine&&Live(GEditor)&&Live(GEngine);}
 TSharedPtr<FJsonObject> Bad(const FString& Error){if(R.Error.IsEmpty()){R.ErrorCode=TEXT("pie_input_failed");R.Error=Error;}auto O=MakeShared<FJsonObject>();O->SetBoolField(TEXT("success"),false);O->SetStringField(TEXT("error"),Error);return O;}
};
TSharedPtr<FJsonObject> Handle(const TSharedPtr<FJsonObject>& Params,FWork& W)
{
    if (!IsInGameThread())
    {
        return W.Bad(TEXT("inject_pie_input_key must execute on the game thread"));
    }
    if (!GEditor || !GEngine)
    {
        return W.Bad(TEXT("The Unreal Editor and engine must be available"));
    }
    if (GEditor->bIsSimulatingInEditor)
    {
        return W.Bad(
            TEXT("inject_pie_input_key requires Play In Editor; Simulate In Editor is not supported"));
    }
    if (!Params.IsValid())
    {
        return W.Bad(TEXT("Missing parameters"));
    }

    FString RequestedKeyName;
    if (!Params->TryGetStringField(TEXT("key"), RequestedKeyName))
    {
        return W.Bad(TEXT("Missing required string parameter 'key'"));
    }
    RequestedKeyName.TrimStartAndEndInline();
    if (RequestedKeyName.IsEmpty())
    {
        return W.Bad(TEXT("Parameter 'key' must not be empty"));
    }

    const FKey Key{FName(*RequestedKeyName)};
    if (!Key.IsValid())
    {
        return W.Bad(
            FString::Printf(TEXT("Unknown Unreal input key: %s"), *RequestedKeyName));
    }

    FString RequestedEvent;
    if (!Params->TryGetStringField(TEXT("event"), RequestedEvent))
    {
        return W.Bad(TEXT("Missing required string parameter 'event'"));
    }
    RequestedEvent.TrimStartAndEndInline();
    RequestedEvent.ToLowerInline();

    EInputEvent InputEvent = IE_MAX;
    if (RequestedEvent == TEXT("pressed"))
    {
        InputEvent = IE_Pressed;
    }
    else if (RequestedEvent == TEXT("released"))
    {
        InputEvent = IE_Released;
    }
    else
    {
        return W.Bad(
            TEXT("Parameter 'event' must be either 'pressed' or 'released'"));
    }

    int32 RequestedPIEInstance = INDEX_NONE;
    const bool bHasRequestedPIEInstance = Params->HasField(TEXT("pie_instance"));
    if (bHasRequestedPIEInstance)
    {
        double RequestedPIEInstanceNumber = 0.0;
        if (!Params->TryGetNumberField(TEXT("pie_instance"), RequestedPIEInstanceNumber) ||
            !FMath::IsFinite(RequestedPIEInstanceNumber) ||
            RequestedPIEInstanceNumber < 0.0 ||
            RequestedPIEInstanceNumber > static_cast<double>(TNumericLimits<int32>::Max()) ||
            RequestedPIEInstanceNumber != FMath::TruncToDouble(RequestedPIEInstanceNumber))
        {
            return W.Bad(
                TEXT("Optional parameter 'pie_instance' must be a non-negative integer"));
        }
        RequestedPIEInstance = static_cast<int32>(RequestedPIEInstanceNumber);
    }

    struct FPIEInputCandidate
    {
        UWorld* World = nullptr;
        ULocalPlayer* LocalPlayer = nullptr;
        APlayerController* PlayerController = nullptr;
        int32 PIEInstance = INDEX_NONE;
        int32 LocalPlayerIndex = INDEX_NONE;
    };

    TArray<FPIEInputCandidate> Candidates;
    TSet<APlayerController*> SeenControllers;
    int32 MatchingPIEContextCount = 0;
    int32 BegunPlayPIEContextCount = 0;

    struct FContextSnapshot { EWorldType::Type WorldType; int32 PIEInstance; TStrongObjectPtr<UWorld> Hold; UWorld* World() const { return Hold.Get(); } };
    TArray<FContextSnapshot> Contexts;
    if(GEngine->GetWorldContexts().Num()>64)return W.Bad(TEXT("PIE world-context budget exceeded"));
    for(const FWorldContext& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE)Contexts.Add({C.WorldType,C.PIEInstance,TStrongObjectPtr<UWorld>(C.World())});
    W.Counts->SetNumberField(TEXT("PIE_contexts_captured"),Contexts.Num());
    for (const FContextSnapshot& WorldContext : Contexts)
    {
        if (WorldContext.WorldType != EWorldType::PIE)
        {
            continue;
        }
        if (bHasRequestedPIEInstance && WorldContext.PIEInstance != RequestedPIEInstance)
        {
            continue;
        }

        ++MatchingPIEContextCount;
        UWorld* World = WorldContext.World();
        if (!IsValid(World) || !World->HasBegunPlay())
        {
            continue;
        }

        ++BegunPlayPIEContextCount;
        UGameInstance* GameInstance = World->GetGameInstance();
        if (!IsValid(GameInstance))
        {
            continue;
        }

        const TArray<ULocalPlayer*> LocalPlayers = GameInstance->GetLocalPlayers();
        if(LocalPlayers.Num()>64)return W.Bad(TEXT("PIE local-player budget exceeded"));
        TStrongObjectPtr<UGameInstance> CapturedInstance(GameInstance);
        for (int32 LocalPlayerIndex = 0; LocalPlayerIndex < LocalPlayers.Num(); ++LocalPlayerIndex)
        {
            ULocalPlayer* LocalPlayer = LocalPlayers[LocalPlayerIndex];
            TStrongObjectPtr<ULocalPlayer> CapturedLocal(LocalPlayer);
            APlayerController* PlayerController =
                IsValid(LocalPlayer) ? LocalPlayer->GetPlayerController(World) : nullptr;
            TStrongObjectPtr<APlayerController> CapturedController(PlayerController);
            if(!Live(PlayerController))continue;
            const bool bLocalCandidate=PlayerController->IsLocalController();
            if(!W.Safe())return W.Bad(TEXT("Editor/GC/exit context changed in IsLocalController; no dispatch"));
            if(!Live(World)||World->bIsTearingDown||!Live(GameInstance)||World->GetGameInstance()!=GameInstance||!Live(LocalPlayer)||!Live(PlayerController)||PlayerController->GetWorld()!=World)return W.Bad(TEXT("PIE candidate role changed in IsLocalController; no dispatch"));
            const auto& CurrentPlayers=GameInstance->GetLocalPlayers();
            if(!CurrentPlayers.IsValidIndex(LocalPlayerIndex)||CurrentPlayers[LocalPlayerIndex]!=LocalPlayer||LocalPlayer->GetPlayerController(World)!=PlayerController)return W.Bad(TEXT("Local player membership changed in IsLocalController; no dispatch"));
            if (!bLocalCandidate || SeenControllers.Contains(PlayerController))
            {
                continue;
            }

            SeenControllers.Add(PlayerController);
            FPIEInputCandidate& Candidate = Candidates.AddDefaulted_GetRef();
            Candidate.World = World;
            Candidate.LocalPlayer = LocalPlayer;
            Candidate.PlayerController = PlayerController;
            Candidate.PIEInstance = WorldContext.PIEInstance;
            Candidate.LocalPlayerIndex = LocalPlayerIndex;
        }
    }

    if (MatchingPIEContextCount == 0)
    {
        return W.Bad(
            bHasRequestedPIEInstance
                ? FString::Printf(TEXT("No PIE world context exists for pie_instance %d"), RequestedPIEInstance)
                : TEXT("No PIE world context is active"));
    }
    if (BegunPlayPIEContextCount == 0)
    {
        return W.Bad(
            bHasRequestedPIEInstance
                ? FString::Printf(TEXT("PIE world for pie_instance %d has not begun play"), RequestedPIEInstance)
                : TEXT("No active PIE world has begun play"));
    }
    if (Candidates.IsEmpty())
    {
        return W.Bad(
            bHasRequestedPIEInstance
                ? FString::Printf(
                    TEXT("No local player controller is available in begun-play pie_instance %d"),
                    RequestedPIEInstance)
                : TEXT("No local player controller is available in any begun-play PIE world"));
    }
    if (Candidates.Num() > 1)
    {
        TArray<FString> CandidateDescriptions;
        CandidateDescriptions.Reserve(Candidates.Num());
        for (const FPIEInputCandidate& Candidate : Candidates)
        {
            CandidateDescriptions.Add(FString::Printf(
                TEXT("pie_instance=%d local_player=%d controller=%s"),
                Candidate.PIEInstance,
                Candidate.LocalPlayerIndex,
                *GetNameSafe(Candidate.PlayerController)));
        }
        const FString ResolutionHint = bHasRequestedPIEInstance
            ? TEXT("The requested PIE instance must contain exactly one local player controller.")
            : TEXT("Specify a PIE instance that contains exactly one local player controller.");
        return W.Bad(FString::Printf(
            TEXT("PIE input target is ambiguous (%d local controllers): %s %s"),
            Candidates.Num(),
            *FString::Join(CandidateDescriptions, TEXT("; ")),
            *ResolutionHint));
    }

    const FPIEInputCandidate& Target = Candidates[0];
    TStrongObjectPtr<UWorld> HoldWorld(Target.World);
    TStrongObjectPtr<ULocalPlayer> HoldPlayer(Target.LocalPlayer);
    TStrongObjectPtr<APlayerController> HoldController(Target.PlayerController);
    UGameInstance* TargetInstance=Target.World->GetGameInstance();
    TStrongObjectPtr<UGameInstance> HoldInstance(TargetInstance);
    TArray<FIdentity> Identities;
    for(UObject* O:TArray<UObject*>{Target.World,Target.LocalPlayer,Target.PlayerController,TargetInstance}){if(!Live(O))return W.Bad(TEXT("Selected PIE object unavailable"));Identities.Emplace(O);}
    auto Exact=[&](){if(!W.Safe()||GEditor->bIsSimulatingInEditor)return false;for(const auto& I:Identities)if(!I.Exact())return false;if(!Live(Target.World)||Target.World->bIsTearingDown||!Target.World->HasBegunPlay()||Target.World->GetGameInstance()!=TargetInstance||!Live(TargetInstance))return false;const auto& Players=TargetInstance->GetLocalPlayers();if(!Players.IsValidIndex(Target.LocalPlayerIndex)||Players[Target.LocalPlayerIndex]!=Target.LocalPlayer||Target.LocalPlayer->GetPlayerController(Target.World)!=Target.PlayerController||Target.PlayerController->GetWorld()!=Target.World)return false;int32 Matches=0;for(const auto& C:GEngine->GetWorldContexts())if(C.WorldType==EWorldType::PIE&&C.PIEInstance==Target.PIEInstance&&C.World()==Target.World)++Matches;return Matches==1;};
    if(!Exact())return W.Bad(TEXT("Selected PIE role changed before input; no dispatch"));
    W.R.Data->SetStringField(TEXT("selected_world"),Target.World->GetPathName());
    W.R.Data->SetStringField(TEXT("selected_controller"),Target.PlayerController->GetPathName());
    W.R.Data->SetNumberField(TEXT("selected_pie_instance"),Target.PIEInstance);
    const FPlatformUserId PlatformUserId = Target.LocalPlayer->GetPlatformUserId();
    if (!PlatformUserId.IsValid())
    {
        return W.Bad(
            TEXT("The selected PIE local player does not have a valid platform user id"));
    }

    FString InputDeviceSource = TEXT("primary_for_platform_user");
    FInputDeviceId InputDevice = UInputDeviceLibrary::GetPrimaryInputDeviceForUser(PlatformUserId);
    if (!UInputDeviceLibrary::IsValidInputDevice(InputDevice))
    {
        InputDevice = UInputDeviceLibrary::GetDefaultInputDevice();
        InputDeviceSource = TEXT("default_fallback");
    }
    if (!UInputDeviceLibrary::IsValidInputDevice(InputDevice))
    {
        return W.Bad(
            TEXT("No valid input device is available for the selected PIE local player"));
    }

    const FPlatformUserId MappedPlatformUserId = UInputDeviceLibrary::GetUserForInputDevice(InputDevice);
    if (MappedPlatformUserId != PlatformUserId)
    {
        return W.Bad(FString::Printf(
            TEXT("Input device %d maps to platform user %d instead of selected platform user %d"),
            InputDevice.GetId(),
            MappedPlatformUserId.GetInternalId(),
            PlatformUserId.GetInternalId()));
    }

    if(!Exact())return W.Bad(TEXT("Selected PIE role changed after device mapping; no dispatch"));
    APlayerController* PlayerController = Target.PlayerController;
    const bool bKeyDownBefore = PlayerController->IsInputKeyDown(Key);
    const bool bJustPressedBefore = PlayerController->WasInputKeyJustPressed(Key);
    const bool bJustReleasedBefore = PlayerController->WasInputKeyJustReleased(Key);
    const float KeyTimeDownBefore = PlayerController->GetInputKeyTimeDown(Key);

    const float AmountDepressed = InputEvent == IE_Released ? 0.0f : 1.0f;
    const FInputKeyEventArgs InputEventArgs = FInputKeyEventArgs::CreateSimulated(
        Key,
        InputEvent,
        AmountDepressed,
        -1,
        InputDevice);

    auto Before=MakeShared<FJsonObject>();Before->SetBoolField(TEXT("key_down"),bKeyDownBefore);Before->SetBoolField(TEXT("just_pressed"),bJustPressedBefore);Before->SetBoolField(TEXT("just_released"),bJustReleasedBefore);Before->SetNumberField(TEXT("key_time_down"),KeyTimeDownBefore);W.R.Data->SetObjectField(TEXT("before"),Before);
    if(!Exact())return W.Bad(TEXT("Selected PIE role changed before InputKey; no dispatch"));
    bool bInputKeyReturn = false;
    TStrongObjectPtr<UWorld> HoldPreviousGlobalWorld(GWorld);
    {
        W.R.Stage=TEXT("conditional_world_switch");W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),true);W.Counts->SetNumberField(TEXT("FScopedConditionalWorldSwitcher"),1);
        FScopedConditionalWorldSwitcher WorldSwitcher(Target.World);
        if(!Exact())return W.Bad(TEXT("PIE target changed in world-switch callback; InputKey not dispatched"));
        W.R.Stage=TEXT("PlayerController_InputKey");W.R.bMutationAttempted=true;W.Counts->SetNumberField(TEXT("PlayerController_InputKey"),1);W.R.Data->SetBoolField(TEXT("input_dispatch_entered"),true);
        bInputKeyReturn = PlayerController->InputKey(InputEventArgs);
        W.R.Data->SetBoolField(TEXT("input_dispatch_returned"),true);W.R.Data->SetBoolField(TEXT("input_key_return"),bInputKeyReturn);W.R.bChanged=true;
    }

    if(!Exact())return W.Bad(TEXT("PIE input was dispatched but target world/controller role changed during callback; actual prefix retained, no readback/replay"));
    TSharedPtr<FJsonObject> DeviceObject = MakeShared<FJsonObject>();
    DeviceObject->SetNumberField(TEXT("input_device_id"), InputDevice.GetId());
    DeviceObject->SetNumberField(TEXT("platform_user_id"), PlatformUserId.GetInternalId());
    DeviceObject->SetNumberField(TEXT("mapped_platform_user_id"), MappedPlatformUserId.GetInternalId());
    DeviceObject->SetBoolField(TEXT("mapped_user_matches"), MappedPlatformUserId == PlatformUserId);
    DeviceObject->SetStringField(TEXT("source"), InputDeviceSource);
    DeviceObject->SetNumberField(TEXT("event_args_controller_id"), InputEventArgs.ControllerId);

    TSharedPtr<FJsonObject> WorldObject = MakeShared<FJsonObject>();
    WorldObject->SetStringField(TEXT("name"), Target.World->GetName());
    WorldObject->SetStringField(TEXT("path"), Target.World->GetPathName());
    WorldObject->SetStringField(
        TEXT("package"),
        Target.World->GetOutermost() ? Target.World->GetOutermost()->GetName() : FString());
    WorldObject->SetStringField(TEXT("type"), TEXT("pie"));
    WorldObject->SetNumberField(TEXT("pie_instance"), Target.PIEInstance);
    WorldObject->SetBoolField(TEXT("has_begun_play"), Target.World->HasBegunPlay());

    TSharedPtr<FJsonObject> ControllerObject = MakeShared<FJsonObject>();
    ControllerObject->SetStringField(TEXT("name"), PlayerController->GetName());
    ControllerObject->SetStringField(TEXT("path"), PlayerController->GetPathName());
    ControllerObject->SetStringField(TEXT("class"), PlayerController->GetClass()->GetPathName());
    ControllerObject->SetNumberField(TEXT("local_player_index"), Target.LocalPlayerIndex);
    const bool bLocalAfter=PlayerController->IsLocalController();
    if(!Exact())return W.Bad(TEXT("PIE target changed in post-input IsLocalController; actual dispatch prefix retained"));
    ControllerObject->SetBoolField(TEXT("is_local_controller"), bLocalAfter);
    APawn* Pawn = PlayerController->GetPawn();
    ControllerObject->SetBoolField(TEXT("input_enabled"), PlayerController->InputEnabled());
    ControllerObject->SetStringField(TEXT("pawn_name"), IsValid(Pawn) ? Pawn->GetName() : FString());
    ControllerObject->SetStringField(
        TEXT("pawn_class"),
        IsValid(Pawn) ? Pawn->GetClass()->GetPathName() : FString());

    TSharedPtr<FJsonObject> ReadbackObject = MakeShared<FJsonObject>();
    ReadbackObject->SetBoolField(TEXT("input_key_return"), bInputKeyReturn);
    ReadbackObject->SetBoolField(TEXT("simulated_input"), InputEventArgs.IsSimulatedInput());
    ReadbackObject->SetBoolField(TEXT("key_down_before"), bKeyDownBefore);
    ReadbackObject->SetBoolField(TEXT("key_down_after"), PlayerController->IsInputKeyDown(Key));
    ReadbackObject->SetBoolField(TEXT("just_pressed_before"), bJustPressedBefore);
    ReadbackObject->SetBoolField(TEXT("just_pressed_after"), PlayerController->WasInputKeyJustPressed(Key));
    ReadbackObject->SetBoolField(TEXT("just_released_before"), bJustReleasedBefore);
    ReadbackObject->SetBoolField(TEXT("just_released_after"), PlayerController->WasInputKeyJustReleased(Key));
    ReadbackObject->SetNumberField(TEXT("key_time_down_before"), KeyTimeDownBefore);
    ReadbackObject->SetNumberField(TEXT("key_time_down_after"), PlayerController->GetInputKeyTimeDown(Key));
    ReadbackObject->SetStringField(
        TEXT("sample_phase"),
        TEXT("immediate_after_input_key_before_next_player_input_tick"));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetStringField(TEXT("key"), Key.GetFName().ToString());
    ResultObj->SetStringField(TEXT("event"), RequestedEvent);
    ResultObj->SetNumberField(TEXT("amount_depressed"), AmountDepressed);
    ResultObj->SetObjectField(TEXT("device"), DeviceObject);
    ResultObj->SetObjectField(TEXT("world"), WorldObject);
    ResultObj->SetObjectField(TEXT("controller"), ControllerObject);
    ResultObj->SetObjectField(TEXT("readback"), ReadbackObject);
    return ResultObj;
}


}
FString InjectKey(const FString& ParamsJson){FWork W;TSharedPtr<FJsonObject> P,L;if(!W.Safe())L=W.Bad(TEXT("Editor/game-thread context unavailable, simulating, GC, exit or save active"));else if(ParamsJson.Len()>1048576)L=W.Bad(TEXT("PIE input JSON budget exceeded"));else if(!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(ParamsJson),P)||!P)L=W.Bad(TEXT("Expected JSON object"));else{for(const auto& KV:P->Values)if(KV.Key!=TEXT("key")&&KV.Key!=TEXT("event")&&KV.Key!=TEXT("pie_instance")){L=W.Bad(FString(TEXT("Unknown PIE input field: "))+FString(KV.Key));break;}if(!L)L=Handle(P,W);}W.R.Data->SetObjectField(TEXT("legacy_result"),L?L:MakeShared<FJsonObject>());W.R.bSuccess=W.R.Error.IsEmpty()&&L.IsValid();W.R.bPostconditionPassed=W.R.bSuccess;if(W.R.bSuccess){W.R.Stage=TEXT("complete");W.R.bChangeStateKnown=true;}else if(W.R.bMutationAttempted)W.R.bChangeStateKnown=false;W.R.Data->SetBoolField(TEXT("native_mutation_attempted"),W.R.bMutationAttempted);W.R.Data->SetBoolField(TEXT("change_state_known"),W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("mutation_state_unknown"),!W.R.bChangeStateKnown);return Operations::EncodeVariableResult(W.R,TEXT("inject_pie_input_key"));}
}
