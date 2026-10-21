#include "Compatibility/UE58/CubelessAIDiscoveryApi.h"
#include "Containers/StringConv.h"
#include "Misc/EngineVersion.h"
#include "Misc/SecureHash.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Class.h"
#include "UObject/GarbageCollection.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectIterator.h"

namespace Cubeless::UE58::AIDiscovery
{
namespace
{
constexpr int32 MaxClassInventory = 32768;
constexpr int32 MaxPackageInventory = 16384;
constexpr int32 MaxSignatureBytes = 16 * 1024 * 1024;
constexpr const TCHAR* BasePaths[] = {
    TEXT("/Script/AIModule.BTCompositeNode"), TEXT("/Script/AIModule.BTTaskNode"),
    TEXT("/Script/AIModule.BTDecorator"), TEXT("/Script/AIModule.BTService")};
constexpr const TCHAR* Categories[] = {TEXT("composite"), TEXT("task"), TEXT("decorator"), TEXT("service")};

bool Fail(Function::FResult& Result, const TCHAR* Code, const FString& Message)
{
    Result.ErrorCode = Code; Result.Error = Message; return false;
}

TSharedRef<FJsonObject> Identity(const UObject* Object)
{
    auto Row = MakeShared<FJsonObject>();
    Row->SetBoolField(TEXT("present"), Object != nullptr);
    if (Object)
    {
        Row->SetStringField(TEXT("path"), Object->GetPathName());
        Row->SetStringField(TEXT("class"), Object->GetClass()->GetPathName());
        Row->SetStringField(TEXT("outer"), Object->GetOuter() ? Object->GetOuter()->GetPathName() : FString());
        Row->SetStringField(TEXT("object_flags"), LexToString(static_cast<uint64>(Object->GetFlags())));
    }
    return Row;
}

TSharedRef<FJsonObject> ClassRow(UClass* Class)
{
    auto Row = Identity(Class);
    Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint64>(Class->GetClassFlags())));
    Row->SetStringField(TEXT("super_class"), Class->GetSuperClass() ? Class->GetSuperClass()->GetPathName() : FString());
    Row->SetNumberField(TEXT("package_flags"), static_cast<uint32>(Class->GetOutermost()->GetPackageFlags()));
    Row->SetBoolField(TEXT("package_dirty"), Class->GetOutermost()->IsDirty());
    // false is essential: this observation never constructs/initializes a CDO.
    Row->SetObjectField(TEXT("existing_cdo"), Identity(Class->GetDefaultObject(false)));
    return Row;
}

FString Text(const TSharedRef<FJsonObject>& Object)
{
    FString Out; FJsonSerializer::Serialize(Object, TJsonWriterFactory<>::Create(&Out)); return Out;
}

