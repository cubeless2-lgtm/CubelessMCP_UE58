#include "Registration/CubelessJsonConverters.h"

#include "Compatibility/UE58/CubelessMaterialArrayConverter.h"
#include "ToolsetRegistry/ToolsetJsonConverter.h"
#include "ToolsetRegistry/ToolsetRegistrySubsystem.h"
#include "UObject/UObjectGlobals.h"

bool Cubeless::Registration::FJsonConverters::Register()
{
    if (MaterialArrays.IsValid()) { return RegistrySubsystem.IsValid(); }
    auto Subsystem = UToolsetRegistrySubsystem::Get(TEXT("Cubeless Material array converter registration"));
    if (!Subsystem.HasValue()) { return false; }
    TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Candidate = UE58::MakeMaterialArrayConverter();
    if (!Subsystem.GetValue()->ToolsetRegistry.RegisterConverter(Candidate)) { return false; }
    RegistrySubsystem = Subsystem.GetValue().Get();
    MaterialArrays = Candidate;
    return true;
}

void Cubeless::Registration::FJsonConverters::Unregister()
{
    // Never discover/load another subsystem during teardown or unregister someone else's converter.
    if (MaterialArrays.IsValid() && UObjectInitialized())
    {
        if (UToolsetRegistrySubsystem* Subsystem = RegistrySubsystem.Get())
        {
            Subsystem->ToolsetRegistry.UnregisterConverter(MaterialArrays);
        }
    }
    MaterialArrays.Reset();
    RegistrySubsystem.Reset();
}
