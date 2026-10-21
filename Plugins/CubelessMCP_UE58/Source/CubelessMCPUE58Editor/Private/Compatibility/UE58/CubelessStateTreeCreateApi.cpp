// Original focused StateTree creator; no server/Toolset transport in the working logic.
#include "Compatibility/UE58/CubelessStateTreeCreateApi.h"
#include "Compatibility/UE58/CubelessStateTreeWork.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "AssetToolsModule.h"
#include "Components/StateTreeAIComponentSchema.h"
#include "Components/StateTreeComponentSchema.h"
#include "Dom/JsonValue.h"
#include "Editor.h"
#include "EditorAssetLibrary.h"
#include "HAL/FileManager.h"
#include "IAssetTools.h"
#include "Logging/TokenizedMessage.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Modules/ModuleManager.h"
#include "ObjectTools.h"
#include "StateTree.h"
#include "StateTreeCompilerLog.h"
#include "StateTreeEditingSubsystem.h"
#include "StateTreeEditorData.h"
#include "StateTreeFactory.h"
#include "StateTreeSchema.h"
#include "UObject/Package.h"
#include "UObject/SoftObjectPath.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/TopLevelAssetPath.h"
#include "UObject/UObjectGlobals.h"
#include "UObject/UObjectHash.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::StateTreeCreate
{
namespace
{
using Variable::FResult;
using StateTreeWork::Loaded;
using StateTreeWork::Count;
using StateTreeWork::Forbidden;
using StateTreeWork::AuditLive;
using StateTreeWork::AuditPersisted;
using StateTreeWork::Identity;
using StateTreeWork::ReadReady;
void Init(FResult& Result)
{
    auto Counts = MakeShared<FJsonObject>();
    for (const TCHAR* Key : { TEXT("asset_exists_calls"), TEXT("asset_load_calls"), TEXT("asset_load_returns"),
        TEXT("factory_allocation_calls"), TEXT("factory_schema_set_calls"), TEXT("asset_create_calls"), TEXT("asset_create_returns"),
        TEXT("explicit_compile_calls"), TEXT("explicit_compile_returns"), TEXT("ready_to_run_calls"),
        TEXT("explicit_mark_dirty_calls"), TEXT("explicit_save_calls"), TEXT("explicit_save_returns"),
        TEXT("live_dependency_audit_calls"), TEXT("persisted_dependency_audit_calls"), TEXT("registry_rescan_calls"),
        TEXT("cleanup_preflight_calls"), TEXT("cleanup_delete_calls"), TEXT("cleanup_file_delete_calls"),
        TEXT("cleanup_registry_package_deleted_calls"), TEXT("runtime_execution_calls") }) { Counts->SetNumberField(Key, 0); }
    Result.Data->SetObjectField(TEXT("native_call_counts"), Counts);
    Result.Data->SetStringField(TEXT("operation"), TEXT("create_state_tree"));
    Result.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), false);
    Result.Data->SetBoolField(TEXT("factory_internal_compile_possible"), false);
    Result.Data->SetBoolField(TEXT("total_SDK_callback_effects_verified"), false);
    Result.Data->SetBoolField(TEXT("runtime_execution_verified"), false);
    Result.Data->SetBoolField(TEXT("cold_reload_verified"), false);
    Result.Data->SetBoolField(TEXT("packaging_verified"), false);
    Result.Data->SetBoolField(TEXT("atomic_rollback_available"), false);
    Result.Data->SetBoolField(TEXT("auto_retry_safe"), false);
}
FResult Fail(FResult Result, const FString& Message, const TCHAR* Code = TEXT("state_tree_command_failed"))
{
    Result.bSuccess = false; Result.bPostconditionPassed = false;
    Result.ErrorCode = Code; Result.Error = Message;
    Result.Data->SetStringField(TEXT("message"), Message);
    Result.Data->SetBoolField(TEXT("mutation_state_unknown"), !Result.bChangeStateKnown);
    Result.Data->SetBoolField(TEXT("do_not_retry"), Result.bMutationAttempted || !Result.bChangeStateKnown);
    Result.Data->SetBoolField(TEXT("partial_mutation"), Result.bChanged || !Result.bChangeStateKnown);
    Result.Data->SetBoolField(TEXT("workflow_recovery_required"), Result.bChanged || !Result.bChangeStateKnown);
    if (Result.bMutationAttempted || !Result.bChangeStateKnown)
    { Result.Data->SetStringField(TEXT("recovery_guidance"), TEXT("Inspect the actual retained asset/package/compiler/save/cleanup prefix before a new request. No automatic replay or atomic rollback is provided.")); }
    return Result;
}
bool Cleanup(UStateTree* Tree, const FString& ObjectPath, const FString& ExpectedPackage, FResult& Result)
{
    Count(Result, TEXT("cleanup_preflight_calls")); auto Receipt = MakeShared<FJsonObject>(); Result.Data->SetObjectField(TEXT("cleanup_receipt"), Receipt);
    Receipt->SetBoolField(TEXT("succeeded"), false);
    Receipt->SetBoolField(TEXT("SDK_delete_veto_dialog_possible"), true);
    Receipt->SetStringField(TEXT("SDK_delete_UI_scope"), TEXT("Public ObjectTools::DeleteSingleObject checks deletion delegates again and may display a veto dialog; cleanup is conditional and is not an atomic rollback."));
    if (!Loaded(Tree) || !Loaded(Tree->GetOutermost()) || Tree->GetPathName() != ObjectPath || Tree->HasAnyFlags(RF_WasLoaded) || Tree->IsRooted()
        || Tree->GetOuter() != Tree->GetOutermost() || Tree->GetOutermost()->GetName() != ExpectedPackage
        || !ExpectedPackage.StartsWith(TEXT("/Game/")) || !Tree->GetOutermost()->GetExternalPackages().IsEmpty())
    { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup requires the exact caller-confirmed new top-level non-rooted /Game asset without external packages.")); return false; }
    UPackage* Package = Tree->GetOutermost();
    TArray<UObject*> Objects; GetObjectsWithPackage(Package, Objects, EGetObjectsFlags::IncludeNestedObjects, RF_Transient, EInternalObjectFlags::Garbage);
    if (Objects.Num() > 16384) { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup package exports exceed the bound.")); return false; }
    for (const UObject* Object : Objects)
    { if (Object && Object != Tree && Object->IsAsset()) { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup refuses another asset in the same package.")); return false; } }
    FString Filename;
    if (!FPackageName::DoesPackageExist(ExpectedPackage, &Filename)
        && !FPackageName::TryConvertLongPackageNameToFilename(ExpectedPackage, Filename, FPackageName::GetAssetPackageExtension()))
    { Receipt->SetStringField(TEXT("error"), TEXT("Exact package filename could not be resolved.")); return false; }
    Filename = FPaths::ConvertRelativePathToFull(Filename); FPaths::NormalizeFilename(Filename);
    FString ContentDir = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir()); FPaths::NormalizeDirectoryName(ContentDir);
    if (!FPaths::IsUnderDirectory(Filename, ContentDir) || (FPaths::FileExists(Filename) && IFileManager::Get().IsReadOnly(*Filename)))
    { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup file is outside project Content or is read-only.")); return false; }
    Receipt->SetStringField(TEXT("exact_package_filename"), Filename);
    bool bReferenced = false, bUndo = false; ObjectTools::GatherObjectReferencersForDeletion(Tree, bReferenced, bUndo);
    Receipt->SetBoolField(TEXT("live_referenced"), bReferenced); Receipt->SetBoolField(TEXT("undo_referenced"), bUndo);
    if (bReferenced || bUndo) { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup refuses live/Undo references and never clears the Undo buffer.")); return false; }
    if (!Loaded(Tree) || !Loaded(Package) || Tree->GetPathName() != ObjectPath || Tree->GetOuter() != Package
        || Tree->GetOutermost() != Package || Package->GetName() != ExpectedPackage)
    { Receipt->SetStringField(TEXT("error"), TEXT("Cleanup target/package lifetime or identity changed during referencer inspection.")); return false; }
    // Reattach only after referencer/Undo admission, so our guards are not treated as foreign references.
    const TStrongObjectPtr<UStateTree> DeleteHeld(Tree);
    const TStrongObjectPtr<UPackage> PackageHeld(Package);
    Receipt->SetBoolField(TEXT("lifetime_guards_attached_after_referencer_check"), true);
    FCanDeleteAssetResult CanDelete; FEditorDelegates::OnAssetsCanDelete.Broadcast({Tree}, CanDelete);
    if (!CanDelete.Get() || !Loaded(Tree) || !Loaded(Package) || Tree->GetPathName() != ObjectPath
        || Tree->GetOuter() != Package || Tree->GetOutermost() != Package || Package->GetName() != ExpectedPackage)
    { Receipt->SetStringField(TEXT("error"), TEXT("Deletion veto or target/package lifetime/identity changed after callback.")); return false; }
    Count(Result, TEXT("cleanup_delete_calls")); const bool bDeleted = ObjectTools::DeleteSingleObject(Tree, false);
    Receipt->SetBoolField(TEXT("object_delete_returned"), bDeleted);
    if (!bDeleted) { Receipt->SetStringField(TEXT("error"), TEXT("Preflighted object deletion refused.")); return false; }
    if (!Loaded(Tree) || !Loaded(Package) || Tree->GetPathName() != ObjectPath || Tree->GetOuter() != Package
        || Tree->GetOutermost() != Package || Package->GetName() != ExpectedPackage)
    { Receipt->SetStringField(TEXT("error"), TEXT("Object deletion returned true but current target/package lifetime or identity is unobservable; no further file deletion was dispatched.")); return false; }
    Tree->MarkAsGarbage();
    if (FPaths::FileExists(Filename))
    {
        Count(Result, TEXT("cleanup_file_delete_calls")); const bool bFileDeleted = IFileManager::Get().Delete(*Filename, true, false, true);
        Receipt->SetBoolField(TEXT("file_delete_returned"), bFileDeleted);
        if (!bFileDeleted) { Receipt->SetStringField(TEXT("error"), TEXT("Object was removed but exact package file could not be deleted.")); return false; }
    }
    Package->SetDirtyFlag(false); Count(Result, TEXT("cleanup_registry_package_deleted_calls")); FAssetRegistryModule::PackageDeleted(Package);
    const UObject* Remaining = FindObject<UObject>(nullptr, *ObjectPath);
    const bool bRegistered = FAssetRegistryModule::GetRegistry().GetAssetByObjectPath(FSoftObjectPath(ObjectPath)).IsValid();
    FString RemainingFilename;
    const bool bGone = (!Remaining || !Remaining->IsAsset()) && !bRegistered && !FPackageName::DoesPackageExist(ExpectedPackage, &RemainingFilename) && !FPaths::FileExists(Filename);
    Receipt->SetBoolField(TEXT("succeeded"), bGone); Receipt->SetBoolField(TEXT("asset_still_registered"), bRegistered);
    Receipt->SetBoolField(TEXT("file_exists_after"), FPaths::FileExists(Filename));
    return bGone;
}
FResult AuditFailure(FResult Result, TStrongObjectPtr<UStateTree>& Held, const FString& ObjectPath, const FString& Package, const TCHAR* Code)
{
    UStateTree* Tree = Held.Get(); Held.Reset(); // Do not count our own strong guard as a foreign live reference.
    const bool bCleaned = Cleanup(Tree, ObjectPath, Package, Result);
    Result.bChangeStateKnown = bCleaned; Result.bChanged = !bCleaned;
    Result.Data->SetBoolField(TEXT("cleanup_attempted"), true); Result.Data->SetBoolField(TEXT("cleanup_succeeded"), bCleaned);
    Result.Data->SetBoolField(TEXT("structural_change_rolled_back"), bCleaned);
    Result.Data->SetBoolField(TEXT("persisted_after_cleanup"), !bCleaned && Result.bSaved);
    return Fail(Result, TEXT("StateTree dependency independence audit failed; actual new-asset cleanup result is retained."), Code);
}
}

Variable::FResult Create(const FString& RawName, const FString& RawPackagePath, const FString& RawSchemaType)
{
    FResult Result; Init(Result);
    if (!IsEditorGameThread()) { return Fail(Result, TEXT("StateTree creation requires the editor game thread.")); }
    if (IsPlaySessionActive()) { return Fail(Result, TEXT("Cannot create StateTree assets while PIE/SIE is active")); }
    const FString Name = RawName.TrimStartAndEnd();
    FString PackagePath = RawPackagePath.TrimStartAndEnd(); PackagePath.TrimQuotesInline(); PackagePath.ReplaceInline(TEXT("\\"), TEXT("/"));
    while (PackagePath.Len() > 1 && PackagePath.EndsWith(TEXT("/"))) { PackagePath.LeftChopInline(1); }
    FString SchemaType = RawSchemaType.TrimStartAndEnd(); SchemaType.ToLowerInline();
    FText InvalidReason;
    if (Name.IsEmpty() || Name.Len() > 64 || Name.Contains(TEXT("/")) || Name.Contains(TEXT("\\")) || Name.Contains(TEXT("."))
        || Name.Equals(TEXT("None"), ESearchCase::IgnoreCase) || !FName::IsValidXName(Name, INVALID_OBJECTNAME_CHARACTERS, &InvalidReason))
    { return Fail(Result, TEXT("name must be a valid simple non-None Unreal asset name of1-64 characters")); }
    if (PackagePath.Len() >= NAME_SIZE || !(PackagePath == TEXT("/Game") || PackagePath.StartsWith(TEXT("/Game/")))
        || !FPackageName::IsValidLongPackageName(PackagePath, false))
    { return Fail(Result, TEXT("package_path must be a valid /Game package path")); }
    if (Forbidden(Name) || Forbidden(PackagePath)) { return Fail(Result, TEXT("name and package_path must not contain an MCP-only dependency token")); }
    const FString Package = PackagePath + TEXT("/") + Name;
    const FString ObjectPath = Package + TEXT(".") + Name;
    if (ObjectPath.Len() >= NAME_SIZE || !FPackageName::IsValidObjectPath(ObjectPath, &InvalidReason)) { return Fail(Result, TEXT("Invalid StateTree object path")); }
    UClass* SchemaClass = SchemaType == TEXT("component") ? UStateTreeComponentSchema::StaticClass()
        : SchemaType == TEXT("ai") ? UStateTreeAIComponentSchema::StaticClass() : nullptr;
    if (!SchemaClass) { return Fail(Result, TEXT("schema must be component or ai")); }
    Result.Data->SetStringField(TEXT("requested_asset_path"), ObjectPath);
    Result.Data->SetStringField(TEXT("schema"), SchemaType); Result.Data->SetStringField(TEXT("schema_class"), SchemaClass->GetPathName());
    Count(Result, TEXT("asset_exists_calls")); const bool bExists = UEditorAssetLibrary::DoesAssetExist(Package);
    Result.Data->SetBoolField(TEXT("existed_before"), bExists);
    if (bExists)
    {
        const UStateTree* Before = FindObject<UStateTree>(nullptr, *ObjectPath);
        const bool bBeforeLoaded = Loaded(Before);
        const uint32 BeforeHash = bBeforeLoaded ? Before->LastCompiledEditorDataHash : 0;
        const bool bBeforeDirty = bBeforeLoaded && Before->GetOutermost()->IsDirty();
        Result.Data->SetBoolField(TEXT("already_loaded_before_call"), bBeforeLoaded);
        if (bBeforeLoaded)
        {
            Result.Data->SetNumberField(TEXT("compiled_hash_before_load"), BeforeHash);
            Result.Data->SetBoolField(TEXT("dirty_before_load"), bBeforeDirty);
        }
        Result.Stage = TEXT("load_existing"); Count(Result, TEXT("asset_load_calls"));
        Result.bChangeStateKnown = false; Result.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), true);
        UStateTree* Existing = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(Package)); Count(Result, TEXT("asset_load_returns"));
        if (!Loaded(Existing)) { return Fail(Result, TEXT("Existing asset is not a readable StateTree")); }
        TStrongObjectPtr<UStateTree> HoldExisting(Existing);
        const uint32 AfterLoadHash = Existing->LastCompiledEditorDataHash;
        const bool bAfterLoadDirty = Existing->GetOutermost()->IsDirty();
        Result.Data->SetNumberField(TEXT("compiled_hash_after_load"), AfterLoadHash);
        Result.Data->SetBoolField(TEXT("dirty_after_load"), bAfterLoadDirty);
        const bool bLoadObservedChange = bBeforeLoaded && (BeforeHash != AfterLoadHash || bBeforeDirty != bAfterLoadDirty);
        Result.Data->SetBoolField(TEXT("observed_load_change"), bLoadObservedChange);
        Result.bChanged = bLoadObservedChange; Result.bChangeStateKnown = bLoadObservedChange;
        if (!Identity(Existing, ObjectPath, SchemaClass)) { return Fail(Result, TEXT("Existing StateTree schema does not match requested schema")); }
        if (!ReadReady(Existing, Result, TEXT("existing_readiness"))) { return Fail(Result, TEXT("Existing StateTree matches schema but is not ready; run compile_state_tree")); }
        if (!Identity(Existing, ObjectPath, SchemaClass)) { return Fail(Result, TEXT("Existing StateTree identity changed during readiness")); }
        if (!AuditLive(Existing, Result, TEXT("existing_live_dependency_audit"))) { return Fail(Result, TEXT("Existing StateTree dependency independence could not be confirmed; no cleanup or save was requested.")); }
        Result.Data->SetStringField(TEXT("asset_path"), Existing->GetPathName()); Result.Data->SetBoolField(TEXT("created"), false);
        Result.Data->SetBoolField(TEXT("already_existed"), true);
        Result.Stage = TEXT("native_readback"); Result.bSuccess = true; Result.bPostconditionPassed = true;
        return Result;
    }
    // A missing AssetRegistry row does not authorize overwriting a file or user-owned loaded package.
    FString ExistingFilename; UPackage* ExistingPackage = FindPackage(nullptr, *Package);
    if (FPackageName::DoesPackageExist(Package, &ExistingFilename) || ExistingPackage)
    { return Fail(Result, TEXT("Target asset is absent from the registry but its package already exists; preserve it and inspect actual state before a new request.")); }
    Result.Stage = TEXT("create_asset"); Result.bMutationAttempted = true; Result.bChangeStateKnown = false;
    Result.bCompileRequested = true; Result.bSaveRequested = true;
    Count(Result, TEXT("factory_allocation_calls")); TStrongObjectPtr<UStateTreeFactory> Factory(NewObject<UStateTreeFactory>());
    if (!Loaded(Factory.Get())) { return Fail(Result, TEXT("StateTree factory allocation failed")); }
    TObjectPtr<UClass> SchemaClassPtr = SchemaClass; Count(Result, TEXT("factory_schema_set_calls")); Factory->SetSchemaClass(SchemaClassPtr);
    Result.Data->SetBoolField(TEXT("factory_internal_compile_possible"), true);
    Count(Result, TEXT("asset_create_calls"));
    UStateTree* Tree = Cast<UStateTree>(FAssetToolsModule::GetModule().Get().CreateAsset(Name, PackagePath, UStateTree::StaticClass(), Factory.Get()));
    Count(Result, TEXT("asset_create_returns"));
    Result.Data->SetStringField(TEXT("created_asset_path"), Loaded(Tree) ? Tree->GetPathName() : FString());
    if (!Loaded(Tree))
    {
        const UObject* Remaining = FindObject<UObject>(nullptr, *ObjectPath);
        Result.Data->SetBoolField(TEXT("target_object_observed_after_failed_create"), Loaded(Remaining));
        Result.Data->SetBoolField(TEXT("target_package_observed_after_failed_create"), FindPackage(nullptr, *Package) != nullptr);
        Result.bChanged = Loaded(Remaining);
        return Fail(Result, TEXT("StateTree factory returned no readable asset; possible allocated package/object and SDK compiler prefix require inspection."));
    }
    TStrongObjectPtr<UStateTree> Held(Tree); Result.bChanged = true; Result.bChangeStateKnown = true;
    Result.Data->SetStringField(TEXT("asset_path"), Tree->GetPathName()); Result.Data->SetBoolField(TEXT("created"), true);
    if (!Identity(Tree, ObjectPath, SchemaClass)) { Result.bChangeStateKnown = false; return Fail(Result, TEXT("Created StateTree target/schema identity differs; no save was dispatched.")); }
    Result.Stage = TEXT("compile_asset"); Result.bCompileAttempted = true;
    Result.Data->SetBoolField(TEXT("save_requested"), true); Result.Data->SetBoolField(TEXT("saved"), false);
    FStateTreeCompilerLog Log; Count(Result, TEXT("explicit_compile_calls"));
    const bool bCompiled = UStateTreeEditingSubsystem::CompileStateTree(Tree, Log); Count(Result, TEXT("explicit_compile_returns"));
    Result.Compiled = bCompiled; Result.Data->SetBoolField(TEXT("compiled"), bCompiled);
    TArray<TSharedPtr<FJsonValue>> Messages;
    for (const TSharedRef<FTokenizedMessage>& Message : Log.ToTokenizedMessages())
    {
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("severity"), FTokenizedMessage::GetSeverityText(Message->GetSeverity()).ToString());
        Row->SetStringField(TEXT("message"), Message->ToText().ToString()); Messages.Add(MakeShared<FJsonValueObject>(Row));
    }
    Result.Data->SetArrayField(TEXT("compiler_messages"), Messages);
    if (!Loaded(Tree) || !Identity(Tree, ObjectPath, SchemaClass)) { Result.bChangeStateKnown = false; return Fail(Result, TEXT("StateTree became unreadable or identity changed during compilation; compile prefix is retained.")); }
    ReadReady(Tree, Result, TEXT("compiled_readiness"));
    if (!Loaded(Tree) || !Identity(Tree, ObjectPath, SchemaClass)) { Result.bChangeStateKnown = false; return Fail(Result, TEXT("StateTree became unreadable or identity changed during compiled readiness; actual compiler/readiness prefix is retained.")); }
    if (!bCompiled) { return Fail(Result, TEXT("StateTree compilation failed; newly created asset remains for inspection.")); }
    Result.Stage = TEXT("pre_save_dependency_audit");
    if (!AuditLive(Tree, Result, TEXT("pre_save_live_dependency_audit")))
    { return AuditFailure(Result, Held, ObjectPath, Package, TEXT("state_tree_pre_save_dependency_audit_failed")); }
    Result.Stage = TEXT("save_asset"); Count(Result, TEXT("explicit_mark_dirty_calls")); Tree->MarkPackageDirty();
    Result.bSaveAttempted = true; Count(Result, TEXT("explicit_save_calls")); const bool bSaveReturn = UEditorAssetLibrary::SaveLoadedAsset(Tree, false);
    Count(Result, TEXT("explicit_save_returns")); Result.Data->SetBoolField(TEXT("save_requested"), true); Result.Data->SetBoolField(TEXT("save_returned"), bSaveReturn);
    if (!Loaded(Tree) || !Identity(Tree, ObjectPath, SchemaClass)) { Result.bChangeStateKnown = false; return Fail(Result, TEXT("Target unreadable or identity changed during save; actual save return is retained.")); }
    Result.bSaved = bSaveReturn && !Tree->GetOutermost()->IsDirty(); Result.Data->SetBoolField(TEXT("saved"), Result.bSaved);
    Result.Data->SetBoolField(TEXT("package_dirty_after_save"), Tree->GetOutermost()->IsDirty());
    if (!Result.bSaved) { return Fail(Result, TEXT("StateTree compiled but a clean persisted save could not be confirmed; no cleanup/retry is implied.")); }
    Result.Stage = TEXT("post_save_dependency_audit");
    if (!AuditPersisted(Tree, Result))
    { return AuditFailure(Result, Held, ObjectPath, Package, TEXT("state_tree_post_save_dependency_audit_failed")); }
    Result.Stage = TEXT("saved_asset_readback"); Count(Result, TEXT("asset_load_calls"));
    UStateTree* Readback = Cast<UStateTree>(UEditorAssetLibrary::LoadAsset(ObjectPath)); Count(Result, TEXT("asset_load_returns"));
    Result.Data->SetBoolField(TEXT("implicit_postload_or_readiness_compile_possible"), true);
    if (!Identity(Readback, ObjectPath, SchemaClass) || Readback != Tree || !ReadReady(Readback, Result, TEXT("saved_readiness"))
        || !Identity(Readback, ObjectPath, SchemaClass) || Readback->GetOutermost()->IsDirty())
    { Result.bChangeStateKnown = Loaded(Readback); return Fail(Result, TEXT("StateTree saved schema/readiness/current-clean readback failed; created compile/save prefix is retained.")); }
    Result.Data->SetBoolField(TEXT("uses_unrealmcp_classes"), false); Result.Data->SetBoolField(TEXT("same_editor_loaded_readback_verified"), true);
    Result.Data->SetBoolField(TEXT("already_existed"), false);
    Result.Stage = TEXT("native_readback"); Result.bSuccess = true; Result.bPostconditionPassed = true;
    return Result;
}
}
