#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AIController.h"
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
    R->SetBoolField(TEXT("native_niagara_dependency_observed"), false);
    R->SetBoolField(TEXT("native_niagara_component_interface_verified"), false);
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
    // This exception belongs only to the Actor-parent path. Native class/CDO
    // identity stays rooted by the reflected native class; no CDO is allocated.
    const UClass* AIControllerRoot = nullptr;
    const UObject* AIControllerCDO = nullptr;
    const UPackage* AIControllerPackage = nullptr;
    bool AIModuleDependencyObserved = false;
    EObjectFlags AIControllerClassFlags = RF_NoFlags, AIControllerCDOFlags = RF_NoFlags;
    auto AIControllerProof = MakeShared<FJsonObject>();
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
            {
                // AIModule is not a general native-parent whitelist. Only the
                // exact public runtime AIController root extends Actor ancestry.
                if (!bActorParent || CurrentPackage != TEXT("/Script/AIModule") || Current != AAIController::StaticClass())
                { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Native ancestry requires Engine/CoreUObject or the exact runtime Actor AIController root; no general component/module exception.")); }
                const UPackage* NativePackage = Current->GetOutermost();
                const UObject* ExistingCDO = Current->GetDefaultObject(false);
                const bool ExactAIController = bActorParent && Current == AAIController::StaticClass()
                    && CurrentPackage == TEXT("/Script/AIModule") && IsValid(NativePackage)
                    && Current->GetOuter() == NativePackage && Current->GetPackage() == NativePackage
                    && Current->GetPathName() == TEXT("/Script/AIModule.AIController") && !Current->ClassGeneratedBy
                    && Current->GetSuperClass() == AController::StaticClass()
                    && !Current->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
                    && NativePackage->HasAnyPackageFlags(PKG_CompiledIn) && !NativePackage->IsDirty()
                    && !NativePackage->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly | PKG_PlayInEditor)
                    && IsValid(ExistingCDO) && ExistingCDO->GetClass() == Current
                    && ExistingCDO->GetOuter() == NativePackage && ExistingCDO->GetPackage() == NativePackage
                    && ExistingCDO->GetOutermost() == NativePackage
                    && ExistingCDO->GetPathName() == TEXT("/Script/AIModule.Default__AIController")
                    && ExistingCDO->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject)
                    && !Current->IsEditorOnly() && !ExistingCDO->IsEditorOnly();
                if (!ExactAIController)
                { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Native ancestry requires Engine/CoreUObject or, only for Actor parents, the exact audited runtime AIModule.AIController root and its existing CDO.")); }
                AIControllerRoot = Current; AIControllerCDO = ExistingCDO; AIControllerPackage = NativePackage;
                AIControllerClassFlags = Current->GetFlags(); AIControllerCDOFlags = ExistingCDO->GetFlags();
                AIControllerProof->SetStringField(TEXT("class_path"), Current->GetPathName());
                AIControllerProof->SetStringField(TEXT("cdo_path"), ExistingCDO->GetPathName());
                AIControllerProof->SetStringField(TEXT("native_package"), CurrentPackage);
                AIControllerProof->SetStringField(TEXT("scope"), TEXT("Exact native AIController root/CDO, existing saved generated Actor ancestry and recursive on-disk dependencies; not arbitrary AIModule classes or full callback/CDO-field purity."));
                AIControllerProof->SetBoolField(TEXT("cdo_allocation_requested"), false);
                R->SetObjectField(TEXT("native_ai_controller_parent"), AIControllerProof);
            }
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
        // FKey pins and native input bindings serialize InputCore.Key. InputCore
        // is an Engine runtime dependency, not an editor/Cubeless module. The
        // recursive editor-tooling audit and strict native ancestry remain above.
        const TSet<FString> NativePackages = {TEXT("/Script/Engine"), TEXT("/Script/CoreUObject"), TEXT("/Script/InputCore"), TEXT("/Script/PhysicsCore"), TEXT("/Script/MeshDescription"), TEXT("/Script/StaticMeshDescription"), TEXT("/Script/NavigationSystem")};
        for (const auto& Edge : Audit->GetArrayField(TEXT("edges")))
        {
            const FString Package = Edge->AsObject()->GetStringField(TEXT("to"));
            if (Package == TEXT("/Script/Niagara"))
            {
                // AF4's saved callback component has this exact runtime script
                // dependency. Do not admit plugin mounts, native Niagara parent
                // classes, Actor parents or arbitrary Niagara consumers here.
                R->SetBoolField(TEXT("native_niagara_dependency_observed"), true);
                const UPackage* NativePackage = FindObject<UPackage>(nullptr, TEXT("/Script/Niagara"));
                const UClass* Callback = FindObject<UClass>(nullptr, TEXT("/Script/Niagara.NiagaraParticleCallbackHandler"));
                const bool Verified = !bActorParent && IsValid(NativePackage) && !NativePackage->IsDirty()
                    && NativePackage->HasAnyPackageFlags(PKG_CompiledIn)
                    && !NativePackage->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly)
                    && IsValid(Callback) && Callback->GetOuter() == NativePackage
                    && Callback->HasAllClassFlags(CLASS_Native | CLASS_Interface)
                    && !Callback->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
                    && !Callback->IsEditorOnly() && Class->ImplementsInterface(Callback);
                R->SetStringField(TEXT("native_niagara_dependency_package"), Package);
                R->SetStringField(TEXT("native_niagara_callback_interface"), Callback ? Callback->GetPathName() : FString());
                R->SetBoolField(TEXT("native_niagara_component_interface_verified"), Verified);
                if (!Verified)
                { return Reject(TEXT("generated_component_dependencies_unsupported"), TEXT("Niagara dependency requires a saved generated non-scene component implementing the exact already-loaded native runtime NiagaraParticleCallbackHandler interface; Actor-parent and other Niagara dependency variants remain outside this slice.")); }
                continue;
            }
            // The actual IG saved BaseAI/ChildAI dependency graph contains
            // AIModule only. No GameplayTasks or other module is inferred here.
            if (bActorParent && AIControllerRoot && Package == TEXT("/Script/AIModule"))
            { AIModuleDependencyObserved = true; AIControllerProof->SetBoolField(TEXT("saved_aimodule_dependency_observed"), true); continue; }
            if (!Package.StartsWith(TEXT("/Game/")) && !Package.StartsWith(TEXT("/Engine/")) && !NativePackages.Contains(Package))
            { return Reject(TEXT("generated_component_dependencies_unsupported"), TEXT("Unaudited script module or plugin mount in saved dependencies: ") + Package); }
        }
        if (AIControllerRoot && !AIModuleDependencyObserved)
        { return Reject(TEXT("generated_component_dependencies_unsupported"), TEXT("Exact AIController ancestry requires its observed AIModule edge in the saved recursive dependency audit.")); }
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
    if (AIControllerRoot)
    {
        const bool SameNativeRoot = IsEditorGameThread() && !IsPlaySessionActive()
            && IsValid(AIControllerRoot) && IsValid(AIControllerPackage) && IsValid(AIControllerCDO)
            && AIControllerRoot == AAIController::StaticClass() && AIControllerRoot->GetDefaultObject(false) == AIControllerCDO
            && AIControllerRoot->GetOuter() == AIControllerPackage && AIControllerRoot->GetPackage() == AIControllerPackage
            && AIControllerRoot->GetOutermost() == AIControllerPackage
            && AIControllerRoot->GetPathName() == TEXT("/Script/AIModule.AIController")
            && AIControllerRoot->GetSuperClass() == AController::StaticClass()
            && AIControllerRoot->GetFlags() == AIControllerClassFlags && !AIControllerRoot->ClassGeneratedBy
            && AIControllerRoot->HasAnyClassFlags(CLASS_Native)
            && !AIControllerRoot->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
            && AIControllerPackage->GetName() == TEXT("/Script/AIModule") && !AIControllerPackage->IsDirty()
            && AIControllerPackage->HasAnyPackageFlags(PKG_CompiledIn)
            && !AIControllerPackage->HasAnyPackageFlags(PKG_EditorOnly | PKG_Developer | PKG_UncookedOnly | PKG_PlayInEditor)
            && AIControllerCDO->GetClass() == AIControllerRoot && AIControllerCDO->GetOuter() == AIControllerPackage
            && AIControllerCDO->GetPackage() == AIControllerPackage && AIControllerCDO->GetOutermost() == AIControllerPackage
            && AIControllerCDO->GetPathName() == TEXT("/Script/AIModule.Default__AIController")
            && AIControllerCDO->GetFlags() == AIControllerCDOFlags
            && AIControllerCDO->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject);
        AIControllerProof->SetBoolField(TEXT("root_and_cdo_identity_unchanged"), SameNativeRoot);
        Unchanged &= SameNativeRoot;
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
