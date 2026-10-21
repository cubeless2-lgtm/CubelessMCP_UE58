#include "CubelessGASAuthoringLibrary.h"
#include "Compatibility/UE58/CubelessGASAuthoringApi.h"

FString UCubelessGASAuthoringLibrary::InspectGASBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::Inspect(ParamsJson);
}

FString UCubelessGASAuthoringLibrary::CreateGameplayAbilityBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::CreateAbility(ParamsJson);
}

FString UCubelessGASAuthoringLibrary::CreateGameplayEffectBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::CreateEffect(ParamsJson);
}

FString UCubelessGASAuthoringLibrary::CreateAttributeSetBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::CreateAttributeSet(ParamsJson);
}

FString UCubelessGASAuthoringLibrary::AddAbilitySystemComponentToBlueprint(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::AddAbilitySystemComponent(ParamsJson);
}

FString UCubelessGASAuthoringLibrary::AddGameplayAttributeToAttributeSet(const FString& ParamsJson)
{
    return Cubeless::UE58::GASAuthoring::AddGameplayAttribute(ParamsJson);
}
