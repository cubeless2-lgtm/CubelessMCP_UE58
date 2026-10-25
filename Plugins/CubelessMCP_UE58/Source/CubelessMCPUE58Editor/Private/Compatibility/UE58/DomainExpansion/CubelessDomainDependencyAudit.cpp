// Port of the supplied UE58 serialized/live dependency validator. Editor-only.
#include "Compatibility/UE58/DomainExpansion/CubelessDomainDependencyAudit.h"
#include "Compatibility/UE58/DomainExpansion/CubelessDomainCommon.h"
#include "Engine/Blueprint.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Modules/ModuleManager.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::DomainExpansion
{
bool FDomainDependencyAudit::IsMCPDependencyReference(const FString& Value)
{
    FString Candidate = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    Candidate.TrimQuotesInline();
    if (Candidate.IsEmpty())
    {
        return false;
    }

    return Candidate.Contains(TEXT("/Script/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("MCPUnreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("mcp_unreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Script/CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigToolsEditor"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessMigrationFixtures"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("ModelContextProtocol"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("ToolsetRegistry"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("MCPClientToolset"), ESearchCase::IgnoreCase);
}

namespace
{
bool IsEngineOrScriptPackage(const FString& PackageName)
{
    return PackageName == TEXT("/Engine") ||
        PackageName.StartsWith(TEXT("/Engine/")) ||
        PackageName == TEXT("/Script") ||
        PackageName.StartsWith(TEXT("/Script/"));
}

bool IsAuthoredDependencyPackage(const FString& PackageName)
{
    return PackageName.StartsWith(TEXT("/")) &&
        !IsEngineOrScriptPackage(PackageName) &&
        !FPackageName::IsMemoryPackage(PackageName) &&
        !FPackageName::IsTempPackage(PackageName);
}

void AddExternalAuthoredPackage(
    const FString& SourcePackageName,
    const FString& ReferencedPackageName,
    TSet<FName>& OutExternalAuthoredPackages)
{
    if (!ReferencedPackageName.IsEmpty() && ReferencedPackageName.Len()<NAME_SIZE &&
        ReferencedPackageName != SourcePackageName &&
        IsAuthoredDependencyPackage(ReferencedPackageName))
    {
        OutExternalAuthoredPackages.Add(FName(*ReferencedPackageName));
    }
}

void CollectMCPObjectAuditTokens(
    const FString& Relationship,
    const UObject* Object,
    TArray<FString>& OutIssues)
{
    if (!Object)
    {
        return;
    }
    if(!SafeContext()||!Live(Object)){OutIssues.AddUnique(TEXT("Dependency audit encountered a retired object/context"));return;}

    const FString ObjectPath = Object->GetPathName();
    if (FDomainDependencyAudit::IsMCPDependencyReference(ObjectPath))
    {
        OutIssues.AddUnique(FString::Printf(
            TEXT("%s path contains MCP-only token: %s"),
            *Relationship,
            *ObjectPath));
    }

    for (const UClass* ObjectClass = Object->GetClass();
         ObjectClass;
         ObjectClass = ObjectClass->GetSuperClass())
    {
        const FString ClassPath = ObjectClass->GetPathName();
        if (FDomainDependencyAudit::IsMCPDependencyReference(ClassPath))
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("%s class hierarchy contains MCP-only token: %s -> %s"),
                *Relationship,
                *ObjectPath,
                *ClassPath));
        }
    }

    if (const UClass* ClassObject = Cast<UClass>(Object))
    {
        for (const UClass* SuperClass = ClassObject;
             SuperClass;
             SuperClass = SuperClass->GetSuperClass())
        {
            const FString SuperClassPath = SuperClass->GetPathName();
            if (FDomainDependencyAudit::IsMCPDependencyReference(SuperClassPath))
            {
                OutIssues.AddUnique(FString::Printf(
                    TEXT("%s class object hierarchy contains MCP-only token: %s -> %s"),
                    *Relationship,
                    *ObjectPath,
                    *SuperClassPath));
            }
        }
    }
}

bool CollectMCPPropertyReferenceAuditTokens(
    const UObject* SourceObject,
    const FString& SourcePackageName,
    int32& InOutPropertyValuesVisited,
    int32& InOutPropertyReferencesInspected,
    TSet<FName>& OutExternalAuthoredPackages,
    TArray<FString>& OutIssues)
{
    if (!SourceObject)
    {
        return true;
    }
    if(!SafeContext()||!Live(SourceObject)){OutIssues.AddUnique(TEXT("Dependency audit property owner is not live"));return false;}

    constexpr int32 MaxSerializedPropertyIteratorSteps = 65536;
    constexpr int32 MaxSerializedPropertyReferences = 16384;
    for (TPropertyValueIterator<const FProperty> It(
             SourceObject->GetClass(),
             SourceObject,
             EPropertyValueIteratorFlags::FullRecursion,
             EFieldIteratorFlags::IncludeDeprecated);
         It;
         ++It)
    {
        ++InOutPropertyValuesVisited;
        if (InOutPropertyValuesVisited > MaxSerializedPropertyIteratorSteps)
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete: package exceeded the %d-serialized-property-iterator safety bound: %s"),
                MaxSerializedPropertyIteratorSteps,
                *SourcePackageName));
            return false;
        }

        const FProperty* Property = It.Key();
        if (!Property)
        {
            continue;
        }
        if (Property->HasAnyPropertyFlags(
                CPF_Transient |
                CPF_DuplicateTransient |
                CPF_NonPIEDuplicateTransient |
                CPF_SkipSerialization))
        {
            It.SkipRecursiveProperty();
            continue;
        }

        const void* PropertyValue = It.Value();
        if (!PropertyValue)
        {
            continue;
        }
        FSoftObjectPath SoftReference;
        FTopLevelAssetPath TopLevelReference;
        bool bHasTopLevelReference = false;
        TArray<const UObject*> ReferencedObjects;
        bool bIsSerializedReferenceValue = false;
        if (const FSoftClassProperty* SoftClassProperty = CastField<FSoftClassProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            SoftReference = SoftClassProperty->GetPropertyValue(PropertyValue).ToSoftObjectPath();
        }
        else if (const FSoftObjectProperty* SoftObjectProperty = CastField<FSoftObjectProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            SoftReference = SoftObjectProperty->GetPropertyValue(PropertyValue).ToSoftObjectPath();
        }
        else if (const FStructProperty* StructProperty = CastField<FStructProperty>(Property))
        {
            if (StructProperty->Struct == TBaseStructure<FSoftObjectPath>::Get() ||
                StructProperty->Struct == TBaseStructure<FSoftClassPath>::Get())
            {
                bIsSerializedReferenceValue = true;
                SoftReference = *static_cast<const FSoftObjectPath*>(PropertyValue);
                It.SkipRecursiveProperty();
            }
            else if (StructProperty->Struct == TBaseStructure<FTopLevelAssetPath>::Get())
            {
                bIsSerializedReferenceValue = true;
                bHasTopLevelReference = true;
                TopLevelReference = *static_cast<const FTopLevelAssetPath*>(PropertyValue);
                It.SkipRecursiveProperty();
            }
        }
        else if (const FWeakObjectProperty* WeakObjectProperty = CastField<FWeakObjectProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            ReferencedObjects.Add(WeakObjectProperty->GetObjectPropertyValue(PropertyValue));
        }
        else if (const FLazyObjectProperty* LazyObjectProperty = CastField<FLazyObjectProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            ReferencedObjects.Add(LazyObjectProperty->GetObjectPropertyValue(PropertyValue));
        }
        else if (CastField<FDelegateProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            const FScriptDelegate& Delegate = *static_cast<const FScriptDelegate*>(PropertyValue);
            ReferencedObjects.Add(Delegate.GetUObject());
        }
        else if (const FMulticastDelegateProperty* MulticastProperty =
                     CastField<FMulticastDelegateProperty>(Property))
        {
            bIsSerializedReferenceValue = true;
            if (const FMulticastScriptDelegate* Delegates =
                    MulticastProperty->GetMulticastDelegate(PropertyValue))
            {
                for (const UObject* BoundObject : Delegates->GetAllObjectsEvenIfUnreachable())
                {
                    ReferencedObjects.Add(BoundObject);
                }
            }
        }

        if (!bIsSerializedReferenceValue)
        {
            continue;
        }
        ++InOutPropertyReferencesInspected;
        if (InOutPropertyReferencesInspected > MaxSerializedPropertyReferences)
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete: package exceeded the %d-serialized-property-reference safety bound: %s"),
                MaxSerializedPropertyReferences,
                *SourcePackageName));
            return false;
        }

        if (!SoftReference.IsNull())
        {
            const FString SoftReferencePath = SoftReference.ToString();
            if (FDomainDependencyAudit::IsMCPDependencyReference(SoftReferencePath))
            {
                OutIssues.AddUnique(FString::Printf(
                    TEXT("Serialized soft reference contains MCP-only token: %s.%s -> %s"),
                    *SourceObject->GetPathName(),
                    *Property->GetName(),
                    *SoftReferencePath));
            }
            AddExternalAuthoredPackage(
                SourcePackageName,
                SoftReference.GetLongPackageName(),
                OutExternalAuthoredPackages);
            if (const UObject* ResolvedObject = SoftReference.ResolveObject())
            {
                CollectMCPObjectAuditTokens(TEXT("Resolved serialized soft reference"), ResolvedObject, OutIssues);
            }
        }
        if (bHasTopLevelReference && !TopLevelReference.IsNull())
        {
            const FString TopLevelReferencePath = TopLevelReference.ToString();
            if (FDomainDependencyAudit::IsMCPDependencyReference(TopLevelReferencePath))
            {
                OutIssues.AddUnique(FString::Printf(
                    TEXT("Serialized top-level asset path contains MCP-only token: %s.%s -> %s"),
                    *SourceObject->GetPathName(),
                    *Property->GetName(),
                    *TopLevelReferencePath));
            }
            AddExternalAuthoredPackage(
                SourcePackageName,
                TopLevelReference.GetPackageName().ToString(),
                OutExternalAuthoredPackages);
        }
        for (const UObject* ReferencedObject : ReferencedObjects)
        {
            CollectMCPObjectAuditTokens(TEXT("Serialized weak or delegate reference"), ReferencedObject, OutIssues);
            if (ReferencedObject && ReferencedObject->GetOutermost())
            {
                AddExternalAuthoredPackage(
                    SourcePackageName,
                    ReferencedObject->GetOutermost()->GetName(),
                    OutExternalAuthoredPackages);
            }
        }
    }

    return true;
}