bool Capture(TSharedRef<FJsonObject> Snapshot, const TArray<UClass*>& Bases)
{
    Snapshot->SetBoolField(TEXT("complete"), false);
    Snapshot->SetStringField(TEXT("scope"), TEXT("All live iterator UClass identities/flags/super/package flags/dirty/existing CDO identities, plus dirty package membership; no metadata/localization cache or world-state inventory"));
    TArray<FString> ClassRows;
    for (TObjectIterator<UClass> ClassIt; ClassIt; ++ClassIt)
    {
        UClass* CurrentClass = *ClassIt;
        if (!IsValid(CurrentClass) || !CurrentClass->GetOutermost())
        { Snapshot->SetStringField(TEXT("failed_stage"), TEXT("loaded_class_identity")); return false; }
        if (ClassRows.Num() >= MaxClassInventory)
        {
            Snapshot->SetNumberField(TEXT("class_count_observed"), ClassRows.Num());
            Snapshot->SetStringField(TEXT("failed_stage"), TEXT("loaded_class_inventory_bound")); return false;
        }
        UObject* ExistingCDO = CurrentClass->GetDefaultObject(false);
        if (ExistingCDO && !IsValid(ExistingCDO))
        {
            Snapshot->SetStringField(TEXT("failed_stage"), TEXT("existing_cdo_identity"));
            Snapshot->SetStringField(TEXT("failed_class"), CurrentClass->GetPathName()); return false;
        }
        ClassRows.Add(Text(ClassRow(CurrentClass)));
    }
    ClassRows.Sort();
    FString Signature;
    for (const FString& Row : ClassRows)
    {
        // Length prefix preserves unambiguous row boundaries without parsing
        // or normalizing any class/flag identity values.
        Signature += LexToString(Row.Len()) + TEXT(":") + Row;
        if (Signature.Len() > MaxSignatureBytes)
        { Snapshot->SetStringField(TEXT("failed_stage"), TEXT("class_signature_bound")); return false; }
    }
    FTCHARToUTF8 Utf8(*Signature);
    if (Utf8.Length() > MaxSignatureBytes)
    { Snapshot->SetStringField(TEXT("failed_stage"), TEXT("class_signature_utf8_bound")); return false; }
    FSHAHash Hash; FSHA1::HashBuffer(Utf8.Get(), Utf8.Length(), Hash.Hash);
    Snapshot->SetNumberField(TEXT("class_count"), ClassRows.Num());
    Snapshot->SetStringField(TEXT("loaded_class_signature_sha1"), Hash.ToString());
    TArray<FString> DirtyNames;
    int32 PackageCount = 0;
    for (TObjectIterator<UPackage> PackageIt; PackageIt; ++PackageIt)
    {
        UPackage* CurrentPackage = *PackageIt;
        if (!IsValid(CurrentPackage) || ++PackageCount > MaxPackageInventory)
        { Snapshot->SetStringField(TEXT("failed_stage"), TEXT("package_inventory")); return false; }
        if (CurrentPackage->IsDirty()) { DirtyNames.Add(CurrentPackage->GetPathName()); }
    }
    DirtyNames.Sort();
    TArray<TSharedPtr<FJsonValue>> DirtyRows;
    for (const FString& Name : DirtyNames) { DirtyRows.Add(MakeShared<FJsonValueString>(Name)); }
    Snapshot->SetArrayField(TEXT("dirty_packages"), DirtyRows);
    Snapshot->SetNumberField(TEXT("package_count"), PackageCount);
    TArray<TSharedPtr<FJsonValue>> BaseRows;
    for (UClass* BaseClass : Bases) { BaseRows.Add(MakeShared<FJsonValueObject>(ClassRow(BaseClass))); }
    Snapshot->SetArrayField(TEXT("native_bases"), BaseRows);
    Snapshot->SetBoolField(TEXT("complete"), true); return true;
}

const TCHAR* NodeCategory(const UClass* Class, const TArray<UClass*>& Bases)
{
    // Preserve the original classifier priority and exclude each exact base.
    for (int32 Index = 0; Index < Bases.Num(); ++Index)
    {
        if (Class != Bases[Index] && Class->IsChildOf(Bases[Index])) { return Categories[Index]; }
    }
    return nullptr;
}

struct FMatch
{
    FString Category, Name, Path, Display;
    TSharedRef<FJsonObject> Row = MakeShared<FJsonObject>();
};
}

