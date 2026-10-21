#pragma once

#include "CoreMinimal.h"
#include "Templates/SharedPointer.h"
#include "UObject/WeakObjectPtrTemplates.h"

class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolset; }
namespace Cubeless::Registration
{
struct FPythonExecutionCallState;
// Root module owns exactly one registration and tears it down before unloading.
class FPythonExecutionRegistration final
{
public:
    bool Register();
    void Unregister();
private:
    TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
    TSharedPtr<UE::ToolsetRegistry::FToolset> Handler;
    TSharedPtr<FPythonExecutionCallState, ESPMode::ThreadSafe> CallState;
};
}
