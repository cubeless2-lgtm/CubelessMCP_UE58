#pragma once
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolset; }
namespace Cubeless::Registration
{
    struct FSkeletalPhysicsRawCallState;
    // Own one fixed PhysicsAsset15 raw Registry method; never replace a handler or register a reflected Python class.
    class FSkeletalPhysicsRawRegistration final
    {
    public:
        bool Register();
        void Unregister();
    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolset> Handler;
        TSharedPtr<FSkeletalPhysicsRawCallState, ESPMode::ThreadSafe> CallState;
    };
}