void CollectMCPSerializedReferenceAuditTokens(
    const FString& SourcePackageName,
    const TArray<UObject*>& SourceObjects,
    int32 SourceObjectCount,
    TSet<FName>& OutExternalAuthoredPackages,
    TArray<FString>& OutIssues)
{
    TArray<UObject*> DirectReferences;
    TArray<TStrongObjectPtr<UObject>> ReferenceHolds;
    FReferenceFinder ReferenceFinder(
        DirectReferences,
        nullptr,
        false,
        true,
        false,
        true);
    constexpr int32 MaxDirectReferences = 16384;
    int32 PropertyValuesVisited = 0;
    int32 PropertyReferencesInspected = 0;
    const int32 ObjectsToInspect = FMath::Min(SourceObjects.Num(), SourceObjectCount);
    for (int32 SourceIndex = 0; SourceIndex < ObjectsToInspect; ++SourceIndex)
    {
        UObject* SourceObject = SourceObjects[SourceIndex];
        if (!SourceObject)
        {
            continue;
        }

        if(!SafeContext()||!Live(SourceObject)){OutIssues.AddUnique(TEXT("Dependency audit source retired before serialization"));return;}
        TStrongObjectPtr<UObject> SourceHold(SourceObject);TStrongObjectPtr<UClass> SourceClass(SourceObject->GetClass());
        const FString SourcePath=SourceObject->GetPathName();const UObject* SourceOuter=SourceObject->GetOuter();
        ReferenceFinder.FindReferences(SourceObject);
        if(!SafeContext()||!Live(SourceObject)||SourceObject->GetClass()!=SourceClass.Get()||SourceObject->GetOuter()!=SourceOuter||SourceObject->GetPathName()!=SourcePath){OutIssues.AddUnique(TEXT("Dependency audit source/context changed during serialization; no later observer dispatched"));return;}
        if (DirectReferences.Num() > MaxDirectReferences)
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete: package exceeded the %d-direct-reference safety bound: %s"),
                MaxDirectReferences,
                *SourcePackageName));
            break;
        }
        // Custom serializers can invoke editor callbacks; retain references before the next serializer.
        for(int32 I=ReferenceHolds.Num();I<DirectReferences.Num();++I)
        {
            if(!Live(DirectReferences[I])){OutIssues.AddUnique(TEXT("Dependency audit serialized reference retired during callback"));return;}
            ReferenceHolds.Emplace(DirectReferences[I]);
        }
        if (!CollectMCPPropertyReferenceAuditTokens(
                SourceObject,
                SourcePackageName,
                PropertyValuesVisited,
                PropertyReferencesInspected,
                OutExternalAuthoredPackages,
                OutIssues))
        {
            break;
        }
    }

    const int32 ReferencesToInspect = FMath::Min(DirectReferences.Num(), MaxDirectReferences);
    for (int32 ReferenceIndex = 0; ReferenceIndex < ReferencesToInspect; ++ReferenceIndex)
    {
        const UObject* ReferencedObject = DirectReferences[ReferenceIndex];
        if(!SafeContext()||!Live(ReferencedObject)){OutIssues.AddUnique(TEXT("Dependency audit direct reference/context retired"));return;}
        CollectMCPObjectAuditTokens(TEXT("Serialized direct reference"), ReferencedObject, OutIssues);
        if (ReferencedObject && ReferencedObject->GetOutermost())
        {
            AddExternalAuthoredPackage(
                SourcePackageName,
                ReferencedObject->GetOutermost()->GetName(),
                OutExternalAuthoredPackages);
        }
    }
}

