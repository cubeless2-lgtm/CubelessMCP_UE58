#include "Compatibility/UE58/CubelessRuntimeObjectApi.h"
#include "Components/ActorComponent.h"
#include "Components/ChildActorComponent.h"
#include "Components/InstancedStaticMeshComponent.h"
#include "Components/SceneComponent.h"
#include "Components/SplineComponent.h"
#include "EdGraphSchema_K2.h"
#include "Engine/StaticMesh.h"
#include "GameFramework/Actor.h"
#include "GameFramework/Controller.h"
#include "GameFramework/Pawn.h"
#include "GameFramework/PlayerController.h"
#include "NiagaraComponent.h"
#include "UObject/Package.h"
#include "UObject/UObjectGlobals.h"

namespace Cubeless::UE58::RuntimeObject
{
bool IsSupportedClass(const UClass* Class)
{
    if (!IsValid(Class) || !Class->HasAnyClassFlags(CLASS_Native)
        || Class->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) || Class->IsEditorOnly()
        || Class->GetOutermost()->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)) { return false; }
    return Class == UObject::StaticClass() || Class == AActor::StaticClass() || Class == AController::StaticClass()
        || Class == APlayerController::StaticClass() || Class == APawn::StaticClass() || Class == UActorComponent::StaticClass()
        || ((Class == USceneComponent::StaticClass() || Class == UChildActorComponent::StaticClass() || Class == USplineComponent::StaticClass()
            || Class == UInstancedStaticMeshComponent::StaticClass() || Class == UStaticMesh::StaticClass())
            && Class->GetOutermost()->GetName() == TEXT("/Script/Engine"))
        || (Class == UNiagaraComponent::StaticClass() && Class->GetOutermost()->GetName() == TEXT("/Script/Niagara"));
}
UClass* FindSupportedClass(const FString& ExactPath)
{
    for (UClass* Class : {UObject::StaticClass(), AActor::StaticClass(), AController::StaticClass(),
        APlayerController::StaticClass(), APawn::StaticClass(), UActorComponent::StaticClass(), UNiagaraComponent::StaticClass(),
        USceneComponent::StaticClass(), UChildActorComponent::StaticClass(), USplineComponent::StaticClass(), UInstancedStaticMeshComponent::StaticClass(), UStaticMesh::StaticClass()})
    { if (Class->GetPathName() == ExactPath && IsSupportedClass(Class)) { return Class; } }
    return nullptr;
}
bool IsSupportedPin(const FEdGraphPinType& Type)
{
    return Type.PinCategory == UEdGraphSchema_K2::PC_Object && Type.PinSubCategory.IsNone()
        && !Type.IsContainer() && !Type.bIsReference && !Type.bIsWeakPointer
        && IsSupportedClass(Cast<UClass>(Type.PinSubCategoryObject.Get()));
}
bool IsSupportedClassPin(const FEdGraphPinType& Type)
{
    if (Type.PinCategory != UEdGraphSchema_K2::PC_Class || !Type.PinSubCategory.IsNone() || Type.IsContainer()
        || Type.bIsReference || Type.bIsConst || Type.bIsWeakPointer || Type.bSerializeAsSinglePrecisionFloat) { return false; }
    // K2 derives bIsUObjectWrapper from CPF_UObjectWrapper. The caller must
    // compare that flag with its exact own FClassProperty and descriptor; it is
    // not a reason to admit another constraint or widen IsSupportedPin.
    const UClass* Constraint = Cast<UClass>(Type.PinSubCategoryObject.Get());
    if (!IsValid(Constraint)) { return false; }
    const FString Path = Constraint->GetPathName();
    if (Path != TEXT("/Script/Engine.Actor") && Path != TEXT("/Script/Engine.ActorComponent")) { return false; }
    const UPackage* Package = Constraint->GetOutermost();
    return FindObject<UClass>(nullptr, *Path) == Constraint && IsSupportedClass(Constraint)
        && (Constraint == AActor::StaticClass() || Constraint == UActorComponent::StaticClass())
        && Constraint->ClassGeneratedBy == nullptr && !Constraint->HasAnyClassFlags(CLASS_Interface)
        && !Constraint->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects)
        && Package && Package->GetName() == TEXT("/Script/Engine") && !Package->IsDirty()
        && !Package->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly);
}
}
