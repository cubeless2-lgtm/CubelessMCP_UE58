#pragma once
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolset; }
namespace Cubeless::Registration
{
    struct FBlueprintExpressionRawCallState;
    // Own one exact named official Registry handler, without replacing a UClass toolset.
    class FBlueprintExpressionRawRegistration final
    {
    public:
        bool Register();
        void Unregister();
    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolset> Handler;
        TSharedPtr<FBlueprintExpressionRawCallState, ESPMode::ThreadSafe> CallState;
    };
}
