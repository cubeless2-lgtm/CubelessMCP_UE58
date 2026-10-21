// GC: original GAS6 authoring and bounded dependency audit through installed public UE5.8 Editor APIs.
// No external server, custom runtime class, private/protected bypass or implicit retry.
#include "Compatibility/UE58/CubelessGASAuthoringApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Components/ActorComponent.h"
#include "EdGraphSchema_K2.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Factories/BlueprintFactory.h"
#include "Factories/Factory.h"
#include "GameFramework/Actor.h"
#include "Interfaces/IPluginManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Misc/PackageName.h"
#include "ScopedTransaction.h"
#include "UObject/NameTypes.h"
#include "UObject/Package.h"
#include "UObject/UnrealType.h"
#include "UObject/UObjectGlobals.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "Dom/JsonObject.h"
#include "HAL/FileManager.h"
#include "Misc/Crc.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "CoreGlobals.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "Serialization/JsonReader.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectHash.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "Engine/World.h"

namespace Cubeless::UE58::GASAuthoring
{
namespace
{

using Cubeless::UE58::Variable::FResult;
bool Running=false;
FString Json(const TSharedPtr<FJsonObject>& O){FString S;if(O){FJsonSerializer::Serialize(O.ToSharedRef(),TJsonWriterFactory<>::Create(&S));}return S;}
bool Live(const UObject* O){return IsValid(O)&&!O->HasAnyFlags(RF_NeedLoad|RF_NeedPostLoad|RF_NeedPostLoadSubobjects|RF_BeginDestroyed|RF_FinishDestroyed);}
struct FWork
{
    FResult R;bool Cleaning=false,WorldCaptured=false,Writer=false,NewPackageOwned=false,OriginalFileExisted=false,CleanupReleased=false;FString FailedStage,TargetPath,PackagePath;
    TWeakObjectPtr<UWorld> World;TStrongObjectPtr<UBlueprint> Target{nullptr};TWeakObjectPtr<UBlueprint> TargetWeak;TStrongObjectPtr<UPackage> Package{nullptr};TStrongObjectPtr<UFactory> Factory{nullptr};
    TWeakObjectPtr<USimpleConstructionScript> CapturedSCS;bool SCSPinned=false;FString SavedFilename;FMD5Hash SavedFileHash;
    TSharedRef<FJsonObject> Counts=MakeShared<FJsonObject>(),Mutation=MakeShared<FJsonObject>(),SavePrefix=MakeShared<FJsonObject>(),Cleanup=MakeShared<FJsonObject>();
    FWork(bool InWriter):Writer(InWriter){R.Compiled=false;R.Data->SetObjectField(TEXT("native_call_counts"),Counts);R.Data->SetObjectField(TEXT("actual_mutation_prefix"),Mutation);R.Data->SetObjectField(TEXT("actual_save_prefix"),SavePrefix);R.Data->SetObjectField(TEXT("actual_cleanup_prefix"),Cleanup);R.Data->SetBoolField(TEXT("automatic_retry_permitted"),false);R.Data->SetBoolField(TEXT("atomic_rollback_available"),false);R.Data->SetBoolField(TEXT("wrapper_forced_async_wait"),false);R.Data->SetStringField(TEXT("synchronous_sdk_scope"),TEXT("Public Factory/Blueprint compile/save and bounded dependency audit are synchronous Editor SDK calls; no fabricated async completion."));}
    bool Fail(const TCHAR* Code,const FString& Message){if(R.Error.IsEmpty()){FailedStage=R.Stage;R.ErrorCode=Code;R.Error=Message;}R.bSuccess=false;return false;}
    bool Admit(const TCHAR* Stage,bool CheckTarget=true)
    {
        R.Stage=Stage;
        if(!IsInGameThread()||IsEngineExitRequested()||GExitPurge||!GEditor||IsGarbageCollecting()||GIsSavingPackage){return Fail(TEXT("gas_editor_context_unavailable"),TEXT("Editor game thread, no GC or package save required; no automatic retry."));}
        if(Writer&&(GEditor->GetPIEWorldContext()||GEditor->PlayWorld)){return Fail(TEXT("gas_play_session_active"),TEXT("GAS structural authoring is unavailable during PIE/SIE."));}
        if(Cleaning&&GEditor->IsTransactionActive()){return Fail(TEXT("gas_cleanup_transaction_active"),TEXT("Cleanup stopped because an Editor transaction became active; preserve residual output."));}
        UWorld* Current=GEditor->GetEditorWorldContext().World();
        if(!Live(Current)||Current->bIsTearingDown){return Fail(TEXT("gas_world_identity_changed"),TEXT("Editor world is unavailable or tearing down; current UObject observation skipped."));}
        if(!WorldCaptured){if(!Live(Current)){return Fail(TEXT("gas_editor_world_missing"),TEXT("A live Editor world is required."));}World=Current;WorldCaptured=true;}
        else if(!World.IsValid()||Current!=World.Get()){return Fail(TEXT("gas_world_identity_changed"),TEXT("Editor world changed across a public SDK dispatch; stop without replay."));}
        if(CheckTarget&&!CleanupReleased&&!TargetPath.IsEmpty())
        {
            UBlueprint* B=TargetWeak.Get();if(!Live(B)||!Live(Package.Get())||B->GetOutermost()!=Package.Get()||!B->GetPathName().Equals(TargetPath,ESearchCase::CaseSensitive)||!Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)){return Fail(TEXT("gas_target_identity_changed"),TEXT("Captured Blueprint/package identity changed; do not mutate/save/cleanup a replacement."));}
        }
        if(CheckTarget&&SCSPinned&&(!CapturedSCS.IsValid()||!Live(CapturedSCS.Get())||!TargetWeak.IsValid()||TargetWeak->SimpleConstructionScript!=CapturedSCS.Get())){return Fail(TEXT("gas_scs_identity_changed"),TEXT("Captured SimpleConstructionScript identity changed; stop before further writes."));}
        return true;
    }
    bool SDK(const TCHAR* Stage,bool MutationAttempt=false){if(!Admit(Stage)){return false;}R.bChangeStateKnown=false;R.bMutationAttempted|=MutationAttempt;double N=0;Counts->TryGetNumberField(Stage,N);Counts->SetNumberField(Stage,N+1);if(MutationAttempt){Mutation->SetBoolField(Stage,true);}return true;}
    TSharedRef<FJsonObject> Projection(UBlueprint* B)
    {
        auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("valid"),Live(B));if(!Live(B)){return J;}J->SetStringField(TEXT("path"),B->GetPathName());J->SetStringField(TEXT("parent"),GetPathNameSafe(B->ParentClass));J->SetStringField(TEXT("generated_class"),GetPathNameSafe(B->GeneratedClass));J->SetNumberField(TEXT("status"),static_cast<int32>(B->Status));J->SetBoolField(TEXT("dirty"),B->GetOutermost()->IsDirty());TArray<TSharedPtr<FJsonValue>> V,N;
        for(const auto& D:B->NewVariables){auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("name"),D.VarName.ToString());X->SetStringField(TEXT("category"),D.Category.ToString());X->SetStringField(TEXT("pin_category"),D.VarType.PinCategory.ToString());X->SetStringField(TEXT("pin_object"),GetPathNameSafe(D.VarType.PinSubCategoryObject.Get()));X->SetStringField(TEXT("flags_decimal"),LexToString(D.PropertyFlags));V.Add(MakeShared<FJsonValueObject>(X));}
        if(B->SimpleConstructionScript){for(const auto* D:B->SimpleConstructionScript->GetAllNodes()){if(D){auto X=MakeShared<FJsonObject>();X->SetStringField(TEXT("name"),D->GetVariableName().ToString());X->SetStringField(TEXT("class"),GetPathNameSafe(D->ComponentClass));N.Add(MakeShared<FJsonValueObject>(X));}}}J->SetArrayField(TEXT("new_variables"),V);J->SetArrayField(TEXT("scs_nodes"),N);return J;
    }
    bool Pin(UBlueprint* B)
    {
        if(!Live(B)||!Live(B->GetOutermost())){return Fail(TEXT("gas_blueprint_identity_invalid"),TEXT("Blueprint and owning package must be live after load/create."));}
        Target.Reset(B);TargetWeak=B;Package.Reset(B->GetOutermost());TargetPath=B->GetPathName();PackagePath=Package->GetName();R.Data->SetObjectField(TEXT("target_before"),Projection(B));return true;
    }
    bool Compile(UBlueprint* B)
    {
        R.bCompileRequested=true;if(!SDK(TEXT("compile_blueprint"),true)){return false;}R.bCompileAttempted=true;R.Compiled.Reset();FKismetEditorUtilities::CompileBlueprint(B);R.Data->SetBoolField(TEXT("explicit_compile_returned"),true);
        if(!Admit(TEXT("after_compile_blueprint"))){R.Data->SetBoolField(TEXT("compile_readback_skipped_unsafe"),true);return false;}
        R.Compiled=Live(B)&&B->Status!=BS_Error&&B->GeneratedClass!=nullptr;return true;
    }
    bool Save(UBlueprint* B,bool Requested)
    {
        R.bSaveRequested=Requested;if(!Requested){return true;}if(!Admit(TEXT("before_exact_save"))||B!=TargetWeak.Get()){return Fail(TEXT("gas_save_identity_mismatch"),TEXT("Only the captured Blueprint/package may be saved."));}
        SavePrefix->SetStringField(TEXT("target_path"),TargetPath);SavePrefix->SetStringField(TEXT("package_path"),PackagePath);SavePrefix->SetBoolField(TEXT("dirty_before"),Package->IsDirty());
        if(!SDK(TEXT("save_loaded_asset"),true)){return false;}R.bSaveAttempted=true;const bool Returned=UEditorAssetLibrary::SaveLoadedAsset(B,false);
        // Actual SDK return is authoritative even if later identity/readback/audit/cleanup fails.
        R.bSaved|=Returned;SavePrefix->SetBoolField(TEXT("SDK_save_returned"),true);SavePrefix->SetBoolField(TEXT("SDK_save_success"),Returned);SavePrefix->SetBoolField(TEXT("actual_saved_history"),R.bSaved);
        if(!Admit(TEXT("after_exact_save"))){SavePrefix->SetField(TEXT("dirty_after"),MakeShared<FJsonValueNull>());SavePrefix->SetBoolField(TEXT("readback_skipped_unsafe"),true);return false;}
        SavePrefix->SetBoolField(TEXT("dirty_after"),Package->IsDirty());
        FString Relative;if(Returned&&FPackageName::TryConvertLongPackageNameToFilename(PackagePath,Relative,FPackageName::GetAssetPackageExtension())){SavedFilename=FPaths::ConvertRelativePathToFull(Relative);FPaths::NormalizeFilename(SavedFilename);const int64 Bytes=IFileManager::Get().FileSize(*SavedFilename);if(Bytes>=0&&Bytes<=64*1024*1024){if(!SDK(TEXT("hash_exact_saved_package"))){return false;}SavedFileHash=FMD5Hash::HashFile(*SavedFilename);}SavePrefix->SetBoolField(TEXT("exact_saved_file_hash_available"),SavedFileHash.IsValid());if(SavedFileHash.IsValid()){SavePrefix->SetStringField(TEXT("exact_saved_file_md5"),LexToString(SavedFileHash));}}
        if(!Admit(TEXT("after_hash_exact_saved_package"))){SavePrefix->SetBoolField(TEXT("final_dirty_readback_skipped_unsafe"),true);return false;}return Returned&&!Package->IsDirty();
    }
    void FinalProjection(){if(!Admit(TEXT("final_projection_admission"),false)){R.Data->SetBoolField(TEXT("current_UObject_observation_skipped_unsafe"),true);return;}if(!TargetPath.IsEmpty()){auto After=Projection(TargetWeak.Get());R.Data->SetObjectField(TEXT("target_after"),After);const TSharedPtr<FJsonObject>* Before=nullptr;R.Data->TryGetObjectField(TEXT("target_before"),Before);R.Data->SetBoolField(TEXT("observed_target_projection_changed"),Before&&(*Before).IsValid()&&!Json(*Before).Equals(Json(After),ESearchCase::CaseSensitive));}R.Data->SetStringField(TEXT("projection_scope"),TEXT("Blueprint identity/parent/generated class/status/dirty, local variable type/category/flags, SCS names/classes. Not whole asset/global SDK/cache purity."));}
};
thread_local FWork* CurrentWork=nullptr;
FWork& Work(){check(CurrentWork);return *CurrentWork;}
struct FScope{FWork* Prior;explicit FScope(FWork& W):Prior(CurrentWork){CurrentWork=&W;}~FScope(){CurrentWork=Prior;}};
enum class EDependencyAuditMode:uint8{InMemoryOnly,RequirePersisted};
struct FGasCompatibility
{
    static bool IsMCPDependencyReference(const FString& Value);
    static FString GetMCPDependencyReferenceError(const FString& FieldName,const FString& Value);
    static void CollectMCPDependencyAuditIssues(const FString& FieldName,UObject* Object,EDependencyAuditMode Mode,TArray<FString>& Issues);
    static bool ValidateMCPDependencyIndependence(const FString& FieldName,UObject* Object,EDependencyAuditMode Mode,FString& Error);
    static bool TryCleanupNewAssetAfterFailedDependencyAudit(UObject* Object,FString& Error);
    static TSharedPtr<FJsonObject> CreateErrorResponse(const FString& Message){auto J=MakeShared<FJsonObject>();J->SetBoolField(TEXT("success"),false);J->SetStringField(TEXT("error"),Message);return J;}
};

