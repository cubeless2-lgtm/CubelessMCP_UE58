#include "Compatibility/UE58/CubelessBlueprintNetworkingApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Compatibility/UE58/CubelessFunctionPinApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/World.h"
#include "Engine/Level.h"
#include "Engine/LevelScriptBlueprint.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/CompilerResultsLog.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "Modules/ModuleManager.h"
#include "PluginDescriptor.h"
#include "ScopedTransaction.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"

namespace Cubeless::UE58::BlueprintNetworking
{
namespace
{
using Variable::FResult;
void Count(FResult& R,const TCHAR* K,int32 N=1) { auto J=R.Data->GetObjectField(TEXT("native_call_counts"));J->SetNumberField(K,J->GetNumberField(K)+N); }
bool Fail(FResult& R,const TCHAR* Code,const FString& Error,bool Unknown=false)
{
    R.bSuccess=R.bPostconditionPassed=false;R.ErrorCode=Code;R.Error=Error;if(Unknown) { R.bChangeStateKnown=false; }R.Data->SetStringField(TEXT("failed_stage"),R.Stage);R.Data->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted);return false;
}
bool Loaded(const UObject* O) { return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_BeginDestroyed|RF_FinishDestroyed); }
bool SafeText(const FString& S,int32 Limit) { if(S.Len()>Limit) { return false; }for(int32 I=0;I<S.Len();++I) { if(S[I]==0) { return false; } }return true; }
void Init(FResult& R)
{
    auto C=MakeShared<FJsonObject>();for(const TCHAR* K:{TEXT("asset_load_calls"),TEXT("asset_load_returns"),TEXT("cdo_get_calls"),TEXT("cdo_get_returns"),TEXT("cdo_identity_observation_calls"),TEXT("settings_observation_calls"),TEXT("public_getter_calls"),TEXT("public_field_read_calls"),TEXT("nullable_parse_calls"),TEXT("nullable_parse_returns"),TEXT("outer_transaction_calls"),TEXT("blueprint_modify_calls"),TEXT("cdo_modify_calls"),TEXT("setter_calls"),TEXT("setter_returns"),TEXT("public_field_write_calls"),TEXT("mark_modified_calls"),TEXT("mark_dirty_calls"),TEXT("full_compile_calls"),TEXT("full_compile_returns"),TEXT("forced_save_calls"),TEXT("forced_save_returns"),TEXT("dependency_audit_calls"),TEXT("dependency_audit_returns")}) { C->SetNumberField(K,0); }R.Data->SetObjectField(TEXT("native_call_counts"),C);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("automatic_replay_allowed"),false);R.Data->SetBoolField(TEXT("runtime_multiplayer_verified"),false);R.Data->SetBoolField(TEXT("cold_reload_verified"),false);R.Data->SetStringField(TEXT("count_scope"),TEXT("Direct wrapper/Actor/compiler/save/audit calls; recursive audit/serialization/registry/SDK callbacks are retained source-qualified work, not fabricated zero counts."));
}
bool IsMCPDependencyReference(const FString& Value)
{
    FString S=FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();S.TrimQuotesInline();
    for(const TCHAR* T:{TEXT("UnrealMCP"),TEXT("MCPUnreal"),TEXT("mcp_unreal"),TEXT("CubelessControlRigTools")}) { if(S.Contains(T,ESearchCase::IgnoreCase)) { return true; } }
    // Current tooling identities extend the original legacy guard; harmless /Game/CubelessMCPTests names do not match.
    for(const TCHAR* P:{TEXT("/Script/CubelessMCP"),TEXT("/CubelessMCP_UE58/"),TEXT("/Script/ToolsetRegistry"),TEXT("/ToolsetRegistry/"),TEXT("/Script/ModelContextProtocol"),TEXT("/ModelContextProtocol/"),TEXT("/Script/PythonScriptPlugin"),TEXT("/PythonScriptPlugin/"),TEXT("/Script/EditorScriptingUtilities"),TEXT("/EditorScriptingUtilities/")}) { if(S.StartsWith(P,ESearchCase::IgnoreCase)) { return true; } }
    for(const TCHAR* Name:{TEXT("CubelessMCP_UE58"),TEXT("ModelContextProtocol"),TEXT("ToolsetRegistry"),TEXT("PythonScriptPlugin"),TEXT("EditorScriptingUtilities")}) { if(const TSharedPtr<IPlugin> Plugin=IPluginManager::Get().FindPlugin(Name)) { for(const auto& Module:Plugin->GetDescriptor().Modules) { if(S.StartsWith(TEXT("/Script/")+Module.Name.ToString(),ESearchCase::IgnoreCase)) { return true; } } } }return false;
}
constexpr float NetworkingFloatTolerance = 0.0001f;

struct FActorNetworkingSettings
{
    bool bReplicates = false;
    bool bReplicateMovement = false;
    ENetDormancy NetDormancy = DORM_Awake;
    bool bAlwaysRelevant = false;
    bool bOnlyRelevantToOwner = false;
    float NetUpdateFrequency = 0.0f;
    float MinNetUpdateFrequency = 0.0f;
    float NetPriority = 0.0f;
    float NetCullDistanceSquared = 0.0f;
};

struct FBoolSettingField
{
    const TCHAR* Name;
    bool FActorNetworkingSettings::* Member;
};

struct FFloatSettingField
{
    const TCHAR* Name;
    float FActorNetworkingSettings::* Member;
};

struct FDormancyOption
{
    const TCHAR* Name;
    ENetDormancy Value;
};

const FBoolSettingField BoolSettingFields[] = {
    {TEXT("replicates"), &FActorNetworkingSettings::bReplicates},
    {TEXT("replicate_movement"), &FActorNetworkingSettings::bReplicateMovement},
    {TEXT("always_relevant"), &FActorNetworkingSettings::bAlwaysRelevant},
    {TEXT("only_relevant_to_owner"), &FActorNetworkingSettings::bOnlyRelevantToOwner},
};

const FFloatSettingField FloatSettingFields[] = {
    {TEXT("net_update_frequency"), &FActorNetworkingSettings::NetUpdateFrequency},
    {TEXT("min_net_update_frequency"), &FActorNetworkingSettings::MinNetUpdateFrequency},
    {TEXT("net_priority"), &FActorNetworkingSettings::NetPriority},
    {TEXT("net_cull_distance_squared"), &FActorNetworkingSettings::NetCullDistanceSquared},
};

const FDormancyOption DormancyOptions[] = {
    {TEXT("DORM_Never"), DORM_Never},
    {TEXT("DORM_Awake"), DORM_Awake},
    {TEXT("DORM_DormantAll"), DORM_DormantAll},
    {TEXT("DORM_DormantPartial"), DORM_DormantPartial},
    {TEXT("DORM_Initial"), DORM_Initial},
};

FString DormancyToString(ENetDormancy Value)
{
    for (const FDormancyOption& Option : DormancyOptions)
    {
        if (Option.Value == Value)
        {
            return Option.Name;
        }
    }
    return TEXT("DORM_MAX");
}

bool TryParseDormancy(const FString& Name, ENetDormancy& OutValue)
{
    for (const FDormancyOption& Option : DormancyOptions)
    {
        if (Name.Equals(Option.Name, ESearchCase::IgnoreCase))
        {
            OutValue = Option.Value;
            return true;
        }
    }
    return false;
}

FString BlueprintStatusToString(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Dirty: return TEXT("dirty");
    case BS_Error: return TEXT("error");
    case BS_UpToDate: return TEXT("up_to_date");
    case BS_BeingCreated: return TEXT("being_created");
    case BS_UpToDateWithWarnings: return TEXT("up_to_date_with_warnings");
    default: return TEXT("unknown");
    }
}

