#include "Modules/ModuleManager.h"
#include "Registration/CubelessJsonConverters.h"
#include "Registration/CubelessNativeToolset.h"
#include "ToolsetRegistry/UToolsetRegistry.h"

DEFINE_LOG_CATEGORY_STATIC(LogCubelessMCPUE58, Log, All);

class FCubelessMCPUE58EditorModule final : public IModuleInterface
{
public:
    virtual void StartupModule() override
    {
        if (!UToolsetRegistry::IsAvailable())
        {
            UE_LOG(LogCubelessMCPUE58, Error, TEXT("Official Toolset Registry is unavailable; Cubeless native toolset was not registered."));
            return;
        }
        // Python init_unreal registers toolsets after this PostEngineInit startup.
        // Their call objects cache schemas: converters must be installed first.
        if (!JsonConverters.Register())
        {
            UE_LOG(LogCubelessMCPUE58, Error, TEXT("Cubeless Material array converter registration failed; a clean editor restart is required after repair."));
            return;
        }
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless Material array converter registered before Python toolset initialization (two exact parameters)."));
        if (UToolsetRegistry::IsToolsetClassRegistered(UCubelessNativeToolset::StaticClass()))
        {
            UE_LOG(LogCubelessMCPUE58, Warning, TEXT("Cubeless native toolset is already registered; existing registration retained."));
            return;
        }
        UToolsetRegistry::RegisterToolsetClass(UCubelessNativeToolset::StaticClass());
        bOwnsRegistration = UToolsetRegistry::IsToolsetClassRegistered(UCubelessNativeToolset::StaticClass());
        UE_LOG(LogCubelessMCPUE58, Display, TEXT("Cubeless native toolset registration: %s"), bOwnsRegistration ? TEXT("confirmed") : TEXT("failed"));
    }

    virtual void ShutdownModule() override
    {
        if (bOwnsRegistration && UObjectInitialized())
        {
            UToolsetRegistry::UnregisterToolsetClass(UCubelessNativeToolset::StaticClass());
            bOwnsRegistration = false;
        }
        JsonConverters.Unregister();
    }

private:
    bool bOwnsRegistration = false;
    Cubeless::Registration::FJsonConverters JsonConverters;
};

IMPLEMENT_MODULE(FCubelessMCPUE58EditorModule, CubelessMCPUE58Editor)
