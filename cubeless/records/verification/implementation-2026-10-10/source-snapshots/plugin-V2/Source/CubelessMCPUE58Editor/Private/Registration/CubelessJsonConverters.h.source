#pragma once

#include "CoreMinimal.h"

class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::Registration
{
    // Own only the converter registered by this module instance, on its original subsystem.
    class FJsonConverters final
    {
    public:
        bool Register();
        void Unregister();

    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> MaterialArrays;
    };
}