struct FExactKeys:DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A,const FString& B) { return A.Equals(B,ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S,TSharedPtr<FJsonObject>& Out)
{
    if(S.IsEmpty()||S.Len()>1048576) { return false; }for(TCHAR C:S) { if(C==0) { return false; } }auto Reader=TJsonReaderFactory<>::Create(S);TArray<bool> Objects;TArray<TSet<FString,FExactKeys>> Keys;EJsonNotation Token;int32 Slots=0;
    while(Reader->ReadNext(Token)) { if(Token==EJsonNotation::Error||++Slots>262144) { return false; }if(Token!=EJsonNotation::ObjectEnd&&Token!=EJsonNotation::ArrayEnd&&!Objects.IsEmpty()&&Objects.Last()) { const FString K(Reader->GetIdentifier());if(Keys.Last().Contains(K)) { return false; }Keys.Last().Add(K); }if(Token==EJsonNotation::ObjectStart||Token==EJsonNotation::ArrayStart) { Objects.Add(Token==EJsonNotation::ObjectStart);Keys.AddDefaulted();if(Objects.Num()>64) { return false; } }else if(Token==EJsonNotation::ObjectEnd||Token==EJsonNotation::ArrayEnd) { if(Objects.IsEmpty()) { return false; }Objects.Pop();Keys.Pop(); }if(Token==EJsonNotation::Number&&!FMath::IsFinite(Reader->GetValueAsNumber())) { return false; } }
    return Objects.IsEmpty()&&FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S),Out)&&Out.IsValid();
}

