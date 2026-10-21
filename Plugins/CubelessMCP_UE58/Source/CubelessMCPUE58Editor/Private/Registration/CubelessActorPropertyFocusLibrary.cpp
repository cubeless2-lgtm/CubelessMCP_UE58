#include "Registration/CubelessActorPropertyFocusLibrary.h"
#include "Compatibility/UE58/CubelessActorPropertyFocusApi.h"
#include "Operations/CubelessVariableOperations.h"

FString UCubelessActorPropertyFocusLibrary::SetActorProperty(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ActorPropertyFocus::SetProperty(ParamsJson),TEXT("set_actor_property"));
}
FString UCubelessActorPropertyFocusLibrary::FocusViewport(FString ParamsJson)
{
    return Cubeless::Operations::EncodeVariableResult(Cubeless::UE58::ActorPropertyFocus::Focus(ParamsJson),TEXT("focus_viewport"));
}