FActorNetworkingSettings ReadSettings(const AActor* ActorCDO)
{
    FActorNetworkingSettings Value;
    if (!ActorCDO)
    {
        return Value;
    }
    Value.bReplicates = ActorCDO->GetIsReplicated();
    Value.bReplicateMovement = ActorCDO->IsReplicatingMovement();
    Value.NetDormancy = ActorCDO->NetDormancy;
    Value.bAlwaysRelevant = ActorCDO->bAlwaysRelevant;
    Value.bOnlyRelevantToOwner = ActorCDO->bOnlyRelevantToOwner;
    Value.NetUpdateFrequency = ActorCDO->GetNetUpdateFrequency();
    Value.MinNetUpdateFrequency = ActorCDO->GetMinNetUpdateFrequency();
    Value.NetPriority = ActorCDO->NetPriority;
    Value.NetCullDistanceSquared = ActorCDO->GetNetCullDistanceSquared();
    return Value;
}

TSharedPtr<FJsonObject> SettingsToJson(const FActorNetworkingSettings& Value)
{
    TSharedPtr<FJsonObject> Json = MakeShared<FJsonObject>();
    Json->SetBoolField(TEXT("replicates"), Value.bReplicates);
    Json->SetBoolField(TEXT("replicate_movement"), Value.bReplicateMovement);
    Json->SetStringField(TEXT("net_dormancy"), DormancyToString(Value.NetDormancy));
    Json->SetBoolField(TEXT("always_relevant"), Value.bAlwaysRelevant);
    Json->SetBoolField(TEXT("only_relevant_to_owner"), Value.bOnlyRelevantToOwner);
    if(FMath::IsFinite(Value.NetUpdateFrequency)) { Json->SetNumberField(TEXT("net_update_frequency"), Value.NetUpdateFrequency); }else { Json->SetField(TEXT("net_update_frequency"),MakeShared<FJsonValueNull>()); }
    if(FMath::IsFinite(Value.MinNetUpdateFrequency)) { Json->SetNumberField(TEXT("min_net_update_frequency"), Value.MinNetUpdateFrequency); }else { Json->SetField(TEXT("min_net_update_frequency"),MakeShared<FJsonValueNull>()); }
    if(FMath::IsFinite(Value.NetPriority)) { Json->SetNumberField(TEXT("net_priority"), Value.NetPriority); }else { Json->SetField(TEXT("net_priority"),MakeShared<FJsonValueNull>()); }
    if(FMath::IsFinite(Value.NetCullDistanceSquared)) { Json->SetNumberField(TEXT("net_cull_distance_squared"), Value.NetCullDistanceSquared); }else { Json->SetField(TEXT("net_cull_distance_squared"),MakeShared<FJsonValueNull>()); }
    return Json;
}

bool ParseRequestedSettings(
    const TSharedPtr<FJsonObject>& Params,
    const FActorNetworkingSettings& Current,
    FActorNetworkingSettings& Out,
    TArray<FString>& OutProvided,
    FString& OutError)
{
    Out = Current;
    for (const FBoolSettingField& Field : BoolSettingFields)
    {
        if (!Params->HasField(Field.Name))
        {
            continue;
        }
        if (!Params->HasTypedField<EJson::Boolean>(Field.Name))
        {
            OutError = FString::Printf(TEXT("'%s' must be a boolean"), Field.Name);
            return false;
        }
        Out.*(Field.Member) = Params->GetBoolField(Field.Name);
        OutProvided.Add(Field.Name);
    }
    for (const FFloatSettingField& Field : FloatSettingFields)
    {
        if (!Params->HasField(Field.Name))
        {
            continue;
        }
        if (!Params->HasTypedField<EJson::Number>(Field.Name))
        {
            OutError = FString::Printf(TEXT("'%s' must be a number"), Field.Name);
            return false;
        }
        const double Number = Params->GetNumberField(Field.Name);
        if (!FMath::IsFinite(Number) || FMath::Abs(Number) > TNumericLimits<float>::Max())
        {
            OutError = FString::Printf(TEXT("'%s' must be a finite float value"), Field.Name);
            return false;
        }
        Out.*(Field.Member) = static_cast<float>(Number);
        OutProvided.Add(Field.Name);
    }

    if (Params->HasField(TEXT("net_dormancy")))
    {
        if (!Params->HasTypedField<EJson::String>(TEXT("net_dormancy")))
        {
            OutError = TEXT("'net_dormancy' must be a string");
            return false;
        }
        const FString Name = Params->GetStringField(TEXT("net_dormancy"));
        if (!TryParseDormancy(Name, Out.NetDormancy))
        {
            OutError = TEXT("Invalid 'net_dormancy'; use a UE 5.6 DORM_* value documented by this tool");
            return false;
        }
        OutProvided.Add(TEXT("net_dormancy"));
    }
    if (OutProvided.IsEmpty())
    {
        OutError = TEXT("At least one networking setting must be provided");
        return false;
    }

    const bool bUpdateProvided = OutProvided.Contains(TEXT("net_update_frequency"));
    const bool bMinUpdateProvided = OutProvided.Contains(TEXT("min_net_update_frequency"));
    if (bUpdateProvided && Out.NetUpdateFrequency <= 0.0f)
    {
        OutError = TEXT("'net_update_frequency' must be greater than zero");
        return false;
    }
    if ((bMinUpdateProvided && Out.MinNetUpdateFrequency < 0.0f) ||
        ((bUpdateProvided || bMinUpdateProvided) && Out.MinNetUpdateFrequency > Out.NetUpdateFrequency))
    {
        OutError = TEXT("'min_net_update_frequency' must be non-negative and no greater than 'net_update_frequency'");
        return false;
    }
    if (OutProvided.Contains(TEXT("net_priority")) && Out.NetPriority <= 0.0f)
    {
        OutError = TEXT("'net_priority' must be greater than zero");
        return false;
    }
    if (OutProvided.Contains(TEXT("net_cull_distance_squared")) && Out.NetCullDistanceSquared < 0.0f)
    {
        OutError = TEXT("'net_cull_distance_squared' must be non-negative");
        return false;
    }
    if ((OutProvided.Contains(TEXT("always_relevant")) || OutProvided.Contains(TEXT("only_relevant_to_owner"))) &&
        Out.bAlwaysRelevant && Out.bOnlyRelevantToOwner)
    {
        OutError = TEXT("always_relevant and only_relevant_to_owner are mutually exclusive; disable the old mode in the same request");
        return false;
    }
    return true;
}

bool JsonFieldMatches(
    const TSharedPtr<FJsonObject>& Left,
    const TSharedPtr<FJsonObject>& Right,
    const FString& Name)
{
    const TSharedPtr<FJsonValue> L = Left->TryGetField(Name);
    const TSharedPtr<FJsonValue> R = Right->TryGetField(Name);
    if (!L.IsValid() || !R.IsValid() || L->Type != R->Type)
    {
        return false;
    }
    return L->Type == EJson::Number
        ? FMath::IsNearlyEqual(L->AsNumber(), R->AsNumber(), static_cast<double>(NetworkingFloatTolerance))
        : FJsonValue::CompareEqual(*L, *R);
}

TArray<FString> GetMismatchedFields(
    const FActorNetworkingSettings& Actual,
    const FActorNetworkingSettings& Expected,
    const TArray<FString>& Fields)
{
    const TSharedPtr<FJsonObject> ActualJson = SettingsToJson(Actual);
    const TSharedPtr<FJsonObject> ExpectedJson = SettingsToJson(Expected);
    TArray<FString> Mismatches;
    for (const FString& Field : Fields)
    {
        if (!JsonFieldMatches(ActualJson, ExpectedJson, Field))
        {
            Mismatches.Add(Field);
        }
    }
    return Mismatches;
}

