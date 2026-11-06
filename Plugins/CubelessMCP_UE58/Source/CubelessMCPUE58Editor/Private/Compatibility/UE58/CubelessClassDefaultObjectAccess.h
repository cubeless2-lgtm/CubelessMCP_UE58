#pragma once

#include "UObject/ObjectPtr.h"

#include <memory>

class UClass;

namespace Cubeless::UE58
{
    // Reads the stored handle without resolving it or creating a default object.
    const TObjectPtr<UObject>& GetClassDefaultObjectSlot(const UClass* Class);

    template <typename ClassPointer>
    const TObjectPtr<UObject>& GetClassDefaultObjectSlot(const ClassPointer& Class)
    {
        return GetClassDefaultObjectSlot(static_cast<const UClass*>(std::to_address(Class)));
    }
}
