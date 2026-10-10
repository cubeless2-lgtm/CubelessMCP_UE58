#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"

namespace Cubeless::UE58::GeneratedComponent
{
namespace
{
TSharedRef<FJsonObject> InspectSavedRuntimeClass(UClass* Class, bool bActorParent)
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("stage"), TEXT("preflight"));
    R->SetBoolField(TEXT("loaded_during_inspection"), false);
    R->SetBoolField(TEXT("compile_requested"), false); R->SetBoolField(TEXT("save_requested"), false);
    R->SetStringField(TEXT("validation_scope"), bActorParent
        ? TEXT("exact loaded saved clean Actor Blueprint and generated parent chain; native ancestry and saved recursive package dependencies, at most 256 packages")
        : TEXT("exact loaded, saved, clean non-scene ActorComponent Blueprint class; native ancestry and saved recursive package dependencies, at most 256 packages"));
    auto Reject = [&R](const TCHAR* Code, const FString& Message)
    { R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Message); return false; };
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Reject(TEXT("generated_component_editor_state_invalid"), TEXT("Class inspection requires the editor game thread outside PIE/SIE.")); return R; }
    UBlueprint* BP = IsValid(Class) ? Cast<UBlueprint>(Class->ClassGeneratedBy) : nullptr;
    if (!IsValid(BP) || !Cast<UBlueprintGeneratedClass>(Class) || BP->GeneratedClass != Class
        || Class->GetAuthoritativeClass() != Class || Class->GetOutermost() != BP->GetOutermost()
        || Class->HasAnyClassFlags(CLASS_Native | CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        || (bActorParent ? !Class->IsChildOf(AActor::StaticClass())
            : (!Class->IsChildOf(UActorComponent::StaticClass()) || Class->IsChildOf(USceneComponent::StaticClass()))))
    { Reject(TEXT("generated_component_class_unsupported"), bActorParent
        ? TEXT("Use an exact current instantiable generated Actor class; native, skeleton, reinstanced and abstract classes are outside this slice.")
        : TEXT("Use an exact current instantiable generated non-scene ActorComponent class; native, scene, skeleton, reinstanced and abstract classes are outside this slice.")); return R; }
    const FString PackageName = BP->GetOutermost()->GetName();
    const bool WasDirty = BP->GetOutermost()->IsDirty(); const EBlueprintStatus OldStatus = BP->Status;
    R->SetStringField(TEXT("class_path"), Class->GetPathName()); R->SetStringField(TEXT("blueprint_path"), BP->GetPathName());
    R->SetStringField(TEXT("package_name"), PackageName); R->SetBoolField(TEXT("dirty_before"), WasDirty);
    R->SetNumberField(TEXT("status_before"), static_cast<int32>(OldStatus));
    const bool Saved = PackageName.StartsWith(TEXT("/Game/")) && FPackageName::DoesPackageExist(PackageName);
    const bool Compiled = OldStatus == BS_UpToDate || OldStatus == BS_UpToDateWithWarnings;
    R->SetBoolField(TEXT("saved_asset"), Saved); R->SetBoolField(TEXT("clean"), !WasDirty); R->SetBoolField(TEXT("compiled_status"), Compiled);
    const bool Passed = [&]()
    {
        if (!Saved || WasDirty || !Compiled || BP->BlueprintType != BPTYPE_Normal || !BP->ParentClass
            || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad)
        { return Reject(TEXT("generated_component_compile_save_required"), TEXT("Component Blueprint must be a saved clean normal /Game asset with an up-to-date compile and no queued/active structural work; this inspection never compiles or saves it.")); }
        const auto Parent = InspectBlueprintParent(Class->GetPathName()); R->SetObjectField(TEXT("parent_inspection"), Parent);
        if (!Parent->GetBoolField(TEXT("success")))
        { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Generated component class failed the existing runtime parent/CDO inspection.")); }
        TArray<TSharedPtr<FJsonValue>> GeneratedParents, ParentPaths;
        TSet<const UClass*> Seen;
        for (const UClass* Current = Class; Current; Current = Current->GetSuperClass())
        {
            if (Seen.Num() >= 128 || Seen.Contains(Current))
            { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Invalid or excessive superclass chain.")); }
            Seen.Add(Current); ParentPaths.Add(MakeShared<FJsonValueString>(Current->GetPathName()));
            const FString CurrentPackage = Current->GetOutermost()->GetName();
            if (Current->HasAnyClassFlags(CLASS_Native) && CurrentPackage != TEXT("/Script/Engine") && CurrentPackage != TEXT("/Script/CoreUObject"))
            { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Native parent module is outside audited Engine/CoreUObject scope.")); }
            if (bActorParent && !Current->HasAnyClassFlags(CLASS_Native))
            {
                const UBlueprint* Ancestor = Cast<UBlueprint>(Current->ClassGeneratedBy);
                if (!IsValid(Ancestor) || !Cast<UBlueprintGeneratedClass>(Current) || Ancestor->GeneratedClass != Current
                    || Current->GetAuthoritativeClass() != Current || Current->GetOutermost() != Ancestor->GetOutermost()
                    || !CurrentPackage.StartsWith(TEXT("/Game/")) || !FPackageName::DoesPackageExist(CurrentPackage)
                    || Ancestor->GetOutermost()->IsDirty() || Ancestor->BlueprintType != BPTYPE_Normal
                    || (Ancestor->Status != BS_UpToDate && Ancestor->Status != BS_UpToDateWithWarnings)
                    || Ancestor->bBeingCompiled || Ancestor->bQueuedForCompilation || Ancestor->bSuppressStructurallyModified || Ancestor->bIsRegeneratingOnLoad
                    || Current->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
                { return Reject(TEXT("generated_actor_parent_state_unsupported"), TEXT("Every generated Actor ancestor must be saved, clean, authoritative, normally compiled and idle.")); }
                auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Current->GetPathName());
                Row->SetStringField(TEXT("generated_class"), Ancestor->GeneratedClass->GetPathName());
                Row->SetStringField(TEXT("blueprint_path"), Ancestor->GetPathName()); Row->SetStringField(TEXT("package_name"), CurrentPackage);
                Row->SetBoolField(TEXT("dirty"), false); Row->SetBoolField(TEXT("saved_asset"), true);
                Row->SetNumberField(TEXT("status"), static_cast<int32>(Ancestor->Status.GetValue()));
                GeneratedParents.Add(MakeShared<FJsonValueObject>(Row));
            }
        }
        if (bActorParent) { R->SetArrayField(TEXT("generated_parents"), GeneratedParents); R->SetArrayField(TEXT("parent_chain"), ParentPaths); }
        const auto Audit = ReadPackageDependencies({PackageName}, true, 256); R->SetObjectField(TEXT("dependency_audit"), Audit);
        bool AuditPassed = false;
        if (!Audit->TryGetBoolField(TEXT("passed"), AuditPassed) || !AuditPassed)
        { return Reject(TEXT("generated_component_dependencies_unsupported"), TEXT("Saved recursive dependency audit failed, is dirty/incomplete, or reaches editor tooling; inspect dependency_audit.")); }
        const TSet<FString> NativePackages = {TEXT("/Script/Engine"), TEXT("/Script/CoreUObject"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
        for (const auto& Edge : Audit->GetArrayField(TEXT("edges")))
        {
            const FString Package = Edge->AsObject()->GetStringField(TEXT("to"));
            if (!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/")) && !NativePackages.Contains(Package))
            { return Reject(TEXT("generated_component_dependencies_unsupported"), TEXT("Unaudited script module or plugin mount in saved dependencies: ") + Package); }
        }
        return true;
    }();
    bool Unchanged = BP->GetOutermost()->IsDirty() == WasDirty && BP->Status == OldStatus && BP->GeneratedClass == Class;
    if (bActorParent && R->HasField(TEXT("generated_parents")))
    {
        for (const auto& Value : R->GetArrayField(TEXT("generated_parents")))
        {
            const auto Row = Value->AsObject(); const UBlueprint* Ancestor = FindObject<UBlueprint>(nullptr, *Row->GetStringField(TEXT("blueprint_path")));
            Unchanged &= IsValid(Ancestor) && !Ancestor->GetOutermost()->IsDirty()
                && static_cast<int32>(Ancestor->Status.GetValue()) == Row->GetIntegerField(TEXT("status"))
                && Ancestor->GeneratedClass && Ancestor->GeneratedClass->GetPathName() == Row->GetStringField(TEXT("generated_class"));
        }
    }
    R->SetBoolField(TEXT("dirty_after"), BP->GetOutermost()->IsDirty()); R->SetNumberField(TEXT("status_after"), static_cast<int32>(BP->Status.GetValue()));
    R->SetBoolField(TEXT("observed_state_unchanged"), Unchanged);
    if (!Unchanged) { Reject(TEXT("generated_component_read_state_changed"), TEXT("Class/Blueprint dirty or compile status changed during dependency inspection; inspect the asset before continuing.")); }
    R->SetBoolField(TEXT("success"), Passed && Unchanged);
    if (Passed && Unchanged) { R->SetStringField(TEXT("stage"), TEXT("complete")); }
    return R;
}
}
TSharedRef<FJsonObject> InspectSavedClass(UClass* Class) { return InspectSavedRuntimeClass(Class, false); }
TSharedRef<FJsonObject> InspectSavedActorParent(UClass* Class) { return InspectSavedRuntimeClass(Class, true); }
}
