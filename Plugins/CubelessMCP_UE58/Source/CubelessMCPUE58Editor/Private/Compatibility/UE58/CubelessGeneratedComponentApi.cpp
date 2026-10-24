#include "Compatibility/UE58/CubelessGeneratedComponentApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AIController.h"
#include "Components/ActorComponent.h"
#include "Components/SceneComponent.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SimpleConstructionScript.h"
#include "GameFramework/Actor.h"
#include "Misc/PackageName.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::GeneratedComponent
{
namespace
{
// Public resolved-path and shallow-slot pattern already used by YB/KI readers.
UObject* ResolvedParentPointer(const TObjectPtr<UObject>& Ref)
{
#if UE_WITH_OBJECT_HANDLE_LATE_RESOLVE
    if(!IsEditorGameThread()||IsPlaySessionActive()||!Ref.IsResolved()||Ref==nullptr)return nullptr;
    const FString Path=Ref.GetPathName();UObject* Found=StaticFindObject(nullptr,nullptr,*Path);
    return IsValid(Found)&&!Found->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed)
        &&IsValid(Found->GetClass())&&Ref==Found?Found:nullptr;
#else
    return nullptr; // Unverified resolution configurations remain incomplete.
#endif
}
bool ResolvedParentSuper(const UClass* Class,UClass*& Out)
{
    Out=nullptr;
    // UStruct::SuperStruct is a reflected object slot (Class.h). Public property
    // access copies its TObjectPtr without invoking tracked GetSuperClass().
    if(!IsValid(Class))return false;
    // The linked intrinsic CoreUObject descriptor is captured once; repeated
    // source-current checks must not traverse tracked native superclass slots.
    static FObjectProperty* const P=FindFProperty<FObjectProperty>(UStruct::StaticClass(),TEXT("SuperStruct"));
    if(!P||P->GetClass()!=FObjectProperty::StaticClass()||P->GetOwnerStruct()!=UStruct::StaticClass()
        ||P->GetOwnerProperty()!=P||P->ArrayDim!=1||P->ElementSize!=sizeof(TObjectPtr<UStruct>)
        ||P->GetSize()!=sizeof(TObjectPtr<UStruct>)||P->GetMinAlignment()!=alignof(TObjectPtr<UStruct>)
        ||P->GetOffset_ForInternal()<0||int64(P->GetOffset_ForInternal())+P->GetSize()>UStruct::StaticClass()->GetPropertiesSize()
        ||P->HasSetterOrGetter()||!P->PropertyClass.IsResolved()||P->PropertyClass!=UStruct::StaticClass())return false;
    const TObjectPtr<UObject> Slot=P->GetObjectPtrPropertyValue(P->ContainerPtrToValuePtr<void>(Class));
    if(!Slot.IsResolved())return false;
    if(Slot==nullptr)return true; // Actual resolved null terminates ancestry.
    Out=Cast<UClass>(ResolvedParentPointer(Slot));
    return Out!=nullptr; // Missing/invalid lookup is never a successful null.
}
TSharedRef<FJsonObject> InspectSavedRuntimeClass(UClass* Class, bool bActorParent, bool bObjectParent = false)
{
    auto R = MakeShared<FJsonObject>();
    R->SetBoolField(TEXT("success"), false); R->SetStringField(TEXT("stage"), TEXT("preflight"));
    R->SetBoolField(TEXT("loaded_during_inspection"), false);
    R->SetBoolField(TEXT("compile_requested"), false); R->SetBoolField(TEXT("save_requested"), false);
    R->SetBoolField(TEXT("native_niagara_dependency_observed"), false);
    R->SetBoolField(TEXT("native_niagara_component_interface_verified"), false);
    R->SetStringField(TEXT("validation_scope"), bObjectParent ? TEXT("exact loaded saved clean pure UObject-root generated Blueprint parent hierarchy; current generated/skeleton/actual nullable CDO/SCS roles and saved runtime dependencies only, no full authored-value or callback purity proof") : bActorParent
        ? TEXT("exact loaded saved clean Actor/ActorComponent/SceneComponent Blueprint parent chain; existing class/CDO source and saved recursive package dependencies, at most 256 packages; no source graph execution or full authored-value semantic proof")
        : TEXT("exact loaded, saved, clean non-scene ActorComponent Blueprint class; native ancestry and saved recursive package dependencies, at most 256 packages"));
    auto Reject = [&R](const TCHAR* Code, const FString& Message)
    { R->SetStringField(TEXT("error_code"), Code); R->SetStringField(TEXT("error"), Message); return false; };
    if (!IsEditorGameThread() || IsPlaySessionActive())
    { Reject(TEXT("generated_component_editor_state_invalid"), TEXT("Class inspection requires the editor game thread outside PIE/SIE.")); return R; }
    TStrongObjectPtr<UClass> RootClassHold(Class);
    // The historical parent-inspector name is shared by create/duplicate. This
    // role admits component parents; component insertion keeps its prior slice.
    const bool ComponentParent = bActorParent && IsValid(Class) && Class->IsChildOf(UActorComponent::StaticClass());
    const bool ObjectParent = bObjectParent && bActorParent && IsValid(Class) && !Class->IsChildOf(AActor::StaticClass()) && !Class->IsChildOf(UActorComponent::StaticClass());
    const bool ScopedParent = ComponentParent || ObjectParent;
    UBlueprint* BP = IsValid(Class) ? Cast<UBlueprint>(ScopedParent
        ? ResolvedParentPointer(Class->ClassGeneratedBy) : Class->ClassGeneratedBy.Get()) : nullptr;
    TStrongObjectPtr<UBlueprint> RootBlueprintHold(BP);
    if (!IsValid(BP) || !Cast<UBlueprintGeneratedClass>(Class) || (ObjectParent?(!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr().Get()!=Class):(ComponentParent?BP->GeneratedClass.GetGCPtr()!=Class:BP->GeneratedClass!=Class))
        || (!ScopedParent && Class->GetAuthoritativeClass() != Class) || Class->GetOutermost() != BP->GetOutermost()
        || Class->HasAnyClassFlags(CLASS_Native | CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists)
        || (bObjectParent ? !ObjectParent : bActorParent ? (!Class->IsChildOf(AActor::StaticClass()) && !ComponentParent)
            : (!Class->IsChildOf(UActorComponent::StaticClass()) || Class->IsChildOf(USceneComponent::StaticClass()))))
    { Reject(TEXT("generated_component_class_unsupported"), bActorParent
        ? TEXT("Use an exact current instantiable generated Actor/ActorComponent/SceneComponent parent class; native, skeleton, reinstanced and abstract classes remain outside this slice.")
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
    struct FComponentParentRole
    {
        TStrongObjectPtr<UClass> Class;TStrongObjectPtr<UBlueprint> Blueprint;
        TStrongObjectPtr<UClass> Parent,Skeleton;TStrongObjectPtr<UObject> CDO,SCS;TStrongObjectPtr<UClass> SkeletonParent;TStrongObjectPtr<UObject> SkeletonCDO,SkeletonCDOOuter;UPackage* SkeletonCDOPackage=nullptr;
        UObject* Outer=nullptr;UPackage* Package=nullptr;FString Path;
        UObject* CDOOuter=nullptr;UPackage* CDOPackage=nullptr;EBlueprintStatus Status=BS_Unknown;
        UObject* SCSOuter=nullptr;UPackage* SCSPackage=nullptr;UClass* SCSClass=nullptr;
    };
    TArray<TUniquePtr<FComponentParentRole>> ComponentParents;
    const auto ComponentCurrent=[&]()->bool
    {
        if(!ScopedParent)return true;
        if(!IsEditorGameThread()||IsPlaySessionActive()||!IsValid(Class)||!IsValid(BP)||(ObjectParent?(!BP->GeneratedClass.GetGCPtr().IsResolved()||BP->GeneratedClass.GetGCPtr().Get()!=Class):BP->GeneratedClass.GetGCPtr()!=Class))
            return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Component parent context or exact generated slot changed; no next inspection/factory or retry."));
        for(const auto& Entry:ComponentParents)
        {
            auto& E=*Entry;auto* K=E.Class.Get();auto* B=E.Blueprint.Get();UClass* Super=nullptr;
            if(ObjectParent)
            {
                if(!IsValid(K)||!IsValid(B)||!K->ClassGeneratedBy.IsResolved()||!K->ClassDefaultObject.IsResolved()
                    ||!B->GeneratedClass.GetGCPtr().IsResolved()||!B->ParentClass.GetGCPtr().IsResolved()
                    ||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved()||!B->SimpleConstructionScript.IsResolved()
                    ||!IsValid(E.Skeleton.Get())||!E.Skeleton->ClassGeneratedBy.IsResolved())
                    return Reject(TEXT("generated_object_parent_current_changed"),TEXT("Captured Object-parent roles must remain live with resolved source slots before current identity is read; no lazy resolution or replacement."));
                if(!IsValid(E.Skeleton.Get())||!E.Skeleton->ClassDefaultObject.IsResolved()||E.Skeleton->ClassDefaultObject.Get()!=E.SkeletonCDO.Get())return Reject(TEXT("generated_object_parent_current_changed"),TEXT("Captured current skeleton/nullable CDO slot changed during this inspection."));
                UClass* SkeletonSuper=nullptr;
                if(!ResolvedParentSuper(E.Skeleton.Get(),SkeletonSuper)||SkeletonSuper!=E.SkeletonParent.Get()||!IsValid(E.SkeletonParent.Get())||(E.SkeletonCDO.IsValid()&&(!IsValid(E.SkeletonCDO.Get())||E.SkeletonCDO->GetClass()!=E.Skeleton.Get()||E.SkeletonCDO->GetOuter()!=E.SkeletonCDOOuter.Get()||E.SkeletonCDO->GetPackage()!=E.SkeletonCDOPackage||!E.SkeletonCDO->HasAnyFlags(RF_ClassDefaultObject))))return Reject(TEXT("generated_object_parent_current_changed"),TEXT("Skeleton uses the actual native or parent-skeleton superclass; nullable CDO identity is independent of the generated CDO."));
            }
            if(!ResolvedParentSuper(K,Super))
                return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Current component superclass slot is unresolved or unreadable; no next inspection/factory."));
            if(!IsValid(K)||!IsValid(B)||K->HasAnyClassFlags(CLASS_Deprecated|CLASS_NewerVersionExists)||K->ClassGeneratedBy!=B||B->GeneratedClass.GetGCPtr()!=K
                ||B->ParentClass.GetGCPtr()!=E.Parent.Get()||Super!=E.Parent.Get()
                ||B->SkeletonGeneratedClass.GetGCPtr()!=E.Skeleton.Get()||B->SimpleConstructionScript!=E.SCS.Get()
                ||(E.Skeleton.IsValid()&&(!IsValid(E.Skeleton.Get())||E.Skeleton->ClassGeneratedBy!=B))
                ||(E.SCS.IsValid()&&(!IsValid(E.SCS.Get())||E.SCS->GetClass()!=E.SCSClass
                    ||E.SCS->GetOuter()!=E.SCSOuter||E.SCS->GetPackage()!=E.SCSPackage))
                ||K->ClassDefaultObject!=E.CDO.Get()||K->GetOuter()!=E.Outer||K->GetPackage()!=E.Package
                ||K->GetPathName()!=E.Path||B->GetPackage()!=E.Package||B->GetPackage()->IsDirty()
                ||B->Status!=E.Status||B->bBeingCompiled||B->bQueuedForCompilation||B->bIsRegeneratingOnLoad
                ||(E.CDO.IsValid()&&(!IsValid(E.CDO.Get())||E.CDO->GetClass()!=K
                    ||E.CDO->GetOuter()!=E.CDOOuter||E.CDO->GetPackage()!=E.CDOPackage
                    ||!E.CDO->HasAnyFlags(RF_ClassDefaultObject))))
                return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Captured component Blueprint/generated/skeleton/parent/CDO/optional-SCS role changed; no replacement is adopted."));
        }
        return true;
    };
    const bool Passed = [&]()
    {
        if (!Saved || WasDirty || !Compiled || BP->BlueprintType != BPTYPE_Normal || (ScopedParent?BP->ParentClass.GetGCPtr()==nullptr:!BP->ParentClass)
            || BP->bBeingCompiled || BP->bQueuedForCompilation || BP->bSuppressStructurallyModified || BP->bIsRegeneratingOnLoad)
        { return Reject(TEXT("generated_component_compile_save_required"), TEXT("Component Blueprint must be a saved clean normal /Game asset with an up-to-date compile and no queued/active structural work; this inspection never compiles or saves it.")); }
        // Capture component-parent roles before the shared SDK inspection. A
        // component Blueprint need not own an Actor construction script: stock
        // SupportsConstructionScript is Actor-based. Preserve its actual slot.
        if(ScopedParent)
        {
            TSet<UClass*> SeenComponents;
            for(auto* K=Class;K;)
            {
                if(SeenComponents.Num()>=128||SeenComponents.Contains(K)||!IsValid(K))
                    return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Bounded current component parent class chain required."));
                if(K->HasAnyClassFlags(CLASS_Native))
                {if(ObjectParent&&K!=UObject::StaticClass())return Reject(TEXT("generated_object_parent_native_root_pending"),TEXT("Only pure generated hierarchy ending directly at native CoreUObject.Object is admitted; other valid native bases remain incomplete."));break;}
                SeenComponents.Add(K);auto* B=Cast<UBlueprint>(ResolvedParentPointer(K->ClassGeneratedBy));
                if(!IsValid(B)||(ObjectParent?(!B->GeneratedClass.GetGCPtr().IsResolved()||B->GeneratedClass.GetGCPtr().Get()!=K):B->GeneratedClass.GetGCPtr()!=K)||!B->ParentClass.GetGCPtr().IsResolved()||B->ParentClass.GetGCPtr()==nullptr||!K->ClassDefaultObject.IsResolved()
                    ||!B->SimpleConstructionScript.IsResolved()||!B->SkeletonGeneratedClass.GetGCPtr().IsResolved())
                    return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Resolved current generated parent/CDO/optional SCS and skeleton slots required; no lazy load or CDO creation."));
                auto E=MakeUnique<FComponentParentRole>();E->Class.Reset(K);E->Blueprint.Reset(B);
                E->Parent.Reset(Cast<UClass>(ResolvedParentPointer(B->ParentClass.GetGCPtr())));E->Skeleton.Reset(Cast<UClass>(ResolvedParentPointer(B->SkeletonGeneratedClass.GetGCPtr())));
                E->CDO.Reset(ResolvedParentPointer(K->ClassDefaultObject));E->SCS.Reset(ResolvedParentPointer(B->SimpleConstructionScript));
                E->Outer=K->GetOuter();E->Package=K->GetPackage();E->Path=K->GetPathName();E->Status=B->Status;
                if(K->ClassDefaultObject!=E->CDO.Get()||B->SimpleConstructionScript!=E->SCS.Get()
                    ||B->SkeletonGeneratedClass.GetGCPtr()!=E->Skeleton.Get()||B->ParentClass.GetGCPtr()!=E->Parent.Get()
                    ||(E->CDO.IsValid()&&!IsValid(E->CDO.Get()))||(E->SCS.IsValid()&&!IsValid(E->SCS.Get()))
                    ||(E->Skeleton.IsValid()&&!IsValid(E->Skeleton.Get()))||!IsValid(E->Parent.Get()))
                    return Reject(TEXT("generated_component_parent_current_changed"),TEXT("Existing returned component parent roles must be live before observation."));
                if(ObjectParent)
                {
                    if(!IsValid(E->Skeleton.Get())||!E->Skeleton->ClassDefaultObject.IsResolved())return Reject(TEXT("generated_object_parent_skeleton_pending"),TEXT("Current resident skeleton and resolved nullable CDO slot required; no skeleton/CDO is created."));
                    E->SkeletonCDO.Reset(ResolvedParentPointer(E->Skeleton->ClassDefaultObject));
                    if(E->Skeleton->ClassDefaultObject.Get()!=E->SkeletonCDO.Get())return Reject(TEXT("generated_object_parent_skeleton_pending"),TEXT("Skeleton CDO must be the current resolved-null or loaded object."));
                    UClass* ExpectedSkeletonParent=E->Parent.Get();
                    if(!E->Parent->HasAnyClassFlags(CLASS_Native))
                    {
                        auto* ParentBlueprint=Cast<UBlueprint>(ResolvedParentPointer(E->Parent->ClassGeneratedBy));
                        if(!IsValid(ParentBlueprint)||!ParentBlueprint->SkeletonGeneratedClass.GetGCPtr().IsResolved())return Reject(TEXT("generated_object_parent_skeleton_pending"),TEXT("Generated parent requires an already-current parent skeleton; it is not the generated parent class."));
                        ExpectedSkeletonParent=Cast<UClass>(ResolvedParentPointer(ParentBlueprint->SkeletonGeneratedClass.GetGCPtr()));
                    }
                    E->SkeletonParent.Reset(ExpectedSkeletonParent);
                    if(!IsValid(ExpectedSkeletonParent))return Reject(TEXT("generated_object_parent_skeleton_pending"),TEXT("Actual native or parent-skeleton superclass unavailable."));
                    if(E->SkeletonCDO.IsValid()){E->SkeletonCDOOuter.Reset(E->SkeletonCDO->GetOuter());E->SkeletonCDOPackage=E->SkeletonCDO->GetPackage();}
                }
                if(E->CDO.IsValid()){E->CDOOuter=E->CDO->GetOuter();E->CDOPackage=E->CDO->GetPackage();}
                if(E->SCS.IsValid()){E->SCSOuter=E->SCS->GetOuter();E->SCSPackage=E->SCS->GetPackage();E->SCSClass=E->SCS->GetClass();}
                ComponentParents.Add(MoveTemp(E));if(!ComponentCurrent())return false;
                K=ComponentParents.Last()->Parent.Get();
            }
            if(ObjectParent){R->SetBoolField(TEXT("generated_object_parent_profile"),true);R->SetBoolField(TEXT("skeleton_parent_roles_observed"),true);R->SetBoolField(TEXT("generated_and_skeleton_cdo_equality_assumed"),false);R->SetBoolField(TEXT("source_authored_defaults_semantics_fully_observed"),false);}
            else R->SetBoolField(TEXT("generated_component_parent_profile"),true);
            R->SetBoolField(TEXT("source_cdo_creation_requested"),false);
            R->SetStringField(TEXT("component_parent_observation_scope"),TEXT("Create/duplicate parent source only: saved-clean ancestry, existing generated/skeleton/parent/CDO and actual nullable SCS slots, runtime dependency audit. No Actor SCS/Outer assumption, template creation, source graph execution, compile/save or full authored/default-value semantic observation."));
        }
        if(!ComponentCurrent())return false;
        const auto Parent = InspectBlueprintParent(Class->GetPathName()); R->SetObjectField(TEXT("parent_inspection"), Parent);
        if(!ComponentCurrent())return false;
        if (!Parent->GetBoolField(TEXT("success")))
        { return Reject(TEXT("generated_component_parent_unsupported"), TEXT("Generated component class failed the existing runtime parent/CDO inspection.")); }
        TArray<TSharedPtr<FJsonValue>> GeneratedParents, ParentPaths;
        TSet<const UClass*> Seen;
        for (const UClass* Current = Class; Current;)
        {
            if(!ComponentCurrent())return false;
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
                UBlueprint* Ancestor = Cast<UBlueprint>(ScopedParent?ResolvedParentPointer(Current->ClassGeneratedBy):Current->ClassGeneratedBy.Get());
                if (!IsValid(Ancestor) || !Cast<UBlueprintGeneratedClass>(Current) || (ScopedParent?Ancestor->GeneratedClass.GetGCPtr()!=Current:Ancestor->GeneratedClass!=Current)
                    || (!ScopedParent && Current->GetAuthoritativeClass() != Current) || Current->GetOutermost() != Ancestor->GetOutermost()
                    || !CurrentPackage.StartsWith(TEXT("/Game/")) || !FPackageName::DoesPackageExist(CurrentPackage)
                    || Ancestor->GetOutermost()->IsDirty() || Ancestor->BlueprintType != BPTYPE_Normal
                    || (Ancestor->Status != BS_UpToDate && Ancestor->Status != BS_UpToDateWithWarnings)
                    || Ancestor->bBeingCompiled || Ancestor->bQueuedForCompilation || Ancestor->bSuppressStructurallyModified || Ancestor->bIsRegeneratingOnLoad
                    || Current->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists))
                { return Reject(TEXT("generated_actor_parent_state_unsupported"), TEXT("Every generated parent ancestor must be saved, clean, authoritative, normally compiled and idle.")); }
                auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("class_path"), Current->GetPathName());
                Row->SetStringField(TEXT("generated_class"), ScopedParent?Current->GetPathName():Ancestor->GeneratedClass->GetPathName());
                Row->SetStringField(TEXT("blueprint_path"), Ancestor->GetPathName()); Row->SetStringField(TEXT("package_name"), CurrentPackage);
                Row->SetBoolField(TEXT("dirty"), false); Row->SetBoolField(TEXT("saved_asset"), true);
                Row->SetNumberField(TEXT("status"), static_cast<int32>(Ancestor->Status.GetValue()));
                GeneratedParents.Add(MakeShared<FJsonValueObject>(Row));
            }
            if(ScopedParent)
            {
                UClass* Next=nullptr;
                if(!ResolvedParentSuper(Current,Next))
                    return Reject(TEXT("generated_component_parent_unsupported"),TEXT("Superclass descriptor/resolved-path observation failed; native ancestry was not completed."));
                Current=Next;
            }
            else Current=Current->GetSuperClass();
        }
        if (bActorParent) { R->SetArrayField(TEXT("generated_parents"), GeneratedParents); R->SetArrayField(TEXT("parent_chain"), ParentPaths); }
        if(!ComponentCurrent())return false;
        const auto Audit = ReadPackageDependencies({PackageName}, true, 256); R->SetObjectField(TEXT("dependency_audit"), Audit);
        if(!ComponentCurrent())return false;
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
    if(ScopedParent&&!ComponentCurrent())
    {
        R->SetBoolField(TEXT("observed_state_unchanged"),false);
        R->SetBoolField(TEXT("inspection_receiver_or_source_lost"),true);
        return R;
    }
    bool Unchanged = BP->GetOutermost()->IsDirty() == WasDirty && BP->Status == OldStatus && (ScopedParent?BP->GeneratedClass.GetGCPtr()==Class:BP->GeneratedClass==Class);
    if (bActorParent && !ScopedParent && R->HasField(TEXT("generated_parents")))
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
TSharedRef<FJsonObject> InspectSavedObjectParent(UClass* Class) { return InspectSavedRuntimeClass(Class, true, true); }
}
