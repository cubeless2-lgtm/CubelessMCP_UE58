#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Components/ActorComponent.h"
#include "EdGraphSchema_K2.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/PlayerController.h"
#include "UObject/Package.h"

namespace Cubeless::UE58::RuntimeObject
{
bool IsSupportedClass(const UClass* Class)
{
    if (!IsValid(Class) || !Class->HasAnyClassFlags(CLASS_Native)
        || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || Class->IsEditorOnly()
        || Class->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)) { return false; }
    return Class == UObject::StaticClass() || Class == AActor::StaticClass() || Class == AController::StaticClass()
        || Class == APlayerController::StaticClass() || Class == UActorComponent::StaticClass();
}
UClass* FindSupportedClass(const FString& ExactPath)
{
    for (UClass* Class : {UObject::StaticClass(), AActor::StaticClass(), AController::StaticClass(),
        APlayerController::StaticClass(), UActorComponent::StaticClass()})
    { if (Class->GetPathName() == ExactPath && IsSupportedClass(Class)) { return Class; } }
    return nullptr;
}
bool IsSupportedPin(const FEdGraphPinType& Type)
{
    return Type.PinCategory == UEdGraphSchema_K2::PC_Object && Type.PinSubCategory.IsNone()
        && !Type.IsContainer() && !Type.bIsReference && !Type.bIsWeakPointer
        && IsSupportedClass(Cast<UClass>(Type.PinSubCategoryObject.Get()));
}
}