void CollectMCPPackageAuditTokens(
    UPackage* Package,
    TSet<FName>& OutExternalAuthoredPackages,
    TArray<FString>& OutIssues)
{
    if (!Package)
    {
        return;
    }
    if(!SafeContext()||!Live(Package)){OutIssues.AddUnique(TEXT("Dependency audit package/context retired"));return;}

    const FString PackageName = Package->GetName();
    if (FDomainDependencyAudit::IsMCPDependencyReference(PackageName))
    {
        OutIssues.AddUnique(FString::Printf(TEXT("Package path contains MCP-only token: %s"), *PackageName));
    }

    TArray<UObject*> PackageExports;
    GetObjectsWithPackage(Package, PackageExports, true, RF_Transient, EInternalObjectFlags::Garbage);

    constexpr int32 MaxPackageExports = 16384;
    const int32 ExportCountToInspect = FMath::Min(PackageExports.Num(), MaxPackageExports);
    TStrongObjectPtr<UPackage> HoldPackage(Package);TArray<TStrongObjectPtr<UObject>> ExportHolds;
    for(int32 I=0;I<ExportCountToInspect;++I)if(Live(PackageExports[I]))ExportHolds.Emplace(PackageExports[I]);
    if (PackageExports.Num() > MaxPackageExports)
    {
        OutIssues.AddUnique(FString::Printf(
            TEXT("Dependency audit infrastructure incomplete: package exceeded the %d-export safety bound: %s"),
            MaxPackageExports,
            *PackageName));
    }

    for (int32 ExportIndex = 0; ExportIndex < ExportCountToInspect; ++ExportIndex)
    {
        UObject* Export = PackageExports[ExportIndex];
        if (!Export)
        {
            continue;
        }

        CollectMCPObjectAuditTokens(TEXT("Package export"), Export, OutIssues);
    }
    CollectMCPSerializedReferenceAuditTokens(
        PackageName,
        PackageExports,
        ExportCountToInspect,
        OutExternalAuthoredPackages,
        OutIssues);
}

