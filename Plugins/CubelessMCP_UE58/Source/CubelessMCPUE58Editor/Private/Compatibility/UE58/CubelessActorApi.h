#pragma once

#include "CoreMinimal.h"
#include "Dom/JsonObject.h"

class AActor;
class ULevel;
class UWorld;

namespace Cubeless::UE58::Actor
{
enum class EOperation : uint8 { List, Find, Inspect, Spawn, Delete, SetTransform };

struct FRequest
{
    EOperation Operation = EOperation::List;
    FString Name;
    FString Type;
    FString BlueprintPath;
    FString Pattern;
    FString LocationJson = TEXT("null");
    FString RotationJson = TEXT("null");
    FString ScaleJson = TEXT("null");
    FString ExpectedLevelPath;
    int32 MaxActors = 10000;
    bool bOriginalTransformContract = false; // UC explicit original4 route only; existing requests remain false.
};

struct FState
{
    TWeakObjectPtr<UWorld> World;
    TWeakObjectPtr<ULevel> Level;
    TWeakObjectPtr<AActor> Target;
    TSharedPtr<FJsonObject> Snapshot;
    TSharedPtr<FJsonObject> TargetSnapshot;
    TMap<FString, TSharedPtr<FJsonObject>> ActorsByPath;
    TArray<FString> CaseInsensitiveNameMatches;
    FTransform TargetTransform = FTransform::Identity;
    FString LevelPath;
    bool bDirty = false;
    int32 ActorVisits = 0;
};

struct FTargetPointMutation;
struct FPreparedMutation
{
    UClass* SpawnClass = nullptr;
    FTransform DesiredTransform = FTransform::Identity;
    bool bNoOp = false;
    bool bBlueprintLoadAttempted = false;
    TSharedPtr<FTargetPointMutation> TargetPoint;
};

FString OperationName(EOperation Operation);
bool IsMutation(EOperation Operation);
bool Capture(const FRequest& Request, FState& Out, FString& Code, FString& Error);
bool PrepareMutation(const FRequest& Request, const FState& Before, FPreparedMutation& Out, FString& Code, FString& Error);
bool ExecuteMutation(const FRequest& Request, const FState& Before, const FPreparedMutation& Prepared);
bool CheckTargetPointScope(const FPreparedMutation& Prepared, FString& Code, FString& Error);
TSharedPtr<FJsonObject> TargetPointDiagnostics(const FPreparedMutation& Prepared);
bool VerifyMutation(const FRequest& Request, const FState& Before, const FState& After, const FPreparedMutation& Prepared, bool bApiAccepted);
TSharedPtr<FJsonObject> ReadPayload(const FRequest& Request, const FState& State);
TSharedPtr<FJsonObject> DescribeChanges(const FState& Before, const FState& After);
FString ToJson(const TSharedPtr<FJsonObject>& Value);
}
