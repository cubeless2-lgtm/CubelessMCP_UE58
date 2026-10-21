// Internal UE58 engine primitives shared by the focused StateTree APIs.
#include "Compatibility/UE58/CubelessStateTreeWork.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Dom/JsonValue.h"
#include "HAL/FileManager.h"
#include "Misc/PackageName.h"
#include "StateTree.h"
#include "StateTreeEditorData.h"
#include "StateTreeSchema.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"
namespace Cubeless::UE58::StateTreeWork
{
using Variable::FResult;
bool Loaded(const UObject* Object)
{
    return IsValid(Object) && !Object->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects
        | RF_BeginDestroyed | RF_FinishDestroyed);
}

void Count(FResult& Result, const TCHAR* Key, int32 Amount)
{
    const auto Counts = Result.Data->GetObjectField(TEXT("native_call_counts"));
    Counts->SetNumberField(Key, Counts->GetNumberField(Key) + Amount);
}

bool Forbidden(const FString& Value)
{
    for (const TCHAR* Token : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools") })
    { if (Value.Contains(Token, ESearchCase::IgnoreCase)) { return true; } }
    const FString Package = FPackageName::ObjectPathToPackageName(Value);
    for (const TCHAR* Plugin : { TEXT("CubelessMCP_UE58"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    { if (Value.StartsWith(FString::Printf(TEXT("/%s/"), Plugin), ESearchCase::IgnoreCase) || Value.Equals(FString::Printf(TEXT("/%s"), Plugin), ESearchCase::IgnoreCase)) { return true; } }
    for (const TCHAR* Module : { TEXT("CubelessMCPUE58Editor"), TEXT("ModelContextProtocol"), TEXT("ToolsetRegistry"), TEXT("PythonScriptPlugin"), TEXT("EditorScriptingUtilities") })
    { if (Package.Equals(FString::Printf(TEXT("/Script/%s"), Module), ESearchCase::IgnoreCase)) { return true; } }
    return false;
}

bool SafeObjectToken(const UObject* Object)
{
    if (!Object) { return true; }
    if (!Loaded(Object) || Forbidden(Object->GetPathName())) { return false; }
    int32 Depth = 0;
    for (const UClass* Class = Object->GetClass(); Class; Class = Class->GetSuperClass())
    { if (++Depth > 128 || Forbidden(Class->GetPathName())) { return false; } }
    if (const UClass* ClassObject = Cast<UClass>(Object))
    {
        Depth = 0;
        for (const UClass* Class = ClassObject; Class; Class = Class->GetSuperClass())
        { if (++Depth > 128 || Forbidden(Class->GetPathName())) { return false; } }
    }
    return true;
}

bool AuditLive(UStateTree* Tree, FResult& Result, const TCHAR* Label)
{
    Count(Result, TEXT("live_dependency_audit_calls"));
    auto Audit = MakeShared<FJsonObject>(); Result.Data->SetObjectField(Label, Audit);
    Audit->SetBoolField(TEXT("passed"), false);
    Audit->SetStringField(TEXT("scope"), TEXT("Bounded package exports, serialized hard/class/soft/top-level/weak/lazy/delegate references and external saved registry dependencies; not package execution."));
    if (!Loaded(Tree)) { Audit->SetStringField(TEXT("error"), TEXT("Target is unreadable.")); return false; }
    const FString RootPackage = Tree->GetOutermost()->GetName();
    TArray<UObject*> Exports;
    GetObjectsWithPackage(Tree->GetOutermost(), Exports, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
    Audit->SetNumberField(TEXT("export_count"), Exports.Num());
    if (Exports.Num() > 16384) { Audit->SetStringField(TEXT("error"), TEXT("Loaded exports exceed16384.")); return false; }
    TArray<FString> External; int32 References = 0, Values = 0;
    TArray<TSharedPtr<FJsonValue>> Rows;
    const auto Token = [&](const FString& Path)
    {
        if (Forbidden(Path)) { Audit->SetStringField(TEXT("forbidden_reference"), Path); return false; }
        const FString Package = FPackageName::ObjectPathToPackageName(Path);
        if (Package != RootPackage && Package.StartsWith(TEXT("/")) && !Package.StartsWith(TEXT("/Script/"))
            && !Package.StartsWith(TEXT("/Engine/")) && !FPackageName::IsMemoryPackage(Package) && !FPackageName::IsTempPackage(Package))
        { External.AddUnique(Package); }
        return true;
    };
    for (UObject* Export : Exports)
    {
        if (!SafeObjectToken(Export) || !Token(Export->GetPathName())) { Audit->SetStringField(TEXT("error"), TEXT("Unreadable or forbidden export/class.")); return false; }
        TArray<UObject*> Hard;
        FReferenceFinder Finder(Hard, nullptr, false, true, false, true); Finder.FindReferences(Export);
        if (Hard.Num() > 16384 - References) { Audit->SetStringField(TEXT("error"), TEXT("Serialized references exceed16384.")); return false; }
        References += Hard.Num(); Audit->SetNumberField(TEXT("references_visited"), References);
        for (const UObject* Reference : Hard)
        {
            if (!Reference) { continue; }
            if (!SafeObjectToken(Reference) || !Token(Reference->GetPathName())) { Audit->SetStringField(TEXT("error"), TEXT("Unreadable or forbidden hard/class reference.")); return false; }
        }
        for (TPropertyValueIterator<const FProperty> It(Export->GetClass(), Export, EPropertyValueIteratorFlags::FullRecursion, EFieldIteratorFlags::IncludeDeprecated); It; ++It)
        {
            if (++Values > 65536) { Audit->SetStringField(TEXT("error"), TEXT("Serialized property values exceed65536.")); return false; }
            Audit->SetNumberField(TEXT("property_values_visited"), Values);
            const FProperty* Property = It.Key(); const void* Value = It.Value();
            if (!Property || !Value) { continue; }
            if (Property->HasAnyPropertyFlags(CPF_Transient | CPF_DuplicateTransient | CPF_NonPIEDuplicateTransient | CPF_SkipSerialization)) { It.SkipRecursiveProperty(); continue; }
            FString SoftPath; TArray<const UObject*> Objects; bool bReference = false;
            if (const FSoftObjectProperty* Soft = CastField<FSoftObjectProperty>(Property))
            {
                const FSoftObjectPtr Stored = Soft->GetPropertyValue(Value);
                SoftPath = Stored.ToSoftObjectPath().ToString(); Objects.Add(Stored.Get()); bReference = true;
            }
            else if (const FStructProperty* Struct = CastField<FStructProperty>(Property))
            {
                const FString StructPath = Struct->Struct ? Struct->Struct->GetPathName() : FString();
                if (StructPath == TEXT("/Script/CoreUObject.SoftObjectPath") || StructPath == TEXT("/Script/CoreUObject.SoftClassPath"))
                {
                    if (Struct->Struct->GetStructureSize() != sizeof(FSoftObjectPath)) { Audit->SetStringField(TEXT("error"), TEXT("Unexpected soft-path layout.")); return false; }
                    const FSoftObjectPath* Stored = static_cast<const FSoftObjectPath*>(Value);
                    SoftPath = Stored->ToString(); Objects.Add(Stored->ResolveObject()); It.SkipRecursiveProperty(); bReference = true;
                }
                else if (StructPath == TEXT("/Script/CoreUObject.TopLevelAssetPath"))
                {
                    if (Struct->Struct->GetStructureSize() != sizeof(FTopLevelAssetPath)) { Audit->SetStringField(TEXT("error"), TEXT("Unexpected top-level path layout.")); return false; }
                    SoftPath = static_cast<const FTopLevelAssetPath*>(Value)->ToString(); It.SkipRecursiveProperty(); bReference = true;
                }
            }
            else if (const FWeakObjectProperty* Weak = CastField<FWeakObjectProperty>(Property)) { Objects.Add(Weak->GetObjectPropertyValue(Value)); bReference = true; }
            else if (const FLazyObjectProperty* Lazy = CastField<FLazyObjectProperty>(Property)) { Objects.Add(Lazy->GetObjectPropertyValue(Value)); bReference = true; }
            else if (CastField<FDelegateProperty>(Property)) { Objects.Add(static_cast<const FScriptDelegate*>(Value)->GetUObject()); bReference = true; }
            else if (const FMulticastDelegateProperty* Multi = CastField<FMulticastDelegateProperty>(Property))
            {
                bReference = true;
                if (const FMulticastScriptDelegate* Delegates = Multi->GetMulticastDelegate(Value))
                { for (const UObject* Object : Delegates->GetAllObjectsEvenIfUnreachable()) { Objects.Add(Object); } }
            }
            if (!bReference) { continue; }
            if (++References > 16384 || Objects.Num() > 16384 - References) { Audit->SetStringField(TEXT("error"), TEXT("Serialized property references exceed16384.")); return false; }
            References += Objects.Num(); Audit->SetNumberField(TEXT("references_visited"), References);
            auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("owner"), Export->GetPathName()); Row->SetStringField(TEXT("property"), Property->GetName()); Row->SetStringField(TEXT("soft_or_top_level_path"), SoftPath);
            Rows.Add(MakeShared<FJsonValueObject>(Row)); Audit->SetArrayField(TEXT("serialized_path_rows"), Rows);
            if (!Token(SoftPath)) { return false; }
            for (const UObject* Object : Objects)
            { if (!SafeObjectToken(Object) || (Object && !Token(Object->GetPathName()))) { Audit->SetStringField(TEXT("error"), TEXT("Unreadable or forbidden weak/lazy/delegate reference.")); return false; } }
        }
    }
    if (!External.IsEmpty())
    {
        const auto Registry = ReadPackageDependencies(External, true, 4096); Audit->SetObjectField(TEXT("external_saved_registry"), Registry);
        bool bPassed = false;
        if (!Registry->TryGetBoolField(TEXT("passed"), bPassed) || !bPassed) { Audit->SetStringField(TEXT("error"), TEXT("External saved dependencies are incomplete or forbidden.")); return false; }
    }
    Audit->SetBoolField(TEXT("passed"), true); return true;
}

bool AuditPersisted(UStateTree* Tree, FResult& Result)
{
    Count(Result, TEXT("persisted_dependency_audit_calls"));
    FString Filename;
    if (!Loaded(Tree) || !FPackageName::DoesPackageExist(Tree->GetOutermost()->GetName(), &Filename) || IFileManager::Get().FileSize(*Filename) <= 0)
    { Result.Data->SetStringField(TEXT("persisted_dependency_audit_error"), TEXT("A saved nonempty target package is required.")); return false; }
    Result.Data->SetStringField(TEXT("saved_package_filename"), Filename);
    Result.Data->SetNumberField(TEXT("saved_package_bytes"), static_cast<double>(IFileManager::Get().FileSize(*Filename)));
    Count(Result, TEXT("registry_rescan_calls"));
    FAssetRegistryModule::GetRegistry().ScanModifiedAssetFiles({Filename});
    const auto Audit = ReadPackageDependencies({ Tree->GetOutermost()->GetName() }, true, 4096);
    Result.Data->SetObjectField(TEXT("persisted_dependency_audit"), Audit);
    bool bPassed = false;
    return Audit->TryGetBoolField(TEXT("passed"), bPassed) && bPassed && AuditLive(Tree, Result, TEXT("post_save_live_dependency_audit"));
}

bool Identity(UStateTree* Tree, const FString& ObjectPath, UClass* SchemaClass)
{
    if (!Loaded(Tree) || !Tree->GetPathName().Equals(ObjectPath, ESearchCase::IgnoreCase) || Tree->GetOuter() != Tree->GetOutermost()) { return false; }
    const UStateTreeEditorData* EditorData = Cast<UStateTreeEditorData>(Tree->EditorData.Get());
    return Loaded(EditorData) && Loaded(EditorData->Schema.Get()) && EditorData->Schema->GetClass() == SchemaClass;
}

bool ReadReady(UStateTree* Tree, FResult& Result, const TCHAR* Label)
{
    auto Receipt = MakeShared<FJsonObject>(); Result.Data->SetObjectField(Label, Receipt);
    if (!Loaded(Tree)) { Result.bChangeStateKnown = false; Receipt->SetBoolField(TEXT("readable"), false); return false; }
    const uint32 BeforeHash = Tree->LastCompiledEditorDataHash; const bool bBeforeDirty = Tree->GetOutermost()->IsDirty();
    Receipt->SetNumberField(TEXT("compiled_hash_before"), BeforeHash); Receipt->SetBoolField(TEXT("dirty_before"), bBeforeDirty);
    Count(Result, TEXT("ready_to_run_calls")); Result.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), true);
    const bool bReady = Tree->IsReadyToRun(); Receipt->SetBoolField(TEXT("ready_to_run"), bReady);
    if (!Loaded(Tree)) { Result.bChangeStateKnown = false; Receipt->SetBoolField(TEXT("readable"), false); return false; }
    const uint32 AfterHash = Tree->LastCompiledEditorDataHash; const bool bAfterDirty = Tree->GetOutermost()->IsDirty();
    Receipt->SetBoolField(TEXT("readable"), true); Receipt->SetNumberField(TEXT("compiled_hash_after"), AfterHash); Receipt->SetBoolField(TEXT("dirty_after"), bAfterDirty);
    const bool bObserved = BeforeHash != AfterHash || bBeforeDirty != bAfterDirty;
    Receipt->SetBoolField(TEXT("observed_change"), bObserved);
    if (bObserved) { Result.bChanged = true; Result.bChangeStateKnown = true; }
    Result.Data->SetBoolField(TEXT("ready_to_run"), bReady); Result.Data->SetNumberField(TEXT("compiled_editor_data_hash"), AfterHash);
    return bReady;
}
}