void CollectLoadedAuthoredPackageAuditIssues(
    UPackage* RootPackage,
    TSet<FName>& OutExternalAuthoredPackages,
    TSet<FName>& OutInMemoryAuditedPackages,
    TArray<FString>& OutIssues)
{
    if (!RootPackage)
    {
        return;
    }

    TArray<UPackage*> Pending{RootPackage};
    TArray<TStrongObjectPtr<UPackage>> PendingHolds;PendingHolds.Emplace(RootPackage);
    TSet<FName> Queued{RootPackage->GetFName()};
    constexpr int32 MaxInMemoryPackages = 256;
    int32 PendingIndex = 0;
    while (PendingIndex < Pending.Num() && OutInMemoryAuditedPackages.Num() < MaxInMemoryPackages)
    {
        UPackage* CurrentPackage = Pending[PendingIndex++];
        if(!SafeContext()||!Live(CurrentPackage)){OutIssues.AddUnique(TEXT("Dependency audit queued package/context retired"));return;}
        if (!CurrentPackage || OutInMemoryAuditedPackages.Contains(CurrentPackage->GetFName()))
        {
            continue;
        }

        OutInMemoryAuditedPackages.Add(CurrentPackage->GetFName());
        TSet<FName> DirectExternalPackages;
        CollectMCPPackageAuditTokens(CurrentPackage, DirectExternalPackages, OutIssues);
        if(!SafeContext()||!Live(CurrentPackage)||!OutIssues.IsEmpty()){return;}
        for (const FName& ExternalPackageName : DirectExternalPackages)
        {
            OutExternalAuthoredPackages.Add(ExternalPackageName);
            if (!Queued.Contains(ExternalPackageName))
            {
                if (UPackage* LoadedPackage = FindPackage(nullptr, *ExternalPackageName.ToString()))
                {
                    if(!Live(LoadedPackage)){OutIssues.AddUnique(TEXT("Dependency audit external package retired"));return;}
                    FString PersistedFilename;
                    const bool bHasPersistedPackage = FPackageName::DoesPackageExist(
                        ExternalPackageName.ToString(),
                        &PersistedFilename);
                    if (LoadedPackage->IsDirty() || !bHasPersistedPackage)
                    {
                        Queued.Add(ExternalPackageName);
                        Pending.Add(LoadedPackage);
                        PendingHolds.Emplace(LoadedPackage);
                    }
                }
            }
        }
    }
    if (PendingIndex < Pending.Num())
    {
        OutIssues.AddUnique(FString::Printf(
            TEXT("Dependency audit infrastructure incomplete: loaded package graph exceeded the %d-package safety bound"),
            MaxInMemoryPackages));
    }
}

