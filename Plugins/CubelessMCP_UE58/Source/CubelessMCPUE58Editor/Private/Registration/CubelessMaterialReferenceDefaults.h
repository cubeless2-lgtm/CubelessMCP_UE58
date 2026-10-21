#pragma once

#include "CoreMinimal.h"

class FJsonObject;
class UToolsetRegistrySubsystem;
namespace UE::ToolsetRegistry { class FToolsetJsonConverter; }

namespace Cubeless::Registration
{
    // One scalar default on the exact canonical unregistered MaterialTools method.
    bool VerifyMaterialReferenceToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);

    // Original sync inputs only: two empty strings and nullable names; no asset work.
    bool VerifyMaterialCollectionSyncToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);

    // Original expansion selector only; one scoped empty-string default, no asset work.
    bool VerifyMaterialFunctionExpansionToolset(UClass* ToolsetClass, const TSharedRef<FJsonObject>& Report, FString& Error);

    class FMaterialReferenceDefaultsOwner final
    {
    public:
        bool Register();
        void Unregister();

    private:
        TWeakObjectPtr<UToolsetRegistrySubsystem> RegistrySubsystem;
        TSharedPtr<UE::ToolsetRegistry::FToolsetJsonConverter> Converter;
    };
}