TArray<TSharedPtr<FJsonValue>> StringsToJson(const TArray<FString>& Values)
{
    TArray<TSharedPtr<FJsonValue>> Result;
    for (const FString& Value : Values)
    {
        Result.Add(MakeShared<FJsonValueString>(Value));
    }
    return Result;
}

TSharedPtr<FJsonObject> BuildDelta(const TArray<FString>& Provided, const TArray<FString>& Changed)
{
    TSharedPtr<FJsonObject> Delta = MakeShared<FJsonObject>();
    Delta->SetNumberField(TEXT("affected_count"), Changed.IsEmpty() ? 0 : 1);
    Delta->SetNumberField(TEXT("updated_count"), Changed.IsEmpty() ? 0 : 1);
    Delta->SetNumberField(TEXT("skipped_count"), Provided.Num() - Changed.Num());
    Delta->SetNumberField(TEXT("requested_field_count"), Provided.Num());
    Delta->SetNumberField(TEXT("changed_field_count"), Changed.Num());
    Delta->SetArrayField(TEXT("changed_fields"), StringsToJson(Changed));
    return Delta;
}


enum class EAuditMode { InMemoryOnly, RequirePersisted };
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
    if (!ReferencedPackageName.IsEmpty() &&
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

    const FString ObjectPath = Object->GetPathName();
    if (IsMCPDependencyReference(ObjectPath))
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
        if (IsMCPDependencyReference(ClassPath))
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
            if (IsMCPDependencyReference(SuperClassPath))
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
            if (IsMCPDependencyReference(SoftReferencePath))
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
            if (IsMCPDependencyReference(TopLevelReferencePath))
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

        ReferenceFinder.FindReferences(SourceObject);
        if (DirectReferences.Num() > MaxDirectReferences)
        {
            OutIssues.AddUnique(FString::Printf(
                TEXT("Dependency audit infrastructure incomplete: package exceeded the %d-direct-reference safety bound: %s"),
                MaxDirectReferences,
                *SourcePackageName));
            break;
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

    const FString PackageName = Package->GetName();
    if (IsMCPDependencyReference(PackageName))
    {
        OutIssues.AddUnique(FString::Printf(TEXT("Package path contains MCP-only token: %s"), *PackageName));
    }

    TArray<UObject*> PackageExports;
    GetObjectsWithPackage(Package, PackageExports, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);

    constexpr int32 MaxPackageExports = 16384;
    const int32 ExportCountToInspect = FMath::Min(PackageExports.Num(), MaxPackageExports);
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
    TSet<FName> Queued{RootPackage->GetFName()};
    constexpr int32 MaxInMemoryPackages = 256;
    int32 PendingIndex = 0;
    while (PendingIndex < Pending.Num() && OutInMemoryAuditedPackages.Num() < MaxInMemoryPackages)
    {
        UPackage* CurrentPackage = Pending[PendingIndex++];
        if (!CurrentPackage || OutInMemoryAuditedPackages.Contains(CurrentPackage->GetFName()))
        {
            continue;
        }

        OutInMemoryAuditedPackages.Add(CurrentPackage->GetFName());
        TSet<FName> DirectExternalPackages;
        CollectMCPPackageAuditTokens(CurrentPackage, DirectExternalPackages, OutIssues);
        for (const FName& ExternalPackageName : DirectExternalPackages)
        {
            OutExternalAuthoredPackages.Add(ExternalPackageName);
            if (!Queued.Contains(ExternalPackageName))
            {
                if (UPackage* LoadedPackage = FindPackage(nullptr, *ExternalPackageName.ToString()))
                {
                    FString PersistedFilename;
                    const bool bHasPersistedPackage = FPackageName::DoesPackageExist(
                        ExternalPackageName.ToString(),
                        &PersistedFilename);
                    if (LoadedPackage->IsDirty() || !bHasPersistedPackage)
                    {
                        Queued.Add(ExternalPackageName);
                        Pending.Add(LoadedPackage);
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
    AssetRegistryModule.Get().ScanModifiedAssetFiles({RootPackageFilename});
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
        if (!bDependenciesAvailable)
        {
            FString CurrentPackageFilename;
            if (FPackageName::DoesPackageExist(Current.ToString(), &CurrentPackageFilename))
            {
                AssetRegistryModule.Get().ScanModifiedAssetFiles({CurrentPackageFilename});
                Dependencies.Reset();
                bDependenciesAvailable = AssetRegistryModule.Get().GetDependencies(Current, Dependencies);
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
            if (IsMCPDependencyReference(DependencyPath))
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
void CollectMCPDependencyAuditIssues(
    const FString& FieldName,
    const UObject* ReferencedObject,
    EAuditMode AuditMode,
    TArray<FString>& OutIssues)
{
    if (!ReferencedObject)
    {
        return;
    }

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
    if (AuditMode == EAuditMode::RequirePersisted)
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
}
TSharedPtr<FJsonObject> CreateNetworkingError(
    const FString& ErrorCode,
    const FString& Message,
    bool bMutationAttempted = false,
    bool bMutationPerformed = false)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error_code"), ErrorCode);
    Result->SetStringField(TEXT("error"), Message);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetBoolField(TEXT("mutation_attempted"), bMutationAttempted);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);
    Result->SetBoolField(TEXT("changed"), bMutationPerformed);
    return Result;
}

struct FResolvedActorBlueprint
{
    FString RequestedName;
    UBlueprint* Blueprint=nullptr;
    AActor* ActorCDO=nullptr;
    TStrongObjectPtr<UBlueprint> BlueprintHold;
    TStrongObjectPtr<UClass> ClassHold;
    TStrongObjectPtr<AActor> CdoHold;
};
template<class T> T* CountedLoad(const FString& Path,FResult& R)
{
    Count(R,TEXT("asset_load_calls"));T* O=LoadObject<T>(nullptr,*Path);Count(R,TEXT("asset_load_returns"));return O;
}
FString NormalizeBlueprintObjectPath(const FString& BlueprintNameOrPath)
{
    FString Path = FPackageName::ExportTextPathToObjectPath(BlueprintNameOrPath).TrimStartAndEnd();
    Path.TrimQuotesInline();

    if (Path.EndsWith(TEXT("_C")))
    {
        Path.LeftChopInline(2);
    }

    if ((Path.StartsWith(TEXT("/Game/")) || Path.StartsWith(TEXT("/Engine/"))) && !Path.Contains(TEXT(".")))
    {
        const FString AssetName = FPackageName::GetShortName(Path);
        Path = FString::Printf(TEXT("%s.%s"), *Path, *AssetName);
    }

    return Path;
}
UBlueprint* LoadBlueprintFromObjectPath(const FString& ObjectPath,FResult& R)
{
    if (ObjectPath.IsEmpty())
    {
        return nullptr;
    }

    const FString PackageName = FPackageName::ObjectPathToPackageName(ObjectPath);
    FText InvalidPackageReason;
    if (PackageName.IsEmpty() ||
        ObjectPath.Contains(TEXT("//")) ||
        PackageName.Contains(TEXT("//")) ||
        !FPackageName::IsValidLongPackageName(PackageName, true, &InvalidPackageReason))
    {
        UE_LOG(LogTemp, Warning, TEXT("Skipping invalid blueprint object path '%s': %s"), *ObjectPath, *InvalidPackageReason.ToString());
        return nullptr;
    }

    if (UBlueprint* Blueprint = CountedLoad<UBlueprint>(ObjectPath,R))
    {
        return Blueprint;
    }

    FString ClassPath = ObjectPath;
    if (!ClassPath.EndsWith(TEXT("_C")))
    {
        ClassPath += TEXT("_C");
    }

    if (UClass* BlueprintClass = CountedLoad<UClass>(ClassPath,R))
    {
        return Cast<UBlueprint>(BlueprintClass->ClassGeneratedBy);
    }

    return nullptr;
}

UBlueprint* LoadExistingLevelBlueprintFromWorldPath(const FString& ObjectPath,FResult& R)
{
    if(ObjectPath.IsEmpty()) { return nullptr; }
    UWorld* World=FindObject<UWorld>(nullptr,*ObjectPath);
    if(!World) { World=CountedLoad<UWorld>(ObjectPath,R); }
    if(!Loaded(World)||!Loaded(World->PersistentLevel)) { return nullptr; }
    // Actual SDK argument is bDontCreate. Preserve the original true=no-create behavior.
    if(ULevelScriptBlueprint* B=World->PersistentLevel->GetLevelScriptBlueprint(true)) { return B; }
    return FindObject<ULevelScriptBlueprint>(World->PersistentLevel,*World->GetName());
}
TArray<FString> FindBlueprintAssetPaths(const FString& BlueprintNameOrPath,FResult& R)
{
    TArray<FString> CandidatePaths;
    const FString Query = NormalizeBlueprintObjectPath(BlueprintNameOrPath);

    if (Query.StartsWith(TEXT("/Game/")) || Query.StartsWith(TEXT("/Engine/")))
    {
        if (LoadBlueprintFromObjectPath(Query,R))
        {
            CandidatePaths.Add(Query);
            return CandidatePaths;
        }
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> BlueprintAssets;
    AssetRegistryModule.Get().GetAssetsByClass(UBlueprint::StaticClass()->GetClassPathName(), BlueprintAssets, true);

    const FString ShortQuery = FPackageName::GetShortName(Query);
    for (const FAssetData& AssetData : BlueprintAssets)
    {
        const FString AssetName = AssetData.AssetName.ToString();
        const FString PackageName = AssetData.PackageName.ToString();
        const FString ObjectPath = AssetData.GetObjectPathString();

        if (AssetName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            AssetName.Equals(ShortQuery, ESearchCase::IgnoreCase) ||
            PackageName.Equals(BlueprintNameOrPath, ESearchCase::IgnoreCase) ||
            ObjectPath.Equals(Query, ESearchCase::IgnoreCase))
        {
            CandidatePaths.Add(ObjectPath);
        }
    }

    return CandidatePaths;
}
UBlueprint* FindBlueprintByName(const FString& BlueprintName,FResult& R)
{
    const FString NormalizedPath = NormalizeBlueprintObjectPath(BlueprintName);
    if (UBlueprint* DirectBlueprint = LoadBlueprintFromObjectPath(NormalizedPath,R))
    {
        return DirectBlueprint;
    }

    if (UBlueprint* LevelBlueprint = LoadExistingLevelBlueprintFromWorldPath(NormalizedPath,R))
    {
        return LevelBlueprint;
    }

    const bool bIsPathLikeInput = BlueprintName.Contains(TEXT("/")) || BlueprintName.Contains(TEXT("."));
    if (!bIsPathLikeInput)
    {
        const FString LegacyObjectPath = FString::Printf(TEXT("/Game/Blueprints/%s.%s"), *BlueprintName, *BlueprintName);
        if (UBlueprint* LegacyBlueprint = LoadBlueprintFromObjectPath(LegacyObjectPath,R))
        {
            return LegacyBlueprint;
        }
    }

    const TArray<FString> CandidatePaths = FindBlueprintAssetPaths(BlueprintName,R);
    if (CandidatePaths.Num() == 1)
    {
        return LoadBlueprintFromObjectPath(CandidatePaths[0],R);
    }

    if (CandidatePaths.Num() > 1)
    {
        UE_LOG(LogTemp, Warning, TEXT("Ambiguous blueprint name '%s'. Use a full asset path. Candidates:"), *BlueprintName);
        for (const FString& CandidatePath : CandidatePaths)
        {
            UE_LOG(LogTemp, Warning, TEXT("  - %s"), *CandidatePath);
        }
    }

    return nullptr;
}

AActor* AcquireCDO(UBlueprint* B,FResult& R,TStrongObjectPtr<UClass>& ClassHold,TStrongObjectPtr<AActor>& Hold,const TCHAR* Phase)
{
    auto J=MakeShared<FJsonObject>();R.Data->SetObjectField(Phase,J);
    UClass* C=Loaded(B)?B->GeneratedClass.Get():nullptr;
    J->SetBoolField(TEXT("generated_class_loaded"),Loaded(C));
    if(!Loaded(C)||!C->IsChildOf(AActor::StaticClass())) { J->SetBoolField(TEXT("cdo_available"),false);return nullptr; }
    ClassHold.Reset(C);Count(R,TEXT("cdo_get_calls"));AActor* A=Cast<AActor>(C->GetDefaultObject());Count(R,TEXT("cdo_get_returns"));
    Hold.Reset(A);Count(R,TEXT("cdo_identity_observation_calls"));
    const bool Valid=Loaded(A)&&A->HasAnyFlags(RF_ClassDefaultObject)&&A->GetClass()==C;
    J->SetBoolField(TEXT("cdo_available"),Valid);J->SetStringField(TEXT("generated_class"),C->GetPathName());
    if(A) { J->SetStringField(TEXT("cdo_path"),A->GetPathName());J->SetStringField(TEXT("cdo_class"),A->GetClass()->GetPathName()); }
    return Valid?A:nullptr;
}
FActorNetworkingSettings ObserveSettings(AActor* A,FResult& R,const TCHAR* Phase)
{
    Count(R,TEXT("settings_observation_calls"));
    if(A) { Count(R,TEXT("public_getter_calls"),5);Count(R,TEXT("public_field_read_calls"),4); }
    auto Value=ReadSettings(A);R.Data->SetObjectField(Phase,SettingsToJson(Value));return Value;
}
bool FiniteSettings(const FActorNetworkingSettings& V)
{
    return FMath::IsFinite(V.NetUpdateFrequency)&&FMath::IsFinite(V.MinNetUpdateFrequency)&&FMath::IsFinite(V.NetPriority)&&FMath::IsFinite(V.NetCullDistanceSquared);
}
bool ResolveActorBlueprint(
    const TSharedPtr<FJsonObject>& Params,
    FResolvedActorBlueprint& Out,
    TSharedPtr<FJsonObject>& OutError,FResult& R)
{
    if (!Params.IsValid() || !Params->HasField(TEXT("blueprint_name")))
    {
        OutError = CreateNetworkingError(TEXT("missing_blueprint_name"), TEXT("Missing non-empty 'blueprint_name' parameter"));
        return false;
    }
    if (!Params->HasTypedField<EJson::String>(TEXT("blueprint_name")))
    {
        OutError = CreateNetworkingError(TEXT("invalid_parameter"), TEXT("'blueprint_name' must be a string"));
        return false;
    }
    Out.RequestedName = Params->GetStringField(TEXT("blueprint_name"));
    Out.RequestedName.TrimStartAndEndInline();
    if (Out.RequestedName.IsEmpty())
    {
        OutError = CreateNetworkingError(TEXT("missing_blueprint_name"), TEXT("Missing non-empty 'blueprint_name' parameter"));
        return false;
    }
    if (!SafeText(Out.RequestedName,NAME_SIZE-1))
    {
        OutError = CreateNetworkingError(
            TEXT("invalid_parameter"),
            TEXT("'blueprint_name' exceeds the Unreal name limit"));
        return false;
    }
    if (IsMCPDependencyReference(Out.RequestedName))
    {
        OutError = CreateNetworkingError(
            TEXT("mcp_dependency_reference_rejected"),
            TEXT("Blueprint target references an Editor-only MCP/tooling package"));
        return false;
    }

    Out.Blueprint = FindBlueprintByName(Out.RequestedName,R);
    Out.BlueprintHold.Reset(Out.Blueprint);
    if (!Loaded(Out.Blueprint))
    {
        OutError = CreateNetworkingError(TEXT("blueprint_not_found"), FString::Printf(TEXT("Blueprint not found: %s"), *Out.RequestedName));
        return false;
    }
    if (!Loaded(Out.Blueprint->GeneratedClass.Get()) || !Out.Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
    {
        OutError = CreateNetworkingError(
            TEXT("blueprint_not_actor_class"),
            FString::Printf(TEXT("Blueprint does not generate an AActor class: %s"), *Out.Blueprint->GetPathName()));
        return false;
    }

    Out.ActorCDO = AcquireCDO(Out.Blueprint,R,Out.ClassHold,Out.CdoHold,TEXT("initial_cdo_identity"));
    if (!Out.ActorCDO)
    {
        OutError = CreateNetworkingError(TEXT("actor_cdo_unavailable"), TEXT("Failed to resolve Actor class default object"));
        return false;
    }
    return true;
}
void AddMutationResultContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const TSharedPtr<FJsonObject>& Before,
    const TSharedPtr<FJsonObject>& After,
    const TSharedPtr<FJsonObject>& Delta,
    bool bVerificationPassed,
    const FString& VerificationMethod,
    const TArray<FString>& VerificationChecks,
    bool bStrictSuccess=true)
{
    if (!Result.IsValid())
    {
        return;
    }

    bool bLegacySuccess = true;
    if (Result->HasTypedField<EJson::Boolean>(TEXT("success")))
    {
        bLegacySuccess = Result->GetBoolField(TEXT("success"));
    }

    Result->SetStringField(TEXT("contract_version"), TEXT("mutation_result_v1"));
    Result->SetStringField(TEXT("mode"), bDryRun ? TEXT("dry_run") : TEXT("apply"));
    Result->SetBoolField(TEXT("dry_run"), bDryRun);
    Result->SetBoolField(TEXT("DRY_RUN"), bDryRun);
    Result->SetBoolField(TEXT("changed"), bChanged);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);

    TSharedPtr<FJsonObject> Mutation = MakeShared<FJsonObject>();
    Mutation->SetBoolField(TEXT("attempted"), !bDryRun);
    Mutation->SetBoolField(TEXT("performed"), bMutationPerformed);
    Mutation->SetStringField(
        TEXT("state"),
        bMutationPerformed ? TEXT("performed") : TEXT("not_performed"));
    Result->SetObjectField(TEXT("mutation"), Mutation);

    if (Before.IsValid())
    {
        Result->SetObjectField(TEXT("before"), Before);
    }
    if (After.IsValid())
    {
        Result->SetObjectField(TEXT("after"), After);
    }

    TSharedPtr<FJsonObject> NormalizedDelta = Delta.IsValid()
        ? Delta
        : MakeShared<FJsonObject>();
    static const TCHAR* CanonicalCountFields[] =
    {
        TEXT("affected_count"),
        TEXT("created_count"),
        TEXT("updated_count"),
        TEXT("removed_count"),
        TEXT("skipped_count"),
        TEXT("failed_count")
    };
    for (const TCHAR* FieldName : CanonicalCountFields)
    {
        if (!NormalizedDelta->HasField(FieldName))
        {
            NormalizedDelta->SetField(FieldName, MakeShared<FJsonValueNull>());
        }
    }
    Result->SetObjectField(TEXT("delta"), NormalizedDelta);

    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(
        TEXT("status"),
        bVerificationPassed ? TEXT("passed") : TEXT("failed"));
    Verification->SetStringField(TEXT("method"), VerificationMethod);
    Verification->SetBoolField(TEXT("before_observed"), Before.IsValid());
    Verification->SetBoolField(TEXT("after_observed"), After.IsValid());
    Verification->SetBoolField(TEXT("strict"), bStrictSuccess);
    TArray<TSharedPtr<FJsonValue>> CheckValues;
    CheckValues.Reserve(VerificationChecks.Num());
    for (const FString& Check : VerificationChecks)
    {
        CheckValues.Add(MakeShared<FJsonValueString>(Check));
    }
    Verification->SetArrayField(TEXT("checks"), CheckValues);
    Result->SetObjectField(TEXT("verification"), Verification);

    const bool bVerifiedSuccess = bLegacySuccess && bVerificationPassed;
    Result->SetBoolField(TEXT("verified_success"), bVerifiedSuccess);
    FString SuccessSemantics = TEXT("legacy_execution");
    if (bDryRun)
    {
        SuccessSemantics = bVerifiedSuccess ? TEXT("verified_dry_run") : TEXT("failed_dry_run");
    }
    else if (bVerifiedSuccess)
    {
        SuccessSemantics = TEXT("verified_postcondition");
    }
    else if (bStrictSuccess && bLegacySuccess)
    {
        SuccessSemantics = TEXT("postcondition_failed");
    }
    else if (!bLegacySuccess)
    {
        SuccessSemantics = TEXT("failed_execution");
    }
    Result->SetStringField(TEXT("success_semantics"), SuccessSemantics);

    if (!Result->HasField(TEXT("success")))
    {
        Result->SetBoolField(TEXT("success"), bLegacySuccess);
    }
    if (bStrictSuccess && bLegacySuccess && !bVerificationPassed)
    {
        Result->SetBoolField(TEXT("legacy_success"), true);
        Result->SetBoolField(TEXT("success"), false);
        Result->SetStringField(TEXT("error_code"), TEXT("mutation_postcondition_failed"));
        if (!Result->HasField(TEXT("error")))
        {
            Result->SetStringField(
                TEXT("error"),
                TEXT("Mutation postcondition failed independent readback"));
        }
    }
}
void AddTarget(const FResolvedActorBlueprint& Target, const TSharedPtr<FJsonObject>& Result)
{
    Result->SetStringField(TEXT("blueprint_name"), Target.RequestedName);
    Result->SetStringField(TEXT("asset_path"), Target.Blueprint->GetPathName());
    Result->SetStringField(TEXT("generated_class_path"), Target.Blueprint->GeneratedClass->GetPathName());
}
void AddContract(
    const TSharedPtr<FJsonObject>& Result,
    bool bDryRun,
    bool bMutationPerformed,
    bool bChanged,
    const FActorNetworkingSettings& Before,
    const FActorNetworkingSettings& After,
    const TArray<FString>& Provided,
    const TArray<FString>& Changed,
    bool bVerified,
    const FString& Method,
    const TArray<FString>& Checks)
{
    AddMutationResultContract(
        Result,
        bDryRun,
        bMutationPerformed,
        bChanged,
        SettingsToJson(Before),
        SettingsToJson(After),
        BuildDelta(Provided, Changed),
        bVerified,
        Method,
        Checks);
}
void SetMutationFailure(
    const TSharedPtr<FJsonObject>& Result,
    const FString& ErrorCode,
    const FString& Message,
    const FString& FailedStage,
    bool bStateUnknown)
{
    Result->SetBoolField(TEXT("success"), false);
    Result->SetStringField(TEXT("error_code"), ErrorCode);
    Result->SetStringField(TEXT("error"), Message);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetBoolField(TEXT("partial_target_modified"), true);
    Result->SetBoolField(TEXT("partial_mutation"), true);
    Result->SetStringField(TEXT("failed_stage"), FailedStage);
    Result->SetStringField(TEXT("failed_stage_effect"), TEXT("mutation"));
    Result->SetBoolField(TEXT("mutation_attempted"), true);
    Result->SetBoolField(TEXT("failed_stage_mutation_performed"), true);
    Result->SetBoolField(TEXT("mutation_state_unknown"), bStateUnknown);
    Result->SetBoolField(TEXT("do_not_retry"), true);
    Result->SetBoolField(TEXT("workflow_recovery_required"), true);
    Result->SetStringField(
        TEXT("recovery_guidance"),
        TEXT("Inspect the dirty Blueprint, then undo/revert or correct and save it manually before retrying."));
}

bool ValidateMCPDependencyIndependence(const FString& Field,const UObject* Object,EAuditMode Mode,FString& Error,FResult& R,const TCHAR* Phase)
{
    auto Receipt=MakeShared<FJsonObject>();R.Data->SetObjectField(Phase,Receipt);Receipt->SetStringField(TEXT("mode"),Mode==EAuditMode::RequirePersisted?TEXT("require_persisted"):TEXT("in_memory_only"));
    Count(R,TEXT("dependency_audit_calls"));TArray<FString> Issues;CollectMCPDependencyAuditIssues(Field,Object,Mode,Issues);Count(R,TEXT("dependency_audit_returns"));
    Receipt->SetArrayField(TEXT("issues"),StringsToJson(Issues));Receipt->SetBoolField(TEXT("passed"),Issues.IsEmpty());
    if(!Issues.IsEmpty()) { Error=Issues[0];return false; }return true;
}
void ApplySettings(AActor* ActorCDO,const FActorNetworkingSettings& Value,const TArray<FString>& Changed,FResult& R)
{
    auto Returns=MakeShared<FJsonObject>();R.Data->SetObjectField(TEXT("setting_write_receipts"),Returns);
    if(Changed.Contains(TEXT("replicates"))) { Returns->SetStringField(TEXT("replicates"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetReplicates(Value.bReplicates);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("replicates"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("replicate_movement"))) { Returns->SetStringField(TEXT("replicate_movement"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetReplicatingMovement(Value.bReplicateMovement);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("replicate_movement"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("net_dormancy"))) { Returns->SetStringField(TEXT("net_dormancy"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetNetDormancy(Value.NetDormancy);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("net_dormancy"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("always_relevant"))) { Returns->SetStringField(TEXT("always_relevant"),TEXT("attempted"));Count(R,TEXT("public_field_write_calls"));ActorCDO->bAlwaysRelevant = Value.bAlwaysRelevant;Returns->SetStringField(TEXT("always_relevant"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("only_relevant_to_owner"))) { Returns->SetStringField(TEXT("only_relevant_to_owner"),TEXT("attempted"));Count(R,TEXT("public_field_write_calls"));ActorCDO->bOnlyRelevantToOwner = Value.bOnlyRelevantToOwner;Returns->SetStringField(TEXT("only_relevant_to_owner"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("net_update_frequency"))) { Returns->SetStringField(TEXT("net_update_frequency"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetNetUpdateFrequency(Value.NetUpdateFrequency);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("net_update_frequency"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("min_net_update_frequency"))) { Returns->SetStringField(TEXT("min_net_update_frequency"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetMinNetUpdateFrequency(Value.MinNetUpdateFrequency);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("min_net_update_frequency"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("net_priority"))) { Returns->SetStringField(TEXT("net_priority"),TEXT("attempted"));Count(R,TEXT("public_field_write_calls"));ActorCDO->NetPriority = Value.NetPriority;Returns->SetStringField(TEXT("net_priority"),TEXT("returned"));R.bChanged=true; }
    if(Changed.Contains(TEXT("net_cull_distance_squared"))) { Returns->SetStringField(TEXT("net_cull_distance_squared"),TEXT("attempted"));Count(R,TEXT("setter_calls"));ActorCDO->SetNetCullDistanceSquared(Value.NetCullDistanceSquared);Count(R,TEXT("setter_returns"));Returns->SetStringField(TEXT("net_cull_distance_squared"),TEXT("returned"));R.bChanged=true; }
}
TSharedPtr<FJsonObject> RunInspect(
    const TSharedPtr<FJsonObject>& Params,FResult& R)
{
    FResolvedActorBlueprint Target;
    TSharedPtr<FJsonObject> Error;
    if (!ResolveActorBlueprint(Params, Target, Error,R))
    {
        return Error;
    }

    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("legacy_result"),Result);
    Result->SetBoolField(TEXT("success"), true);
    AddTarget(Target, Result);
    const FActorNetworkingSettings Inspected=ObserveSettings(Target.ActorCDO,R,TEXT("inspected_settings"));
    if(!FiniteSettings(Inspected)) { return CreateNetworkingError(TEXT("nonfinite_current_settings"),TEXT("Current Actor networking float fields must be finite")); }
    Result->SetObjectField(TEXT("settings"), SettingsToJson(Inspected));
    Result->SetStringField(TEXT("blueprint_status"), BlueprintStatusToString(Target.Blueprint->Status));
    Result->SetBoolField(TEXT("package_dirty"), Target.Blueprint->GetOutermost()->IsDirty());
    return Result;
}
bool DecodeCarriers(const TArray<FString>& Values,TSharedRef<FJsonObject> Params,FResult& R);
TSharedPtr<FJsonObject> RunSet(
    const TSharedPtr<FJsonObject>& Params,FResult& R,const TArray<FString>& Carriers)
{
    if (IsPlaySessionActive())
    {
        return CreateNetworkingError(TEXT("play_session_active"), TEXT("Cannot edit Blueprint networking defaults while PIE/SIE is active"));
    }

    FResolvedActorBlueprint Target;
    TSharedPtr<FJsonObject> Error;
    if (!ResolveActorBlueprint(Params, Target, Error,R))
    {
        return Error;
    }

    const FString ResolvedPackageName = Target.Blueprint->GetOutermost()->GetName();
    const bool bIsGamePackage =
        ResolvedPackageName.Equals(TEXT("/Game"), ESearchCase::CaseSensitive) ||
        ResolvedPackageName.StartsWith(TEXT("/Game/"), ESearchCase::CaseSensitive);
    if (!bIsGamePackage)
    {
        TSharedPtr<FJsonObject> Result = CreateNetworkingError(
            TEXT("networking_non_project_asset_mutation_blocked"),
            FString::Printf(
                TEXT("Networking settings may only be authored on /Game Blueprint packages: %s"),
                *ResolvedPackageName));
        AddTarget(Target, Result);
        Result->SetStringField(TEXT("resolved_package_name"), ResolvedPackageName);
        return Result;
    }

    bool bDryRun = true;
    if (Params->HasField(TEXT("dry_run")))
    {
        if (!Params->HasTypedField<EJson::Boolean>(TEXT("dry_run")))
        {
            return CreateNetworkingError(TEXT("invalid_parameter"), TEXT("'dry_run' must be a boolean"));
        }
        bDryRun = Params->GetBoolField(TEXT("dry_run"));
    }

    if(!DecodeCarriers(Carriers,Params.ToSharedRef(),R)) { return CreateNetworkingError(R.ErrorCode,R.Error); }
    const FActorNetworkingSettings Before = ObserveSettings(Target.ActorCDO,R,TEXT("before_settings"));
    if(!FiniteSettings(Before)) { return CreateNetworkingError(TEXT("nonfinite_current_settings"),TEXT("Current Actor networking float fields must be finite")); }
    FActorNetworkingSettings Requested;
    TArray<FString> Provided;
    FString ValidationError;
    if (!ParseRequestedSettings(Params, Before, Requested, Provided, ValidationError))
    {
        TSharedPtr<FJsonObject> Result = CreateNetworkingError(TEXT("invalid_networking_settings"), ValidationError);
        AddTarget(Target, Result);
        return Result;
    }

    const TArray<FString> Changed = GetMismatchedFields(Before, Requested, Provided);
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    R.Data->SetObjectField(TEXT("legacy_result"),Result);
    Result->SetBoolField(TEXT("success"), true);
    AddTarget(Target, Result);
    Result->SetArrayField(TEXT("requested_fields"), StringsToJson(Provided));

    if (bDryRun || Changed.IsEmpty())
    {
        Result->SetBoolField(TEXT("would_change"), !Changed.IsEmpty());
        Result->SetArrayField(TEXT("changed_fields"), StringsToJson(Changed));
        Result->SetStringField(
            TEXT("message"),
            bDryRun ? TEXT("Networking settings validated; no mutation was performed")
                    : TEXT("Networking settings already match the request"));
        Result->SetBoolField(TEXT("compiled"), false);
        Result->SetBoolField(TEXT("saved"), false);
        AddContract(
            Result,
            bDryRun,
            false,
            false,
            Before,
            Before,
            Provided,
            TArray<FString>{},
            true,
            bDryRun ? TEXT("projected_ue_5_6_actor_cdo_settings") : TEXT("actor_cdo_readback_noop"),
            bDryRun ? TArray<FString>{TEXT("parameters_valid")} : TArray<FString>{TEXT("requested_fields_already_match")});
        return Result;
    }

    FString PreMutationAuditError;
    if (Target.Blueprint->GetOutermost()->IsDirty())
    {
        PreMutationAuditError = TEXT(
            "Refusing to modify a Blueprint package with unsaved changes; save it before the persisted dependency audit");
    }
    if (!PreMutationAuditError.IsEmpty() ||
        !ValidateMCPDependencyIndependence(
            TEXT("networking target Blueprint before mutation"),
            Target.Blueprint,
            EAuditMode::RequirePersisted,
            PreMutationAuditError,R,TEXT("pre_mutation_dependency_audit")))
    {
        TSharedPtr<FJsonObject> AuditError = CreateNetworkingError(
            TEXT("networking_mcp_dependency_audit_failed"),
            PreMutationAuditError);
        AddTarget(Target, AuditError);
        AuditError->SetStringField(TEXT("dependency_audit_mode"), TEXT("require_persisted"));
        AuditError->SetStringField(TEXT("dependency_audit_stage"), TEXT("pre_mutation"));
        AuditError->SetBoolField(TEXT("dependency_audit_passed"), false);
        return AuditError;
    }
    Result->SetStringField(TEXT("dependency_audit_mode"), TEXT("require_persisted"));
    Result->SetBoolField(TEXT("pre_mutation_dependency_audit_passed"), true);

    if(Target.ActorCDO->IsActorInitialized()||Target.ActorCDO->GetWorld()!=nullptr||Target.ActorCDO->GetLocalRole()!=ROLE_Authority)
    {
        return CreateNetworkingError(TEXT("actor_cdo_world_state_unsupported"),TEXT("Apply requires an uninitialized authority CDO without a borrowed world"));
    }
    R.Stage=TEXT("mutate");R.bCompileRequested=R.bSaveRequested=true;Count(R,TEXT("outer_transaction_calls"));
    FScopedTransaction Transaction(NSLOCTEXT(
        "UnrealMCP",
        "MCPSetBlueprintNetworkingSettings",
        "MCP Set Blueprint Networking Settings"));
    R.bMutationAttempted=true;Result->SetBoolField(TEXT("mutation_attempted"), true);
    Count(R,TEXT("blueprint_modify_calls"));Target.Blueprint->Modify();
    Count(R,TEXT("cdo_modify_calls"));Target.ActorCDO->Modify();
    ApplySettings(Target.ActorCDO, Requested, Changed,R);
    Count(R,TEXT("mark_modified_calls"));FBlueprintEditorUtils::MarkBlueprintAsModified(Target.Blueprint);
    Count(R,TEXT("mark_dirty_calls"));Target.Blueprint->MarkPackageDirty();

    FCompilerResultsLog CompileLog;
    CompileLog.SetSourcePath(Target.Blueprint->GetPathName());
    R.Stage=TEXT("compile_and_readback");R.bCompileAttempted=true;Count(R,TEXT("full_compile_calls"));
    FKismetEditorUtilities::CompileBlueprint(Target.Blueprint, EBlueprintCompileOptions::None, &CompileLog);
    Count(R,TEXT("full_compile_returns"));
    const bool bCompiled =
        (Target.Blueprint->Status == BS_UpToDate || Target.Blueprint->Status == BS_UpToDateWithWarnings) &&
        CompileLog.NumErrors == 0;
    R.Compiled=bCompiled;Result->SetBoolField(TEXT("compiled"), bCompiled);
    Result->SetStringField(TEXT("compile_status"), BlueprintStatusToString(Target.Blueprint->Status));
    Result->SetNumberField(TEXT("compile_error_count"), CompileLog.NumErrors);
    Result->SetNumberField(TEXT("compile_warning_count"), CompileLog.NumWarnings);

    TStrongObjectPtr<UClass> CompiledClassHold;TStrongObjectPtr<AActor> CompiledHold;
    AActor* CompiledCDO=AcquireCDO(Target.Blueprint,R,CompiledClassHold,CompiledHold,TEXT("post_compile_cdo_identity"));
    const FActorNetworkingSettings Compiled = ObserveSettings(CompiledCDO,R,TEXT("post_compile_settings"));
    if(!CompiledCDO||!FiniteSettings(Compiled)) { R.bChangeStateKnown=false; }
    const TArray<FString> CompileMismatches = CompiledCDO&&FiniteSettings(Compiled)
        ? GetMismatchedFields(Compiled, Requested, Provided)
        : Provided;
    Result->SetArrayField(TEXT("readback_mismatches"), StringsToJson(CompileMismatches));
    if (!bCompiled || !CompileMismatches.IsEmpty())
    {
        SetMutationFailure(
            Result,
            bCompiled ? TEXT("networking_post_compile_readback_failed") : TEXT("blueprint_compile_failed"),
            bCompiled ? TEXT("Compiled CDO networking settings do not match the request")
                      : TEXT("Blueprint networking settings changed in memory, but compilation failed"),
            TEXT("compile_and_readback"),
            false);
        Result->SetBoolField(TEXT("saved"), false);
        AddContract(
            Result, false, true, true, Before, Compiled, Provided, Changed, false,
            TEXT("post_compile_actor_cdo_readback"),
            {TEXT("blueprint_compile"), TEXT("requested_fields_match")});
        return Result;
    }

    FString PreSaveAuditError;
    if (!ValidateMCPDependencyIndependence(
            TEXT("networking target Blueprint before save"),
            Target.Blueprint,
            EAuditMode::InMemoryOnly,
            PreSaveAuditError,R,TEXT("pre_save_dependency_audit")))
    {
        SetMutationFailure(
            Result,
            TEXT("networking_pre_save_mcp_dependency_audit_failed"),
            PreSaveAuditError,
            TEXT("pre_save_dependency_audit"),
            false);
        Result->SetBoolField(TEXT("saved"), false);
        Result->SetStringField(TEXT("dependency_audit_mode"), TEXT("in_memory_only"));
        Result->SetBoolField(TEXT("pre_save_dependency_audit_passed"), false);
        AddContract(
            Result, false, true, true, Before, Compiled, Provided, Changed, false,
            TEXT("pre_save_dependency_audit"),
            {TEXT("blueprint_compile"), TEXT("requested_fields_match"), TEXT("mcp_dependency_audit")});
        return Result;
    }
    Result->SetBoolField(TEXT("pre_save_dependency_audit_passed"), true);

    R.Stage=TEXT("save_and_readback");R.bSaveAttempted=true;Count(R,TEXT("forced_save_calls"));
    const bool bSaved = UEditorAssetLibrary::SaveLoadedAsset(Target.Blueprint, false);
    Count(R,TEXT("forced_save_returns"));R.bSaved=bSaved;
    Result->SetBoolField(TEXT("saved"), bSaved);
    TStrongObjectPtr<UClass> SavedClassHold;TStrongObjectPtr<AActor> SavedHold;
    AActor* SavedCDO=AcquireCDO(Target.Blueprint,R,SavedClassHold,SavedHold,TEXT("post_save_cdo_identity"));
    const FActorNetworkingSettings Saved = ObserveSettings(SavedCDO,R,TEXT("post_save_settings"));
    if(!SavedCDO||!FiniteSettings(Saved)) { R.bChangeStateKnown=false; }
    const TArray<FString> SaveMismatches = SavedCDO&&FiniteSettings(Saved)
        ? GetMismatchedFields(Saved, Requested, Provided)
        : Provided;
    Result->SetArrayField(TEXT("readback_mismatches"), StringsToJson(SaveMismatches));
    FString PostSaveAuditError;
    bool bPostSaveDependencyAuditPassed = false;
    if (bSaved)
    {
        if (Target.Blueprint->GetOutermost()->IsDirty())
        {
            PostSaveAuditError = TEXT("Blueprint package remained dirty after save, so persisted dependency independence could not be proven");
        }
        else
        {
            bPostSaveDependencyAuditPassed = ValidateMCPDependencyIndependence(
                TEXT("networking target Blueprint after save"),
                Target.Blueprint,
                EAuditMode::RequirePersisted,
                PostSaveAuditError,R,TEXT("post_save_dependency_audit"));
        }
    }
    Result->SetBoolField(TEXT("post_save_dependency_audit_performed"), bSaved);
    Result->SetBoolField(TEXT("post_save_dependency_audit_passed"), bPostSaveDependencyAuditPassed);
    const bool bVerified = bSaved && SaveMismatches.IsEmpty() && bPostSaveDependencyAuditPassed;
    if (!bVerified)
    {
        if (!bSaved)
        {
            SetMutationFailure(
                Result,
                TEXT("asset_save_failed"),
                TEXT("Blueprint networking settings compiled, but the asset could not be saved"),
                TEXT("save_and_readback"),
                true);
        }
        else if (!bPostSaveDependencyAuditPassed)
        {
            SetMutationFailure(
                Result,
                TEXT("networking_post_save_mcp_dependency_audit_failed"),
                PostSaveAuditError,
                TEXT("post_save_dependency_audit"),
                false);
            Result->SetBoolField(TEXT("persisted_contamination_possible"), true);
            Result->SetStringField(
                TEXT("recovery_guidance"),
                TEXT("The Blueprint was saved but persisted dependency independence could not be proven. Restore this package from source control before disabling UnrealMCP or packaging."));
        }
        else
        {
            SetMutationFailure(
                Result,
                TEXT("networking_post_save_readback_failed"),
                TEXT("Saved CDO networking settings do not match the request"),
                TEXT("save_and_readback"),
                false);
        }
    }
    else
    {
        Result->SetStringField(TEXT("message"), TEXT("Networking settings updated, compiled, verified, and saved"));
    }

    AddContract(
        Result, false, true, true, Before, Saved, Provided, Changed, bVerified,
        TEXT("post_save_actor_cdo_readback"),
        {TEXT("blueprint_compile"), TEXT("asset_save"), TEXT("requested_fields_match"), TEXT("mcp_dependency_audit")});
    return Result;
}

void Finish(FResult& R,const TSharedPtr<FJsonObject>& Legacy)
{
    R.Data->SetObjectField(TEXT("legacy_result"),Legacy);bool Success=false;Legacy->TryGetBoolField(TEXT("success"),Success);
    R.bSuccess=Success;R.bPostconditionPassed=Success;
    bool Unknown=false;Legacy->TryGetBoolField(TEXT("mutation_state_unknown"),Unknown);if(Unknown) { R.bChangeStateKnown=false; }
    if(!Success) { Legacy->TryGetStringField(TEXT("error_code"),R.ErrorCode);Legacy->TryGetStringField(TEXT("error"),R.Error);FString Failed; if(Legacy->TryGetStringField(TEXT("failed_stage"),Failed)) { R.Stage=Failed; } }
    else { R.Stage=TEXT("complete"); }
    R.Data->SetBoolField(TEXT("do_not_retry"),R.bMutationAttempted);
    R.Data->SetBoolField(TEXT("same_process_cdo_readback_only"),true);
    R.Data->SetBoolField(TEXT("compiler_transitive_callbacks_counted"),false);
}
bool DecodeCarriers(const TArray<FString>& Values,TSharedRef<FJsonObject> Params,FResult& R)
{
    const TCHAR* Keys[]={TEXT("replicates"),TEXT("replicate_movement"),TEXT("net_dormancy"),TEXT("always_relevant"),TEXT("only_relevant_to_owner"),TEXT("net_update_frequency"),TEXT("min_net_update_frequency"),TEXT("net_priority"),TEXT("net_cull_distance_squared")};
    for(int32 I=0;I<9;++I)
    {
        if(Values[I].IsEmpty()) { continue; }
        if(!SafeText(Values[I],4096)) { return Fail(R,TEXT("invalid_nullable_json"),TEXT("Nullable carrier exceeds bounded text/no-NUL admission")); }
        Count(R,TEXT("nullable_parse_calls"));Function::FResult Parse;TSharedPtr<FJsonValue> Value;
        const bool Parsed=Function::Pin::ParseJson(Values[I],Value,Parse);Count(R,TEXT("nullable_parse_returns"));
        if(!Parsed||!Value.IsValid()) { return Fail(R,TEXT("invalid_nullable_json"),Parse.Error); }
        // Root null is a supplied JSON null, never the public omitted-field sentinel.
        Params->SetField(Keys[I],Value);
    }
    return true;
}
}
Variable::FResult Inspect(const FString& BlueprintName)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("operation"),TEXT("inspect_blueprint_networking_settings"));
    if(!IsEditorGameThread()) { Fail(R,TEXT("editor_game_thread_required"),TEXT("Editor game thread required"));return R; }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("blueprint_name"),BlueprintName);Finish(R,RunInspect(Params,R));return R;
}
Variable::FResult Set(const FString& BlueprintName,const FString& ReplicatesJson,const FString& ReplicateMovementJson,const FString& NetDormancyJson,const FString& AlwaysRelevantJson,const FString& OnlyRelevantToOwnerJson,const FString& NetUpdateFrequencyJson,const FString& MinNetUpdateFrequencyJson,const FString& NetPriorityJson,const FString& NetCullDistanceSquaredJson,bool bDryRun)
{
    FResult R;Init(R);R.Data->SetStringField(TEXT("operation"),TEXT("set_blueprint_networking_settings"));
    if(!IsEditorGameThread()) { Fail(R,TEXT("editor_game_thread_required"),TEXT("Editor game thread required"));return R; }
    auto Params=MakeShared<FJsonObject>();Params->SetStringField(TEXT("blueprint_name"),BlueprintName);Params->SetBoolField(TEXT("dry_run"),bDryRun);
    R.Data->SetArrayField(TEXT("private_nullable_carriers"),StringsToJson({ReplicatesJson,ReplicateMovementJson,NetDormancyJson,AlwaysRelevantJson,OnlyRelevantToOwnerJson,NetUpdateFrequencyJson,MinNetUpdateFrequencyJson,NetPriorityJson,NetCullDistanceSquaredJson}));
    // Original play/target/name/CDO admission precedes optional-field validation.
    // Decode is done inside RunSet after target resolution; empty means omitted, supplied null remains invalid.
    R.Data->SetBoolField(TEXT("dry_run"),bDryRun);
    const TArray<FString> Carriers={ReplicatesJson,ReplicateMovementJson,NetDormancyJson,AlwaysRelevantJson,OnlyRelevantToOwnerJson,NetUpdateFrequencyJson,MinNetUpdateFrequencyJson,NetPriorityJson,NetCullDistanceSquaredJson};
    Finish(R,RunSet(Params,R,Carriers));return R;
}
}