void CollectTransitiveMCPPackageDependencyIssues(
    const FString& FieldName,
    const FString& RootPackageName,
    const FString& RootPackageFilename,
    TSet<FName>& InOutAuditedPackages,
    TArray<FString>& OutIssues)
{
    const FName RootPackageFName(*RootPackageName);
    if (InOutAuditedPackages.Contains(RootPackageFName))
    {
        return;
    }

    FAssetRegistryModule& AssetRegistryModule =
        FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    if(!SafeContext()){OutIssues.AddUnique(TEXT("Dependency audit context changed loading registry"));return;}
    AssetRegistryModule.Get().ScanModifiedAssetFiles({RootPackageFilename});
    if(!SafeContext()){OutIssues.AddUnique(TEXT("Dependency audit context changed during registry scan"));return;}
    TArray<FName> Pending{RootPackageFName};
    TSet<FName> Queued = InOutAuditedPackages;
    Queued.Add(RootPackageFName);
    constexpr int32 MaxDependencyPackages = 4096;
    int32 PendingIndex = 0;
    while (PendingIndex < Pending.Num() && InOutAuditedPackages.Num() < MaxDependencyPackages)
    {
        const FName Current = Pending[PendingIndex++];
        if (InOutAuditedPackages.Contains(Current))
        {
            continue;
        }

        TArray<FName> Dependencies;
        bool bDependenciesAvailable = AssetRegistryModule.Get().GetDependencies(Current, Dependencies);
        if(!SafeContext()){OutIssues.AddUnique(TEXT("Dependency audit context changed querying registry"));return;}
        if (!bDependenciesAvailable)
        {
            FString CurrentPackageFilename;
            if (FPackageName::DoesPackageExist(Current.ToString(), &CurrentPackageFilename))
            {
                AssetRegistryModule.Get().ScanModifiedAssetFiles({CurrentPackageFilename});
                if(!SafeContext()){OutIssues.AddUnique(TEXT("Dependency audit context changed rescanning registry"));return;}
                Dependencies.Reset();
                bDependenciesAvailable = AssetRegistryModule.Get().GetDependencies(Current, Dependencies);
                if(!SafeContext()){OutIssues.AddUnique(TEXT("Dependency audit context changed requerying registry"));return;}
            }
        }
        if (!bDependenciesAvailable)
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete for %s: could not inspect package after a synchronous registry scan: %s"),
                *FieldName,
                *Current.ToString()));
            continue;
        }
        InOutAuditedPackages.Add(Current);
        for (const FName& Dependency : Dependencies)
        {
            const FString DependencyPath = Dependency.ToString();
            if (FDomainDependencyAudit::IsMCPDependencyReference(DependencyPath))
            {
                OutIssues.AddUnique(FString::Printf(
                    TEXT("%s has an MCP-only package dependency: %s"),
                    *FieldName,
                    *DependencyPath));
            }
            if (IsAuthoredDependencyPackage(DependencyPath) && !Queued.Contains(Dependency))
            {
                Queued.Add(Dependency);
                Pending.Add(Dependency);
            }
        }
    }
    if (PendingIndex < Pending.Num())
    {
        OutIssues.AddUnique(FString::Printf(
            TEXT("Dependency audit infrastructure incomplete for %s: persisted graph exceeded the %d-package safety bound"),
            *FieldName,
            MaxDependencyPackages));
    }
}
}

