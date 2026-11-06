#include "CubelessClassDefaultObjectAccess.h"

#include "UObject/Class.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58
{
    const TObjectPtr<UObject>& GetClassDefaultObjectSlot(const UClass* Class)
    {
        // GetDefaultObject(false) still resolves the stored handle. Some guards
        // must reject an unresolved handle before any CDO read.
        static const FObjectProperty* const Property = []()
        {
            const UClass* ClassType = UClass::StaticClass();
            const FObjectProperty* Slot = FindFProperty<FObjectProperty>(ClassType, TEXT("ClassDefaultObject"));
            checkf(Slot && Slot->GetOwnerStruct() == ClassType && Slot->PropertyClass == UObject::StaticClass()
                && Slot->ArrayDim == 1 && Slot->GetElementSize() == sizeof(TObjectPtr<UObject>),
                TEXT("UE 5.8 must reflect the native ClassDefaultObject handle slot."));
            return Slot;
        }();

        return *Property->ContainerPtrToValuePtr<TObjectPtr<UObject>>(Class);
    }
}
