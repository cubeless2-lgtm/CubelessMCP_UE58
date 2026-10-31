#pragma once
#include "CoreMinimal.h"
class UObject;
namespace Cubeless::UE58::ActorInterface
{
// Editor authoring adapter only. Declaration attachment does not compile/save
// the target or certify its runtime implementation/full authored preservation.
FString AttachEventInterface(UObject* Blueprint, UObject* InterfaceBlueprint, bool Execute);
}