void FDomainDependencyAudit::CollectMCPDependencyAuditIssues(
    const FString& FieldName,
    const UObject* ReferencedObject,
    EDomainAuditMode AuditMode,
    TArray<FString>& OutIssues)
{
    if (!ReferencedObject)
    {
        return;
    }

    if(!SafeContext()||!Live(ReferencedObject)){OutIssues.AddUnique(TEXT("Dependency audit requires a live object in an idle Editor game-thread context"));return;}
    TStrongObjectPtr<UObject> RootHold(const_cast<UObject*>(ReferencedObject));
    TStrongObjectPtr<UPackage> PackageHold(ReferencedObject->GetOutermost());
    TStrongObjectPtr<UClass> ClassHold(ReferencedObject->GetClass());
    const FString RootPath=ReferencedObject->GetPathName();
    auto RootCurrent=[&]{return SafeContext()&&Live(RootHold.Get())&&Live(PackageHold.Get())&&Live(ClassHold.Get())&&RootHold->GetClass()==ClassHold.Get()&&RootHold->GetOutermost()==PackageHold.Get()&&RootHold->GetPathName()==RootPath;};
    CollectMCPObjectAuditTokens(FieldName, ReferencedObject, OutIssues);

    UPackage* RootPackage = ReferencedObject->GetOutermost();
    const FString RootPackageName = RootPackage ? RootPackage->GetName() : FString();
    TSet<FName> ExternalAuthoredPackages;
    TSet<FName> InMemoryAuditedPackages;
    if (RootPackage && IsAuthoredDependencyPackage(RootPackageName))
    {
        CollectLoadedAuthoredPackageAuditIssues(
            RootPackage,
            ExternalAuthoredPackages,
            InMemoryAuditedPackages,
            OutIssues);
    }
    else if (ReferencedObject)
    {
        TArray<UObject*> ReferencedObjects{const_cast<UObject*>(ReferencedObject)};
        CollectMCPSerializedReferenceAuditTokens(
            RootPackageName,
            ReferencedObjects,
            ReferencedObjects.Num(),
            ExternalAuthoredPackages,
            OutIssues);
    }
    TSet<FName> PersistedAuditedPackages;
    if(!RootCurrent()||!OutIssues.IsEmpty()){if(!RootCurrent())OutIssues.AddUnique(TEXT("Dependency audit root/context changed before persisted traversal"));return;}
    if (AuditMode == EDomainAuditMode::RequirePersisted)
    {
        if (RootPackageName.IsEmpty() || !RootPackageName.StartsWith(TEXT("/")))
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete for %s: a named persisted package is required"),
                *FieldName));
            return;
        }
        if (IsAuthoredDependencyPackage(RootPackageName))
        {
            FString RootPackageFilename;
            if (!FPackageName::DoesPackageExist(RootPackageName, &RootPackageFilename))
            {
                OutIssues.AddUnique(FString::Printf(
                    TEXT("Dependency audit infrastructure incomplete for %s: the root package has no on-disk state to inspect: %s"),
                    *FieldName,
                    *RootPackageName));
                return;
            }
            CollectTransitiveMCPPackageDependencyIssues(
                FieldName,
                RootPackageName,
                RootPackageFilename,
                PersistedAuditedPackages,
                OutIssues);
        }
    }

    if(!RootCurrent()){OutIssues.AddUnique(TEXT("Dependency audit root/context changed during serialized or registry callbacks"));return;}
    // An unpersisted root is allowed in InMemoryOnly mode, but every external
    // authored package must still be proven from disk or from its loaded dirty state.
    ExternalAuthoredPackages.Remove(FName(*RootPackageName));
    for (const FName& ExternalPackageName : ExternalAuthoredPackages)
    {
        FString ExternalPackageFilename;
        if (PersistedAuditedPackages.Contains(ExternalPackageName))
        {
            continue;
        }
        if (FPackageName::DoesPackageExist(ExternalPackageName.ToString(), &ExternalPackageFilename))
        {
            CollectTransitiveMCPPackageDependencyIssues(
                FieldName,
                ExternalPackageName.ToString(),
                ExternalPackageFilename,
                PersistedAuditedPackages,
                OutIssues);
        }
        else if (!InMemoryAuditedPackages.Contains(ExternalPackageName))
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete for %s: external authored package is neither persisted nor loaded for an in-memory audit: %s"),
                *FieldName,
                *ExternalPackageName.ToString()));
        }
    }
    if(!RootCurrent())OutIssues.AddUnique(TEXT("Dependency audit root/context changed before completion"));
}

bool FDomainDependencyAudit::ValidateMCPDependencyIndependence(
    const FString& FieldName,
    const UObject* ReferencedObject,
    EDomainAuditMode AuditMode,
    FString& OutError)
{
    if (!ReferencedObject)
    {
        OutError = FString::Printf(
            TEXT("Refusing '%s' because the referenced object is null"),
            *FieldName);
        return false;
    }

    TArray<FString> Issues;
    CollectMCPDependencyAuditIssues(FieldName, ReferencedObject, AuditMode, Issues);
    if (Issues.IsEmpty())
    {
        return true;
    }

    OutError = FString::Printf(
        TEXT("Refusing '%s' because its dependency audit failed: %s"),
        *FieldName,
        *Issues[0]);
    return false;
}


}