Function::FResult SearchBehaviorTreeNodeClasses(const FString& Category, const FString& Query,
    bool bIncludeAbstract, int32 MaxResults)
{
    Function::FResult Result;
    Result.Data->SetStringField(TEXT("operation"), TEXT("search_ai_behavior_tree_node_classes"));
    Result.Data->SetBoolField(TEXT("mutation_attempted"), false);
    Result.Data->SetBoolField(TEXT("mutation_performed"), false);
    Result.Data->SetBoolField(TEXT("mutation_state_unknown"), false);
    Result.Data->SetBoolField(TEXT("changed"), false);
    Result.Data->SetBoolField(TEXT("readback_confirmed"), false);
    Result.Data->SetBoolField(TEXT("observations_complete"), false);
    Result.Data->SetBoolField(TEXT("state_preserved"), false);
    Result.Data->SetNumberField(TEXT("explicit_asset_load_calls"), 0);
    Result.Data->SetNumberField(TEXT("explicit_cdo_creation_calls"), 0);
    Result.Data->SetBoolField(TEXT("metadata_and_localization_cache_effects_instrumented"), false);
    Result.Data->SetStringField(TEXT("engine_target"), TEXT("Unreal Engine ") + FEngineVersion::Current().ToString());
    if (!IsInGameThread() || IsGarbageCollecting())
    { Fail(Result, TEXT("ai_discovery_context_unavailable"), TEXT("Loaded-class discovery requires the Game Thread outside garbage collection")); return Result; }
    if (Category.Len() > 32 || Query.Len() > 1024)
    { Fail(Result, TEXT("ai_discovery_input_bound"), TEXT("Initial finite discovery admits category at most32 and query at most1024 characters")); return Result; }
    const FString SelectedCategory = Category.ToLower();
    const FString SelectedQuery = Query.ToLower();
    Result.Data->SetStringField(TEXT("category"), SelectedCategory);
    Result.Data->SetStringField(TEXT("query"), SelectedQuery);
    Result.Data->SetNumberField(TEXT("max_results_requested"), MaxResults);
    const int32 Limit = FMath::Clamp(MaxResults, 1, 500);
    Result.Data->SetNumberField(TEXT("max_results_effective"), Limit);
    if (SelectedCategory != TEXT("all") && SelectedCategory != TEXT("composite") && SelectedCategory != TEXT("task")
        && SelectedCategory != TEXT("decorator") && SelectedCategory != TEXT("service"))
    {
        Fail(Result, TEXT("ai_discovery_category_invalid"), FString::Printf(
            TEXT("Unsupported category '%s'. Use all, composite, task, decorator, or service."), *SelectedCategory)); return Result;
    }
    TArray<UClass*> Bases;
    TArray<TStrongObjectPtr<UClass>> KeepClasses;
    for (const TCHAR* BasePath : BasePaths)
    {
        UClass* BaseClass = FindObject<UClass>(nullptr, BasePath);
        if (!IsValid(BaseClass) || BaseClass->GetPathName() != BasePath || !BaseClass->HasAnyClassFlags(CLASS_Native)
            || BaseClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)
            || BaseClass->GetOutermost()->GetName() != TEXT("/Script/AIModule"))
        {
            Result.Data->SetStringField(TEXT("failed_base_path"), BasePath);
            Fail(Result, TEXT("ai_discovery_native_bases_unavailable"), TEXT("The four exact native AIModule bases must already be loaded; no warm/load fallback")); return Result;
        }
        Bases.Add(BaseClass); KeepClasses.Emplace(BaseClass);
    }
    Result.Stage = TEXT("observe_before");
    auto Before = MakeShared<FJsonObject>(); Result.Data->SetObjectField(TEXT("before"), Before);
    if (!Capture(Before, Bases))
    { Fail(Result, TEXT("ai_discovery_before_unobservable"), TEXT("Initial loaded-class/package inventory is incomplete; no search output or load was attempted")); return Result; }
    Result.Stage = TEXT("enumerate_loaded");
    TArray<FMatch> Matches;
    int32 Scanned = 0;
    for (TObjectIterator<UClass> NodeIt; NodeIt; ++NodeIt)
    {
        UClass* NodeClass = *NodeIt;
        if (++Scanned > MaxClassInventory || !IsValid(NodeClass))
        {
            Result.bChangeStateKnown = false; Result.Data->SetBoolField(TEXT("mutation_state_unknown"), true);
            Result.Data->SetNumberField(TEXT("scanned_class_count"), Scanned);
            TArray<TSharedPtr<FJsonValue>> PrefixRows;
            for (int32 PrefixIndex = 0; PrefixIndex < FMath::Min(Matches.Num(), Limit); ++PrefixIndex)
            { PrefixRows.Add(MakeShared<FJsonValueObject>(Matches[PrefixIndex].Row)); }
            Result.Data->SetNumberField(TEXT("matching_prefix_count"), Matches.Num());
            Result.Data->SetNumberField(TEXT("matching_prefix_returned_count"), PrefixRows.Num());
            Result.Data->SetBoolField(TEXT("matching_prefix_truncated"), Matches.Num() > PrefixRows.Num());
            Result.Data->SetBoolField(TEXT("matching_prefix_complete"), false);
            Result.Data->SetStringField(TEXT("matching_prefix_scope"), TEXT("Actual matching rows before the failed iterator entry, in unsorted loaded-iterator order; full total_matched is unknown"));
            Result.Data->SetArrayField(TEXT("matching_prefix"), PrefixRows);
            Fail(Result, TEXT("ai_discovery_search_unobservable"), TEXT("Loaded-class search inventory became incomplete")); return Result;
        }
        if (NodeClass->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists)) { continue; }
        const TCHAR* ActualCategory = NodeCategory(NodeClass, Bases);
        if (!ActualCategory || (SelectedCategory != TEXT("all") && SelectedCategory != ActualCategory)) { continue; }
        const FString Name = NodeClass->GetName();
        if (Name.StartsWith(TEXT("SKEL_")) || Name.StartsWith(TEXT("REINST_")) || Name.StartsWith(TEXT("TRASHCLASS_"))) { continue; }
        const bool bAbstract = NodeClass->HasAnyClassFlags(CLASS_Abstract);
        if (!bIncludeAbstract && bAbstract) { continue; }
        const FString Path = NodeClass->GetPathName();
        const FString Display = NodeClass->GetDisplayNameText().ToString();
        if (!SelectedQuery.IsEmpty() && !Name.ToLower().Contains(SelectedQuery)
            && !Path.ToLower().Contains(SelectedQuery) && !Display.ToLower().Contains(SelectedQuery)) { continue; }
        KeepClasses.Emplace(NodeClass);
        FMatch Match; Match.Category = ActualCategory; Match.Name = Name; Match.Path = Path; Match.Display = Display;
        Match.Row->SetStringField(TEXT("category"), Match.Category);
        Match.Row->SetStringField(TEXT("class_name"), Name);
        Match.Row->SetStringField(TEXT("class_path"), Path);
        Match.Row->SetStringField(TEXT("display_name"), Display);
        Match.Row->SetBoolField(TEXT("abstract"), bAbstract);
        Match.Row->SetBoolField(TEXT("native"), NodeClass->HasAnyClassFlags(CLASS_Native));
        Match.Row->SetBoolField(TEXT("blueprint_generated"), NodeClass->HasAnyClassFlags(CLASS_CompiledFromBlueprint));
        Matches.Add(MoveTemp(Match));
    }
    Matches.Sort([](const FMatch& Left, const FMatch& Right)
    {
        const int32 CategoryCompare = Left.Category.Compare(Right.Category, ESearchCase::IgnoreCase);
        return CategoryCompare == 0 ? Left.Name.Compare(Right.Name, ESearchCase::IgnoreCase) < 0 : CategoryCompare < 0;
    });
    TArray<TSharedPtr<FJsonValue>> Values;
    for (int32 Index = 0; Index < FMath::Min(Matches.Num(), Limit); ++Index)
    { Values.Add(MakeShared<FJsonValueObject>(Matches[Index].Row)); }
    Result.Data->SetNumberField(TEXT("scanned_class_count"), Scanned);
    Result.Data->SetNumberField(TEXT("total_matched"), Matches.Num());
    Result.Data->SetNumberField(TEXT("returned_count"), Values.Num());
    Result.Data->SetBoolField(TEXT("truncated"), Matches.Num() > Values.Num());
    Result.Data->SetArrayField(TEXT("classes"), Values);
    Result.Stage = TEXT("observe_after");
    auto After = MakeShared<FJsonObject>(); Result.Data->SetObjectField(TEXT("after"), After);
    if (!Capture(After, Bases))
    {
        Result.bChangeStateKnown = false; Result.Data->SetBoolField(TEXT("mutation_state_unknown"), true);
        Fail(Result, TEXT("ai_discovery_after_unobservable"), TEXT("Returned classes/counts are retained but final loaded-state preservation is unobservable")); return Result;
    }
    Result.Data->SetBoolField(TEXT("observations_complete"), true);
    const bool bPreserved = Text(Before) == Text(After);
    Result.Data->SetBoolField(TEXT("state_preserved"), bPreserved);
    if (!bPreserved)
    {
        Result.bChangeStateKnown = false; Result.Data->SetBoolField(TEXT("mutation_state_unknown"), true);
        Fail(Result, TEXT("ai_discovery_state_preservation_failed"), TEXT("Observed loaded class/CDO/dirty package scope changed; no write/compile/save/retry was dispatched")); return Result;
    }
    auto Verification = MakeShared<FJsonObject>();
    Verification->SetStringField(TEXT("status"), TEXT("passed"));
    Verification->SetStringField(TEXT("method"), TEXT("ue_5_8_native_loaded_class_scoped_readback"));
    Result.Data->SetObjectField(TEXT("verification"), Verification);
    Result.Data->SetBoolField(TEXT("readback_confirmed"), true);
    Result.bSuccess = Result.bPostconditionPassed = true; Result.Stage = TEXT("complete"); return Result;
}
}
