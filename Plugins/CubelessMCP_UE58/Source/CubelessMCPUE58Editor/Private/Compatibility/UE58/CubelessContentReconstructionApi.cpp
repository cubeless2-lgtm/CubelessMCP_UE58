#include "Compatibility/UE58/CubelessContentReconstructionApi.h"
#include "Compatibility/UE58/CubelessVariableApi.h"
#include "Operations/CubelessVariableOperations.h"
#include "AssetRegistry/IAssetRegistry.h"
#include "ObjectTools.h"
#include "Engine/World.h"
#include "UObject/UObjectIterator.h"
#include "StaticMeshResources.h"
#include "Serialization/JsonReader.h"
#include "UObject/GarbageCollection.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UnrealType.h"
#include "Misc/Crc.h"
#include "Interfaces/IPluginManager.h"
#include "EdGraph/EdGraphPin.h"
#include "Algo/Sort.h"
#include "AssetCompilingManager.h"
#include "AssetRegistry/AssetRegistryModule.h"
#include "Blueprint/WidgetTree.h"
#include "Components/ActorComponent.h"
#include "Components/Widget.h"
#include "CoreGlobals.h"
#include "EdGraph/EdGraph.h"
#include "EditorAssetLibrary.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/DataTable.h"
#include "Engine/Engine.h"
#include "Engine/Level.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/StaticMesh.h"
#include "Engine/StaticMeshSocket.h"
#include "Engine/Texture.h"
#include "Engine/Texture2D.h"
#include "Engine/TextureCube.h"
#include "Engine/UserDefinedEnum.h"
#include "Engine/VolumeTexture.h"
#include "Editor.h"
#include "Editor/Transactor.h"
#include "FileHelpers.h"
#include "GameFramework/InputSettings.h"
#include "GameFramework/Actor.h"
#include "HAL/IConsoleManager.h"
#include "K2Node_BaseMCDelegate.h"
#include "K2Node_CallFunction.h"
#include "K2Node_CallFunctionOnMember.h"
#include "K2Node_DynamicCast.h"
#include "K2Node_EditablePinBase.h"
#include "K2Node_Event.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_FunctionTerminator.h"
#include "K2Node_StructOperation.h"
#include "K2Node_Variable.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "Kismet2/EnumEditorUtils.h"
#include "Kismet2/KismetReinstanceUtilities.h"
#include "Kismet2/KismetEditorUtilities.h"
#include "Kismet2/StructureEditorUtils.h"
#include "MaterialShared.h"
#include "Materials/Material.h"
#include "Materials/MaterialExpression.h"
#include "Materials/MaterialExpressionComment.h"
#include "Materials/MaterialExpressionMaterialFunctionCall.h"
#include "Materials/MaterialExpressionNamedReroute.h"
#include "Materials/MaterialExpressionTextureBase.h"
#include "Materials/MaterialExpressionTextureSample.h"
#include "Materials/MaterialFunction.h"
#include "Materials/MaterialInstance.h"
#include "Materials/MaterialInterface.h"
#include "MeshDescription.h"
#include "Misc/App.h"
#include "Misc/EngineVersionComparison.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "PackageTools.h"
#include "PhysicsEngine/BodySetup.h"
#include "ScopedTransaction.h"
#include "Serialization/ArchiveReplaceObjectRef.h"
#include "Serialization/JsonSerializer.h"
#include "Serialization/JsonWriter.h"
#include "Sound/SoundWave.h"
#include "Templates/UnrealTemplate.h"
#include "UObject/ScriptInterface.h"
#include "UObject/SavePackage.h"
#include "UObject/UObjectHash.h"
#include "StructUtils/UserDefinedStruct.h"
#include "UserDefinedStructure/UserDefinedStructEditorData.h"
#include "WidgetBlueprint.h"


namespace Cubeless::UE58::ContentReconstruction
{
namespace
{
using Cubeless::UE58::Variable::FResult;
bool Running = false;
struct FWork;
FWork* Current = nullptr;
bool Live(const UObject* O) { return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool Safe() { return IsInGameThread() && GEditor && !IsEngineExitRequested() && !GExitPurge && !IsGarbageCollecting(); }
bool Under(const FString& P, const FString& R) { return !R.IsEmpty() && (P.Equals(R, ESearchCase::IgnoreCase) || P.StartsWith(R + TEXT("/"), ESearchCase::IgnoreCase)); }
FString EncodeJson(const TSharedPtr<FJsonObject>& J) { FString S; if (J) FJsonSerializer::Serialize(J.ToSharedRef(), TJsonWriterFactory<>::Create(&S)); return S; }
bool MCPOnly(const FString& Text)
{
    for (const TCHAR* Token : { TEXT("UnrealMCP"), TEXT("MCPUnreal"), TEXT("mcp_unreal"), TEXT("CubelessControlRigTools"), TEXT("/Script/CubelessMCP"),
        TEXT("/CubelessMCP_UE58/"), TEXT("/ModelContextProtocol/"), TEXT("/ToolsetRegistry/"), TEXT("/PythonScriptPlugin/"), TEXT("/EditorScriptingUtilities/"),
        TEXT("/Script/ModelContextProtocol"), TEXT("/Script/ToolsetRegistry"), TEXT("/Script/PythonScriptPlugin"), TEXT("/Script/EditorScriptingUtilities") })
        if (Text.Contains(Token, ESearchCase::IgnoreCase)) return true;
    return false;
}
struct FExactKeys : DefaultKeyFuncs<FString>
{
    static bool Matches(const FString& A, const FString& B) { return A.Equals(B, ESearchCase::CaseSensitive); }
    static uint32 GetKeyHash(const FString& S) { return FCrc::StrCrc32(*S); }
};
bool Parse(const FString& S, TSharedPtr<FJsonObject>& O)
{
    if (S.IsEmpty() || S.Len() > 8388608) return false;
    for (TCHAR C : S) if (C == 0) return false;
    auto Reader = TJsonReaderFactory<>::Create(S);
    TArray<bool> Objects; TArray<TSet<FString, FExactKeys>> Keys; EJsonNotation Token; int32 Count = 0;
    while (Reader->ReadNext(Token))
    {
        if (Token == EJsonNotation::Error || ++Count > 1048576) return false;
        if (Token != EJsonNotation::ObjectEnd && Token != EJsonNotation::ArrayEnd && !Objects.IsEmpty() && Objects.Last())
        { FString K(Reader->GetIdentifier()); if (Keys.Last().Contains(K)) return false; Keys.Last().Add(K); }
        if (Token == EJsonNotation::ObjectStart || Token == EJsonNotation::ArrayStart)
        { Objects.Add(Token == EJsonNotation::ObjectStart); Keys.AddDefaulted(); if (Objects.Num() > 64) return false; }
        else if (Token == EJsonNotation::ObjectEnd || Token == EJsonNotation::ArrayEnd)
        { if (Objects.IsEmpty()) return false; Objects.Pop(); Keys.Pop(); }
        if (Token == EJsonNotation::Number && !FMath::IsFinite(Reader->GetValueAsNumber())) return false;
    }
    return Objects.IsEmpty() && FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(S), O) && O.IsValid();
}
struct FIdentity
{
    TWeakObjectPtr<UObject> Object, Outer;
    TWeakObjectPtr<UClass> Class;
    TWeakObjectPtr<UPackage> Package;
    TStrongObjectPtr<UObject> Hold{nullptr};
    FString Path, PackageName;
    bool BeforeDirty = false, Created = false, Retired = false;
};
struct FWork
{
    FResult R; FString Operation, SourceRoot, TargetRoot, FailedStage;
    TWeakObjectPtr<UWorld> InitialWorld;
    TArray<FIdentity> Targets;
    TArray<TSharedPtr<FJsonValue>> Prefix, Saves, Compiles, Reports, Retirement;
    uint64 Serial = 0;
    TUniquePtr<FScopedTransaction> Transaction;
    TSet<FName> OwnedPackageNames, ClearedPackageNames;
    bool DryRun = false;
    TWeakObjectPtr<UPackage> LastWritePackage;
    bool LastWriteDirty = false;
    FWork() { R.Compiled = false; }
    bool Fail(const TCHAR* Code, const FString& Error)
    { if (R.Error.IsEmpty()) { R.ErrorCode = TEXT("content_reconstruction_") + FString(Code); R.Error = Error; FailedStage = R.Stage; } return false; }
    bool Guard()
    {
        if (!Safe()) return Fail(TEXT("unsafe_context"), TEXT("Game thread/editor lifetime, Engine exit or GC admission failed; no further SDK work was dispatched"));
        if (!R.Error.IsEmpty()) return false;
        if (!DryRun && GEditor->GetPIEWorldContext()) return Fail(TEXT("pie_active"), TEXT("Content writes require idle Editor state"));
        UWorld* Now = GEditor->GetEditorWorldContext().World();
        if (Now != InitialWorld.Get() || (Now && Now->bIsTearingDown))
            return Fail(TEXT("world_identity_lost"), TEXT("Active Editor world changed or is tearing down"));
        for (const auto& T : Targets)
        {
            if (T.Retired) continue;
            UObject* O = T.Object.Get();
            if (!Live(O) || !Live(T.Package.Get()) || O->GetClass() != T.Class.Get() || O->GetOuter() != T.Outer.Get() ||
                O->GetPackage() != T.Package.Get() || !O->GetPathName().Equals(T.Path, ESearchCase::CaseSensitive) ||
                !T.Package->GetName().Equals(T.PackageName, ESearchCase::CaseSensitive))
                return Fail(TEXT("target_identity_lost"), TEXT("Captured class/outer/package/path changed across an SDK callback"));
        }
        return true;
    }
    void PrefixEvent(const TCHAR* Stage, bool Write)
    {
        R.Stage = Stage; R.bChangeStateKnown = false;
        if (Write) { R.bMutationAttempted = true; }
        auto J = MakeShared<FJsonObject>(); J->SetNumberField(TEXT("index"), ++Serial); J->SetStringField(TEXT("stage"), Stage);
        J->SetBoolField(TEXT("write_attempted"), Write); J->SetBoolField(TEXT("sdk_returned"), false);
        if (Prefix.Num() < 131072) Prefix.Add(MakeShared<FJsonValueObject>(J));
        else R.Data->SetBoolField(TEXT("actual_prefix_truncated"), true);
    }
    bool Before(const TCHAR* Stage, bool Write)
    { if (!Guard()) return false; if (Write && DryRun) return Fail(TEXT("dry_run_write"), TEXT("A dry-run attempted an asset writer")); if (Write && !Transaction && !GEditor->IsTransactionActive())
        { Transaction = MakeUnique<FScopedTransaction>(FText::FromString(TEXT("Cubeless content reconstruction"))); if (!Guard()) return false; }
        LastWritePackage.Reset();
        PrefixEvent(Stage, Write); return true; }
    bool After()
    {
        if (!Prefix.IsEmpty()) Prefix.Last()->AsObject()->SetBoolField(TEXT("sdk_returned"), true);
        if (Safe() && Live(LastWritePackage.Get()))
        { const bool Dirty = LastWritePackage->IsDirty(); R.bChanged |= LastWriteDirty != Dirty; if (!Prefix.IsEmpty()) Prefix.Last()->AsObject()->SetBoolField(TEXT("package_dirty_after"), Dirty); }
        LastWritePackage.Reset();
        return Guard();
    }
    bool Track(UObject* O, bool Created = false)
    {
        if (!Live(O)) return Fail(TEXT("object_unresolved"), TEXT("SDK object is null, pending load or destroyed"));
        for (auto& T : Targets) if (!T.Retired && T.Object.Get() == O) { T.Created |= Created; return true; }
        FIdentity T; T.Object = O; T.Outer = O->GetOuter(); T.Class = O->GetClass(); T.Package = O->GetPackage();
        if (!Live(T.Package.Get())) return Fail(TEXT("package_unresolved"), TEXT("Object package is unavailable"));
        T.Hold.Reset(O); T.Path = O->GetPathName(); T.PackageName = T.Package->GetName(); T.BeforeDirty = T.Package->IsDirty(); T.Created = Created;
        Targets.Add(MoveTemp(T)); return true;
    }
    bool Write(UObject* O, const TCHAR* Stage)
    {
        if (!Guard() || !Live(O)) return Fail(TEXT("write_target_unresolved"), TEXT("Writer target is unavailable"));
        const FString PackageName = O->GetPackage()->GetName();
        if (!Under(PackageName, TargetRoot) || Under(PackageName, SourceRoot) || MCPOnly(O->GetClass()->GetPathName()))
            return Fail(TEXT("write_scope_violation"), TEXT("Writer escaped target_root, reached source_root, or targets an MCP integration type"));
        if (!Track(O) || !Before(Stage, true)) return false;
        LastWritePackage = O->GetPackage(); LastWriteDirty = LastWritePackage->IsDirty();
        if (!Prefix.IsEmpty()) { auto J = Prefix.Last()->AsObject(); J->SetStringField(TEXT("object_path_before"), O->GetPathName()); J->SetBoolField(TEXT("package_dirty_before"), LastWriteDirty); }
        return true;
    }
    bool AdmitLoadedTargets()
    {
        if (!Guard()) return false;
        // All already loaded packages are checked before the first operation writer.
        for (TObjectIterator<UPackage> It; It; ++It)
        {
            UPackage* P = *It;
            if (!Live(P) || !Under(P->GetName(), TargetRoot)) continue;
            if (!DryRun && P->IsDirty()) return Fail(TEXT("target_dirty"), TEXT("A target package has unsaved pre-existing changes: ") + P->GetName());
        }
        return true;
    }
    void Retire(UObject* O, const TCHAR* Reason)
    {
        auto J = MakeShared<FJsonObject>(); J->SetStringField(TEXT("reason"), Reason);
        if (Safe() && Live(O)) { J->SetStringField(TEXT("path_before"), O->GetPathName()); J->SetStringField(TEXT("package_before"), O->GetPackage()->GetName()); }
        for (auto& T : Targets) if (T.Object.Get() == O) T.Retired = true;
        Retirement.Add(MakeShared<FJsonValueObject>(J));
    }
    void Observe()
    {
        if (!Safe()) { R.bChangeStateKnown = false; return; }
        TArray<TSharedPtr<FJsonValue>> Rows;
        for (const auto& T : Targets)
        {
            auto J = MakeShared<FJsonObject>(); UObject* O = T.Object.Get();
            J->SetStringField(TEXT("path_before"), T.Path); J->SetStringField(TEXT("package_before"), T.PackageName);
            J->SetBoolField(TEXT("dirty_before"), T.BeforeDirty); J->SetBoolField(TEXT("created"), T.Created); J->SetBoolField(TEXT("retired"), T.Retired);
            J->SetBoolField(TEXT("live_after"), Live(O));
            if (Live(O))
            {
                const bool Same = O->GetClass() == T.Class.Get() && O->GetOuter() == T.Outer.Get() && O->GetPackage() == T.Package.Get() && O->GetPathName().Equals(T.Path, ESearchCase::CaseSensitive);
                J->SetBoolField(TEXT("identity_unchanged"), Same); J->SetStringField(TEXT("path_after"), O->GetPathName());
                J->SetBoolField(TEXT("dirty_after"), O->GetPackage()->IsDirty());
                R.bChanged |= T.Created || !Same || T.BeforeDirty != O->GetPackage()->IsDirty();
            }
            else R.bChanged |= T.Created || T.Retired;
            Rows.Add(MakeShared<FJsonValueObject>(J));
        }
        R.Data->SetArrayField(TEXT("actual_target_observations"), Rows);
    }
    FString Finish(const TSharedPtr<FJsonObject>& Legacy)
    {
        if (Transaction)
        { if (Safe()) { Transaction.Reset(); Guard(); } else { (void)Transaction.Release(); R.Data->SetBoolField(TEXT("transaction_end_skipped_unsafe"), true); } }
        if (R.bCompileAttempted)
        { TMap<FString, bool> Final; for (const auto& V : Compiles) { const auto J = V->AsObject(); FString P; bool Pass = false; if (J && J->TryGetStringField(TEXT("path_before"), P)) { J->TryGetBoolField(TEXT("compile_error_absent"), Pass); Final.Add(P, Pass); } } bool All = !Final.IsEmpty(); for (const auto& Row : Final) All &= Row.Value; R.Compiled = All; }
        Observe(); bool LegacySuccess = false, Verification = false;
        if (Legacy) { Legacy->TryGetBoolField(TEXT("success"), LegacySuccess); Verification = LegacySuccess; Legacy->TryGetBoolField(TEXT("verification_pass"), Verification); }
        R.bSuccess = LegacySuccess && R.Error.IsEmpty(); R.bPostconditionPassed = R.bSuccess && Verification;
        if (!LegacySuccess && R.Error.IsEmpty()) { FString E; if (!Legacy || !Legacy->TryGetStringField(TEXT("error"), E)) E = TEXT("Original operation returned failure; see legacy_result"); Fail(TEXT("legacy_failed"), E); }
        R.Stage = R.Error.IsEmpty() ? TEXT("complete") : FailedStage;
        R.Data->SetObjectField(TEXT("legacy_result"), Legacy ? Legacy : MakeShared<FJsonObject>());
        R.Data->SetArrayField(TEXT("actual_prefix"), Prefix); R.Data->SetArrayField(TEXT("actual_save_rows"), Saves);
        R.Data->SetArrayField(TEXT("actual_compile_rows"), Compiles); R.Data->SetArrayField(TEXT("report_write_rows"), Reports);
        R.Data->SetArrayField(TEXT("retired_object_observations"), Retirement);
        R.Data->SetBoolField(TEXT("operation_complete"), true); R.Data->SetBoolField(TEXT("automatic_retry_permitted"), false);
        R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("sdk_global_side_effects_known"), false);
        R.Data->SetBoolField(TEXT("dry_run"), DryRun); R.Data->SetBoolField(TEXT("explicit_asset_mutation_attempted"), R.bMutationAttempted);
        R.Data->SetStringField(TEXT("save_scope"), TEXT("saved is the OR of actual UPackage::SavePackage true returns, retained after later failures; rows separate captured package identity, dirty readback and post-Guard status"));
        R.Data->SetStringField(TEXT("compile_scope"), TEXT("Only dispatched target Blueprint/Struct compilation; SDK-dependent rebuilds and incidental compilation are not independently counted"));
        R.Data->SetStringField(TEXT("sync_sdk_scope"), TEXT("Public compilation, texture build waits, sound payload future reads, reinstance, serialization and save may block the game thread; no extra wait, worker UObject or atomic rollback"));
        return Cubeless::Operations::EncodeVariableResult(R, *Operation);
    }
};
bool Check() { return Current && Current->Guard(); }
bool Write(UObject* O, const TCHAR* Stage) { return Current && Current->Write(O, Stage); }
bool Before(const TCHAR* Stage, bool Mutate = true) { return Current && Current->Before(Stage, Mutate); }
bool After() { return Current && Current->After(); }
TSharedPtr<FJsonObject> LegacyError(const FString& E) { auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"), false); J->SetStringField(TEXT("error"), E); return J; }
int32 CheckedI32(double V)
{ if (!FMath::IsFinite(V) || V < MIN_int32 || V > MAX_int32) { if (Current) Current->Fail(TEXT("numeric_range"), TEXT("Original int32 conversion exceeds its finite safe range")); return 0; } return static_cast<int32>(V); }
UObject* LoadAssetForGD(const FString& P)
{
    if (!Before(TEXT("load_asset"), false)) return nullptr;
    UObject* O = UEditorAssetLibrary::LoadAsset(P);
    if (!After()) return nullptr;
    if (O && !Current->Track(O)) return nullptr;
    return O;
}
UObject* LoadAssetDataForGD(const FAssetData& A) { return LoadAssetForGD(A.GetObjectPathString()); }

bool AdmitFreshObject(UObject* Outer, UClass* Class, FName Name)
{
    if (!Check() || !Live(Outer) || !Live(Class)) return false;
    for (UClass* C = Class; C; C = C->GetSuperClass())
        if (MCPOnly(C->GetPathName())) return Current->Fail(TEXT("runtime_class"), TEXT("Fresh output class derives from an MCP integration"));
    if (!Class->GetDefaultObject(false))
    { if (!Before(TEXT("prepare_fresh_class_default_object"), false)) return false; UObject* CDO = Class->GetDefaultObject(); if (!After() || !Live(CDO)) return false; }
    if (Name.IsNone())
        return Current->Fail(TEXT("fresh_name_unresolved"), TEXT("Fresh output requires its exact explicit object name"));
    if (Outer->IsA<UPackage>())
    {
        const FString PackageName = Outer->GetPackage()->GetName();
        if (!Current->OwnedPackageNames.Contains(FName(*PackageName)))
            return Current->Fail(TEXT("fresh_package_not_owned"), TEXT("Fresh top-level output did not retain its admitted operation-owned package"));
        FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
        TArray<FAssetData> Existing; Registry.Get().GetAssetsByPackageName(FName(*PackageName), Existing);
        if (!Check()) return false;
        if (!Existing.IsEmpty() || FPackageName::DoesPackageExist(PackageName))
            return Current->Fail(TEXT("late_fresh_collision"), TEXT("Fresh output acquired registry/disk residue before factory dispatch"));
    }
    if (!Check()) return false;
    if (StaticFindObjectFast(UObject::StaticClass(), Outer, Name, EFindObjectFlags::None, RF_NoFlags, EInternalObjectFlags::None))
        return Current->Fail(TEXT("late_fresh_collision"), TEXT("An object with the exact fresh name already exists; no UObject reuse or overwrite followed"));
    return Check();
}

UPackage* CreatePackageForGD(const TCHAR* P)
{
    if (!Check() || !Under(P, Current->TargetRoot) || Under(P, Current->SourceRoot))
    { if (Current) Current->Fail(TEXT("package_scope"), TEXT("Creation package escaped target_root")); return nullptr; }
    const FString Requested(P);
    const FName PackageKey(P);
    UPackage* Existing = FindPackage(nullptr, P);
    const bool Owned = Current->OwnedPackageNames.Contains(PackageKey);
    const bool Cleared = Current->ClearedPackageNames.Contains(PackageKey);
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FAssetData> Assets; Registry.Get().GetAssetsByPackageName(PackageKey, Assets);
    if (!Check()) return nullptr;
    if ((!Owned && !Cleared && Existing) || !Assets.IsEmpty() || (!Owned && !Cleared && FPackageName::DoesPackageExist(Requested)))
    { Current->Fail(TEXT("late_package_collision"), TEXT("A fresh output package already exists in memory/registry/on disk")); return nullptr; }
    if (!Before(TEXT("create_package"), true)) return nullptr;
    if (FindPackage(nullptr, P) != Existing) { Current->Fail(TEXT("late_package_collision"), TEXT("Package identity appeared or changed during transaction admission")); return nullptr; }
    UPackage* Package = CreatePackage(P);
    if (!Current->Prefix.IsEmpty()) Current->Prefix.Last()->AsObject()->SetBoolField(TEXT("returned_package_nonnull"), Package != nullptr);
    if (Package) Current->R.bChanged |= Existing == nullptr;
    if (!After()) return nullptr;
    if (Package && !Package->GetName().Equals(Requested, ESearchCase::CaseSensitive)) { Current->Fail(TEXT("created_package_identity"), TEXT("Created package name changed during SDK callback")); return nullptr; }
    if (Package) Current->OwnedPackageNames.Add(PackageKey);
    if (Package && !Current->Track(Package, Existing == nullptr)) return nullptr;
    return Package;
}
template<typename T> T* NewOwnedObject(UObject* Outer, UClass* Class, FName Name, EObjectFlags Flags)
{
    if (!Write(Outer, TEXT("new_object")) || !AdmitFreshObject(Outer, Class, Name)) return nullptr;
    T* O = NewObject<T>(Outer, Class, Name, Flags);
    if (!Current->Prefix.IsEmpty()) Current->Prefix.Last()->AsObject()->SetBoolField(TEXT("returned_object_nonnull"), O != nullptr);
    if (O) Current->R.bChanged = true;
    if (!After()) return nullptr;
    if (O && (!Live(O) || O->GetClass() != Class || O->GetOuter() != Outer || O->GetFName() != Name))
    { Current->Fail(TEXT("fresh_result_identity"), TEXT("Fresh result class/outer/name changed across factory callback")); return nullptr; }
    if (O && !Current->Track(O, true)) return nullptr;
    return O;
}

bool AuditRuntimeReferences(UObject* O)
{
    if (!Check() || !Live(O)) return false;
    TArray<UObject*> Objects; GetObjectsWithPackage(O->GetPackage(), Objects, true, RF_Transient, EInternalObjectFlags::Garbage);
    if (Objects.Num() > 100000) return Current->Fail(TEXT("reference_budget"), TEXT("Package runtime reference audit exceeded its object budget"));
    for (UObject* Item : Objects)
    {
        if (!Live(Item)) continue;
        for (UClass* C = Item->GetClass(); C; C = C->GetSuperClass())
            if (MCPOnly(C->GetPathName())) return Current->Fail(TEXT("runtime_dependency"), TEXT("Target object uses an MCP/Editor integration class"));
        TArray<UObject*> References; FReferenceFinder Finder(References, nullptr, false, false, false, false);
        if (!Before(TEXT("audit_runtime_references"), false)) return false;
        Finder.FindReferences(Item);
        if (!After()) return false;
        if (References.Num() > 262144) return Current->Fail(TEXT("reference_budget"), TEXT("Runtime reference audit exceeded its reference budget"));
        for (UObject* Reference : References)
            if (Live(Reference) && MCPOnly(Reference->GetPathName()))
                return Current->Fail(TEXT("runtime_dependency"), TEXT("Serialized hard reference to an MCP/Editor integration was found before save"));
    }
    return true;
}
bool SaveExactForGD(UPackage* Package, UObject* Asset, const FString& Filename, EObjectFlags Flags)
{
    if (!Check() || !Live(Package) || !Live(Asset) || Asset->GetPackage() != Package || !AuditRuntimeReferences(Asset) || !Write(Asset, TEXT("save_package"))) return false;
    Current->R.bSaveAttempted = true; const FString Path = Package->GetName();
    TStrongObjectPtr<UPackage> Pin(Package); FSavePackageArgs Args; Args.TopLevelFlags = Flags; Args.SaveFlags = SAVE_NoError; Args.bSlowTask = false;
    const bool Returned = UPackage::SavePackage(Package, Asset, *Filename, Args);
    Current->R.bSaved |= Returned;
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("package_before"), Path); Row->SetStringField(TEXT("filename"), Filename); Row->SetBoolField(TEXT("sdk_returned_true"), Returned);
    const bool SafeRead = Safe() && Live(Package);
    Row->SetBoolField(TEXT("after_readback_safe"), SafeRead);
    if (SafeRead) { Row->SetStringField(TEXT("package_after"), Package->GetName()); Row->SetBoolField(TEXT("name_unchanged"), Package->GetName().Equals(Path, ESearchCase::CaseSensitive)); Row->SetBoolField(TEXT("dirty_after"), Package->IsDirty()); }
    const bool Admitted = After(); Row->SetBoolField(TEXT("post_guard_passed"), Admitted);
    Current->Saves.Add(MakeShared<FJsonValueObject>(Row));
    return Returned && Admitted && SafeRead && !Package->IsDirty();
}



bool BeforeCleanup(const TCHAR* Stage)
{
    if (!Check()) return false;
    if (Current->Transaction)
    {
        Current->Transaction->Cancel();
        Current->Transaction.Reset();
        Current->R.Data->SetBoolField(TEXT("owned_transaction_cancelled_for_cleanup"), true);
        if (!Check()) return false;
    }
    if (GEditor->IsTransactionActive())
        return Current->Fail(TEXT("cleanup_outer_transaction"), TEXT("Destructive cleanup is blocked inside an external Editor/batch transaction; its Undo state was not reset"));
    if (Current->DryRun) return Current->Fail(TEXT("dry_run_cleanup"), TEXT("Dry-run cannot enter cleanup"));
    Current->PrefixEvent(Stage, true);
    Current->R.Data->SetBoolField(TEXT("public_delete_hook_side_effects_unknown"), true);
    Current->R.Data->SetStringField(TEXT("public_delete_scope"), TEXT("Exact targets and references/Undo are preflighted; public deletion may invoke OnAssetsCanDelete/editor/world hooks and modal logic. Their internal late changes cannot be proven pure or globally atomic."));
    return true;
}

bool MoveOwnedObject(UObject* O, const TCHAR* Name, UObject* Outer, ERenameFlags Flags)
{
    if (!Check() || !Live(O) || (Outer && !Live(Outer))) return false;
    bool Owned = Under(O->GetPackage()->GetName(), Current->TargetRoot) && !Under(O->GetPackage()->GetName(), Current->SourceRoot);
    for (const auto& T : Current->Targets)
        if (T.Object.Get() == O && Under(T.PackageName, Current->TargetRoot) && !Under(T.PackageName, Current->SourceRoot)) Owned = true;
    if (!Owned) return Current->Fail(TEXT("rename_scope"), TEXT("Rename is not an exact target-owned object"));
    if (Outer && Outer != GetTransientPackage() && !Under(Outer->GetPackage()->GetName(), Current->TargetRoot))
        return Current->Fail(TEXT("rename_destination_scope"), TEXT("Rename destination escaped the captured target scope"));
    struct FChildMove { UObject* Object; UObject* Outer; UClass* Class; };
    TArray<FChildMove> Children;
    for (const auto& T : Current->Targets) if (!T.Retired && T.Object.Get() != O && Live(T.Object.Get()) && T.Object->IsIn(O)) Children.Add({ T.Object.Get(), T.Outer.Get(), T.Class.Get() });
    if (!Before(TEXT("rename_owned_target"), true)) return false;
    Current->Retire(O, TEXT("intentional_owned_object_rename"));
    for (const auto& Child : Children) Current->Retire(Child.Object, TEXT("intentional_parent_rename"));
    const bool Returned = O->Rename(Name, Outer, Flags);
    if (!Current->Prefix.IsEmpty()) Current->Prefix.Last()->AsObject()->SetBoolField(TEXT("rename_returned"), Returned);
    Current->R.bChanged |= Returned;
    if (!After()) return false;
    if (!Returned || !Current->Track(O)) return false;
    for (const auto& Child : Children)
    { if (!Live(Child.Object) || Child.Object->GetClass() != Child.Class || Child.Object->GetOuter() != Child.Outer || !Child.Object->IsIn(O)) return Current->Fail(TEXT("rename_child_identity"), TEXT("Owned child class/outer changed during parent rename")); if (!Current->Track(Child.Object)) return false; }
    return true;
}
template<typename F> auto CreateOwnedFactory(UObject* Outer, UClass* Class, FName Name, const TCHAR* Stage, F&& Factory) -> decltype(Factory())
{
    using TReturn = decltype(Factory());
    if (!Write(Outer, Stage) || !AdmitFreshObject(Outer, Class, Name)) return TReturn{};
    TReturn O = Factory();
    if (!Current->Prefix.IsEmpty()) Current->Prefix.Last()->AsObject()->SetBoolField(TEXT("returned_object_nonnull"), O != nullptr);
    if (O) Current->R.bChanged = true;
    if (!After()) return TReturn{};
    if (O && (!Live(O) || O->GetClass() != Class || O->GetOuter() != Outer || O->GetFName() != Name))
    { Current->Fail(TEXT("fresh_result_identity"), TEXT("Factory result class/outer/name changed across callback")); return TReturn{}; }
    if (O && !Current->Track(O, true)) return TReturn{};
    return O;
}

UPackage* LoadPackageForGD(UPackage* Outer, const TCHAR* Name, uint32 Flags)
{
    if (!Before(TEXT("load_package"), false)) return nullptr;
    UPackage* P = LoadPackage(Outer, Name, Flags);
    if (!After()) return nullptr;
    if (P && !Current->Track(P)) return nullptr;
    return P;
}
bool DeleteExactAssetForGD(const FString& Path)
{
    UObject* O = LoadAssetForGD(Path);
    if (!O || !Check() || !Under(O->GetPackage()->GetName(), Current->TargetRoot) || Under(O->GetPackage()->GetName(), Current->SourceRoot)) return false;
    if (O->GetPackage()->IsDirty()) return Current->Fail(TEXT("overwrite_dirty"), TEXT("Overwrite target acquired dirty state before deletion"));
    FAssetRegistryModule& Registry = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    TArray<FName> Refs; Registry.Get().GetReferencers(FName(*O->GetPackage()->GetName()), Refs, UE::AssetRegistry::EDependencyCategory::Package);
    for (const FName& Ref : Refs)
        if (Ref != O->GetPackage()->GetFName()) return Current->Fail(TEXT("overwrite_referenced"), TEXT("Existing asset has serialized referencers; no force replacement was attempted"));
    for (auto& T : Current->Targets) if (T.Object.Get() == O) T.Hold.Reset();
    bool Referenced = false, Undo = false;
    ObjectTools::GatherObjectReferencersForDeletion(O, Referenced, Undo);
    if (!Check()) return false;
    if (Referenced || Undo) return Current->Fail(TEXT("overwrite_referenced"), TEXT("Existing asset has loaded or Undo referencers; no reset or force delete was attempted"));
    if (!BeforeCleanup(TEXT("overwrite_delete_exact_asset"))) return false;
    Current->Retire(O, TEXT("explicit_overwrite"));
    const int32 DeletedCount = ObjectTools::DeleteObjects({ O }, false, ObjectTools::EAllowCancelDuringDelete::AllowCancel);
    const bool Deleted = DeletedCount == 1;
    Current->R.Data->SetBoolField(TEXT("overwrite_delete_returned"), Deleted);
    Current->R.bChanged |= Deleted;
    if (Deleted) Current->ClearedPackageNames.Add(FName(*Path));
    if (!After() || !Deleted) return false;
    if (FPackageName::DoesPackageExist(Path) || UEditorAssetLibrary::DoesAssetExist(Path))
        return Current->Fail(TEXT("overwrite_cleanup_residual"), TEXT("Deleted asset still has disk/registry residue; no same-path recreation followed"));
    return true;
}
UObject* DuplicateAssetForGD(const FString& Source, const FString& Target)
{
    if (!Check() || !Under(Target, Current->TargetRoot) || Under(Target, Current->SourceRoot)) return nullptr;
    if (UEditorAssetLibrary::DoesAssetExist(Target))
    { Current->Fail(TEXT("late_output_collision"), TEXT("A target appeared before duplicate dispatch; no overwrite was attempted")); return nullptr; }
    if (!Before(TEXT("duplicate_asset"), true)) return nullptr;
    Current->R.Data->SetBoolField(TEXT("sdk_implicit_save_possible"), true);
    Current->R.Data->SetStringField(TEXT("sdk_implicit_save_scope"), TEXT("AssetTools duplication may perform source-control checkout/save internally; explicit save counters are not a guarantee of no disk writes"));
    UObject* O = UEditorAssetLibrary::DuplicateAsset(Source, Target);
    if (!Current->Prefix.IsEmpty()) Current->Prefix.Last()->AsObject()->SetBoolField(TEXT("returned_object_nonnull"), O != nullptr);
    Current->R.bChanged |= O != nullptr;
    if (!After()) return nullptr;
    if (O && (!O->GetPackage()->GetName().Equals(Target, ESearchCase::CaseSensitive) || !Current->Track(O, true)))
    { Current->Fail(TEXT("duplicate_identity"), TEXT("Duplicate result did not retain its exact requested package")); return nullptr; }
    return O;
}
bool RemoveEmptyDirectoryTree(const FString& PackageRoot, TArray<FString>& Warnings)
{
    FString Directory;
    if (!FPackageName::TryConvertLongPackageNameToFilename(PackageRoot, Directory)) return false;
    Directory = FPaths::ConvertRelativePathToFull(Directory);
    FString Content = FPaths::ConvertRelativePathToFull(FPaths::ProjectContentDir());
    FPaths::NormalizeDirectoryName(Directory); FPaths::NormalizeDirectoryName(Content);
    if (!Directory.StartsWith(Content + TEXT("/"), ESearchCase::IgnoreCase)) return Current->Fail(TEXT("directory_scope"), TEXT("Resolved target directory escaped project Content"));
    if (!IFileManager::Get().DirectoryExists(*Directory)) return true;
    TArray<FString> Files, Directories;
    IFileManager::Get().FindFilesRecursive(Files, *Directory, TEXT("*"), true, false);
    if (!Files.IsEmpty())
    { Warnings.Add(TEXT("Residual files were retained; empty-only directory cleanup did not delete files")); return false; }
    IFileManager::Get().FindFilesRecursive(Directories, *Directory, TEXT("*"), false, true);
    Directories.Sort([](const FString& A, const FString& B) { return A.Len() > B.Len(); }); Directories.Add(Directory);
    for (const FString& D : Directories)
    {
        if (!BeforeCleanup(TEXT("remove_empty_directory"))) return false;
        const bool Removed = IFileManager::Get().DeleteDirectory(*D, false, false);
        if (!After() || !Removed) { Warnings.Add(TEXT("Directory was not empty or could not be removed: ") + D); return false; }
    }
    return true;
}
void CopyOwnedProperties(UObject* Source, UObject* Target, UEngine::FCopyPropertiesForUnrelatedObjectsParams Params)
{
    if (!Write(Target, TEXT("copy_properties_for_unrelated_objects"))) return;
    UEngine::CopyPropertiesForUnrelatedObjects(Source, Target, Params);
    After();
}
int64 RemapOwnedReferences(UObject* Target, const TMap<UObject*, UObject*>& Map, EArchiveReplaceObjectFlags Flags)
{
    if (!Write(Target, TEXT("archive_replace_object_references"))) return 0;
    FArchiveReplaceObjectRef<UObject> Archive(Target, Map, Flags);
    const int64 ReplacementCount = Archive.GetCount();
    if (!After()) return 0;
    return ReplacementCount;
}
void CompileOwnedBlueprint(UBlueprint* Blueprint)
{
    if (!Write(Blueprint, TEXT("compile_blueprint"))) return;
    const FString Path = Blueprint->GetPathName();
    UPackage* const TargetPackage = Blueprint->GetPackage();
    // Only the exact compiled target's current CDOs may be moved aside by this SDK call.
    // Source/foreign classes, the Blueprint itself and newly acquired CDOs are not retired.
    auto CaptureClassCDO = [Blueprint, TargetPackage](UClass* Class, UObject*& OutCDO) -> bool
    {
        OutCDO = nullptr;
        if (!Class) return true;
        if (!Live(Class) || Class->ClassGeneratedBy != Blueprint || Class->GetPackage() != TargetPackage)
            return Current->Fail(TEXT("compile_cdo_owner_mismatch"), TEXT("Compiled target class is not owned by the exact target Blueprint/package"));
        OutCDO = Class->GetDefaultObject(false);
        if (OutCDO && (!Live(OutCDO) || OutCDO->GetClass() != Class ||
            !OutCDO->HasAnyFlags(RF_ClassDefaultObject) || OutCDO->GetPackage() != TargetPackage))
            return Current->Fail(TEXT("compile_cdo_owner_mismatch"), TEXT("Compiled target CDO does not match its exact Blueprint class/package"));
        return true;
    };
    UObject* PreviousGeneratedCDO = nullptr;
    UObject* PreviousSkeletonCDO = nullptr;
    if (!CaptureClassCDO(Blueprint->GeneratedClass, PreviousGeneratedCDO) ||
        !CaptureClassCDO(Blueprint->SkeletonGeneratedClass, PreviousSkeletonCDO)) return;
    auto RetirePreviousCDOPins = [TargetPackage](UObject* PreviousCDO, const TCHAR* Reason)
    {
        if (!PreviousCDO) return;
        for (const auto& T : Current->Targets)
        {
            UObject* O = T.Object.Get();
            if (!T.Retired && Live(O) && O->GetPackage() == TargetPackage &&
                (O == PreviousCDO || O->IsIn(PreviousCDO)))
                Current->Retire(O, Reason);
        }
    };
    RetirePreviousCDOPins(PreviousGeneratedCDO, TEXT("compile_target_generated_cdo_reinstance"));
    RetirePreviousCDOPins(PreviousSkeletonCDO, TEXT("compile_target_skeleton_cdo_reinstance"));
    Current->R.bCompileAttempted = true;
    FKismetEditorUtilities::CompileBlueprint(Blueprint);
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path_before"), Path); Row->SetBoolField(TEXT("sdk_returned"), true);
    const bool Readable = Safe() && Live(Blueprint);
    Row->SetBoolField(TEXT("after_readback_safe"), Readable);
    const bool Passed = Readable && Blueprint->Status != BS_Error;
    if (Readable) Row->SetNumberField(TEXT("status_after"), static_cast<int32>(Blueprint->Status));
    Row->SetBoolField(TEXT("compile_error_absent"), Passed);
    Current->R.Compiled = Current->R.Compiled.Get(false) || Passed;
    bool Admitted = After();
    if (Admitted)
    {
        // Reacquire from the current Blueprint rather than reuse a pre-compile CDO.
        UObject* CurrentGeneratedCDO = nullptr;
        UObject* CurrentSkeletonCDO = nullptr;
        Admitted = CaptureClassCDO(Blueprint->GeneratedClass, CurrentGeneratedCDO) &&
            CaptureClassCDO(Blueprint->SkeletonGeneratedClass, CurrentSkeletonCDO);
        if (Admitted && Blueprint->GeneratedClass) Admitted = Current->Track(Blueprint->GeneratedClass);
        if (Admitted && Blueprint->SkeletonGeneratedClass) Admitted = Current->Track(Blueprint->SkeletonGeneratedClass);
        if (Admitted && CurrentGeneratedCDO) Admitted = Current->Track(CurrentGeneratedCDO);
        if (Admitted && CurrentSkeletonCDO) Admitted = Current->Track(CurrentSkeletonCDO);
        Row->SetBoolField(TEXT("current_class_cdo_reacquired_and_pinned"), Admitted);
    }
    Row->SetBoolField(TEXT("post_guard_passed"), Admitted);
    Current->Compiles.Add(MakeShared<FJsonValueObject>(Row));
}
void CompileOwnedStructure(UUserDefinedStruct* Struct)
{
    if (!Write(Struct, TEXT("compile_structure"))) return;
    Current->R.bCompileAttempted = true; const FString Path = Struct->GetPathName();
    FStructureEditorUtils::CompileStructure(Struct);
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path_before"), Path); Row->SetBoolField(TEXT("sdk_returned"), true);
    const bool Passed = Safe() && Live(Struct) && Struct->Status != EUserDefinedStructureStatus::UDSS_Error;
    Row->SetBoolField(TEXT("compile_error_absent"), Passed); Current->R.Compiled = Current->R.Compiled.Get(false) || Passed;
    const bool Admitted = After(); Row->SetBoolField(TEXT("post_guard_passed"), Admitted); Current->Compiles.Add(MakeShared<FJsonValueObject>(Row));
}

struct FPathStringReplacement
{
    FString Source;
    FString Target;
};

struct FDeferredGeneratedClassAsset
{
    FAssetData SourceAsset;
    FString TargetPath;
    FString Strategy;
    FString Detail;
};

struct FAcceptedFallbackRule
{
    FString SourcePath;
    FString SourcePathPrefix;
    FString ClassName;
    TArray<FString> DetailContainsAny;
    FString Reason;
};

struct FEditorLogMarker
{
    FString LogPath;
    int32 StartLineCount = 0;
    int64 StartFileSize = 0;
    bool bEnabled = false;
};

struct FEditorLogHealthResult
{
    FString LogPath;
    bool bChecked = false;
    bool bPass = true;
    int32 IssueCount = 0;
    int32 HandledEnsureCount = 0;
    int32 IgnoredHandledEnsureCount = 0;
    int32 FatalErrorCount = 0;
    int32 CriticalErrorCount = 0;
    int32 ForceDeleteIssueCount = 0;
    int32 WorldMemoryLeakCount = 0;
    TArray<FString> IssueSamples;
};


FString NormalizeContentRoot(FString Path);
FString NormalizePackagePathParam(FString Path);
UWorld* LoadWorldPackageForRepair(const FString& WorldPackageName);
bool SaveWorldPackageForRepair(UWorld* LoadedWorld, const FString& WorldPackageName);
bool SaveLoadedAssetForMCP(UObject* Asset);
FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FString& DefaultValue);
bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, bool bDefaultValue);
int32 GetIntParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, int32 DefaultValue);
bool TryGetRequiredStringParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutValue,
    FString& OutError);
bool TryGetRequiredContentRootParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutRoot,
    FString& OutError);
bool ValidateContentRootPairForMCP(const FString& SourceRoot, const FString& TargetRoot, FString& OutError);
bool IsLiveObjectForMCP(const UObject* Object);
bool IsPackageUnderRoot(const UObject* Object, const FString& Root);
bool IsGeneratedClassInstanceFromRoot(const UObject* Object, const FString& Root);
bool ShouldSkipArchiveObjectReferenceRemap(const UObject* Object);
bool ShouldSkipAssetArchiveObjectReferenceRemap(const UObject* Object);
int32 GetBlueprintInheritanceDepth(
    const UBlueprint* Blueprint,
    const TMap<UClass*, const UBlueprint*>& BlueprintByGeneratedClass,
    TMap<const UBlueprint*, int32>& DepthByBlueprint,
    TSet<const UBlueprint*>& VisitingBlueprints);
TArray<UBlueprint*> SortBlueprintsByInheritance(const TArray<UBlueprint*>& Blueprints);
int32 CompileBlueprintsForMCP(
    const TArray<UBlueprint*>& Blueprints,
    int32 CompilePasses,
    bool bRefreshBlueprintNodes,
    bool bRefreshFailedBlueprintNodes,
    int32& OutCompileErrorCount,
    TArray<FString>* OutFailedAssets);
int32 CompileUserDefinedStructsForMCP(const TArray<UUserDefinedStruct*>& Structs, TArray<FString>* OutFailedAssets);
void AddReplacementMapping(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject);
void AddTargetWorldPackageName(TArray<FString>& TargetWorldPackageNames, UObject* TargetObject);
void AddBlueprintReplacementMappings(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject);
UActorComponent* DuplicateComponentTemplateForTargetBlueprint(
    USCS_Node* SourceNode,
    UBlueprint* TargetBlueprint,
    UClass* TargetComponentClass,
    const FName TargetVariableName);
int32 RepairBlueprintSCSNodes(UBlueprint* SourceBlueprint, UBlueprint* TargetBlueprint, TMap<UObject*, UObject*>& ReplacementMap);
UWidget* DuplicateWidgetForTargetClass(UWidget* SourceWidget, UWidgetTree* TargetWidgetTree, UClass* TargetWidgetClass);
int32 RepairWidgetBlueprintTree(UBlueprint* TargetBlueprint, const TMap<UObject*, UObject*>& ReplacementMap);
int32 RepairTargetSubobjectsWithSourceClasses(const TSet<UPackage*>& TargetPackages, TMap<UObject*, UObject*>& ReplacementMap);
void RefreshMaterialReferencesAndCaches(
    const TSet<UPackage*>& TargetPackages,
    const TMap<UObject*, UObject*>& ReplacementMap,
    int32& OutTextureReferenceRepairCount,
    int32& OutMaterialCacheRefreshCount);
int32 RepairTargetWorldComponentClasses(
    UWorld* LoadedWorld,
    const TMap<UObject*, UObject*>& ReplacementMap,
    TArray<FString>& FailedAssets);
int32 RemapObjectPropertyValueForMCP(
    FObjectPropertyBase* ObjectProperty,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap);
int32 RemapInterfacePropertyValueForMCP(
    FInterfaceProperty* InterfaceProperty,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap);
int32 RemapObjectReferencesInPropertyValueForMCP(
    FProperty* Property,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap);
int32 RemapObjectReferencesInObjectPropertiesForMCP(UObject* Object, const TMap<UObject*, UObject*>& ReplacementMap);
int32 RemapTargetWorldActorInstanceReferences(UWorld* LoadedWorld, const TMap<UObject*, UObject*>& ReplacementMap);
bool ShouldRemoveRemappedMapKey(UObject* KeyObject, const FString& SourceRoot);
int32 RemoveSourceObjectKeyMapEntries(UObject* Object, const FString& SourceRoot);
int32 RemoveSourceObjectKeyMapEntriesFromWorld(UWorld* LoadedWorld, const FString& SourceRoot);
int32 CountSourceObjectKeyMapEntries(UObject* Object, const FString& SourceRoot);
int32 CountSourceObjectKeyMapEntriesFromWorld(UWorld* LoadedWorld, const FString& SourceRoot);
void CountWorldSourceClassInstances(
    UWorld* LoadedWorld,
    const FString& SourceRoot,
    int32& OutSourceActorCount,
    int32& OutSourceComponentCount);
int32 RepairLoadedWorldActorInstances(
    UWorld* LoadedWorld,
    const FString& WorldPackageName,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const TArray<FPathStringReplacement>& PathReplacements,
    const FString& SourceRoot,
    int32& OutLevelActorReferenceRemapCount,
    int32& OutSavedMapCount,
    int32& OutLevelComponentClassRepairCount,
    TArray<FString>& FailedAssets,
    bool bRemapLevelBlueprint,
    bool bRepairActorClasses,
    bool bRepairComponentClasses,
    bool bRemapActorReferences,
    bool bRemoveSourceObjectMapKeys,
    bool bSaveMap);
int32 RepairTargetWorldActorsWithSourceClasses(
    const TArray<FString>& TargetWorldPackageNames,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const TArray<FPathStringReplacement>& PathReplacements,
    const FString& SourceRoot,
    int32& OutLevelActorReferenceRemapCount,
    int32& OutSavedMapCount,
    int32& OutLevelComponentClassRepairCount,
    TArray<FString>& FailedAssets,
    bool bRemapLevelBlueprint = true,
    bool bRepairActorClasses = true,
    bool bRepairComponentClasses = true,
    bool bRemapActorReferences = true,
    bool bRemoveSourceObjectMapKeys = true,
    bool bSaveMap = true);
UObject* FindReplacementObject(const TMap<UObject*, UObject*>& ReplacementMap, UObject* Object);
bool RemapSimpleMemberReference(FSimpleMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapTerminalType(FEdGraphTerminalType& TerminalType, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapPinType(FEdGraphPinType& PinType, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapMemberReferenceParent(FMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap);
FGuid FindBlueprintSelfVariableGuid(UBlueprint* Blueprint, const FName MemberName);
FProperty* FindBlueprintPropertyByName(UBlueprint* Blueprint, const FName MemberName);
UFunction* FindBlueprintFunctionByName(UBlueprint* Blueprint, const FName MemberName);
bool RemapSelfMemberReference(FMemberReference& Reference, UBlueprint* Blueprint);
bool RemapVariableReference(UK2Node_Variable* VariableNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapFunctionReference(UK2Node_CallFunction* FunctionNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapDelegateReference(UK2Node_BaseMCDelegate* DelegateNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapEditableUserPins(UK2Node_EditablePinBase* EditableNode, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements);
bool RemapFunctionEntryLocalVariables(UK2Node_FunctionEntry* FunctionEntry, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements);
bool RemapStructOperation(UK2Node_StructOperation* StructNode, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapDynamicCastTarget(UK2Node_DynamicCast* CastNode, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapGraphPin(UEdGraphNode* OwnerNode, UEdGraphPin* Pin, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements);
bool NodeHasSplitPins(const UEdGraphNode* Node);
int32 RemapBlueprintGraphReferences(UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements);
void AddPathReplacement(TArray<FPathStringReplacement>& Replacements, const FString& Source, const FString& Target);
void AddObjectPathReplacements(TArray<FPathStringReplacement>& Replacements, UObject* SourceObject, UObject* TargetObject);
FString ApplyPathReplacements(FString Text, const TArray<FPathStringReplacement>& Replacements);
int32 ReplaceExportedPropertyPaths(UObject* Object, const TArray<FPathStringReplacement>& Replacements, const FString& SourceRoot);
int32 CollectOriginalDependencyPackages(
    IAssetRegistry& AssetRegistry,
    const TArray<FName>& TargetPackageNames,
    const FString& SourceRoot,
    TSet<FName>& OutPackagesWithOriginalDependencies,
    TArray<FString>& OutSamples,
    int32 MaxSamples);
FString BuildTargetAssetPath(const FAssetData& SourceAsset, const FString& SourceRoot, const FString& TargetRoot, const FString& Suffix);
int32 GetRecreateCreationPriority(const FAssetData& SourceAsset);
FString GetFallbackReason(const FAssetData& SourceAsset);
bool IsFreshPropertyCopyAssetClassName(const FString& ClassName);
FString GetFreshPropertyCopyReason(const FAssetData& SourceAsset);
UObject* CreateFreshPropertyCopyAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    UClass* TargetClass,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
void DiscardFreshAssetForFallback(UObject* Object);
bool IsFreshMaterialGraphAssetClassName(const FString& ClassName);
TConstArrayView<TObjectPtr<UMaterialExpression>> GetMaterialGraphExpressions(UObject* GraphObject);
FMaterialExpressionCollection* GetMutableMaterialGraphExpressionCollection(UObject* GraphObject);
const FMaterialExpressionCollection* GetMaterialGraphExpressionCollection(UObject* GraphObject);
bool ValidateFreshMaterialFunctionAsset(UMaterialFunction* SourceFunction, UMaterialFunction* TargetFunction, FString& OutDetail);
bool ValidateFreshMaterialGraphAsset(UObject* SourceObject, UObject* TargetObject, FString& OutDetail, int32& OutExpressionCount);
int32 AddMaterialGraphExpressionReplacementMappings(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject);
void CopyFreshMaterialFunctionSettings(UMaterialFunction* SourceFunction, UMaterialFunction* TargetFunction);
bool CloneMaterialExpressionsIntoFreshGraph(UObject* SourceObject, UObject* TargetObject, FString& OutDetail);
UObject* CreateFreshMaterialGraphAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
bool ValidateFreshSoundWaveAsset(USoundWave* SourceSound, USoundWave* TargetSound, FString& OutDetail);
UObject* CreateFreshSoundWaveAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
bool ValidateFreshTextureSourceCopy(
    UTexture* SourceTexture,
    UTexture* TargetTexture,
    const FString& DetailPrefix,
    FString& OutDetail);
UObject* CreateFreshTextureCubeAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
UObject* CreateFreshVolumeTextureAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
UObject* CreateFreshTexture2DAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
bool ArePerPlatformFloatsEqualForMCP(const FPerPlatformFloat& A, const FPerPlatformFloat& B);
UObject* ResolveReplacementOrOriginalForMCP(const TMap<UObject*, UObject*>* OptionalReplacementMap, UObject* SourceObject);
UMaterialInterface* ResolveMaterialReplacementForMCP(
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    UMaterialInterface* SourceMaterial);
UStaticMesh* ResolveStaticMeshReplacementForMCP(
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    UStaticMesh* SourceMesh);
void FinishCompilationForObjectForMCP(UObject* Object);
bool CompareStaticMeshDescriptionShapeForMCP(
    const FMeshDescription& SourceDescription,
    const FMeshDescription& TargetDescription,
    int32 LodIndex,
    FString& OutDetail);
bool GetStaticMeshDescriptionForFreshCopyForMCP(
    UStaticMesh* Mesh,
    int32 LodIndex,
    FMeshDescription& OutDescription,
    bool& bOutUsedRenderExport,
    FString& OutDetail);
bool CompareStaticMeshRenderStatsForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail);
bool CompareStaticMeshSectionInfoMapForMCP(
    const FMeshSectionInfoMap& SourceMap,
    const FMeshSectionInfoMap& TargetMap,
    const TCHAR* Label,
    FString& OutDetail);
bool CompareStaticMeshMaterialsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
bool CompareStaticMeshSocketsForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail);
int32 GetBodySetupCollisionElementCountForMCP(const UBodySetup* BodySetup);
bool CompareStaticMeshBodySetupForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail);
bool ValidateFreshStaticMeshAsset(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
void CopyStaticMeshMaterialsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap);
bool CopyStaticMeshBodySetupForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail);
bool CopyStaticMeshSocketsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
UObject* CreateFreshStaticMeshAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail);
bool ValidateFreshDataTableAsset(
    UDataTable* SourceTable,
    UDataTable* TargetTable,
    const UScriptStruct* ExpectedTargetRowStruct,
    FString& OutDetail);
UObject* CreateFreshDataTableAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const FString& SourceRoot,
    FString& OutDetail);
void CopyEnumMetaDataValue(const UEnum* SourceEnum, UEnum* TargetEnum, const TCHAR* Key, int32 NameIndex = INDEX_NONE);
UObject* CreateFreshUserDefinedEnumAsset(UObject* SourceObject, const FString& TargetPath, FString& OutDetail);
UObject* RemapObjectForFreshUserDefinedStruct(UObject* SourceObject, const TMap<UObject*, UObject*>& ReplacementMap);
bool RemapFreshStructVariableDescription(FStructVariableDescription& Description, const TMap<UObject*, UObject*>& ReplacementMap);
UObject* CreateFreshUserDefinedStructAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    const TMap<UObject*, UObject*>& ReplacementMap,
    FString& OutDetail);
int32 RemapUserDefinedStructDescriptionPaths(
    const TArray<UUserDefinedStruct*>& Structs,
    const TArray<FPathStringReplacement>& PathReplacements,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const FString& SourceRoot);
int32 CollectLoadedPackagesUnderRoot(const FString& Root, TArray<UPackage*>& OutPackages, TArray<FString>& OutSamples, int32 MaxSamples = 20);
bool DeleteTargetRootForRecreate(
    const FString& TargetRoot,
    FString& OutDeleteMode,
    TArray<FString>& OutWarnings);
TSharedPtr<FJsonObject> AssetReportToJson(
    const FAssetData& SourceAsset,
    const FString& TargetPath,
    const FString& Strategy,
    const FString& Status,
    const FString& Detail);
void AddFallbackRuleDetailTokensForMCP(const TSharedPtr<FJsonObject>& RuleObject, TArray<FString>& OutTokens);
TArray<FAcceptedFallbackRule> ParseAcceptedFallbackRulesForMCP(const TSharedPtr<FJsonObject>& Params);
bool AcceptedFallbackRuleMatchesForMCP(
    const FAcceptedFallbackRule& Rule,
    const FString& SourcePath,
    const FString& ClassName,
    const FString& FreshCreateDetail);
FString GetAcceptedFreshCreateFallbackReasonForMCP(
    const FAssetData& SourceAsset,
    const FString& FreshCreateDetail,
    const TArray<FAcceptedFallbackRule>& AcceptedFallbackRules);
void ClassifyFreshCreateFallbackForMCP(
    const FAssetData& SourceAsset,
    const FString& TargetPath,
    const FString& FreshCreateDetail,
    const TArray<FAcceptedFallbackRule>& AcceptedFallbackRules,
    TSharedPtr<FJsonObject> AssetReport,
    int32& AcceptedFallbackCount,
    int32& UnresolvedFallbackCount,
    TArray<FString>& AcceptedFallbackSamples,
    TArray<FString>& UnresolvedFallbackSamples);
void AddStringArrayField(TSharedPtr<FJsonObject> Object, const FString& FieldName, const TArray<FString>& Values);
FString MakeSafeReportFilenameStemForMCP(FString Stem);
FString BuildMCPReportFilename(const FString& PackagePathOrRoot, const FString& OperationName);
FString SaveJsonReportForMCP(const TSharedPtr<FJsonObject>& ReportObject, const FString& ReportFilename);
TSharedPtr<FJsonObject> CloneJsonObjectForMCP(const TSharedPtr<FJsonObject>& Source);
void SetBoolFieldIfMissingForMCP(TSharedPtr<FJsonObject> Object, const FString& FieldName, bool bValue);
void SetNumberFieldIfMissingForMCP(TSharedPtr<FJsonObject> Object, const FString& FieldName, double Value);
bool GetJsonBoolFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool bDefaultValue = false);
int32 GetJsonIntFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 DefaultValue = 0);
FString GetJsonStringFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName);
bool PipelineStepResultPassesForMCP(const TSharedPtr<FJsonObject>& Result);
TSharedPtr<FJsonObject> MakePipelineStepSummaryForMCP(
    const FString& StepName,
    bool bRan,
    const TSharedPtr<FJsonObject>& Result,
    const FString& SkippedReason = FString());
void CopyNumberFieldIfPresentForMCP(
    const TSharedPtr<FJsonObject>& Source,
    TSharedPtr<FJsonObject> Target,
    const FString& SourceFieldName,
    const FString& TargetFieldName);
FString GetEditorLogPath();
int32 CountLogLines(const FString& Text);
FEditorLogMarker CaptureEditorLogMarker(bool bEnabled);
void AddEditorLogIssueSample(FEditorLogHealthResult& Result, const TArray<FString>& Lines, int32 LineIndex, const FString& Type);
bool IsBenignBlueprintUberGraphFrameEnsure(const TArray<FString>& Lines, int32 LineIndex, const FString& TargetRoot);
FEditorLogHealthResult ScanEditorLogSince(const FEditorLogMarker& Marker, const FString& BenignBlueprintEnsureRoot = FString());
void AddEditorLogHealthFields(TSharedPtr<FJsonObject> Object, const FEditorLogHealthResult& Result);
TSharedPtr<FJsonObject> HandleRecreateContentFolderMCP(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandlePostprocessContentFolderMCP(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleRepairWorldActorInstancesMCP(const TSharedPtr<FJsonObject>& Params);
TSharedPtr<FJsonObject> HandleRunContentValidationPipelineMCP(const TSharedPtr<FJsonObject>& Params);


FString NormalizeContentRoot(FString Path)
{

    Path.TrimStartAndEndInline();
    while (Path.Len() > 1 && Path.EndsWith(TEXT("/")))
    {
        Path.LeftChopInline(1);
    }
    return Path;
}



FString NormalizePackagePathParam(FString Path)
{

    Path.TrimStartAndEndInline();
    Path.TrimQuotesInline();

    FString LongPackageName;
    if (FPackageName::TryConvertFilenameToLongPackageName(Path, LongPackageName))
    {
        return NormalizeContentRoot(LongPackageName);
    }

    if (Path.Contains(TEXT(".")))
    {
        Path = FPackageName::ObjectPathToPackageName(Path);
    }
    return NormalizeContentRoot(Path);
}



UWorld* LoadWorldPackageForRepair(const FString& WorldPackageName)
{
    if (!Before(TEXT("load_world_package"), false)) return nullptr;
    const FString WorldObjectPath = WorldPackageName + TEXT(".") + FPackageName::GetShortName(WorldPackageName);
    UWorld* LoadedWorld = LoadObject<UWorld>(nullptr, *WorldObjectPath, nullptr, LOAD_None, nullptr);
    if (!After()) return nullptr;
    if (!LoadedWorld)
    {
        if (!Before(TEXT("load_world_package_fallback"), false)) return nullptr;
        UPackage* P = LoadPackage(nullptr, *WorldPackageName, LOAD_None);
        if (!After()) return nullptr;
        LoadedWorld = P ? UWorld::FindWorldInPackage(P) : nullptr;
    }
    if (LoadedWorld && !Current->Track(LoadedWorld)) return nullptr;
    return LoadedWorld;
}



bool SaveWorldPackageForRepair(UWorld* LoadedWorld, const FString& WorldPackageName)
{
    if (!Check() || !Live(LoadedWorld)) return false;
    FString Filename;
    if (!LoadedWorld->GetPackage()->GetName().Equals(WorldPackageName, ESearchCase::CaseSensitive) ||
        !FPackageName::TryConvertLongPackageNameToFilename(WorldPackageName, Filename, FPackageName::GetMapPackageExtension())) return false;
    return SaveExactForGD(LoadedWorld->GetPackage(), LoadedWorld, Filename, RF_Standalone);
}



bool SaveLoadedAssetForMCP(UObject* Asset)
{
    if (!Check() || !Live(Asset)) return false;
    UPackage* P = Asset->GetPackage();
    if (!Live(P) || P == GetTransientPackage()) return false;
    UObject* Top = Asset;
    UWorld* World = Cast<UWorld>(Asset);
    if (!World) World = UWorld::FindWorldInPackage(P);
    if (World) Top = World;
    FString Filename;
    if (!FPackageName::TryConvertLongPackageNameToFilename(P->GetName(), Filename, World ? FPackageName::GetMapPackageExtension() : FPackageName::GetAssetPackageExtension())) return false;
    return SaveExactForGD(P, Top, Filename, RF_Public | RF_Standalone);
}



FString GetStringParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, const FString& DefaultValue)
{

    FString Value;
    if (Params.IsValid() && Params->TryGetStringField(FieldName, Value) && !Value.IsEmpty())
    {
        return Value;
    }
    return DefaultValue;
}



bool GetBoolParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, bool bDefaultValue)
{

    bool bValue = bDefaultValue;
    if (Params.IsValid() && Params->HasTypedField<EJson::Boolean>(FieldName))
    {
        bValue = Params->GetBoolField(FieldName);
    }
    return bValue;
}



int32 GetIntParam(const TSharedPtr<FJsonObject>& Params, const TCHAR* FieldName, int32 DefaultValue)
{

    double Value = DefaultValue;
    if (Params.IsValid() && Params->TryGetNumberField(FieldName, Value))
    {
        if (!FMath::IsFinite(Value) || Value < MIN_int32 || Value > MAX_int32) { Current->Fail(TEXT("numeric_range"), TEXT("Original rounded int32 conversion exceeds its finite range")); return 0; }
        return FMath::Max(0, CheckedI32(static_cast<double>(FMath::RoundToInt64(Value))));
    }
    return DefaultValue;
}



bool TryGetRequiredStringParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutValue,
    FString& OutError)
{
    if (!Check()) return {};

    FString Value;
    if (!Params.IsValid() || !Params->TryGetStringField(FieldName, Value))
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    Value.TrimStartAndEndInline();
    if (Value.IsEmpty())
    {
        OutError = FString::Printf(TEXT("Missing required parameter: %s"), FieldName);
        return false;
    }

    OutValue = Value;
    return true;
}



bool TryGetRequiredContentRootParamForMCP(
    const TSharedPtr<FJsonObject>& Params,
    const TCHAR* FieldName,
    FString& OutRoot,
    FString& OutError)
{
    if (!Check()) return {};

    FString RawValue;
    if (!TryGetRequiredStringParamForMCP(Params, FieldName, RawValue, OutError))
    {
        return false;
    }

    OutRoot = NormalizeContentRoot(RawValue);
    if (!FPackageName::IsValidLongPackageName(OutRoot) || !OutRoot.StartsWith(TEXT("/Game/")))
    {
        OutError = FString::Printf(TEXT("Invalid %s: %s"), FieldName, *OutRoot);
        return false;
    }

    return true;
}



bool ValidateContentRootPairForMCP(const FString& SourceRoot, const FString& TargetRoot, FString& OutError)
{
    if (!Check()) return {};

    if (TargetRoot.Equals(SourceRoot) || TargetRoot.StartsWith(SourceRoot + TEXT("/")))
    {
        OutError = TEXT("target_root must not be the source root or a child of source_root");
        return false;
    }
    return true;
}



bool IsLiveObjectForMCP(const UObject* Object)
{
    if (!Check()) return {};

    if (!Object || !IsValid(Object))
    {
        return false;
    }

    if (Object->HasAnyFlags(RF_BeginDestroyed | RF_FinishDestroyed)
        || Object->HasAnyInternalFlags(EInternalObjectFlags::Garbage | EInternalObjectFlags::Unreachable))
    {
        return false;
    }

    return Object->GetClass() != nullptr && Object->GetOutermost() != nullptr;
}



bool IsPackageUnderRoot(const UObject* Object, const FString& Root)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object))
    {
        return false;
    }

    const UPackage* Package = Object->GetOutermost();
    if (!Package)
    {
        return false;
    }

    const FString PackageName = Package->GetName();
    return PackageName.Equals(Root) || PackageName.StartsWith(Root + TEXT("/"));
}



bool IsGeneratedClassInstanceFromRoot(const UObject* Object, const FString& Root)
{
    if (!Check()) return {};

    const UBlueprintGeneratedClass* GeneratedClass = IsLiveObjectForMCP(Object) ? Cast<UBlueprintGeneratedClass>(Object->GetClass()) : nullptr;
    const UBlueprint* ClassBlueprint = GeneratedClass ? Cast<UBlueprint>(GeneratedClass->ClassGeneratedBy.Get()) : nullptr;
    return ClassBlueprint && IsPackageUnderRoot(ClassBlueprint, Root);
}



bool ShouldSkipArchiveObjectReferenceRemap(const UObject* Object)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object))
    {
        return true;
    }

    // Blueprint CDOs validate their UberGraphFrame while serialized. Direct archive
    // replacement can trip that validation before the Blueprint compile pass repairs it.
    return Object->HasAnyFlags(RF_ClassDefaultObject) && Cast<UBlueprintGeneratedClass>(Object->GetClass()) != nullptr;
}



bool ShouldSkipAssetArchiveObjectReferenceRemap(const UObject* Object)
{
    if (!Check()) return {};

    if (ShouldSkipArchiveObjectReferenceRemap(Object))
    {
        return true;
    }

    if (Object->IsA<UBlueprint>())
    {
        return true;
    }

    return Cast<UBlueprintGeneratedClass>(Object->GetClass()) != nullptr;
}



int32 GetBlueprintInheritanceDepth(
    const UBlueprint* Blueprint,
    const TMap<UClass*, const UBlueprint*>& BlueprintByGeneratedClass,
    TMap<const UBlueprint*, int32>& DepthByBlueprint,
    TSet<const UBlueprint*>& VisitingBlueprints)
{
    if (!Check()) return {};

    if (!Blueprint)
    {
        return 0;
    }

    if (const int32* ExistingDepth = DepthByBlueprint.Find(Blueprint))
    {
        return *ExistingDepth;
    }

    if (VisitingBlueprints.Contains(Blueprint))
    {
        return 0;
    }

    VisitingBlueprints.Add(Blueprint);

    int32 Depth = 0;
    if (const UBlueprint* ParentBlueprint = BlueprintByGeneratedClass.FindRef(Blueprint->ParentClass))
    {
        if (ParentBlueprint != Blueprint)
        {
            Depth = GetBlueprintInheritanceDepth(ParentBlueprint, BlueprintByGeneratedClass, DepthByBlueprint, VisitingBlueprints) + 1;
        }
    }

    VisitingBlueprints.Remove(Blueprint);
    DepthByBlueprint.Add(Blueprint, Depth);
    return Depth;
}



TArray<UBlueprint*> SortBlueprintsByInheritance(const TArray<UBlueprint*>& Blueprints)
{
    if (!Check()) return {};

    TArray<UBlueprint*> OrderedBlueprints;
    TSet<UBlueprint*> AddedBlueprints;
    for (UBlueprint* Blueprint : Blueprints)
    {
        if (Blueprint && !AddedBlueprints.Contains(Blueprint))
        {
            OrderedBlueprints.Add(Blueprint);
            AddedBlueprints.Add(Blueprint);
        }
    }

    TMap<UClass*, const UBlueprint*> BlueprintByGeneratedClass;
    for (const UBlueprint* Blueprint : OrderedBlueprints)
    {
        if (!Blueprint)
        {
            continue;
        }
        if (Blueprint->GeneratedClass)
        {
            BlueprintByGeneratedClass.Add(Blueprint->GeneratedClass, Blueprint);
        }
        if (Blueprint->SkeletonGeneratedClass)
        {
            BlueprintByGeneratedClass.Add(Blueprint->SkeletonGeneratedClass, Blueprint);
        }
    }

    TMap<const UBlueprint*, int32> DepthByBlueprint;
    TSet<const UBlueprint*> VisitingBlueprints;
    for (const UBlueprint* Blueprint : OrderedBlueprints)
    {
        GetBlueprintInheritanceDepth(Blueprint, BlueprintByGeneratedClass, DepthByBlueprint, VisitingBlueprints);
    }

    Algo::Sort(OrderedBlueprints, [&DepthByBlueprint](const UBlueprint* A, const UBlueprint* B)
    {
        const int32 ADepth = DepthByBlueprint.FindRef(A);
        const int32 BDepth = DepthByBlueprint.FindRef(B);
        if (ADepth != BDepth)
        {
            return ADepth < BDepth;
        }

        const FString APath = A ? A->GetPathName() : FString();
        const FString BPath = B ? B->GetPathName() : FString();
        return APath < BPath;
    });

    return OrderedBlueprints;
}



int32 CompileBlueprintsForMCP(
    const TArray<UBlueprint*>& Blueprints,
    int32 CompilePasses,
    bool bRefreshBlueprintNodes,
    bool bRefreshFailedBlueprintNodes,
    int32& OutCompileErrorCount,
    TArray<FString>* OutFailedAssets)
{
    if (!Check()) return {};

    OutCompileErrorCount = 0;

    const TArray<UBlueprint*> OrderedBlueprints = SortBlueprintsByInheritance(Blueprints);
    TSet<UBlueprint*> FinalErrorBlueprints;
    TSet<UBlueprint*> BlueprintsToRefreshThisPass;
    int32 CompiledCount = 0;

    for (int32 PassIndex = 0; PassIndex < CompilePasses; ++PassIndex)
    {
        FinalErrorBlueprints.Reset();
        for (UBlueprint* Blueprint : OrderedBlueprints)
        {
            if (!Blueprint)
            {
                continue;
            }

            if (bRefreshBlueprintNodes || BlueprintsToRefreshThisPass.Contains(Blueprint))
            {
                if (!Write(Blueprint, TEXT("FBlueprintEditorUtils_RefreshAllNodes"))) return {};
                FBlueprintEditorUtils::RefreshAllNodes(Blueprint);
                if (!After()) return {};
            }
            if (!Write(Blueprint, TEXT("FBlueprintEditorUtils_MarkBlueprintAsModified"))) return {};
            FBlueprintEditorUtils::MarkBlueprintAsModified(Blueprint);
            if (!After()) return {};
            if (!Check()) return {};
            CompileOwnedBlueprint(Blueprint);
            if (!Check()) return {};
            ++CompiledCount;
            if (Blueprint->Status == BS_Error)
            {
                FinalErrorBlueprints.Add(Blueprint);
            }
        }

        BlueprintsToRefreshThisPass.Reset();
        if (bRefreshFailedBlueprintNodes)
        {
            BlueprintsToRefreshThisPass = FinalErrorBlueprints;
        }
    }

    OutCompileErrorCount = FinalErrorBlueprints.Num();
    if (OutFailedAssets)
    {
        for (UBlueprint* ErrorBlueprint : FinalErrorBlueprints)
        {
            if (ErrorBlueprint)
            {
                OutFailedAssets->Add(ErrorBlueprint->GetPathName());
            }
        }
    }

    return CompiledCount;
}



int32 CompileUserDefinedStructsForMCP(const TArray<UUserDefinedStruct*>& Structs, TArray<FString>* OutFailedAssets)
{
    if (!Check()) return {};

    TSet<UUserDefinedStruct*> UniqueStructsToCompile;
    for (UUserDefinedStruct* Struct : Structs)
    {
        if (Struct)
        {
            UniqueStructsToCompile.Add(Struct);
        }
    }

    int32 CompileErrorCount = 0;
    for (UUserDefinedStruct* Struct : UniqueStructsToCompile)
    {
        Struct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
        if (!Check()) return {};
        CompileOwnedStructure(Struct);
        if (!Check()) return {};
        if (!Write(Struct, TEXT("FStructureEditorUtils_RecreateDefaultInstanceInEditorData"))) return {};
        FStructureEditorUtils::RecreateDefaultInstanceInEditorData(Struct);
        if (!After()) return {};
        if (!Write(Struct, TEXT("UpdateStructFlags"))) return {};
        Struct->UpdateStructFlags();
        if (!After()) return {};
        if (Struct->Status == EUserDefinedStructureStatus::UDSS_Error)
        {
            ++CompileErrorCount;
            if (OutFailedAssets)
            {
                OutFailedAssets->Add(Struct->GetPathName());
            }
        }
        else
        {
            if (!Write(Struct, TEXT("MarkPackageDirty"))) return {};
            Struct->MarkPackageDirty();
            if (!After()) return {};
            if (UPackage* Package = Struct->GetOutermost())
            {
                if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
                Package->MarkPackageDirty();
                if (!After()) return {};
            }
        }
    }

    return CompileErrorCount;
}



void AddReplacementMapping(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject)
{
    if (!Check()) return;
    if (SourceObject && !Current->Track(SourceObject)) return;
    if (TargetObject && !Current->Track(TargetObject)) return;

    if (SourceObject && TargetObject && SourceObject != TargetObject)
    {
        ReplacementMap.Add(SourceObject, TargetObject);
    }
}



void AddTargetWorldPackageName(TArray<FString>& TargetWorldPackageNames, UObject* TargetObject)
{
    if (!Check()) return;

    if (UWorld* TargetWorld = Cast<UWorld>(TargetObject))
    {
        TargetWorldPackageNames.AddUnique(TargetWorld->GetOutermost()->GetName());
    }
}



void AddBlueprintReplacementMappings(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject)
{
    if (!Check()) return;

    UBlueprint* SourceBlueprint = Cast<UBlueprint>(SourceObject);
    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject);
    if (!SourceBlueprint || !TargetBlueprint)
    {
        return;
    }

    AddReplacementMapping(ReplacementMap, SourceBlueprint->GeneratedClass, TargetBlueprint->GeneratedClass);
    AddReplacementMapping(ReplacementMap, SourceBlueprint->SkeletonGeneratedClass, TargetBlueprint->SkeletonGeneratedClass);

    if (SourceBlueprint->GeneratedClass && TargetBlueprint->GeneratedClass)
    {
        AddReplacementMapping(
            ReplacementMap,
            SourceBlueprint->GeneratedClass->GetDefaultObject(false),
            TargetBlueprint->GeneratedClass->GetDefaultObject(false));
    }

    if (SourceBlueprint->SkeletonGeneratedClass && TargetBlueprint->SkeletonGeneratedClass)
    {
        AddReplacementMapping(
            ReplacementMap,
            SourceBlueprint->SkeletonGeneratedClass->GetDefaultObject(false),
            TargetBlueprint->SkeletonGeneratedClass->GetDefaultObject(false));
    }

    AddReplacementMapping(ReplacementMap, SourceBlueprint->SimpleConstructionScript, TargetBlueprint->SimpleConstructionScript);
    if (SourceBlueprint->SimpleConstructionScript && TargetBlueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& SourceNodes = SourceBlueprint->SimpleConstructionScript->GetAllNodes();
        const TArray<USCS_Node*>& TargetNodes = TargetBlueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* SourceNode : SourceNodes)
        {
            if (!SourceNode)
            {
                continue;
            }

            for (USCS_Node* TargetNode : TargetNodes)
            {
                if (TargetNode && TargetNode->GetVariableName() == SourceNode->GetVariableName())
                {
                    AddReplacementMapping(ReplacementMap, SourceNode, TargetNode);
                    AddReplacementMapping(ReplacementMap, SourceNode->ComponentTemplate, TargetNode->ComponentTemplate);
                    break;
                }
            }
        }
    }
}



UActorComponent* DuplicateComponentTemplateForTargetBlueprint(
    USCS_Node* SourceNode,
    UBlueprint* TargetBlueprint,
    UClass* TargetComponentClass,
    const FName TargetVariableName)
{
    if (!Check()) return {};

    if (!SourceNode || !TargetBlueprint || !TargetBlueprint->GeneratedClass || !TargetComponentClass ||
        !TargetComponentClass->IsChildOf(UActorComponent::StaticClass()) || TargetVariableName.IsNone())
    {
        return nullptr;
    }

    UObject* TemplateOuter = TargetBlueprint->GeneratedClass;
    const FString TemplateName = TargetVariableName.ToString() + USimpleConstructionScript::ComponentTemplateNameSuffix;
    if (UObject* ExistingTemplate = FindObject<UObject>(TemplateOuter, *TemplateName))
    {
        if (!Check()) return {};
        MoveOwnedObject(ExistingTemplate, nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);
        if (!Check()) return {};
    }

    UActorComponent* NewTemplate = nullptr;
    if (SourceNode->ComponentTemplate)
    {
        const UClass* SourceTemplateClass = SourceNode->ComponentTemplate->GetClass();
        if (!SourceTemplateClass || TargetComponentClass->GetPropertiesSize() >= SourceTemplateClass->GetPropertiesSize())
        {
            FObjectDuplicationParameters DuplicationParameters(SourceNode->ComponentTemplate, TemplateOuter);
            DuplicationParameters.DestName = FName(*TemplateName);
            DuplicationParameters.DestClass = TargetComponentClass;
            DuplicationParameters.ApplyFlags = RF_ArchetypeObject | RF_Public | RF_Transactional;
            if (!Check()) return {};
            NewTemplate = Cast<UActorComponent>(CreateOwnedFactory(DuplicationParameters.DestOuter, DuplicationParameters.DestClass, DuplicationParameters.DestName, TEXT("StaticDuplicateObjectEx"), [&]() { return StaticDuplicateObjectEx(DuplicationParameters); }));
            if (!Check()) return {};
        }
    }

    if (!NewTemplate)
    {
        NewTemplate = NewOwnedObject<UActorComponent>(
            TemplateOuter,
            TargetComponentClass,
            FName(*TemplateName),
            RF_ArchetypeObject | RF_Public | RF_Transactional);
    }

    if (NewTemplate)
    {
        if (!Write(NewTemplate, TEXT("SetFlags"))) return {};
        NewTemplate->SetFlags(RF_ArchetypeObject | RF_Public | RF_Transactional);
        if (!After()) return {};
        if (!Write(NewTemplate, TEXT("ClearFlags"))) return {};
        NewTemplate->ClearFlags(RF_Transient);
        if (!After()) return {};
    }
    return NewTemplate;
}



int32 RepairBlueprintSCSNodes(UBlueprint* SourceBlueprint, UBlueprint* TargetBlueprint, TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!SourceBlueprint || !TargetBlueprint || !SourceBlueprint->SimpleConstructionScript || !TargetBlueprint->SimpleConstructionScript)
    {
        return 0;
    }

    int32 RepairCount = 0;
    USimpleConstructionScript* SourceSCS = SourceBlueprint->SimpleConstructionScript;
    USimpleConstructionScript* TargetSCS = TargetBlueprint->SimpleConstructionScript;

    TMap<FName, USCS_Node*> TargetNodesByName;
    const TArray<USCS_Node*>& TargetNodes = TargetSCS->GetAllNodes();
    for (USCS_Node* TargetNode : TargetNodes)
    {
        if (TargetNode && !TargetNode->GetVariableName().IsNone())
        {
            TargetNodesByName.FindOrAdd(TargetNode->GetVariableName(), TargetNode);
        }
    }

    const TArray<USCS_Node*>& SourceNodes = SourceSCS->GetAllNodes();
    for (USCS_Node* SourceNode : SourceNodes)
    {
        if (!SourceNode || SourceNode->GetVariableName().IsNone() || !SourceNode->ComponentClass)
        {
            continue;
        }

        UClass* TargetComponentClass = Cast<UClass>(FindReplacementObject(ReplacementMap, SourceNode->ComponentClass));
        if (!TargetComponentClass)
        {
            TargetComponentClass = SourceNode->ComponentClass;
        }
        if (!TargetComponentClass || !TargetComponentClass->IsChildOf(UActorComponent::StaticClass()))
        {
            continue;
        }

        const FName VariableName = SourceNode->GetVariableName();
        USCS_Node* TargetNode = TargetNodesByName.FindRef(VariableName);
        if (TargetNode)
        {
            AddReplacementMapping(ReplacementMap, SourceNode, TargetNode);
            AddReplacementMapping(ReplacementMap, SourceNode->ComponentTemplate, TargetNode->ComponentTemplate);

            if (TargetNode->ComponentClass != TargetComponentClass)
            {
                if (UActorComponent* NewTemplate = DuplicateComponentTemplateForTargetBlueprint(SourceNode, TargetBlueprint, TargetComponentClass, VariableName))
                {
                    if (!Write(TargetNode, TEXT("Modify"))) return {};
                    TargetNode->Modify();
                    if (!After()) return {};
                    TargetNode->ComponentClass = TargetComponentClass;
                    TargetNode->ComponentTemplate = NewTemplate;
                    TargetNode->VariableGuid = SourceNode->VariableGuid.IsValid() ? SourceNode->VariableGuid : TargetNode->VariableGuid;
                    TargetNode->CategoryName = SourceNode->CategoryName;
                    TargetNode->MetaDataArray = SourceNode->MetaDataArray;
                    TargetNode->AttachToName = SourceNode->AttachToName;
                    AddReplacementMapping(ReplacementMap, SourceNode->ComponentTemplate, NewTemplate);
                    ++RepairCount;
                }
            }
            continue;
        }

        UActorComponent* NewTemplate = DuplicateComponentTemplateForTargetBlueprint(SourceNode, TargetBlueprint, TargetComponentClass, VariableName);
        if (!NewTemplate)
        {
            continue;
        }

        USCS_Node* NewNode = TargetSCS->CreateNodeAndRenameComponent(NewTemplate);
        if (!NewNode)
        {
            continue;
        }

        if (!Write(NewNode, TEXT("Modify"))) return {};
        NewNode->Modify();
        if (!After()) return {};
        NewNode->ComponentClass = TargetComponentClass;
        NewNode->VariableGuid = SourceNode->VariableGuid.IsValid() ? SourceNode->VariableGuid : NewNode->VariableGuid;
        NewNode->CategoryName = SourceNode->CategoryName;
        NewNode->MetaDataArray = SourceNode->MetaDataArray;
        NewNode->AttachToName = SourceNode->AttachToName;
        if (!Write(NewNode, TEXT("SetVariableName"))) return {};
        NewNode->SetVariableName(VariableName, true);
        if (!After()) return {};

        USCS_Node* SourceParentNode = SourceSCS->FindParentNode(SourceNode);
        USCS_Node* TargetParentNode = SourceParentNode ? TargetNodesByName.FindRef(SourceParentNode->GetVariableName()) : nullptr;
        if (TargetParentNode)
        {
            if (!Write(NewNode, TEXT("SetParent"))) return {};
            NewNode->SetParent(TargetParentNode);
            if (!After()) return {};
            if (!Write(TargetParentNode, TEXT("AddChildNode"))) return {};
            TargetParentNode->AddChildNode(NewNode);
            if (!After()) return {};
        }
        else
        {
            if (!Write(TargetSCS, TEXT("AddNode"))) return {};
            TargetSCS->AddNode(NewNode);
            if (!After()) return {};
        }

        TargetNodesByName.Add(VariableName, NewNode);
        AddReplacementMapping(ReplacementMap, SourceNode, NewNode);
        AddReplacementMapping(ReplacementMap, SourceNode->ComponentTemplate, NewNode->ComponentTemplate);
        ++RepairCount;
    }

    if (RepairCount > 0)
    {
        if (!Write(TargetSCS, TEXT("Modify"))) return {};
        TargetSCS->Modify();
        if (!After()) return {};
        if (!Write(TargetSCS, TEXT("CreateNameToSCSNodeMap"))) return {};
        TargetSCS->CreateNameToSCSNodeMap();
        if (!After()) return {};
        TargetSCS->ValidateSceneRootNodes();
        if (!Write(TargetBlueprint, TEXT("FBlueprintEditorUtils_MarkBlueprintAsStructurallyModified"))) return {};
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(TargetBlueprint);
        if (!After()) return {};
        if (!Write(TargetBlueprint, TEXT("MarkPackageDirty"))) return {};
        TargetBlueprint->MarkPackageDirty();
        if (!After()) return {};
    }

    return RepairCount;
}



UWidget* DuplicateWidgetForTargetClass(UWidget* SourceWidget, UWidgetTree* TargetWidgetTree, UClass* TargetWidgetClass)
{
    if (!Check()) return {};

    if (!SourceWidget || !TargetWidgetTree || !TargetWidgetClass || !TargetWidgetClass->IsChildOf(UWidget::StaticClass()))
    {
        return nullptr;
    }

    const UClass* SourceWidgetClass = SourceWidget->GetClass();
    if (SourceWidgetClass && TargetWidgetClass->GetPropertiesSize() < SourceWidgetClass->GetPropertiesSize())
    {
        return nullptr;
    }

    const FName WidgetName = SourceWidget->GetFName();
    const FName TransientName = MakeUniqueObjectName(GetTransientPackage(), SourceWidget->GetClass(), WidgetName);
    if (!Check()) return {};
    MoveOwnedObject(SourceWidget, *TransientName.ToString(), GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);
    if (!Check()) return {};

    FObjectDuplicationParameters DuplicationParameters(SourceWidget, TargetWidgetTree);
    DuplicationParameters.DestName = WidgetName;
    DuplicationParameters.DestClass = TargetWidgetClass;
    DuplicationParameters.ApplyFlags = RF_Transactional;
    if (!Check()) return {};
    UWidget* NewWidget = Cast<UWidget>(CreateOwnedFactory(DuplicationParameters.DestOuter, DuplicationParameters.DestClass, DuplicationParameters.DestName, TEXT("StaticDuplicateObjectEx"), [&]() { return StaticDuplicateObjectEx(DuplicationParameters); }));
    if (!Check()) return {};
    if (!NewWidget)
    {
        if (!Check()) return {};
        MoveOwnedObject(SourceWidget, *WidgetName.ToString(), TargetWidgetTree, REN_DoNotDirty | REN_DontCreateRedirectors);
        if (!Check()) return {};
        return nullptr;
    }

    if (!Write(NewWidget, TEXT("SetFlags"))) return {};
    NewWidget->SetFlags(RF_Transactional);
    if (!After()) return {};
    NewWidget->bIsVariable = SourceWidget->bIsVariable;
    return NewWidget;
}



int32 RepairWidgetBlueprintTree(UBlueprint* TargetBlueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    UWidgetBlueprint* WidgetBlueprint = Cast<UWidgetBlueprint>(TargetBlueprint);
    if (!WidgetBlueprint || !WidgetBlueprint->WidgetTree)
    {
        return 0;
    }

    TArray<UWidget*> Widgets;
    WidgetBlueprint->WidgetTree->GetAllWidgets(Widgets);
    TMap<UObject*, UObject*> WidgetReplacementMap;
    int32 RepairCount = 0;

    for (UWidget* Widget : Widgets)
    {
        if (!Widget)
        {
            continue;
        }

        UClass* TargetWidgetClass = Cast<UClass>(FindReplacementObject(ReplacementMap, Widget->GetClass()));
        if (!TargetWidgetClass || TargetWidgetClass == Widget->GetClass() || !TargetWidgetClass->IsChildOf(UWidget::StaticClass()))
        {
            continue;
        }

        if (UWidget* NewWidget = DuplicateWidgetForTargetClass(Widget, WidgetBlueprint->WidgetTree, TargetWidgetClass))
        {
#if WITH_EDITORONLY_DATA
            NewWidget->WidgetGeneratedBy = WidgetBlueprint;
#endif
            WidgetReplacementMap.Add(Widget, NewWidget);
            ++RepairCount;
        }
    }

    if (WidgetReplacementMap.IsEmpty())
    {
        return 0;
    }

    TSet<UObject*> ObjectsToPatch;
    ObjectsToPatch.Add(WidgetBlueprint);
    ObjectsToPatch.Add(WidgetBlueprint->WidgetTree);
    if (UPackage* Package = WidgetBlueprint->GetOutermost())
    {
        TArray<UObject*> PackageObjects;
        GetObjectsWithPackage(Package, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
        for (UObject* PackageObject : PackageObjects)
        {
            if (IsLiveObjectForMCP(PackageObject))
            {
                ObjectsToPatch.Add(PackageObject);
            }
        }
    }

    for (UObject* ObjectToPatch : ObjectsToPatch)
    {
        if (ShouldSkipArchiveObjectReferenceRemap(ObjectToPatch))
        {
            continue;
        }

        if (!Write(ObjectToPatch, TEXT("Modify"))) return {};
        ObjectToPatch->Modify();
        if (!After()) return {};
        if (!Check()) return {};
        const int64 ReplacementCount = RemapOwnedReferences(
            ObjectToPatch,
            WidgetReplacementMap,
            EArchiveReplaceObjectFlags::IgnoreOuterRef |
            EArchiveReplaceObjectFlags::IgnoreArchetypeRef |
            EArchiveReplaceObjectFlags::IncludeClassGeneratedByRef);
        if (!Check()) return {};
        if (ReplacementCount > 0)
        {
            if (!Write(ObjectToPatch, TEXT("MarkPackageDirty"))) return {};
            ObjectToPatch->MarkPackageDirty();
            if (!After()) return {};
        }
    }

    if (!Write(WidgetBlueprint->WidgetTree, TEXT("Modify"))) return {};
    WidgetBlueprint->WidgetTree->Modify();
    if (!After()) return {};
    if (!Write(WidgetBlueprint->WidgetTree, TEXT("MarkPackageDirty"))) return {};
    WidgetBlueprint->WidgetTree->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(WidgetBlueprint, TEXT("FBlueprintEditorUtils_MarkBlueprintAsStructurallyModified"))) return {};
    FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(WidgetBlueprint);
    if (!After()) return {};
    if (!Write(WidgetBlueprint, TEXT("MarkPackageDirty"))) return {};
    WidgetBlueprint->MarkPackageDirty();
    if (!After()) return {};
    return RepairCount;
}



int32 RepairTargetSubobjectsWithSourceClasses(const TSet<UPackage*>& TargetPackages, TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (TargetPackages.IsEmpty() || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    TMap<UObject*, UObject*> LocalReplacementMap;
    for (UPackage* TargetPackage : TargetPackages)
    {
        if (!TargetPackage)
        {
            continue;
        }

        TArray<UObject*> PackageObjects;
        GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
        for (UObject* PackageObject : PackageObjects)
        {
            if (!IsLiveObjectForMCP(PackageObject) || PackageObject->HasAnyFlags(RF_ClassDefaultObject) || PackageObject->IsA<UClass>() || PackageObject->IsA<UBlueprint>())
            {
                continue;
            }

            UBlueprintGeneratedClass* OuterGeneratedClass = Cast<UBlueprintGeneratedClass>(PackageObject->GetOuter());
            if (!OuterGeneratedClass)
            {
                continue;
            }

            UClass* SourceClass = PackageObject->GetClass();
            if (!IsLiveObjectForMCP(SourceClass))
            {
                continue;
            }

            UClass* TargetClass = Cast<UClass>(FindReplacementObject(ReplacementMap, SourceClass));
            if (!IsLiveObjectForMCP(TargetClass) || TargetClass == SourceClass || TargetClass->GetPropertiesSize() < SourceClass->GetPropertiesSize())
            {
                continue;
            }

            UObject* OriginalOuter = PackageObject->GetOuter();
            const FName OriginalName = PackageObject->GetFName();
            if (!IsLiveObjectForMCP(OriginalOuter) || OriginalName.IsNone())
            {
                continue;
            }

            const EObjectFlags OriginalFlags = PackageObject->GetFlags();
            const FName TransientName = MakeUniqueObjectName(GetTransientPackage(), SourceClass, OriginalName);
            if (!Check()) return {};
            MoveOwnedObject(PackageObject, *TransientName.ToString(), GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors);
            if (!Check()) return {};

            FObjectDuplicationParameters DuplicationParameters(PackageObject, OriginalOuter);
            DuplicationParameters.DestName = OriginalName;
            DuplicationParameters.DestClass = TargetClass;
            DuplicationParameters.ApplyFlags = OriginalFlags & (RF_Public | RF_Standalone | RF_Transactional | RF_ArchetypeObject);

            if (!Check()) return {};
            UObject* NewObject = CreateOwnedFactory(DuplicationParameters.DestOuter, DuplicationParameters.DestClass, DuplicationParameters.DestName, TEXT("StaticDuplicateObjectEx"), [&]() { return StaticDuplicateObjectEx(DuplicationParameters); });
            if (!Check()) return {};
            if (!NewObject)
            {
                if (!Check()) return {};
                MoveOwnedObject(PackageObject, *OriginalName.ToString(), OriginalOuter, REN_DoNotDirty | REN_DontCreateRedirectors);
                if (!Check()) return {};
                continue;
            }

            if (!Write(NewObject, TEXT("SetFlags"))) return {};
            NewObject->SetFlags(OriginalFlags & (RF_Public | RF_Standalone | RF_Transactional | RF_ArchetypeObject));
            if (!After()) return {};
            if (!Write(NewObject, TEXT("ClearFlags"))) return {};
            NewObject->ClearFlags(RF_Transient);
            if (!After()) return {};
            LocalReplacementMap.Add(PackageObject, NewObject);
            ReplacementMap.Add(PackageObject, NewObject);
        }
    }

    if (LocalReplacementMap.IsEmpty())
    {
        return 0;
    }

    TSet<UObject*> ObjectsToPatch;
    for (UPackage* TargetPackage : TargetPackages)
    {
        if (!TargetPackage)
        {
            continue;
        }

        TArray<UObject*> PackageObjects;
        GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
        for (UObject* PackageObject : PackageObjects)
        {
            if (IsLiveObjectForMCP(PackageObject))
            {
                ObjectsToPatch.Add(PackageObject);
            }
        }
    }

    for (UObject* ObjectToPatch : ObjectsToPatch)
    {
        if (ShouldSkipArchiveObjectReferenceRemap(ObjectToPatch))
        {
            continue;
        }

        if (!Write(ObjectToPatch, TEXT("Modify"))) return {};
        ObjectToPatch->Modify();
        if (!After()) return {};
        if (!Check()) return {};
        const int64 ReplacementCount = RemapOwnedReferences(
            ObjectToPatch,
            LocalReplacementMap,
            EArchiveReplaceObjectFlags::IgnoreOuterRef |
            EArchiveReplaceObjectFlags::IgnoreArchetypeRef |
            EArchiveReplaceObjectFlags::IncludeClassGeneratedByRef);
        if (!Check()) return {};
        if (ReplacementCount > 0)
        {
            if (!Write(ObjectToPatch, TEXT("MarkPackageDirty"))) return {};
            ObjectToPatch->MarkPackageDirty();
            if (!After()) return {};
            if (UPackage* Package = ObjectToPatch->GetOutermost())
            {
                if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
                Package->MarkPackageDirty();
                if (!After()) return {};
            }
        }
    }

    return LocalReplacementMap.Num();
}



void RefreshMaterialReferencesAndCaches(
    const TSet<UPackage*>& TargetPackages,
    const TMap<UObject*, UObject*>& ReplacementMap,
    int32& OutTextureReferenceRepairCount,
    int32& OutMaterialCacheRefreshCount)
{
    if (!Check()) return;

    if (TargetPackages.IsEmpty())
    {
        return;
    }

    TSet<UMaterial*> Materials;
    TSet<UMaterialFunction*> MaterialFunctions;
    TSet<UMaterialInstance*> MaterialInstances;
    TSet<UPackage*> DirtyPackages;

    for (UPackage* TargetPackage : TargetPackages)
    {
        if (!TargetPackage)
        {
            continue;
        }

        TArray<UObject*> PackageObjects;
        GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
        for (UObject* PackageObject : PackageObjects)
        {
            if (!IsLiveObjectForMCP(PackageObject))
            {
                continue;
            }

            if (UMaterialExpressionTextureBase* TextureExpression = Cast<UMaterialExpressionTextureBase>(PackageObject))
            {
                if (UTexture* ReplacementTexture = Cast<UTexture>(FindReplacementObject(ReplacementMap, TextureExpression->Texture)))
                {
                    if (ReplacementTexture != TextureExpression->Texture)
                    {
                        if (!Write(TextureExpression, TEXT("Modify"))) return;
                        TextureExpression->Modify();
                        if (!After()) return;
                        TextureExpression->Texture = ReplacementTexture;
                        TextureExpression->AutoSetSampleType();
                        if (!Write(TextureExpression, TEXT("MarkPackageDirty"))) return;
                        TextureExpression->MarkPackageDirty();
                        if (!After()) return;
                        DirtyPackages.Add(TextureExpression->GetOutermost());
                        ++OutTextureReferenceRepairCount;
                    }
                }
                else if (TextureExpression->Texture)
                {
                    TextureExpression->AutoSetSampleType();
                }
            }

            if (UMaterial* Material = Cast<UMaterial>(PackageObject))
            {
                Materials.Add(Material);
            }
            else if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(PackageObject))
            {
                MaterialFunctions.Add(MaterialFunction);
            }
            else if (UMaterialInstance* MaterialInstance = Cast<UMaterialInstance>(PackageObject))
            {
                MaterialInstances.Add(MaterialInstance);
            }
        }
    }

    for (UMaterialInstance* MaterialInstance : MaterialInstances)
    {
        if (!MaterialInstance)
        {
            continue;
        }

        bool bChanged = false;
        for (FTextureParameterValue& TextureParameter : MaterialInstance->TextureParameterValues)
        {
            if (UTexture* ReplacementTexture = Cast<UTexture>(FindReplacementObject(ReplacementMap, TextureParameter.ParameterValue)))
            {
                if (ReplacementTexture != TextureParameter.ParameterValue)
                {
                    if (!bChanged)
                    {
                        if (!Write(MaterialInstance, TEXT("Modify"))) return;
                        MaterialInstance->Modify();
                        if (!After()) return;
                    }
                    TextureParameter.ParameterValue = ReplacementTexture;
                    bChanged = true;
                    ++OutTextureReferenceRepairCount;
                }
            }
        }

        if (bChanged)
        {
            if (!Write(MaterialInstance, TEXT("MarkPackageDirty"))) return;
            MaterialInstance->MarkPackageDirty();
            if (!After()) return;
            DirtyPackages.Add(MaterialInstance->GetOutermost());
        }
    }

    FMaterialUpdateContext MaterialUpdateContext(FMaterialUpdateContext::EOptions::Default);

    for (UMaterialFunction* MaterialFunction : MaterialFunctions)
    {
        if (!MaterialFunction)
        {
            continue;
        }

        if (!Write(MaterialFunction, TEXT("Modify"))) return;
        MaterialFunction->Modify();
        if (!After()) return;
        if (!Write(MaterialFunction, TEXT("UpdateFromFunctionResource"))) return;
        MaterialFunction->UpdateFromFunctionResource();
        if (!After()) return;
        if (!Write(MaterialFunction, TEXT("UpdateInputOutputTypes"))) return;
        MaterialFunction->UpdateInputOutputTypes();
        if (!After()) return;
        if (!Write(MaterialFunction, TEXT("UpdateDependentFunctionCandidates"))) return;
        MaterialFunction->UpdateDependentFunctionCandidates();
        if (!After()) return;
        MaterialFunction->ForceRecompileForRendering(MaterialUpdateContext, nullptr);
        if (!Write(MaterialFunction, TEXT("MarkPackageDirty"))) return;
        MaterialFunction->MarkPackageDirty();
        if (!After()) return;
        DirtyPackages.Add(MaterialFunction->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UMaterial* Material : Materials)
    {
        if (!Material)
        {
            continue;
        }

        if (!Write(Material, TEXT("Modify"))) return;
        Material->Modify();
        if (!After()) return;
        if (!Write(Material, TEXT("UpdateCachedExpressionData"))) return;
        Material->UpdateCachedExpressionData();
        if (!After()) return;
        if (!Write(Material, TEXT("BuildEditorParameterList"))) return;
        Material->BuildEditorParameterList();
        if (!After()) return;
        Material->ForceRecompileForRendering(EMaterialShaderPrecompileMode::None);
        if (!Write(Material, TEXT("MarkPackageDirty"))) return;
        Material->MarkPackageDirty();
        if (!After()) return;
        DirtyPackages.Add(Material->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UMaterialInstance* MaterialInstance : MaterialInstances)
    {
        if (!MaterialInstance)
        {
            continue;
        }

        if (!Write(MaterialInstance, TEXT("Modify"))) return;
        MaterialInstance->Modify();
        if (!After()) return;
        MaterialInstance->ForceRecompileForRendering(EMaterialShaderPrecompileMode::None);
        if (!Write(MaterialInstance, TEXT("MarkPackageDirty"))) return;
        MaterialInstance->MarkPackageDirty();
        if (!After()) return;
        DirtyPackages.Add(MaterialInstance->GetOutermost());
        ++OutMaterialCacheRefreshCount;
    }

    for (UPackage* DirtyPackage : DirtyPackages)
    {
        if (DirtyPackage)
        {
            if (!Write(DirtyPackage, TEXT("MarkPackageDirty"))) return;
            DirtyPackage->MarkPackageDirty();
            if (!After()) return;
        }
    }
}



int32 RepairTargetWorldComponentClasses(
    UWorld* LoadedWorld,
    const TMap<UObject*, UObject*>& ReplacementMap,
    TArray<FString>& FailedAssets)
{
    if (!Check()) return {};

    if (!LoadedWorld || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    TArray<TPair<UActorComponent*, UClass*>> ComponentsToRepair;
    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (!Level)
        {
            continue;
        }

        for (AActor* Actor : Level->Actors)
        {
            if (!IsLiveObjectForMCP(Actor))
            {
                continue;
            }

            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (!IsLiveObjectForMCP(Component))
                {
                    continue;
                }

                UClass* SourceClass = Component->GetClass();
                if (!IsLiveObjectForMCP(SourceClass))
                {
                    continue;
                }

                UClass* TargetClass = Cast<UClass>(FindReplacementObject(ReplacementMap, SourceClass));
                if (!IsLiveObjectForMCP(TargetClass) || TargetClass == SourceClass || !TargetClass->IsChildOf(UActorComponent::StaticClass()))
                {
                    continue;
                }

                ComponentsToRepair.Emplace(Component, TargetClass);
            }
        }
    }

    if (ComponentsToRepair.IsEmpty())
    {
        return 0;
    }

    int32 RepairedComponentCount = 0;
    TMap<UObject*, UObject*> ComponentReplacementMap;
    TMap<UObject*, UObject*> CopyReplacementMap = ReplacementMap;

    for (const TPair<UActorComponent*, UClass*>& ComponentPair : ComponentsToRepair)
    {
        UActorComponent* OldComponent = ComponentPair.Key;
        UClass* TargetClass = ComponentPair.Value;
        if (!IsLiveObjectForMCP(OldComponent) || !IsLiveObjectForMCP(TargetClass))
        {
            continue;
        }

        AActor* Owner = OldComponent->GetOwner();
        UObject* OriginalOuter = OldComponent->GetOuter();
        const FName OriginalName = OldComponent->GetFName();
        if (!IsLiveObjectForMCP(Owner) || !IsLiveObjectForMCP(OriginalOuter))
        {
            FailedAssets.Add(OldComponent->GetPathName() + TEXT(" (failed to resolve owner/outer for component class repair)"));
            continue;
        }

        if (!Write(Owner, TEXT("Modify"))) return {};
        Owner->Modify();
        if (!After()) return {};
        if (!Write(OriginalOuter, TEXT("Modify"))) return {};
        OriginalOuter->Modify();
        if (!After()) return {};
        if (!Write(OldComponent, TEXT("Modify"))) return {};
        OldComponent->Modify();
        if (!After()) return {};

        const bool bWasRegistered = OldComponent->IsRegistered();
        if (bWasRegistered)
        {
            if (!Write(OldComponent, TEXT("UnregisterComponent"))) return {};
            OldComponent->UnregisterComponent();
            if (!After()) return {};
        }
        if (!Write(Owner, TEXT("RemoveInstanceComponent"))) return {};
        Owner->RemoveInstanceComponent(OldComponent);
        if (!After()) return {};
        if (!Write(Owner, TEXT("RemoveOwnedComponent"))) return {};
        Owner->RemoveOwnedComponent(OldComponent);
        if (!After()) return {};

        const FName ReplacedName = MakeUniqueObjectName(
            GetTransientPackage(),
            OldComponent->GetClass(),
            FName(*FString::Printf(TEXT("%s_Replaced"), *OriginalName.ToString())));

        if (!MoveOwnedObject(OldComponent, *ReplacedName.ToString(), GetTransientPackage(), REN_DontCreateRedirectors | REN_NonTransactional))
        {
            FailedAssets.Add(OldComponent->GetPathName() + TEXT(" (failed to rename source-class component before repair)"));
            continue;
        }

        UActorComponent* NewComponent = NewOwnedObject<UActorComponent>(OriginalOuter, TargetClass, OriginalName, RF_Transactional);
        if (!NewComponent)
        {
            FailedAssets.Add(Owner->GetPathName() + TEXT(".") + OriginalName.ToString() + TEXT(" (failed to create target-class component)"));
            continue;
        }

        UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
        CopyParams.OptionalReplacementMappings = &CopyReplacementMap;
        CopyParams.bReplaceObjectClassReferences = true;
        CopyParams.bReplaceInternalReferenceUponRead = true;
        if (!Check()) return {};
        CopyOwnedProperties(OldComponent, NewComponent, CopyParams);
        if (!Check()) return {};

        if (!Write(Owner, TEXT("AddInstanceComponent"))) return {};
        Owner->AddInstanceComponent(NewComponent);
        if (!After()) return {};
        if (!Write(Owner, TEXT("AddOwnedComponent"))) return {};
        Owner->AddOwnedComponent(NewComponent);
        if (!After()) return {};
        if (bWasRegistered && !NewComponent->IsRegistered())
        {
            if (!Write(NewComponent, TEXT("RegisterComponent"))) return {};
            NewComponent->RegisterComponent();
            if (!After()) return {};
        }

        ComponentReplacementMap.Add(OldComponent, NewComponent);
        ++RepairedComponentCount;
    }

    if (ComponentReplacementMap.IsEmpty())
    {
        return 0;
    }

    TArray<UObject*> WorldPackageObjects;
    GetObjectsWithPackage(LoadedWorld->GetOutermost(), WorldPackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
    for (UObject* WorldPackageObject : WorldPackageObjects)
    {
        if (ShouldSkipArchiveObjectReferenceRemap(WorldPackageObject))
        {
            continue;
        }

        if (!Check()) return {};
        RemapOwnedReferences(
            WorldPackageObject,
            ComponentReplacementMap,
            EArchiveReplaceObjectFlags::IgnoreOuterRef | EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
        if (!Check()) return {};
    }

    if (!Write(LoadedWorld, TEXT("MarkPackageDirty"))) return {};
    LoadedWorld->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(LoadedWorld->GetOutermost(), TEXT("MarkPackageDirty"))) return {};
    LoadedWorld->GetOutermost()->MarkPackageDirty();
    if (!After()) return {};
    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (Level)
        {
            if (!Write(Level, TEXT("MarkPackageDirty"))) return {};
            Level->MarkPackageDirty();
            if (!After()) return {};
        }
    }

    return RepairedComponentCount;
}



int32 RemapObjectPropertyValueForMCP(
    FObjectPropertyBase* ObjectProperty,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!ObjectProperty || !ValuePtr)
    {
        return 0;
    }

    UObject* CurrentObject = ObjectProperty->GetObjectPropertyValue(ValuePtr);
    UObject* const* ReplacementObject = ReplacementMap.Find(CurrentObject);
    if (!ReplacementObject || !IsLiveObjectForMCP(*ReplacementObject))
    {
        return 0;
    }

    ObjectProperty->SetObjectPropertyValue(ValuePtr, *ReplacementObject);
    return 1;
}



int32 RemapInterfacePropertyValueForMCP(
    FInterfaceProperty* InterfaceProperty,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!InterfaceProperty || !ValuePtr)
    {
        return 0;
    }

    FScriptInterface* InterfaceValue = static_cast<FScriptInterface*>(ValuePtr);
    UObject* CurrentObject = InterfaceValue ? InterfaceValue->GetObject() : nullptr;
    UObject* const* ReplacementObject = ReplacementMap.Find(CurrentObject);
    if (!ReplacementObject || !IsLiveObjectForMCP(*ReplacementObject))
    {
        return 0;
    }

    UClass* ReplacementClass = (*ReplacementObject)->GetClass();
    if (!ReplacementClass || !ReplacementClass->ImplementsInterface(InterfaceProperty->InterfaceClass))
    {
        return 0;
    }

    InterfaceValue->SetObject(*ReplacementObject);
    InterfaceValue->SetInterface((*ReplacementObject)->GetInterfaceAddress(InterfaceProperty->InterfaceClass));
    return 1;
}



int32 RemapObjectReferencesInPropertyValueForMCP(
    FProperty* Property,
    void* ValuePtr,
    const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!Property || !ValuePtr)
    {
        return 0;
    }

    if (FObjectPropertyBase* ObjectProperty = CastField<FObjectPropertyBase>(Property))
    {
        return RemapObjectPropertyValueForMCP(ObjectProperty, ValuePtr, ReplacementMap);
    }

    if (FInterfaceProperty* InterfaceProperty = CastField<FInterfaceProperty>(Property))
    {
        return RemapInterfacePropertyValueForMCP(InterfaceProperty, ValuePtr, ReplacementMap);
    }

    if (FArrayProperty* ArrayProperty = CastField<FArrayProperty>(Property))
    {
        int32 RemapCount = 0;
        FScriptArrayHelper ArrayHelper(ArrayProperty, ValuePtr);
        for (int32 Index = 0; Index < ArrayHelper.Num(); ++Index)
        {
            RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                ArrayProperty->Inner,
                ArrayHelper.GetRawPtr(Index),
                ReplacementMap);
        }
        return RemapCount;
    }

    if (FSetProperty* SetProperty = CastField<FSetProperty>(Property))
    {
        int32 RemapCount = 0;
        bool bChangedSetKey = false;
        FScriptSetHelper SetHelper(SetProperty, ValuePtr);
        for (int32 InternalIndex = 0; InternalIndex < SetHelper.GetMaxIndex(); ++InternalIndex)
        {
            if (!SetHelper.IsValidIndex(InternalIndex))
            {
                continue;
            }

            const int32 CountBefore = RemapCount;
            RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                SetProperty->ElementProp,
                SetHelper.GetElementPtr(InternalIndex),
                ReplacementMap);
            bChangedSetKey |= RemapCount != CountBefore;
        }

        if (bChangedSetKey)
        {
            SetHelper.Rehash();
        }
        return RemapCount;
    }

    if (FMapProperty* MapProperty = CastField<FMapProperty>(Property))
    {
        int32 RemapCount = 0;
        bool bChangedMapKey = false;
        FScriptMapHelper MapHelper(MapProperty, ValuePtr);
        for (int32 InternalIndex = 0; InternalIndex < MapHelper.GetMaxIndex(); ++InternalIndex)
        {
            if (!MapHelper.IsValidIndex(InternalIndex))
            {
                continue;
            }

            const int32 CountBeforeKey = RemapCount;
            RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                MapProperty->KeyProp,
                MapHelper.GetKeyPtr(InternalIndex),
                ReplacementMap);
            bChangedMapKey |= RemapCount != CountBeforeKey;

            RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                MapProperty->ValueProp,
                MapHelper.GetValuePtr(InternalIndex),
                ReplacementMap);
        }

        if (bChangedMapKey)
        {
            MapHelper.Rehash();
        }
        return RemapCount;
    }

    if (FStructProperty* StructProperty = CastField<FStructProperty>(Property))
    {
        if (!StructProperty->Struct)
        {
            return 0;
        }

        int32 RemapCount = 0;
        for (TFieldIterator<FProperty> InnerPropertyIt(StructProperty->Struct); InnerPropertyIt; ++InnerPropertyIt)
        {
            FProperty* InnerProperty = *InnerPropertyIt;
            if (!InnerProperty)
            {
                continue;
            }

            for (int32 ArrayIndex = 0; ArrayIndex < InnerProperty->ArrayDim; ++ArrayIndex)
            {
                RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                    InnerProperty,
                    InnerProperty->ContainerPtrToValuePtr<void>(ValuePtr, ArrayIndex),
                    ReplacementMap);
            }
        }
        return RemapCount;
    }

    return 0;
}



int32 RemapObjectReferencesInObjectPropertiesForMCP(UObject* Object, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object) || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    if (!Write(Object, TEXT("Modify"))) return {};
    Object->Modify();
    if (!After()) return {};

    int32 RemapCount = 0;
    for (TFieldIterator<FProperty> PropertyIt(Object->GetClass()); PropertyIt; ++PropertyIt)
    {
        FProperty* Property = *PropertyIt;
        if (!Property)
        {
            continue;
        }

        for (int32 ArrayIndex = 0; ArrayIndex < Property->ArrayDim; ++ArrayIndex)
        {
            RemapCount += RemapObjectReferencesInPropertyValueForMCP(
                Property,
                Property->ContainerPtrToValuePtr<void>(Object, ArrayIndex),
                ReplacementMap);
        }
    }

    if (RemapCount > 0)
    {
        if (!Write(Object, TEXT("MarkPackageDirty"))) return {};
        Object->MarkPackageDirty();
        if (!After()) return {};
        if (UPackage* Package = Object->GetOutermost())
        {
            if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
            Package->MarkPackageDirty();
            if (!After()) return {};
        }
    }

    return RemapCount;
}



int32 RemapTargetWorldActorInstanceReferences(UWorld* LoadedWorld, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!LoadedWorld || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    int32 RemapCount = 0;
    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (!Level)
        {
            continue;
        }

        for (AActor* Actor : Level->Actors)
        {
            if (!IsLiveObjectForMCP(Actor))
            {
                continue;
            }

            int32 ActorRemapCount = RemapObjectReferencesInObjectPropertiesForMCP(Actor, ReplacementMap);
            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                ActorRemapCount += RemapObjectReferencesInObjectPropertiesForMCP(Component, ReplacementMap);
            }

            if (ActorRemapCount > 0)
            {
                RemapCount += ActorRemapCount;
                if (!Write(Actor, TEXT("MarkPackageDirty"))) return {};
                Actor->MarkPackageDirty();
                if (!After()) return {};
            }
        }
    }

    return RemapCount;
}



bool ShouldRemoveRemappedMapKey(UObject* KeyObject, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(KeyObject))
    {
        return true;
    }

    return IsPackageUnderRoot(KeyObject, SourceRoot)
        || IsPackageUnderRoot(KeyObject->GetClass(), SourceRoot)
        || IsGeneratedClassInstanceFromRoot(KeyObject, SourceRoot);
}



int32 RemoveSourceObjectKeyMapEntries(UObject* Object, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object))
    {
        return 0;
    }

    int32 RemovedCount = 0;
    for (TFieldIterator<FProperty> PropertyIt(Object->GetClass()); PropertyIt; ++PropertyIt)
    {
        FMapProperty* MapProperty = CastField<FMapProperty>(*PropertyIt);
        if (!MapProperty)
        {
            continue;
        }

        FObjectPropertyBase* KeyObjectProperty = CastField<FObjectPropertyBase>(MapProperty->KeyProp);
        if (!KeyObjectProperty)
        {
            continue;
        }

        void* MapValuePtr = MapProperty->ContainerPtrToValuePtr<void>(Object);
        FScriptMapHelper MapHelper(MapProperty, MapValuePtr);
        bool bRemovedFromMap = false;
        for (int32 InternalIndex = MapHelper.GetMaxIndex() - 1; InternalIndex >= 0; --InternalIndex)
        {
            if (!MapHelper.IsValidIndex(InternalIndex))
            {
                continue;
            }

            UObject* KeyObject = KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(InternalIndex));
            if (!ShouldRemoveRemappedMapKey(KeyObject, SourceRoot))
            {
                continue;
            }

            MapHelper.RemoveAt(InternalIndex);
            ++RemovedCount;
            bRemovedFromMap = true;
        }

        if (bRemovedFromMap)
        {
            MapHelper.Rehash();
        }
    }

    if (RemovedCount > 0)
    {
        if (!Write(Object, TEXT("Modify"))) return {};
        Object->Modify();
        if (!After()) return {};
        if (!Write(Object, TEXT("MarkPackageDirty"))) return {};
        Object->MarkPackageDirty();
        if (!After()) return {};
    }
    return RemovedCount;
}



int32 RemoveSourceObjectKeyMapEntriesFromWorld(UWorld* LoadedWorld, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!LoadedWorld)
    {
        return 0;
    }

    int32 RemovedCount = 0;
    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (!Level)
        {
            continue;
        }

        for (AActor* Actor : Level->Actors)
        {
            if (!IsLiveObjectForMCP(Actor))
            {
                continue;
            }

            RemovedCount += RemoveSourceObjectKeyMapEntries(Actor, SourceRoot);

            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (IsLiveObjectForMCP(Component))
                {
                    RemovedCount += RemoveSourceObjectKeyMapEntries(Component, SourceRoot);
                }
            }
        }
    }
    return RemovedCount;
}



int32 CountSourceObjectKeyMapEntries(UObject* Object, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object))
    {
        return 0;
    }

    int32 SourceKeyCount = 0;
    for (TFieldIterator<FProperty> PropertyIt(Object->GetClass()); PropertyIt; ++PropertyIt)
    {
        FMapProperty* MapProperty = CastField<FMapProperty>(*PropertyIt);
        if (!MapProperty)
        {
            continue;
        }

        FObjectPropertyBase* KeyObjectProperty = CastField<FObjectPropertyBase>(MapProperty->KeyProp);
        if (!KeyObjectProperty)
        {
            continue;
        }

        void* MapValuePtr = MapProperty->ContainerPtrToValuePtr<void>(Object);
        FScriptMapHelper MapHelper(MapProperty, MapValuePtr);
        for (int32 InternalIndex = 0; InternalIndex < MapHelper.GetMaxIndex(); ++InternalIndex)
        {
            if (!MapHelper.IsValidIndex(InternalIndex))
            {
                continue;
            }

            UObject* KeyObject = KeyObjectProperty->GetObjectPropertyValue(MapHelper.GetKeyPtr(InternalIndex));
            if (ShouldRemoveRemappedMapKey(KeyObject, SourceRoot))
            {
                ++SourceKeyCount;
            }
        }
    }

    return SourceKeyCount;
}



int32 CountSourceObjectKeyMapEntriesFromWorld(UWorld* LoadedWorld, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!LoadedWorld)
    {
        return 0;
    }

    int32 SourceKeyCount = 0;
    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (!Level)
        {
            continue;
        }

        for (AActor* Actor : Level->Actors)
        {
            if (!IsLiveObjectForMCP(Actor))
            {
                continue;
            }

            SourceKeyCount += CountSourceObjectKeyMapEntries(Actor, SourceRoot);

            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                if (IsLiveObjectForMCP(Component))
                {
                    SourceKeyCount += CountSourceObjectKeyMapEntries(Component, SourceRoot);
                }
            }
        }
    }

    return SourceKeyCount;
}



void CountWorldSourceClassInstances(
    UWorld* LoadedWorld,
    const FString& SourceRoot,
    int32& OutSourceActorCount,
    int32& OutSourceComponentCount)
{
    if (!Check()) return;

    OutSourceActorCount = 0;
    OutSourceComponentCount = 0;

    if (!LoadedWorld)
    {
        return;
    }

    for (ULevel* Level : LoadedWorld->GetLevels())
    {
        if (!Level)
        {
            continue;
        }

        for (AActor* Actor : Level->Actors)
        {
            if (!IsLiveObjectForMCP(Actor))
            {
                continue;
            }

            UClass* ActorClass = Actor->GetClass();
            if (IsLiveObjectForMCP(ActorClass) && IsPackageUnderRoot(ActorClass, SourceRoot))
            {
                ++OutSourceActorCount;
            }

            TArray<UActorComponent*> Components;
            Actor->GetComponents(Components);
            for (UActorComponent* Component : Components)
            {
                UClass* ComponentClass = IsLiveObjectForMCP(Component) ? Component->GetClass() : nullptr;
                if (IsLiveObjectForMCP(ComponentClass) && IsPackageUnderRoot(ComponentClass, SourceRoot))
                {
                    ++OutSourceComponentCount;
                }
            }
        }
    }
}



int32 RepairLoadedWorldActorInstances(
    UWorld* LoadedWorld,
    const FString& WorldPackageName,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const TArray<FPathStringReplacement>& PathReplacements,
    const FString& SourceRoot,
    int32& OutLevelActorReferenceRemapCount,
    int32& OutSavedMapCount,
    int32& OutLevelComponentClassRepairCount,
    TArray<FString>& FailedAssets,
    bool bRemapLevelBlueprint,
    bool bRepairActorClasses,
    bool bRepairComponentClasses,
    bool bRemapActorReferences,
    bool bRemoveSourceObjectMapKeys,
    bool bSaveMap)
{
    if (!Check()) return {};

    if (!LoadedWorld || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    bool bWorldChanged = false;
    TArray<UObject*> WorldPackageObjects;
    GetObjectsWithPackage(LoadedWorld->GetOutermost(), WorldPackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
    for (UObject* WorldPackageObject : WorldPackageObjects)
    {
        if (!IsLiveObjectForMCP(WorldPackageObject))
        {
            continue;
        }

        const int32 PathRemapCount = ReplaceExportedPropertyPaths(WorldPackageObject, PathReplacements, SourceRoot);
        if (PathRemapCount > 0)
        {
            OutLevelActorReferenceRemapCount += PathRemapCount;
            bWorldChanged = true;
        }

        if (!bRemapLevelBlueprint)
        {
            continue;
        }

        UBlueprint* WorldBlueprint = Cast<UBlueprint>(WorldPackageObject);
        if (WorldBlueprint)
        {
            const int32 GraphRemapCount = RemapBlueprintGraphReferences(WorldBlueprint, ReplacementMap, PathReplacements);
            if (GraphRemapCount > 0)
            {
                OutLevelActorReferenceRemapCount += GraphRemapCount;
                bWorldChanged = true;
                if (!Write(WorldBlueprint, TEXT("FBlueprintEditorUtils_RefreshAllNodes"))) return {};
                FBlueprintEditorUtils::RefreshAllNodes(WorldBlueprint);
                if (!After()) return {};
                OutLevelActorReferenceRemapCount += RemapBlueprintGraphReferences(WorldBlueprint, ReplacementMap, PathReplacements);
                if (!Write(WorldBlueprint, TEXT("FBlueprintEditorUtils_MarkBlueprintAsModified"))) return {};
                FBlueprintEditorUtils::MarkBlueprintAsModified(WorldBlueprint);
                if (!After()) return {};
                if (!Check()) return {};
                CompileOwnedBlueprint(WorldBlueprint);
                if (!Check()) return {};
                if (WorldBlueprint->Status == BS_Error)
                {
                    FailedAssets.Add(WorldBlueprint->GetPathName() + TEXT(" (level blueprint compile error after reference repair)"));
                }
            }
        }
    }

    int32 RepairedActorCount = 0;
    TArray<AActor*> ActorsToRepair;
    TSet<UObject*> TargetActorInstances;
    TMap<UClass*, UClass*> OldToNewClassMap;

    if (bRepairActorClasses)
    {
        for (ULevel* Level : LoadedWorld->GetLevels())
        {
            if (!Level)
            {
                continue;
            }

            for (AActor* Actor : Level->Actors)
            {
                if (!IsLiveObjectForMCP(Actor))
                {
                    continue;
                }

                UClass* SourceClass = Actor->GetClass();
                if (!IsLiveObjectForMCP(SourceClass))
                {
                    continue;
                }

                UClass* TargetClass = Cast<UClass>(FindReplacementObject(ReplacementMap, SourceClass));
                if (!IsLiveObjectForMCP(TargetClass) || TargetClass == SourceClass || !TargetClass->IsChildOf(AActor::StaticClass()))
                {
                    continue;
                }

                ActorsToRepair.Add(Actor);
                TargetActorInstances.Add(Actor);
                OldToNewClassMap.Add(SourceClass, TargetClass);
            }
        }
    }

    if (!ActorsToRepair.IsEmpty() && !OldToNewClassMap.IsEmpty())
    {
        TSet<UObject*> InstancesThatShouldUseOldClass;
        for (TObjectIterator<UObject> It(RF_ClassDefaultObject, true, EInternalObjectFlags::Garbage); It; ++It)
        {
            UObject* Object = *It;
            if (!IsLiveObjectForMCP(Object) || TargetActorInstances.Contains(Object))
            {
                continue;
            }

            UClass* ObjectClass = Object->GetClass();
            if (IsLiveObjectForMCP(ObjectClass) && OldToNewClassMap.Contains(ObjectClass))
            {
                InstancesThatShouldUseOldClass.Add(Object);
            }
        }

        TSet<UObject*> ObjectsThatShouldUseOldStuff;
        UPackage* ExactWorldPackage = LoadedWorld->GetPackage();
        for (TObjectIterator<UObject> It; It; ++It)
        { UObject* O = *It; if (Live(O) && O->GetPackage() != ExactWorldPackage) ObjectsThatShouldUseOldStuff.Add(O); }
        if (Current->Transaction) { Current->Transaction->Cancel(); Current->Transaction.Reset(); Current->R.Data->SetBoolField(TEXT("owned_transaction_cancelled_for_reinstance"), true); if (!Check()) return 0; }
        if (!Before(TEXT("world_reinstance_reference_admission"), false)) return 0;
        for (AActor* Actor : ActorsToRepair)
        {
            if (!Live(Actor) || Actor->GetWorld() != LoadedWorld || Actor->GetPackage() != ExactWorldPackage) { Current->Fail(TEXT("reinstance_actor_identity"), TEXT("Actor left the exact target world before reinstancing")); return 0; }
            bool Referenced = false, Undo = false; FReferencerInformationList References;
            ObjectTools::GatherObjectReferencersForDeletion(Actor, Referenced, Undo, &References);
            if (!Check()) return 0;
            if (Undo) { Current->Fail(TEXT("reinstance_undo_reference"), TEXT("Existing Undo references to a replaced actor are retained; reinstancing was not dispatched")); return 0; }
            for (const FReferencerInformation& Ref : References.ExternalReferences)
                if (Live(Ref.Referencer) && Ref.Referencer->GetPackage() != ExactWorldPackage && Ref.Referencer->GetPackage()->GetName().StartsWith(TEXT("/Game/")))
                { Current->Fail(TEXT("reinstance_foreign_reference"), TEXT("A source or foreign Game package references a target actor; it was not rewritten")); return 0; }
        }
        if (!After() || !Write(LoadedWorld, TEXT("batch_replace_target_world_actor_classes"))) return 0;
        ObjectsThatShouldUseOldStuff.Reset();
        for (TObjectIterator<UObject> It; It; ++It) { UObject* O = *It; if (Live(O) && O->GetPackage() != ExactWorldPackage) ObjectsThatShouldUseOldStuff.Add(O); }
        for (AActor* Actor : ActorsToRepair)
        {
            for (auto& T : Current->Targets) if (!T.Retired && Live(T.Object.Get()) && T.Object->IsIn(Actor)) T.Retired = true;
            Current->Retire(Actor, TEXT("intentional_target_world_actor_reinstance"));
        }
        FReplaceInstancesOfClassParameters ReinstanceParams;
        ReinstanceParams.ObjectsThatShouldUseOldStuff = &ObjectsThatShouldUseOldStuff;
        ReinstanceParams.InstancesThatShouldUseOldClass = &InstancesThatShouldUseOldClass;
        ReinstanceParams.bPreserveRootComponent = true;
        ReinstanceParams.bReplaceReferencesToOldClasses = false;
        ReinstanceParams.bReplaceReferencesToOldCDOs = false;
        FBlueprintCompileReinstancer::BatchReplaceInstancesOfClass(OldToNewClassMap, ReinstanceParams);
        if (!After()) return 0;

        RepairedActorCount += ActorsToRepair.Num();
        bWorldChanged = true;
        if (!Write(LoadedWorld, TEXT("MarkPackageDirty"))) return {};
        LoadedWorld->MarkPackageDirty();
        if (!After()) return {};
        for (ULevel* Level : LoadedWorld->GetLevels())
        {
            if (Level)
            {
                if (!Write(Level, TEXT("MarkPackageDirty"))) return {};
                Level->MarkPackageDirty();
                if (!After()) return {};
            }
        }
    }

    if (bRepairComponentClasses)
    {
        const int32 ComponentRepairCount = RepairTargetWorldComponentClasses(LoadedWorld, ReplacementMap, FailedAssets);
        if (ComponentRepairCount > 0)
        {
            OutLevelComponentClassRepairCount += ComponentRepairCount;
            bWorldChanged = true;
        }
    }

    if (bRemapActorReferences)
    {
        const int32 ActorInstanceReferenceRemapCount = RemapTargetWorldActorInstanceReferences(LoadedWorld, ReplacementMap);
        if (ActorInstanceReferenceRemapCount > 0)
        {
            OutLevelActorReferenceRemapCount += ActorInstanceReferenceRemapCount;
            bWorldChanged = true;
        }
    }

    if (bRemoveSourceObjectMapKeys)
    {
        const int32 RemovedSourceMapKeyCount = RemoveSourceObjectKeyMapEntriesFromWorld(LoadedWorld, SourceRoot);
        if (RemovedSourceMapKeyCount > 0)
        {
            OutLevelActorReferenceRemapCount += RemovedSourceMapKeyCount;
            bWorldChanged = true;
        }
    }

    if (!bWorldChanged)
    {
        return RepairedActorCount;
    }

    if (!Write(LoadedWorld->GetOutermost(), TEXT("MarkPackageDirty"))) return {};
    LoadedWorld->GetOutermost()->MarkPackageDirty();
    if (!After()) return {};
    if (!bSaveMap)
    {
        return RepairedActorCount;
    }

    if (SaveWorldPackageForRepair(LoadedWorld, WorldPackageName))
    {
        ++OutSavedMapCount;
    }
    else
    {
        FailedAssets.Add(WorldPackageName + TEXT(" (failed to save target map after actor class repair)"));
    }

    return RepairedActorCount;
}



int32 RepairTargetWorldActorsWithSourceClasses(
    const TArray<FString>& TargetWorldPackageNames,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const TArray<FPathStringReplacement>& PathReplacements,
    const FString& SourceRoot,
    int32& OutLevelActorReferenceRemapCount,
    int32& OutSavedMapCount,
    int32& OutLevelComponentClassRepairCount,
    TArray<FString>& FailedAssets,
    bool bRemapLevelBlueprint,
    bool bRepairActorClasses,
    bool bRepairComponentClasses,
    bool bRemapActorReferences,
    bool bRemoveSourceObjectMapKeys,
    bool bSaveMap)
{
    if (!Check()) return {};

    if (TargetWorldPackageNames.IsEmpty() || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    int32 RepairedActorCount = 0;

    for (const FString& WorldPackageName : TargetWorldPackageNames)
    {
        UWorld* LoadedWorld = LoadWorldPackageForRepair(WorldPackageName);
        if (!LoadedWorld)
        {
            FailedAssets.Add(WorldPackageName + TEXT(" (failed to load target map for actor class repair)"));
            continue;
        }

        RepairedActorCount += RepairLoadedWorldActorInstances(
            LoadedWorld,
            WorldPackageName,
            ReplacementMap,
            PathReplacements,
            SourceRoot,
            OutLevelActorReferenceRemapCount,
            OutSavedMapCount,
            OutLevelComponentClassRepairCount,
            FailedAssets,
            bRemapLevelBlueprint,
            bRepairActorClasses,
            bRepairComponentClasses,
            bRemapActorReferences,
            bRemoveSourceObjectMapKeys,
            bSaveMap);
    }

    return RepairedActorCount;
}



UObject* FindReplacementObject(const TMap<UObject*, UObject*>& ReplacementMap, UObject* Object)
{
    if (!Check()) return {};

    if (!IsLiveObjectForMCP(Object))
    {
        return nullptr;
    }

    if (UObject* const* Replacement = ReplacementMap.Find(Object))
    {
        return IsLiveObjectForMCP(*Replacement) ? *Replacement : nullptr;
    }
    return nullptr;
}



bool RemapSimpleMemberReference(FSimpleMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (UObject* ReplacementParent = FindReplacementObject(ReplacementMap, Reference.MemberParent))
    {
        Reference.MemberParent = ReplacementParent;
        return true;
    }
    return false;
}



bool RemapTerminalType(FEdGraphTerminalType& TerminalType, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (UObject* Replacement = FindReplacementObject(ReplacementMap, TerminalType.TerminalSubCategoryObject.Get(true)))
    {
        TerminalType.TerminalSubCategoryObject = Replacement;
        return true;
    }
    return false;
}



bool RemapPinType(FEdGraphPinType& PinType, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    bool bChanged = false;
    if (UObject* Replacement = FindReplacementObject(ReplacementMap, PinType.PinSubCategoryObject.Get(true)))
    {
        PinType.PinSubCategoryObject = Replacement;
        bChanged = true;
    }

    bChanged |= RemapSimpleMemberReference(PinType.PinSubCategoryMemberReference, ReplacementMap);
    bChanged |= RemapTerminalType(PinType.PinValueType, ReplacementMap);
    return bChanged;
}



bool RemapMemberReferenceParent(FMemberReference& Reference, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (Reference.IsSelfContext() || Reference.IsLocalScope())
    {
        return false;
    }

    UClass* ParentClass = Reference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    Reference.SetExternalMember(Reference.GetMemberName(), ReplacementClass, Reference.GetMemberGuid());
    return true;
}



FGuid FindBlueprintSelfVariableGuid(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Check()) return {};

    if (!Blueprint || MemberName.IsNone())
    {
        return FGuid();
    }

    FGuid Guid = FBlueprintEditorUtils::FindMemberVariableGuidByName(Blueprint, MemberName);
    if (Guid.IsValid())
    {
        return Guid;
    }

    if (Blueprint->SimpleConstructionScript)
    {
        const TArray<USCS_Node*>& SCSNodes = Blueprint->SimpleConstructionScript->GetAllNodes();
        for (USCS_Node* SCSNode : SCSNodes)
        {
            if (SCSNode && SCSNode->GetVariableName() == MemberName && SCSNode->VariableGuid.IsValid())
            {
                return SCSNode->VariableGuid;
            }
        }
    }

    for (const FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        if (Variable.VarName == MemberName && Variable.VarGuid.IsValid())
        {
            return Variable.VarGuid;
        }
    }

    for (const FBPVariableDescription& Variable : Blueprint->GeneratedVariables)
    {
        if (Variable.VarName == MemberName && Variable.VarGuid.IsValid())
        {
            return Variable.VarGuid;
        }
    }

    return FGuid();
}



FProperty* FindBlueprintPropertyByName(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Check()) return {};

    if (!Blueprint || MemberName.IsNone())
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (FProperty* Property = FindFProperty<FProperty>(Blueprint->SkeletonGeneratedClass, MemberName))
        {
            return Property;
        }
    }

    if (Blueprint->GeneratedClass)
    {
        if (FProperty* Property = FindFProperty<FProperty>(Blueprint->GeneratedClass, MemberName))
        {
            return Property;
        }
    }

    return nullptr;
}



UFunction* FindBlueprintFunctionByName(UBlueprint* Blueprint, const FName MemberName)
{
    if (!Check()) return {};

    if (!Blueprint || MemberName.IsNone())
    {
        return nullptr;
    }

    if (Blueprint->SkeletonGeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->SkeletonGeneratedClass, MemberName))
        {
            return Function;
        }
    }

    if (Blueprint->GeneratedClass)
    {
        if (UFunction* Function = FindUField<UFunction>(Blueprint->GeneratedClass, MemberName))
        {
            return Function;
        }
    }

    return nullptr;
}



bool RemapSelfMemberReference(FMemberReference& Reference, UBlueprint* Blueprint)
{
    if (!Check()) return {};

    if (!Blueprint || !Reference.IsSelfContext() || Reference.IsLocalScope())
    {
        return false;
    }

    const FName MemberName = Reference.GetMemberName();
    const FGuid Guid = FindBlueprintSelfVariableGuid(Blueprint, MemberName);
    if (Guid.IsValid() && Guid != Reference.GetMemberGuid())
    {
        Reference.SetSelfMember(MemberName, Guid);
        return true;
    }

    return false;
}



bool RemapVariableReference(UK2Node_Variable* VariableNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!VariableNode)
    {
        return false;
    }

    const FName MemberName = VariableNode->VariableReference.GetMemberName();
    if (VariableNode->VariableReference.IsLocalScope())
    {
        return false;
    }

    if (VariableNode->VariableReference.IsSelfContext())
    {
        if (!Write(VariableNode, TEXT("Modify"))) return {};
        VariableNode->Modify();
        if (!After()) return {};
        if (FProperty* ReplacementProperty = FindBlueprintPropertyByName(Blueprint, MemberName))
        {
            if (!Write(VariableNode, TEXT("SetFromProperty"))) return {};
            VariableNode->SetFromProperty(ReplacementProperty, true, ReplacementProperty->GetOwnerClass());
            if (!After()) return {};
            return true;
        }

        const FGuid Guid = FindBlueprintSelfVariableGuid(Blueprint, MemberName);
        if (Guid.IsValid() && Guid != VariableNode->VariableReference.GetMemberGuid())
        {
            VariableNode->VariableReference.SetSelfMember(MemberName, Guid);
            return true;
        }

        return false;
    }

    UClass* ParentClass = VariableNode->VariableReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Write(VariableNode, TEXT("Modify"))) return {};
    VariableNode->Modify();
    if (!After()) return {};
    if (const FProperty* ReplacementProperty = FindFProperty<FProperty>(ReplacementClass, MemberName))
    {
        if (!Write(VariableNode, TEXT("SetFromProperty"))) return {};
        VariableNode->SetFromProperty(ReplacementProperty, false, ReplacementClass);
        if (!After()) return {};
    }
    else
    {
        VariableNode->VariableReference.SetExternalMember(
            MemberName,
            ReplacementClass,
            VariableNode->VariableReference.GetMemberGuid());
    }
    return true;
}



bool RemapFunctionReference(UK2Node_CallFunction* FunctionNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!FunctionNode || FunctionNode->FunctionReference.IsLocalScope())
    {
        return false;
    }

    if (FunctionNode->FunctionReference.IsSelfContext())
    {
        if (const UFunction* ReplacementFunction = FindBlueprintFunctionByName(Blueprint, FunctionNode->FunctionReference.GetMemberName()))
        {
            if (!Write(FunctionNode, TEXT("Modify"))) return {};
            FunctionNode->Modify();
            if (!After()) return {};
            if (!Write(FunctionNode, TEXT("SetFromFunction"))) return {};
            FunctionNode->SetFromFunction(ReplacementFunction);
            if (!After()) return {};
            return true;
        }

        return false;
    }

    UClass* ParentClass = FunctionNode->FunctionReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Write(FunctionNode, TEXT("Modify"))) return {};
    FunctionNode->Modify();
    if (!After()) return {};
    if (const UFunction* ReplacementFunction = FindUField<UFunction>(ReplacementClass, FunctionNode->FunctionReference.GetMemberName()))
    {
        if (!Write(FunctionNode, TEXT("SetFromFunction"))) return {};
        FunctionNode->SetFromFunction(ReplacementFunction);
        if (!After()) return {};
    }
    else
    {
        FunctionNode->FunctionReference.SetExternalMember(
            FunctionNode->FunctionReference.GetMemberName(),
            ReplacementClass,
            FunctionNode->FunctionReference.GetMemberGuid());
    }
    return true;
}



bool RemapDelegateReference(UK2Node_BaseMCDelegate* DelegateNode, UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!DelegateNode || DelegateNode->DelegateReference.IsLocalScope())
    {
        return false;
    }

    const FName MemberName = DelegateNode->DelegateReference.GetMemberName();
    if (DelegateNode->DelegateReference.IsSelfContext())
    {
        if (FProperty* ReplacementProperty = FindBlueprintPropertyByName(Blueprint, MemberName))
        {
            if (!Write(DelegateNode, TEXT("Modify"))) return {};
            DelegateNode->Modify();
            if (!After()) return {};
            if (!Write(DelegateNode, TEXT("SetFromProperty"))) return {};
            DelegateNode->SetFromProperty(ReplacementProperty, true, ReplacementProperty->GetOwnerClass());
            if (!After()) return {};
            return true;
        }

        return RemapSelfMemberReference(DelegateNode->DelegateReference, Blueprint);
    }

    UClass* ParentClass = DelegateNode->DelegateReference.GetMemberParentClass();
    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, ParentClass));
    if (!ReplacementClass)
    {
        return false;
    }

    if (!Write(DelegateNode, TEXT("Modify"))) return {};
    DelegateNode->Modify();
    if (!After()) return {};
    if (const FProperty* ReplacementProperty = FindFProperty<FProperty>(ReplacementClass, MemberName))
    {
        if (!Write(DelegateNode, TEXT("SetFromProperty"))) return {};
        DelegateNode->SetFromProperty(ReplacementProperty, false, ReplacementClass);
        if (!After()) return {};
    }
    else
    {
        DelegateNode->DelegateReference.SetExternalMember(MemberName, ReplacementClass, DelegateNode->DelegateReference.GetMemberGuid());
    }
    return true;
}



bool RemapEditableUserPins(UK2Node_EditablePinBase* EditableNode, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Check()) return {};

    if (!EditableNode)
    {
        return false;
    }

    bool bChanged = false;
    for (TSharedPtr<FUserPinInfo>& UserPin : EditableNode->UserDefinedPins)
    {
        if (!UserPin.IsValid())
        {
            continue;
        }

        bChanged |= RemapPinType(UserPin->PinType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(UserPin->PinDefaultValue, PathReplacements);
        if (NewDefaultValue != UserPin->PinDefaultValue)
        {
            UserPin->PinDefaultValue = NewDefaultValue;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        if (!Write(EditableNode, TEXT("Modify"))) return {};
        EditableNode->Modify();
        if (!After()) return {};
    }
    return bChanged;
}



bool RemapFunctionEntryLocalVariables(UK2Node_FunctionEntry* FunctionEntry, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Check()) return {};

    if (!FunctionEntry)
    {
        return false;
    }

    bool bChanged = false;
    for (FBPVariableDescription& LocalVariable : FunctionEntry->LocalVariables)
    {
        bChanged |= RemapPinType(LocalVariable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(LocalVariable.DefaultValue, PathReplacements);
        if (NewDefaultValue != LocalVariable.DefaultValue)
        {
            LocalVariable.DefaultValue = NewDefaultValue;
            bChanged = true;
        }
    }

    if (bChanged)
    {
        if (!Write(FunctionEntry, TEXT("Modify"))) return {};
        FunctionEntry->Modify();
        if (!After()) return {};
    }
    return bChanged;
}



bool RemapStructOperation(UK2Node_StructOperation* StructNode, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!StructNode || !StructNode->StructType)
    {
        return false;
    }

    UScriptStruct* ReplacementStruct = Cast<UScriptStruct>(FindReplacementObject(ReplacementMap, StructNode->StructType));
    if (!ReplacementStruct || ReplacementStruct == StructNode->StructType)
    {
        return false;
    }

    if (!Write(StructNode, TEXT("Modify"))) return {};
    StructNode->Modify();
    if (!After()) return {};
    StructNode->StructType = ReplacementStruct;
    return true;
}



bool RemapDynamicCastTarget(UK2Node_DynamicCast* CastNode, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!CastNode || !CastNode->TargetType)
    {
        return false;
    }

    UClass* ReplacementClass = Cast<UClass>(FindReplacementObject(ReplacementMap, CastNode->TargetType));
    if (!ReplacementClass || ReplacementClass == CastNode->TargetType)
    {
        return false;
    }

    if (!Write(CastNode, TEXT("Modify"))) return {};
    CastNode->Modify();
    if (!After()) return {};
    CastNode->TargetType = ReplacementClass;
    return true;
}



bool RemapGraphPin(UEdGraphNode* OwnerNode, UEdGraphPin* Pin, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Check()) return {};
    if (!Pin || !Live(OwnerNode)) return false;
    if (!OwnerNode->Pins.Contains(Pin))
        return Current->Fail(TEXT("graph_pin_replaced"), TEXT("Captured graph pin no longer belongs to its owner node"));
    const FGuid PinIdentity = Pin->PinId;
    if (Pin->GetOwningNode() != OwnerNode)
        return Current->Fail(TEXT("graph_pin_owner_changed"), TEXT("Captured graph pin owner differs from the admitted node"));
    const auto ValidatePin = [&]() -> bool
    {
        if (!Check()) return false;
        if (!Live(OwnerNode) || !OwnerNode->Pins.Contains(Pin))
            return Current->Fail(TEXT("graph_pin_replaced"), TEXT("Exact graph pin was replaced during an SDK callback; it was not retargeted"));
        if (Pin->PinId != PinIdentity || Pin->GetOwningNode() != OwnerNode)
            return Current->Fail(TEXT("graph_pin_identity_changed"), TEXT("Captured graph pin identity or owner changed during an SDK callback"));
        return true;
    };

    FEdGraphPinType NewPinType = Pin->PinType;
    bool bChanged = RemapPinType(NewPinType, ReplacementMap);
    UObject* NewDefaultObject = Pin->DefaultObject;
    if (UObject* Replacement = FindReplacementObject(ReplacementMap, NewDefaultObject))
    {
        NewDefaultObject = Replacement;
        bChanged = true;
    }
    const FString NewDefaultValue = ApplyPathReplacements(Pin->DefaultValue, PathReplacements);
    bChanged |= NewDefaultValue != Pin->DefaultValue;
    const FString NewAutogeneratedDefaultValue = ApplyPathReplacements(Pin->AutogeneratedDefaultValue, PathReplacements);
    bChanged |= NewAutogeneratedDefaultValue != Pin->AutogeneratedDefaultValue;

    if (bChanged)
    {
        // UEdGraphPin is not a UObject; its public Modify delegates to this node.
        if (!Write(OwnerNode, TEXT("Modify")) || !ValidatePin()) return false;
        Pin->Modify();
        if (!After() || !ValidatePin()) return false;
        Pin->PinType = NewPinType;
        Pin->DefaultObject = NewDefaultObject;
        Pin->DefaultValue = NewDefaultValue;
        Pin->AutogeneratedDefaultValue = NewAutogeneratedDefaultValue;
    }
    return bChanged;
}



bool NodeHasSplitPins(const UEdGraphNode* Node)
{
    if (!Check()) return {};

    if (!Node)
    {
        return false;
    }

    for (const UEdGraphPin* Pin : Node->Pins)
    {
        if (Pin && Pin->SubPins.Num() > 0)
        {
            return true;
        }
    }
    return false;
}



int32 RemapBlueprintGraphReferences(UBlueprint* Blueprint, const TMap<UObject*, UObject*>& ReplacementMap, const TArray<FPathStringReplacement>& PathReplacements)
{
    if (!Check()) return {};

    if (!Blueprint || ReplacementMap.IsEmpty())
    {
        return 0;
    }

    int32 RemapCount = 0;
    bool bBlueprintChanged = false;

    for (FBPVariableDescription& Variable : Blueprint->NewVariables)
    {
        bool bVariableChanged = RemapPinType(Variable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(Variable.DefaultValue, PathReplacements);
        if (NewDefaultValue != Variable.DefaultValue)
        {
            Variable.DefaultValue = NewDefaultValue;
            bVariableChanged = true;
        }

        if (bVariableChanged)
        {
            ++RemapCount;
            bBlueprintChanged = true;
        }
    }

    for (FBPVariableDescription& Variable : Blueprint->GeneratedVariables)
    {
        bool bVariableChanged = RemapPinType(Variable.VarType, ReplacementMap);
        const FString NewDefaultValue = ApplyPathReplacements(Variable.DefaultValue, PathReplacements);
        if (NewDefaultValue != Variable.DefaultValue)
        {
            Variable.DefaultValue = NewDefaultValue;
            bVariableChanged = true;
        }

        if (bVariableChanged)
        {
            ++RemapCount;
            bBlueprintChanged = true;
        }
    }

    TArray<UEdGraph*> Graphs;
    Blueprint->GetAllGraphs(Graphs);
    TArray<UEdGraphNode*> NodesToReconstruct;
    for (UEdGraph* Graph : Graphs)
    {
        if (!Graph)
        {
            continue;
        }

        for (UEdGraphNode* Node : Graph->Nodes)
        {
            if (!Node)
            {
                continue;
            }

            bool bNodeChanged = false;
            bool bNeedsReconstruct = false;
            const bool bHasSplitPins = NodeHasSplitPins(Node);
            const bool bIsStructOperation = Node->IsA<UK2Node_StructOperation>();
            if (UK2Node_CallFunction* FunctionNode = Cast<UK2Node_CallFunction>(Node))
            {
                const bool bFunctionChanged = RemapFunctionReference(FunctionNode, Blueprint, ReplacementMap);
                bNodeChanged |= bFunctionChanged;
                bNeedsReconstruct |= bFunctionChanged;
                if (UK2Node_CallFunctionOnMember* FunctionOnMemberNode = Cast<UK2Node_CallFunctionOnMember>(FunctionNode))
                {
                    const bool bMemberParentChanged = RemapMemberReferenceParent(FunctionOnMemberNode->MemberVariableToCallOn, ReplacementMap);
                    const bool bSelfMemberChanged = RemapSelfMemberReference(FunctionOnMemberNode->MemberVariableToCallOn, Blueprint);
                    bNodeChanged |= bMemberParentChanged || bSelfMemberChanged;
                    bNeedsReconstruct |= bMemberParentChanged || bSelfMemberChanged;
                }
            }
            if (UK2Node_Variable* VariableNode = Cast<UK2Node_Variable>(Node))
            {
                const bool bVariableChanged = RemapVariableReference(VariableNode, Blueprint, ReplacementMap);
                bNodeChanged |= bVariableChanged;
                bNeedsReconstruct |= bVariableChanged;
            }
            if (UK2Node_Event* EventNode = Cast<UK2Node_Event>(Node))
            {
                const bool bEventParentChanged = RemapMemberReferenceParent(EventNode->EventReference, ReplacementMap);
                const bool bSelfEventChanged = RemapSelfMemberReference(EventNode->EventReference, Blueprint);
                bNodeChanged |= bEventParentChanged || bSelfEventChanged;
                bNeedsReconstruct |= bEventParentChanged || bSelfEventChanged;
            }
            if (UK2Node_BaseMCDelegate* DelegateNode = Cast<UK2Node_BaseMCDelegate>(Node))
            {
                const bool bDelegateChanged = RemapDelegateReference(DelegateNode, Blueprint, ReplacementMap);
                bNodeChanged |= bDelegateChanged;
                bNeedsReconstruct |= bDelegateChanged;
            }
            if (UK2Node_FunctionTerminator* FunctionTerminator = Cast<UK2Node_FunctionTerminator>(Node))
            {
                const bool bTerminatorParentChanged = RemapMemberReferenceParent(FunctionTerminator->FunctionReference, ReplacementMap);
                const bool bSelfTerminatorChanged = RemapSelfMemberReference(FunctionTerminator->FunctionReference, Blueprint);
                bNodeChanged |= bTerminatorParentChanged || bSelfTerminatorChanged;
                bNeedsReconstruct |= bTerminatorParentChanged || bSelfTerminatorChanged;
            }
            if (UK2Node_EditablePinBase* EditableNode = Cast<UK2Node_EditablePinBase>(Node))
            {
                bNodeChanged |= RemapEditableUserPins(EditableNode, ReplacementMap, PathReplacements);
            }
            if (UK2Node_FunctionEntry* FunctionEntry = Cast<UK2Node_FunctionEntry>(Node))
            {
                bNodeChanged |= RemapFunctionEntryLocalVariables(FunctionEntry, ReplacementMap, PathReplacements);
            }
            if (UK2Node_StructOperation* StructNode = Cast<UK2Node_StructOperation>(Node))
            {
                bNodeChanged |= RemapStructOperation(StructNode, ReplacementMap);
            }
            if (UK2Node_DynamicCast* CastNode = Cast<UK2Node_DynamicCast>(Node))
            {
                const bool bCastTargetChanged = RemapDynamicCastTarget(CastNode, ReplacementMap);
                bNodeChanged |= bCastTargetChanged;
                bNeedsReconstruct |= bCastTargetChanged;
            }

            const TArray<UEdGraphPin*> PinsBeforeRemap = Node->Pins;
            for (UEdGraphPin* Pin : PinsBeforeRemap)
            {
                bNodeChanged |= RemapGraphPin(Node, Pin, ReplacementMap, PathReplacements);
            }

            if (bNodeChanged)
            {
                ++RemapCount;
                bBlueprintChanged = true;
                if (bNeedsReconstruct && !bHasSplitPins && !bIsStructOperation)
                {
                    NodesToReconstruct.AddUnique(Node);
                }
            }
        }
    }

    for (UEdGraphNode* Node : NodesToReconstruct)
    {
        if (!Node)
        {
            continue;
        }

        if (!Write(Node, TEXT("ReconstructNode"))) return {};
        Node->ReconstructNode();
        if (!After()) return {};
        const TArray<UEdGraphPin*> PinsBeforeRemap = Node->Pins;
        for (UEdGraphPin* Pin : PinsBeforeRemap)
        {
            if (RemapGraphPin(Node, Pin, ReplacementMap, PathReplacements))
            {
                ++RemapCount;
            }
        }
    }

    if (bBlueprintChanged)
    {
        if (!Write(Blueprint, TEXT("FBlueprintEditorUtils_MarkBlueprintAsStructurallyModified"))) return {};
        FBlueprintEditorUtils::MarkBlueprintAsStructurallyModified(Blueprint);
        if (!After()) return {};
        if (!Write(Blueprint, TEXT("MarkPackageDirty"))) return {};
        Blueprint->MarkPackageDirty();
        if (!After()) return {};
    }

    return RemapCount;
}



void AddPathReplacement(TArray<FPathStringReplacement>& Replacements, const FString& Source, const FString& Target)
{
    if (!Check()) return;

    if (!Source.IsEmpty() && !Target.IsEmpty() && Source != Target)
    {
        Replacements.Add({Source, Target});
    }
}



void AddObjectPathReplacements(TArray<FPathStringReplacement>& Replacements, UObject* SourceObject, UObject* TargetObject)
{
    if (!Check()) return;

    if (!IsValid(SourceObject) || !IsValid(TargetObject))
    {
        return;
    }

    UPackage* SourcePackage = SourceObject->GetOutermost();
    UPackage* TargetPackage = TargetObject->GetOutermost();
    if (!SourcePackage || !TargetPackage)
    {
        return;
    }

    AddPathReplacement(Replacements, SourceObject->GetPathName(), TargetObject->GetPathName());
    AddPathReplacement(Replacements, SourcePackage->GetName(), TargetPackage->GetName());

    UBlueprint* SourceBlueprint = Cast<UBlueprint>(SourceObject);
    UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject);
    if (SourceBlueprint && TargetBlueprint)
    {
        AddPathReplacement(Replacements,
            SourceObject->GetPathName() + TEXT("_C"),
            TargetObject->GetPathName() + TEXT("_C"));
        AddPathReplacement(Replacements,
            SourcePackage->GetName() + TEXT(".") + SourceObject->GetName() + TEXT("_C"),
            TargetPackage->GetName() + TEXT(".") + TargetObject->GetName() + TEXT("_C"));
        if (IsValid(SourceBlueprint->GeneratedClass) && IsValid(TargetBlueprint->GeneratedClass))
        {
            AddPathReplacement(Replacements, SourceBlueprint->GeneratedClass->GetPathName(), TargetBlueprint->GeneratedClass->GetPathName());
        }
        if (IsValid(SourceBlueprint->SkeletonGeneratedClass) && IsValid(TargetBlueprint->SkeletonGeneratedClass))
        {
            AddPathReplacement(Replacements, SourceBlueprint->SkeletonGeneratedClass->GetPathName(), TargetBlueprint->SkeletonGeneratedClass->GetPathName());
        }
    }
}



FString ApplyPathReplacements(FString Text, const TArray<FPathStringReplacement>& Replacements)
{
    if (!Check()) return {};

    for (const FPathStringReplacement& Replacement : Replacements)
    {
        if (Text.Contains(Replacement.Source, ESearchCase::CaseSensitive))
        {
            Text.ReplaceInline(*Replacement.Source, *Replacement.Target, ESearchCase::CaseSensitive);
        }
    }
    return Text;
}



int32 ReplaceExportedPropertyPaths(UObject* Object, const TArray<FPathStringReplacement>& Replacements, const FString& SourceRoot)
{
    if (!Check()) return {};

    if (!Object || Replacements.IsEmpty())
    {
        return 0;
    }

    int32 ReplaceCount = 0;
    bool bObjectModified = false;
    for (TFieldIterator<FProperty> PropertyIterator(Object->GetClass()); PropertyIterator; ++PropertyIterator)
    {
        FProperty* Property = *PropertyIterator;
        if (!Property || Property->HasAnyPropertyFlags(CPF_Transient))
        {
            continue;
        }

        FString ExportedValue;
        Property->ExportText_InContainer(0, ExportedValue, Object, Object, Object, PPF_SerializedAsImportText);
        if (!ExportedValue.Contains(SourceRoot, ESearchCase::CaseSensitive))
        {
            continue;
        }

        const FString ReplacedValue = ApplyPathReplacements(ExportedValue, Replacements);
        if (ReplacedValue == ExportedValue)
        {
            continue;
        }

        if (!bObjectModified)
        {
            if (!Write(Object, TEXT("Modify"))) return {};
            Object->Modify();
            if (!After()) return {};
            bObjectModified = true;
        }

        void* ValueAddress = Property->ContainerPtrToValuePtr<void>(Object);
        const TCHAR* ImportEnd = Property->ImportText_Direct(
            *ReplacedValue,
            ValueAddress,
            Object,
            PPF_SerializedAsImportText,
            GWarn);

        if (ImportEnd)
        {
            ++ReplaceCount;
        }
    }

    if (ReplaceCount > 0)
    {
        if (!Write(Object, TEXT("MarkPackageDirty"))) return {};
        Object->MarkPackageDirty();
        if (!After()) return {};
        if (UPackage* Package = Object->GetOutermost())
        {
            if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
            Package->MarkPackageDirty();
            if (!After()) return {};
        }
    }
    return ReplaceCount;
}



int32 CollectOriginalDependencyPackages(
    IAssetRegistry& AssetRegistry,
    const TArray<FName>& TargetPackageNames,
    const FString& SourceRoot,
    TSet<FName>& OutPackagesWithOriginalDependencies,
    TArray<FString>& OutSamples,
    int32 MaxSamples)
{
    if (!Check()) return {};

    OutPackagesWithOriginalDependencies.Reset();
    OutSamples.Reset();

    for (const FName& TargetPackageName : TargetPackageNames)
    {
        TArray<FName> Dependencies;
        AssetRegistry.GetDependencies(TargetPackageName, Dependencies);
        bool bHasOriginalDependency = false;
        for (const FName& Dependency : Dependencies)
        {
            const FString DependencyPath = Dependency.ToString();
            if (DependencyPath.StartsWith(SourceRoot + TEXT("/")) || DependencyPath.Equals(SourceRoot))
            {
                bHasOriginalDependency = true;
                if (OutSamples.Num() < MaxSamples)
                {
                    OutSamples.Add(FString::Printf(TEXT("%s -> %s"), *TargetPackageName.ToString(), *DependencyPath));
                }
            }
        }

        if (bHasOriginalDependency)
        {
            OutPackagesWithOriginalDependencies.Add(TargetPackageName);
        }
    }

    return OutPackagesWithOriginalDependencies.Num();
}



FString BuildTargetAssetPath(const FAssetData& SourceAsset, const FString& SourceRoot, const FString& TargetRoot, const FString& Suffix)
{
    if (!Check()) return {};

    const FString SourceDirectory = SourceAsset.PackagePath.ToString();
    FString RelativeDirectory;
    if (SourceDirectory.Equals(SourceRoot))
    {
        RelativeDirectory = FString();
    }
    else if (SourceDirectory.StartsWith(SourceRoot + TEXT("/")))
    {
        RelativeDirectory = SourceDirectory.RightChop(SourceRoot.Len());
    }
    else
    {
        RelativeDirectory = TEXT("/") + FPackageName::GetShortName(SourceDirectory);
    }

    FString TargetName = SourceAsset.AssetName.ToString();
    if (!TargetName.EndsWith(Suffix))
    {
        TargetName += Suffix;
    }

    return TargetRoot + RelativeDirectory + TEXT("/") + TargetName;
}



int32 GetRecreateCreationPriority(const FAssetData& SourceAsset)
{
    if (!Check()) return {};

    const FString ClassName = SourceAsset.AssetClassPath.GetAssetName().ToString();
    if (ClassName == TEXT("UserDefinedEnum"))
    {
        return 0;
    }
    if (ClassName == TEXT("UserDefinedStruct"))
    {
        return 1;
    }
    if (ClassName == TEXT("DataTable"))
    {
        return 2;
    }
    return 10;
}



FString GetFallbackReason(const FAssetData& SourceAsset)
{
    if (!Check()) return {};

    const FString ClassName = SourceAsset.AssetClassPath.GetAssetName().ToString();
    return FString::Printf(
        TEXT("exact_behavior_requires_serialized_asset_data_for_%s"),
        *ClassName);
}



bool IsFreshPropertyCopyAssetClassName(const FString& ClassName)
{
    if (!Check()) return {};

    static const TSet<FString> FreshPropertyCopyClasses = {
        TEXT("MaterialInstanceConstant"),
        TEXT("CurveFloat"),
        TEXT("CurveLinearColor"),
        TEXT("CurveVector"),
        TEXT("TextureRenderTarget2D"),
        TEXT("TextureRenderTargetCube"),
        TEXT("TextureRenderTargetVolume"),
        TEXT("MaterialParameterCollection"),
        TEXT("NiagaraParameterCollection"),
        TEXT("AudioBus"),
        TEXT("SoundClass"),
        TEXT("SoundAttenuation"),
    };
    return FreshPropertyCopyClasses.Contains(ClassName);
}



FString GetFreshPropertyCopyReason(const FAssetData& SourceAsset)
{
    if (!Check()) return {};

    const FString ClassName = SourceAsset.AssetClassPath.GetAssetName().ToString();
    return FString::Printf(
        TEXT("fresh_new_object_property_copy_for_%s"),
        *ClassName);
}



UObject* CreateFreshPropertyCopyAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    UClass* TargetClass,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceObject)
    {
        OutDetail = TEXT("source asset is null");
        return nullptr;
    }

    if (!TargetClass)
    {
        OutDetail = TEXT("target class is null");
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UObject* TargetObject = NewOwnedObject<UObject>(
        TargetPackage,
        TargetClass,
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetObject)
    {
        OutDetail = TEXT("could not create target object");
        return nullptr;
    }

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceObject, TargetObject, CopyParams);
    if (!Check()) return {};

    if (!Write(TargetObject, TEXT("SetFlags"))) return {};
    TargetObject->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("ClearFlags"))) return {};
    TargetObject->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("PostEditChange"))) return {};
    TargetObject->PostEditChange();
    if (!After()) return {};
    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetObject);
    if (!After()) return {};

    OutDetail = FString::Printf(TEXT("fresh_property_copy_from_%s"), *SourceObject->GetClass()->GetName());
    return TargetObject;
}



void DiscardFreshAssetForFallback(UObject* Object)
{
    if (!Check() || !Live(Object)) return;
    bool Owned = false;
    for (auto& T : Current->Targets)
        if (T.Object.Get() == Object && T.Created) { Owned = true; T.Hold.Reset(); }
    if (!Owned) { Current->Fail(TEXT("fallback_cleanup_not_owned"), TEXT("Fresh fallback cleanup cannot prove operation ownership")); return; }
    if (!BeforeCleanup(TEXT("admit_failed_fresh_cleanup"))) return;
    bool Referenced = false, Undo = false;
    ObjectTools::GatherObjectReferencersForDeletion(Object, Referenced, Undo);
    if (!Check()) return;
    if (Referenced || Undo)
    { Current->R.Data->SetBoolField(TEXT("fallback_cleanup_residual"), true); Current->Fail(TEXT("fallback_cleanup_referenced"), TEXT("Fresh asset cleanup retained referenced/Undo-owned output; no unsafe rename/flag clearing or duplicate fallback followed")); return; }
    if (!BeforeCleanup(TEXT("discard_failed_fresh_asset"))) return;
    Current->Retire(Object, TEXT("failed_fresh_asset_reference_checked_cleanup"));
    const bool Deleted = ObjectTools::DeleteSingleObject(Object, true);
    Current->R.Data->SetBoolField(TEXT("fallback_cleanup_delete_returned"), Deleted);
    if (!After() || !Deleted) { Current->R.Data->SetBoolField(TEXT("fallback_cleanup_residual"), true); Current->Fail(TEXT("fallback_cleanup_failed"), TEXT("Public cleanup did not complete; output was retained and not retried")); return; }
    if (!MoveOwnedObject(Object, nullptr, GetTransientPackage(), REN_DoNotDirty | REN_DontCreateRedirectors | REN_NonTransactional))
    { Current->R.Data->SetBoolField(TEXT("fallback_cleanup_residual"), true); Current->Fail(TEXT("fallback_cleanup_name_residual"), TEXT("Failed fresh object name was not released; no duplicate fallback followed")); }
}



bool IsFreshMaterialGraphAssetClassName(const FString& ClassName)
{
    if (!Check()) return {};

    return ClassName == TEXT("Material") || ClassName == TEXT("MaterialFunction");
}



TConstArrayView<TObjectPtr<UMaterialExpression>> GetMaterialGraphExpressions(UObject* GraphObject)
{
    if (!Check()) return {};

    if (UMaterial* Material = Cast<UMaterial>(GraphObject))
    {
        return Material->GetExpressions();
    }
    if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(GraphObject))
    {
        return MaterialFunction->GetExpressions();
    }
    return TConstArrayView<TObjectPtr<UMaterialExpression>>();
}



FMaterialExpressionCollection* GetMutableMaterialGraphExpressionCollection(UObject* GraphObject)
{
    if (!Check()) return {};

    if (UMaterial* Material = Cast<UMaterial>(GraphObject))
    {
        return &Material->GetExpressionCollection();
    }
    if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(GraphObject))
    {
        return &MaterialFunction->GetExpressionCollection();
    }
    return nullptr;
}



const FMaterialExpressionCollection* GetMaterialGraphExpressionCollection(UObject* GraphObject)
{
    if (!Check()) return {};

    if (UMaterial* Material = Cast<UMaterial>(GraphObject))
    {
        return &Material->GetExpressionCollection();
    }
    if (UMaterialFunction* MaterialFunction = Cast<UMaterialFunction>(GraphObject))
    {
        return &MaterialFunction->GetExpressionCollection();
    }
    return nullptr;
}



bool ValidateFreshMaterialFunctionAsset(UMaterialFunction* SourceFunction, UMaterialFunction* TargetFunction, FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceFunction || !TargetFunction)
    {
        OutDetail = TEXT("source or target material function is null");
        return false;
    }

    if (SourceFunction->GetMaterialFunctionUsage() != TargetFunction->GetMaterialFunctionUsage())
    {
        OutDetail = TEXT("material function usage mismatch");
        return false;
    }

    TArray<FFunctionExpressionInput> SourceInputs;
    TArray<FFunctionExpressionOutput> SourceOutputs;
    TArray<FFunctionExpressionInput> TargetInputs;
    TArray<FFunctionExpressionOutput> TargetOutputs;
    SourceFunction->GetInputsAndOutputs(SourceInputs, SourceOutputs);
    TargetFunction->GetInputsAndOutputs(TargetInputs, TargetOutputs);

    if (SourceInputs.Num() != TargetInputs.Num() || SourceOutputs.Num() != TargetOutputs.Num())
    {
        OutDetail = FString::Printf(
            TEXT("material function io count mismatch: source_inputs=%d target_inputs=%d source_outputs=%d target_outputs=%d"),
            SourceInputs.Num(),
            TargetInputs.Num(),
            SourceOutputs.Num(),
            TargetOutputs.Num());
        return false;
    }

    for (int32 Index = 0; Index < SourceInputs.Num(); ++Index)
    {
        if (SourceInputs[Index].Input.InputName != TargetInputs[Index].Input.InputName)
        {
            OutDetail = FString::Printf(TEXT("material function input name mismatch at index %d"), Index);
            return false;
        }
    }

    for (int32 Index = 0; Index < SourceOutputs.Num(); ++Index)
    {
        if (SourceOutputs[Index].Output.OutputName != TargetOutputs[Index].Output.OutputName)
        {
            OutDetail = FString::Printf(TEXT("material function output name mismatch at index %d"), Index);
            return false;
        }
    }

    return true;
}



bool ValidateFreshMaterialGraphAsset(UObject* SourceObject, UObject* TargetObject, FString& OutDetail, int32& OutExpressionCount)
{
    if (!Check()) return {};

    const TConstArrayView<TObjectPtr<UMaterialExpression>> SourceExpressions = GetMaterialGraphExpressions(SourceObject);
    const TConstArrayView<TObjectPtr<UMaterialExpression>> TargetExpressions = GetMaterialGraphExpressions(TargetObject);
    OutExpressionCount = TargetExpressions.Num();

    if (SourceExpressions.Num() != TargetExpressions.Num())
    {
        OutDetail = FString::Printf(
            TEXT("material graph expression count mismatch: source=%d target=%d"),
            SourceExpressions.Num(),
            TargetExpressions.Num());
        return false;
    }

    for (int32 Index = 0; Index < SourceExpressions.Num(); ++Index)
    {
        UMaterialExpression* SourceExpression = SourceExpressions[Index].Get();
        UMaterialExpression* TargetExpression = TargetExpressions[Index].Get();
        if (!SourceExpression || !TargetExpression)
        {
            OutDetail = FString::Printf(TEXT("material graph expression null at index %d"), Index);
            return false;
        }
        if (SourceExpression->GetClass() != TargetExpression->GetClass())
        {
            OutDetail = FString::Printf(
                TEXT("material graph expression class mismatch at index %d: source=%s target=%s"),
                Index,
                *SourceExpression->GetClass()->GetName(),
                *TargetExpression->GetClass()->GetName());
            return false;
        }
        if (!TargetExpression->IsIn(TargetObject))
        {
            OutDetail = FString::Printf(
                TEXT("material graph expression at index %d is not outered to target asset: %s"),
                Index,
                *TargetExpression->GetPathName());
            return false;
        }
    }

    if (UMaterialFunction* SourceFunction = Cast<UMaterialFunction>(SourceObject))
    {
        if (!ValidateFreshMaterialFunctionAsset(SourceFunction, Cast<UMaterialFunction>(TargetObject), OutDetail))
        {
            return false;
        }
    }

    return true;
}



int32 AddMaterialGraphExpressionReplacementMappings(TMap<UObject*, UObject*>& ReplacementMap, UObject* SourceObject, UObject* TargetObject)
{
    if (!Check()) return {};

    const TConstArrayView<TObjectPtr<UMaterialExpression>> SourceExpressions = GetMaterialGraphExpressions(SourceObject);
    const TConstArrayView<TObjectPtr<UMaterialExpression>> TargetExpressions = GetMaterialGraphExpressions(TargetObject);
    if (SourceExpressions.Num() != TargetExpressions.Num())
    {
        return 0;
    }

    int32 AddedCount = 0;
    for (int32 Index = 0; Index < SourceExpressions.Num(); ++Index)
    {
        UMaterialExpression* SourceExpression = SourceExpressions[Index].Get();
        UMaterialExpression* TargetExpression = TargetExpressions[Index].Get();
        if (!SourceExpression || !TargetExpression || SourceExpression->GetClass() != TargetExpression->GetClass())
        {
            continue;
        }

        AddReplacementMapping(ReplacementMap, SourceExpression, TargetExpression);
        ++AddedCount;
    }
    return AddedCount;
}



void CopyFreshMaterialFunctionSettings(UMaterialFunction* SourceFunction, UMaterialFunction* TargetFunction)
{
    if (!Check()) return;

    if (!SourceFunction || !TargetFunction)
    {
        return;
    }

    TargetFunction->Description = SourceFunction->Description;
    TargetFunction->UserExposedCaption = SourceFunction->UserExposedCaption;
    TargetFunction->bExposeToLibrary = SourceFunction->bExposeToLibrary;
    TargetFunction->bEnableNewHLSLGenerator = SourceFunction->bEnableNewHLSLGenerator;
    TargetFunction->SetMaterialFunctionUsage(SourceFunction->GetMaterialFunctionUsage());
#if WITH_EDITORONLY_DATA
    TargetFunction->LibraryCategoriesText = SourceFunction->LibraryCategoriesText;
    TargetFunction->PreviewBlendMode = SourceFunction->PreviewBlendMode;
    TargetFunction->PreviewMaterialDomain = SourceFunction->PreviewMaterialDomain;
#endif
}



bool CloneMaterialExpressionsIntoFreshGraph(UObject* SourceObject, UObject* TargetObject, FString& OutDetail)
{
    if (!Check()) return {};

    UMaterial* SourceMaterial = Cast<UMaterial>(SourceObject);
    UMaterial* TargetMaterial = Cast<UMaterial>(TargetObject);
    UMaterialFunction* TargetFunction = Cast<UMaterialFunction>(TargetObject);
    if (!SourceObject || !TargetObject || (!TargetMaterial && !TargetFunction))
    {
        OutDetail = TEXT("source or target material graph object is invalid");
        return false;
    }

    const FMaterialExpressionCollection* SourceCollection = GetMaterialGraphExpressionCollection(SourceObject);
    FMaterialExpressionCollection* TargetCollection = GetMutableMaterialGraphExpressionCollection(TargetObject);
    if (!SourceCollection || !TargetCollection)
    {
        OutDetail = TEXT("source or target material graph expression collection is invalid");
        return false;
    }

    TargetCollection->Empty();
    TMap<UMaterialExpression*, UMaterialExpression*> SourceToTargetExpressionMap;
    TMap<UObject*, UObject*> ExpressionReplacementMap;

    for (const TObjectPtr<UMaterialExpression>& SourceExpressionPtr : SourceCollection->Expressions)
    {
        UMaterialExpression* SourceExpression = SourceExpressionPtr.Get();
        if (!SourceExpression)
        {
            OutDetail = TEXT("source material graph expression is null");
            return false;
        }
        if (!SourceExpression->IsAllowedIn(TargetObject))
        {
            OutDetail = FString::Printf(
                TEXT("source material graph expression is not allowed in target asset: %s"),
                *SourceExpression->GetClass()->GetName());
            return false;
        }

        if (!Check()) return {};
        const FName FreshExpressionName = MakeUniqueObjectName(TargetObject, SourceExpression->GetClass(), SourceExpression->GetFName());
        UMaterialExpression* NewExpression = Cast<UMaterialExpression>(CreateOwnedFactory(TargetObject, SourceExpression->GetClass(), FreshExpressionName, TEXT("StaticDuplicateObject"), [&]() { return StaticDuplicateObject(
            SourceExpression,
            TargetObject,
            FreshExpressionName,
            RF_Transactional); }));
        if (!Check()) return {};
        if (!NewExpression)
        {
            OutDetail = FString::Printf(
                TEXT("could not duplicate material graph expression: %s"),
                *SourceExpression->GetPathName());
            return false;
        }

        NewExpression->Material = TargetMaterial;
        NewExpression->Function = TargetFunction;
        TargetCollection->AddExpression(NewExpression);
        SourceToTargetExpressionMap.Add(SourceExpression, NewExpression);
        ExpressionReplacementMap.Add(SourceExpression, NewExpression);
    }

    for (const TObjectPtr<UMaterialExpression>& TargetExpressionPtr : TargetCollection->Expressions)
    {
        UMaterialExpression* NewExpression = TargetExpressionPtr.Get();
        if (!NewExpression)
        {
            continue;
        }

        for (FExpressionInputIterator It{ NewExpression }; It; ++It)
        {
            if (UMaterialExpression* InputExpression = It->Expression)
            {
                if (UMaterialExpression** NewInputExpression = SourceToTargetExpressionMap.Find(InputExpression))
                {
                    It->Expression = *NewInputExpression;
                }
                else if (InputExpression->IsIn(SourceObject))
                {
                    It->Expression = nullptr;
                }
            }
        }

        if (UMaterialExpressionNamedRerouteUsage* NewNamedRerouteUsage = Cast<UMaterialExpressionNamedRerouteUsage>(NewExpression))
        {
            UMaterialExpressionNamedRerouteDeclaration* SourceDeclaration = NewNamedRerouteUsage->Declaration;
            if (SourceDeclaration)
            {
                if (UMaterialExpression** NewDeclaration = SourceToTargetExpressionMap.Find(SourceDeclaration))
                {
                    NewNamedRerouteUsage->Declaration = Cast<UMaterialExpressionNamedRerouteDeclaration>(*NewDeclaration);
                }
                else if (SourceDeclaration->IsIn(SourceObject))
                {
                    NewNamedRerouteUsage->Declaration = nullptr;
                }
            }
        }
    }

    if (SourceMaterial && TargetMaterial)
    {
        for (int32 PropertyIndex = 0; PropertyIndex < static_cast<int32>(MP_MAX); ++PropertyIndex)
        {
            FExpressionInput* SourceInput = SourceMaterial->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex));
            FExpressionInput* TargetInput = TargetMaterial->GetExpressionInputForProperty(static_cast<EMaterialProperty>(PropertyIndex));
            if (!SourceInput || !TargetInput)
            {
                continue;
            }

            *TargetInput = *SourceInput;
            if (TargetInput->Expression)
            {
                if (UMaterialExpression** NewInputExpression = SourceToTargetExpressionMap.Find(TargetInput->Expression))
                {
                    TargetInput->Expression = *NewInputExpression;
                }
                else if (TargetInput->Expression->IsIn(SourceObject))
                {
                    TargetInput->Expression = nullptr;
                }
            }
        }
    }

    for (const TObjectPtr<UMaterialExpressionComment>& SourceCommentPtr : SourceCollection->EditorComments)
    {
        UMaterialExpressionComment* SourceComment = SourceCommentPtr.Get();
        if (!SourceComment)
        {
            continue;
        }

        if (!Check()) return {};
        const FName FreshCommentName = MakeUniqueObjectName(TargetObject, SourceComment->GetClass(), SourceComment->GetFName());
        UMaterialExpressionComment* NewComment = Cast<UMaterialExpressionComment>(CreateOwnedFactory(TargetObject, SourceComment->GetClass(), FreshCommentName, TEXT("StaticDuplicateObject"), [&]() { return StaticDuplicateObject(
            SourceComment,
            TargetObject,
            FreshCommentName,
            RF_Transactional); }));
        if (!Check()) return {};
        if (!NewComment)
        {
            OutDetail = FString::Printf(
                TEXT("could not duplicate material comment: %s"),
                *SourceComment->GetPathName());
            return false;
        }

        NewComment->Material = TargetMaterial;
        NewComment->Function = TargetFunction;
        TargetCollection->AddComment(NewComment);
    }

    if (ExpressionReplacementMap.Num() > 0)
    {
        if (!Write(TargetObject, TEXT("Modify"))) return {};
        TargetObject->Modify();
        if (!After()) return {};
        if (!Check()) return {};
        const int64 ReplacementCount = RemapOwnedReferences(
            TargetObject,
            ExpressionReplacementMap,
            EArchiveReplaceObjectFlags::IgnoreOuterRef |
            EArchiveReplaceObjectFlags::IgnoreArchetypeRef);
        if (!Check()) return {};
        if (ReplacementCount > 0)
        {
            if (!Write(TargetObject, TEXT("MarkPackageDirty"))) return {};
            TargetObject->MarkPackageDirty();
            if (!After()) return {};
        }
    }

    return true;
}



UObject* CreateFreshMaterialGraphAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    const bool bIsMaterial = Cast<UMaterial>(SourceObject) != nullptr;
    const bool bIsMaterialFunction = Cast<UMaterialFunction>(SourceObject) != nullptr;
    if (!bIsMaterial && !bIsMaterialFunction)
    {
        OutDetail = TEXT("source asset is not UMaterial or UMaterialFunction");
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target material graph package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UObject* TargetObject = NewOwnedObject<UObject>(
        TargetPackage,
        SourceObject->GetClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetObject)
    {
        OutDetail = TEXT("could not create target material graph object");
        return nullptr;
    }

    if (!Write(TargetObject, TEXT("PreEditChange"))) return {};
    TargetObject->PreEditChange(nullptr);
    if (!After()) return {};

    if (UMaterial* SourceMaterial = Cast<UMaterial>(SourceObject))
    {
        UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
        CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
        CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
        if (!Check()) return {};
        CopyOwnedProperties(SourceMaterial, TargetObject, CopyParams);
        if (!Check()) return {};

        if (!CloneMaterialExpressionsIntoFreshGraph(SourceMaterial, TargetObject, OutDetail))
        {
            DiscardFreshAssetForFallback(TargetObject);
            return nullptr;
        }
    }
    else if (UMaterialFunction* SourceFunction = Cast<UMaterialFunction>(SourceObject))
    {
        UMaterialFunction* TargetFunction = Cast<UMaterialFunction>(TargetObject);
        if (!TargetFunction || !TargetFunction->GetEditorOnlyData())
        {
            OutDetail = TEXT("target material function editor-only data is invalid");
            DiscardFreshAssetForFallback(TargetObject);
            return nullptr;
        }

        CopyFreshMaterialFunctionSettings(SourceFunction, TargetFunction);
        if (!CloneMaterialExpressionsIntoFreshGraph(SourceFunction, TargetFunction, OutDetail))
        {
            DiscardFreshAssetForFallback(TargetObject);
            return nullptr;
        }
    }

    if (!Write(TargetObject, TEXT("SetFlags"))) return {};
    TargetObject->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("ClearFlags"))) return {};
    TargetObject->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("PostEditChange"))) return {};
    TargetObject->PostEditChange();
    if (!After()) return {};

    if (UMaterial* TargetMaterial = Cast<UMaterial>(TargetObject))
    {
        if (!Write(TargetMaterial, TEXT("UpdateCachedExpressionData"))) return {};
        TargetMaterial->UpdateCachedExpressionData();
        if (!After()) return {};
        if (!Write(TargetMaterial, TEXT("BuildEditorParameterList"))) return {};
        TargetMaterial->BuildEditorParameterList();
        if (!After()) return {};
        TargetMaterial->ForceRecompileForRendering(EMaterialShaderPrecompileMode::None);
    }
    else if (UMaterialFunction* TargetFunction = Cast<UMaterialFunction>(TargetObject))
    {
        FMaterialUpdateContext MaterialUpdateContext(FMaterialUpdateContext::EOptions::Default);
        if (!Write(TargetFunction, TEXT("UpdateFromFunctionResource"))) return {};
        TargetFunction->UpdateFromFunctionResource();
        if (!After()) return {};
        if (!Write(TargetFunction, TEXT("UpdateInputOutputTypes"))) return {};
        TargetFunction->UpdateInputOutputTypes();
        if (!After()) return {};
        if (!Write(TargetFunction, TEXT("UpdateDependentFunctionCandidates"))) return {};
        TargetFunction->UpdateDependentFunctionCandidates();
        if (!After()) return {};
        TargetFunction->ForceRecompileForRendering(MaterialUpdateContext, nullptr);
    }

    int32 ExpressionCount = 0;
    if (!ValidateFreshMaterialGraphAsset(SourceObject, TargetObject, OutDetail, ExpressionCount))
    {
        DiscardFreshAssetForFallback(TargetObject);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetObject, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetObject);
    if (!After()) return {};

    OutDetail = FString::Printf(
        TEXT("fresh_material_graph_copy_from_%s_expressions_%d"),
        *SourceObject->GetName(),
        ExpressionCount);
    return TargetObject;
}



bool ValidateFreshSoundWaveAsset(USoundWave* SourceSound, USoundWave* TargetSound, FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceSound || !TargetSound)
    {
        OutDetail = TEXT("source or target sound wave is null");
        return false;
    }

    if (!TargetSound->RawData.HasPayloadData())
    {
        OutDetail = TEXT("target sound wave has no editor raw payload");
        return false;
    }

    TArray<uint8> SourceRawPCMData;
    uint32 SourceSampleRate = 0;
    uint16 SourceNumChannels = 0;
    if (!SourceSound->GetImportedSoundWaveData(SourceRawPCMData, SourceSampleRate, SourceNumChannels))
    {
        OutDetail = TEXT("could not read source imported sound wave data");
        return false;
    }

    TArray<uint8> TargetRawPCMData;
    uint32 TargetSampleRate = 0;
    uint16 TargetNumChannels = 0;
    if (!TargetSound->GetImportedSoundWaveData(TargetRawPCMData, TargetSampleRate, TargetNumChannels))
    {
        OutDetail = TEXT("could not read target imported sound wave data");
        return false;
    }

    if (SourceSampleRate != TargetSampleRate || SourceNumChannels != TargetNumChannels)
    {
        OutDetail = FString::Printf(
            TEXT("sound wave imported metadata mismatch: source_rate=%u target_rate=%u source_channels=%u target_channels=%u"),
            SourceSampleRate,
            TargetSampleRate,
            SourceNumChannels,
            TargetNumChannels);
        return false;
    }

    if (SourceRawPCMData.Num() != TargetRawPCMData.Num())
    {
        OutDetail = FString::Printf(
            TEXT("sound wave imported pcm size mismatch: source=%d target=%d"),
            SourceRawPCMData.Num(),
            TargetRawPCMData.Num());
        return false;
    }

    if (SourceRawPCMData != TargetRawPCMData)
    {
        OutDetail = TEXT("sound wave imported pcm data mismatch");
        return false;
    }

    if (!FMath::IsNearlyEqual(SourceSound->Duration, TargetSound->Duration, 0.001f))
    {
        OutDetail = FString::Printf(
            TEXT("sound wave duration mismatch: source=%.6f target=%.6f"),
            SourceSound->Duration,
            TargetSound->Duration);
        return false;
    }

    if (!FMath::IsNearlyEqual(SourceSound->GetSampleRateForCurrentPlatform(), TargetSound->GetSampleRateForCurrentPlatform(), 0.1f) ||
        SourceSound->NumChannels != TargetSound->NumChannels)
    {
        OutDetail = FString::Printf(
            TEXT("sound wave runtime metadata mismatch: source_rate=%.3f target_rate=%.3f source_channels=%d target_channels=%d"),
            SourceSound->GetSampleRateForCurrentPlatform(),
            TargetSound->GetSampleRateForCurrentPlatform(),
            SourceSound->NumChannels,
            TargetSound->NumChannels);
        return false;
    }

    if (SourceSound->IsLooping() != TargetSound->IsLooping())
    {
        OutDetail = TEXT("sound wave looping flag mismatch");
        return false;
    }

    return true;
}



UObject* CreateFreshSoundWaveAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    USoundWave* SourceSound = Cast<USoundWave>(SourceObject);
    if (!SourceSound)
    {
        OutDetail = TEXT("source asset is not USoundWave");
        return nullptr;
    }

    if (!SourceSound->RawData.HasPayloadData())
    {
        OutDetail = TEXT("source sound wave has no editor raw payload");
        return nullptr;
    }

    TArray<uint8> SourceRawPCMData;
    uint32 SourceSampleRate = 0;
    uint16 SourceNumChannels = 0;
    if (!SourceSound->GetImportedSoundWaveData(SourceRawPCMData, SourceSampleRate, SourceNumChannels))
    {
        OutDetail = TEXT("could not read source imported sound wave data");
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target sound wave package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UObject* TargetObject = NewOwnedObject<UObject>(
        TargetPackage,
        SourceObject->GetClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    USoundWave* TargetSound = Cast<USoundWave>(TargetObject);
    if (!TargetSound)
    {
        OutDetail = TEXT("could not create target sound wave object");
        return nullptr;
    }

    if (!Write(TargetSound, TEXT("PreEditChange"))) return {};
    TargetSound->PreEditChange(nullptr);
    if (!After()) return {};

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceSound, TargetSound, CopyParams);
    if (!Check()) return {};

    TFuture<FSharedBuffer> SourcePayloadFuture = SourceSound->RawData.GetPayload();
    const FSharedBuffer SourcePayload = SourcePayloadFuture.Get();
    if (SourcePayload.GetSize() <= 0)
    {
        OutDetail = TEXT("source sound wave raw payload is empty");
        DiscardFreshAssetForFallback(TargetSound);
        return nullptr;
    }

    TargetSound->RawData.UpdatePayload(FSharedBuffer::Clone(SourcePayload.GetData(), SourcePayload.GetSize()), TargetSound);
    TargetSound->SetImportedSampleRate(SourceSound->GetImportedSampleRate());
    if (SourceSampleRate > 0)
    {
        TargetSound->SetSampleRate(SourceSampleRate);
    }

    TargetSound->InvalidateCompressedData(true, true);
    if (!Write(TargetSound, TEXT("SetFlags"))) return {};
    TargetSound->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetSound, TEXT("ClearFlags"))) return {};
    TargetSound->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetSound, TEXT("PostEditChange"))) return {};
    TargetSound->PostEditChange();
    if (!After()) return {};
    FinishCompilationForObjectForMCP(TargetSound);

    if (!ValidateFreshSoundWaveAsset(SourceSound, TargetSound, OutDetail))
    {
        DiscardFreshAssetForFallback(TargetSound);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetSound, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetSound);
    if (!After()) return {};

    OutDetail = FString::Printf(
        TEXT("fresh_sound_wave_payload_copy_from_%s_bytes_%lld_pcm_%d_rate_%u_channels_%u"),
        *SourceSound->GetName(),
        static_cast<int64>(SourcePayload.GetSize()),
        SourceRawPCMData.Num(),
        SourceSampleRate,
        SourceNumChannels);
    return TargetSound;
}



bool ValidateFreshTextureSourceCopy(
    UTexture* SourceTexture,
    UTexture* TargetTexture,
    const FString& DetailPrefix,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceTexture || !TargetTexture)
    {
        OutDetail = TEXT("source or target texture is null");
        return false;
    }

#if WITH_EDITORONLY_DATA
    FTextureSource& Source = SourceTexture->Source;
    FTextureSource& Target = TargetTexture->Source;
    if (!Source.IsValid() || !Target.IsValid())
    {
        OutDetail = TEXT("source or target texture has no valid editor source data");
        return false;
    }

    if (SourceTexture->SRGB != TargetTexture->SRGB)
    {
        OutDetail = FString::Printf(
            TEXT("texture SRGB mismatch: source=%s target=%s"),
            SourceTexture->SRGB ? TEXT("true") : TEXT("false"),
            TargetTexture->SRGB ? TEXT("true") : TEXT("false"));
        return false;
    }

    if (Source.GetSizeX() != Target.GetSizeX() ||
        Source.GetSizeY() != Target.GetSizeY() ||
        Source.GetNumSlices() != Target.GetNumSlices() ||
        Source.GetNumMips() != Target.GetNumMips() ||
        Source.GetNumLayers() != Target.GetNumLayers() ||
        Source.GetNumBlocks() != Target.GetNumBlocks())
    {
        OutDetail = FString::Printf(
            TEXT("texture source shape mismatch: source=%lldx%lld slices=%d mips=%d layers=%d blocks=%d target=%lldx%lld slices=%d mips=%d layers=%d blocks=%d"),
            static_cast<int64>(Source.GetSizeX()),
            static_cast<int64>(Source.GetSizeY()),
            Source.GetNumSlices(),
            Source.GetNumMips(),
            Source.GetNumLayers(),
            Source.GetNumBlocks(),
            static_cast<int64>(Target.GetSizeX()),
            static_cast<int64>(Target.GetSizeY()),
            Target.GetNumSlices(),
            Target.GetNumMips(),
            Target.GetNumLayers(),
            Target.GetNumBlocks());
        return false;
    }

    for (int32 LayerIndex = 0; LayerIndex < Source.GetNumLayers(); ++LayerIndex)
    {
        if (Source.GetFormat(LayerIndex) != Target.GetFormat(LayerIndex))
        {
            OutDetail = FString::Printf(TEXT("texture source layer format mismatch at layer %d"), LayerIndex);
            return false;
        }
    }

    for (int32 BlockIndex = 0; BlockIndex < Source.GetNumBlocks(); ++BlockIndex)
    {
        FTextureSourceBlock SourceBlock;
        FTextureSourceBlock TargetBlock;
        Source.GetBlock(BlockIndex, SourceBlock);
        Target.GetBlock(BlockIndex, TargetBlock);
        if (SourceBlock.BlockX != TargetBlock.BlockX ||
            SourceBlock.BlockY != TargetBlock.BlockY ||
            SourceBlock.SizeX != TargetBlock.SizeX ||
            SourceBlock.SizeY != TargetBlock.SizeY ||
            SourceBlock.NumSlices != TargetBlock.NumSlices ||
            SourceBlock.NumMips != TargetBlock.NumMips)
        {
            OutDetail = FString::Printf(TEXT("texture source block mismatch at block %d"), BlockIndex);
            return false;
        }

        for (int32 LayerIndex = 0; LayerIndex < Source.GetNumLayers(); ++LayerIndex)
        {
            for (int32 MipIndex = 0; MipIndex < SourceBlock.NumMips; ++MipIndex)
            {
                TArray64<uint8> SourceMipData;
                TArray64<uint8> TargetMipData;
                if (!Source.GetMipData(SourceMipData, BlockIndex, LayerIndex, MipIndex) ||
                    !Target.GetMipData(TargetMipData, BlockIndex, LayerIndex, MipIndex))
                {
                    OutDetail = FString::Printf(
                        TEXT("texture source mip data read failed at block=%d layer=%d mip=%d"),
                        BlockIndex,
                        LayerIndex,
                        MipIndex);
                    return false;
                }

                if (SourceMipData.Num() != TargetMipData.Num() || SourceMipData != TargetMipData)
                {
                    OutDetail = FString::Printf(
                        TEXT("texture source mip data mismatch at block=%d layer=%d mip=%d source_bytes=%lld target_bytes=%lld"),
                        BlockIndex,
                        LayerIndex,
                        MipIndex,
                        SourceMipData.Num(),
                        TargetMipData.Num());
                    return false;
                }
            }
        }
    }

    OutDetail = FString::Printf(
        TEXT("%s_from_%s_%lldx%lld_mips_%d_slices_%d_layers_%d_blocks_%d"),
        *DetailPrefix,
        *SourceTexture->GetName(),
        static_cast<int64>(Source.GetSizeX()),
        static_cast<int64>(Source.GetSizeY()),
        Source.GetNumMips(),
        Source.GetNumSlices(),
        Source.GetNumLayers(),
        Source.GetNumBlocks());
    return true;
#else
    OutDetail = TEXT("texture source validation requires editor source data");
    return false;
#endif
}



UObject* CreateFreshTextureCubeAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    UTextureCube* SourceTexture = Cast<UTextureCube>(SourceObject);
    if (!SourceTexture)
    {
        OutDetail = TEXT("source asset is not UTextureCube");
        return nullptr;
    }
    SourceTexture->BlockOnAnyAsyncBuild();

#if WITH_EDITORONLY_DATA
    if (!SourceTexture->Source.IsValid())
    {
        OutDetail = TEXT("source texture cube has no valid editor source data");
        return nullptr;
    }
#else
    OutDetail = TEXT("texture cube source recreation requires editor source data");
    return nullptr;
#endif

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target texture cube package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UTextureCube* TargetTexture = NewOwnedObject<UTextureCube>(
        TargetPackage,
        UTextureCube::StaticClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetTexture)
    {
        OutDetail = TEXT("could not create target texture cube object");
        return nullptr;
    }

    if (!Write(TargetTexture, TEXT("PreEditChange"))) return {};
    TargetTexture->PreEditChange(nullptr);
    if (!After()) return {};

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceTexture, TargetTexture, CopyParams);
    if (!Check()) return {};

#if WITH_EDITORONLY_DATA
    TargetTexture->Source = SourceTexture->Source.CopyTornOff();
    TargetTexture->Source.SetOwner(TargetTexture);
#endif

    if (!Write(TargetTexture, TEXT("SetFlags"))) return {};
    TargetTexture->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("ClearFlags"))) return {};
    TargetTexture->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("PostEditChange"))) return {};
    TargetTexture->PostEditChange();
    if (!After()) return {};
    TargetTexture->BlockOnAnyAsyncBuild();

    FString ValidationDetail;
    if (!ValidateFreshTextureSourceCopy(SourceTexture, TargetTexture, TEXT("fresh_texturecube_source_copy"), ValidationDetail))
    {
        OutDetail = ValidationDetail;
        DiscardFreshAssetForFallback(TargetTexture);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetTexture);
    if (!After()) return {};

    OutDetail = ValidationDetail;
    return TargetTexture;
}



UObject* CreateFreshVolumeTextureAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    UVolumeTexture* SourceTexture = Cast<UVolumeTexture>(SourceObject);
    if (!SourceTexture)
    {
        OutDetail = TEXT("source asset is not UVolumeTexture");
        return nullptr;
    }
    SourceTexture->BlockOnAnyAsyncBuild();

#if WITH_EDITORONLY_DATA
    if (!SourceTexture->Source.IsValid())
    {
        OutDetail = TEXT("source volume texture has no valid editor source data");
        return nullptr;
    }
#else
    OutDetail = TEXT("volume texture source recreation requires editor source data");
    return nullptr;
#endif

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target volume texture package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UVolumeTexture* TargetTexture = NewOwnedObject<UVolumeTexture>(
        TargetPackage,
        UVolumeTexture::StaticClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetTexture)
    {
        OutDetail = TEXT("could not create target volume texture object");
        return nullptr;
    }

    if (!Write(TargetTexture, TEXT("PreEditChange"))) return {};
    TargetTexture->PreEditChange(nullptr);
    if (!After()) return {};

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceTexture, TargetTexture, CopyParams);
    if (!Check()) return {};

#if WITH_EDITORONLY_DATA
    TargetTexture->Source = SourceTexture->Source.CopyTornOff();
    TargetTexture->Source.SetOwner(TargetTexture);
#endif

    if (!Write(TargetTexture, TEXT("SetFlags"))) return {};
    TargetTexture->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("ClearFlags"))) return {};
    TargetTexture->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("PostEditChange"))) return {};
    TargetTexture->PostEditChange();
    if (!After()) return {};
    TargetTexture->BlockOnAnyAsyncBuild();

    FString ValidationDetail;
    if (!ValidateFreshTextureSourceCopy(SourceTexture, TargetTexture, TEXT("fresh_volumetexture_source_copy"), ValidationDetail))
    {
        OutDetail = ValidationDetail;
        DiscardFreshAssetForFallback(TargetTexture);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetTexture);
    if (!After()) return {};

    OutDetail = ValidationDetail;
    return TargetTexture;
}



UObject* CreateFreshTexture2DAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    UTexture2D* SourceTexture = Cast<UTexture2D>(SourceObject);
    if (!SourceTexture)
    {
        OutDetail = TEXT("source asset is not UTexture2D");
        return nullptr;
    }
    SourceTexture->BlockOnAnyAsyncBuild();

#if WITH_EDITORONLY_DATA
    if (!SourceTexture->Source.IsValid())
    {
        OutDetail = TEXT("source texture has no valid editor source data");
        return nullptr;
    }
#else
    OutDetail = TEXT("texture source recreation requires editor source data");
    return nullptr;
#endif

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target texture package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UTexture2D* TargetTexture = NewOwnedObject<UTexture2D>(
        TargetPackage,
        UTexture2D::StaticClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetTexture)
    {
        OutDetail = TEXT("could not create target texture object");
        return nullptr;
    }

    if (!Write(TargetTexture, TEXT("PreEditChange"))) return {};
    TargetTexture->PreEditChange(nullptr);
    if (!After()) return {};

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceTexture, TargetTexture, CopyParams);
    if (!Check()) return {};

#if WITH_EDITORONLY_DATA
    TargetTexture->Source = SourceTexture->Source.CopyTornOff();
    TargetTexture->Source.SetOwner(TargetTexture);
#endif

    if (!Write(TargetTexture, TEXT("SetFlags"))) return {};
    TargetTexture->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("ClearFlags"))) return {};
    TargetTexture->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("PostEditChange"))) return {};
    TargetTexture->PostEditChange();
    if (!After()) return {};
    TargetTexture->BlockOnAnyAsyncBuild();

    FString ValidationDetail;
    if (!ValidateFreshTextureSourceCopy(SourceTexture, TargetTexture, TEXT("fresh_texture2d_source_copy"), ValidationDetail))
    {
        OutDetail = ValidationDetail;
        DiscardFreshAssetForFallback(TargetTexture);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetTexture, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetTexture);
    if (!After()) return {};

    OutDetail = ValidationDetail;
    return TargetTexture;
}



bool ArePerPlatformFloatsEqualForMCP(const FPerPlatformFloat& A, const FPerPlatformFloat& B)
{
    if (!Check()) return {};

    if (!FMath::IsNearlyEqual(A.Default, B.Default))
    {
        return false;
    }
#if WITH_EDITORONLY_DATA
    if (A.PerPlatform.Num() != B.PerPlatform.Num())
    {
        return false;
    }
    for (const TPair<FName, float>& Pair : A.PerPlatform)
    {
        const float* OtherValue = B.PerPlatform.Find(Pair.Key);
        if (!OtherValue || !FMath::IsNearlyEqual(Pair.Value, *OtherValue))
        {
            return false;
        }
    }
#endif
    return true;
}



UObject* ResolveReplacementOrOriginalForMCP(const TMap<UObject*, UObject*>* OptionalReplacementMap, UObject* SourceObject)
{
    if (!Check()) return {};

    if (!SourceObject)
    {
        return nullptr;
    }
    if (OptionalReplacementMap)
    {
        if (UObject* Replacement = FindReplacementObject(*OptionalReplacementMap, SourceObject))
        {
            return Replacement;
        }
    }
    return SourceObject;
}



UMaterialInterface* ResolveMaterialReplacementForMCP(
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    UMaterialInterface* SourceMaterial)
{
    if (!Check()) return {};

    return Cast<UMaterialInterface>(ResolveReplacementOrOriginalForMCP(OptionalReplacementMap, SourceMaterial));
}



UStaticMesh* ResolveStaticMeshReplacementForMCP(
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    UStaticMesh* SourceMesh)
{
    if (!Check()) return {};

    return Cast<UStaticMesh>(ResolveReplacementOrOriginalForMCP(OptionalReplacementMap, SourceMesh));
}



void FinishCompilationForObjectForMCP(UObject* Object)
{
    if (!Check()) return;

    if (!Object)
    {
        return;
    }

    TArray<UObject*> Objects;
    Objects.Add(Object);
    FAssetCompilingManager::Get().FinishCompilationForObjects(Objects);
}



bool CompareStaticMeshDescriptionShapeForMCP(
    const FMeshDescription& SourceDescription,
    const FMeshDescription& TargetDescription,
    int32 LodIndex,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (SourceDescription.Vertices().Num() != TargetDescription.Vertices().Num() ||
        SourceDescription.VertexInstances().Num() != TargetDescription.VertexInstances().Num() ||
        SourceDescription.Edges().Num() != TargetDescription.Edges().Num() ||
        SourceDescription.Triangles().Num() != TargetDescription.Triangles().Num() ||
        SourceDescription.Polygons().Num() != TargetDescription.Polygons().Num() ||
        SourceDescription.PolygonGroups().Num() != TargetDescription.PolygonGroups().Num())
    {
        OutDetail = FString::Printf(
            TEXT("static mesh LOD %d mesh description mismatch: source vertices=%d instances=%d edges=%d triangles=%d polygons=%d groups=%d target vertices=%d instances=%d edges=%d triangles=%d polygons=%d groups=%d"),
            LodIndex,
            SourceDescription.Vertices().Num(),
            SourceDescription.VertexInstances().Num(),
            SourceDescription.Edges().Num(),
            SourceDescription.Triangles().Num(),
            SourceDescription.Polygons().Num(),
            SourceDescription.PolygonGroups().Num(),
            TargetDescription.Vertices().Num(),
            TargetDescription.VertexInstances().Num(),
            TargetDescription.Edges().Num(),
            TargetDescription.Triangles().Num(),
            TargetDescription.Polygons().Num(),
            TargetDescription.PolygonGroups().Num());
        return false;
    }
    return true;
}



bool GetStaticMeshDescriptionForFreshCopyForMCP(
    UStaticMesh* Mesh,
    int32 LodIndex,
    FMeshDescription& OutDescription,
    bool& bOutUsedRenderExport,
    FString& OutDetail)
{
    if (!Check()) return {};

    bOutUsedRenderExport = false;
    if (!Mesh)
    {
        OutDetail = TEXT("static mesh is null");
        return false;
    }

    if (Mesh->CloneMeshDescription(LodIndex, OutDescription))
    {
        return true;
    }

    if (LodIndex <= 0 || LodIndex >= Mesh->GetNumLODs() || !Mesh->HasValidRenderData(true, LodIndex))
    {
        OutDetail = FString::Printf(TEXT("could not clone source static mesh description at LOD %d"), LodIndex);
        return false;
    }


    Mesh->ExportStaticMeshLOD(Mesh->GetLODForExport(LodIndex), OutDescription);
    if (OutDescription.Vertices().Num() <= 0 || OutDescription.Triangles().Num() <= 0)
    {
        OutDetail = FString::Printf(TEXT("could not export source static mesh render LOD %d to mesh description"), LodIndex);
        return false;
    }

    bOutUsedRenderExport = true;
    return true;

}



bool CompareStaticMeshRenderStatsForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail)
{
    if (!Check()) return {};

    const int32 SourceRenderLODCount = SourceMesh->GetNumLODs();
    const int32 TargetRenderLODCount = TargetMesh->GetNumLODs();
    if (SourceRenderLODCount != TargetRenderLODCount)
    {
        OutDetail = FString::Printf(TEXT("static mesh render LOD count mismatch: source=%d target=%d"), SourceRenderLODCount, TargetRenderLODCount);
        return false;
    }

    for (int32 LodIndex = 0; LodIndex < SourceRenderLODCount; ++LodIndex)
    {
        if (SourceMesh->GetNumVertices(LodIndex) != TargetMesh->GetNumVertices(LodIndex) ||
            SourceMesh->GetNumTriangles(LodIndex) != TargetMesh->GetNumTriangles(LodIndex) ||
            SourceMesh->GetNumSections(LodIndex) != TargetMesh->GetNumSections(LodIndex) ||
            SourceMesh->GetNumUVChannels(LodIndex) != TargetMesh->GetNumUVChannels(LodIndex))
        {
            OutDetail = FString::Printf(
                TEXT("static mesh render LOD %d mismatch: source vertices=%d triangles=%d sections=%d uv=%d target vertices=%d triangles=%d sections=%d uv=%d"),
                LodIndex,
                SourceMesh->GetNumVertices(LodIndex),
                SourceMesh->GetNumTriangles(LodIndex),
                SourceMesh->GetNumSections(LodIndex),
                SourceMesh->GetNumUVChannels(LodIndex),
                TargetMesh->GetNumVertices(LodIndex),
                TargetMesh->GetNumTriangles(LodIndex),
                TargetMesh->GetNumSections(LodIndex),
                TargetMesh->GetNumUVChannels(LodIndex));
            return false;
        }
    }

    return true;
}



bool CompareStaticMeshSectionInfoMapForMCP(
    const FMeshSectionInfoMap& SourceMap,
    const FMeshSectionInfoMap& TargetMap,
    const TCHAR* Label,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (SourceMap.Map.Num() != TargetMap.Map.Num())
    {
        OutDetail = FString::Printf(
            TEXT("static mesh %s section map count mismatch: source=%d target=%d"),
            Label,
            SourceMap.Map.Num(),
            TargetMap.Map.Num());
        return false;
    }

    for (const TPair<uint32, FMeshSectionInfo>& SourcePair : SourceMap.Map)
    {
        const FMeshSectionInfo* TargetInfo = TargetMap.Map.Find(SourcePair.Key);
        if (!TargetInfo)
        {
            OutDetail = FString::Printf(TEXT("static mesh %s section map missing key %u"), Label, SourcePair.Key);
            return false;
        }
        if (SourcePair.Value.MaterialIndex != TargetInfo->MaterialIndex ||
            SourcePair.Value.bEnableCollision != TargetInfo->bEnableCollision ||
            SourcePair.Value.bCastShadow != TargetInfo->bCastShadow ||
            SourcePair.Value.bVisibleInRayTracing != TargetInfo->bVisibleInRayTracing ||
            SourcePair.Value.bAffectDistanceFieldLighting != TargetInfo->bAffectDistanceFieldLighting ||
            SourcePair.Value.bForceOpaque != TargetInfo->bForceOpaque)
        {
            OutDetail = FString::Printf(TEXT("static mesh %s section map value mismatch at key %u"), Label, SourcePair.Key);
            return false;
        }
    }
    return true;
}



bool CompareStaticMeshMaterialsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    const TArray<FStaticMaterial>& SourceMaterials = SourceMesh->GetStaticMaterials();
    const TArray<FStaticMaterial>& TargetMaterials = TargetMesh->GetStaticMaterials();
    if (SourceMaterials.Num() != TargetMaterials.Num())
    {
        OutDetail = FString::Printf(
            TEXT("static mesh material slot count mismatch: source=%d target=%d"),
            SourceMaterials.Num(),
            TargetMaterials.Num());
        return false;
    }

    for (int32 Index = 0; Index < SourceMaterials.Num(); ++Index)
    {
        const FStaticMaterial& SourceMaterial = SourceMaterials[Index];
        const FStaticMaterial& TargetMaterial = TargetMaterials[Index];
        if (SourceMaterial.MaterialSlotName != TargetMaterial.MaterialSlotName ||
            SourceMaterial.ImportedMaterialSlotName != TargetMaterial.ImportedMaterialSlotName)
        {
            OutDetail = FString::Printf(TEXT("static mesh material slot name mismatch at index %d"), Index);
            return false;
        }

        UMaterialInterface* ExpectedMaterial = ResolveMaterialReplacementForMCP(OptionalReplacementMap, SourceMaterial.MaterialInterface);
        UMaterialInterface* ExpectedOverlayMaterial = ResolveMaterialReplacementForMCP(OptionalReplacementMap, SourceMaterial.OverlayMaterialInterface);
        if (TargetMaterial.MaterialInterface != ExpectedMaterial ||
            TargetMaterial.OverlayMaterialInterface != ExpectedOverlayMaterial)
        {
            OutDetail = FString::Printf(TEXT("static mesh material reference mismatch at index %d"), Index);
            return false;
        }
    }
    return true;
}



bool CompareStaticMeshSocketsForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail)
{
    if (!Check()) return {};

    if (SourceMesh->Sockets.Num() != TargetMesh->Sockets.Num())
    {
        OutDetail = FString::Printf(
            TEXT("static mesh socket count mismatch: source=%d target=%d"),
            SourceMesh->Sockets.Num(),
            TargetMesh->Sockets.Num());
        return false;
    }

    for (const TObjectPtr<UStaticMeshSocket>& SourceSocketPtr : SourceMesh->Sockets)
    {
        const UStaticMeshSocket* SourceSocket = SourceSocketPtr.Get();
        if (!SourceSocket)
        {
            continue;
        }

        const UStaticMeshSocket* TargetSocket = TargetMesh->FindSocket(SourceSocket->SocketName);
        if (!TargetSocket)
        {
            OutDetail = FString::Printf(TEXT("static mesh missing socket: %s"), *SourceSocket->SocketName.ToString());
            return false;
        }

        if (TargetSocket->GetOuter() != TargetMesh ||
            TargetSocket->RelativeLocation != SourceSocket->RelativeLocation ||
            TargetSocket->RelativeRotation != SourceSocket->RelativeRotation ||
            TargetSocket->RelativeScale != SourceSocket->RelativeScale ||
            TargetSocket->Tag != SourceSocket->Tag)
        {
            OutDetail = FString::Printf(TEXT("static mesh socket data mismatch: %s"), *SourceSocket->SocketName.ToString());
            return false;
        }
    }
    return true;
}



int32 GetBodySetupCollisionElementCountForMCP(const UBodySetup* BodySetup)
{
    if (!Check()) return {};

    return BodySetup ? BodySetup->AggGeom.GetElementCount() : 0;
}



bool CompareStaticMeshBodySetupForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail)
{
    if (!Check()) return {};

    const UBodySetup* SourceBodySetup = SourceMesh->GetBodySetup();
    const UBodySetup* TargetBodySetup = TargetMesh->GetBodySetup();
    if (!SourceBodySetup && !TargetBodySetup)
    {
        return true;
    }
    if (!SourceBodySetup || !TargetBodySetup)
    {
        OutDetail = TEXT("static mesh body setup presence mismatch");
        return false;
    }
    if (TargetBodySetup == SourceBodySetup || TargetBodySetup->GetOuter() != TargetMesh)
    {
        OutDetail = TEXT("static mesh body setup was not recreated under target mesh");
        return false;
    }
    if (GetBodySetupCollisionElementCountForMCP(SourceBodySetup) != GetBodySetupCollisionElementCountForMCP(TargetBodySetup))
    {
        OutDetail = FString::Printf(
            TEXT("static mesh collision element count mismatch: source=%d target=%d"),
            GetBodySetupCollisionElementCountForMCP(SourceBodySetup),
            GetBodySetupCollisionElementCountForMCP(TargetBodySetup));
        return false;
    }
    if (SourceBodySetup->GetCollisionTraceFlag() != TargetBodySetup->GetCollisionTraceFlag() ||
        SourceBodySetup->bDoubleSidedGeometry != TargetBodySetup->bDoubleSidedGeometry)
    {
        OutDetail = TEXT("static mesh body setup collision settings mismatch");
        return false;
    }
    return true;
}



bool ValidateFreshStaticMeshAsset(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceMesh || !TargetMesh)
    {
        OutDetail = TEXT("source or target static mesh is null");
        return false;
    }

    FinishCompilationForObjectForMCP(SourceMesh);
    FinishCompilationForObjectForMCP(TargetMesh);

    const int32 SourceLODCount = SourceMesh->GetNumSourceModels();
    const int32 TargetLODCount = TargetMesh->GetNumSourceModels();
    if (SourceLODCount != TargetLODCount)
    {
        OutDetail = FString::Printf(TEXT("static mesh LOD count mismatch: source=%d target=%d"), SourceLODCount, TargetLODCount);
        return false;
    }

    int32 RenderExportValidatedLODCount = 0;
    for (int32 LodIndex = 0; LodIndex < SourceLODCount; ++LodIndex)
    {
        FMeshDescription SourceDescription;
        FMeshDescription TargetDescription;
        bool bSourceUsedRenderExport = false;
        bool bTargetUsedRenderExport = false;
        if (!GetStaticMeshDescriptionForFreshCopyForMCP(SourceMesh, LodIndex, SourceDescription, bSourceUsedRenderExport, OutDetail) ||
            !GetStaticMeshDescriptionForFreshCopyForMCP(TargetMesh, LodIndex, TargetDescription, bTargetUsedRenderExport, OutDetail))
        {
            return false;
        }
        if (!CompareStaticMeshDescriptionShapeForMCP(SourceDescription, TargetDescription, LodIndex, OutDetail))
        {
            return false;
        }
        if (bSourceUsedRenderExport || bTargetUsedRenderExport)
        {
            ++RenderExportValidatedLODCount;
        }
        if (SourceMesh->GetNumUVChannels(LodIndex) != TargetMesh->GetNumUVChannels(LodIndex))
        {
            OutDetail = FString::Printf(
                TEXT("static mesh UV channel count mismatch at LOD %d: source=%d target=%d"),
                LodIndex,
                SourceMesh->GetNumUVChannels(LodIndex),
                TargetMesh->GetNumUVChannels(LodIndex));
            return false;
        }

        const FStaticMeshSourceModel& SourceModel = SourceMesh->GetSourceModel(LodIndex);
        const FStaticMeshSourceModel& TargetModel = TargetMesh->GetSourceModel(LodIndex);
        FMeshBuildSettings ExpectedBuildSettings = SourceModel.BuildSettings;
        ExpectedBuildSettings.DistanceFieldReplacementMesh = ResolveStaticMeshReplacementForMCP(
            OptionalReplacementMap,
            ExpectedBuildSettings.DistanceFieldReplacementMesh);
        if (TargetModel.BuildSettings != ExpectedBuildSettings)
        {
            OutDetail = FString::Printf(TEXT("static mesh build settings mismatch at LOD %d"), LodIndex);
            return false;
        }
        if (TargetModel.ReductionSettings != SourceModel.ReductionSettings)
        {
            OutDetail = FString::Printf(TEXT("static mesh reduction settings mismatch at LOD %d"), LodIndex);
            return false;
        }
        if (!ArePerPlatformFloatsEqualForMCP(SourceModel.ScreenSize, TargetModel.ScreenSize))
        {
            OutDetail = FString::Printf(TEXT("static mesh screen size mismatch at LOD %d"), LodIndex);
            return false;
        }
        if (TargetModel.SourceImportFilename != SourceModel.SourceImportFilename)
        {
            OutDetail = FString::Printf(TEXT("static mesh source import filename mismatch at LOD %d"), LodIndex);
            return false;
        }
#if WITH_EDITORONLY_DATA
        if (TargetModel.bImportWithBaseMesh != SourceModel.bImportWithBaseMesh)
        {
            OutDetail = FString::Printf(TEXT("static mesh import-with-base flag mismatch at LOD %d"), LodIndex);
            return false;
        }
#endif
    }

    if (!CompareStaticMeshRenderStatsForMCP(SourceMesh, TargetMesh, OutDetail))
    {
        return false;
    }

    if (!CompareStaticMeshMaterialsForMCP(SourceMesh, TargetMesh, OptionalReplacementMap, OutDetail) ||
        !CompareStaticMeshSectionInfoMapForMCP(SourceMesh->GetSectionInfoMap(), TargetMesh->GetSectionInfoMap(), TEXT("current"), OutDetail) ||
        !CompareStaticMeshSectionInfoMapForMCP(SourceMesh->GetOriginalSectionInfoMap(), TargetMesh->GetOriginalSectionInfoMap(), TEXT("original"), OutDetail) ||
        !CompareStaticMeshSocketsForMCP(SourceMesh, TargetMesh, OutDetail) ||
        !CompareStaticMeshBodySetupForMCP(SourceMesh, TargetMesh, OutDetail))
    {
        return false;
    }

#if WITH_EDITORONLY_DATA
    UStaticMesh* ExpectedComplexCollisionMesh = ResolveStaticMeshReplacementForMCP(OptionalReplacementMap, SourceMesh->ComplexCollisionMesh);
    if (TargetMesh->ComplexCollisionMesh != ExpectedComplexCollisionMesh)
    {
        OutDetail = TEXT("static mesh complex collision mesh mismatch");
        return false;
    }
#endif


    if (TargetMesh->GetNaniteSettings() != SourceMesh->GetNaniteSettings())

    {
        OutDetail = TEXT("static mesh nanite settings mismatch");
        return false;
    }

    OutDetail = FString::Printf(
        TEXT("fresh_static_mesh_from_%s_lods_%d_materials_%d_sockets_%d_collision_shapes_%d"),
        *SourceMesh->GetName(),
        SourceLODCount,
        SourceMesh->GetStaticMaterials().Num(),
        SourceMesh->Sockets.Num(),
        GetBodySetupCollisionElementCountForMCP(SourceMesh->GetBodySetup()));
    if (RenderExportValidatedLODCount > 0)
    {
        OutDetail += FString::Printf(TEXT("_render_export_validated_lods_%d"), RenderExportValidatedLODCount);
    }
    return true;
}



void CopyStaticMeshMaterialsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap)
{
    if (!Check()) return;

    TArray<FStaticMaterial> TargetMaterials = SourceMesh->GetStaticMaterials();
    for (FStaticMaterial& TargetMaterial : TargetMaterials)
    {
        TargetMaterial.MaterialInterface = ResolveMaterialReplacementForMCP(OptionalReplacementMap, TargetMaterial.MaterialInterface);
        TargetMaterial.OverlayMaterialInterface = ResolveMaterialReplacementForMCP(OptionalReplacementMap, TargetMaterial.OverlayMaterialInterface);
    }
    TargetMesh->SetStaticMaterials(TargetMaterials);
}



bool CopyStaticMeshBodySetupForMCP(UStaticMesh* SourceMesh, UStaticMesh* TargetMesh, FString& OutDetail)
{
    if (!Check()) return {};

    UBodySetup* SourceBodySetup = SourceMesh ? SourceMesh->GetBodySetup() : nullptr;
    if (!SourceBodySetup)
    {
        TargetMesh->SetBodySetup(nullptr);
        return true;
    }

    const FName BodySetupName = MakeUniqueObjectName(TargetMesh, UBodySetup::StaticClass(), UStaticMesh::GetBodySetupName());
    if (!Check()) return {};
    UBodySetup* TargetBodySetup = Cast<UBodySetup>(CreateOwnedFactory(TargetMesh, SourceBodySetup->GetClass(), BodySetupName, TEXT("StaticDuplicateObject"), [&]() { return StaticDuplicateObject(
        SourceBodySetup,
        TargetMesh,
        BodySetupName,
        RF_Transactional); }));
    if (!Check()) return {};
    if (!TargetBodySetup)
    {
        OutDetail = TEXT("could not duplicate static mesh body setup");
        return false;
    }
    if (!Write(TargetBodySetup, TEXT("ClearFlags"))) return {};
    TargetBodySetup->ClearFlags(RF_Transient);
    if (!After()) return {};
    TargetMesh->SetBodySetup(TargetBodySetup);
    return true;
}



bool CopyStaticMeshSocketsForMCP(
    UStaticMesh* SourceMesh,
    UStaticMesh* TargetMesh,
    const TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    TargetMesh->Sockets.Reset();
    for (const TObjectPtr<UStaticMeshSocket>& SourceSocketPtr : SourceMesh->Sockets)
    {
        UStaticMeshSocket* SourceSocket = SourceSocketPtr.Get();
        if (!SourceSocket)
        {
            continue;
        }

        const FName SocketObjectName = MakeUniqueObjectName(TargetMesh, UStaticMeshSocket::StaticClass(), SourceSocket->GetFName());
        if (!Check()) return {};
        UStaticMeshSocket* TargetSocket = Cast<UStaticMeshSocket>(CreateOwnedFactory(TargetMesh, SourceSocket->GetClass(), SocketObjectName, TEXT("StaticDuplicateObject"), [&]() { return StaticDuplicateObject(
            SourceSocket,
            TargetMesh,
            SocketObjectName,
            RF_Transactional); }));
        if (!Check()) return {};
        if (!TargetSocket)
        {
            OutDetail = FString::Printf(TEXT("could not duplicate static mesh socket: %s"), *SourceSocket->SocketName.ToString());
            return false;
        }
#if WITH_EDITORONLY_DATA
        TargetSocket->PreviewStaticMesh = ResolveStaticMeshReplacementForMCP(OptionalReplacementMap, SourceSocket->PreviewStaticMesh);
#endif
        TargetMesh->AddSocket(TargetSocket);
    }
    return true;
}



UObject* CreateFreshStaticMeshAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    TMap<UObject*, UObject*>* OptionalReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

#if WITH_EDITORONLY_DATA
    UStaticMesh* SourceMesh = Cast<UStaticMesh>(SourceObject);
    if (!SourceMesh)
    {
        OutDetail = TEXT("source asset is not UStaticMesh");
        return nullptr;
    }

    FinishCompilationForObjectForMCP(SourceMesh);

    const int32 SourceLODCount = SourceMesh->GetNumSourceModels();
    if (SourceLODCount <= 0)
    {
        OutDetail = TEXT("source static mesh has no source models");
        return nullptr;
    }

    TArray<FMeshDescription> SourceMeshDescriptions;
    int32 RenderExportedLODCount = 0;
    SourceMeshDescriptions.SetNum(SourceLODCount);
    for (int32 LodIndex = 0; LodIndex < SourceLODCount; ++LodIndex)
    {
        bool bUsedRenderExport = false;
        if (!GetStaticMeshDescriptionForFreshCopyForMCP(SourceMesh, LodIndex, SourceMeshDescriptions[LodIndex], bUsedRenderExport, OutDetail))
        {
            return nullptr;
        }
        if (bUsedRenderExport)
        {
            ++RenderExportedLODCount;
        }
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target static mesh package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UStaticMesh* TargetMesh = NewOwnedObject<UStaticMesh>(
        TargetPackage,
        UStaticMesh::StaticClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetMesh)
    {
        OutDetail = TEXT("could not create target static mesh object");
        return nullptr;
    }

    if (!Write(TargetMesh, TEXT("PreEditChange"))) return {};
    TargetMesh->PreEditChange(nullptr);
    if (!After()) return {};

    UEngine::FCopyPropertiesForUnrelatedObjectsParams CopyParams;
    CopyParams.OptionalReplacementMappings = OptionalReplacementMap;
    CopyParams.bReplaceInternalReferenceUponRead = OptionalReplacementMap && OptionalReplacementMap->Num() > 0;
    if (!Check()) return {};
    CopyOwnedProperties(SourceMesh, TargetMesh, CopyParams);
    if (!Check()) return {};

    TArray<FStaticMeshSourceModel> EmptySourceModels;
    TargetMesh->SetSourceModels(MoveTemp(EmptySourceModels));
    TargetMesh->SetNumSourceModels(SourceLODCount);
    for (int32 LodIndex = 0; LodIndex < SourceLODCount; ++LodIndex)
    {
        FMeshDescription TargetMeshDescription = MoveTemp(SourceMeshDescriptions[LodIndex]);
        TargetMesh->CreateMeshDescription(LodIndex, MoveTemp(TargetMeshDescription));

        UStaticMesh::FCommitMeshDescriptionParams CommitParams;
        CommitParams.bMarkPackageDirty = false;
        CommitParams.bUseHashAsGuid = true;
        if (!Write(TargetMesh, TEXT("CommitMeshDescription"))) return {};
        TargetMesh->CommitMeshDescription(LodIndex, CommitParams);
        if (!After()) return {};

        const FStaticMeshSourceModel& SourceModel = SourceMesh->GetSourceModel(LodIndex);
        FStaticMeshSourceModel& TargetModel = TargetMesh->GetSourceModel(LodIndex);
        TargetModel.BuildSettings = SourceModel.BuildSettings;
        TargetModel.BuildSettings.DistanceFieldReplacementMesh = ResolveStaticMeshReplacementForMCP(
            OptionalReplacementMap,
            SourceModel.BuildSettings.DistanceFieldReplacementMesh);
        TargetModel.ReductionSettings = SourceModel.ReductionSettings;
        TargetModel.ScreenSize = SourceModel.ScreenSize;
        TargetModel.SourceImportFilename = SourceModel.SourceImportFilename;
        TargetModel.bImportWithBaseMesh = SourceModel.bImportWithBaseMesh;
    }

    CopyStaticMeshMaterialsForMCP(SourceMesh, TargetMesh, OptionalReplacementMap);
    TargetMesh->GetSectionInfoMap().CopyFrom(SourceMesh->GetSectionInfoMap());
    TargetMesh->GetOriginalSectionInfoMap().CopyFrom(SourceMesh->GetOriginalSectionInfoMap());

    TargetMesh->SetNaniteSettings(SourceMesh->GetNaniteSettings());

    TargetMesh->ComplexCollisionMesh = ResolveStaticMeshReplacementForMCP(OptionalReplacementMap, SourceMesh->ComplexCollisionMesh);

    if (!CopyStaticMeshBodySetupForMCP(SourceMesh, TargetMesh, OutDetail) ||
        !CopyStaticMeshSocketsForMCP(SourceMesh, TargetMesh, OptionalReplacementMap, OutDetail))
    {
        DiscardFreshAssetForFallback(TargetMesh);
        return nullptr;
    }

    if (!Write(TargetMesh, TEXT("SetFlags"))) return {};
    TargetMesh->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetMesh, TEXT("ClearFlags"))) return {};
    TargetMesh->ClearFlags(RF_Transient);
    if (!After()) return {};

    TArray<FText> BuildErrors;
    UStaticMesh::FBuildParameters BuildParameters;
    BuildParameters.bInSilent = true;
    BuildParameters.OutErrors = &BuildErrors;
    BuildParameters.bInRebuildUVChannelData = true;
    if (!Write(TargetMesh, TEXT("Build"))) return {};
    TargetMesh->Build(BuildParameters);
    if (!After()) return {};
    FinishCompilationForObjectForMCP(TargetMesh);
    if (!Write(TargetMesh, TEXT("PostEditChange"))) return {};
    TargetMesh->PostEditChange();
    if (!After()) return {};
    FinishCompilationForObjectForMCP(TargetMesh);

    if (!BuildErrors.IsEmpty())
    {
        TArray<FString> ErrorStrings;
        for (const FText& BuildError : BuildErrors)
        {
            ErrorStrings.Add(BuildError.ToString());
        }
        OutDetail = FString::Printf(TEXT("static mesh build failed: %s"), *FString::Join(ErrorStrings, TEXT("; ")));
        DiscardFreshAssetForFallback(TargetMesh);
        return nullptr;
    }

    FString ValidationDetail;
    if (!ValidateFreshStaticMeshAsset(SourceMesh, TargetMesh, OptionalReplacementMap, ValidationDetail))
    {
        OutDetail = ValidationDetail;
        DiscardFreshAssetForFallback(TargetMesh);
        return nullptr;
    }

    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetMesh, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetMesh);
    if (!After()) return {};

    OutDetail = ValidationDetail;
    if (RenderExportedLODCount > 0)
    {
        OutDetail += FString::Printf(TEXT("_render_exported_lods_%d"), RenderExportedLODCount);
    }
    return TargetMesh;
#else
    OutDetail = TEXT("static mesh recreation requires editor-only mesh description data");
    return nullptr;
#endif
}



bool ValidateFreshDataTableAsset(
    UDataTable* SourceTable,
    UDataTable* TargetTable,
    const UScriptStruct* ExpectedTargetRowStruct,
    FString& OutDetail)
{
    if (!Check()) return {};

    if (!SourceTable || !TargetTable)
    {
        OutDetail = TEXT("source or target data table is null");
        return false;
    }

    if (!ExpectedTargetRowStruct || TargetTable->GetRowStruct() != ExpectedTargetRowStruct)
    {
        OutDetail = TEXT("target data table row struct mismatch");
        return false;
    }

    TArray<FName> SourceRowNames = SourceTable->GetRowNames();
    TArray<FName> TargetRowNames = TargetTable->GetRowNames();
    if (SourceRowNames.Num() != TargetRowNames.Num())
    {
        OutDetail = FString::Printf(
            TEXT("data table row count mismatch: source=%d target=%d"),
            SourceRowNames.Num(),
            TargetRowNames.Num());
        return false;
    }

    SourceRowNames.Sort([](const FName& A, const FName& B)
    {
        return A.ToString() < B.ToString();
    });
    TargetRowNames.Sort([](const FName& A, const FName& B)
    {
        return A.ToString() < B.ToString();
    });

    for (int32 Index = 0; Index < SourceRowNames.Num(); ++Index)
    {
        if (SourceRowNames[Index] != TargetRowNames[Index])
        {
            OutDetail = FString::Printf(
                TEXT("data table row name mismatch at index %d: source=%s target=%s"),
                Index,
                *SourceRowNames[Index].ToString(),
                *TargetRowNames[Index].ToString());
            return false;
        }
    }

#if WITH_EDITOR
    const FString SourceJson = SourceTable->GetTableAsJSON();
    const FString TargetJson = TargetTable->GetTableAsJSON();
    if (SourceJson != TargetJson)
    {
        OutDetail = TEXT("data table JSON export mismatch");
        return false;
    }
#endif

    return true;
}



UObject* CreateFreshDataTableAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const FString& SourceRoot,
    FString& OutDetail)
{
    if (!Check()) return {};

    UDataTable* SourceTable = Cast<UDataTable>(SourceObject);
    if (!SourceTable)
    {
        OutDetail = TEXT("source asset is not UDataTable");
        return nullptr;
    }

    const UScriptStruct* SourceRowStruct = SourceTable->GetRowStruct();
    if (!SourceRowStruct)
    {
        OutDetail = TEXT("source data table has no row struct");
        return nullptr;
    }

    UObject* RowStructObject = SourceTable->RowStruct.Get();
    UObject* ReplacementRowStructObject = FindReplacementObject(ReplacementMap, RowStructObject);
    UScriptStruct* TargetRowStruct = Cast<UScriptStruct>(ReplacementRowStructObject ? ReplacementRowStructObject : RowStructObject);
    if (!TargetRowStruct)
    {
        OutDetail = TEXT("could not resolve target data table row struct");
        return nullptr;
    }

    const FString SourceRowStructPackageName = SourceRowStruct->GetOutermost()
        ? SourceRowStruct->GetOutermost()->GetName()
        : FString();
    const bool bRowStructFromSourceRoot = SourceRowStructPackageName.Equals(SourceRoot)
        || SourceRowStructPackageName.StartsWith(SourceRoot + TEXT("/"));
    if (bRowStructFromSourceRoot && TargetRowStruct == SourceRowStruct)
    {
        OutDetail = FString::Printf(
            TEXT("source data table row struct has no target replacement: %s"),
            *SourceRowStruct->GetPathName());
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target data table package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    UDataTable* TargetTable = NewOwnedObject<UDataTable>(
        TargetPackage,
        UDataTable::StaticClass(),
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional);
    if (!TargetTable)
    {
        OutDetail = TEXT("could not create target data table object");
        return nullptr;
    }

    if (!Write(TargetTable, TEXT("PreEditChange"))) return {};
    TargetTable->PreEditChange(nullptr);
    if (!After()) return {};

#if WITH_EDITOR
    TargetTable->CopyImportOptions(SourceTable);
#endif

    TMap<FName, const uint8*> SourceRows;
    SourceRows.Reserve(SourceTable->GetRowMap().Num());
    for (const TPair<FName, uint8*>& RowPair : SourceTable->GetRowMap())
    {
        if (!RowPair.Value)
        {
            OutDetail = FString::Printf(TEXT("source data table row is null: %s"), *RowPair.Key.ToString());
            DiscardFreshAssetForFallback(TargetTable);
            return nullptr;
        }
        SourceRows.Add(RowPair.Key, RowPair.Value);
    }

    TArray<FString> CreateProblems = TargetTable->CreateTableFromRawData(SourceRows, TargetRowStruct);
    if (!CreateProblems.IsEmpty())
    {
        OutDetail = FString::Printf(TEXT("fresh data table raw import failed: %s"), *FString::Join(CreateProblems, TEXT("; ")));
        DiscardFreshAssetForFallback(TargetTable);
        return nullptr;
    }

#if WITH_EDITORONLY_DATA
    TargetTable->RowStructName_DEPRECATED = TargetRowStruct->GetFName();
    TargetTable->RowStructPathName = TargetRowStruct->GetStructPathName();
#endif

    if (!ValidateFreshDataTableAsset(SourceTable, TargetTable, TargetRowStruct, OutDetail))
    {
        DiscardFreshAssetForFallback(TargetTable);
        return nullptr;
    }

    if (!Write(TargetTable, TEXT("SetFlags"))) return {};
    TargetTable->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetTable, TEXT("ClearFlags"))) return {};
    TargetTable->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetTable, TEXT("PostEditChange"))) return {};
    TargetTable->PostEditChange();
    if (!After()) return {};
    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetTable, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetTable);
    if (!After()) return {};

    OutDetail = FString::Printf(
        TEXT("fresh_data_table_from_%s_rows_%d_row_struct_%s"),
        *SourceTable->GetName(),
        SourceTable->GetRowMap().Num(),
        *TargetRowStruct->GetPathName());
    return TargetTable;
}



void CopyEnumMetaDataValue(const UEnum* SourceEnum, UEnum* TargetEnum, const TCHAR* Key, int32 NameIndex)
{
    if (!Check()) return;

#if WITH_METADATA
    if (SourceEnum && TargetEnum && SourceEnum->HasMetaData(Key, NameIndex))
    {
        if (!Write(TargetEnum, TEXT("SetMetaData"))) return;
        TargetEnum->SetMetaData(Key, *SourceEnum->GetMetaData(Key, NameIndex, false), NameIndex);
        if (!After()) return;
    }
#endif
}



UObject* CreateFreshUserDefinedEnumAsset(UObject* SourceObject, const FString& TargetPath, FString& OutDetail)
{
    if (!Check()) return {};

    UUserDefinedEnum* SourceEnum = Cast<UUserDefinedEnum>(SourceObject);
    if (!SourceEnum)
    {
        OutDetail = TEXT("source asset is not UUserDefinedEnum");
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target enum package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    if (!Check()) return {};
    UUserDefinedEnum* TargetEnum = Cast<UUserDefinedEnum>(CreateOwnedFactory(TargetPackage, UUserDefinedEnum::StaticClass(), TargetObjectName, TEXT("FEnumEditorUtils_CreateUserDefinedEnum"), [&]() { return FEnumEditorUtils::CreateUserDefinedEnum(
        TargetPackage,
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional); }));
    if (!Check()) return {};
    if (!TargetEnum)
    {
        OutDetail = TEXT("could not create target user-defined enum");
        return nullptr;
    }

    TArray<TPair<FName, int64>> TargetEnumNames;
    const int32 SourceEnumeratorCount = FMath::Max(SourceEnum->NumEnums() - 1, 0);
    TargetEnumNames.Reserve(SourceEnumeratorCount);
    for (int32 Index = 0; Index < SourceEnumeratorCount; ++Index)
    {
        const FString ShortName = SourceEnum->GetNameStringByIndex(Index);
        const FString TargetFullName = TargetEnum->GenerateFullEnumName(*ShortName);
        TargetEnumNames.Emplace(FName(*TargetFullName), SourceEnum->GetValueByIndex(Index));
    }

    const EEnumFlags TargetEnumFlags = SourceEnum->HasAnyEnumFlags(EEnumFlags::Flags)
        ? EEnumFlags::Flags
        : EEnumFlags::None;
    TargetEnum->SetEnums(TargetEnumNames, SourceEnum->GetCppForm(), SourceEnum->GetUnderlyingType(), TargetEnumFlags, UEnum::EAddMaxKeyIfMissing::Yes);
    if (!Write(TargetEnum, TEXT("SetMetaData"))) return {};
    TargetEnum->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
    if (!After()) return {};
    CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("Bitflags"));
    CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("UseEnumValuesAsMaskValuesInEditor"));
    CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("ToolTip"));
    CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("ShortTooltip"));

#if WITH_EDITORONLY_DATA
    TargetEnum->EnumDescription = SourceEnum->EnumDescription;
#endif

    TargetEnum->DisplayNameMap.Empty(SourceEnumeratorCount);
    for (int32 Index = 0; Index < SourceEnumeratorCount; ++Index)
    {
        const FName TargetEntryName(*TargetEnum->GetNameStringByIndex(Index));
        TargetEnum->DisplayNameMap.Add(TargetEntryName, SourceEnum->GetDisplayNameTextByIndex(Index));
        CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("DisplayName"), Index);
        CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("ToolTip"), Index);
        CopyEnumMetaDataValue(SourceEnum, TargetEnum, TEXT("Hidden"), Index);
    }
    FEnumEditorUtils::EnsureAllDisplayNamesExist(TargetEnum);

    if (!Write(TargetEnum, TEXT("SetFlags"))) return {};
    TargetEnum->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetEnum, TEXT("ClearFlags"))) return {};
    TargetEnum->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetEnum, TEXT("PostEditChange"))) return {};
    TargetEnum->PostEditChange();
    if (!After()) return {};
    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetEnum, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetEnum);
    if (!After()) return {};

    OutDetail = FString::Printf(TEXT("fresh_user_defined_enum_from_%s_with_%d_enumerator(s)"), *SourceEnum->GetName(), SourceEnumeratorCount);
    return TargetEnum;
}



UObject* RemapObjectForFreshUserDefinedStruct(UObject* SourceObject, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    if (!SourceObject)
    {
        return nullptr;
    }

    if (UObject* ReplacementObject = FindReplacementObject(ReplacementMap, SourceObject))
    {
        return ReplacementObject;
    }
    return SourceObject;
}



bool RemapFreshStructVariableDescription(FStructVariableDescription& Description, const TMap<UObject*, UObject*>& ReplacementMap)
{
    if (!Check()) return {};

    bool bChanged = false;
    if (UObject* SubCategoryObject = Description.SubCategoryObject.LoadSynchronous())
    {
        UObject* ReplacementObject = RemapObjectForFreshUserDefinedStruct(SubCategoryObject, ReplacementMap);
        if (ReplacementObject != SubCategoryObject)
        {
            Description.SubCategoryObject = ReplacementObject;
            bChanged = true;
        }
    }

    if (UObject* TerminalObject = Description.PinValueType.TerminalSubCategoryObject.Get())
    {
        UObject* ReplacementObject = RemapObjectForFreshUserDefinedStruct(TerminalObject, ReplacementMap);
        if (ReplacementObject != TerminalObject)
        {
            Description.PinValueType.TerminalSubCategoryObject = ReplacementObject;
            bChanged = true;
        }
    }

    return bChanged;
}



UObject* CreateFreshUserDefinedStructAsset(
    UObject* SourceObject,
    const FString& TargetPath,
    const TMap<UObject*, UObject*>& ReplacementMap,
    FString& OutDetail)
{
    if (!Check()) return {};

    UUserDefinedStruct* SourceStruct = Cast<UUserDefinedStruct>(SourceObject);
    if (!SourceStruct)
    {
        OutDetail = TEXT("source asset is not UUserDefinedStruct");
        return nullptr;
    }

    if (!Check()) return {};
    UPackage* TargetPackage = CreatePackageForGD(*TargetPath);
    if (!Check()) return {};
    if (!TargetPackage)
    {
        OutDetail = TEXT("could not create target struct package");
        return nullptr;
    }

    const FName TargetObjectName(*FPackageName::GetShortName(TargetPath));
    if (!Check()) return {};
    UUserDefinedStruct* TargetStruct = CreateOwnedFactory(TargetPackage, UUserDefinedStruct::StaticClass(), TargetObjectName, TEXT("FStructureEditorUtils_CreateUserDefinedStruct"), [&]() { return FStructureEditorUtils::CreateUserDefinedStruct(
        TargetPackage,
        TargetObjectName,
        RF_Public | RF_Standalone | RF_Transactional); });
    if (!Check()) return {};
    if (!TargetStruct)
    {
        OutDetail = TEXT("could not create target user-defined struct");
        return nullptr;
    }

    const TArray<FStructVariableDescription>& SourceDescriptions = FStructureEditorUtils::GetVarDesc(SourceStruct);
    TArray<FStructVariableDescription>& TargetDescriptions = FStructureEditorUtils::GetVarDesc(TargetStruct);
    TargetDescriptions = SourceDescriptions;
    for (FStructVariableDescription& Description : TargetDescriptions)
    {
        RemapFreshStructVariableDescription(Description, ReplacementMap);
    }

    const FString SourceTooltip = FStructureEditorUtils::GetTooltip(SourceStruct);
    if (!SourceTooltip.IsEmpty())
    {
        if (!Write(TargetStruct, TEXT("FStructureEditorUtils_ChangeTooltip"))) return {};
        FStructureEditorUtils::ChangeTooltip(TargetStruct, SourceTooltip);
        if (!After()) return {};
    }
    if (!Write(TargetStruct, TEXT("SetMetaData"))) return {};
    TargetStruct->SetMetaData(TEXT("BlueprintType"), TEXT("true"));
    if (!After()) return {};
    TargetStruct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
    if (!Check()) return {};
    CompileOwnedStructure(TargetStruct);
    if (!Check()) return {};
    if (!Write(TargetStruct, TEXT("FStructureEditorUtils_RecreateDefaultInstanceInEditorData"))) return {};
    FStructureEditorUtils::RecreateDefaultInstanceInEditorData(TargetStruct);
    if (!After()) return {};
    if (!Write(TargetStruct, TEXT("UpdateStructFlags"))) return {};
    TargetStruct->UpdateStructFlags();
    if (!After()) return {};

    if (TargetStruct->Status == EUserDefinedStructureStatus::UDSS_Error)
    {
        OutDetail = FString::Printf(TEXT("fresh user-defined struct compile failed: %s"), *TargetStruct->ErrorMessage);
        DiscardFreshAssetForFallback(TargetStruct);
        return nullptr;
    }

    if (!Write(TargetStruct, TEXT("SetFlags"))) return {};
    TargetStruct->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
    if (!After()) return {};
    if (!Write(TargetStruct, TEXT("ClearFlags"))) return {};
    TargetStruct->ClearFlags(RF_Transient);
    if (!After()) return {};
    if (!Write(TargetStruct, TEXT("PostEditChange"))) return {};
    TargetStruct->PostEditChange();
    if (!After()) return {};
    if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
    TargetPackage->MarkPackageDirty();
    if (!After()) return {};
    if (!Write(TargetStruct, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
    FAssetRegistryModule::AssetCreated(TargetStruct);
    if (!After()) return {};

    OutDetail = FString::Printf(TEXT("fresh_user_defined_struct_from_%s_with_%d_member(s)"), *SourceStruct->GetName(), SourceDescriptions.Num());
    return TargetStruct;
}



int32 RemapUserDefinedStructDescriptionPaths(
    const TArray<UUserDefinedStruct*>& Structs,
    const TArray<FPathStringReplacement>& PathReplacements,
    const TMap<UObject*, UObject*>& ReplacementMap,
    const FString& SourceRoot)
{
    if (!Check()) return {};

    if (Structs.IsEmpty() || PathReplacements.IsEmpty())
    {
        return 0;
    }

    TSet<UUserDefinedStruct*> UniqueStructs;
    for (UUserDefinedStruct* Struct : Structs)
    {
        if (IsValid(Struct))
        {
            UniqueStructs.Add(Struct);
        }
    }

    int32 ReplaceCount = 0;
    for (UUserDefinedStruct* Struct : UniqueStructs)
    {
        if (!Struct)
        {
            continue;
        }

#if WITH_EDITORONLY_DATA
        if (!Struct->EditorData)
        {
            continue;
        }
#endif

        bool bStructChanged = false;
        TArray<FStructVariableDescription>& Descriptions = FStructureEditorUtils::GetVarDesc(Struct);
        for (FStructVariableDescription& Description : Descriptions)
        {
            bStructChanged |= RemapFreshStructVariableDescription(Description, ReplacementMap);

            auto RemapStringField = [&PathReplacements, &SourceRoot, &ReplaceCount](FString& Value) -> bool
            {
                if (Value.IsEmpty() || !Value.Contains(SourceRoot, ESearchCase::CaseSensitive))
                {
                    return false;
                }

                const FString ReplacedValue = ApplyPathReplacements(Value, PathReplacements);
                if (ReplacedValue == Value)
                {
                    return false;
                }

                Value = ReplacedValue;
                ++ReplaceCount;
                return true;
            };

            bStructChanged |= RemapStringField(Description.DefaultValue);
            bStructChanged |= RemapStringField(Description.CurrentDefaultValue);
            bStructChanged |= RemapStringField(Description.ToolTip);
            for (TPair<FName, FString>& MetaDataPair : Description.MetaData)
            {
                bStructChanged |= RemapStringField(MetaDataPair.Value);
            }
        }

        if (bStructChanged)
        {
            if (!Write(Struct, TEXT("Modify"))) return {};
            Struct->Modify();
            if (!After()) return {};
            FStructureEditorUtils::ModifyStructData(Struct);
            Struct->Status = EUserDefinedStructureStatus::UDSS_Dirty;
            if (!Check()) return {};
            CompileOwnedStructure(Struct);
            if (!Check()) return {};
            if (!Write(Struct, TEXT("FStructureEditorUtils_RecreateDefaultInstanceInEditorData"))) return {};
            FStructureEditorUtils::RecreateDefaultInstanceInEditorData(Struct);
            if (!After()) return {};
            if (!Write(Struct, TEXT("UpdateStructFlags"))) return {};
            Struct->UpdateStructFlags();
            if (!After()) return {};
            if (!Write(Struct, TEXT("PostEditChange"))) return {};
            Struct->PostEditChange();
            if (!After()) return {};
            if (!Write(Struct, TEXT("MarkPackageDirty"))) return {};
            Struct->MarkPackageDirty();
            if (!After()) return {};
            if (UPackage* Package = Struct->GetOutermost())
            {
                if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
                Package->MarkPackageDirty();
                if (!After()) return {};
            }
        }
    }

    return ReplaceCount;
}



int32 CollectLoadedPackagesUnderRoot(const FString& Root, TArray<UPackage*>& OutPackages, TArray<FString>& OutSamples, int32 MaxSamples)
{
    if (!Check()) return {};

    OutPackages.Reset();
    OutSamples.Reset();

    for (TObjectIterator<UPackage> It; It; ++It)
    {
        UPackage* Package = *It;
        if (!Package)
        {
            continue;
        }

        const FString PackageName = Package->GetName();
        if (PackageName.Equals(Root) || PackageName.StartsWith(Root + TEXT("/")))
        {
            OutPackages.Add(Package);
            if (OutSamples.Num() < MaxSamples)
            {
                OutSamples.Add(PackageName);
            }
        }
    }

    return OutPackages.Num();
}



bool DeleteTargetRootForRecreate(
    const FString& TargetRoot,
    FString& OutDeleteMode,
    TArray<FString>& OutWarnings)
{
    OutDeleteMode = TEXT("public_reference_checked_asset_delete");
    OutWarnings.Reset();
    if (!Check() || !Under(TargetRoot, Current->TargetRoot) || Under(Current->SourceRoot, TargetRoot)) return false;
    TArray<FAssetData> Assets;
    FAssetRegistryModule& Module = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    Module.Get().GetAssetsByPath(FName(*TargetRoot), Assets, true);
    TArray<UObject*> Objects;
    for (const FAssetData& A : Assets)
    {
        if (!Check()) return false;
        TArray<FName> Referencers;
        Module.Get().GetReferencers(A.PackageName, Referencers, UE::AssetRegistry::EDependencyCategory::Package);
        for (const FName& Referencer : Referencers)
            if (!Under(Referencer.ToString(), TargetRoot))
            { OutWarnings.Add(TEXT("Foreign serialized referencer: ") + Referencer.ToString()); return Current->Fail(TEXT("delete_foreign_reference"), OutWarnings.Last()); }
        UObject* O = LoadAssetDataForGD(A);
        if (!O || O->GetPackage()->IsDirty())
        { OutWarnings.Add(TEXT("Target asset is unavailable or dirty: ") + A.GetObjectPathString()); return false; }
        Objects.Add(O);
    }
    for (UObject* O : Objects)
    {
        bool Referenced = false, Undo = false;
        FReferencerInformationList Refs;
        for (auto& T : Current->Targets) if (T.Object.Get() == O) T.Hold.Reset();
        ObjectTools::GatherObjectReferencersForDeletion(O, Referenced, Undo, &Refs);
        if (!Check()) return false;
        if (Undo) { OutWarnings.Add(TEXT("Existing Undo references block deletion: ") + O->GetPathName()); return Current->Fail(TEXT("delete_undo_reference"), OutWarnings.Last()); }
        for (const FReferencerInformation& Ref : Refs.ExternalReferences)
            if (Live(Ref.Referencer) && !Under(Ref.Referencer->GetPackage()->GetName(), TargetRoot))
            { OutWarnings.Add(TEXT("Foreign loaded referencer blocks deletion: ") + Ref.Referencer->GetPathName()); return Current->Fail(TEXT("delete_foreign_reference"), OutWarnings.Last()); }
    }
    if (!Objects.IsEmpty())
    {
        if (!BeforeCleanup(TEXT("delete_target_assets"))) return false;
        for (UObject* O : Objects) Current->Retire(O, TEXT("explicit_delete_target_root"));
        const int32 Deleted = ObjectTools::DeleteObjects(Objects, false, ObjectTools::EAllowCancelDuringDelete::AllowCancel);
        auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("target_root"), TargetRoot); Row->SetNumberField(TEXT("requested_asset_count"), Objects.Num()); Row->SetNumberField(TEXT("sdk_deleted_count"), Deleted);
        Current->R.Data->SetObjectField(TEXT("target_root_delete_result"), Row); Current->R.bChanged |= Deleted > 0;
        if (Deleted == Objects.Num()) for (const FAssetData& A : Assets) Current->ClearedPackageNames.Add(A.PackageName);
        if (!After() || Deleted != Objects.Num()) { OutWarnings.Add(TEXT("Public deletion was incomplete; no force delete or raw recursive filesystem delete was attempted")); return false; }
    }
    TArray<FAssetData> Remaining;
    Module.Get().GetAssetsByPath(FName(*TargetRoot), Remaining, true);
    if (!Remaining.IsEmpty()) { OutWarnings.Add(TEXT("Asset registry still reports target assets; deletion is incomplete")); return false; }
    return RemoveEmptyDirectoryTree(TargetRoot, OutWarnings);
}



TSharedPtr<FJsonObject> AssetReportToJson(
    const FAssetData& SourceAsset,
    const FString& TargetPath,
    const FString& Strategy,
    const FString& Status,
    const FString& Detail)
{
    if (!Check()) return {};

    TSharedPtr<FJsonObject> Object = MakeShared<FJsonObject>();
    Object->SetStringField(TEXT("source_path"), SourceAsset.PackageName.ToString());
    Object->SetStringField(TEXT("source_name"), SourceAsset.AssetName.ToString());
    Object->SetStringField(TEXT("class"), SourceAsset.AssetClassPath.GetAssetName().ToString());
    Object->SetStringField(TEXT("target_path"), TargetPath);
    Object->SetStringField(TEXT("strategy"), Strategy);
    Object->SetStringField(TEXT("status"), Status);
    if (!Detail.IsEmpty())
    {
        Object->SetStringField(TEXT("detail"), Detail);
    }
    return Object;
}



void AddFallbackRuleDetailTokensForMCP(const TSharedPtr<FJsonObject>& RuleObject, TArray<FString>& OutTokens)
{
    if (!Check()) return;

    FString DetailContains;
    if (RuleObject->TryGetStringField(TEXT("detail_contains"), DetailContains) && !DetailContains.IsEmpty())
    {
        OutTokens.Add(DetailContains);
    }

    const TArray<TSharedPtr<FJsonValue>>* DetailContainsAny = nullptr;
    if (RuleObject->TryGetArrayField(TEXT("detail_contains_any"), DetailContainsAny))
    {
        for (const TSharedPtr<FJsonValue>& Value : *DetailContainsAny)
        {
            FString Token;
            if (Value.IsValid() && Value->TryGetString(Token) && !Token.IsEmpty())
            {
                OutTokens.Add(Token);
            }
        }
    }
}



TArray<FAcceptedFallbackRule> ParseAcceptedFallbackRulesForMCP(const TSharedPtr<FJsonObject>& Params)
{
    if (!Check()) return {};

    TArray<FAcceptedFallbackRule> Rules;

    const TArray<TSharedPtr<FJsonValue>>* RuleValues = nullptr;
    if (!Params.IsValid() || !Params->TryGetArrayField(TEXT("accepted_fallback_rules"), RuleValues))
    {
        return Rules;
    }

    for (const TSharedPtr<FJsonValue>& RuleValue : *RuleValues)
    {
        TSharedPtr<FJsonObject> RuleObject = RuleValue.IsValid() ? RuleValue->AsObject() : nullptr;
        if (!RuleObject.IsValid())
        {
            continue;
        }

        FAcceptedFallbackRule Rule;
        RuleObject->TryGetStringField(TEXT("source_path"), Rule.SourcePath);
        RuleObject->TryGetStringField(TEXT("source_path_prefix"), Rule.SourcePathPrefix);
        if (!RuleObject->TryGetStringField(TEXT("class"), Rule.ClassName))
        {
            RuleObject->TryGetStringField(TEXT("asset_class"), Rule.ClassName);
        }
        RuleObject->TryGetStringField(TEXT("reason"), Rule.Reason);
        AddFallbackRuleDetailTokensForMCP(RuleObject, Rule.DetailContainsAny);

        Rule.SourcePath = NormalizeContentRoot(Rule.SourcePath);
        Rule.SourcePathPrefix = NormalizeContentRoot(Rule.SourcePathPrefix);
        Rule.ClassName.TrimStartAndEndInline();
        Rule.Reason.TrimStartAndEndInline();

        if (Rule.Reason.IsEmpty())
        {
            Rule.Reason = TEXT("accepted_by_fallback_rule");
        }
        if (Rule.SourcePath.IsEmpty() && Rule.SourcePathPrefix.IsEmpty())
        {
            continue;
        }

        Rules.Add(MoveTemp(Rule));
    }

    return Rules;
}



bool AcceptedFallbackRuleMatchesForMCP(
    const FAcceptedFallbackRule& Rule,
    const FString& SourcePath,
    const FString& ClassName,
    const FString& FreshCreateDetail)
{
    if (!Check()) return {};

    const bool bSourcePathMatches = !Rule.SourcePath.IsEmpty() && SourcePath == Rule.SourcePath;
    const bool bSourcePathPrefixMatches = !Rule.SourcePathPrefix.IsEmpty()
        && (SourcePath == Rule.SourcePathPrefix || SourcePath.StartsWith(Rule.SourcePathPrefix + TEXT("/")));
    if (!bSourcePathMatches && !bSourcePathPrefixMatches)
    {
        return false;
    }

    if (!Rule.ClassName.IsEmpty() && Rule.ClassName != ClassName)
    {
        return false;
    }

    if (Rule.DetailContainsAny.IsEmpty())
    {
        return true;
    }

    for (const FString& Token : Rule.DetailContainsAny)
    {
        if (!Token.IsEmpty() && FreshCreateDetail.Contains(Token))
        {
            return true;
        }
    }

    return false;
}



FString GetAcceptedFreshCreateFallbackReasonForMCP(
    const FAssetData& SourceAsset,
    const FString& FreshCreateDetail,
    const TArray<FAcceptedFallbackRule>& AcceptedFallbackRules)
{
    if (!Check()) return {};

    const FString SourcePath = SourceAsset.PackageName.ToString();
    const FString ClassName = SourceAsset.AssetClassPath.GetAssetName().ToString();
    for (const FAcceptedFallbackRule& Rule : AcceptedFallbackRules)
    {
        if (AcceptedFallbackRuleMatchesForMCP(Rule, SourcePath, ClassName, FreshCreateDetail))
        {
            return Rule.Reason;
        }
    }

    return FString();
}



void ClassifyFreshCreateFallbackForMCP(
    const FAssetData& SourceAsset,
    const FString& TargetPath,
    const FString& FreshCreateDetail,
    const TArray<FAcceptedFallbackRule>& AcceptedFallbackRules,
    TSharedPtr<FJsonObject> AssetReport,
    int32& AcceptedFallbackCount,
    int32& UnresolvedFallbackCount,
    TArray<FString>& AcceptedFallbackSamples,
    TArray<FString>& UnresolvedFallbackSamples)
{
    if (!Check()) return;

    const FString AcceptedReason = GetAcceptedFreshCreateFallbackReasonForMCP(SourceAsset, FreshCreateDetail, AcceptedFallbackRules);
    const bool bAccepted = !AcceptedReason.IsEmpty();
    AssetReport->SetStringField(TEXT("fallback_resolution"), bAccepted ? TEXT("accepted") : TEXT("unresolved"));
    if (bAccepted)
    {
        AssetReport->SetStringField(TEXT("fallback_acceptance_reason"), AcceptedReason);
        ++AcceptedFallbackCount;
        if (AcceptedFallbackSamples.Num() < 50)
        {
            AcceptedFallbackSamples.Add(FString::Printf(
                TEXT("%s -> %s | %s | %s"),
                *SourceAsset.PackageName.ToString(),
                *TargetPath,
                *AcceptedReason,
                *FreshCreateDetail));
        }
    }
    else
    {
        ++UnresolvedFallbackCount;
        if (UnresolvedFallbackSamples.Num() < 50)
        {
            UnresolvedFallbackSamples.Add(FString::Printf(
                TEXT("%s -> %s | %s"),
                *SourceAsset.PackageName.ToString(),
                *TargetPath,
                FreshCreateDetail.IsEmpty() ? TEXT("fresh create fallback without detail") : *FreshCreateDetail));
        }
    }
}



void AddStringArrayField(TSharedPtr<FJsonObject> Object, const FString& FieldName, const TArray<FString>& Values)
{

    TArray<TSharedPtr<FJsonValue>> JsonValues;
    for (const FString& Value : Values)
    {
        JsonValues.Add(MakeShared<FJsonValueString>(Value));
    }
    Object->SetArrayField(FieldName, JsonValues);
}



FString MakeSafeReportFilenameStemForMCP(FString Stem)
{
    if (!Check()) return {};

    Stem.TrimStartAndEndInline();
    if (Stem.IsEmpty())
    {
        Stem = TEXT("content");
    }

    for (int32 CharacterIndex = 0; CharacterIndex < Stem.Len(); ++CharacterIndex)
    {
        TCHAR& Character = Stem[CharacterIndex];
        if (!FChar::IsAlnum(Character) && Character != TEXT('_') && Character != TEXT('-') && Character != TEXT('.'))
        {
            Character = TEXT('_');
        }
    }

    return Stem;
}



FString BuildMCPReportFilename(const FString& PackagePathOrRoot, const FString& OperationName)
{

    FString Stem = FPackageName::GetShortName(PackagePathOrRoot);
    if (Stem.IsEmpty())
    {
        Stem = FPackageName::GetShortName(NormalizePackagePathParam(PackagePathOrRoot));
    }
    Stem = MakeSafeReportFilenameStemForMCP(Stem);
    return Stem + TEXT("_") + MakeSafeReportFilenameStemForMCP(OperationName) + TEXT("_report.json");
}



FString SaveJsonReportForMCP(const TSharedPtr<FJsonObject>& ReportObject, const FString& ReportFilename)
{
    if (!Check() || !ReportObject) return FString();
    const FString Directory = FPaths::Combine(FPaths::ProjectDir(), TEXT("cubeless"), TEXT("records"), TEXT("verification"), TEXT("content-reconstruction"));
    const FString Filename = FPaths::GetCleanFilename(ReportFilename);
    if (Filename.IsEmpty() || !Filename.Equals(ReportFilename, ESearchCase::CaseSensitive)) { Current->Fail(TEXT("report_path"), TEXT("Report filename escaped the records directory")); return FString(); }
    const FString UniqueFilename = FPaths::GetBaseFilename(Filename) + TEXT("_") + FGuid::NewGuid().ToString(EGuidFormats::Digits) + TEXT(".") + FPaths::GetExtension(Filename);
    const FString ReportPath = FPaths::ConvertRelativePathToFull(FPaths::Combine(Directory, UniqueFilename));
    ReportObject->SetStringField(TEXT("report_path"), ReportPath);
    FString Text; FJsonSerializer::Serialize(ReportObject.ToSharedRef(), TJsonWriterFactory<>::Create(&Text));
    auto Row = MakeShared<FJsonObject>(); Row->SetStringField(TEXT("path"), ReportPath); Row->SetBoolField(TEXT("attempted"), true);
    Current->R.Data->SetBoolField(TEXT("report_write_attempted"), true);
    IFileManager::Get().MakeDirectory(*Directory, true);
    const bool Returned = FFileHelper::SaveStringToFile(Text, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8, &IFileManager::Get(), FILEWRITE_NoReplaceExisting);
    Row->SetBoolField(TEXT("sdk_returned_true"), Returned); Current->Reports.Add(MakeShared<FJsonValueObject>(Row));
    Current->R.Data->SetBoolField(TEXT("report_write_returned"), Returned); Current->R.Data->SetStringField(TEXT("report_path"), ReportPath);
    if (!Returned) Current->Fail(TEXT("report_write_failed"), TEXT("Report JSON could not be written; prior asset changes and save history are retained"));
    return Returned ? ReportPath : FString();
}



TSharedPtr<FJsonObject> CloneJsonObjectForMCP(const TSharedPtr<FJsonObject>& Source)
{

    TSharedPtr<FJsonObject> Clone = MakeShared<FJsonObject>();
    if (!Source.IsValid())
    {
        return Clone;
    }

    for (const auto& Pair : Source->Values)
    {
        Clone->SetField(FString(Pair.Key), Pair.Value);
    }
    return Clone;
}



void SetBoolFieldIfMissingForMCP(TSharedPtr<FJsonObject> Object, const FString& FieldName, bool bValue)
{

    if (Object.IsValid() && !Object->HasField(FieldName))
    {
        Object->SetBoolField(FieldName, bValue);
    }
}



void SetNumberFieldIfMissingForMCP(TSharedPtr<FJsonObject> Object, const FString& FieldName, double Value)
{

    if (Object.IsValid() && !Object->HasField(FieldName))
    {
        Object->SetNumberField(FieldName, Value);
    }
}



bool GetJsonBoolFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, bool bDefaultValue)
{

    bool bValue = bDefaultValue;
    if (Object.IsValid() && Object->TryGetBoolField(FieldName, bValue))
    {
        return bValue;
    }
    return bDefaultValue;
}



int32 GetJsonIntFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName, int32 DefaultValue)
{

    double Value = DefaultValue;
    if (Object.IsValid() && Object->TryGetNumberField(FieldName, Value))
    {
        return FMath::RoundToInt(Value);
    }
    return DefaultValue;
}



FString GetJsonStringFieldForMCP(const TSharedPtr<FJsonObject>& Object, const FString& FieldName)
{
    if (!Check()) return {};

    FString Value;
    if (Object.IsValid())
    {
        Object->TryGetStringField(FieldName, Value);
    }
    return Value;
}



bool PipelineStepResultPassesForMCP(const TSharedPtr<FJsonObject>& Result)
{

    return GetJsonBoolFieldForMCP(Result, TEXT("success"), false)
        && GetJsonBoolFieldForMCP(Result, TEXT("verification_pass"), false);
}



TSharedPtr<FJsonObject> MakePipelineStepSummaryForMCP(
    const FString& StepName,
    bool bRan,
    const TSharedPtr<FJsonObject>& Result,
    const FString& SkippedReason)
{

    TSharedPtr<FJsonObject> StepObject = MakeShared<FJsonObject>();
    StepObject->SetStringField(TEXT("step"), StepName);
    StepObject->SetBoolField(TEXT("ran"), bRan);

    if (!bRan)
    {
        StepObject->SetBoolField(TEXT("success"), true);
        StepObject->SetBoolField(TEXT("verification_pass"), true);
        StepObject->SetStringField(TEXT("skipped_reason"), SkippedReason);
        return StepObject;
    }

    StepObject->SetBoolField(TEXT("success"), GetJsonBoolFieldForMCP(Result, TEXT("success"), false));
    StepObject->SetBoolField(TEXT("verification_pass"), GetJsonBoolFieldForMCP(Result, TEXT("verification_pass"), false));

    const FString ReportPath = GetJsonStringFieldForMCP(Result, TEXT("report_path"));
    if (!ReportPath.IsEmpty())
    {
        StepObject->SetStringField(TEXT("report_path"), ReportPath);
    }

    const FString VerificationError = GetJsonStringFieldForMCP(Result, TEXT("verification_error"));
    if (!VerificationError.IsEmpty())
    {
        StepObject->SetStringField(TEXT("verification_error"), VerificationError);
    }

    const FString Error = GetJsonStringFieldForMCP(Result, TEXT("error"));
    if (!Error.IsEmpty())
    {
        StepObject->SetStringField(TEXT("error"), Error);
    }

    return StepObject;
}



void CopyNumberFieldIfPresentForMCP(
    const TSharedPtr<FJsonObject>& Source,
    TSharedPtr<FJsonObject> Target,
    const FString& SourceFieldName,
    const FString& TargetFieldName)
{

    double Value = 0.0;
    if (Source.IsValid() && Target.IsValid() && Source->TryGetNumberField(SourceFieldName, Value))
    {
        Target->SetNumberField(TargetFieldName, Value);
    }
}



FString GetEditorLogPath()
{
    if (!Check()) return {};

    FString ProjectName = FApp::GetProjectName();
    if (ProjectName.IsEmpty() && !FPaths::GetProjectFilePath().IsEmpty())
    {
        ProjectName = FPaths::GetBaseFilename(FPaths::GetProjectFilePath());
    }
    if (!ProjectName.IsEmpty())
    {
        return FPaths::Combine(FPaths::ProjectLogDir(), ProjectName + TEXT(".log"));
    }
    return FString();
}



int32 CountLogLines(const FString& Text)
{
    if (!Check()) return {};

    if (Text.IsEmpty())
    {
        return 0;
    }

    int32 Count = 0;
    for (int32 Index = 0; Index < Text.Len(); ++Index)
    {
        if (Text[Index] == TEXT('\n'))
        {
            ++Count;
        }
    }
    if (!Text.EndsWith(TEXT("\n")))
    {
        ++Count;
    }
    return Count;
}



FEditorLogMarker CaptureEditorLogMarker(bool bEnabled)
{
    if (!Check()) return {};

    FEditorLogMarker Marker;
    Marker.bEnabled = bEnabled;
    Marker.LogPath = GetEditorLogPath();
    if (!bEnabled || Marker.LogPath.IsEmpty())
    {
        return Marker;
    }

    Marker.StartFileSize = IFileManager::Get().FileSize(*Marker.LogPath);
    FString LogText;
    if (FFileHelper::LoadFileToString(LogText, *Marker.LogPath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
    {
        Marker.StartLineCount = CountLogLines(LogText);
    }
    return Marker;
}



void AddEditorLogIssueSample(FEditorLogHealthResult& Result, const TArray<FString>& Lines, int32 LineIndex, const FString& Type)
{
    if (!Check()) return;

    if (Result.IssueSamples.Num() >= 20 || !Lines.IsValidIndex(LineIndex))
    {
        return;
    }

    FString Sample = Type + TEXT(": ");
    const int32 LastLine = FMath::Min(LineIndex + 3, Lines.Num() - 1);
    for (int32 Index = LineIndex; Index <= LastLine; ++Index)
    {
        FString Line = Lines[Index];
        Line.TrimStartAndEndInline();
        if (!Line.IsEmpty())
        {
            if (Index > LineIndex)
            {
                Sample += TEXT(" | ");
            }
            Sample += Line;
        }
    }

    if (Sample.Len() > 1200)
    {
        Sample = Sample.Left(1200) + TEXT("...");
    }
    Result.IssueSamples.Add(Sample);
}



bool IsBenignBlueprintUberGraphFrameEnsure(const TArray<FString>& Lines, int32 LineIndex, const FString& TargetRoot)
{
    if (!Check()) return {};

    if (TargetRoot.IsEmpty())
    {
        return false;
    }

    bool bHasUberGraphFrameKeyMismatch = false;
    bool bHasKeyMismatchDiagnostic = false;
    bool bHasTargetPath = false;
    const int32 LastLine = FMath::Min(LineIndex + 8, Lines.Num() - 1);
    for (int32 Index = LineIndex; Index <= LastLine; ++Index)
    {
        bHasUberGraphFrameKeyMismatch |= Lines[Index].Contains(TEXT("PointerToUberGraphFrame->UberGraphFunctionKey"), ESearchCase::CaseSensitive);
        bHasKeyMismatchDiagnostic |= Lines[Index].Contains(TEXT("Detected key mismatch in uber graph frame for instance"), ESearchCase::CaseSensitive);
        bHasTargetPath |= Lines[Index].Contains(TargetRoot, ESearchCase::CaseSensitive);
        if (bHasUberGraphFrameKeyMismatch && bHasKeyMismatchDiagnostic && bHasTargetPath)
        {
            return true;
        }
    }
    return false;
}



FEditorLogHealthResult ScanEditorLogSince(const FEditorLogMarker& Marker, const FString& BenignBlueprintEnsureRoot)
{
    if (!Check()) return {};

    FEditorLogHealthResult Result;
    Result.LogPath = Marker.LogPath;
    Result.bChecked = Marker.bEnabled && !Marker.LogPath.IsEmpty();
    if (!Result.bChecked)
    {
        return Result;
    }

    FString LogText;
    if (!FFileHelper::LoadFileToString(LogText, *Marker.LogPath, FFileHelper::EHashOptions::None, FILEREAD_AllowWrite))
    {
        Result.bChecked = false;
        return Result;
    }

    TArray<FString> Lines;
    LogText.ParseIntoArrayLines(Lines, false);
    const int32 StartIndex = FMath::Clamp(Marker.StartLineCount, 0, Lines.Num());
    for (int32 Index = StartIndex; Index < Lines.Num(); ++Index)
    {
        const FString& Line = Lines[Index];
        if (Line.Contains(TEXT("=== Handled ensure: ==="), ESearchCase::CaseSensitive))
        {
            if (IsBenignBlueprintUberGraphFrameEnsure(Lines, Index, BenignBlueprintEnsureRoot))
            {
                ++Result.IgnoredHandledEnsureCount;
                continue;
            }
            ++Result.HandledEnsureCount;
            AddEditorLogIssueSample(Result, Lines, Index, TEXT("handled_ensure"));
        }
        if (Line.Contains(TEXT("Fatal error:"), ESearchCase::CaseSensitive))
        {
            ++Result.FatalErrorCount;
            AddEditorLogIssueSample(Result, Lines, Index, TEXT("fatal_error"));
        }
        if (Line.Contains(TEXT("=== Critical error: ==="), ESearchCase::CaseSensitive))
        {
            ++Result.CriticalErrorCount;
            AddEditorLogIssueSample(Result, Lines, Index, TEXT("critical_error"));
        }
        if (Line.Contains(TEXT("ForceDeleteObjects"), ESearchCase::CaseSensitive) ||
            Line.Contains(TEXT("ObjectTools.cpp"), ESearchCase::CaseSensitive))
        {
            ++Result.ForceDeleteIssueCount;
            AddEditorLogIssueSample(Result, Lines, Index, TEXT("force_delete_issue"));
        }
        if (Line.Contains(TEXT("World Memory Leaks"), ESearchCase::CaseSensitive))
        {
            ++Result.WorldMemoryLeakCount;
            AddEditorLogIssueSample(Result, Lines, Index, TEXT("world_memory_leak"));
        }
    }

    Result.IssueCount =
        Result.HandledEnsureCount +
        Result.FatalErrorCount +
        Result.CriticalErrorCount +
        Result.ForceDeleteIssueCount +
        Result.WorldMemoryLeakCount;
    Result.bPass = Result.IssueCount == 0;
    return Result;
}



void AddEditorLogHealthFields(TSharedPtr<FJsonObject> Object, const FEditorLogHealthResult& Result)
{
    if (!Check()) return;

    Object->SetBoolField(TEXT("editor_log_health_checked"), Result.bChecked);
    Object->SetBoolField(TEXT("editor_log_health_pass"), Result.bPass);
    Object->SetStringField(TEXT("editor_log_path"), Result.LogPath);
    Object->SetNumberField(TEXT("editor_log_issue_count"), Result.IssueCount);
    Object->SetNumberField(TEXT("editor_log_handled_ensure_count"), Result.HandledEnsureCount);
    Object->SetNumberField(TEXT("editor_log_ignored_handled_ensure_count"), Result.IgnoredHandledEnsureCount);
    Object->SetNumberField(TEXT("editor_log_fatal_error_count"), Result.FatalErrorCount);
    Object->SetNumberField(TEXT("editor_log_critical_error_count"), Result.CriticalErrorCount);
    Object->SetNumberField(TEXT("editor_log_force_delete_issue_count"), Result.ForceDeleteIssueCount);
    Object->SetNumberField(TEXT("editor_log_world_memory_leak_count"), Result.WorldMemoryLeakCount);
    AddStringArrayField(Object, TEXT("editor_log_issue_samples"), Result.IssueSamples);
}



TSharedPtr<FJsonObject> HandleRecreateContentFolderMCP(const TSharedPtr<FJsonObject>& Params)
{
    if (!Check()) return {};

    FString SourceRoot;
    FString TargetRoot;
    FString RootParamError;
    if (!TryGetRequiredContentRootParamForMCP(Params, TEXT("source_root"), SourceRoot, RootParamError) ||
        !TryGetRequiredContentRootParamForMCP(Params, TEXT("target_root"), TargetRoot, RootParamError) ||
        !ValidateContentRootPairForMCP(SourceRoot, TargetRoot, RootParamError))
    {
        return LegacyError(RootParamError);
    }
    const FString Suffix = GetStringParam(Params, TEXT("suffix"), TEXT("_MCP"));
    const TArray<FAcceptedFallbackRule> AcceptedFallbackRules = ParseAcceptedFallbackRulesForMCP(Params);
    const bool bAllowFallbackDuplicate = GetBoolParam(Params, TEXT("allow_fallback_duplicate"), true);
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false);
    const bool bOverwriteExisting = GetBoolParam(Params, TEXT("overwrite_existing"), false);
    const bool bDeleteTargetRootFirst = GetBoolParam(Params, TEXT("delete_target_root_first"), false);
    const bool bRemapReferences = GetBoolParam(Params, TEXT("remap_references"), true);
    const bool bCompileBlueprints = GetBoolParam(Params, TEXT("compile_blueprints"), true);
    const bool bLogArchiveRemapObjects = GetBoolParam(Params, TEXT("log_archive_remap_objects"), false);
    const bool bRefreshBlueprintNodes = GetBoolParam(Params, TEXT("refresh_blueprint_nodes"), false);
    const bool bRefreshFailedBlueprintNodes = GetBoolParam(Params, TEXT("refresh_failed_blueprint_nodes"), true);
    const bool bSaveAssets = GetBoolParam(Params, TEXT("save_assets"), true);
    const bool bRepairLevelActors = GetBoolParam(Params, TEXT("repair_level_actors"), true);
    const bool bForceRepairLevelActors = GetBoolParam(Params, TEXT("force_repair_level_actors"), true);
    const int32 CompilePasses = FMath::Max(1, GetIntParam(Params, TEXT("compile_passes"), 2));
    const bool bCheckEditorLogHealth = GetBoolParam(Params, TEXT("check_editor_log_health"), true);
    const bool bFailOnEditorLogIssues = GetBoolParam(Params, TEXT("fail_on_editor_log_issues"), true);
    const bool bSuppressEditorPrompts = GetBoolParam(Params, TEXT("suppress_editor_prompts"), true);
    TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || bSuppressEditorPrompts);
    const FEditorLogMarker EditorLogMarker = CaptureEditorLogMarker(bCheckEditorLogHealth);

    if (!FPackageName::IsValidLongPackageName(SourceRoot) || !SourceRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid source_root: %s"), *SourceRoot));
    }

    if (!FPackageName::IsValidLongPackageName(TargetRoot) || !TargetRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid target_root: %s"), *TargetRoot));
    }

    if (TargetRoot.Equals(SourceRoot) || TargetRoot.StartsWith(SourceRoot + TEXT("/")))
    {
        return LegacyError(TEXT("target_root must not be the source root or a child of source_root"));
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FAssetData> SourceAssets;
    AssetRegistry.GetAssetsByPath(FName(*SourceRoot), SourceAssets, true);
    SourceAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        const int32 APriority = GetRecreateCreationPriority(A);
        const int32 BPriority = GetRecreateCreationPriority(B);
        if (APriority != BPriority)
        {
            return APriority < BPriority;
        }
        return A.PackageName.LexicalLess(B.PackageName);
    });

    if (SourceAssets.IsEmpty())
    {
        return LegacyError(FString::Printf(TEXT("No source assets found under %s"), *SourceRoot));
    }

    TSharedPtr<FJsonObject> ReportObject = MakeShared<FJsonObject>();
    ReportObject->SetStringField(TEXT("source_root"), SourceRoot);
    ReportObject->SetStringField(TEXT("target_root"), TargetRoot);
    ReportObject->SetStringField(TEXT("suffix"), Suffix);
    ReportObject->SetBoolField(TEXT("dry_run"), bDryRun);

    TArray<TSharedPtr<FJsonValue>> AssetReports;
    TMap<FString, int32> ClassCounts;
    TMap<UObject*, UObject*> ReplacementMap;
    TArray<FPathStringReplacement> PathReplacements;
    TArray<TPair<UObject*, UObject*>> SourceTargetPairs;
    TArray<UObject*> TargetObjects;
    TArray<UBlueprint*> TargetBlueprints;
    TArray<UUserDefinedStruct*> TargetUserDefinedStructs;
    TArray<FName> TargetPackageNames;
    TArray<FString> TargetWorldPackageNames;
    TSet<UPackage*> TargetPackages;
    TArray<FDeferredGeneratedClassAsset> DeferredGeneratedClassAssets;
    TArray<FString> FailedAssets;
    TArray<FString> SkippedExistingAssets;
    TArray<FString> DeleteTargetRootWarnings;
    FString DeleteTargetRootMode = TEXT("none");

    int32 FallbackDuplicateCount = 0;
    int32 CreatedCount = 0;
    int32 SavedCount = 0;
    int32 RemappedObjectCount = 0;
    int32 ArchiveObjectRemapSkipCount = 0;
    int32 PathPropertyRemapCount = 0;
    int32 GeneratedClassAssetRecreateCount = 0;
    int32 FreshPropertyCopyAssetCount = 0;
    int32 FreshGeneratedClassAssetCount = 0;
    int32 FreshMaterialAssetCount = 0;
    int32 FreshMaterialFunctionAssetCount = 0;
    int32 FreshMaterialExpressionMappingCount = 0;
    int32 FreshTexture2DAssetCount = 0;
    int32 FreshTextureCubeAssetCount = 0;
    int32 FreshVolumeTextureAssetCount = 0;
    int32 FreshStaticMeshAssetCount = 0;
    int32 FreshSoundWaveAssetCount = 0;
    int32 FreshDataTableAssetCount = 0;
    int32 FreshUserDefinedEnumAssetCount = 0;
    int32 FreshUserDefinedStructAssetCount = 0;
    int32 FreshCreateFallbackCount = 0;
    int32 AcceptedFallbackCount = 0;
    int32 UnresolvedFallbackCount = 0;
    int32 UserDefinedStructDescriptionPathRemapCount = 0;
    int32 UserDefinedStructCompileErrorCount = 0;
    int32 SCSNodeRepairCount = 0;
    int32 WidgetTreeRepairCount = 0;
    int32 GeneratedClassSubobjectRepairCount = 0;
    int32 MaterialTextureReferenceRepairCount = 0;
    int32 MaterialCacheRefreshCount = 0;
    int32 LevelActorReferenceRemapCount = 0;
    int32 LevelActorClassRepairCount = 0;
    int32 LevelComponentClassRepairCount = 0;
    int32 SavedMapCount = 0;
    int32 SkippedCleanWorldRepairCount = 0;
    int32 BlueprintGraphReferenceRemapCount = 0;
    int32 CompiledBlueprintCount = 0;
    int32 BlueprintCompileErrorCount = 0;
    TArray<FString> AcceptedFallbackSamples;
    TArray<FString> UnresolvedFallbackSamples;

    if (!bDryRun)
    {
        if (bDeleteTargetRootFirst && UEditorAssetLibrary::DoesDirectoryExist(TargetRoot))
        {
            if (!DeleteTargetRootForRecreate(TargetRoot, DeleteTargetRootMode, DeleteTargetRootWarnings))
            {
                TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
                ResultObj->SetBoolField(TEXT("success"), false);
                ResultObj->SetStringField(TEXT("source_root"), SourceRoot);
                ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
                ResultObj->SetStringField(TEXT("delete_target_root_mode"), DeleteTargetRootMode);
                AddStringArrayField(ResultObj, TEXT("delete_target_root_warnings"), DeleteTargetRootWarnings);
                ResultObj->SetStringField(TEXT("message"), TEXT("Failed to prepare target_root for deletion. Restart the editor or unload target packages, then rerun."));
                return ResultObj;
            }
            TArray<FString> RescanPaths;
            RescanPaths.Add(TargetRoot);
            AssetRegistry.ScanPathsSynchronous(RescanPaths, true);
        }
        UEditorAssetLibrary::MakeDirectory(TargetRoot);
    }

    for (const FAssetData& SourceAsset : SourceAssets)
    {
        const FString ClassName = SourceAsset.AssetClassPath.GetAssetName().ToString();
        ClassCounts.FindOrAdd(ClassName)++;

        const FString TargetPath = BuildTargetAssetPath(SourceAsset, SourceRoot, TargetRoot, Suffix);
        const FString TargetDirectory = FPackageName::GetLongPackagePath(TargetPath);
        const bool bFreshPropertyCopyCandidate = IsFreshPropertyCopyAssetClassName(ClassName);
        const bool bFreshTexture2DCandidate = ClassName == TEXT("Texture2D");
        const bool bFreshTextureCubeCandidate = ClassName == TEXT("TextureCube");
        const bool bFreshVolumeTextureCandidate = ClassName == TEXT("VolumeTexture");
        const bool bFreshStaticMeshCandidate = ClassName == TEXT("StaticMesh");
        const bool bFreshSoundWaveCandidate = ClassName == TEXT("SoundWave");
        const bool bFreshMaterialCandidate = ClassName == TEXT("Material");
        const bool bFreshMaterialFunctionCandidate = ClassName == TEXT("MaterialFunction");
        const bool bFreshMaterialGraphCandidate = IsFreshMaterialGraphAssetClassName(ClassName);
        const bool bFreshDataTableCandidate = ClassName == TEXT("DataTable");
        const bool bFreshUserDefinedEnumCandidate = ClassName == TEXT("UserDefinedEnum");
        const bool bFreshUserDefinedStructCandidate = ClassName == TEXT("UserDefinedStruct");
        const bool bFreshCreateCandidate = bFreshPropertyCopyCandidate || bFreshTexture2DCandidate || bFreshTextureCubeCandidate || bFreshVolumeTextureCandidate || bFreshStaticMeshCandidate || bFreshSoundWaveCandidate || bFreshMaterialGraphCandidate || bFreshDataTableCandidate || bFreshUserDefinedEnumCandidate || bFreshUserDefinedStructCandidate;
        FString PlannedStrategy = TEXT("fallback_duplicate");
        FString PlannedDetail = GetFallbackReason(SourceAsset);
        if (bFreshTexture2DCandidate)
        {
            PlannedStrategy = TEXT("fresh_texture2d_source_copy");
            PlannedDetail = TEXT("fresh_texture2d_source_copy");
        }
        else if (bFreshTextureCubeCandidate)
        {
            PlannedStrategy = TEXT("fresh_texturecube_source_copy");
            PlannedDetail = TEXT("fresh_texturecube_source_copy");
        }
        else if (bFreshVolumeTextureCandidate)
        {
            PlannedStrategy = TEXT("fresh_volumetexture_source_copy");
            PlannedDetail = TEXT("fresh_volumetexture_source_copy");
        }
        else if (bFreshStaticMeshCandidate)
        {
            PlannedStrategy = TEXT("fresh_static_mesh_meshdescription_copy");
            PlannedDetail = TEXT("fresh_static_mesh_meshdescription_copy");
        }
        else if (bFreshSoundWaveCandidate)
        {
            PlannedStrategy = TEXT("fresh_sound_wave_payload_copy");
            PlannedDetail = TEXT("fresh_sound_wave_payload_copy");
        }
        else if (bFreshMaterialCandidate)
        {
            PlannedStrategy = TEXT("fresh_material_graph_copy");
            PlannedDetail = TEXT("fresh_material_graph_copy");
        }
        else if (bFreshMaterialFunctionCandidate)
        {
            PlannedStrategy = TEXT("fresh_material_function_graph_copy");
            PlannedDetail = TEXT("fresh_material_graph_copy");
        }
        else if (bFreshUserDefinedEnumCandidate)
        {
            PlannedStrategy = TEXT("fresh_user_defined_enum");
            PlannedDetail = TEXT("fresh_user_defined_enum_copy");
        }
        else if (bFreshDataTableCandidate)
        {
            PlannedStrategy = TEXT("fresh_data_table");
            PlannedDetail = TEXT("fresh_data_table_row_copy");
        }
        else if (bFreshUserDefinedStructCandidate)
        {
            PlannedStrategy = TEXT("fresh_user_defined_struct");
            PlannedDetail = TEXT("fresh_user_defined_struct_copy");
        }
        else if (bFreshPropertyCopyCandidate)
        {
            PlannedStrategy = TEXT("fresh_property_copy");
            PlannedDetail = GetFreshPropertyCopyReason(SourceAsset);
        }

        if (bDryRun)
        {
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, PlannedStrategy, TEXT("planned"), PlannedDetail)));
            if (bFreshTexture2DCandidate)
            {
                ++FreshTexture2DAssetCount;
            }
            else if (bFreshTextureCubeCandidate)
            {
                ++FreshTextureCubeAssetCount;
            }
            else if (bFreshVolumeTextureCandidate)
            {
                ++FreshVolumeTextureAssetCount;
            }
            else if (bFreshStaticMeshCandidate)
            {
                ++FreshStaticMeshAssetCount;
            }
            else if (bFreshSoundWaveCandidate)
            {
                ++FreshSoundWaveAssetCount;
            }
            else if (bFreshMaterialCandidate)
            {
                ++FreshMaterialAssetCount;
            }
            else if (bFreshMaterialFunctionCandidate)
            {
                ++FreshMaterialFunctionAssetCount;
            }
            else if (bFreshUserDefinedEnumCandidate)
            {
                ++FreshUserDefinedEnumAssetCount;
            }
            else if (bFreshDataTableCandidate)
            {
                ++FreshDataTableAssetCount;
            }
            else if (bFreshUserDefinedStructCandidate)
            {
                ++FreshUserDefinedStructAssetCount;
            }
            else if (bFreshPropertyCopyCandidate)
            {
                ++FreshPropertyCopyAssetCount;
            }
            else
            {
                ++FallbackDuplicateCount;
            }
            continue;
        }

        if (UEditorAssetLibrary::DoesAssetExist(TargetPath))
        {
            if (!bOverwriteExisting)
            {
                SkippedExistingAssets.Add(TargetPath);
                if (!Check()) return {};
                UObject* ExistingTarget = LoadAssetForGD(TargetPath);
                if (!Check()) return {};
                if (!Check()) return {};
                UObject* SourceObject = LoadAssetDataForGD(SourceAsset);
                if (!Check()) return {};
                if (SourceObject && ExistingTarget)
                {
                    AddReplacementMapping(ReplacementMap, SourceObject, ExistingTarget);
                    SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, ExistingTarget));
                    TargetObjects.Add(ExistingTarget);
                    TargetPackageNames.Add(ExistingTarget->GetOutermost()->GetFName());
                    AddTargetWorldPackageName(TargetWorldPackageNames, ExistingTarget);
                    TargetPackages.Add(ExistingTarget->GetOutermost());
                    if (UBlueprint* ExistingBlueprint = Cast<UBlueprint>(ExistingTarget))
                    {
                        TargetBlueprints.Add(ExistingBlueprint);
                    }
                    if (UUserDefinedStruct* ExistingStruct = Cast<UUserDefinedStruct>(ExistingTarget))
                    {
                        TargetUserDefinedStructs.Add(ExistingStruct);
                    }
                }
                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, PlannedStrategy, TEXT("skipped_existing"), TEXT("target exists"))));
                continue;
            }

            if (!DeleteExactAssetForGD(TargetPath))
            {
                FailedAssets.Add(TargetPath);
                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, PlannedStrategy, TEXT("failed"), TEXT("could not delete existing target"))));
                continue;
            }
        }

        UEditorAssetLibrary::MakeDirectory(TargetDirectory);
        if (!Check()) return {};
        UObject* SourceObject = LoadAssetDataForGD(SourceAsset);
        if (!Check()) return {};
        if (!SourceObject)
        {
            FailedAssets.Add(SourceAsset.PackageName.ToString());
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, PlannedStrategy, TEXT("failed"), TEXT("could not load source asset"))));
            continue;
        }

        if (IsGeneratedClassInstanceFromRoot(SourceObject, SourceRoot))
        {
            DeferredGeneratedClassAssets.Add({
                SourceAsset,
                TargetPath,
                TEXT("fresh_generated_class_property_copy"),
                TEXT("fresh_target_generated_class_property_copy")});
            continue;
        }

        FString FreshCreateDetail;
        if (bFreshTexture2DCandidate)
        {
            if (UObject* TargetObject = CreateFreshTexture2DAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshTexture2DAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_texture2d_source_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshTextureCubeCandidate)
        {
            if (UObject* TargetObject = CreateFreshTextureCubeAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshTextureCubeAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_texturecube_source_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshVolumeTextureCandidate)
        {
            if (UObject* TargetObject = CreateFreshVolumeTextureAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshVolumeTextureAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_volumetexture_source_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshStaticMeshCandidate)
        {
            if (UObject* TargetObject = CreateFreshStaticMeshAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshStaticMeshAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_static_mesh_meshdescription_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshSoundWaveCandidate)
        {
            if (UObject* TargetObject = CreateFreshSoundWaveAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshSoundWaveAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_sound_wave_payload_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshMaterialGraphCandidate)
        {
            if (UObject* TargetObject = CreateFreshMaterialGraphAsset(SourceObject, TargetPath, &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                FreshMaterialExpressionMappingCount += AddMaterialGraphExpressionReplacementMappings(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                if (bFreshMaterialCandidate)
                {
                    ++FreshMaterialAssetCount;
                }
                else
                {
                    ++FreshMaterialFunctionAssetCount;
                }

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    bFreshMaterialCandidate ? TEXT("fresh_material_graph_copy") : TEXT("fresh_material_function_graph_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshUserDefinedEnumCandidate)
        {
            if (UObject* TargetObject = CreateFreshUserDefinedEnumAsset(SourceObject, TargetPath, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshUserDefinedEnumAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_user_defined_enum"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshDataTableCandidate)
        {
            if (UObject* TargetObject = CreateFreshDataTableAsset(SourceObject, TargetPath, ReplacementMap, SourceRoot, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshDataTableAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_data_table"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshUserDefinedStructCandidate)
        {
            if (UObject* TargetObject = CreateFreshUserDefinedStructAsset(SourceObject, TargetPath, ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                if (UUserDefinedStruct* TargetStruct = Cast<UUserDefinedStruct>(TargetObject))
                {
                    TargetUserDefinedStructs.Add(TargetStruct);
                }
                ++CreatedCount;
                ++FreshUserDefinedStructAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_user_defined_struct"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }
        else if (bFreshPropertyCopyCandidate)
        {
            if (UObject* TargetObject = CreateFreshPropertyCopyAsset(SourceObject, TargetPath, SourceObject->GetClass(), &ReplacementMap, FreshCreateDetail))
            {
                AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
                SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
                TargetObjects.Add(TargetObject);
                TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
                AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
                TargetPackages.Add(TargetObject->GetOutermost());
                ++CreatedCount;
                ++FreshPropertyCopyAssetCount;

                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    SourceAsset,
                    TargetPath,
                    TEXT("fresh_property_copy"),
                    TEXT("created"),
                    FreshCreateDetail)));
                continue;
            }
        }

        if (!bAllowFallbackDuplicate)
        {
            FailedAssets.Add(SourceAsset.PackageName.ToString());
            const FString FailureDetail = FreshCreateDetail.IsEmpty()
                ? TEXT("fallback duplicate disabled")
                : FString::Printf(TEXT("fresh create failed: %s; fallback duplicate disabled"), *FreshCreateDetail);
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, PlannedStrategy, TEXT("failed"), FailureDetail)));
            continue;
        }

        if (!Check()) return {};
        UObject* TargetObject = DuplicateAssetForGD(SourceAsset.PackageName.ToString(), TargetPath);
        if (!Check()) return {};
        if (!TargetObject)
        {
            FailedAssets.Add(SourceAsset.PackageName.ToString());
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(SourceAsset, TargetPath, TEXT("fallback_duplicate"), TEXT("failed"), TEXT("duplicate fallback failed"))));
            continue;
        }

        AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
        SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
        TargetObjects.Add(TargetObject);
        TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
        AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
        TargetPackages.Add(TargetObject->GetOutermost());
        ++CreatedCount;
        ++FallbackDuplicateCount;
        if (bFreshCreateCandidate)
        {
            ++FreshCreateFallbackCount;
        }

        if (UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject))
        {
            TargetBlueprints.Add(TargetBlueprint);
        }
        if (UUserDefinedStruct* TargetStruct = Cast<UUserDefinedStruct>(TargetObject))
        {
            TargetUserDefinedStructs.Add(TargetStruct);
        }

        const FString FallbackDetail = bFreshCreateCandidate && !FreshCreateDetail.IsEmpty()
            ? FString::Printf(TEXT("fresh create failed: %s; fallback duplicate used"), *FreshCreateDetail)
            : GetFallbackReason(SourceAsset);
        TSharedPtr<FJsonObject> FallbackReport = AssetReportToJson(SourceAsset, TargetPath, TEXT("fallback_duplicate"), TEXT("created"), FallbackDetail);
        if (bFreshCreateCandidate)
        {
            ClassifyFreshCreateFallbackForMCP(
                SourceAsset,
                TargetPath,
                FreshCreateDetail,
                AcceptedFallbackRules,
                FallbackReport,
                AcceptedFallbackCount,
                UnresolvedFallbackCount,
                AcceptedFallbackSamples,
                UnresolvedFallbackSamples);
        }
        AssetReports.Add(MakeShared<FJsonValueObject>(FallbackReport));
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddBlueprintReplacementMappings(ReplacementMap, Pair.Key, Pair.Value);
    }

    for (const FDeferredGeneratedClassAsset& DeferredAsset : DeferredGeneratedClassAssets)
    {
        if (!Check()) return {};
        UObject* SourceObject = LoadAssetDataForGD(DeferredAsset.SourceAsset);
        if (!Check()) return {};
        UClass* SourceClass = SourceObject ? SourceObject->GetClass() : nullptr;
        UClass* TargetClass = Cast<UClass>(FindReplacementObject(ReplacementMap, SourceClass));
        if (!SourceObject || !SourceClass || !TargetClass)
        {
            FailedAssets.Add(DeferredAsset.SourceAsset.PackageName.ToString());
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                DeferredAsset.SourceAsset,
                DeferredAsset.TargetPath,
                DeferredAsset.Strategy,
                TEXT("failed"),
                TEXT("could not resolve target generated class"))));
            continue;
        }

        if (TargetClass->GetPropertiesSize() < SourceClass->GetPropertiesSize())
        {
            FailedAssets.Add(DeferredAsset.SourceAsset.PackageName.ToString());
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                DeferredAsset.SourceAsset,
                DeferredAsset.TargetPath,
                DeferredAsset.Strategy,
                TEXT("failed"),
                TEXT("target generated class is not serialization-compatible"))));
            continue;
        }

        const FString TargetDirectory = FPackageName::GetLongPackagePath(DeferredAsset.TargetPath);
        UEditorAssetLibrary::MakeDirectory(TargetDirectory);

        FString FreshCreateDetail;
        UObject* TargetObject = CreateFreshPropertyCopyAsset(
            SourceObject,
            DeferredAsset.TargetPath,
            TargetClass,
            &ReplacementMap,
            FreshCreateDetail);
        bool bFreshGeneratedClassCreated = TargetObject != nullptr;

        if (!TargetObject && !bAllowFallbackDuplicate)
        {
            FailedAssets.Add(DeferredAsset.SourceAsset.PackageName.ToString());
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                DeferredAsset.SourceAsset,
                DeferredAsset.TargetPath,
                DeferredAsset.Strategy,
                TEXT("failed"),
                FreshCreateDetail.IsEmpty()
                    ? TEXT("fresh generated-class property copy failed; fallback duplicate disabled")
                    : FString::Printf(TEXT("fresh generated-class property copy failed: %s; fallback duplicate disabled"), *FreshCreateDetail))));
            continue;
        }

        if (!TargetObject)
        {
            if (!Check()) return {};
            UPackage* TargetPackage = CreatePackageForGD(*DeferredAsset.TargetPath);
            if (!Check()) return {};
            if (!TargetPackage)
            {
                FailedAssets.Add(DeferredAsset.SourceAsset.PackageName.ToString());
                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    DeferredAsset.SourceAsset,
                    DeferredAsset.TargetPath,
                    TEXT("fallback_duplicate"),
                    TEXT("failed"),
                    TEXT("could not create target package"))));
                continue;
            }

            FObjectDuplicationParameters DuplicationParameters(SourceObject, TargetPackage);
            DuplicationParameters.DestName = FName(*FPackageName::GetShortName(DeferredAsset.TargetPath));
            DuplicationParameters.DestClass = TargetClass;
            DuplicationParameters.ApplyFlags = RF_Public | RF_Standalone | RF_Transactional;
            if (!Check()) return {};
            TargetObject = CreateOwnedFactory(DuplicationParameters.DestOuter, DuplicationParameters.DestClass, DuplicationParameters.DestName, TEXT("StaticDuplicateObjectEx"), [&]() { return StaticDuplicateObjectEx(DuplicationParameters); });
            if (!Check()) return {};
            if (!TargetObject)
            {
                FailedAssets.Add(DeferredAsset.SourceAsset.PackageName.ToString());
                AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                    DeferredAsset.SourceAsset,
                    DeferredAsset.TargetPath,
                    TEXT("fallback_duplicate"),
                    TEXT("failed"),
                    TEXT("generated-class asset duplication failed"))));
                continue;
            }

            if (!Write(TargetObject, TEXT("SetFlags"))) return {};
            TargetObject->SetFlags(RF_Public | RF_Standalone | RF_Transactional);
            if (!After()) return {};
            if (!Write(TargetObject, TEXT("ClearFlags"))) return {};
            TargetObject->ClearFlags(RF_Transient);
            if (!After()) return {};
            if (!Write(TargetPackage, TEXT("MarkPackageDirty"))) return {};
            TargetPackage->MarkPackageDirty();
            if (!After()) return {};
            if (!Write(TargetObject, TEXT("FAssetRegistryModule_AssetCreated"))) return {};
            FAssetRegistryModule::AssetCreated(TargetObject);
            if (!After()) return {};
            bFreshGeneratedClassCreated = false;
        }

        AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
        SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
        TargetObjects.Add(TargetObject);
        TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
        AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
        TargetPackages.Add(TargetObject->GetOutermost());
        ++CreatedCount;
        ++GeneratedClassAssetRecreateCount;
        if (bFreshGeneratedClassCreated)
        {
            ++FreshGeneratedClassAssetCount;
        }
        else
        {
            ++FallbackDuplicateCount;
            ++FreshCreateFallbackCount;
        }

        const FString DeferredFallbackDetail = FreshCreateDetail.IsEmpty()
            ? TEXT("fresh generated-class property copy failed; duplicated_with_target_generated_class")
            : FString::Printf(TEXT("fresh generated-class property copy failed: %s; duplicated_with_target_generated_class"), *FreshCreateDetail);
        TSharedPtr<FJsonObject> DeferredAssetReport = AssetReportToJson(
            DeferredAsset.SourceAsset,
            DeferredAsset.TargetPath,
            bFreshGeneratedClassCreated ? DeferredAsset.Strategy : TEXT("fallback_duplicate"),
            TEXT("created"),
            bFreshGeneratedClassCreated ? FreshCreateDetail : DeferredFallbackDetail);
        if (!bFreshGeneratedClassCreated)
        {
            ClassifyFreshCreateFallbackForMCP(
                DeferredAsset.SourceAsset,
                DeferredAsset.TargetPath,
                FreshCreateDetail,
                AcceptedFallbackRules,
                DeferredAssetReport,
                AcceptedFallbackCount,
                UnresolvedFallbackCount,
                AcceptedFallbackSamples,
                UnresolvedFallbackSamples);
        }
        AssetReports.Add(MakeShared<FJsonValueObject>(DeferredAssetReport));
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        UBlueprint* SourceBlueprint = Cast<UBlueprint>(Pair.Key);
        UBlueprint* TargetBlueprint = Cast<UBlueprint>(Pair.Value);
        if (SourceBlueprint && TargetBlueprint)
        {
            SCSNodeRepairCount += RepairBlueprintSCSNodes(SourceBlueprint, TargetBlueprint, ReplacementMap);
            WidgetTreeRepairCount += RepairWidgetBlueprintTree(TargetBlueprint, ReplacementMap);
        }
    }
    GeneratedClassSubobjectRepairCount += RepairTargetSubobjectsWithSourceClasses(TargetPackages, ReplacementMap);

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddObjectPathReplacements(PathReplacements, Pair.Key, Pair.Value);
    }
    PathReplacements.Sort([](const FPathStringReplacement& A, const FPathStringReplacement& B)
    {
        return A.Source.Len() > B.Source.Len();
    });

    if (!bDryRun && bRemapReferences && !TargetUserDefinedStructs.IsEmpty())
    {
        UserDefinedStructDescriptionPathRemapCount += RemapUserDefinedStructDescriptionPaths(
            TargetUserDefinedStructs,
            PathReplacements,
            ReplacementMap,
            SourceRoot);
    }

    if (!bDryRun && bRemapReferences && ReplacementMap.Num() > 0)
    {
        TSet<UObject*> ObjectsToRemap;
        for (UObject* TargetObject : TargetObjects)
        {
            if (IsLiveObjectForMCP(TargetObject))
            {
                ObjectsToRemap.Add(TargetObject);
            }
        }
        for (UPackage* TargetPackage : TargetPackages)
        {
            if (!TargetPackage)
            {
                continue;
            }

            TArray<UObject*> PackageObjects;
            GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
            for (UObject* PackageObject : PackageObjects)
            {
                if (IsLiveObjectForMCP(PackageObject))
                {
                    ObjectsToRemap.Add(PackageObject);
                }
            }
        }

        for (UObject* TargetObject : ObjectsToRemap)
        {
            if (!IsLiveObjectForMCP(TargetObject))
            {
                continue;
            }

            if (ShouldSkipAssetArchiveObjectReferenceRemap(TargetObject))
            {
                ++ArchiveObjectRemapSkipCount;
                continue;
            }

            if (bLogArchiveRemapObjects)
            {
                const UClass* TargetObjectClass = TargetObject->GetClass();
                UE_LOG(LogTemp, Display, TEXT("MCP recreate archive remap object: %s | class=%s"),
                    *TargetObject->GetPathName(),
                    TargetObjectClass ? *TargetObjectClass->GetPathName() : TEXT("<null>"));
            }

            if (!Write(TargetObject, TEXT("Modify"))) return {};
            TargetObject->Modify();
            if (!After()) return {};
            if (!Check()) return {};
            const int64 ReplacementCount = RemapOwnedReferences(
                TargetObject,
                ReplacementMap,
                EArchiveReplaceObjectFlags::IgnoreOuterRef |
                EArchiveReplaceObjectFlags::IgnoreArchetypeRef |
                EArchiveReplaceObjectFlags::IncludeClassGeneratedByRef);
            if (!Check()) return {};

            if (ReplacementCount > 0)
            {
                ++RemappedObjectCount;
                if (!Write(TargetObject, TEXT("MarkPackageDirty"))) return {};
                TargetObject->MarkPackageDirty();
                if (!After()) return {};
                if (UPackage* Package = TargetObject->GetOutermost())
                {
                    if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
                    Package->MarkPackageDirty();
                    if (!After()) return {};
                }
            }
        }
    }

    if (!bDryRun && !TargetUserDefinedStructs.IsEmpty())
    {
        UserDefinedStructCompileErrorCount += CompileUserDefinedStructsForMCP(TargetUserDefinedStructs, &FailedAssets);
    }

    if (!bDryRun && bRemapReferences && ReplacementMap.Num() > 0)
    {
        for (UBlueprint* Blueprint : TargetBlueprints)
        {
            BlueprintGraphReferenceRemapCount += RemapBlueprintGraphReferences(Blueprint, ReplacementMap, PathReplacements);
        }
    }

    if (!bDryRun && bRemapReferences)
    {
        RefreshMaterialReferencesAndCaches(
            TargetPackages,
            ReplacementMap,
            MaterialTextureReferenceRepairCount,
            MaterialCacheRefreshCount);

        if (!TargetUserDefinedStructs.IsEmpty())
        {
            UserDefinedStructCompileErrorCount += CompileUserDefinedStructsForMCP(TargetUserDefinedStructs, &FailedAssets);
        }
    }

    if (!bDryRun && bSaveAssets)
    {
        for (UObject* TargetObject : TargetObjects)
        {
            if (SaveLoadedAssetForMCP(TargetObject))
            {
                ++SavedCount;
            }
        }
    }

    TArray<FString> OriginalDependencySamples;
    TSet<FName> OriginalDependencyPackages;
    int32 OriginalDependencyAssetCount = 0;
    if (!bDryRun)
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(TargetRoot);
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
        OriginalDependencyAssetCount = CollectOriginalDependencyPackages(
            AssetRegistry,
            TargetPackageNames,
            SourceRoot,
            OriginalDependencyPackages,
            OriginalDependencySamples,
            50);
    }

    if (!bDryRun && bRemapReferences && PathReplacements.Num() > 0 && OriginalDependencyPackages.Num() > 0)
    {
        TSet<UObject*> ObjectsToPathRemap;
        for (const FName& PackageName : OriginalDependencyPackages)
        {
            UPackage* TargetPackage = FindPackage(nullptr, *PackageName.ToString());
            if (!TargetPackage)
            {
                if (!Check()) return {};
                TargetPackage = LoadPackageForGD(nullptr, *PackageName.ToString(), LOAD_None);
                if (!Check()) return {};
            }
            if (!TargetPackage)
            {
                continue;
            }

            TArray<UObject*> PackageObjects;
            GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
            for (UObject* PackageObject : PackageObjects)
            {
                if (IsLiveObjectForMCP(PackageObject))
                {
                    ObjectsToPathRemap.Add(PackageObject);
                }
            }
        }

        for (UObject* TargetObject : ObjectsToPathRemap)
        {
            if (!IsLiveObjectForMCP(TargetObject))
            {
                continue;
            }

            PathPropertyRemapCount += ReplaceExportedPropertyPaths(TargetObject, PathReplacements, SourceRoot);
        }

        if (!TargetUserDefinedStructs.IsEmpty())
        {
            UserDefinedStructDescriptionPathRemapCount += RemapUserDefinedStructDescriptionPaths(
                TargetUserDefinedStructs,
                PathReplacements,
                ReplacementMap,
                SourceRoot);
        }

        RefreshMaterialReferencesAndCaches(
            TargetPackages,
            ReplacementMap,
            MaterialTextureReferenceRepairCount,
            MaterialCacheRefreshCount);
    }

    if (!bDryRun && bSaveAssets && (PathPropertyRemapCount > 0 || UserDefinedStructDescriptionPathRemapCount > 0))
    {
        for (UObject* TargetObject : TargetObjects)
        {
            if (SaveLoadedAssetForMCP(TargetObject))
            {
                ++SavedCount;
            }
        }
    }

    if (!bDryRun && bCompileBlueprints)
    {
        CompiledBlueprintCount = CompileBlueprintsForMCP(
            TargetBlueprints,
            CompilePasses,
            bRefreshBlueprintNodes,
            bRefreshFailedBlueprintNodes,
            BlueprintCompileErrorCount,
            &FailedAssets);
    }

    if (!Current->R.Error.IsEmpty()) return LegacyError(Current->R.Error);
    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs) AddBlueprintReplacementMappings(ReplacementMap, Pair.Key, Pair.Value);

    if (!bDryRun && bSaveAssets)
    {
        for (UBlueprint* TargetBlueprint : TargetBlueprints)
        {
            if (SaveLoadedAssetForMCP(TargetBlueprint))
            {
                ++SavedCount;
            }
        }
    }

    if (!bDryRun && bRemapReferences && bRepairLevelActors)
    {
        TArray<FString> WorldPackagesForLevelRepair;
        if (bForceRepairLevelActors)
        {
            WorldPackagesForLevelRepair = TargetWorldPackageNames;
        }
        else
        {
            for (const FString& WorldPackageName : TargetWorldPackageNames)
            {
                if (OriginalDependencyPackages.Contains(FName(*WorldPackageName)))
                {
                    WorldPackagesForLevelRepair.Add(WorldPackageName);
                }
                else
                {
                    ++SkippedCleanWorldRepairCount;
                }
            }
        }

        LevelActorClassRepairCount += RepairTargetWorldActorsWithSourceClasses(
            WorldPackagesForLevelRepair,
            ReplacementMap,
            PathReplacements,
            SourceRoot,
            LevelActorReferenceRemapCount,
            SavedMapCount,
            LevelComponentClassRepairCount,
            FailedAssets);
    }

    if (!bDryRun)
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(TargetRoot);
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
        OriginalDependencyAssetCount = CollectOriginalDependencyPackages(
            AssetRegistry,
            TargetPackageNames,
            SourceRoot,
            OriginalDependencyPackages,
            OriginalDependencySamples,
            50);
    }

    TArray<TSharedPtr<FJsonValue>> ClassCountValues;
    for (const TPair<FString, int32>& Pair : ClassCounts)
    {
        TSharedPtr<FJsonObject> ClassObject = MakeShared<FJsonObject>();
        ClassObject->SetStringField(TEXT("class"), Pair.Key);
        ClassObject->SetNumberField(TEXT("count"), Pair.Value);
        ClassCountValues.Add(MakeShared<FJsonValueObject>(ClassObject));
    }

    const FEditorLogHealthResult EditorLogHealth = ScanEditorLogSince(EditorLogMarker, TargetRoot);
    const bool bEditorLogVerificationPass = !bCheckEditorLogHealth || !bFailOnEditorLogIssues || EditorLogHealth.bPass;
    const bool bVerificationPass = FailedAssets.IsEmpty()
        && UnresolvedFallbackCount == 0
        && BlueprintCompileErrorCount == 0
        && UserDefinedStructCompileErrorCount == 0
        && OriginalDependencyAssetCount == 0
        && bEditorLogVerificationPass;

    ReportObject->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ReportObject->SetNumberField(TEXT("fallback_duplicate_count"), FallbackDuplicateCount);
    ReportObject->SetNumberField(TEXT("created_count"), CreatedCount);
    ReportObject->SetNumberField(TEXT("saved_count"), SavedCount);
    ReportObject->SetStringField(TEXT("asset_save_mode"), TEXT("package_save_no_prompt"));
    ReportObject->SetNumberField(TEXT("remapped_object_count"), RemappedObjectCount);
    ReportObject->SetNumberField(TEXT("archive_object_remap_skip_count"), ArchiveObjectRemapSkipCount);
    ReportObject->SetNumberField(TEXT("path_property_remap_count"), PathPropertyRemapCount);
    ReportObject->SetNumberField(TEXT("generated_class_asset_recreate_count"), GeneratedClassAssetRecreateCount);
    ReportObject->SetNumberField(TEXT("fresh_property_copy_asset_count"), FreshPropertyCopyAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_generated_class_asset_count"), FreshGeneratedClassAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_material_asset_count"), FreshMaterialAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_material_function_asset_count"), FreshMaterialFunctionAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_material_expression_mapping_count"), FreshMaterialExpressionMappingCount);
    ReportObject->SetNumberField(TEXT("fresh_texture2d_asset_count"), FreshTexture2DAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_texturecube_asset_count"), FreshTextureCubeAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_volumetexture_asset_count"), FreshVolumeTextureAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_static_mesh_asset_count"), FreshStaticMeshAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_sound_wave_asset_count"), FreshSoundWaveAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_data_table_asset_count"), FreshDataTableAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_user_defined_enum_asset_count"), FreshUserDefinedEnumAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_user_defined_struct_asset_count"), FreshUserDefinedStructAssetCount);
    ReportObject->SetNumberField(TEXT("fresh_create_fallback_count"), FreshCreateFallbackCount);
    ReportObject->SetNumberField(TEXT("accepted_fallback_rule_count"), AcceptedFallbackRules.Num());
    ReportObject->SetNumberField(TEXT("accepted_fallback_count"), AcceptedFallbackCount);
    ReportObject->SetNumberField(TEXT("unresolved_fallback_count"), UnresolvedFallbackCount);
    ReportObject->SetNumberField(TEXT("user_defined_struct_description_path_remap_count"), UserDefinedStructDescriptionPathRemapCount);
    ReportObject->SetNumberField(TEXT("scs_node_repair_count"), SCSNodeRepairCount);
    ReportObject->SetNumberField(TEXT("widget_tree_repair_count"), WidgetTreeRepairCount);
    ReportObject->SetNumberField(TEXT("generated_class_subobject_repair_count"), GeneratedClassSubobjectRepairCount);
    ReportObject->SetNumberField(TEXT("material_texture_reference_repair_count"), MaterialTextureReferenceRepairCount);
    ReportObject->SetNumberField(TEXT("material_cache_refresh_count"), MaterialCacheRefreshCount);
    ReportObject->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ReportObject->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ReportObject->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ReportObject->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ReportObject->SetNumberField(TEXT("skipped_clean_world_repair_count"), SkippedCleanWorldRepairCount);
    ReportObject->SetNumberField(TEXT("blueprint_graph_reference_remap_count"), BlueprintGraphReferenceRemapCount);
    ReportObject->SetNumberField(TEXT("compiled_blueprint_count"), CompiledBlueprintCount);
    ReportObject->SetNumberField(TEXT("blueprint_compile_error_count"), BlueprintCompileErrorCount);
    ReportObject->SetNumberField(TEXT("user_defined_struct_compile_error_count"), UserDefinedStructCompileErrorCount);
    ReportObject->SetNumberField(TEXT("original_dependency_asset_count"), OriginalDependencyAssetCount);
    AddEditorLogHealthFields(ReportObject, EditorLogHealth);
    ReportObject->SetStringField(TEXT("delete_target_root_mode"), DeleteTargetRootMode);
    ReportObject->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ReportObject->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ReportObject->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ReportObject->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ReportObject->SetBoolField(TEXT("log_archive_remap_objects"), bLogArchiveRemapObjects);
    ReportObject->SetBoolField(TEXT("refresh_blueprint_nodes"), bRefreshBlueprintNodes);
    ReportObject->SetBoolField(TEXT("refresh_failed_blueprint_nodes"), bRefreshFailedBlueprintNodes);
    ReportObject->SetBoolField(TEXT("repair_level_actors"), bRepairLevelActors);
    ReportObject->SetBoolField(TEXT("force_repair_level_actors"), bForceRepairLevelActors);
    ReportObject->SetArrayField(TEXT("class_counts"), ClassCountValues);
    AddStringArrayField(ReportObject, TEXT("failed_assets"), FailedAssets);
    AddStringArrayField(ReportObject, TEXT("skipped_existing_assets"), SkippedExistingAssets);
    AddStringArrayField(ReportObject, TEXT("accepted_fallback_samples"), AcceptedFallbackSamples);
    AddStringArrayField(ReportObject, TEXT("unresolved_fallback_samples"), UnresolvedFallbackSamples);
    AddStringArrayField(ReportObject, TEXT("delete_target_root_warnings"), DeleteTargetRootWarnings);
    AddStringArrayField(ReportObject, TEXT("original_dependency_samples"), OriginalDependencySamples);
    ReportObject->SetArrayField(TEXT("assets"), AssetReports);
    if (!FailedAssets.IsEmpty())
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d failed assets"), FailedAssets.Num()));
    }
    else if (UnresolvedFallbackCount > 0)
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d unresolved fresh-create fallback asset(s)"), UnresolvedFallbackCount));
    }
    else if (OriginalDependencyAssetCount > 0)
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d target assets still depending on the source root"), OriginalDependencyAssetCount));
    }
    else if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
    }

    FString ReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportJson);
    FJsonSerializer::Serialize(ReportObject.ToSharedRef(), Writer);

    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString ReportPath = FPaths::Combine(ReportDirectory, BuildMCPReportFilename(TargetRoot, TEXT("recreate")));
    FFileHelper::SaveStringToFile(ReportJson, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ResultObj->SetStringField(TEXT("source_root"), SourceRoot);
    ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
    ResultObj->SetStringField(TEXT("report_path"), ReportPath);
    ResultObj->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ResultObj->SetNumberField(TEXT("fallback_duplicate_count"), FallbackDuplicateCount);
    ResultObj->SetNumberField(TEXT("created_count"), CreatedCount);
    ResultObj->SetNumberField(TEXT("saved_count"), SavedCount);
    ResultObj->SetStringField(TEXT("asset_save_mode"), TEXT("package_save_no_prompt"));
    ResultObj->SetNumberField(TEXT("remapped_object_count"), RemappedObjectCount);
    ResultObj->SetNumberField(TEXT("archive_object_remap_skip_count"), ArchiveObjectRemapSkipCount);
    ResultObj->SetNumberField(TEXT("path_property_remap_count"), PathPropertyRemapCount);
    ResultObj->SetNumberField(TEXT("generated_class_asset_recreate_count"), GeneratedClassAssetRecreateCount);
    ResultObj->SetNumberField(TEXT("fresh_property_copy_asset_count"), FreshPropertyCopyAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_generated_class_asset_count"), FreshGeneratedClassAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_material_asset_count"), FreshMaterialAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_material_function_asset_count"), FreshMaterialFunctionAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_material_expression_mapping_count"), FreshMaterialExpressionMappingCount);
    ResultObj->SetNumberField(TEXT("fresh_texture2d_asset_count"), FreshTexture2DAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_texturecube_asset_count"), FreshTextureCubeAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_volumetexture_asset_count"), FreshVolumeTextureAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_static_mesh_asset_count"), FreshStaticMeshAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_sound_wave_asset_count"), FreshSoundWaveAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_data_table_asset_count"), FreshDataTableAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_user_defined_enum_asset_count"), FreshUserDefinedEnumAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_user_defined_struct_asset_count"), FreshUserDefinedStructAssetCount);
    ResultObj->SetNumberField(TEXT("fresh_create_fallback_count"), FreshCreateFallbackCount);
    ResultObj->SetNumberField(TEXT("accepted_fallback_rule_count"), AcceptedFallbackRules.Num());
    ResultObj->SetNumberField(TEXT("accepted_fallback_count"), AcceptedFallbackCount);
    ResultObj->SetNumberField(TEXT("unresolved_fallback_count"), UnresolvedFallbackCount);
    ResultObj->SetNumberField(TEXT("user_defined_struct_description_path_remap_count"), UserDefinedStructDescriptionPathRemapCount);
    ResultObj->SetNumberField(TEXT("scs_node_repair_count"), SCSNodeRepairCount);
    ResultObj->SetNumberField(TEXT("widget_tree_repair_count"), WidgetTreeRepairCount);
    ResultObj->SetNumberField(TEXT("generated_class_subobject_repair_count"), GeneratedClassSubobjectRepairCount);
    ResultObj->SetNumberField(TEXT("material_texture_reference_repair_count"), MaterialTextureReferenceRepairCount);
    ResultObj->SetNumberField(TEXT("material_cache_refresh_count"), MaterialCacheRefreshCount);
    ResultObj->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ResultObj->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ResultObj->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ResultObj->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ResultObj->SetNumberField(TEXT("skipped_clean_world_repair_count"), SkippedCleanWorldRepairCount);
    ResultObj->SetNumberField(TEXT("blueprint_graph_reference_remap_count"), BlueprintGraphReferenceRemapCount);
    ResultObj->SetNumberField(TEXT("compiled_blueprint_count"), CompiledBlueprintCount);
    ResultObj->SetNumberField(TEXT("blueprint_compile_error_count"), BlueprintCompileErrorCount);
    ResultObj->SetNumberField(TEXT("user_defined_struct_compile_error_count"), UserDefinedStructCompileErrorCount);
    ResultObj->SetNumberField(TEXT("original_dependency_asset_count"), OriginalDependencyAssetCount);
    AddEditorLogHealthFields(ResultObj, EditorLogHealth);
    ResultObj->SetStringField(TEXT("delete_target_root_mode"), DeleteTargetRootMode);
    ResultObj->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ResultObj->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ResultObj->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ResultObj->SetBoolField(TEXT("log_archive_remap_objects"), bLogArchiveRemapObjects);
    ResultObj->SetBoolField(TEXT("refresh_blueprint_nodes"), bRefreshBlueprintNodes);
    ResultObj->SetBoolField(TEXT("refresh_failed_blueprint_nodes"), bRefreshFailedBlueprintNodes);
    ResultObj->SetBoolField(TEXT("repair_level_actors"), bRepairLevelActors);
    ResultObj->SetBoolField(TEXT("force_repair_level_actors"), bForceRepairLevelActors);
    AddStringArrayField(ResultObj, TEXT("failed_asset_samples"), FailedAssets);
    AddStringArrayField(ResultObj, TEXT("accepted_fallback_samples"), AcceptedFallbackSamples);
    AddStringArrayField(ResultObj, TEXT("unresolved_fallback_samples"), UnresolvedFallbackSamples);
    AddStringArrayField(ResultObj, TEXT("delete_target_root_warnings"), DeleteTargetRootWarnings);
    AddStringArrayField(ResultObj, TEXT("original_dependency_samples"), OriginalDependencySamples);
    if (!FailedAssets.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d failed assets"), FailedAssets.Num()));
    }
    else if (UnresolvedFallbackCount > 0)
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d unresolved fresh-create fallback asset(s)"), UnresolvedFallbackCount));
    }
    else if (OriginalDependencyAssetCount > 0)
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate finished with %d target assets still depending on the source root"), OriginalDependencyAssetCount));
    }
    else if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Recreate editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
    }
    return ResultObj;
}



TSharedPtr<FJsonObject> HandlePostprocessContentFolderMCP(const TSharedPtr<FJsonObject>& Params)
{
    if (!Check()) return {};

    FString SourceRoot;
    FString TargetRoot;
    FString RootParamError;
    if (!TryGetRequiredContentRootParamForMCP(Params, TEXT("source_root"), SourceRoot, RootParamError) ||
        !TryGetRequiredContentRootParamForMCP(Params, TEXT("target_root"), TargetRoot, RootParamError) ||
        !ValidateContentRootPairForMCP(SourceRoot, TargetRoot, RootParamError))
    {
        return LegacyError(RootParamError);
    }
    const FString Suffix = GetStringParam(Params, TEXT("suffix"), TEXT("_MCP"));
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false);
    const bool bRemapReferences = GetBoolParam(Params, TEXT("remap_references"), true);
    const bool bCompileBlueprints = GetBoolParam(Params, TEXT("compile_blueprints"), true);
    const bool bRefreshBlueprintNodes = GetBoolParam(Params, TEXT("refresh_blueprint_nodes"), false);
    const bool bRefreshFailedBlueprintNodes = GetBoolParam(Params, TEXT("refresh_failed_blueprint_nodes"), true);
    const bool bSaveAssets = GetBoolParam(Params, TEXT("save_assets"), true);
    const bool bRepairLevelActors = GetBoolParam(Params, TEXT("repair_level_actors"), false);
    const bool bForceRepairLevelActors = GetBoolParam(Params, TEXT("force_repair_level_actors"), false);
    const int32 CompilePasses = FMath::Max(1, GetIntParam(Params, TEXT("compile_passes"), 2));
    const bool bCheckEditorLogHealth = GetBoolParam(Params, TEXT("check_editor_log_health"), true);
    const bool bFailOnEditorLogIssues = GetBoolParam(Params, TEXT("fail_on_editor_log_issues"), true);
    const bool bSuppressEditorPrompts = GetBoolParam(Params, TEXT("suppress_editor_prompts"), true);
    TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || bSuppressEditorPrompts);
    const FEditorLogMarker EditorLogMarker = CaptureEditorLogMarker(bCheckEditorLogHealth);

    if (!FPackageName::IsValidLongPackageName(SourceRoot) || !SourceRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid source_root: %s"), *SourceRoot));
    }

    if (!FPackageName::IsValidLongPackageName(TargetRoot) || !TargetRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid target_root: %s"), *TargetRoot));
    }

    if (TargetRoot.Equals(SourceRoot) || TargetRoot.StartsWith(SourceRoot + TEXT("/")))
    {
        return LegacyError(TEXT("target_root must not be the source root or a child of source_root"));
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FString> InitialScanPaths;
    InitialScanPaths.Add(SourceRoot);
    InitialScanPaths.Add(TargetRoot);
    AssetRegistry.ScanPathsSynchronous(InitialScanPaths, true);

    TArray<FAssetData> SourceAssets;
    AssetRegistry.GetAssetsByPath(FName(*SourceRoot), SourceAssets, true);
    SourceAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.PackageName.LexicalLess(B.PackageName);
    });

    if (SourceAssets.IsEmpty())
    {
        return LegacyError(FString::Printf(TEXT("No source assets found under %s"), *SourceRoot));
    }

    TSharedPtr<FJsonObject> ReportObject = MakeShared<FJsonObject>();
    ReportObject->SetStringField(TEXT("operation"), TEXT("postprocess_content_folder_mcp"));
    ReportObject->SetStringField(TEXT("source_root"), SourceRoot);
    ReportObject->SetStringField(TEXT("target_root"), TargetRoot);
    ReportObject->SetStringField(TEXT("suffix"), Suffix);
    ReportObject->SetBoolField(TEXT("dry_run"), bDryRun);

    TArray<TSharedPtr<FJsonValue>> AssetReports;
    TMap<UObject*, UObject*> ReplacementMap;
    TArray<FPathStringReplacement> PathReplacements;
    TArray<TPair<UObject*, UObject*>> SourceTargetPairs;
    TArray<UObject*> TargetObjects;
    TArray<UBlueprint*> TargetBlueprints;
    TArray<UUserDefinedStruct*> TargetUserDefinedStructs;
    TArray<FName> TargetPackageNames;
    TArray<FString> TargetWorldPackageNames;
    TSet<UPackage*> TargetPackages;
    TArray<FString> FailedAssets;
    TArray<FString> MissingTargetAssets;

    int32 PairedAssetCount = 0;
    int32 SavedCount = 0;
    int32 RemappedObjectCount = 0;
    int32 ArchiveObjectRemapSkipCount = 0;
    int32 PathPropertyRemapCount = 0;
    int32 UserDefinedStructDescriptionPathRemapCount = 0;
    int32 SCSNodeRepairCount = 0;
    int32 WidgetTreeRepairCount = 0;
    int32 GeneratedClassSubobjectRepairCount = 0;
    int32 MaterialTextureReferenceRepairCount = 0;
    int32 MaterialCacheRefreshCount = 0;
    int32 LevelActorReferenceRemapCount = 0;
    int32 LevelActorClassRepairCount = 0;
    int32 LevelComponentClassRepairCount = 0;
    int32 SavedMapCount = 0;
    int32 SkippedCleanWorldRepairCount = 0;
    int32 BlueprintGraphReferenceRemapCount = 0;
    int32 CompiledBlueprintCount = 0;
    int32 BlueprintCompileErrorCount = 0;

    for (const FAssetData& SourceAsset : SourceAssets)
    {
        const FString TargetPath = BuildTargetAssetPath(SourceAsset, SourceRoot, TargetRoot, Suffix);
        if (!UEditorAssetLibrary::DoesAssetExist(TargetPath))
        {
            MissingTargetAssets.Add(FString::Printf(TEXT("%s -> %s"), *SourceAsset.PackageName.ToString(), *TargetPath));
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                SourceAsset,
                TargetPath,
                TEXT("postprocess_existing_target"),
                TEXT("missing_target"),
                TEXT("target asset does not exist"))));
            continue;
        }

        if (bDryRun)
        {
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                SourceAsset,
                TargetPath,
                TEXT("postprocess_existing_target"),
                TEXT("planned"),
                TEXT("target asset exists"))));
            ++PairedAssetCount;
            continue;
        }

        if (!Check()) return {};
        UObject* SourceObject = LoadAssetDataForGD(SourceAsset);
        if (!Check()) return {};
        if (!Check()) return {};
        UObject* TargetObject = LoadAssetForGD(TargetPath);
        if (!Check()) return {};
        if (!SourceObject || !TargetObject)
        {
            FailedAssets.Add(FString::Printf(TEXT("%s -> %s (failed to load source or target asset)"), *SourceAsset.PackageName.ToString(), *TargetPath));
            AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
                SourceAsset,
                TargetPath,
                TEXT("postprocess_existing_target"),
                TEXT("failed"),
                TEXT("failed to load source or target asset"))));
            continue;
        }

        AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
        SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
        TargetObjects.Add(TargetObject);
        TargetPackageNames.Add(TargetObject->GetOutermost()->GetFName());
        AddTargetWorldPackageName(TargetWorldPackageNames, TargetObject);
        TargetPackages.Add(TargetObject->GetOutermost());
        ++PairedAssetCount;

        if (UBlueprint* TargetBlueprint = Cast<UBlueprint>(TargetObject))
        {
            TargetBlueprints.Add(TargetBlueprint);
        }
        if (UUserDefinedStruct* TargetStruct = Cast<UUserDefinedStruct>(TargetObject))
        {
            TargetUserDefinedStructs.Add(TargetStruct);
        }

        AssetReports.Add(MakeShared<FJsonValueObject>(AssetReportToJson(
            SourceAsset,
            TargetPath,
            TEXT("postprocess_existing_target"),
            TEXT("paired"),
            TEXT("loaded_existing_target"))));
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddBlueprintReplacementMappings(ReplacementMap, Pair.Key, Pair.Value);
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        UBlueprint* SourceBlueprint = Cast<UBlueprint>(Pair.Key);
        UBlueprint* TargetBlueprint = Cast<UBlueprint>(Pair.Value);
        if (SourceBlueprint && TargetBlueprint)
        {
            SCSNodeRepairCount += RepairBlueprintSCSNodes(SourceBlueprint, TargetBlueprint, ReplacementMap);
            WidgetTreeRepairCount += RepairWidgetBlueprintTree(TargetBlueprint, ReplacementMap);
        }
    }

    if (!bDryRun)
    {
        GeneratedClassSubobjectRepairCount += RepairTargetSubobjectsWithSourceClasses(TargetPackages, ReplacementMap);
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddObjectPathReplacements(PathReplacements, Pair.Key, Pair.Value);
    }
    PathReplacements.Sort([](const FPathStringReplacement& A, const FPathStringReplacement& B)
    {
        return A.Source.Len() > B.Source.Len();
    });

    if (!bDryRun && bRemapReferences && !TargetUserDefinedStructs.IsEmpty())
    {
        UserDefinedStructDescriptionPathRemapCount += RemapUserDefinedStructDescriptionPaths(
            TargetUserDefinedStructs,
            PathReplacements,
            ReplacementMap,
            SourceRoot);
    }

    if (!bDryRun && bRemapReferences && ReplacementMap.Num() > 0)
    {
        TSet<UObject*> ObjectsToRemap;
        for (UObject* TargetObject : TargetObjects)
        {
            if (IsLiveObjectForMCP(TargetObject))
            {
                ObjectsToRemap.Add(TargetObject);
            }
        }
        for (UPackage* TargetPackage : TargetPackages)
        {
            if (!TargetPackage)
            {
                continue;
            }

            TArray<UObject*> PackageObjects;
            GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
            for (UObject* PackageObject : PackageObjects)
            {
                if (IsLiveObjectForMCP(PackageObject))
                {
                    ObjectsToRemap.Add(PackageObject);
                }
            }
        }

        for (UObject* TargetObject : ObjectsToRemap)
        {
            if (!IsLiveObjectForMCP(TargetObject))
            {
                continue;
            }

            if (ShouldSkipAssetArchiveObjectReferenceRemap(TargetObject))
            {
                ++ArchiveObjectRemapSkipCount;
                continue;
            }

            if (!Write(TargetObject, TEXT("Modify"))) return {};
            TargetObject->Modify();
            if (!After()) return {};
            if (!Check()) return {};
            const int64 ReplacementCount = RemapOwnedReferences(
                TargetObject,
                ReplacementMap,
                EArchiveReplaceObjectFlags::IgnoreOuterRef |
                EArchiveReplaceObjectFlags::IgnoreArchetypeRef |
                EArchiveReplaceObjectFlags::IncludeClassGeneratedByRef);
            if (!Check()) return {};

            if (ReplacementCount > 0)
            {
                ++RemappedObjectCount;
                if (!Write(TargetObject, TEXT("MarkPackageDirty"))) return {};
                TargetObject->MarkPackageDirty();
                if (!After()) return {};
                if (UPackage* Package = TargetObject->GetOutermost())
                {
                    if (!Write(Package, TEXT("MarkPackageDirty"))) return {};
                    Package->MarkPackageDirty();
                    if (!After()) return {};
                }
            }
        }
    }

    if (!bDryRun && bRemapReferences && ReplacementMap.Num() > 0)
    {
        for (UBlueprint* Blueprint : TargetBlueprints)
        {
            BlueprintGraphReferenceRemapCount += RemapBlueprintGraphReferences(Blueprint, ReplacementMap, PathReplacements);
        }
    }

    if (!bDryRun && bRemapReferences)
    {
        RefreshMaterialReferencesAndCaches(
            TargetPackages,
            ReplacementMap,
            MaterialTextureReferenceRepairCount,
            MaterialCacheRefreshCount);
    }

    if (!bDryRun && bSaveAssets)
    {
        for (UObject* TargetObject : TargetObjects)
        {
            if (SaveLoadedAssetForMCP(TargetObject))
            {
                ++SavedCount;
            }
        }
    }

    TArray<FString> OriginalDependencySamples;
    TSet<FName> OriginalDependencyPackages;
    int32 OriginalDependencyAssetCount = 0;
    if (!bDryRun)
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(TargetRoot);
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
        OriginalDependencyAssetCount = CollectOriginalDependencyPackages(
            AssetRegistry,
            TargetPackageNames,
            SourceRoot,
            OriginalDependencyPackages,
            OriginalDependencySamples,
            50);
    }

    if (!bDryRun && bRemapReferences && PathReplacements.Num() > 0 && OriginalDependencyPackages.Num() > 0)
    {
        TSet<UObject*> ObjectsToPathRemap;
        for (const FName& PackageName : OriginalDependencyPackages)
        {
            UPackage* TargetPackage = FindPackage(nullptr, *PackageName.ToString());
            if (!TargetPackage)
            {
                if (!Check()) return {};
                TargetPackage = LoadPackageForGD(nullptr, *PackageName.ToString(), LOAD_None);
                if (!Check()) return {};
            }
            if (!TargetPackage)
            {
                continue;
            }

            TArray<UObject*> PackageObjects;
            GetObjectsWithPackage(TargetPackage, PackageObjects, true, RF_Transient, EInternalObjectFlags::Garbage);
            for (UObject* PackageObject : PackageObjects)
            {
                if (IsLiveObjectForMCP(PackageObject))
                {
                    ObjectsToPathRemap.Add(PackageObject);
                }
            }
        }

        for (UObject* TargetObject : ObjectsToPathRemap)
        {
            if (!IsLiveObjectForMCP(TargetObject))
            {
                continue;
            }

            PathPropertyRemapCount += ReplaceExportedPropertyPaths(TargetObject, PathReplacements, SourceRoot);
        }

        if (!TargetUserDefinedStructs.IsEmpty())
        {
            UserDefinedStructDescriptionPathRemapCount += RemapUserDefinedStructDescriptionPaths(
                TargetUserDefinedStructs,
                PathReplacements,
                ReplacementMap,
                SourceRoot);
        }

        RefreshMaterialReferencesAndCaches(
            TargetPackages,
            ReplacementMap,
            MaterialTextureReferenceRepairCount,
            MaterialCacheRefreshCount);
    }

    if (!bDryRun && bSaveAssets && (PathPropertyRemapCount > 0 || UserDefinedStructDescriptionPathRemapCount > 0))
    {
        for (UObject* TargetObject : TargetObjects)
        {
            if (SaveLoadedAssetForMCP(TargetObject))
            {
                ++SavedCount;
            }
        }
    }

    if (!bDryRun && bCompileBlueprints)
    {
        CompiledBlueprintCount = CompileBlueprintsForMCP(
            TargetBlueprints,
            CompilePasses,
            bRefreshBlueprintNodes,
            bRefreshFailedBlueprintNodes,
            BlueprintCompileErrorCount,
            &FailedAssets);
    }

    if (!Current->R.Error.IsEmpty()) return LegacyError(Current->R.Error);
    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs) AddBlueprintReplacementMappings(ReplacementMap, Pair.Key, Pair.Value);

    if (!bDryRun && bSaveAssets)
    {
        for (UBlueprint* TargetBlueprint : TargetBlueprints)
        {
            if (SaveLoadedAssetForMCP(TargetBlueprint))
            {
                ++SavedCount;
            }
        }
    }

    if (!bDryRun && bRemapReferences && bRepairLevelActors)
    {
        TArray<FString> WorldPackagesForLevelRepair;
        if (bForceRepairLevelActors)
        {
            WorldPackagesForLevelRepair = TargetWorldPackageNames;
        }
        else
        {
            for (const FString& WorldPackageName : TargetWorldPackageNames)
            {
                if (OriginalDependencyPackages.Contains(FName(*WorldPackageName)))
                {
                    WorldPackagesForLevelRepair.Add(WorldPackageName);
                }
                else
                {
                    ++SkippedCleanWorldRepairCount;
                }
            }
        }

        LevelActorClassRepairCount += RepairTargetWorldActorsWithSourceClasses(
            WorldPackagesForLevelRepair,
            ReplacementMap,
            PathReplacements,
            SourceRoot,
            LevelActorReferenceRemapCount,
            SavedMapCount,
            LevelComponentClassRepairCount,
            FailedAssets);
    }

    if (!bDryRun)
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(TargetRoot);
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);
        OriginalDependencyAssetCount = CollectOriginalDependencyPackages(
            AssetRegistry,
            TargetPackageNames,
            SourceRoot,
            OriginalDependencyPackages,
            OriginalDependencySamples,
            50);
    }

    const FEditorLogHealthResult EditorLogHealth = ScanEditorLogSince(EditorLogMarker, TargetRoot);
    const bool bEditorLogVerificationPass = !bCheckEditorLogHealth || !bFailOnEditorLogIssues || EditorLogHealth.bPass;
    const bool bVerificationPass = bDryRun
        ? FailedAssets.IsEmpty() && MissingTargetAssets.IsEmpty() && bEditorLogVerificationPass
        : FailedAssets.IsEmpty() && MissingTargetAssets.IsEmpty() && BlueprintCompileErrorCount == 0 && OriginalDependencyAssetCount == 0 && bEditorLogVerificationPass;

    ReportObject->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ReportObject->SetNumberField(TEXT("paired_asset_count"), PairedAssetCount);
    ReportObject->SetNumberField(TEXT("missing_target_asset_count"), MissingTargetAssets.Num());
    ReportObject->SetNumberField(TEXT("saved_count"), SavedCount);
    ReportObject->SetStringField(TEXT("asset_save_mode"), TEXT("package_save_no_prompt"));
    ReportObject->SetNumberField(TEXT("remapped_object_count"), RemappedObjectCount);
    ReportObject->SetNumberField(TEXT("archive_object_remap_skip_count"), ArchiveObjectRemapSkipCount);
    ReportObject->SetNumberField(TEXT("path_property_remap_count"), PathPropertyRemapCount);
    ReportObject->SetNumberField(TEXT("user_defined_struct_description_path_remap_count"), UserDefinedStructDescriptionPathRemapCount);
    ReportObject->SetNumberField(TEXT("scs_node_repair_count"), SCSNodeRepairCount);
    ReportObject->SetNumberField(TEXT("widget_tree_repair_count"), WidgetTreeRepairCount);
    ReportObject->SetNumberField(TEXT("generated_class_subobject_repair_count"), GeneratedClassSubobjectRepairCount);
    ReportObject->SetNumberField(TEXT("material_texture_reference_repair_count"), MaterialTextureReferenceRepairCount);
    ReportObject->SetNumberField(TEXT("material_cache_refresh_count"), MaterialCacheRefreshCount);
    ReportObject->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ReportObject->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ReportObject->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ReportObject->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ReportObject->SetNumberField(TEXT("skipped_clean_world_repair_count"), SkippedCleanWorldRepairCount);
    ReportObject->SetNumberField(TEXT("blueprint_graph_reference_remap_count"), BlueprintGraphReferenceRemapCount);
    ReportObject->SetNumberField(TEXT("compiled_blueprint_count"), CompiledBlueprintCount);
    ReportObject->SetNumberField(TEXT("blueprint_compile_error_count"), BlueprintCompileErrorCount);
    ReportObject->SetNumberField(TEXT("original_dependency_asset_count"), OriginalDependencyAssetCount);
    AddEditorLogHealthFields(ReportObject, EditorLogHealth);
    ReportObject->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ReportObject->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ReportObject->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ReportObject->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ReportObject->SetBoolField(TEXT("refresh_blueprint_nodes"), bRefreshBlueprintNodes);
    ReportObject->SetBoolField(TEXT("refresh_failed_blueprint_nodes"), bRefreshFailedBlueprintNodes);
    ReportObject->SetBoolField(TEXT("repair_level_actors"), bRepairLevelActors);
    ReportObject->SetBoolField(TEXT("force_repair_level_actors"), bForceRepairLevelActors);
    AddStringArrayField(ReportObject, TEXT("failed_assets"), FailedAssets);
    AddStringArrayField(ReportObject, TEXT("missing_target_assets"), MissingTargetAssets);
    AddStringArrayField(ReportObject, TEXT("original_dependency_samples"), OriginalDependencySamples);
    ReportObject->SetArrayField(TEXT("assets"), AssetReports);
    if (!FailedAssets.IsEmpty())
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess finished with %d failed assets"), FailedAssets.Num()));
    }
    else if (!MissingTargetAssets.IsEmpty())
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess found %d missing target assets"), MissingTargetAssets.Num()));
    }
    else if (OriginalDependencyAssetCount > 0)
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess finished with %d target assets still depending on the source root"), OriginalDependencyAssetCount));
    }
    else if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
    {
        ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
    }

    FString ReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportJson);
    FJsonSerializer::Serialize(ReportObject.ToSharedRef(), Writer);

    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString ReportFilename = BuildMCPReportFilename(TargetRoot, TEXT("postprocess"));
    const FString ReportPath = FPaths::Combine(ReportDirectory, ReportFilename);
    FFileHelper::SaveStringToFile(ReportJson, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ResultObj->SetStringField(TEXT("source_root"), SourceRoot);
    ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
    ResultObj->SetStringField(TEXT("report_path"), ReportPath);
    ResultObj->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ResultObj->SetNumberField(TEXT("paired_asset_count"), PairedAssetCount);
    ResultObj->SetNumberField(TEXT("missing_target_asset_count"), MissingTargetAssets.Num());
    ResultObj->SetNumberField(TEXT("saved_count"), SavedCount);
    ResultObj->SetStringField(TEXT("asset_save_mode"), TEXT("package_save_no_prompt"));
    ResultObj->SetNumberField(TEXT("remapped_object_count"), RemappedObjectCount);
    ResultObj->SetNumberField(TEXT("archive_object_remap_skip_count"), ArchiveObjectRemapSkipCount);
    ResultObj->SetNumberField(TEXT("path_property_remap_count"), PathPropertyRemapCount);
    ResultObj->SetNumberField(TEXT("user_defined_struct_description_path_remap_count"), UserDefinedStructDescriptionPathRemapCount);
    ResultObj->SetNumberField(TEXT("scs_node_repair_count"), SCSNodeRepairCount);
    ResultObj->SetNumberField(TEXT("widget_tree_repair_count"), WidgetTreeRepairCount);
    ResultObj->SetNumberField(TEXT("generated_class_subobject_repair_count"), GeneratedClassSubobjectRepairCount);
    ResultObj->SetNumberField(TEXT("material_texture_reference_repair_count"), MaterialTextureReferenceRepairCount);
    ResultObj->SetNumberField(TEXT("material_cache_refresh_count"), MaterialCacheRefreshCount);
    ResultObj->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ResultObj->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ResultObj->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ResultObj->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ResultObj->SetNumberField(TEXT("skipped_clean_world_repair_count"), SkippedCleanWorldRepairCount);
    ResultObj->SetNumberField(TEXT("blueprint_graph_reference_remap_count"), BlueprintGraphReferenceRemapCount);
    ResultObj->SetNumberField(TEXT("compiled_blueprint_count"), CompiledBlueprintCount);
    ResultObj->SetNumberField(TEXT("blueprint_compile_error_count"), BlueprintCompileErrorCount);
    ResultObj->SetNumberField(TEXT("original_dependency_asset_count"), OriginalDependencyAssetCount);
    AddEditorLogHealthFields(ResultObj, EditorLogHealth);
    ResultObj->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ResultObj->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ResultObj->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ResultObj->SetBoolField(TEXT("refresh_blueprint_nodes"), bRefreshBlueprintNodes);
    ResultObj->SetBoolField(TEXT("refresh_failed_blueprint_nodes"), bRefreshFailedBlueprintNodes);
    ResultObj->SetBoolField(TEXT("repair_level_actors"), bRepairLevelActors);
    ResultObj->SetBoolField(TEXT("force_repair_level_actors"), bForceRepairLevelActors);
    AddStringArrayField(ResultObj, TEXT("failed_asset_samples"), FailedAssets);
    AddStringArrayField(ResultObj, TEXT("missing_target_asset_samples"), MissingTargetAssets);
    AddStringArrayField(ResultObj, TEXT("original_dependency_samples"), OriginalDependencySamples);
    if (!FailedAssets.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess finished with %d failed assets"), FailedAssets.Num()));
    }
    else if (!MissingTargetAssets.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess found %d missing target assets"), MissingTargetAssets.Num()));
    }
    else if (OriginalDependencyAssetCount > 0)
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess finished with %d target assets still depending on the source root"), OriginalDependencyAssetCount));
    }
    else if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
    {
        ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("Postprocess editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
    }
    return ResultObj;
}



TSharedPtr<FJsonObject> HandleRepairWorldActorInstancesMCP(const TSharedPtr<FJsonObject>& Params)
{
    if (!Check()) return {};

    FString SourceRoot;
    FString TargetRoot;
    FString RootParamError;
    if (!TryGetRequiredContentRootParamForMCP(Params, TEXT("source_root"), SourceRoot, RootParamError) ||
        !TryGetRequiredContentRootParamForMCP(Params, TEXT("target_root"), TargetRoot, RootParamError) ||
        !ValidateContentRootPairForMCP(SourceRoot, TargetRoot, RootParamError))
    {
        return LegacyError(RootParamError);
    }
    const FString Suffix = GetStringParam(Params, TEXT("suffix"), TEXT("_MCP"));
    const FString MapPath = NormalizePackagePathParam(GetStringParam(Params, TEXT("map_path"), FString()));
    const bool bDryRun = GetBoolParam(Params, TEXT("dry_run"), false);
    const bool bSaveMap = GetBoolParam(Params, TEXT("save_map"), true);
    const bool bRemapLevelBlueprint = GetBoolParam(Params, TEXT("remap_level_blueprint"), false);
    const bool bRepairActorClasses = GetBoolParam(Params, TEXT("repair_actor_classes"), true);
    const bool bRepairComponentClasses = GetBoolParam(Params, TEXT("repair_component_classes"), true);
    const bool bRemapActorReferences = GetBoolParam(Params, TEXT("remap_actor_references"), true);
    const bool bRemoveSourceObjectMapKeys = GetBoolParam(Params, TEXT("remove_source_object_map_keys"), true);
    const bool bCheckEditorLogHealth = GetBoolParam(Params, TEXT("check_editor_log_health"), true);
    const bool bFailOnEditorLogIssues = GetBoolParam(Params, TEXT("fail_on_editor_log_issues"), true);
    const bool bSuppressEditorPrompts = GetBoolParam(Params, TEXT("suppress_editor_prompts"), true);
    TGuardValue<bool> UnattendedScriptGuard(GIsRunningUnattendedScript, GIsRunningUnattendedScript || bSuppressEditorPrompts);
    const FEditorLogMarker EditorLogMarker = CaptureEditorLogMarker(bCheckEditorLogHealth);

    if (!FPackageName::IsValidLongPackageName(SourceRoot) || !SourceRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid source_root: %s"), *SourceRoot));
    }

    if (!FPackageName::IsValidLongPackageName(TargetRoot) || !TargetRoot.StartsWith(TEXT("/Game/")))
    {
        return LegacyError(FString::Printf(TEXT("Invalid target_root: %s"), *TargetRoot));
    }

    if (TargetRoot.Equals(SourceRoot) || TargetRoot.StartsWith(SourceRoot + TEXT("/")))
    {
        return LegacyError(TEXT("target_root must not be the source root or a child of source_root"));
    }

    if (MapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(MapPath))
    {
        return LegacyError(FString::Printf(TEXT("Invalid map_path: %s"), *MapPath));
    }

    if (!MapPath.Equals(TargetRoot) && !MapPath.StartsWith(TargetRoot + TEXT("/")))
    {
        return LegacyError(TEXT("map_path must be under target_root"));
    }

    if (!UEditorAssetLibrary::DoesAssetExist(MapPath))
    {
        return LegacyError(FString::Printf(TEXT("Target map does not exist: %s"), *MapPath));
    }

    FAssetRegistryModule& AssetRegistryModule = FModuleManager::LoadModuleChecked<FAssetRegistryModule>(TEXT("AssetRegistry"));
    IAssetRegistry& AssetRegistry = AssetRegistryModule.Get();

    TArray<FString> InitialScanPaths;
    InitialScanPaths.Add(SourceRoot);
    InitialScanPaths.Add(TargetRoot);
    AssetRegistry.ScanPathsSynchronous(InitialScanPaths, true);

    TArray<FAssetData> SourceAssets;
    AssetRegistry.GetAssetsByPath(FName(*SourceRoot), SourceAssets, true);
    SourceAssets.Sort([](const FAssetData& A, const FAssetData& B)
    {
        return A.PackageName.LexicalLess(B.PackageName);
    });

    if (SourceAssets.IsEmpty())
    {
        return LegacyError(FString::Printf(TEXT("No source assets found under %s"), *SourceRoot));
    }

    TMap<UObject*, UObject*> ReplacementMap;
    TArray<TPair<UObject*, UObject*>> SourceTargetPairs;
    TArray<FString> MissingTargetAssets;
    TArray<FString> FailedAssets;
    int32 PairedAssetCount = 0;

    for (const FAssetData& SourceAsset : SourceAssets)
    {
        const FString TargetPath = BuildTargetAssetPath(SourceAsset, SourceRoot, TargetRoot, Suffix);
        if (!UEditorAssetLibrary::DoesAssetExist(TargetPath))
        {
            MissingTargetAssets.Add(FString::Printf(TEXT("%s -> %s"), *SourceAsset.PackageName.ToString(), *TargetPath));
            continue;
        }

        if (!Check()) return {};
        UObject* SourceObject = LoadAssetDataForGD(SourceAsset);
        if (!Check()) return {};
        if (!Check()) return {};
        UObject* TargetObject = LoadAssetForGD(TargetPath);
        if (!Check()) return {};
        if (!SourceObject || !TargetObject)
        {
            FailedAssets.Add(FString::Printf(TEXT("%s -> %s (failed to load source or target asset)"), *SourceAsset.PackageName.ToString(), *TargetPath));
            continue;
        }

        AddReplacementMapping(ReplacementMap, SourceObject, TargetObject);
        SourceTargetPairs.Add(TPair<UObject*, UObject*>(SourceObject, TargetObject));
        ++PairedAssetCount;
    }

    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddBlueprintReplacementMappings(ReplacementMap, Pair.Key, Pair.Value);
    }

    TArray<FPathStringReplacement> PathReplacements;
    for (const TPair<UObject*, UObject*>& Pair : SourceTargetPairs)
    {
        AddObjectPathReplacements(PathReplacements, Pair.Key, Pair.Value);
    }
    PathReplacements.Sort([](const FPathStringReplacement& A, const FPathStringReplacement& B)
    {
        return A.Source.Len() > B.Source.Len();
    });

    UWorld* LoadedWorld = LoadWorldPackageForRepair(MapPath);
    if (!LoadedWorld)
    {
        return LegacyError(FString::Printf(TEXT("Failed to load target map: %s"), *MapPath));
    }

    int32 BeforeSourceActorCount = 0;
    int32 BeforeSourceComponentCount = 0;
    CountWorldSourceClassInstances(LoadedWorld, SourceRoot, BeforeSourceActorCount, BeforeSourceComponentCount);
    const int32 BeforeSourceObjectMapKeyCount = CountSourceObjectKeyMapEntriesFromWorld(LoadedWorld, SourceRoot);

    int32 LevelActorReferenceRemapCount = 0;
    int32 SavedMapCount = 0;
    int32 LevelComponentClassRepairCount = 0;
    int32 LevelActorClassRepairCount = 0;

    if (!bDryRun)
    {
        LevelActorClassRepairCount = RepairLoadedWorldActorInstances(
            LoadedWorld,
            MapPath,
            ReplacementMap,
            PathReplacements,
            SourceRoot,
            LevelActorReferenceRemapCount,
            SavedMapCount,
            LevelComponentClassRepairCount,
            FailedAssets,
            bRemapLevelBlueprint,
            bRepairActorClasses,
            bRepairComponentClasses,
            bRemapActorReferences,
            bRemoveSourceObjectMapKeys,
            bSaveMap);
    }

    int32 AfterSourceActorCount = 0;
    int32 AfterSourceComponentCount = 0;
    CountWorldSourceClassInstances(LoadedWorld, SourceRoot, AfterSourceActorCount, AfterSourceComponentCount);
    const int32 AfterSourceObjectMapKeyCount = CountSourceObjectKeyMapEntriesFromWorld(LoadedWorld, SourceRoot);

    TArray<FString> SourceDependencySamples;
    int32 SourceHardDependencyCount = 0;
    {
        TArray<FString> PathsToScan;
        PathsToScan.Add(FPackageName::GetLongPackagePath(MapPath));
        AssetRegistry.ScanPathsSynchronous(PathsToScan, true);

        TArray<FName> Dependencies;
        AssetRegistry.GetDependencies(FName(*MapPath), Dependencies);
        for (const FName& Dependency : Dependencies)
        {
            const FString DependencyPath = Dependency.ToString();
            if (DependencyPath.Equals(SourceRoot) || DependencyPath.StartsWith(SourceRoot + TEXT("/")))
            {
                ++SourceHardDependencyCount;
                if (SourceDependencySamples.Num() < 50)
                {
                    SourceDependencySamples.Add(FString::Printf(TEXT("%s -> %s"), *MapPath, *DependencyPath));
                }
            }
        }
    }

    const FEditorLogHealthResult EditorLogHealth = ScanEditorLogSince(EditorLogMarker, TargetRoot);
    const bool bEditorLogVerificationPass = !bCheckEditorLogHealth || !bFailOnEditorLogIssues || EditorLogHealth.bPass;
    const bool bVerificationPass = FailedAssets.IsEmpty()
        && MissingTargetAssets.IsEmpty()
        && AfterSourceActorCount == 0
        && AfterSourceComponentCount == 0
        && AfterSourceObjectMapKeyCount == 0
        && SourceHardDependencyCount == 0
        && bEditorLogVerificationPass;

    TSharedPtr<FJsonObject> ReportObject = MakeShared<FJsonObject>();
    ReportObject->SetStringField(TEXT("operation"), TEXT("repair_world_actor_instances_mcp"));
    ReportObject->SetBoolField(TEXT("success"), true);
    ReportObject->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ReportObject->SetStringField(TEXT("source_root"), SourceRoot);
    ReportObject->SetStringField(TEXT("target_root"), TargetRoot);
    ReportObject->SetStringField(TEXT("map_path"), MapPath);
    ReportObject->SetStringField(TEXT("suffix"), Suffix);
    ReportObject->SetBoolField(TEXT("dry_run"), bDryRun);
    ReportObject->SetBoolField(TEXT("save_map"), bSaveMap);
    ReportObject->SetBoolField(TEXT("remap_level_blueprint"), bRemapLevelBlueprint);
    ReportObject->SetBoolField(TEXT("repair_actor_classes"), bRepairActorClasses);
    ReportObject->SetBoolField(TEXT("repair_component_classes"), bRepairComponentClasses);
    ReportObject->SetBoolField(TEXT("remap_actor_references"), bRemapActorReferences);
    ReportObject->SetBoolField(TEXT("remove_source_object_map_keys"), bRemoveSourceObjectMapKeys);
    ReportObject->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ReportObject->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ReportObject->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ReportObject->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ReportObject->SetNumberField(TEXT("paired_asset_count"), PairedAssetCount);
    ReportObject->SetNumberField(TEXT("missing_target_asset_count"), MissingTargetAssets.Num());
    ReportObject->SetNumberField(TEXT("before_source_actor_count"), BeforeSourceActorCount);
    ReportObject->SetNumberField(TEXT("before_source_component_count"), BeforeSourceComponentCount);
    ReportObject->SetNumberField(TEXT("before_source_object_map_key_count"), BeforeSourceObjectMapKeyCount);
    ReportObject->SetNumberField(TEXT("after_source_actor_count"), AfterSourceActorCount);
    ReportObject->SetNumberField(TEXT("after_source_component_count"), AfterSourceComponentCount);
    ReportObject->SetNumberField(TEXT("after_source_object_map_key_count"), AfterSourceObjectMapKeyCount);
    ReportObject->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ReportObject->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ReportObject->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ReportObject->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ReportObject->SetNumberField(TEXT("source_hard_dependency_count"), SourceHardDependencyCount);
    AddEditorLogHealthFields(ReportObject, EditorLogHealth);
    AddStringArrayField(ReportObject, TEXT("failed_assets"), FailedAssets);
    AddStringArrayField(ReportObject, TEXT("missing_target_assets"), MissingTargetAssets);
    AddStringArrayField(ReportObject, TEXT("source_dependency_samples"), SourceDependencySamples);
    if (!bVerificationPass)
    {
        if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
        {
            ReportObject->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("World actor repair editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
        }
        else
        {
            ReportObject->SetStringField(TEXT("verification_error"), TEXT("World actor repair finished with residual source references or failed assets"));
        }
    }

    FString ReportJson;
    const TSharedRef<TJsonWriter<>> Writer = TJsonWriterFactory<>::Create(&ReportJson);
    FJsonSerializer::Serialize(ReportObject.ToSharedRef(), Writer);

    const FString ReportDirectory = FPaths::Combine(FPaths::ProjectSavedDir(), TEXT("MCP"));
    IFileManager::Get().MakeDirectory(*ReportDirectory, true);
    const FString ReportFilename = BuildMCPReportFilename(MapPath, TEXT("world_repair"));
    const FString ReportPath = FPaths::Combine(ReportDirectory, ReportFilename);
    FFileHelper::SaveStringToFile(ReportJson, *ReportPath, FFileHelper::EEncodingOptions::ForceUTF8);

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ResultObj->SetStringField(TEXT("source_root"), SourceRoot);
    ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
    ResultObj->SetStringField(TEXT("map_path"), MapPath);
    ResultObj->SetStringField(TEXT("report_path"), ReportPath);
    ResultObj->SetBoolField(TEXT("check_editor_log_health"), bCheckEditorLogHealth);
    ResultObj->SetBoolField(TEXT("fail_on_editor_log_issues"), bFailOnEditorLogIssues);
    ResultObj->SetBoolField(TEXT("suppress_editor_prompts"), bSuppressEditorPrompts);
    ResultObj->SetNumberField(TEXT("source_asset_count"), SourceAssets.Num());
    ResultObj->SetNumberField(TEXT("paired_asset_count"), PairedAssetCount);
    ResultObj->SetNumberField(TEXT("missing_target_asset_count"), MissingTargetAssets.Num());
    ResultObj->SetNumberField(TEXT("before_source_actor_count"), BeforeSourceActorCount);
    ResultObj->SetNumberField(TEXT("before_source_component_count"), BeforeSourceComponentCount);
    ResultObj->SetNumberField(TEXT("before_source_object_map_key_count"), BeforeSourceObjectMapKeyCount);
    ResultObj->SetNumberField(TEXT("after_source_actor_count"), AfterSourceActorCount);
    ResultObj->SetNumberField(TEXT("after_source_component_count"), AfterSourceComponentCount);
    ResultObj->SetNumberField(TEXT("after_source_object_map_key_count"), AfterSourceObjectMapKeyCount);
    ResultObj->SetNumberField(TEXT("level_actor_reference_remap_count"), LevelActorReferenceRemapCount);
    ResultObj->SetNumberField(TEXT("level_actor_class_repair_count"), LevelActorClassRepairCount);
    ResultObj->SetNumberField(TEXT("level_component_class_repair_count"), LevelComponentClassRepairCount);
    ResultObj->SetNumberField(TEXT("saved_map_count"), SavedMapCount);
    ResultObj->SetNumberField(TEXT("source_hard_dependency_count"), SourceHardDependencyCount);
    AddEditorLogHealthFields(ResultObj, EditorLogHealth);
    AddStringArrayField(ResultObj, TEXT("failed_asset_samples"), FailedAssets);
    AddStringArrayField(ResultObj, TEXT("missing_target_asset_samples"), MissingTargetAssets);
    AddStringArrayField(ResultObj, TEXT("source_dependency_samples"), SourceDependencySamples);
    if (!bVerificationPass)
    {
        if (bCheckEditorLogHealth && bFailOnEditorLogIssues && !EditorLogHealth.bPass)
        {
            ResultObj->SetStringField(TEXT("verification_error"), FString::Printf(TEXT("World actor repair editor log health check found %d issue(s)"), EditorLogHealth.IssueCount));
        }
        else
        {
            ResultObj->SetStringField(TEXT("verification_error"), TEXT("World actor repair finished with residual source references or failed assets"));
        }
    }
    return ResultObj;
}



TSharedPtr<FJsonObject> HandleRunContentValidationPipelineMCP(const TSharedPtr<FJsonObject>& Params)
{
    if (!Check()) return {};

    if (!Params.IsValid())
    {
        return LegacyError(TEXT("Missing parameters"));
    }

    FString SourceRoot;
    FString TargetRoot;
    FString RootParamError;
    if (!TryGetRequiredContentRootParamForMCP(Params, TEXT("source_root"), SourceRoot, RootParamError) ||
        !TryGetRequiredContentRootParamForMCP(Params, TEXT("target_root"), TargetRoot, RootParamError) ||
        !ValidateContentRootPairForMCP(SourceRoot, TargetRoot, RootParamError))
    {
        return LegacyError(RootParamError);
    }

    const FString Suffix = GetStringParam(Params, TEXT("suffix"), TEXT("_MCP"));
    const FString MapPath = NormalizePackagePathParam(GetStringParam(Params, TEXT("map_path"), FString()));
    const bool bContinueOnFailure = GetBoolParam(Params, TEXT("continue_on_failure"), false);
    const bool bRunWorldRepair = GetBoolParam(Params, TEXT("run_world_repair"), !MapPath.IsEmpty());

    if (bRunWorldRepair)
    {
        if (MapPath.IsEmpty() || !FPackageName::IsValidLongPackageName(MapPath))
        {
            return LegacyError(TEXT("run_world_repair requires a valid map_path"));
        }
        if (!MapPath.Equals(TargetRoot) && !MapPath.StartsWith(TargetRoot + TEXT("/")))
        {
            return LegacyError(TEXT("map_path must be under target_root"));
        }
    }

    TArray<TSharedPtr<FJsonValue>> PipelineStepValues;

    TSharedPtr<FJsonObject> RecreateParams = CloneJsonObjectForMCP(Params);
    RecreateParams->SetStringField(TEXT("source_root"), SourceRoot);
    RecreateParams->SetStringField(TEXT("target_root"), TargetRoot);
    RecreateParams->SetStringField(TEXT("suffix"), Suffix);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("allow_fallback_duplicate"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("dry_run"), false);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("overwrite_existing"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("delete_target_root_first"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("remap_references"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("compile_blueprints"), true);
    SetNumberFieldIfMissingForMCP(RecreateParams, TEXT("compile_passes"), 3);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("save_assets"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("repair_level_actors"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("force_repair_level_actors"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("check_editor_log_health"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("fail_on_editor_log_issues"), true);
    SetBoolFieldIfMissingForMCP(RecreateParams, TEXT("suppress_editor_prompts"), true);

    TSharedPtr<FJsonObject> RecreateResult = HandleRecreateContentFolderMCP(RecreateParams);
    if (!RecreateResult) RecreateResult = LegacyError(Current->R.Error.IsEmpty() ? TEXT("Native child returned no result") : Current->R.Error);
    const bool bRecreatePass = PipelineStepResultPassesForMCP(RecreateResult);
    PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
        TEXT("recreate"),
        true,
        RecreateResult)));

    const bool bCanRunPostprocess = (bRecreatePass || bContinueOnFailure) && Current->R.Error.IsEmpty();
    TSharedPtr<FJsonObject> PostprocessResult;
    bool bPostprocessPass = false;
    if (bCanRunPostprocess)
    {
        TSharedPtr<FJsonObject> PostprocessParams = CloneJsonObjectForMCP(Params);
        PostprocessParams->SetStringField(TEXT("source_root"), SourceRoot);
        PostprocessParams->SetStringField(TEXT("target_root"), TargetRoot);
        PostprocessParams->SetStringField(TEXT("suffix"), Suffix);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("dry_run"), false);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("remap_references"), true);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("compile_blueprints"), true);
        SetNumberFieldIfMissingForMCP(PostprocessParams, TEXT("compile_passes"), 3);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("save_assets"), true);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("repair_level_actors"), false);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("check_editor_log_health"), true);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("fail_on_editor_log_issues"), true);
        SetBoolFieldIfMissingForMCP(PostprocessParams, TEXT("suppress_editor_prompts"), true);

        PostprocessResult = HandlePostprocessContentFolderMCP(PostprocessParams);
    if (!PostprocessResult) PostprocessResult = LegacyError(Current->R.Error.IsEmpty() ? TEXT("Native child returned no result") : Current->R.Error);
        bPostprocessPass = PipelineStepResultPassesForMCP(PostprocessResult);
        PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
            TEXT("postprocess"),
            true,
            PostprocessResult)));
    }
    else
    {
        PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
            TEXT("postprocess"),
            false,
            nullptr,
            Current->R.Error.IsEmpty() ? TEXT("recreate_failed") : TEXT("native_health_failure_no_followup"))));
    }

    TSharedPtr<FJsonObject> WorldRepairResult;
    bool bWorldRepairPass = !bRunWorldRepair;
    if (bRunWorldRepair)
    {
        const bool bCanRunWorldRepair = (bPostprocessPass || bContinueOnFailure) && bCanRunPostprocess && Current->R.Error.IsEmpty();
        if (bCanRunWorldRepair)
        {
            TSharedPtr<FJsonObject> WorldRepairParams = CloneJsonObjectForMCP(Params);
            WorldRepairParams->SetStringField(TEXT("source_root"), SourceRoot);
            WorldRepairParams->SetStringField(TEXT("target_root"), TargetRoot);
            WorldRepairParams->SetStringField(TEXT("suffix"), Suffix);
            WorldRepairParams->SetStringField(TEXT("map_path"), MapPath);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("dry_run"), false);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("save_map"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("remap_actor_references"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("repair_actor_classes"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("repair_component_classes"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("remove_source_object_map_keys"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("check_editor_log_health"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("fail_on_editor_log_issues"), true);
            SetBoolFieldIfMissingForMCP(WorldRepairParams, TEXT("suppress_editor_prompts"), true);

            WorldRepairResult = HandleRepairWorldActorInstancesMCP(WorldRepairParams);
    if (!WorldRepairResult) WorldRepairResult = LegacyError(Current->R.Error.IsEmpty() ? TEXT("Native child returned no result") : Current->R.Error);
            bWorldRepairPass = PipelineStepResultPassesForMCP(WorldRepairResult);
            PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
                TEXT("world_repair"),
                true,
                WorldRepairResult)));
        }
        else
        {
            PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
                TEXT("world_repair"),
                false,
                nullptr,
                TEXT("previous_step_failed"))));
        }
    }
    else
    {
        PipelineStepValues.Add(MakeShared<FJsonValueObject>(MakePipelineStepSummaryForMCP(
            TEXT("world_repair"),
            false,
            nullptr,
            TEXT("map_path_not_supplied_or_run_world_repair_false"))));
    }

    const bool bVerificationPass = bRecreatePass && bPostprocessPass && (!bRunWorldRepair || bWorldRepairPass);
    const int32 TotalBlueprintCompileErrors =
        GetJsonIntFieldForMCP(RecreateResult, TEXT("blueprint_compile_error_count"))
        + GetJsonIntFieldForMCP(PostprocessResult, TEXT("blueprint_compile_error_count"));
    const int32 TotalEditorLogIssueCount =
        GetJsonIntFieldForMCP(RecreateResult, TEXT("editor_log_issue_count"))
        + GetJsonIntFieldForMCP(PostprocessResult, TEXT("editor_log_issue_count"))
        + GetJsonIntFieldForMCP(WorldRepairResult, TEXT("editor_log_issue_count"));

    TSharedPtr<FJsonObject> ReportObject = MakeShared<FJsonObject>();
    ReportObject->SetStringField(TEXT("operation"), TEXT("run_content_validation_pipeline_mcp"));
    ReportObject->SetBoolField(TEXT("success"), true);
    ReportObject->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ReportObject->SetStringField(TEXT("source_root"), SourceRoot);
    ReportObject->SetStringField(TEXT("target_root"), TargetRoot);
    ReportObject->SetStringField(TEXT("suffix"), Suffix);
    ReportObject->SetBoolField(TEXT("continue_on_failure"), bContinueOnFailure);
    ReportObject->SetBoolField(TEXT("run_world_repair"), bRunWorldRepair);
    if (!MapPath.IsEmpty())
    {
        ReportObject->SetStringField(TEXT("map_path"), MapPath);
    }
    ReportObject->SetArrayField(TEXT("pipeline_steps"), PipelineStepValues);
    ReportObject->SetNumberField(TEXT("blueprint_compile_error_count"), TotalBlueprintCompileErrors);
    ReportObject->SetNumberField(TEXT("editor_log_issue_count"), TotalEditorLogIssueCount);
    CopyNumberFieldIfPresentForMCP(RecreateResult, ReportObject, TEXT("accepted_fallback_rule_count"), TEXT("accepted_fallback_rule_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ReportObject, TEXT("accepted_fallback_count"), TEXT("accepted_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ReportObject, TEXT("unresolved_fallback_count"), TEXT("unresolved_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ReportObject, TEXT("fresh_create_fallback_count"), TEXT("fresh_create_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ReportObject, TEXT("original_dependency_asset_count"), TEXT("recreate_original_dependency_asset_count"));
    CopyNumberFieldIfPresentForMCP(PostprocessResult, ReportObject, TEXT("original_dependency_asset_count"), TEXT("postprocess_original_dependency_asset_count"));
    CopyNumberFieldIfPresentForMCP(PostprocessResult, ReportObject, TEXT("missing_target_asset_count"), TEXT("postprocess_missing_target_asset_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ReportObject, TEXT("source_hard_dependency_count"), TEXT("world_repair_source_hard_dependency_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ReportObject, TEXT("after_source_actor_count"), TEXT("world_repair_after_source_actor_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ReportObject, TEXT("after_source_component_count"), TEXT("world_repair_after_source_component_count"));
    if (RecreateResult.IsValid())
    {
        ReportObject->SetObjectField(TEXT("recreate_result"), RecreateResult);
    }
    if (PostprocessResult.IsValid())
    {
        ReportObject->SetObjectField(TEXT("postprocess_result"), PostprocessResult);
    }
    if (WorldRepairResult.IsValid())
    {
        ReportObject->SetObjectField(TEXT("world_repair_result"), WorldRepairResult);
    }
    if (!bVerificationPass)
    {
        ReportObject->SetStringField(TEXT("verification_error"), TEXT("Content validation pipeline failed one or more verification steps"));
    }

    const FString ReportPath = SaveJsonReportForMCP(
        ReportObject,
        BuildMCPReportFilename(TargetRoot, TEXT("validation_pipeline")));

    TSharedPtr<FJsonObject> ResultObj = MakeShared<FJsonObject>();
    ResultObj->SetBoolField(TEXT("success"), true);
    ResultObj->SetBoolField(TEXT("verification_pass"), bVerificationPass);
    ResultObj->SetStringField(TEXT("source_root"), SourceRoot);
    ResultObj->SetStringField(TEXT("target_root"), TargetRoot);
    ResultObj->SetStringField(TEXT("suffix"), Suffix);
    ResultObj->SetBoolField(TEXT("continue_on_failure"), bContinueOnFailure);
    ResultObj->SetBoolField(TEXT("run_world_repair"), bRunWorldRepair);
    if (!MapPath.IsEmpty())
    {
        ResultObj->SetStringField(TEXT("map_path"), MapPath);
    }
    ResultObj->SetStringField(TEXT("report_path"), ReportPath);
    ResultObj->SetArrayField(TEXT("pipeline_steps"), PipelineStepValues);
    ResultObj->SetNumberField(TEXT("blueprint_compile_error_count"), TotalBlueprintCompileErrors);
    ResultObj->SetNumberField(TEXT("editor_log_issue_count"), TotalEditorLogIssueCount);
    CopyNumberFieldIfPresentForMCP(RecreateResult, ResultObj, TEXT("accepted_fallback_rule_count"), TEXT("accepted_fallback_rule_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ResultObj, TEXT("accepted_fallback_count"), TEXT("accepted_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ResultObj, TEXT("unresolved_fallback_count"), TEXT("unresolved_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ResultObj, TEXT("fresh_create_fallback_count"), TEXT("fresh_create_fallback_count"));
    CopyNumberFieldIfPresentForMCP(RecreateResult, ResultObj, TEXT("original_dependency_asset_count"), TEXT("recreate_original_dependency_asset_count"));
    CopyNumberFieldIfPresentForMCP(PostprocessResult, ResultObj, TEXT("original_dependency_asset_count"), TEXT("postprocess_original_dependency_asset_count"));
    CopyNumberFieldIfPresentForMCP(PostprocessResult, ResultObj, TEXT("missing_target_asset_count"), TEXT("postprocess_missing_target_asset_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ResultObj, TEXT("source_hard_dependency_count"), TEXT("world_repair_source_hard_dependency_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ResultObj, TEXT("after_source_actor_count"), TEXT("world_repair_after_source_actor_count"));
    CopyNumberFieldIfPresentForMCP(WorldRepairResult, ResultObj, TEXT("after_source_component_count"), TEXT("world_repair_after_source_component_count"));
    if (RecreateResult.IsValid())
    {
        ResultObj->SetObjectField(TEXT("recreate_result"), RecreateResult);
    }
    if (PostprocessResult.IsValid())
    {
        ResultObj->SetObjectField(TEXT("postprocess_result"), PostprocessResult);
    }
    if (WorldRepairResult.IsValid())
    {
        ResultObj->SetObjectField(TEXT("world_repair_result"), WorldRepairResult);
    }
    if (!bVerificationPass)
    {
        ResultObj->SetStringField(TEXT("verification_error"), TEXT("Content validation pipeline failed one or more verification steps"));
    }
    return ResultObj;
}


using FDomainOperation = TSharedPtr<FJsonObject> (*)(const TSharedPtr<FJsonObject>&);
FString RunDedicated(const FString& ParamsJson, const TCHAR* Operation, FDomainOperation DomainOperation)
{
    FWork Work; Work.Operation = Operation;
    if (!Safe()) { Work.Fail(TEXT("unsafe_context"), TEXT("Editor/game thread is unavailable")); return Work.Finish(nullptr); }
    Work.InitialWorld = GEditor->GetEditorWorldContext().World();
    if (Running) { Work.Fail(TEXT("reentrant"), TEXT("A content reconstruction operation is already executing")); return Work.Finish(nullptr); }
    TGuardValue<bool> Busy(Running, true);
    TGuardValue<FWork*> Context(Current, &Work);
    TSharedPtr<FJsonObject> Params;
    if (!Parse(ParamsJson, Params))
    { Work.Fail(TEXT("invalid_params_json"), TEXT("ParamsJson must be a finite, bounded JSON object with unique case-sensitive keys")); return Work.Finish(nullptr); }
    Work.DryRun = GetBoolParam(Params, TEXT("dry_run"), false);
    Work.R.bSaveRequested = FString(Operation).Equals(TEXT("repair_world_actor_instances_mcp"), ESearchCase::CaseSensitive)
        ? GetBoolParam(Params, TEXT("save_map"), true)
        : GetBoolParam(Params, TEXT("save_assets"), true);
    if (FString(Operation).Equals(TEXT("run_content_validation_pipeline_mcp"), ESearchCase::CaseSensitive))
        Work.R.bSaveRequested |= GetBoolParam(Params, TEXT("save_map"), true);
    Work.R.bCompileRequested = FString(Operation).Equals(TEXT("repair_world_actor_instances_mcp"), ESearchCase::CaseSensitive)
        ? GetBoolParam(Params, TEXT("remap_level_blueprint"), false)
        : GetBoolParam(Params, TEXT("compile_blueprints"), true);
    FString Error;
    Work.R.Stage = TEXT("validate_roots");
    if (!TryGetRequiredContentRootParamForMCP(Params, TEXT("source_root"), Work.SourceRoot, Error) ||
        !TryGetRequiredContentRootParamForMCP(Params, TEXT("target_root"), Work.TargetRoot, Error) ||
        !ValidateContentRootPairForMCP(Work.SourceRoot, Work.TargetRoot, Error))
        return Work.Finish(LegacyError(Error));
    if (Under(Work.SourceRoot, Work.TargetRoot) || Under(Work.TargetRoot, Work.SourceRoot))
    { Work.Fail(TEXT("overlapping_roots"), TEXT("Source and target roots must be disjoint in both directions; no source ancestor can be deleted or rewritten")); return Work.Finish(nullptr); }
    if (!Work.AdmitLoadedTargets()) return Work.Finish(nullptr);
    Work.R.Data->SetStringField(TEXT("source_root"), Work.SourceRoot);
    Work.R.Data->SetStringField(TEXT("target_root"), Work.TargetRoot);
    Work.R.Data->SetBoolField(TEXT("external_transaction_active_at_entry"), GEditor->IsTransactionActive());
    Work.R.Data->SetStringField(TEXT("source_preservation_scope"), TEXT("No explicit writer/save/delete is admitted outside target_root or in source_root. Public loaders, compilation/reinstance/serialization may have indirect callbacks; source disk/global purity is not asserted."));
    TSharedPtr<FJsonObject> Legacy = DomainOperation(Params);
    if (!Legacy) Legacy = LegacyError(Work.R.Error.IsEmpty() ? TEXT("Native operation returned no legacy result") : Work.R.Error);
    return Work.Finish(Legacy);
}
}
FString Recreate(const FString& ParamsJson) { return RunDedicated(ParamsJson, TEXT("recreate_content_folder_mcp"), HandleRecreateContentFolderMCP); }
FString Postprocess(const FString& ParamsJson) { return RunDedicated(ParamsJson, TEXT("postprocess_content_folder_mcp"), HandlePostprocessContentFolderMCP); }
FString RepairWorld(const FString& ParamsJson) { return RunDedicated(ParamsJson, TEXT("repair_world_actor_instances_mcp"), HandleRepairWorldActorInstancesMCP); }
FString RunValidationPipeline(const FString& ParamsJson) { return RunDedicated(ParamsJson, TEXT("run_content_validation_pipeline_mcp"), HandleRunContentValidationPipelineMCP); }
}
