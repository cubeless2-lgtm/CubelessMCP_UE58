#pragma once
#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolset; }
namespace Cubeless::Registration
{
    struct FAnimGraphInspectionRawCallState;
    // Own one exact named official Registry handler, without replacing a UClass toolset.
    class FAnimGraphInspectionRawRegistration final
    {
    public:
        bool Register();
        void Unregister();
    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolset> Handler;
        TSharedPtr<FAnimGraphInspectionRawCallState, ESPMode::ThreadSafe> CallState;
    };
}
