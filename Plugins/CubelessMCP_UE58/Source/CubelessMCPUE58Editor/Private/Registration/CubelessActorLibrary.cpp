#include "Registration/CubelessActorLibrary.h"
#include "Operations/CubelessActorOperations.h"

using Cubeless::UE58::Actor::EOperation;
using Cubeless::UE58::Actor::FRequest;
using Cubeless::Operations::RunActorOperation;

namespace
{
FRequest MakeRequest(EOperation Operation, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request;
    Request.Operation = Operation;
    Request.ExpectedLevelPath = ExpectedLevelPath;
    Request.MaxActors = MaxActors;
    return Request;
}
}

FString UCubelessActorLibrary::GetActorsInLevel(const FString& ExpectedLevelPath, int32 MaxActors)
{
    return RunActorOperation(MakeRequest(EOperation::List, ExpectedLevelPath, MaxActors));
}

FString UCubelessActorLibrary::FindActorsByName(const FString& Pattern, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::Find, ExpectedLevelPath, MaxActors); Request.Pattern = Pattern;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::GetActorProperties(const FString& Name, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::Inspect, ExpectedLevelPath, MaxActors); Request.Name = Name;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::SpawnActor(const FString& Name, const FString& Type, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::Spawn, ExpectedLevelPath, MaxActors);
    Request.Name = Name; Request.Type = Type; Request.LocationJson = LocationJson; Request.RotationJson = RotationJson; Request.ScaleJson = ScaleJson;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::SpawnBlueprintActor(const FString& BlueprintPath, const FString& Name, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::Spawn, ExpectedLevelPath, MaxActors);
    Request.Name = Name; Request.BlueprintPath = BlueprintPath;
    Request.LocationJson = LocationJson; Request.RotationJson = RotationJson; Request.ScaleJson = ScaleJson;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::DeleteActor(const FString& Name, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::Delete, ExpectedLevelPath, MaxActors); Request.Name = Name;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::SetActorTransform(const FString& Name, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::SetTransform, ExpectedLevelPath, MaxActors);
    Request.Name = Name; Request.LocationJson = LocationJson; Request.RotationJson = RotationJson; Request.ScaleJson = ScaleJson;
    return RunActorOperation(Request);
}

FString UCubelessActorLibrary::SetActorTransformOriginal(const FString& Name, const FString& LocationJson, const FString& RotationJson, const FString& ScaleJson, const FString& ExpectedLevelPath, int32 MaxActors)
{
    FRequest Request = MakeRequest(EOperation::SetTransform, ExpectedLevelPath, MaxActors);
    Request.Name = Name; Request.LocationJson = LocationJson; Request.RotationJson = RotationJson; Request.ScaleJson = ScaleJson;
    Request.bOriginalTransformContract = true;
    return RunActorOperation(Request);
}