bool FGasCompatibility::IsMCPDependencyReference(const FString& Value)
{
    FString Candidate = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    Candidate.TrimQuotesInline();
    if (Candidate.IsEmpty())
    {
        return false;
    }

    return Candidate.Contains(TEXT("CubelessMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("cubeless_mcp"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("ModelContextProtocol"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("ToolsetRegistry"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Engine/PythonTypes"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Script/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("UnrealMCP"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("MCPUnreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("mcp_unreal"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("/Script/CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigTools"), ESearchCase::IgnoreCase) ||
        Candidate.Contains(TEXT("CubelessControlRigToolsEditor"), ESearchCase::IgnoreCase);
}

FString FGasCompatibility::GetMCPDependencyReferenceError(const FString& FieldName, const FString& Value)
{
    const FString FieldLabel = FieldName.IsEmpty() ? TEXT("value") : FieldName;
    return FString::Printf(
        TEXT("Refusing to persist MCP-only reference in '%s': %s. UnrealMCP is an editor authoring bridge; finished assets must not depend on it."),
        *FieldLabel,
        *Value);
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
    if (FGasCompatibility::IsMCPDependencyReference(ObjectPath))
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
        if (FGasCompatibility::IsMCPDependencyReference(ClassPath))
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
            if (FGasCompatibility::IsMCPDependencyReference(SuperClassPath))
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
            if (FGasCompatibility::IsMCPDependencyReference(SoftReferencePath))
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
            if (FGasCompatibility::IsMCPDependencyReference(TopLevelReferencePath))
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
    if (FGasCompatibility::IsMCPDependencyReference(PackageName))
    {
        OutIssues.AddUnique(FString::Printf(TEXT("Package path contains MCP-only token: %s"), *PackageName));
    }

    TArray<UObject*> PackageExports;
    GetObjectsWithPackage(Package, PackageExports, true, RF_Transient, EInternalObjectFlags::Garbage);

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
            if (FGasCompatibility::IsMCPDependencyReference(DependencyPath))
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

void FGasCompatibility::CollectMCPDependencyAuditIssues(
    const FString& FieldName,
    UObject* ReferencedObject,
    EDependencyAuditMode AuditMode,
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
        TArray<UObject*> ReferencedObjects{ReferencedObject};
        CollectMCPSerializedReferenceAuditTokens(
            RootPackageName,
            ReferencedObjects,
            ReferencedObjects.Num(),
            ExternalAuthoredPackages,
            OutIssues);
    }
    TSet<FName> PersistedAuditedPackages;
    if (AuditMode == EDependencyAuditMode::RequirePersisted)
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

bool FGasCompatibility::ValidateMCPDependencyIndependence(
    const FString& FieldName,
    UObject* ReferencedObject,
    EDependencyAuditMode AuditMode,
    FString& OutError)
{
    if (!ReferencedObject)
    {
        OutError = FString::Printf(
            TEXT("Refusing '%s' because the referenced object is null"),
            *FieldName);
        return false;
    }

    if(!Work().SDK(TEXT("dependency_audit"))){OutError=Work().R.Error;return false;}
    TArray<FString> Issues;
    CollectMCPDependencyAuditIssues(FieldName, ReferencedObject, AuditMode, Issues);
    if(!Work().Admit(TEXT("after_dependency_audit"))){OutError=Work().R.Error;return false;}
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

bool FGasCompatibility::TryCleanupNewAssetAfterFailedDependencyAudit(UObject* NewAsset,FString& OutError)
{
    auto& W=Work();W.Cleanup->SetBoolField(TEXT("attempted"),true);W.Cleanup->SetBoolField(TEXT("complete"),false);
    auto Refuse=[&](const FString& Why){OutError=Why;W.Cleanup->SetStringField(TEXT("error"),Why);W.Cleanup->SetBoolField(TEXT("residual"),true);return false;};
    if(!W.Admit(TEXT("before_owned_cleanup"))){return Refuse(TEXT("cleanup_refused: unsafe Editor/target context"));}
    if(GEditor->IsTransactionActive()||!W.NewPackageOwned||W.OriginalFileExisted||NewAsset!=W.TargetWeak.Get()||!Live(NewAsset)||NewAsset->HasAnyFlags(RF_WasLoaded)||NewAsset->IsRooted()){return Refuse(TEXT("cleanup_refused: exact caller-created/unloaded/unrooted target and no active Undo transaction required"));}
    W.Cleaning=true;W.Cleanup->SetStringField(TEXT("asset_path"),W.TargetPath);W.Cleanup->SetStringField(TEXT("package_path"),W.PackagePath);
    TWeakObjectPtr<UObject> Weak=NewAsset;TWeakObjectPtr<UClass> Class=NewAsset->GetClass();TWeakObjectPtr<UPackage> Package=NewAsset->GetOutermost();const FString Path=W.TargetPath,PackagePath=W.PackagePath;
    if(Package.Get()!=W.Package.Get()||NewAsset->GetOuter()!=Package.Get()||!PackagePath.StartsWith(TEXT("/Game/"))||!Package->GetExternalPackages().IsEmpty()){return Refuse(TEXT("cleanup_refused: one exact new top-level /Game package without external packages required"));}
    UBlueprint* Blueprint=Cast<UBlueprint>(NewAsset);if(!Blueprint){return Refuse(TEXT("cleanup_refused: exact Blueprint output required"));}
    TArray<TWeakObjectPtr<UBlueprintGeneratedClass>> OwnedClasses;TArray<TWeakObjectPtr<UClass>> OwnedClassTypes;TArray<FString> OwnedNames;
    const FString AssetName=FPackageName::GetLongPackageAssetName(PackagePath);
    auto CaptureOwnedClass=[&](UClass* InClass,const FString& ExpectedName){if(!InClass){return true;}auto* C=Cast<UBlueprintGeneratedClass>(InClass);if(!Live(C)||C->IsRooted()||C->GetOuter()!=Package.Get()||C->GetOutermost()!=Package.Get()||C->ClassGeneratedBy!=Blueprint||!C->GetName().Equals(ExpectedName,ESearchCase::CaseSensitive)){return false;}OwnedClasses.Add(C);OwnedClassTypes.Add(C->GetClass());OwnedNames.Add(ExpectedName);return true;};
    if(!CaptureOwnedClass(Blueprint->GeneratedClass,AssetName+TEXT("_C"))||!CaptureOwnedClass(Blueprint->SkeletonGeneratedClass,TEXT("SKEL_")+AssetName+TEXT("_C"))){return Refuse(TEXT("cleanup_refused: generated/skeleton class ownership or expected identity differs"));}
    const TWeakObjectPtr<UClass> OriginalGenerated=Blueprint->GeneratedClass,OriginalSkeleton=Blueprint->SkeletonGeneratedClass;
    auto OwnedClassExact=[&](){if(!Live(Blueprint)||Blueprint->GeneratedClass!=OriginalGenerated.Get()||Blueprint->SkeletonGeneratedClass!=OriginalSkeleton.Get()){return false;}for(int32 I=0;I<OwnedClasses.Num();++I){auto* C=OwnedClasses[I].Get();if(!Live(C)||C->IsRooted()||!OwnedClassTypes[I].IsValid()||C->GetClass()!=OwnedClassTypes[I].Get()||C->ClassGeneratedBy!=Blueprint||C->GetOuter()!=Package.Get()||C->GetOutermost()!=Package.Get()||!C->GetName().Equals(OwnedNames[I],ESearchCase::CaseSensitive)){return false;}}return true;};
    auto NoForeignAssets=[&](){TArray<UObject*> Exports;GetObjectsWithPackage(Package.Get(),Exports,true,RF_Transient,EInternalObjectFlags::Garbage);for(const auto* O:Exports){if(O&&O!=NewAsset&&O->IsAsset()){bool ExactOwned=false;for(const auto& C:OwnedClasses){ExactOwned|=O==C.Get();}if(!ExactOwned){return false;}}}return true;};
    if(!OwnedClassExact()||!NoForeignAssets()){return Refuse(TEXT("cleanup_refused: package contains a foreign asset or generated class changed"));}
    W.Cleanup->SetNumberField(TEXT("exact_owned_generated_class_count"),OwnedClasses.Num());

    FString Filename;if(!FPackageName::TryConvertLongPackageNameToFilename(PackagePath,Filename,FPackageName::GetAssetPackageExtension())){return Refuse(TEXT("cleanup_refused: exact package filename unavailable"));}Filename=FPaths::ConvertRelativePathToFull(Filename);FPaths::NormalizeFilename(Filename);
    FString Content=FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());FPaths::NormalizeDirectoryName(Content);if(!FPaths::IsUnderDirectory(Filename,Content)){return Refuse(TEXT("cleanup_refused: file outside ProjectContent"));}
    const bool FileExisted=FPaths::FileExists(Filename);W.Cleanup->SetBoolField(TEXT("file_present_before"),FileExisted);
    if(FileExisted&&(!W.SavedFileHash.IsValid()||!Filename.Equals(W.SavedFilename,ESearchCase::CaseSensitive))){return Refuse(TEXT("cleanup_refused: current file lacks this request's exact successful-save hash"));}
    auto FileSame=[&](){if(FPaths::FileExists(Filename)!=FileExisted){return false;}if(!FileExisted){return true;}return !IFileManager::Get().IsReadOnly(*Filename)&&IFileManager::Get().FileSize(*Filename)<=64*1024*1024&&FMD5Hash::HashFile(*Filename)==W.SavedFileHash;};
    auto Exact=[&](){return W.Admit(TEXT("cleanup_exact_identity"),false)&&Weak.IsValid()&&Class.IsValid()&&Package.IsValid()&&Live(Weak.Get())&&Live(Package.Get())&&Weak->GetClass()==Class.Get()&&Weak->GetOuter()==Package.Get()&&Weak->GetOutermost()==W.Package.Get()&&Weak->GetPathName().Equals(Path,ESearchCase::CaseSensitive)&&Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)&&OwnedClassExact()&&NoForeignAssets()&&FileSame();};
    // Release only our own temporary strong Blueprint hold. Never reset Undo or external references.
    W.Target.Reset();W.CleanupReleased=true;
    auto NoReferences=[&](){if(!Exact()||!W.SDK(TEXT("cleanup_reference_admission"))){return false;}bool Referenced=false,Undo=false;ObjectTools::GatherObjectReferencersForDeletion(Weak.Get(),Referenced,Undo);W.Cleanup->SetBoolField(TEXT("live_referenced"),Referenced);W.Cleanup->SetBoolField(TEXT("Undo_referenced"),Undo);return Exact()&&!Referenced&&!Undo;};
    if(!NoReferences()){return Refuse(TEXT("cleanup_refused: live/Undo references, file or target/context changed"));}
    FCanDeleteAssetResult CanDelete;if(!W.SDK(TEXT("cleanup_delete_veto"))){return Refuse(W.R.Error);}FEditorDelegates::OnAssetsCanDelete.Broadcast({Weak.Get()},CanDelete);
    if(!CanDelete.Get()||!NoReferences()){return Refuse(TEXT("cleanup_refused: veto or references/identity/transaction/file changed after external callback"));}
    if(!Exact()||!W.SDK(TEXT("cleanup_delete_single_object"),true)){return Refuse(TEXT("cleanup_refused: final delete admission failed"));}
    // Public SDK repeats its own veto/close-editor hooks. false avoids its reference path's Undo reset.
    // Those hooks are not atomic with the preflight. Recheck before GC marking or exact file deletion.
    const bool Deleted=ObjectTools::DeleteSingleObject(Weak.Get(),false);W.Cleanup->SetBoolField(TEXT("DeleteSingleObject_returned"),Deleted);
    if(!Deleted||!NoReferences()){return Refuse(TEXT("cleanup_incomplete: public delete/hooks left residual or changed references/context; no GC/file deletion"));}
    if(!Exact()||!W.SDK(TEXT("cleanup_mark_garbage"),true)){return Refuse(TEXT("cleanup_incomplete: GC mark admission failed; preserve residual"));}
    // These are only the exact captured outputs of this Blueprint, not arbitrary classes in its package.
    // No external SDK hook occurs between this final admission and these flag/GC marker writes.
    for(const auto& Entry:OwnedClasses){Entry.Get()->ClearFlags(RF_Public|RF_Standalone);Entry.Get()->MarkAsGarbage();}
    Weak.Get()->MarkAsGarbage();W.Cleanup->SetBoolField(TEXT("MarkAsGarbage_dispatched"),true);
    auto PackageExact=[&](){return W.Admit(TEXT("cleanup_package_identity"),false)&&Package.IsValid()&&Live(Package.Get())&&Package.Get()==W.Package.Get()&&Package->GetName().Equals(PackagePath,ESearchCase::CaseSensitive)&&!GEditor->IsTransactionActive();};
    if(!PackageExact()||!FileSame()){return Refuse(TEXT("cleanup_incomplete: exact file/package changed after object removal"));}
    if(FileExisted){if(!W.SDK(TEXT("cleanup_exact_file_delete"),true)){return Refuse(W.R.Error);}const bool Removed=IFileManager::Get().Delete(*Filename,true,false,true);W.Cleanup->SetBoolField(TEXT("exact_file_delete_returned"),Removed);if(!Removed){return Refuse(TEXT("cleanup_incomplete: exact saved file deletion failed"));}}
    if(!PackageExact()||!W.SDK(TEXT("cleanup_package_dirty_and_registry"),true)){return Refuse(TEXT("cleanup_incomplete: package unavailable after file deletion"));}Package->SetDirtyFlag(false);FAssetRegistryModule::PackageDeleted(Package.Get());
    if(!PackageExact()||!W.SDK(TEXT("cleanup_residual_observation"))){return Refuse(TEXT("cleanup_incomplete: residual observation unsafe"));}FAssetRegistryModule& Registry=FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));const bool Registered=Registry.Get().GetAssetByObjectPath(FSoftObjectPath(Path)).IsValid();
    if(!PackageExact()){return Refuse(TEXT("cleanup_incomplete: context lost during residual query"));}UObject* Remaining=FindObject<UObject>(nullptr,*Path);FString RemainingFilename;const bool PackageExists=FPackageName::DoesPackageExist(PackagePath,&RemainingFilename);
    TArray<UObject*> ResidualExports;GetObjectsWithPackage(Package.Get(),ResidualExports,true,RF_Transient,EInternalObjectFlags::Garbage);bool ResidualAsset=false;for(const auto* O:ResidualExports){ResidualAsset|=O&&O->IsAsset();}
    if((Remaining&&Remaining->IsAsset())||ResidualAsset||Registered||PackageExists||FPaths::FileExists(Filename)){return Refuse(TEXT("cleanup_incomplete: object/registry/package/file residual remains"));}
    W.Cleanup->SetBoolField(TEXT("complete"),true);W.Cleanup->SetBoolField(TEXT("residual"),false);W.Cleanup->SetBoolField(TEXT("atomicity_available"),false);W.Cleanup->SetStringField(TEXT("scope"),TEXT("Exact owned new output only. Public hooks/file TOCTOU/global SDK effects are not atomically rolled back; actual saved history remains."));return true;
}

constexpr const TCHAR* GameplayAbilityClassPath = TEXT("/Script/GameplayAbilities.GameplayAbility");
constexpr const TCHAR* GameplayEffectClassPath = TEXT("/Script/GameplayAbilities.GameplayEffect");
constexpr const TCHAR* AttributeSetClassPath = TEXT("/Script/GameplayAbilities.AttributeSet");
constexpr const TCHAR* AbilitySystemComponentClassPath = TEXT("/Script/GameplayAbilities.AbilitySystemComponent");
constexpr const TCHAR* GameplayAttributeDataStructPath = TEXT("/Script/GameplayAbilities.GameplayAttributeData");
constexpr const TCHAR* GameplayAbilityFactoryClassPath = TEXT("/Script/GameplayAbilitiesEditor.GameplayAbilitiesBlueprintFactory");

enum class EGasBlueprintKind
{
    GameplayAbility,
    GameplayEffect,
    AttributeSet,
};

struct FGasTypes
{
    UClass* GameplayAbility = nullptr;
    UClass* GameplayEffect = nullptr;
    UClass* AttributeSet = nullptr;
    UClass* AbilitySystemComponent = nullptr;
    UScriptStruct* GameplayAttributeData = nullptr;
};

struct FGasComponentMatch
{
    FString Name;
    UClass* ComponentClass = nullptr;
    FString Source;
};

UObject* GetDefaultObjectTracked(UClass* Class){if(!Work().SDK(TEXT("get_default_object"))){return nullptr;}auto* O=Class->GetDefaultObject();return Work().Admit(TEXT("after_get_default_object"))?O:nullptr;}

TSharedPtr<FJsonObject> MakeGasError(
    const FString& ErrorCode,
    const FString& Message,
    bool bMutationAttempted = false,
    bool bMutationPerformed = false)
{
    TSharedPtr<FJsonObject> Result = FGasCompatibility::CreateErrorResponse(Message);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetStringField(TEXT("error_code"), ErrorCode);
    Result->SetBoolField(TEXT("mutation_attempted"), bMutationAttempted);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);
    Result->SetBoolField(TEXT("mutation_state_unknown"), false);
    Result->SetBoolField(TEXT("changed"), bMutationPerformed);
    Result->SetBoolField(TEXT("do_not_retry"), false);
    Result->SetStringField(TEXT("engine_target"), TEXT("5.8.3"));
    Work().Fail(*ErrorCode,Message);
    return Result;
}

TSharedPtr<FJsonObject> MakeGasPartialMutationError(
    const FString& ErrorCode,
    const FString& Message,
    const FString& RecoveryGuidance)
{
    TSharedPtr<FJsonObject> Result = MakeGasError(ErrorCode, Message, true, true);
    Result->SetBoolField(TEXT("partial_mutation"), true);
    Result->SetBoolField(TEXT("workflow_recovery_required"), true);
    Result->SetBoolField(TEXT("do_not_retry"), true);
    Result->SetStringField(TEXT("recovery_guidance"), RecoveryGuidance);
    return Result;
}

TSharedPtr<FJsonObject> MakeGasCreatedAssetAuditError(
    const FString& ErrorCode,
    const FString& Message,
    UObject* NewAsset,
    const FString& AssetPath)
{
    Work().Fail(*ErrorCode,Message);
    FString CleanupError;
    const bool bCleanupSucceeded =
        FGasCompatibility::TryCleanupNewAssetAfterFailedDependencyAudit(NewAsset, CleanupError);
    TSharedPtr<FJsonObject> Result = MakeGasError(
        ErrorCode,
        Message,
        true,
        !bCleanupSucceeded);
    Result->SetStringField(TEXT("created_asset_path"), AssetPath);
    Result->SetBoolField(TEXT("cleanup_attempted"), true);
    Result->SetBoolField(TEXT("cleanup_succeeded"), bCleanupSucceeded);
    Result->SetBoolField(TEXT("structural_change_rolled_back"), bCleanupSucceeded);
    Result->SetBoolField(TEXT("partial_mutation"), !bCleanupSucceeded);
    Result->SetBoolField(TEXT("workflow_recovery_required"), !bCleanupSucceeded);
    Result->SetBoolField(TEXT("do_not_retry"), !bCleanupSucceeded);
    if (!bCleanupSucceeded)
    {
        Result->SetStringField(TEXT("cleanup_error"), CleanupError);
        Result->SetStringField(
            TEXT("recovery_guidance"),
            TEXT("Delete the newly created GAS Blueprint or restore the package from source control before retrying."));
    }
    return Result;
}

TSharedPtr<FJsonObject> MakeGasSuccess(
    const FString& Message,
    bool bMutationAttempted,
    bool bMutationPerformed,
    bool bChanged)
{
    TSharedPtr<FJsonObject> Result = MakeShared<FJsonObject>();
    Result->SetBoolField(TEXT("success"), true);
    Result->SetStringField(TEXT("message"), Message);
    Result->SetBoolField(TEXT("mutation_attempted"), bMutationAttempted);
    Result->SetBoolField(TEXT("mutation_performed"), bMutationPerformed);
    Result->SetBoolField(TEXT("mutation_state_unknown"), false);
    Result->SetBoolField(TEXT("changed"), bChanged);
    TSharedPtr<FJsonObject> Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(TEXT("status"), TEXT("passed"));
    Verification->SetStringField(TEXT("method"), TEXT("ue_5_8_native_readback"));
    Result->SetObjectField(TEXT("verification"), Verification);
    Result->SetStringField(TEXT("engine_target"), TEXT("5.8.3"));
    return Result;
}

bool IsPlaySessionActive()
{
    return GEditor && GEditor->GetPIEWorldContext() != nullptr;
}

bool TryGetRequiredString(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutValue,
    FString& OutError)
{
    const TSharedPtr<FJsonValue> Field = Params.IsValid() ? Params->TryGetField(FieldName) : nullptr;
    if (!Field.IsValid() || Field->Type != EJson::String || !Field->TryGetString(OutValue))
    {
        OutError = FString::Printf(TEXT("Missing or invalid '%s' parameter"), FieldName);
        return false;
    }
    OutValue.TrimStartAndEndInline();
    if (OutValue.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    return true;
}

bool TryGetOptionalString(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    const FString& DefaultValue,
    FString& OutValue,
    FString& OutError)
{
    OutValue = DefaultValue;
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return true;
    }
    const TSharedPtr<FJsonValue> Field = Params->TryGetField(FieldName);
    if (!Field.IsValid() || Field->Type != EJson::String || !Field->TryGetString(OutValue))
    {
        OutError = FString::Printf(TEXT("'%s' must be a string"), FieldName);
        return false;
    }
    OutValue.TrimStartAndEndInline();
    if (OutValue.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    return true;
}

bool TryGetOptionalBool(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    bool bDefaultValue,
    bool& bOutValue,
    FString& OutError)
{
    bOutValue = bDefaultValue;
    if(FString(FieldName).Equals(TEXT("save"),ESearchCase::CaseSensitive)){Work().R.bSaveRequested=bDefaultValue;}
    if (!Params.IsValid() || !Params->HasField(FieldName))
    {
        return true;
    }
    const TSharedPtr<FJsonValue> Field = Params->TryGetField(FieldName);
    if (!Field.IsValid() || Field->Type != EJson::Boolean || !Field->TryGetBool(bOutValue))
    {
        OutError = FString::Printf(TEXT("'%s' must be a boolean"), FieldName);
        return false;
    }
    if(FString(FieldName).Equals(TEXT("save"),ESearchCase::CaseSensitive)){Work().R.bSaveRequested=bOutValue;}
    return true;
}

bool ValidateIdentifier(const TCHAR* FieldName, const FString& Value, FString& OutError)
{
    if (Value.IsEmpty())
    {
        OutError = FString::Printf(TEXT("'%s' must not be empty"), FieldName);
        return false;
    }
    if (Value.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(
            TEXT("'%s' exceeds the Unreal name limit of %d UTF-16 code units"),
            FieldName,
            NAME_SIZE - 1);
        return false;
    }
    if (Value.Equals(TEXT("None"), ESearchCase::IgnoreCase))
    {
        OutError = FString::Printf(TEXT("'%s' must not resolve to NAME_None"), FieldName);
        return false;
    }
    if (FGasCompatibility::IsMCPDependencyReference(Value))
    {
        OutError = FGasCompatibility::GetMCPDependencyReferenceError(FieldName, Value);
        return false;
    }
    if (Value[0] != TEXT('_') && !FChar::IsAlpha(Value[0]))
    {
        OutError = FString::Printf(TEXT("'%s' must begin with a letter or underscore"), FieldName);
        return false;
    }
    for (const TCHAR Character : Value)
    {
        if (Character != TEXT('_') && !FChar::IsAlnum(Character))
        {
            OutError = FString::Printf(
                TEXT("'%s' may contain only letters, numbers, and underscores"),
                FieldName);
            return false;
        }
    }
    FText InvalidReason;
    if (!FName::IsValidXName(Value, INVALID_OBJECTNAME_CHARACTERS, &InvalidReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' value '%s': %s"),
            FieldName,
            *Value,
            *InvalidReason.ToString());
        return false;
    }
    return true;
}

bool ValidateCategory(const FString& Value, FString& OutError)
{
    if (Value.IsEmpty() || Value.Len() >= NAME_SIZE || Value.Contains(TEXT("\r")) || Value.Contains(TEXT("\n")))
    {
        OutError = TEXT("'category' must be a non-empty single-line string within the Unreal name limit");
        return false;
    }
    if (FGasCompatibility::IsMCPDependencyReference(Value))
    {
        OutError = FGasCompatibility::GetMCPDependencyReferenceError(TEXT("category"), Value);
        return false;
    }
    return true;
}

bool NormalizePackageDirectory(FString Value, FString& OutPackageDirectory, FString& OutError)
{
    Value.TrimStartAndEndInline();
    Value.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (Value.Len() > 1 && Value.EndsWith(TEXT("/")))
    {
        Value.LeftChopInline(1);
    }
    if (Value.Len() >= NAME_SIZE)
    {
        OutError = TEXT("'package_path' exceeds the Unreal name limit");
        return false;
    }
    if (Value != TEXT("/Game") && !Value.StartsWith(TEXT("/Game/")))
    {
        OutError = TEXT("'package_path' must be /Game or a child of /Game");
        return false;
    }
    if (Value.Contains(TEXT(".")) || Value.Contains(TEXT("//")))
    {
        OutError = TEXT("'package_path' must be a package directory, not an object path");
        return false;
    }
    if (FGasCompatibility::IsMCPDependencyReference(Value))
    {
        OutError = FGasCompatibility::GetMCPDependencyReferenceError(TEXT("package_path"), Value);
        return false;
    }
    if (Value != TEXT("/Game"))
    {
        FText InvalidReason;
        if (!FPackageName::IsValidLongPackageName(Value, false, &InvalidReason))
        {
            OutError = FString::Printf(
                TEXT("Invalid 'package_path' '%s': %s"),
                *Value,
                *InvalidReason.ToString());
            return false;
        }
    }
    OutPackageDirectory = Value;
    return true;
}

bool BuildAssetPaths(
    const FString& PackageDirectory,
    const FString& AssetName,
    FString& OutPackageName,
    FString& OutObjectPath,
    FString& OutError)
{
    OutPackageName = PackageDirectory + TEXT("/") + AssetName;
    if (OutPackageName.Len() >= NAME_SIZE)
    {
        OutError = TEXT("The destination asset path exceeds the Unreal name limit");
        return false;
    }
    FText InvalidReason;
    if (!FPackageName::IsValidLongPackageName(OutPackageName, false, &InvalidReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid destination package '%s': %s"),
            *OutPackageName,
            *InvalidReason.ToString());
        return false;
    }
    OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *AssetName);
    if (OutObjectPath.Len() >= NAME_SIZE)
    {
        OutError = TEXT("The destination object path exceeds the Unreal name limit");
        return false;
    }
    return true;
}

bool NormalizeBlueprintPath(
    FString Value,
    const TCHAR* FieldName,
    FString& OutPackageName,
    FString& OutObjectPath,
    FString& OutError)
{
    Value = FPackageName::ExportTextPathToObjectPath(Value).TrimStartAndEnd();
    Value.TrimQuotesInline();
    Value.ReplaceInline(TEXT("\\"), TEXT("/"));
    if (Value.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(TEXT("'%s' exceeds the Unreal name limit"), FieldName);
        return false;
    }
    if (Value.Contains(TEXT("//")) || (!Value.StartsWith(TEXT("/Game/"))))
    {
        OutError = FString::Printf(TEXT("'%s' must identify a Blueprint under /Game"), FieldName);
        return false;
    }
    if (FGasCompatibility::IsMCPDependencyReference(Value))
    {
        OutError = FGasCompatibility::GetMCPDependencyReferenceError(FieldName, Value);
        return false;
    }

    OutPackageName = Value.Contains(TEXT("."))
        ? FPackageName::ObjectPathToPackageName(Value)
        : Value;
    FString AssetName = FPackageName::GetLongPackageAssetName(OutPackageName);
    if (Value.Contains(TEXT(".")))
    {
        FString RequestedObjectName = FPackageName::ObjectPathToObjectName(Value);
        if (RequestedObjectName.EndsWith(TEXT("_C")))
        {
            RequestedObjectName.LeftChopInline(2);
        }
        if (!RequestedObjectName.Equals(AssetName, ESearchCase::CaseSensitive))
        {
            OutError = FString::Printf(
                TEXT("'%s' object name must match its package asset name"),
                FieldName);
            return false;
        }
    }
    if (!ValidateIdentifier(FieldName, AssetName, OutError))
    {
        return false;
    }
    FText InvalidReason;
    if (!FPackageName::IsValidLongPackageName(OutPackageName, false, &InvalidReason))
    {
        OutError = FString::Printf(
            TEXT("Invalid '%s' package '%s': %s"),
            FieldName,
            *OutPackageName,
            *InvalidReason.ToString());
        return false;
    }
    OutObjectPath = FString::Printf(TEXT("%s.%s"), *OutPackageName, *AssetName);
    if (OutObjectPath.Len() >= NAME_SIZE)
    {
        OutError = FString::Printf(TEXT("'%s' object path exceeds the Unreal name limit"), FieldName);
        return false;
    }
    return true;
}

UClass* LoadScriptClass(const TCHAR* ClassPath)
{
    if(!Work().R.Error.IsEmpty()||!Work().Admit(TEXT("resolve_script_class"))){return nullptr;}
    if (UClass* Existing = FindObject<UClass>(nullptr, ClassPath))
    {
        return Existing;
    }
    if(!Work().SDK(TEXT("load_script_class"))){return nullptr;}
    auto* Loaded=LoadObject<UClass>(nullptr,ClassPath);return Work().Admit(TEXT("after_load_script_class"))?Loaded:nullptr;
}

UScriptStruct* LoadScriptStruct(const TCHAR* StructPath)
{
    if(!Work().R.Error.IsEmpty()||!Work().Admit(TEXT("resolve_script_struct"))){return nullptr;}
    if (UScriptStruct* Existing = FindObject<UScriptStruct>(nullptr, StructPath))
    {
        return Existing;
    }
    if(!Work().SDK(TEXT("load_script_struct"))){return nullptr;}
    auto* Loaded=LoadObject<UScriptStruct>(nullptr,StructPath);return Work().Admit(TEXT("after_load_script_struct"))?Loaded:nullptr;
}

bool ResolveGasTypes(FGasTypes& OutTypes, FString& OutError)
{
    const TSharedPtr<IPlugin> GameplayAbilitiesPlugin = IPluginManager::Get().FindPlugin(TEXT("GameplayAbilities"));
    if (!GameplayAbilitiesPlugin.IsValid() || !GameplayAbilitiesPlugin->IsEnabled())
    {
        OutError = TEXT("GameplayAbilities plugin is not enabled. Enable the UE 5.8 GameplayAbilities plugin for the project and restart the editor.");
        return false;
    }

    OutTypes.GameplayAbility = LoadScriptClass(GameplayAbilityClassPath);
    OutTypes.GameplayEffect = LoadScriptClass(GameplayEffectClassPath);
    OutTypes.AttributeSet = LoadScriptClass(AttributeSetClassPath);
    OutTypes.AbilitySystemComponent = LoadScriptClass(AbilitySystemComponentClassPath);
    OutTypes.GameplayAttributeData = LoadScriptStruct(GameplayAttributeDataStructPath);
    if (!OutTypes.GameplayAbility || !OutTypes.GameplayEffect || !OutTypes.AttributeSet ||
        !OutTypes.AbilitySystemComponent || !OutTypes.GameplayAttributeData)
    {
        OutError = TEXT("UE 5.8 GameplayAbilities types could not be resolved. Verify the GameplayAbilities plugin is enabled and loaded.");
        return false;
    }
    if (!OutTypes.AbilitySystemComponent->IsChildOf(UActorComponent::StaticClass()))
    {
        OutError = TEXT("Resolved AbilitySystemComponent is not an ActorComponent");
        return false;
    }
    return true;
}

UBlueprint* LoadBlueprintAsset(const FString& PackageName)
{
    if(!Work().SDK(TEXT("load_blueprint"))){return nullptr;}
    auto* B=Cast<UBlueprint>(UEditorAssetLibrary::LoadAsset(PackageName));
    if(!Work().Admit(TEXT("after_load_blueprint"))||!B||!Work().Pin(B)){return nullptr;}return B;
}

FString BlueprintStatusToString(EBlueprintStatus Status)
{
    switch (Status)
    {
    case BS_Unknown: return TEXT("unknown");
    case BS_Dirty: return TEXT("dirty");
    case BS_Error: return TEXT("error");
    case BS_UpToDate: return TEXT("up_to_date");
    case BS_BeingCreated: return TEXT("being_created");
    case BS_UpToDateWithWarnings: return TEXT("up_to_date_with_warnings");
    default: return TEXT("unknown");
    }
}

UClass* ParentClassForKind(const FGasTypes& Types, EGasBlueprintKind Kind)
{
    switch (Kind)
    {
    case EGasBlueprintKind::GameplayAbility: return Types.GameplayAbility;
    case EGasBlueprintKind::GameplayEffect: return Types.GameplayEffect;
    case EGasBlueprintKind::AttributeSet: return Types.AttributeSet;
    default: return nullptr;
    }
}

const TCHAR* KindName(EGasBlueprintKind Kind)
{
    switch (Kind)
    {
    case EGasBlueprintKind::GameplayAbility: return TEXT("gameplay_ability");
    case EGasBlueprintKind::GameplayEffect: return TEXT("gameplay_effect");
    case EGasBlueprintKind::AttributeSet: return TEXT("attribute_set");
    default: return TEXT("unknown");
    }
}

UFactory* CreateFactoryForKind(
    EGasBlueprintKind Kind,
    UClass* ParentClass,
    UClass*& OutAssetClass,
    FString& OutError)
{
    OutAssetClass = nullptr;
    if (Kind != EGasBlueprintKind::GameplayAbility)
    {
        if(!Work().SDK(TEXT("create_transient_blueprint_factory"))){OutError=Work().R.Error;return nullptr;}
        UBlueprintFactory* Factory = NewObject<UBlueprintFactory>();
        if(!Live(Factory)||!Work().Admit(TEXT("after_create_transient_factory"))){OutError=TEXT("Factory creation/context failed");return nullptr;}Work().Factory.Reset(Factory);
        Factory->ParentClass = ParentClass;
        OutAssetClass = UBlueprint::StaticClass();
        return Factory;
    }

    UClass* FactoryClass = LoadScriptClass(GameplayAbilityFactoryClassPath);
    if (!FactoryClass || !FactoryClass->IsChildOf(UFactory::StaticClass()))
    {
        OutError = TEXT("UE 5.8 GameplayAbilities Blueprint factory is unavailable");
        return nullptr;
    }
    if(!Work().SDK(TEXT("create_transient_ability_factory"))){OutError=Work().R.Error;return nullptr;}
    UFactory* Factory = NewObject<UFactory>(GetTransientPackage(), FactoryClass);
    Work().Factory.Reset(Factory);
    if(!Work().Admit(TEXT("after_create_transient_ability_factory"))){OutError=Work().R.Error;return nullptr;}
    FClassProperty* ParentClassProperty = FindFProperty<FClassProperty>(FactoryClass, TEXT("ParentClass"));
    FByteProperty* BlueprintTypeProperty = FindFProperty<FByteProperty>(FactoryClass, TEXT("BlueprintType"));
    if (!Factory || !ParentClassProperty || !BlueprintTypeProperty)
    {
        OutError = TEXT("UE 5.8 GameplayAbilities Blueprint factory contract is unavailable");
        return nullptr;
    }
    ParentClassProperty->SetObjectPropertyValue_InContainer(Factory, ParentClass);
    BlueprintTypeProperty->SetPropertyValue_InContainer(Factory, static_cast<uint8>(BPTYPE_Normal));
    OutAssetClass = Factory->GetSupportedClass();
    if (!OutAssetClass || !OutAssetClass->IsChildOf(UBlueprint::StaticClass()))
    {
        OutError = TEXT("UE 5.8 GameplayAbilities Blueprint factory returned an invalid supported class");
        return nullptr;
    }
    return Factory;
}

bool SaveBlueprintIfRequested(UBlueprint* Blueprint,bool bSave){return Work().Save(Blueprint,bSave);}

TSharedPtr<FJsonObject> BuildIdempotentBlueprintResult(
    UBlueprint* Blueprint,
    const FString& PackageName,
    EGasBlueprintKind Kind)
{
    TSharedPtr<FJsonObject> Result = MakeGasSuccess(
        TEXT("Compatible GAS Blueprint already exists; no mutation was needed"),
        false,
        false,
        false);
    Result->SetBoolField(TEXT("already_exists"), true);
    Result->SetBoolField(TEXT("created"), false);
    Result->SetStringField(TEXT("gas_blueprint_kind"), KindName(Kind));
    Result->SetStringField(TEXT("asset_path"), PackageName);
    Result->SetStringField(TEXT("object_path"), Blueprint ? Blueprint->GetPathName() : FString());
    Result->SetStringField(TEXT("parent_class"), Blueprint && Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
    Result->SetBoolField(TEXT("saved"), false);
    return Result;
}

TSharedPtr<FJsonObject> CreateGasBlueprint(
    const TSharedPtr<FJsonObject>& Params,
    EGasBlueprintKind Kind,
    const TCHAR* DefaultPackagePath)
{
    FString AssetName;
    FString PackageDirectory;
    FString Error;
    bool bSave = true;
    if (!TryGetRequiredString(Params, TEXT("asset_name"), AssetName, Error) ||
        !TryGetOptionalString(Params, TEXT("package_path"), DefaultPackagePath, PackageDirectory, Error) ||
        !TryGetOptionalBool(Params, TEXT("save"), true, bSave, Error) ||
        !ValidateIdentifier(TEXT("asset_name"), AssetName, Error) ||
        !NormalizePackageDirectory(PackageDirectory, PackageDirectory, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }

    FString PackageName;
    FString ObjectPath;
    if (!BuildAssetPaths(PackageDirectory, AssetName, PackageName, ObjectPath, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    if (IsPlaySessionActive())
    {
        return MakeGasError(
            TEXT("gas_play_session_active"),
            TEXT("Cannot create or replace GAS Blueprints while PIE/SIE is active"));
    }

    FGasTypes Types;
    if (!ResolveGasTypes(Types, Error))
    {
        return MakeGasError(TEXT("gas_plugin_unavailable"), Error);
    }
    UClass* ParentClass = ParentClassForKind(Types, Kind);
    if (!ParentClass || !FKismetEditorUtilities::CanCreateBlueprintOfClass(ParentClass))
    {
        return MakeGasError(
            TEXT("gas_parent_class_not_blueprintable"),
            FString::Printf(TEXT("UE 5.8 GAS parent class is not Blueprintable: %s"), *GetPathNameSafe(ParentClass)));
    }

    UClass* AssetClass = nullptr;
    UFactory* Factory = CreateFactoryForKind(Kind, ParentClass, AssetClass, Error);
    if (!Factory || !AssetClass)
    {
        return MakeGasError(TEXT("gas_factory_unavailable"), Error);
    }

    if(!Work().SDK(TEXT("does_asset_exist"))){return MakeGasError(TEXT("gas_context_changed"),Work().R.Error);}
    const bool bAssetExists = UEditorAssetLibrary::DoesAssetExist(PackageName);
    if(!Work().Admit(TEXT("after_does_asset_exist"))){return MakeGasError(TEXT("gas_context_changed"),Work().R.Error);}
    UBlueprint* ExistingBlueprint = bAssetExists ? LoadBlueprintAsset(PackageName) : nullptr;
    if (bAssetExists && !ExistingBlueprint)
    {
        return MakeGasError(
            TEXT("gas_existing_asset_wrong_type"),
            FString::Printf(TEXT("Destination exists but is not a Blueprint: %s"), *PackageName));
    }
    if (ExistingBlueprint)
    {
        if (!ExistingBlueprint->GeneratedClass ||
            !ExistingBlueprint->GeneratedClass->IsChildOf(ParentClass) ||
            ExistingBlueprint->Status == BS_Error)
        {
            return MakeGasError(
                TEXT("gas_existing_blueprint_verification_failed"),
                FString::Printf(
                    TEXT("Destination Blueprint is uncompiled or does not derive from %s: %s"),
                    *ParentClass->GetPathName(),
                    *PackageName));
        }
        return BuildIdempotentBlueprintResult(ExistingBlueprint, PackageName, Kind);
    }

    Work().NewPackageOwned=FindPackage(nullptr,*PackageName)==nullptr;
    FString ExistingFilename;Work().OriginalFileExisted=FPackageName::DoesPackageExist(PackageName,&ExistingFilename);
    FString RequestedFilename;const bool HasRequestedFilename=FPackageName::TryConvertLongPackageNameToFilename(PackageName,RequestedFilename,FPackageName::GetAssetPackageExtension());
    if(!Work().NewPackageOwned||Work().OriginalFileExisted||!HasRequestedFilename||FPaths::FileExists(RequestedFilename)){return MakeGasError(TEXT("gas_destination_state_changed"),TEXT("Destination memory/package/file appeared after initial existence check; stop without overwrite or automatic reload/retry."));}
    if(!Work().SDK(TEXT("create_package"),true)){return MakeGasError(TEXT("gas_context_changed"),Work().R.Error);}
    UPackage* Package = CreatePackage(*PackageName);
    Work().Package.Reset(Package);
    if (!Package)
    {
        return MakeGasError(
            TEXT("gas_package_create_failed"),
            TEXT("Failed to create GAS Blueprint package"),
            true,
            false);
    }

    const TWeakObjectPtr<UPackage> RequestedPackage=Package;const TWeakObjectPtr<UClass> RequestedAssetClass=AssetClass;const FString RequestedObjectPath=ObjectPath;
    Work().R.Data->SetStringField(TEXT("requested_output_object_path"),RequestedObjectPath);
    if(!Work().Admit(TEXT("before_factory_collision_admission"),false)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect the created package; no replay."));}
    if(!Work().SDK(TEXT("does_asset_exist_pre_factory"))){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect created package; no replay."));}
    const bool RegistryCollision=UEditorAssetLibrary::DoesAssetExist(PackageName);
    if(!Work().Admit(TEXT("after_factory_collision_lookup"),false)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect created package; no replay."));}
    if(RegistryCollision||!Live(Package)||!Package->GetName().Equals(PackageName,ESearchCase::CaseSensitive)||FindPackage(nullptr,*PackageName)!=Package||FindObject<UObject>(Package,*AssetName)||FPaths::FileExists(RequestedFilename)){return MakeGasPartialMutationError(TEXT("gas_destination_state_changed"),TEXT("Requested object/registry/disk collision or package replacement appeared before Factory dispatch"),TEXT("Inspect actual package without overwrite or automatic reload/retry."));}
    if(!Work().SDK(TEXT("factory_create_blueprint"),true)||!Live(Factory)||!Live(Package)){return MakeGasPartialMutationError(TEXT("gas_blueprint_create_failed"),TEXT("Factory/package context unavailable"),TEXT("Inspect the partial package before retrying."));}
    UObject* CreatedObject = Factory->FactoryCreateNew(
        AssetClass,
        Package,
        FName(*AssetName),
        RF_Public | RF_Standalone | RF_Transactional,
        nullptr,
        GWarn);
    Work().R.Data->SetBoolField(TEXT("factory_returned"),true);Work().R.Data->SetBoolField(TEXT("factory_returned_nonnull"),CreatedObject!=nullptr);Work().R.Data->SetStringField(TEXT("factory_returned_pointer_identity"),LexToString(reinterpret_cast<UPTRINT>(CreatedObject)));
    if(!Work().Admit(TEXT("after_factory_return"),false)){return MakeGasPartialMutationError(TEXT("gas_factory_context_lost"),Work().R.Error,TEXT("Returned nonnull history retained; UObject observation/cleanup skipped."));}
    if(CreatedObject&&(!Live(CreatedObject)||!RequestedPackage.IsValid()||RequestedPackage.Get()!=Package||!RequestedAssetClass.IsValid()||CreatedObject->GetClass()!=RequestedAssetClass.Get()||CreatedObject->GetOuter()!=RequestedPackage.Get()||!CreatedObject->GetPathName().Equals(RequestedObjectPath,ESearchCase::CaseSensitive))){Work().R.Data->SetBoolField(TEXT("factory_exact_output_verified"),false);return MakeGasPartialMutationError(TEXT("gas_factory_output_identity_mismatch"),TEXT("Factory returned a different class/outer/package/path than the captured request"),TEXT("Inspect unexpected/partial outputs. They are not adopted as an owned cleanup target."));}
    Work().R.Data->SetBoolField(TEXT("factory_exact_output_verified"),CreatedObject!=nullptr);
    UBlueprint* Blueprint = Cast<UBlueprint>(CreatedObject);
    if (!Blueprint)
    {
        return MakeGasPartialMutationError(
            TEXT("gas_blueprint_create_failed"),
            FString::Printf(TEXT("Failed to create GAS Blueprint: %s"), *PackageName),
            TEXT("Inspect and remove any partial package before retrying."));
    }

    if(!Work().Admit(TEXT("after_factory_create"))||!Work().Pin(Blueprint)||!Work().SDK(TEXT("asset_registry_asset_created"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect the created asset; no replay."));}
    FAssetRegistryModule::AssetCreated(Blueprint);
    if(!Work().SDK(TEXT("mark_blueprint_structurally_modified"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);

    if(!Work().Compile(Blueprint)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    if(!Work().SDK(TEXT("mark_package_dirty"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    Package->MarkPackageDirty();

    if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(ParentClass) || Blueprint->Status == BS_Error)
    {
        return MakeGasPartialMutationError(
            TEXT("gas_blueprint_verification_failed"),
            FString::Printf(TEXT("Created GAS Blueprint failed compile/type verification: %s"), *PackageName),
            TEXT("Inspect or delete the partial Blueprint before retrying."));
    }

    FString DependencyAuditError;
    if (!FGasCompatibility::ValidateMCPDependencyIndependence(
            TEXT("new GAS Blueprint after compile and before save"),
            Blueprint,
            EDependencyAuditMode::InMemoryOnly,
            DependencyAuditError))
    {
        return MakeGasCreatedAssetAuditError(
            TEXT("gas_blueprint_pre_save_dependency_audit_failed"),
            DependencyAuditError,
            Blueprint,
            PackageName);
    }
    if (!SaveBlueprintIfRequested(Blueprint, bSave))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_blueprint_save_failed"),
            FString::Printf(TEXT("Created GAS Blueprint but failed to save it: %s"), *PackageName),
            TEXT("Resolve package write access, then save or revert the dirty Blueprint manually."));
    }
    if (bSave && !FGasCompatibility::ValidateMCPDependencyIndependence(
                     TEXT("new GAS Blueprint after save"),
                     Blueprint,
                     EDependencyAuditMode::RequirePersisted,
                     DependencyAuditError))
    {
        return MakeGasCreatedAssetAuditError(
            TEXT("gas_blueprint_post_save_dependency_audit_failed"),
            DependencyAuditError,
            Blueprint,
            PackageName);
    }

    TSharedPtr<FJsonObject> Result = MakeGasSuccess(
        TEXT("Created and verified UE 5.8 GAS Blueprint"),
        true,
        true,
        true);
    Result->SetBoolField(TEXT("already_exists"), false);
    Result->SetBoolField(TEXT("created"), true);
    Result->SetBoolField(TEXT("saved"), bSave);
    Result->SetStringField(TEXT("gas_blueprint_kind"), KindName(Kind));
    Result->SetStringField(TEXT("asset_path"), PackageName);
    Result->SetStringField(TEXT("object_path"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("blueprint_class"), Blueprint->GetClass()->GetPathName());
    Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
    Result->SetStringField(TEXT("compile_status"), BlueprintStatusToString(Blueprint->Status));
    Result->SetStringField(TEXT("verification_method"), TEXT("generated_class_parent_and_blueprint_status_readback"));
    return Result;
}

USCS_Node* FindSCSNodeByName(UBlueprint* Blueprint, const FName ComponentName)
{
    if (!Blueprint || !Blueprint->SimpleConstructionScript)
    {
        return nullptr;
    }
    for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
    {
        if (Node && Node->GetVariableName() == ComponentName)
        {
            return Node;
        }
    }
    return nullptr;
}

bool FindAbilitySystemComponent(
    UBlueprint* Blueprint,
    UClass* AbilitySystemComponentClass,
    FGasComponentMatch& OutMatch)
{
    if (!Blueprint || !AbilitySystemComponentClass)
    {
        return false;
    }
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (Node && Node->ComponentClass && Node->ComponentClass->IsChildOf(AbilitySystemComponentClass))
            {
                OutMatch.Name = Node->GetVariableName().ToString();
                OutMatch.ComponentClass = Node->ComponentClass;
                OutMatch.Source = TEXT("simple_construction_script");
                return true;
            }
        }
    }
    if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
    {
        if (AActor* ActorCDO = Cast<AActor>(GetDefaultObjectTracked(Blueprint->GeneratedClass)))
        {
            TArray<UActorComponent*> Components;
            ActorCDO->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (Component && Component->IsA(AbilitySystemComponentClass))
                {
                    OutMatch.Name = Component->GetName();
                    OutMatch.ComponentClass = Component->GetClass();
                    OutMatch.Source = TEXT("class_default_object");
                    return true;
                }
            }
        }
    }
    return false;
}
TSharedPtr<FJsonObject> HandleInspectGASBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    FString RequestedPath;
    FString Error;
    if (!TryGetRequiredString(Params, TEXT("blueprint_path"), RequestedPath, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    FString PackageName;
    FString ObjectPath;
    if (!NormalizeBlueprintPath(RequestedPath, TEXT("blueprint_path"), PackageName, ObjectPath, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }

    FGasTypes Types;
    if (!ResolveGasTypes(Types, Error))
    {
        return MakeGasError(TEXT("gas_plugin_unavailable"), Error);
    }
    UBlueprint* Blueprint = LoadBlueprintAsset(PackageName);
    if (!Blueprint)
    {
        return MakeGasError(
            TEXT("gas_blueprint_not_found"),
            FString::Printf(TEXT("Blueprint not found: %s"), *PackageName));
    }

    TSharedPtr<FJsonObject> Result = MakeGasSuccess(
        TEXT("Inspected UE 5.8 GAS Blueprint"),
        false,
        false,
        false);
    Result->SetStringField(TEXT("blueprint_path"), PackageName);
    Result->SetStringField(TEXT("object_path"), Blueprint->GetPathName());
    Result->SetStringField(TEXT("blueprint_class"), Blueprint->GetClass()->GetPathName());
    Result->SetStringField(TEXT("parent_class"), Blueprint->ParentClass ? Blueprint->ParentClass->GetPathName() : FString());
    Result->SetStringField(TEXT("generated_class"), Blueprint->GeneratedClass ? Blueprint->GeneratedClass->GetPathName() : FString());
    Result->SetStringField(TEXT("compile_status"), BlueprintStatusToString(Blueprint->Status));

    const bool bGameplayAbility = Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(Types.GameplayAbility);
    const bool bGameplayEffect = Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(Types.GameplayEffect);
    const bool bAttributeSet = Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(Types.AttributeSet);
    Result->SetBoolField(TEXT("is_gameplay_ability"), bGameplayAbility);
    Result->SetBoolField(TEXT("is_gameplay_effect"), bGameplayEffect);
    Result->SetBoolField(TEXT("is_attribute_set"), bAttributeSet);
    Result->SetBoolField(TEXT("is_gas_blueprint"), bGameplayAbility || bGameplayEffect || bAttributeSet);

    TArray<TSharedPtr<FJsonValue>> ComponentValues;
    TSet<FString> SeenComponents;
    if (Blueprint->SimpleConstructionScript)
    {
        for (USCS_Node* Node : Blueprint->SimpleConstructionScript->GetAllNodes())
        {
            if (!Node || !Node->ComponentClass || !Node->ComponentClass->IsChildOf(Types.AbilitySystemComponent))
            {
                continue;
            }
            const FString Key = Node->GetVariableName().ToString() + TEXT("|") + Node->ComponentClass->GetPathName();
            SeenComponents.Add(Key);
            TSharedPtr<FJsonObject> Component = MakeShared<FJsonObject>();
            Component->SetStringField(TEXT("component_name"), Node->GetVariableName().ToString());
            Component->SetStringField(TEXT("component_class"), Node->ComponentClass->GetPathName());
            Component->SetStringField(TEXT("source"), TEXT("simple_construction_script"));
            ComponentValues.Add(MakeShared<FJsonValueObject>(Component));
        }
    }
    if (Blueprint->GeneratedClass && Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()))
    {
        if (AActor* ActorCDO = Cast<AActor>(GetDefaultObjectTracked(Blueprint->GeneratedClass)))
        {
            TArray<UActorComponent*> Components;
            ActorCDO->GetComponents(Components);
            for (UActorComponent* ComponentInstance : Components)
            {
                if (!ComponentInstance || !ComponentInstance->IsA(Types.AbilitySystemComponent))
                {
                    continue;
                }
                const FString Key = ComponentInstance->GetName() + TEXT("|") + ComponentInstance->GetClass()->GetPathName();
                if (SeenComponents.Contains(Key))
                {
                    continue;
                }
                SeenComponents.Add(Key);
                TSharedPtr<FJsonObject> Component = MakeShared<FJsonObject>();
                Component->SetStringField(TEXT("component_name"), ComponentInstance->GetName());
                Component->SetStringField(TEXT("component_class"), ComponentInstance->GetClass()->GetPathName());
                Component->SetStringField(TEXT("source"), TEXT("class_default_object"));
                ComponentValues.Add(MakeShared<FJsonValueObject>(Component));
            }
        }
    }
    Result->SetBoolField(TEXT("has_ability_system_component"), ComponentValues.Num() > 0);
    Result->SetNumberField(TEXT("ability_system_component_count"), ComponentValues.Num());
    Result->SetArrayField(TEXT("ability_system_components"), ComponentValues);

    TArray<TSharedPtr<FJsonValue>> AttributeValues;
    if (Blueprint->GeneratedClass)
    {
        for (TFieldIterator<FStructProperty> PropertyIt(Blueprint->GeneratedClass); PropertyIt; ++PropertyIt)
        {
            FStructProperty* Property = *PropertyIt;
            if (!Property || Property->Struct != Types.GameplayAttributeData)
            {
                continue;
            }
            TSharedPtr<FJsonObject> Attribute = MakeShared<FJsonObject>();
            Attribute->SetStringField(TEXT("attribute_name"), Property->GetName());
            Attribute->SetStringField(TEXT("struct_type"), Property->Struct->GetPathName());
            Attribute->SetStringField(TEXT("owner_class"), GetPathNameSafe(Property->GetOwnerClass()));
            Attribute->SetBoolField(TEXT("declared_here"), Property->GetOwnerClass() == Blueprint->GeneratedClass);
            AttributeValues.Add(MakeShared<FJsonValueObject>(Attribute));
        }
    }
    Result->SetNumberField(TEXT("gameplay_attribute_count"), AttributeValues.Num());
    Result->SetArrayField(TEXT("gameplay_attributes"), AttributeValues);
    Result->SetStringField(TEXT("verification_method"), TEXT("generated_class_and_scs_reflection"));
    return Result;
}

TSharedPtr<FJsonObject> HandleCreateGameplayAbilityBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    return CreateGasBlueprint(Params, EGasBlueprintKind::GameplayAbility, TEXT("/Game/GAS/Abilities"));
}

TSharedPtr<FJsonObject> HandleCreateGameplayEffectBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    return CreateGasBlueprint(Params, EGasBlueprintKind::GameplayEffect, TEXT("/Game/GAS/Effects"));
}

TSharedPtr<FJsonObject> HandleCreateAttributeSetBlueprint(
    const TSharedPtr<FJsonObject>& Params)
{
    return CreateGasBlueprint(Params, EGasBlueprintKind::AttributeSet, TEXT("/Game/GAS/Attributes"));
}

TSharedPtr<FJsonObject> HandleAddAbilitySystemComponent(
    const TSharedPtr<FJsonObject>& Params)
{
    FString RequestedPath;
    FString ComponentName;
    FString Error;
    bool bSave = true;
    if (!TryGetRequiredString(Params, TEXT("blueprint_path"), RequestedPath, Error) ||
        !TryGetOptionalString(Params, TEXT("component_name"), TEXT("AbilitySystem"), ComponentName, Error) ||
        !TryGetOptionalBool(Params, TEXT("save"), true, bSave, Error) ||
        !ValidateIdentifier(TEXT("component_name"), ComponentName, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    FString PackageName;
    FString ObjectPath;
    if (!NormalizeBlueprintPath(RequestedPath, TEXT("blueprint_path"), PackageName, ObjectPath, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    if (IsPlaySessionActive())
    {
        return MakeGasError(
            TEXT("gas_play_session_active"),
            TEXT("Cannot modify Blueprint components while PIE/SIE is active"));
    }

    FGasTypes Types;
    if (!ResolveGasTypes(Types, Error))
    {
        return MakeGasError(TEXT("gas_plugin_unavailable"), Error);
    }
    UBlueprint* Blueprint = LoadBlueprintAsset(PackageName);
    if (!Blueprint)
    {
        return MakeGasError(TEXT("gas_blueprint_not_found"), FString::Printf(TEXT("Blueprint not found: %s"), *PackageName));
    }
    if (!Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(AActor::StaticClass()) || !Blueprint->SimpleConstructionScript)
    {
        return MakeGasError(
            TEXT("gas_blueprint_not_actor_based"),
            TEXT("AbilitySystemComponent can only be added to an Actor-based Blueprint with a SimpleConstructionScript"));
    }
    if (Blueprint->Status == BS_Error)
    {
        return MakeGasError(
            TEXT("gas_blueprint_compile_error"),
            TEXT("Blueprint has compile errors; fix and compile it before adding an AbilitySystemComponent"));
    }

    FGasComponentMatch ExistingASC;
    if (FindAbilitySystemComponent(Blueprint, Types.AbilitySystemComponent, ExistingASC))
    {
        TSharedPtr<FJsonObject> Result = MakeGasSuccess(
            TEXT("Blueprint already has an AbilitySystemComponent; no mutation was needed"),
            false,
            false,
            false);
        Result->SetBoolField(TEXT("already_exists"), true);
        Result->SetStringField(TEXT("blueprint_path"), PackageName);
        Result->SetStringField(TEXT("component_name"), ExistingASC.Name);
        Result->SetStringField(TEXT("component_class"), GetPathNameSafe(ExistingASC.ComponentClass));
        Result->SetStringField(TEXT("component_source"), ExistingASC.Source);
        Result->SetBoolField(TEXT("saved"), false);
        return Result;
    }

    const FName ComponentFName(*ComponentName);
    USCS_Node* NameCollision = FindSCSNodeByName(Blueprint, ComponentFName);
    if (NameCollision)
    {
        return MakeGasError(
            TEXT("gas_component_name_conflict"),
            FString::Printf(
                TEXT("A different component already uses name '%s'"),
                *ComponentName));
    }
    UBlueprint* VariableOwner = nullptr;
    if (!NameCollision &&
        (FBlueprintEditorUtils::FindNewVariableIndexAndBlueprint(
             Blueprint,
             ComponentFName,
             VariableOwner) != INDEX_NONE ||
         FindFProperty<FProperty>(Blueprint->GeneratedClass, ComponentFName)))
    {
        return MakeGasError(
            TEXT("gas_component_property_name_conflict"),
            FString::Printf(
                TEXT("A Blueprint or inherited property already uses component name '%s'"),
                *ComponentName));
    }

    FString DependencyAuditError;
    if (!FGasCompatibility::ValidateMCPDependencyIndependence(
            TEXT("GAS AbilitySystemComponent Blueprint before mutation"),
            Blueprint,
            EDependencyAuditMode::InMemoryOnly,
            DependencyAuditError))
    {
        return MakeGasError(
            TEXT("mcp_dependency_reference_rejected"),
            DependencyAuditError);
    }

    Work().CapturedSCS=Blueprint->SimpleConstructionScript;Work().SCSPinned=true;
    if(!Work().SDK(TEXT("modify_blueprint"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    Blueprint->Modify();
    if(!Work().SDK(TEXT("modify_simple_construction_script"),true)||!Live(Blueprint->SimpleConstructionScript)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    Blueprint->SimpleConstructionScript->Modify();

    if(!Work().SDK(TEXT("create_scs_node"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    USCS_Node* NewNode = Blueprint->SimpleConstructionScript->CreateNode(
        Types.AbilitySystemComponent,
        ComponentFName);
    if (!NewNode)
    {
        return MakeGasPartialMutationError(
            TEXT("gas_component_create_failed"),
            TEXT("Failed to create AbilitySystemComponent SCS node"),
            TEXT("Inspect the Blueprint component tree before retrying."));
    }
    if(!Work().Admit(TEXT("after_create_scs_node"))||!Live(NewNode)||NewNode->GetOuter()!=Work().CapturedSCS.Get()||NewNode->GetVariableName()!=ComponentFName||NewNode->ComponentClass!=Types.AbilitySystemComponent||!Live(NewNode->ComponentTemplate)||NewNode->ComponentTemplate->GetClass()!=Types.AbilitySystemComponent||NewNode->ComponentTemplate->GetOuter()!=Blueprint->GeneratedClass||!Work().SDK(TEXT("add_scs_node"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    Blueprint->SimpleConstructionScript->AddNode(NewNode);
    if(!Work().SDK(TEXT("mark_blueprint_structurally_modified"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if(!Work().Compile(Blueprint)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}

    USCS_Node* VerifiedNode = FindSCSNodeByName(Blueprint, ComponentFName);
    if (Blueprint->Status == BS_Error || !VerifiedNode || !VerifiedNode->ComponentClass ||
        !VerifiedNode->ComponentClass->IsChildOf(Types.AbilitySystemComponent))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_component_verification_failed"),
            TEXT("AbilitySystemComponent was added but Blueprint compile/readback verification failed"),
            TEXT("Inspect or undo the partial Blueprint component change before retrying."));
    }
    const TWeakObjectPtr<USCS_Node> VerifiedNodeIdentity=VerifiedNode;const TWeakObjectPtr<UActorComponent> VerifiedTemplateIdentity=VerifiedNode->ComponentTemplate;
    if (!FGasCompatibility::ValidateMCPDependencyIndependence(
            TEXT("GAS AbilitySystemComponent Blueprint after compile and before save"),
            Blueprint,
            EDependencyAuditMode::InMemoryOnly,
            DependencyAuditError))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_component_pre_save_dependency_audit_failed"),
            DependencyAuditError,
            TEXT("Undo or remove the in-memory MCP-only dependency before saving the Blueprint."));
    }
    if (!SaveBlueprintIfRequested(Blueprint, bSave))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_component_save_failed"),
            TEXT("AbilitySystemComponent was added but the Blueprint could not be saved"),
            TEXT("Resolve package write access, then save or revert the dirty Blueprint manually."));
    }
    if (bSave && !FGasCompatibility::ValidateMCPDependencyIndependence(
                     TEXT("GAS AbilitySystemComponent Blueprint after save"),
                     Blueprint,
                     EDependencyAuditMode::RequirePersisted,
                     DependencyAuditError))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_component_post_save_dependency_audit_failed"),
            DependencyAuditError,
            TEXT("Restore or remove the persisted MCP-only dependency before using the Blueprint without UnrealMCP."));
    }

    TSharedPtr<FJsonObject> Result = MakeGasSuccess(
        TEXT("Added and verified AbilitySystemComponent"),
        true,
        true,
        true);
    Result->SetBoolField(TEXT("already_exists"), false);
    Result->SetBoolField(TEXT("saved"), bSave);
    Result->SetStringField(TEXT("blueprint_path"), PackageName);
    Result->SetStringField(TEXT("component_name"), ComponentName);
    if(!Work().Admit(TEXT("final_ASC_readback"))){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    USCS_Node* CurrentNode=FindSCSNodeByName(Blueprint,ComponentFName);
    if(!Live(CurrentNode)||CurrentNode!=VerifiedNodeIdentity.Get()||!Live(CurrentNode->ComponentTemplate)||CurrentNode->ComponentTemplate!=VerifiedTemplateIdentity.Get()||!CurrentNode->ComponentClass||!CurrentNode->ComponentClass->IsChildOf(Types.AbilitySystemComponent)){return MakeGasPartialMutationError(TEXT("gas_component_post_save_verification_failed"),TEXT("SCS/node/template changed after audit/save"),TEXT("Inspect actual saved history and current structure; no replay."));}
    Result->SetStringField(TEXT("component_class"),CurrentNode->ComponentClass->GetPathName());
    Result->SetStringField(TEXT("verification_method"), TEXT("scs_node_class_readback_after_compile"));
    return Result;
}

TSharedPtr<FJsonObject> HandleAddGameplayAttribute(
    const TSharedPtr<FJsonObject>& Params)
{
    FString RequestedPath;
    FString AttributeName;
    FString Category;
    FString Error;
    bool bSave = true;
    if (!TryGetRequiredString(Params, TEXT("attribute_set_path"), RequestedPath, Error) ||
        !TryGetRequiredString(Params, TEXT("attribute_name"), AttributeName, Error) ||
        !TryGetOptionalString(Params, TEXT("category"), TEXT("GAS|Attributes"), Category, Error) ||
        !TryGetOptionalBool(Params, TEXT("save"), true, bSave, Error) ||
        !ValidateIdentifier(TEXT("attribute_name"), AttributeName, Error) ||
        !ValidateCategory(Category, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    FString PackageName;
    FString ObjectPath;
    if (!NormalizeBlueprintPath(RequestedPath, TEXT("attribute_set_path"), PackageName, ObjectPath, Error))
    {
        return MakeGasError(TEXT("gas_invalid_request"), Error);
    }
    if (IsPlaySessionActive())
    {
        return MakeGasError(
            TEXT("gas_play_session_active"),
            TEXT("Cannot modify AttributeSet Blueprint structure while PIE/SIE is active"));
    }

    FGasTypes Types;
    if (!ResolveGasTypes(Types, Error))
    {
        return MakeGasError(TEXT("gas_plugin_unavailable"), Error);
    }
    UBlueprint* Blueprint = LoadBlueprintAsset(PackageName);
    if (!Blueprint || !Blueprint->GeneratedClass || !Blueprint->GeneratedClass->IsChildOf(Types.AttributeSet))
    {
        return MakeGasError(
            TEXT("gas_attribute_set_not_found"),
            FString::Printf(TEXT("AttributeSet Blueprint not found or wrong parent type: %s"), *PackageName));
    }
    if (Blueprint->Status == BS_Error)
    {
        return MakeGasError(
            TEXT("gas_attribute_set_compile_error"),
            TEXT("AttributeSet Blueprint has compile errors; fix and compile it before adding an attribute"));
    }

    const FName AttributeFName(*AttributeName);
    FBPVariableDescription* ExistingVariable = nullptr;
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == AttributeFName)
        {
            ExistingVariable = &Variable;
            break;
        }
    }
    if (ExistingVariable &&
        ExistingVariable->VarType.PinCategory == UEdGraphSchema_K2::PC_Struct &&
        ExistingVariable->VarType.PinSubCategoryObject.Get() == Types.GameplayAttributeData)
    {
        TSharedPtr<FJsonObject> Result = MakeGasSuccess(
            TEXT("Gameplay attribute already exists with the expected UE 5.8 type"),
            false,
            false,
            false);
        Result->SetBoolField(TEXT("already_exists"), true);
        Result->SetStringField(TEXT("attribute_set_path"), PackageName);
        Result->SetStringField(TEXT("attribute_name"), AttributeName);
        Result->SetStringField(TEXT("attribute_struct"), Types.GameplayAttributeData->GetPathName());
        Result->SetBoolField(TEXT("saved"), false);
        return Result;
    }
    if (ExistingVariable)
    {
        return MakeGasError(
            TEXT("gas_attribute_name_conflict"),
            FString::Printf(
                TEXT("A different Blueprint variable already uses '%s'"),
                *AttributeName));
    }
    if (!ExistingVariable && FindFProperty<FProperty>(Blueprint->GeneratedClass, AttributeFName))
    {
        return MakeGasError(
            TEXT("gas_inherited_attribute_name_conflict"),
            FString::Printf(TEXT("An inherited property already uses attribute name '%s'"), *AttributeName));
    }

    FEdGraphPinType PinType;
    PinType.PinCategory = UEdGraphSchema_K2::PC_Struct;
    PinType.PinSubCategoryObject = Types.GameplayAttributeData;

    FString DependencyAuditError;
    if (!FGasCompatibility::ValidateMCPDependencyIndependence(
            TEXT("GAS AttributeSet Blueprint before mutation"),
            Blueprint,
            EDependencyAuditMode::InMemoryOnly,
            DependencyAuditError))
    {
        return MakeGasError(
            TEXT("mcp_dependency_reference_rejected"),
            DependencyAuditError);
    }

    if(!Work().SDK(TEXT("modify_blueprint"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    Blueprint->Modify();
    if(!Work().SDK(TEXT("add_member_variable"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    if (!FBlueprintEditorUtils::AddMemberVariable(Blueprint, AttributeFName, PinType))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_attribute_create_failed"),
            FString::Printf(TEXT("Failed to add gameplay attribute '%s'"), *AttributeName),
            TEXT("Inspect the AttributeSet Blueprint for a conflicting inherited property."));
    }

    if(!Work().Admit(TEXT("after_add_member_variable"))||!Work().SDK(TEXT("set_attribute_category_and_flags"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == AttributeFName)
        {
            Variable.Category = FText::FromString(Category);
            Variable.PropertyFlags |= static_cast<uint64>(CPF_Edit | CPF_BlueprintVisible);
            break;
        }
    }
    if(!Work().SDK(TEXT("set_blueprint_variable_category"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    FBlueprintEditorUtils::SetBlueprintVariableCategory(
        Blueprint,
        AttributeFName,
        nullptr,
        FText::FromString(Category),
        true);
    if(!Work().SDK(TEXT("mark_blueprint_structurally_modified"),true)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
    if(!Work().Compile(Blueprint)){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}

    FStructProperty* VerifiedProperty = Blueprint->GeneratedClass
        ? FindFProperty<FStructProperty>(Blueprint->GeneratedClass, AttributeFName)
        : nullptr;
    if (Blueprint->Status == BS_Error || !VerifiedProperty || VerifiedProperty->Struct != Types.GameplayAttributeData)
    {
        return MakeGasPartialMutationError(
            TEXT("gas_attribute_verification_failed"),
            TEXT("Gameplay attribute was added but Blueprint compile/type readback failed"),
            TEXT("Inspect or undo the partial AttributeSet Blueprint change before retrying."));
    }
    const TWeakObjectPtr<UClass> VerifiedAttributeClass=Blueprint->GeneratedClass;
    if (!FGasCompatibility::ValidateMCPDependencyIndependence(
            TEXT("GAS AttributeSet Blueprint after compile and before save"),
            Blueprint,
            EDependencyAuditMode::InMemoryOnly,
            DependencyAuditError))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_attribute_pre_save_dependency_audit_failed"),
            DependencyAuditError,
            TEXT("Undo or remove the in-memory MCP-only dependency before saving the AttributeSet Blueprint."));
    }
    if (!SaveBlueprintIfRequested(Blueprint, bSave))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_attribute_save_failed"),
            TEXT("Gameplay attribute was added but the AttributeSet Blueprint could not be saved"),
            TEXT("Resolve package write access, then save or revert the dirty Blueprint manually."));
    }
    if (bSave && !FGasCompatibility::ValidateMCPDependencyIndependence(
                     TEXT("GAS AttributeSet Blueprint after save"),
                     Blueprint,
                     EDependencyAuditMode::RequirePersisted,
                     DependencyAuditError))
    {
        return MakeGasPartialMutationError(
            TEXT("gas_attribute_post_save_dependency_audit_failed"),
            DependencyAuditError,
            TEXT("Restore or remove the persisted MCP-only dependency before using the AttributeSet without UnrealMCP."));
    }

    TSharedPtr<FJsonObject> Result = MakeGasSuccess(
        TEXT("Added and verified FGameplayAttributeData member"),
        true,
        true,
        true);
    Result->SetBoolField(TEXT("already_exists"), false);
    Result->SetBoolField(TEXT("saved"), bSave);
    Result->SetStringField(TEXT("attribute_set_path"), PackageName);
    Result->SetStringField(TEXT("attribute_name"), AttributeName);
    Result->SetStringField(TEXT("category"), Category);
    if(!Work().Admit(TEXT("final_attribute_readback"))){return MakeGasPartialMutationError(TEXT("gas_context_changed"),Work().R.Error,TEXT("Inspect actual Blueprint compile/structure/save prefix; no automatic replay."));}
    if(Blueprint->GeneratedClass!=VerifiedAttributeClass.Get()){return MakeGasPartialMutationError(TEXT("gas_attribute_post_save_verification_failed"),TEXT("GeneratedClass changed after audit/save"),TEXT("Inspect saved history/current class; no replay."));}
    FStructProperty* CurrentProperty=Blueprint->GeneratedClass?FindFProperty<FStructProperty>(Blueprint->GeneratedClass,AttributeFName):nullptr;
    if(!CurrentProperty||CurrentProperty->Struct!=Types.GameplayAttributeData){return MakeGasPartialMutationError(TEXT("gas_attribute_post_save_verification_failed"),TEXT("Current attribute type differs after audit/save"),TEXT("Inspect saved history/current property; no replay."));}
    Result->SetStringField(TEXT("attribute_struct"),CurrentProperty->Struct->GetPathName());
    Result->SetStringField(TEXT("verification_method"), TEXT("generated_class_struct_property_readback"));
    return Result;
}

TSharedPtr<FJsonObject> Unavailable(const FString& Command)
{
    auto J=MakeGasError(TEXT("dependency_plugin_disabled"),TEXT("GameplayAbilities is disabled or missing. Enable the engine plugin for this project and restart; no automatic enable/retry was performed."));J->SetStringField(TEXT("command"),Command);J->SetStringField(TEXT("domain"),TEXT("gas"));J->SetBoolField(TEXT("available"),false);J->SetBoolField(TEXT("provider_registered"),false);J->SetArrayField(TEXT("required_plugins"),{MakeShared<FJsonValueString>(TEXT("GameplayAbilities"))});J->SetArrayField(TEXT("disabled_plugins"),{MakeShared<FJsonValueString>(TEXT("GameplayAbilities"))});J->SetBoolField(TEXT("do_not_retry"),true);J->SetStringField(TEXT("recovery_guidance"),TEXT("Enable the listed engine plugin explicitly and restart the editor after preserving work."));return J;
}
using FHandler=TSharedPtr<FJsonObject>(*)(const TSharedPtr<FJsonObject>&);
FString Run(const TCHAR* Operation,const TCHAR* Command,const FString& ParamsJson,bool Writer,bool StructuralEdit,FHandler Handler)
{
    FWork W(Writer);FScope Scope(W);TSharedPtr<FJsonObject> Legacy,Params;
    if(Running){Legacy=MakeGasError(TEXT("gas_reentrant_operation"),TEXT("Another GAS native operation is active; no replay."));}
    else
    {
        TGuardValue<bool> Guard(Running,true);
        if(!W.Admit(TEXT("editor_admission"))){Legacy=MakeGasError(TEXT("gas_editor_context_unavailable"),W.R.Error);}
        else
        {
            auto Plugin=IPluginManager::Get().FindPlugin(TEXT("GameplayAbilities"));
            if(!Plugin.IsValid()||!Plugin->IsEnabled()){Legacy=Unavailable(Command);}
            else if(!Parse(ParamsJson,Params)){Legacy=MakeGasError(TEXT("gas_invalid_request"),TEXT("Bounded finite JSON object with depth64/unique exact keys required."));}
            else
            {
                TUniquePtr<FScopedTransaction> Transaction;const bool Joined=GEditor->IsTransactionActive();W.R.Data->SetBoolField(TEXT("joined_outer_transaction"),Joined);
                if(StructuralEdit&&!Joined&&W.SDK(TEXT("begin_structure_transaction"))){Transaction=MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless GAS Blueprint structure")));}
                if(W.R.Error.IsEmpty()){Legacy=Handler(Params);}
                // The scoped Editor transaction is not atomic rollback. Creation intentionally has no new transaction.
            }
        }
    }
    W.FinalProjection();if(!Legacy){Legacy=MakeGasError(TEXT("gas_native_result_missing"),W.R.Error.IsEmpty()?TEXT("Native operation did not return a result"):W.R.Error);}
    const bool LegacySuccess=Legacy->HasTypedField<EJson::Boolean>(TEXT("success"))&&Legacy->GetBoolField(TEXT("success"));
    W.R.bSuccess=LegacySuccess&&W.R.Error.IsEmpty();W.R.bPostconditionPassed=W.R.bSuccess;bool Changed=false;if(Legacy->TryGetBoolField(TEXT("changed"),Changed)){W.R.bChanged|=Changed;}
    W.R.Data->SetObjectField(TEXT("legacy_result"),Legacy);W.R.Data->SetBoolField(TEXT("operation_complete"),true);W.R.Data->SetBoolField(TEXT("read_only_direct_API"),!Writer);W.R.Data->SetBoolField(TEXT("SDK_global_purity_unverified"),!W.R.bChangeStateKnown);W.R.Data->SetBoolField(TEXT("explicit_notify_or_runtime_grants"),false);
    W.R.Data->SetStringField(TEXT("legacy_command"),Command);W.R.Data->SetBoolField(TEXT("saved_history_not_erased_by_cleanup"),true);W.R.Data->SetBoolField(TEXT("cleanup_complete"),W.Cleanup->HasTypedField<EJson::Boolean>(TEXT("complete"))&&W.Cleanup->GetBoolField(TEXT("complete")));
    if(W.R.bSuccess){W.R.Stage=TEXT("complete");}else{if(W.R.Error.IsEmpty()){FString Message;Legacy->TryGetStringField(TEXT("error"),Message);if(Message.IsEmpty()){Legacy->TryGetStringField(TEXT("message"),Message);}W.Fail(TEXT("gas_native_failed"),Message);}W.R.Stage=W.FailedStage;W.R.Data->SetStringField(TEXT("failed_stage"),W.FailedStage);}
    return Cubeless::Operations::EncodeVariableResult(W.R,Operation);
}
}
FString Inspect(const FString& P){return Run(TEXT("inspect_gas_blueprint"),TEXT("gas_inspect_blueprint"),P,false,false,&HandleInspectGASBlueprint);}
FString CreateAbility(const FString& P){return Run(TEXT("create_gameplay_ability_blueprint"),TEXT("gas_create_gameplay_ability_blueprint"),P,true,false,&HandleCreateGameplayAbilityBlueprint);}
FString CreateEffect(const FString& P){return Run(TEXT("create_gameplay_effect_blueprint"),TEXT("gas_create_gameplay_effect_blueprint"),P,true,false,&HandleCreateGameplayEffectBlueprint);}
FString CreateAttributeSet(const FString& P){return Run(TEXT("create_attribute_set_blueprint"),TEXT("gas_create_attribute_set_blueprint"),P,true,false,&HandleCreateAttributeSetBlueprint);}
FString AddAbilitySystemComponent(const FString& P){return Run(TEXT("add_ability_system_component_to_blueprint"),TEXT("gas_add_ability_system_component_to_blueprint"),P,true,true,&HandleAddAbilitySystemComponent);}
FString AddGameplayAttribute(const FString& P){return Run(TEXT("add_gameplay_attribute_to_attribute_set"),TEXT("gas_add_gameplay_attribute_to_attribute_set"),P,true,true,&HandleAddGameplayAttribute);}
}
