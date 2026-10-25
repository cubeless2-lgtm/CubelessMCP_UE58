// UE56 Actor-CDO tick and local SkeletalMeshComponent default contracts.
// Dedicated public Engine authoring routes; no generic property permission bypass.
#include "Compatibility/UE58/CubelessBlueprintDefaultsApi.h"
#include "Compatibility/UE58/CubelessEditorApi.h"
#include "Animation/AnimInstance.h"
#include "Components/SkeletalMeshComponent.h"
#include "EdGraph/EdGraph.h"
#include "EdGraph/EdGraphNode.h"
#include "EdGraph/EdGraphPin.h"
#include "EditorAssetLibrary.h"
#include "Editor.h"
#include "Engine/World.h"
#include "UObject/UObjectHash.h"
#include "UObject/UObjectGlobals.h"
#include "Engine/Blueprint.h"
#include "Engine/BlueprintGeneratedClass.h"
#include "Engine/CollisionProfile.h"
#include "Engine/SCS_Node.h"
#include "Engine/SimpleConstructionScript.h"
#include "Engine/SkeletalMesh.h"
#include "GameFramework/Actor.h"
#include "HAL/FileManager.h"
#include "Kismet2/BlueprintEditorUtils.h"
#include "K2Node_Event.h"
#include "EdGraphSchema_K2.h"
#include "K2Node_FunctionEntry.h"
#include "K2Node_CallFunction.h"
#include "Kismet/BlueprintFunctionLibrary.h"
#include "UObject/MetaData.h"
#include "Misc/FileHelper.h"
#include "Misc/PackageName.h"
#include "Misc/Paths.h"
#include "Misc/SecureHash.h"
#include "PhysicsEngine/PhysicsAsset.h"
#include "PhysicsEngine/BodyInstance.h"
#include "Serialization/JsonSerializer.h"
#include "UObject/Package.h"
#include "UObject/StrongObjectPtr.h"
#include "UObject/UObjectIterator.h"
#include "UObject/UnrealType.h"

namespace Cubeless::UE58::BlueprintDefaults
{
namespace
{
constexpr int32 MaxProperties = 2048, MaxText = 1048576, MaxNodes = 256, MaxGraphs = 64, MaxPins = 16384, MaxPackages = 8192;
constexpr int64 MaxDiskBytes = 64 * 1024 * 1024;
struct FResult
{
    FString Operation, Stage = TEXT("preflight"), Code, Error;
    bool bSuccess = false, bAttempted = false, bKnown = true, bChanged = false, bPost = false;
    bool bCompileRequested = false, bCompileAttempted = false, bSaveRequested = false, bSaveAttempted = false, bSaved = false;
    TOptional<bool> Compiled;
    TSharedRef<FJsonObject> Data = MakeShared<FJsonObject>();
};
FString Text(const TSharedRef<FJsonObject>& J)
{ FString S; FJsonSerializer::Serialize(J, TJsonWriterFactory<>::Create(&S)); return S; }
bool Fail(FResult& R, const TCHAR* Code, const FString& Error, bool Unknown = false)
{ R.Code = Code; R.Error = Error; R.bSuccess = false; R.bPost = false; if (Unknown) { R.bKnown = false; } return false; }
void Count(FResult& R, const TCHAR* Key)
{ R.Data->SetNumberField(Key, R.Data->GetNumberField(Key) + 1); }
void Init(FResult& R, const TCHAR* Operation, bool Compile, bool Save)
{
    R.Operation = Operation; R.bCompileRequested = Compile; R.bSaveRequested = Save;
    for (const TCHAR* Key : { TEXT("modify_calls"), TEXT("mark_blueprint_modified_calls"), TEXT("setter_calls"), TEXT("post_edit_change_calls"), TEXT("full_compile_attempts"), TEXT("save_attempt_count"), TEXT("asset_load_count"), TEXT("postcompile_tick_restore_calls"), TEXT("postcompile_tick_notification_calls"), TEXT("postcompile_mark_package_dirty_calls") }) { R.Data->SetNumberField(Key, 0); }
    for (const TCHAR* Key : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed"), TEXT("runtime_verified"), TEXT("implicit_engine_work_measured"), TEXT("mcp_dependency_written"), TEXT("used_set_all_bodies_simulate_physics") }) { R.Data->SetBoolField(Key, false); }
    R.Data->SetArrayField(TEXT("save_calls"), {}); R.Data->SetArrayField(TEXT("asset_load_calls"), {});
    R.Data->SetStringField(TEXT("dependency_tier"), TEXT("EngineAPI: exact Engine CDO/SCS defaults, public setters, Blueprint editor compiler and EditorAssetLibrary; no ServerRPC or runtime Cubeless class"));
    R.Data->SetStringField(TEXT("effect_scope"), TEXT("loaded Actor Blueprint with an exact current clean Blueprint-parent chain to an Engine native Actor class, without selected live/derived instances; exact local native unregistered SkeletalMeshComponent where selected; public setters/notifications/compile can dispatch implicit animation, render, physics and skeleton callbacks; these are not instrumented or claimed absent"));
    R.Data->SetStringField(TEXT("read_scope"), TEXT("bounded current CDO and local SCS defaults, typed selected values, reflected unselected properties, stored graphs/declarations, exact read-only generated-parent CDO/SCS/source and native root/default state, explicit asset sources, target/parent disk and other package dirty flags"));
    R.Data->SetStringField(TEXT("change_accounting_scope"), TEXT("selected Blueprint authoring after explicit asset resolution; requested source loads/PostLoad and initial wrapped library-CDO effects are separately disclosed, not certified absent by target changed=false"));
    R.Data->SetBoolField(TEXT("atomic_rollback_available"), false); R.Data->SetBoolField(TEXT("auto_retry_safe"), false);
    R.Data->SetBoolField(TEXT("compiler_tick_policy_checked"), false); R.Data->SetBoolField(TEXT("compiler_tick_policy_compatible"), false); R.Data->SetBoolField(TEXT("central_tick_persistence_verified"), false);
    R.Data->SetBoolField(TEXT("postcompile_tick_restore_required"), false); R.Data->SetBoolField(TEXT("postcompile_tick_restore_returned"), false);
    R.Data->SetStringField(TEXT("tick_persistence_scope"), TEXT("conservative immediate-parent CDO compiler eligibility only; no graph insertion, CDO restore hook or compile-on-load override; cold/cook persistence is not certified by current CDO/save readback"));
}
FString Encode(FResult& R)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("success"), R.bSuccess); J->SetStringField(TEXT("operation"), R.Operation); J->SetStringField(TEXT("stage"), R.Stage);
    J->SetBoolField(TEXT("mutation_attempted"), R.bAttempted); J->SetBoolField(TEXT("change_state_known"), R.bKnown); J->SetBoolField(TEXT("outcome_unknown"), !R.bKnown);
    if (R.bKnown) { J->SetBoolField(TEXT("changed"), R.bChanged); } else { J->SetField(TEXT("changed"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("postcondition_passed"), R.bPost); J->SetBoolField(TEXT("compile_requested"), R.bCompileRequested); J->SetBoolField(TEXT("compile_attempted"), R.bCompileAttempted);
    if (R.Compiled.IsSet()) { J->SetBoolField(TEXT("compiled"), R.Compiled.GetValue()); } else { J->SetField(TEXT("compiled"), MakeShared<FJsonValueNull>()); }
    J->SetBoolField(TEXT("save_requested"), R.bSaveRequested); J->SetBoolField(TEXT("save_attempted"), R.bSaveAttempted); J->SetBoolField(TEXT("saved"), R.bSaved);
    J->SetBoolField(TEXT("atomic_rollback_available"), false); J->SetBoolField(TEXT("auto_retry_safe"), false);
    if (!R.Code.IsEmpty()) { J->SetStringField(TEXT("error_code"), R.Code); J->SetStringField(TEXT("error"), R.Error); }
    J->SetObjectField(TEXT("data"), R.Data); return Text(J);
}
bool Loaded(const UObject* O)
{ return IsValid(O) && !O->HasAnyFlags(RF_NeedLoad | RF_NeedPostLoad | RF_NeedPostLoadSubobjects | RF_BeginDestroyed | RF_FinishDestroyed); }
bool CleanText(const FString& S)
{ for (const TCHAR* P : { TEXT("/Script/Cubeless"), TEXT("/Script/UnrealMCP"), TEXT("/Engine/PythonTypes"), TEXT("/Game/UnrealMCP"), TEXT("/CubelessMCP_UE58/"), TEXT("/UnrealMCP/"), TEXT("/MCPUnreal/"), TEXT("/MCP_Unreal/") }) { if (S.Contains(P, ESearchCase::IgnoreCase)) { return false; } } return true; }
bool Name(const FString& S)
{ FText Why; return !S.IsEmpty() && S.Len() < NAME_SIZE && FName::IsValidXName(S, INVALID_OBJECTNAME_CHARACTERS, &Why) && !FName(*S).IsNone(); }
FString Path(FString S)
{ S = FPackageName::ExportTextPathToObjectPath(S).TrimStartAndEnd(); S.TrimQuotesInline(); S.ReplaceInline(TEXT("\\"), TEXT("/")); if (!S.IsEmpty() && !S.Contains(TEXT("."))) { S += TEXT(".") + FPackageName::GetShortName(S); } return S; }
bool AssetPath(const FString& S)
{ const FString P = FPackageName::ObjectPathToPackageName(S); return S.Len() < 1024 && FPackageName::IsValidObjectPath(S) && (P.StartsWith(TEXT("/Game/")) || P.StartsWith(TEXT("/Engine/"))) && !S.Contains(TEXT(":")) && CleanText(S); }
TSharedRef<FJsonObject> Identity(const UObject* O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("valid"), Loaded(O));
    J->SetStringField(TEXT("path"), Loaded(O) ? O->GetPathName() : FString()); J->SetStringField(TEXT("class"), Loaded(O) ? O->GetClass()->GetPathName() : FString());
    J->SetStringField(TEXT("outer"), Loaded(O) && O->GetOuter() ? O->GetOuter()->GetPathName() : FString()); J->SetStringField(TEXT("object_flags"), Loaded(O) ? LexToString(static_cast<uint32>(O->GetFlags())) : FString()); return J;
}
bool NativeAnimReadAdmission();
bool Properties(UStruct* Type, const void* Container, UObject* Owner, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{
    if (!NativeAnimReadAdmission() || !Type || !Container || !Loaded(Owner)) { return false; } int32 Count = 0, Chars = 0;
    for (TFieldIterator<FProperty> It(Type, EFieldIteratorFlags::IncludeSuper); It; ++It)
    {
        FProperty* P = *It; if (Skip.Contains(P->GetFName())) { continue; } if (++Count > MaxProperties || P->ArrayDim < 1 || P->ArrayDim > 64 || !P->GetOwnerStruct()) { return false; }
        auto Row = MakeShared<FJsonObject>(); J->SetObjectField(P->GetName(), Row); Row->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Row->SetStringField(TEXT("class"), P->GetClass()->GetName()); Row->SetStringField(TEXT("flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags())));
        TArray<TSharedPtr<FJsonValue>> Values;
        for (int32 I = 0; I < P->ArrayDim; ++I) { FString S; const void* V = P->ContainerPtrToValuePtr<void>(Container, I); if (!P->ExportText_Direct(S, V, V, Owner, PPF_None) || !NativeAnimReadAdmission() || (Chars += S.Len()) > MaxText || !CleanText(S)) { return false; } Values.Add(MakeShared<FJsonValueString>(S)); }
        Row->SetArrayField(TEXT("values"), Values);
    }
    return true;
}
bool Properties(UObject* O, const TSet<FName>& Skip, TSharedRef<FJsonObject>& J)
{ return Loaded(O) && Properties(O->GetClass(), O, O, Skip, J); }
bool Equal(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (A->Type == EJson::Object) { if (A->AsObject()->Values.Num() != B->AsObject()->Values.Num()) { return false; } for (const auto& P : A->AsObject()->Values) { const auto Q = B->AsObject()->TryGetField(P.Key.ToView()); if (!Equal(P.Value, Q)) { return false; } } return true; }
    if (A->Type == EJson::Array) { const auto& X = A->AsArray(); const auto& Y = B->AsArray(); if (X.Num() != Y.Num()) { return false; } for (int32 I = 0; I < X.Num(); ++I) { if (!Equal(X[I], Y[I])) { return false; } } return true; }
    if (A->Type == EJson::String) { return A->AsString() == B->AsString(); } if (A->Type == EJson::Number) { return A->AsNumber() == B->AsNumber(); } if (A->Type == EJson::Boolean) { return A->AsBool() == B->AsBool(); } return A->Type == EJson::Null;
}
bool Equal(const TSharedRef<FJsonObject>& A, const TSharedRef<FJsonObject>& B)
{ return Equal(MakeShared<FJsonValueObject>(A), MakeShared<FJsonValueObject>(B)); }
bool Native(UClass* C)
{ return Loaded(C) && C->HasAnyClassFlags(CLASS_Native) && !C->HasAnyClassFlags(CLASS_Deprecated | CLASS_NewerVersionExists) && !C->ClassGeneratedBy && C->GetOutermost()->GetName() == TEXT("/Script/Engine") && !C->GetOutermost()->IsDirty() && C->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn); }
bool Sources(bool Skeletal, UClass* Parent, TSharedRef<FJsonObject>& J)
{
    TArray<UClass*> Classes = { AActor::StaticClass(), UBlueprint::StaticClass(), USCS_Node::StaticClass(), USimpleConstructionScript::StaticClass() };
    if (Parent && !Classes.Contains(Parent)) { Classes.Add(Parent); }
    if (Skeletal) { Classes.Add(USkeletalMeshComponent::StaticClass()); Classes.Add(UAnimInstance::StaticClass()); Classes.Add(USkeletalMesh::StaticClass()); Classes.Add(UPhysicsAsset::StaticClass()); }
    for (UClass* C : Classes)
    {
        auto Row = Identity(C); J->SetObjectField(C->GetPathName(), Row); if (!Native(C)) { J->SetStringField(TEXT("failed_class"), C->GetPathName()); return false; } Row->SetStringField(TEXT("class_flags"), LexToString(static_cast<uint32>(C->GetClassFlags())));
        UObject* CDO = C->GetDefaultObject(false); Row->SetBoolField(TEXT("existing_cdo_available"), CDO != nullptr); if (!CDO) { continue; }
        auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("existing_cdo"), Identity(CDO)); Row->SetObjectField(TEXT("existing_cdo_properties"), P);
        if (!Loaded(CDO) || CDO->GetOuter() != C->GetOuter() || CDO->GetClass() != C || !CDO->HasAnyFlags(RF_ClassDefaultObject) || !Properties(CDO, {}, P)) { J->SetStringField(TEXT("failed_class"), C->GetPathName()); return false; }
    }
    return true;
}
bool Disk(const FString& Package, TSharedRef<FJsonObject>& J)
{
    FString F; if (!FPackageName::TryConvertLongPackageNameToFilename(Package, F, FPackageName::GetAssetPackageExtension())) { return false; } F = FPaths::ConvertRelativePathToFull(F);
    const int64 Size = IFileManager::Get().FileSize(*F); J->SetStringField(TEXT("path"), F); J->SetBoolField(TEXT("exists"), Size >= 0); J->SetNumberField(TEXT("bytes"), FMath::Max<int64>(0, Size)); J->SetStringField(TEXT("sha1"), TEXT("")); if (Size < 0) { return true; }
    TArray<uint8> Bytes; if (Size > MaxDiskBytes || !FFileHelper::LoadFileToArray(Bytes, *F) || Bytes.Num() != Size) { return false; } uint8 Hash[FSHA1::DigestSize]; FSHA1::HashBuffer(Bytes.GetData(), Bytes.Num(), Hash); J->SetStringField(TEXT("sha1"), BytesToHex(Hash, FSHA1::DigestSize)); return true;
}
bool Dirty(const FString& Package, TSharedRef<FJsonObject>& J)
{ int32 Count = 0; for (TObjectIterator<UPackage> It; It; ++It) { if (!Loaded(*It) || It->GetName() == Package) { continue; } if (++Count > MaxPackages) { return false; } J->SetBoolField(It->GetName(), It->IsDirty()); } return true; }
// IF: only the newly supported generated-parent scope needs this retained source
// inventory. The existing native-parent authoring route and public ABI are shared.
struct FParentIdentity
{
    TStrongObjectPtr<UObject> Object;
    UObject* Outer = nullptr; UClass* Class = nullptr; UPackage* Package = nullptr; UPackage* Effective = nullptr;
    FString ObjectPath; EObjectFlags Flags = RF_NoFlags;
    explicit FParentIdentity(UObject* O) : Object(O), Outer(O->GetOuter()), Class(O->GetClass()), Package(O->GetOutermost()), Effective(O->GetPackage()), ObjectPath(O->GetPathName()), Flags(O->GetFlags()) {}
};
struct FParentRole
{
    TStrongObjectPtr<UBlueprint> Blueprint;
    TStrongObjectPtr<UClass> Class, Skeleton, Super;
    TStrongObjectPtr<AActor> CDO;
    TStrongObjectPtr<USimpleConstructionScript> SCS;
    UClass* SkeletonSuper = nullptr; EClassFlags ClassFlags = CLASS_None, SkeletonFlags = CLASS_None;
    TArray<USCS_Node*> Nodes;
    TArray<UObject*> Templates; TArray<UClass*> NodeClasses; TArray<FGuid> NodeGuids;
    TArray<UObject*> CdoChildren;
};
// JF: concrete compiled-in Engine animation source, not a generic reference loader.
struct FNativeAnimSource
{
    TStrongObjectPtr<UClass> Class;
    TStrongObjectPtr<UObject> CDO;
    TArray<TUniquePtr<FParentIdentity>> Objects;
    UClass* Super=nullptr;EClassFlags ClassFlags=CLASS_None;uint32 PackageFlags=0;
    bool Ready=false,Reading=false,Rebinding=false;
    TSharedRef<FJsonObject> Fields=MakeShared<FJsonObject>(),Observed=MakeShared<FJsonObject>();
};
struct FContext
{
    TStrongObjectPtr<UBlueprint> Blueprint;
    FString Package, SelectedName;
    USCS_Node* SelectedNode = nullptr;
    USkeletalMeshComponent* Skeletal = nullptr;
    AActor* CDO = nullptr;
    bool bSkeletal = false;
    TArray<TStrongObjectPtr<UObject>> Assets;
    TUniquePtr<FNativeAnimSource> NativeAnim;
    bool GeneratedParents = false;
    TWeakObjectPtr<UEditorEngine> Editor; TWeakObjectPtr<UWorld> World;
    UClass* NativeRoot = nullptr;
    TArray<FParentRole> Parents;
    TArray<TUniquePtr<FParentIdentity>> ParentObjects;
    TArray<TUniquePtr<FParentIdentity>> ChildObjects;
    TStrongObjectPtr<UClass> ExpectedGenerated, ExpectedSkeleton;
    TStrongObjectPtr<AActor> ExpectedCDO;
    TStrongObjectPtr<USimpleConstructionScript> ExpectedSCS;
    TStrongObjectPtr<USCS_Node> ExpectedNode;
    TStrongObjectPtr<USkeletalMeshComponent> ExpectedTemplate;
};
bool CurrentDerivedTarget(const FContext& C,FResult& R);
bool NativeAnimCheck(const FContext& C,FResult& R);
FContext* NativeAnimCurrent=nullptr;FResult* NativeAnimResult=nullptr;
struct FNativeAnimReadScope
{
    FContext* Previous;FResult* PreviousResult;
    FNativeAnimReadScope(FContext& C,FResult& R):Previous(NativeAnimCurrent),PreviousResult(NativeAnimResult){NativeAnimCurrent=&C;NativeAnimResult=&R;}
    ~FNativeAnimReadScope(){NativeAnimCurrent=Previous;NativeAnimResult=PreviousResult;}
};
bool ParentSafe(const FContext& C)
{
    return IsEditorGameThread() && GEditor && GEditor == C.Editor.Get() && !GExitPurge && !IsEngineExitRequested()
        && !IsGarbageCollecting() && !UE::IsSavingPackage() && !IsPlaySessionActive() && C.World.IsValid()
        && GEditor->GetEditorWorldContext().World() == C.World.Get() && !C.World->bIsTearingDown;
}
bool ParentKeep(FContext& C, UObject* O)
{
    if (!Loaded(O)) { return false; }
    for (const auto& I : C.ParentObjects) { if (I->Object.Get() == O) { return true; } }
    if (C.ParentObjects.Num() >= 4096 || !Loaded(O->GetClass()) || !Loaded(O->GetOutermost()) || !Loaded(O->GetPackage())
        || (O->GetOuter() && !Loaded(O->GetOuter())) || !CleanText(O->GetPathName()) || !CleanText(O->GetClass()->GetPathName())) { return false; }
    C.ParentObjects.Add(MakeUnique<FParentIdentity>(O)); return true;
}
bool ParentAdmission(const FContext& C, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentSafe(C)) { return Fail(R, TEXT("blueprint_defaults_parent_context_changed"), TEXT("Captured Editor/world or GT/PIE/GC/exit/save admission changed; no next target SDK or retry."), true); }
    for (const auto& I : C.ParentObjects)
    {
        UObject* O = I->Object.Get();
        if (!Loaded(O) || !Loaded(I->Class) || !Loaded(I->Package) || !Loaded(I->Effective) || (I->Outer && !Loaded(I->Outer))
            || O->GetClass() != I->Class || O->GetOuter() != I->Outer || O->GetOutermost() != I->Package || O->GetPackage() != I->Effective || O->GetPathName() != I->ObjectPath || O->GetFlags() != I->Flags)
        { return Fail(R, TEXT("blueprint_defaults_parent_identity_changed"), TEXT("Captured parent authoring object identity changed; parent replacement is not adopted."), true); }
    }
    if (!Loaded(C.Blueprint.Get()) || C.Parents.IsEmpty() || C.Blueprint->ParentClass != C.Parents[0].Class.Get())
    { return Fail(R, TEXT("blueprint_defaults_parent_chain_changed"), TEXT("Selected Blueprint immediate parent changed."), true); }
    for (const auto& P : C.Parents)
    {
        auto* B = P.Blueprint.Get(); auto* K = P.Class.Get(); auto* S = P.Skeleton.Get();
        if (!Loaded(B) || !Loaded(K) || !Loaded(S) || !Loaded(P.Super.Get()) || !Loaded(P.CDO.Get()) || B->GeneratedClass != K || B->SkeletonGeneratedClass != S
            || B->ParentClass != P.Super.Get() || K->GetSuperClass() != P.Super.Get() || K->ClassGeneratedBy != B || S->ClassGeneratedBy != B
            || S->GetSuperClass() != P.SkeletonSuper || K->GetClassFlags() != P.ClassFlags || S->GetClassFlags() != P.SkeletonFlags
            || K->GetDefaultObject(false) != P.CDO.Get() || P.CDO->GetClass() != K || B->SimpleConstructionScript != P.SCS.Get()
            || B->bBeingCompiled || B->bQueuedForCompilation || B->bIsRegeneratingOnLoad || B->bSuppressStructurallyModified || B->GetOutermost()->IsDirty()
            || (B->Status != BS_UpToDate && B->Status != BS_UpToDateWithWarnings)
            || (P.SCS.IsValid() && P.SCS->GetAllNodes() != P.Nodes))
        { return Fail(R, TEXT("blueprint_defaults_parent_role_changed"), TEXT("Current parent Blueprint/class/CDO/SCS/compile-readiness role changed; no inherited mutation, reinstancing adoption or retry."), true); }
        for (int32 N = 0; N < P.Nodes.Num(); ++N)
        {
            USCS_Node* Node = P.Nodes[N];
            if (Node->ComponentTemplate != P.Templates[N] || Node->ComponentClass != P.NodeClasses[N] || Node->VariableGuid != P.NodeGuids[N])
            { return Fail(R, TEXT("blueprint_defaults_parent_template_changed"), TEXT("Captured parent SCS node/template/class/GUID role changed; no new inherited object is read or adopted."), true); }
        }
    }
    return true;
}
bool ChildIdentity(const FContext& C, const FParentIdentity& I, FResult& R)
{
    UObject* O = I.Object.Get();
    if (!Loaded(O) || !Loaded(I.Class) || !Loaded(I.Package) || !Loaded(I.Effective) || (I.Outer && !Loaded(I.Outer))
        || O->GetClass() != I.Class || O->GetOuter() != I.Outer || O->GetOutermost() != I.Package || O->GetPackage() != I.Effective || O->GetPathName() != I.ObjectPath)
    { R.Data->SetStringField(TEXT("first_child_identity_failure"), I.ObjectPath); return Fail(R, TEXT("blueprint_defaults_child_identity_changed"), TEXT("Captured selected child class/outer/effective package/outermost package/path changed; no next SDK or retry."), true); }
    const EObjectFlags AddedSaveBits = RF_WasLoaded | RF_LoadCompleted;
    const bool SavedLoadFlags = R.bSaveAttempted && R.bSaved && I.Package->GetName() == C.Package && !EnumHasAnyFlags(I.Flags, RF_Transient)
        && O->GetFlags() == (I.Flags | AddedSaveBits);
    if (O->GetFlags() != I.Flags && !SavedLoadFlags)
    { R.Data->SetStringField(TEXT("first_child_identity_failure"), I.ObjectPath); return Fail(R, TEXT("blueprint_defaults_child_flags_changed"), TEXT("Selected child flags changed outside exact successful owned-save added load bits; no flags are cleared or normalized."), true); }
    return true;
}
bool FreezeChildScope(FContext& C, FResult& R)
{
    if (!C.GeneratedParents && !C.NativeAnim) { return true; }
    if (!ParentAdmission(C, R) || (C.NativeAnim && !ParentSafe(C))) { return false; }
    C.ChildObjects.Reset(); C.ExpectedSCS.Reset(C.Blueprint->SimpleConstructionScript);
    const TArray<UObject*> Selected = { C.Blueprint.Get(), C.ExpectedGenerated.Get(), C.ExpectedSkeleton.Get(), C.ExpectedCDO.Get(), C.ExpectedSCS.Get(), C.ExpectedNode.Get(), C.ExpectedTemplate.Get() };
    for (UObject* O : Selected)
    {
        if (!O) { continue; }
        if (!Loaded(O) || !Loaded(O->GetClass()) || !Loaded(O->GetOutermost()) || !Loaded(O->GetPackage()) || (O->GetOuter() && !Loaded(O->GetOuter())) || O->GetOutermost()->GetName() != C.Package)
        { return Fail(R, TEXT("blueprint_defaults_child_capture_incomplete"), TEXT("Exact bounded selected child identity capture failed."), R.bAttempted); }
        C.ChildObjects.Add(MakeUnique<FParentIdentity>(O));
    }
    return true;
}
bool DerivedBlueprintAsset(const FContext& C, FResult& R)
{
    if (!C.GeneratedParents && !C.NativeAnim) { return true; }
    if (!ParentAdmission(C, R) || !NativeAnimCheck(C,R) || C.ChildObjects.IsEmpty()) { return false; }
    // A successful compiler may replace only child class/CDO/template roles.
    // The selected Blueprint asset itself may never be renamed/reoutered.
    return ChildIdentity(C, *C.ChildObjects[0], R);
}
bool CurrentDerivedTarget(const FContext& C, FResult& R)
{
    if (!C.GeneratedParents && !C.NativeAnim) { return true; }
    if (!ParentAdmission(C, R) || !NativeAnimCheck(C,R)) { return false; }
    if(C.NativeAnim && C.NativeAnim->Rebinding){return !C.ChildObjects.IsEmpty() && ChildIdentity(C,*C.ChildObjects[0],R);}
    for (const auto& I : C.ChildObjects) { if (!ChildIdentity(C, *I, R)) { return false; } }
    UBlueprint* B = C.Blueprint.Get();
    if (B->SimpleConstructionScript != C.ExpectedSCS.Get() || B->GeneratedClass != C.ExpectedGenerated.Get() || B->SkeletonGeneratedClass != C.ExpectedSkeleton.Get() || !Loaded(C.ExpectedGenerated.Get()) || !Loaded(C.ExpectedSkeleton.Get())
        || B->GeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != B->ParentClass || B->GeneratedClass->GetDefaultObject(false) != C.ExpectedCDO.Get()
        || !Loaded(C.CDO) || C.CDO != C.ExpectedCDO.Get() || C.CDO->GetClass() != B->GeneratedClass || !C.CDO->HasAnyFlags(RF_ClassDefaultObject)
        || (C.bSkeletal && (B->SimpleConstructionScript != C.ExpectedSCS.Get() || !Loaded(C.ExpectedSCS.Get()) || !Loaded(C.ExpectedNode.Get()) || !Loaded(C.ExpectedTemplate.Get())
            || !C.ExpectedSCS->GetAllNodes().Contains(C.ExpectedNode.Get()) || C.ExpectedNode->GetOuter() != C.ExpectedSCS.Get() || C.ExpectedNode->ComponentTemplate != C.ExpectedTemplate.Get()
            || C.ExpectedTemplate->GetOuter() != B->GeneratedClass || C.Skeletal != C.ExpectedTemplate.Get())))
    { return Fail(R, TEXT("blueprint_defaults_child_role_changed"), TEXT("Current child generated class/CDO or selected own local SCS template changed across a callback; no stale template use or automatic re-adoption."), true); }
    return true;
}
bool NativeAnimReadAdmission()
{
    if(!NativeAnimCurrent||!NativeAnimCurrent->NativeAnim){return true;}
    auto& Source=*NativeAnimCurrent->NativeAnim;TGuardValue<bool> Reading(Source.Reading,true);
    return NativeAnimResult&&CurrentDerivedTarget(*NativeAnimCurrent,*NativeAnimResult);
}
bool NativeAnimCheck(const FContext& C,FResult& R)
{
    if(!C.NativeAnim){return true;}auto& S=*C.NativeAnim;
    if(!ParentSafe(C)){return Fail(R,TEXT("blueprint_native_anim_context_changed"),TEXT("Captured Editor/world/GT/PIE/GC/save/exit changed; no next getter/writer/retry."),true);}
    if(!Loaded(C.Blueprint.Get())||(!C.GeneratedParents&&C.Blueprint->ParentClass!=C.NativeRoot)){return Fail(R,TEXT("blueprint_native_anim_target_parent_changed"),TEXT("Selected Blueprint/parent changed."),true);}
    for(const auto& I:S.Objects){UObject* O=I->Object.Get();if(!Loaded(O)||!Loaded(I->Class)||!Loaded(I->Package)||!Loaded(I->Effective)||(I->Outer&&!Loaded(I->Outer))
        ||O->GetClass()!=I->Class||O->GetOuter()!=I->Outer||O->GetOutermost()!=I->Package||O->GetPackage()!=I->Effective||O->GetFlags()!=I->Flags||!O->GetPathName().Equals(I->ObjectPath,ESearchCase::CaseSensitive))
        {return Fail(R,TEXT("blueprint_native_anim_source_identity_changed"),TEXT("Native class/CDO/package identity changed; stale source is not observed."),true);}}
    if(!S.Class.IsValid()){return true;}auto* K=S.Class.Get();auto* D=S.CDO.Get();
    if(!Native(K)||K->GetClass()!=UClass::StaticClass()||K->GetSuperClass()!=S.Super||K->GetClassFlags()!=S.ClassFlags||K->GetPackage()->GetPackageFlags()!=S.PackageFlags
        ||K->GetDefaultObject(false)!=D||!Loaded(D)||D->GetClass()!=K||D->GetOuter()!=K->GetOuter()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))
        {return Fail(R,TEXT("blueprint_native_anim_source_role_changed"),TEXT("Current compiled-in Engine class/existing CDO/default/clean package role changed."),true);}
    if(!S.Ready||S.Reading){return true;}
    TGuardValue<bool> Reading(S.Reading,true);auto Now=MakeShared<FJsonObject>();auto ClassFields=MakeShared<FJsonObject>(),DefaultFields=MakeShared<FJsonObject>();
    if(!Properties(K,{},ClassFields)||!Properties(D,{},DefaultFields)||!NativeAnimReadAdmission()){return false;}
    Now->SetObjectField(TEXT("class_properties"),ClassFields);Now->SetObjectField(TEXT("cdo_properties"),DefaultFields);S.Observed=Now;
    if(!Text(S.Fields).Equals(Text(Now),ESearchCase::CaseSensitive)){return Fail(R,TEXT("blueprint_native_anim_source_fields_changed"),TEXT("Readonly native class/CDO fields changed; no next target SDK/save."),true);}
    return true;
}
bool FreezeNativeAnimTarget(FContext& C,FResult& R)
{
    if(!C.NativeAnim){return true;}if(!ParentSafe(C)){return Fail(R,TEXT("blueprint_native_anim_context_required"),TEXT("Idle captured Editor/world required for native animation input."),R.bAttempted);}
    auto* B=C.Blueprint.Get();if(!Loaded(B)||!Loaded(C.CDO)||!Loaded(C.SelectedNode)||!Loaded(C.Skeletal)){return Fail(R,TEXT("blueprint_native_anim_target_unavailable"),TEXT("Current own selected Blueprint/CDO/SCS/template required."),R.bAttempted);}
    C.ExpectedGenerated.Reset(B->GeneratedClass);C.ExpectedSkeleton.Reset(B->SkeletonGeneratedClass);C.ExpectedCDO.Reset(C.CDO);
    C.ExpectedSCS.Reset(B->SimpleConstructionScript);C.ExpectedNode.Reset(C.SelectedNode);C.ExpectedTemplate.Reset(C.Skeletal);
    return FreezeChildScope(C,R)&&CurrentDerivedTarget(C,R);
}
bool BindNativeAnim(FContext& C,UClass* K,FResult& R)
{
    if(!C.NativeAnim||!CurrentDerivedTarget(C,R)||!Native(K)||K->GetClass()!=UClass::StaticClass()||!K->IsChildOf(UAnimInstance::StaticClass())
        ||K->HasAnyClassFlags(CLASS_Abstract)||K->GetOuter()!=K->GetPackage()||K->GetPackage()!=FindPackage(nullptr,TEXT("/Script/Engine")))
        {return Fail(R,TEXT("blueprint_native_anim_class_unsupported"),TEXT("Exact nonabstract compiled-in Engine native AnimInstance class required; CLASS_Transient is an instance policy, not a refusal."));}
    auto& S=*C.NativeAnim;S.Class.Reset(K);UObject* D=K->GetDefaultObject(false);S.CDO.Reset(D);S.Super=K->GetSuperClass();S.ClassFlags=K->GetClassFlags();S.PackageFlags=K->GetPackage()->GetPackageFlags();
    if(!Loaded(D)||D->GetClass()!=K||D->GetOuter()!=K->GetOuter()||!D->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!Loaded(S.Super))
        {return Fail(R,TEXT("blueprint_native_anim_cdo_unavailable"),TEXT("An existing exact native CDO is required; no explicit CDO or animation instance creation."));}
    for(UObject* O:{static_cast<UObject*>(K),D,static_cast<UObject*>(K->GetPackage()),static_cast<UObject*>(S.Super)})
    {if(!Loaded(O)||!Loaded(O->GetClass())||!Loaded(O->GetOutermost())||!Loaded(O->GetPackage())||(O->GetOuter()&&!Loaded(O->GetOuter()))){return Fail(R,TEXT("blueprint_native_anim_capture_failed"),TEXT("Native source identity is not readable."),true);}S.Objects.Add(MakeUnique<FParentIdentity>(O));}
    auto ClassFields=MakeShared<FJsonObject>(),DefaultFields=MakeShared<FJsonObject>();
    {TGuardValue<bool> Reading(S.Reading,true);if(!Properties(K,{},ClassFields)||!Properties(D,{},DefaultFields)||!NativeAnimReadAdmission()){return false;}}
    S.Fields->SetObjectField(TEXT("class_properties"),ClassFields);S.Fields->SetObjectField(TEXT("cdo_properties"),DefaultFields);S.Observed=S.Fields;S.Ready=true;
    auto Proof=MakeShared<FJsonObject>();Proof->SetStringField(TEXT("class_path"),K->GetPathName());Proof->SetStringField(TEXT("kind"),TEXT("exact_compiled_in_Engine_native_AnimInstance"));
    Proof->SetBoolField(TEXT("source_uasset_required"),false);Proof->SetBoolField(TEXT("explicit_source_compile"),false);Proof->SetBoolField(TEXT("explicit_cdo_or_instance_creation"),false);
    Proof->SetBoolField(TEXT("CLASS_Transient_present"),K->HasAnyClassFlags(CLASS_Transient));Proof->SetStringField(TEXT("scope"),TEXT("Current native class/CDO identity, classflags, reflected fields and compiled-in package flags/dirty; not full reflection/replication-cache/global SDK purity or runtime animation proof."));
    R.Data->SetObjectField(TEXT("native_anim_source"),Proof);return CurrentDerivedTarget(C,R);
}
bool PrepareParents(FContext& C, FResult& R)
{
    UClass* Parent = C.Blueprint->ParentClass.Get(); C.NativeRoot = Parent;
    if (Native(Parent)) { return true; }
    C.Editor = GEditor; C.World = GEditor ? GEditor->GetEditorWorldContext().World() : nullptr;
    if (!ParentSafe(C)) { return Fail(R, TEXT("blueprint_defaults_parent_editor_context_required"), TEXT("Safe captured Editor/world required for a Blueprint-parent input.")); }
    TSet<UClass*> Seen;
    for (int32 Depth = 0; !Native(Parent); ++Depth)
    {
        if (Depth >= 32 || !Loaded(Parent) || Parent->GetClass() != UBlueprintGeneratedClass::StaticClass() || !Parent->IsChildOf(AActor::StaticClass()) || Seen.Contains(Parent)
            || Parent->HasAnyClassFlags(CLASS_Native | CLASS_Deprecated | CLASS_NewerVersionExists) || !CleanText(Parent->GetPathName()))
        { return Fail(R, TEXT("blueprint_defaults_parent_class_unsupported"), TEXT("At most 32 exact current runtime Actor Blueprint parent classes ending at an Engine native Actor class are supported.")); }
        Seen.Add(Parent); UBlueprint* B = Cast<UBlueprint>(Parent->ClassGeneratedBy);
        if (!Loaded(B) || B == C.Blueprint.Get() || B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GeneratedClass != Parent
            || !Loaded(B->SkeletonGeneratedClass) || B->SkeletonGeneratedClass->ClassGeneratedBy != B || !Loaded(B->ParentClass) || Parent->GetSuperClass() != B->ParentClass
            || B->GetOuter() != B->GetOutermost() || Parent->GetOuter() != B->GetOutermost() || B->SkeletonGeneratedClass->GetOutermost() != B->GetOutermost()
            || (!B->GetOutermost()->GetName().StartsWith(TEXT("/Game/")) && !B->GetOutermost()->GetName().StartsWith(TEXT("/Engine/"))) || B->GetOutermost()->IsDirty()
            || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor) || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
            || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || (B->Status != BS_UpToDate && B->Status != BS_UpToDateWithWarnings))
        { return Fail(R, TEXT("blueprint_defaults_parent_blueprint_unsupported"), TEXT("Loaded clean completed current normal Game/Engine Blueprint parent required; no parent load, compile, dirty reset or reparenting.")); }
        AActor* D = Cast<AActor>(Parent->GetDefaultObject(false));
        if (!Loaded(D) || D->GetClass() != Parent || D->GetOuter() != Parent->GetOuter() || !D->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject))
        { return Fail(R, TEXT("blueprint_defaults_parent_cdo_unsupported"), TEXT("Existing exact parent Actor CDO required; no CDO allocation or parent modification.")); }
        FParentRole P; P.Blueprint.Reset(B); P.Class.Reset(Parent); P.Skeleton.Reset(B->SkeletonGeneratedClass); P.Super.Reset(B->ParentClass); P.CDO.Reset(D); P.SCS.Reset(B->SimpleConstructionScript); P.SkeletonSuper = B->SkeletonGeneratedClass->GetSuperClass(); P.ClassFlags = Parent->GetClassFlags(); P.SkeletonFlags = B->SkeletonGeneratedClass->GetClassFlags();
        if (!ParentKeep(C, B) || !ParentKeep(C, Parent) || !ParentKeep(C, B->SkeletonGeneratedClass) || !ParentKeep(C, B->ParentClass) || !ParentKeep(C, P.SkeletonSuper) || !ParentKeep(C, D)) { return Fail(R, TEXT("blueprint_defaults_parent_capture_incomplete"), TEXT("Bounded parent identity capture failed before target mutation.")); }
        GetObjectsWithOuter(D, P.CdoChildren, EGetObjectsFlags::IncludeNestedObjects);
        if (P.CdoChildren.Num() > MaxNodes) { return Fail(R, TEXT("blueprint_defaults_parent_cdo_bound"), TEXT("Bounded parent CDO default-subobject inventory required.")); }
        for (UObject* O : P.CdoChildren) { if (!Loaded(O) || !O->IsIn(D) || O->GetOutermost() != B->GetOutermost() || !ParentKeep(C, O)) { return Fail(R, TEXT("blueprint_defaults_parent_cdo_subobject_unsupported"), TEXT("Exact parent CDO-owned default subtree required.")); } }
        if (P.SCS.IsValid())
        {
            if (!ParentKeep(C, P.SCS.Get()) || P.SCS->GetOuter() != Parent || P.SCS->GetAllNodes().Num() > MaxNodes) { return Fail(R, TEXT("blueprint_defaults_parent_scs_unsupported"), TEXT("Bounded own parent SCS required.")); }
            P.Nodes = P.SCS->GetAllNodes();
            for (USCS_Node* N : P.Nodes) { if (!Loaded(N) || N->GetOuter() != P.SCS.Get() || !N->VariableGuid.IsValid() || !Native(N->ComponentClass) || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetOuter() != Parent || N->ComponentTemplate->GetClass() != N->ComponentClass || !ParentKeep(C, N) || !ParentKeep(C, N->ComponentTemplate)) { return Fail(R, TEXT("blueprint_defaults_parent_scs_unsupported"), TEXT("Parent local SCS identities must remain exact and read-only; inherited template editing is not enabled.")); } P.Templates.Add(N->ComponentTemplate); P.NodeClasses.Add(N->ComponentClass); P.NodeGuids.Add(N->VariableGuid); }
        }
        TArray<UEdGraph*> GraphList; B->GetAllGraphs(GraphList);
        if (GraphList.Num() > MaxGraphs) { return Fail(R, TEXT("blueprint_defaults_parent_graph_bound"), TEXT("Bounded parent source graphs required.")); }
        int32 NodeCount = 0;
        for (UEdGraph* G : GraphList) { if (!Loaded(G) || G->GetOutermost() != B->GetOutermost() || !ParentKeep(C, G)) { return Fail(R, TEXT("blueprint_defaults_parent_graph_unsupported"), TEXT("Exact parent-owned source graph required.")); } for (const auto& N : G->Nodes) { if (++NodeCount > 4096 || !ParentKeep(C, N.Get())) { return Fail(R, TEXT("blueprint_defaults_parent_graph_unsupported"), TEXT("Exact bounded parent source nodes required.")); } } }
        C.Parents.Add(MoveTemp(P)); Parent = B->ParentClass.Get();
    }
    if (!Parent || !Parent->IsChildOf(AActor::StaticClass())) { return Fail(R, TEXT("blueprint_defaults_parent_native_root_unsupported"), TEXT("Current parent chain must end at the existing native Engine Actor policy.")); }
    C.NativeRoot = Parent; C.GeneratedParents = true;
    R.Data->SetNumberField(TEXT("generated_parent_count"), C.Parents.Num()); R.Data->SetStringField(TEXT("parent_authoring_scope"), TEXT("Exact current clean Blueprint parents are read-only; only selected child CDO/own local SCS is mutable; no inherited component or parent-default override is edited."));
    return ParentAdmission(C, R);
}
bool Prepare(const FString& BlueprintPath, const FString& ComponentName, bool Skeletal, FContext& C, FResult& R)
{
    if (!IsEditorGameThread() || IsPlaySessionActive()) { return Fail(R, TEXT("blueprint_defaults_editor_context_required"), TEXT("Editor game thread without PIE/SIE required.")); }
    const FString P = Path(BlueprintPath); C.Package = FPackageName::ObjectPathToPackageName(P); C.SelectedName = ComponentName; C.bSkeletal = Skeletal;
    if (!AssetPath(P) || !C.Package.StartsWith(TEXT("/Game/")) || (Skeletal && !Name(ComponentName))) { return Fail(R, TEXT("blueprint_defaults_inputs_invalid"), TEXT("Mutable Game Blueprint and bounded exact local component selector required.")); }
    C.Blueprint.Reset(FindObject<UBlueprint>(nullptr, *P)); UBlueprint* B = C.Blueprint.Get(); R.Data->SetStringField(TEXT("blueprint_name"), BlueprintPath); R.Data->SetStringField(TEXT("asset_path"), P);
    if (!Loaded(B) || B->GetClass() != UBlueprint::StaticClass() || B->BlueprintType != BPTYPE_Normal || B->GetOuter() != B->GetOutermost() || B->HasAnyFlags(RF_Transient | RF_ClassDefaultObject | RF_ArchetypeObject)
        || B->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn | PKG_EditorOnly | PKG_PlayInEditor) || !Loaded(B->ParentClass) || !B->ParentClass->IsChildOf(AActor::StaticClass())
        || !Loaded(B->GeneratedClass) || !Loaded(B->SkeletonGeneratedClass) || B->GeneratedClass->ClassGeneratedBy != B || B->SkeletonGeneratedClass->ClassGeneratedBy != B || B->GeneratedClass->GetSuperClass() != B->ParentClass
        || B->GeneratedClass->GetOutermost() != B->GetOutermost() || B->SkeletonGeneratedClass->GetOutermost() != B->GetOutermost() || B->bBeingCompiled || B->bQueuedForCompilation || B->bSuppressStructurallyModified || B->bIsRegeneratingOnLoad || B->Status == BS_Error || B->Status == BS_BeingCreated)
    { return Fail(R, TEXT("blueprint_defaults_blueprint_unsupported"), TEXT("Loaded normal current Actor Blueprint required; only verified current Blueprint-parent chains to Engine native Actor classes are supported.")); }
    if (!PrepareParents(C, R)) { return false; }
    C.CDO = Cast<AActor>(B->GeneratedClass->GetDefaultObject(false)); if (!Loaded(C.CDO) || C.CDO->GetClass() != B->GeneratedClass || C.CDO->GetOuter() != B->GeneratedClass->GetOuter() || !C.CDO->HasAnyFlags(RF_ClassDefaultObject)) { return Fail(R, TEXT("blueprint_defaults_cdo_unsupported"), TEXT("Existing exact generated Actor CDO required; no CDO prewarming.")); }
    for (TObjectIterator<AActor> It; It; ++It) { if (Loaded(*It) && !It->IsTemplate() && It->IsA(B->GeneratedClass)) { return Fail(R, TEXT("blueprint_defaults_live_instance_unsupported"), TEXT("Loaded live selected Actor instance is outside the finite template compile scope.")); } }
    for (TObjectIterator<UBlueprintGeneratedClass> It; It; ++It) { if (Loaded(*It) && *It != B->GeneratedClass && *It != B->SkeletonGeneratedClass && It->IsChildOf(B->GeneratedClass)) { return Fail(R, TEXT("blueprint_defaults_derived_class_unsupported"), TEXT("Loaded derived Blueprint classes are outside finite compile propagation proof.")); } }
    if (C.GeneratedParents) { C.ExpectedGenerated.Reset(B->GeneratedClass); C.ExpectedSkeleton.Reset(B->SkeletonGeneratedClass); C.ExpectedCDO.Reset(C.CDO); }
    if (!Skeletal) { return FreezeChildScope(C, R) && CurrentDerivedTarget(C, R); }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); if (!Loaded(SCS) || SCS->GetOuter() != B->GeneratedClass || SCS->GetAllNodes().Num() > MaxNodes) { return Fail(R, TEXT("blueprint_defaults_scs_unsupported"), TEXT("Finite local SCS owned by current generated class required.")); }
    TSet<USCS_Node*> Seen;
    for (USCS_Node* N : SCS->GetAllNodes())
    {
        if (!Loaded(N) || N->GetOuter() != SCS || Seen.Contains(N) || !N->VariableGuid.IsValid() || !Native(N->ComponentClass) || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetClass() != N->ComponentClass || N->ComponentTemplate->GetOuter() != B->GeneratedClass || !N->ComponentTemplate->HasAnyFlags(RF_ArchetypeObject)) { return Fail(R, TEXT("blueprint_defaults_scs_ownership_unsupported"), TEXT("Exact own local native node/GUID/template ownership required.")); }
        Seen.Add(N); if (N->GetVariableName().ToString() == ComponentName) { C.SelectedNode = N; }
    }
    C.Skeletal = C.SelectedNode ? Cast<USkeletalMeshComponent>(C.SelectedNode->ComponentTemplate) : nullptr;
    if (!Loaded(C.Skeletal) || C.Skeletal->GetClass() != USkeletalMeshComponent::StaticClass() || C.Skeletal->IsRegistered() || C.Skeletal->IsPreRegistering() || C.Skeletal->GetOwner() || C.Skeletal->GetWorld()) { return Fail(R, TEXT("blueprint_defaults_skeletal_component_unsupported"), TEXT("Exact named own native unregistered/non-preregistering SkeletalMeshComponent template without owner/world required; inherited/custom variants remain incomplete.")); }
    if (C.GeneratedParents) { C.ExpectedSCS.Reset(SCS); C.ExpectedNode.Reset(C.SelectedNode); C.ExpectedTemplate.Reset(C.Skeletal); }
    return FreezeChildScope(C, R) && CurrentDerivedTarget(C, R);
}
TSharedRef<FJsonObject> Tick(const AActor* A)
{
    auto J = MakeShared<FJsonObject>(); const FActorTickFunction& T = A->PrimaryActorTick;
    J->SetBoolField(TEXT("can_ever_tick"), T.bCanEverTick); J->SetBoolField(TEXT("start_with_tick_enabled"), T.bStartWithTickEnabled); J->SetBoolField(TEXT("allow_tick_on_dedicated_server"), T.bAllowTickOnDedicatedServer); J->SetBoolField(TEXT("tick_even_when_paused"), T.bTickEvenWhenPaused); J->SetNumberField(TEXT("tick_interval"), T.TickInterval); J->SetNumberField(TEXT("tick_hz"), T.TickInterval > 0 ? 1.0f / T.TickInterval : 0.0f); return J;
}
FString ObjectPath(const UObject* O) { return O ? O->GetPathName() : FString(); }
bool SkeletalValues(USkeletalMeshComponent* S, TSharedRef<FJsonObject>& J)
{
    if (!Loaded(S) || S->GetClass() != USkeletalMeshComponent::StaticClass()) { return false; }
    auto* Override = FindFProperty<FObjectPropertyBase>(S->GetClass(), TEXT("PhysicsAssetOverride")); if (!Override || Override->ArrayDim != 1 || Override->PropertyClass != UPhysicsAsset::StaticClass() || !Override->HasAnyPropertyFlags(CPF_Edit)) { return false; }
    J->SetStringField(TEXT("skeletal_mesh"), ObjectPath(S->GetSkeletalMeshAsset())); J->SetStringField(TEXT("anim_class"), ObjectPath(S->GetAnimClass())); J->SetNumberField(TEXT("animation_mode"), static_cast<uint8>(S->GetAnimationMode()));
    J->SetStringField(TEXT("physics_asset"), ObjectPath(S->GetPhysicsAsset())); J->SetStringField(TEXT("physics_asset_override"), ObjectPath(Override->GetObjectPropertyValue_InContainer(S))); J->SetBoolField(TEXT("simulate_physics"), S->BodyInstance.bSimulatePhysics); J->SetBoolField(TEXT("blend_physics"), S->bBlendPhysics); J->SetBoolField(TEXT("enable_gravity"), S->IsGravityEnabled());
    J->SetNumberField(TEXT("collision_enabled_value"), static_cast<uint8>(S->GetCollisionEnabled())); TArray<TSharedPtr<FJsonValue>> Responses;
    for (int32 I = 0; I < static_cast<int32>(ECC_MAX); ++I) { Responses.Add(MakeShared<FJsonValueNumber>(static_cast<uint8>(S->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(I))))); }
    J->SetArrayField(TEXT("collision_response_values"), Responses); J->SetNumberField(TEXT("visibility_based_anim_tick_option_value"), static_cast<uint8>(S->VisibilityBasedAnimTickOption)); return true;
}
bool Graphs(UBlueprint* B, TSharedRef<FJsonObject>& J)
{
    TArray<UEdGraph*> All; B->GetAllGraphs(All); if (All.Num() > MaxGraphs) { return false; }
    TSet<UEdGraph*> Seen; int32 Nodes = 0, Pins = 0; TArray<TSharedPtr<FJsonValue>> Rows; J->SetArrayField(TEXT("graphs"), Rows);
    for (UEdGraph* G : All)
    {
        if (!Loaded(G) || !CleanText(G->GetClass()->GetPathName()) || G->GetOutermost() != B->GetOutermost() || Seen.Contains(G) || !G->GraphGuid.IsValid()) { return false; } Seen.Add(G);
        auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("identity"), Identity(G)); Row->SetStringField(TEXT("guid"), G->GraphGuid.ToString()); Rows.Add(MakeShared<FJsonValueObject>(Row)); J->SetArrayField(TEXT("graphs"), Rows);
        TSet<UEdGraphNode*> Owned; TSet<const UEdGraphPin*> OwnedPins;
        for (UEdGraphNode* N : G->Nodes)
        {
            if (!Loaded(N) || !CleanText(N->GetClass()->GetPathName()) || N->GetOuter() != G || Owned.Contains(N) || !N->NodeGuid.IsValid() || ++Nodes > 4096) { return false; } Owned.Add(N);
            for (const UEdGraphPin* P : N->Pins) { if (!P || P->GetOwningNodeUnchecked() != N || OwnedPins.Contains(P) || !P->PinId.IsValid() || ++Pins > MaxPins) { return false; } OwnedPins.Add(P); }
        }
        TArray<TSharedPtr<FJsonValue>> NodeRows;
        for (UEdGraphNode* N : G->Nodes)
        {
            auto NR = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("identity"), Identity(N)); NR->SetStringField(TEXT("guid"), N->NodeGuid.ToString()); NodeRows.Add(MakeShared<FJsonValueObject>(NR)); Row->SetArrayField(TEXT("nodes"), NodeRows);
            auto Props = MakeShared<FJsonObject>(); NR->SetObjectField(TEXT("properties"), Props); if (!Properties(N, {}, Props)) { return false; }
            TArray<TSharedPtr<FJsonValue>> PinRows;
            for (const UEdGraphPin* P : N->Pins)
            {
                auto PR = MakeShared<FJsonObject>(); PR->SetStringField(TEXT("id"), P->PinId.ToString()); PR->SetStringField(TEXT("name"), P->PinName.ToString()); PR->SetNumberField(TEXT("direction"), static_cast<uint8>(P->Direction));
                FString Type; FEdGraphPinType::StaticStruct()->ExportText(Type, &P->PinType, nullptr, N, PPF_None, nullptr); if (Type.Len() > MaxText || !CleanText(Type) || !CleanText(P->DefaultValue) || !CleanText(P->AutogeneratedDefaultValue)) { return false; }
                if (!CleanText(ObjectPath(P->DefaultObject)) || !CleanText(P->DefaultTextValue.ToString())) { return false; }
                PR->SetStringField(TEXT("type"), Type); PR->SetStringField(TEXT("default"), P->DefaultValue); PR->SetStringField(TEXT("autogenerated_default"), P->AutogeneratedDefaultValue); PR->SetStringField(TEXT("default_object"), ObjectPath(P->DefaultObject)); PR->SetStringField(TEXT("default_text"), P->DefaultTextValue.ToString());
                TArray<TSharedPtr<FJsonValue>> Links;
                for (const UEdGraphPin* Peer : P->LinkedTo) { if (!Peer || !OwnedPins.Contains(Peer) || !Peer->LinkedTo.Contains(P) || Peer->Direction == P->Direction) { return false; } Links.Add(MakeShared<FJsonValueString>(Peer->PinId.ToString())); }
                PR->SetArrayField(TEXT("links"), Links); PinRows.Add(MakeShared<FJsonValueObject>(PR)); NR->SetArrayField(TEXT("pins"), PinRows);
            }
        }
    }
    J->SetNumberField(TEXT("node_count"), Nodes); J->SetNumberField(TEXT("pin_count"), Pins); return true;
}
bool ParentSources(FContext& C, TSharedRef<FJsonObject>& Sources, FResult& R)
{
    if (!C.GeneratedParents) { return true; }
    if (!ParentAdmission(C, R)) { return false; }
    auto All = MakeShared<FJsonObject>(); Sources->SetObjectField(TEXT("generated_parent_blueprints"), All);
    for (const auto& P : C.Parents)
    {
        UBlueprint* B = P.Blueprint.Get(); auto Row = MakeShared<FJsonObject>(); All->SetObjectField(B->GetPathName(), Row);
        Row->SetObjectField(TEXT("blueprint"), Identity(B)); Row->SetObjectField(TEXT("class"), Identity(P.Class.Get())); Row->SetObjectField(TEXT("skeleton"), Identity(P.Skeleton.Get())); Row->SetObjectField(TEXT("cdo"), Identity(P.CDO.Get()));
        Row->SetStringField(TEXT("super"), P.Super->GetPathName()); Row->SetBoolField(TEXT("dirty"), B->GetOutermost()->IsDirty()); Row->SetNumberField(TEXT("status"), static_cast<uint8>(B->Status.GetValue()));
        auto D = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"), D); if (!Disk(B->GetOutermost()->GetName(), D)) { return false; }
        auto Cdo = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_properties"), Cdo); if (!Properties(P.CDO.Get(), {}, Cdo) || !ParentAdmission(C, R)) { return false; }
        TArray<UObject*> CurrentChildren; GetObjectsWithOuter(P.CDO.Get(), CurrentChildren, EGetObjectsFlags::IncludeNestedObjects);
        if (CurrentChildren.Num() != P.CdoChildren.Num()) { return false; }
        auto Children = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("cdo_owned_subobjects"), Children);
        for (UObject* O : CurrentChildren) { if (!P.CdoChildren.Contains(O) || !Loaded(O)) { return false; } auto Item = MakeShared<FJsonObject>(); Children->SetObjectField(O->GetPathName(), Item); Item->SetObjectField(TEXT("identity"), Identity(O)); auto Props = MakeShared<FJsonObject>(); Item->SetObjectField(TEXT("properties"), Props); if (!Properties(O, {}, Props) || !ParentAdmission(C, R)) { return false; } }
        auto Declarations = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("declarations"), Declarations);
        for (const FName Field : { FName(TEXT("NewVariables")), FName(TEXT("ImplementedInterfaces")), FName(TEXT("ParentClass")) }) { FProperty* Prop = FindFProperty<FProperty>(B->GetClass(), Field); FString Value; if (!Prop || !Prop->ExportText_Direct(Value, Prop->ContainerPtrToValuePtr<void>(B), Prop->ContainerPtrToValuePtr<void>(B), B, PPF_None) || Value.Len() > MaxText || !CleanText(Value) || !ParentAdmission(C, R)) { return false; } Declarations->SetStringField(Field.ToString(), Value); }
        auto Graph = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("graphs"), Graph); if (!Graphs(B, Graph) || !ParentAdmission(C, R)) { return false; }
        auto Nodes = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("local_scs"), Nodes);
        if (P.SCS.IsValid())
        {
            Row->SetObjectField(TEXT("scs"), Identity(P.SCS.Get()));
            for (USCS_Node* N : P.Nodes) { auto Item = MakeShared<FJsonObject>(); Nodes->SetObjectField(N->VariableGuid.ToString(), Item); Item->SetObjectField(TEXT("identity"), Identity(N)); Item->SetObjectField(TEXT("template"), Identity(N->ComponentTemplate)); auto NP = MakeShared<FJsonObject>(), TP = MakeShared<FJsonObject>(); Item->SetObjectField(TEXT("node_properties"), NP); Item->SetObjectField(TEXT("template_properties"), TP); if (!Properties(N, {}, NP) || !ParentAdmission(C, R) || !Properties(N->ComponentTemplate, {}, TP) || !ParentAdmission(C, R)) { return false; } }
        }
    }
    return ParentAdmission(C, R);
}
bool CompilerTickPolicy(const FContext& C, bool DesiredCanEverTick, FResult& R, bool RawTick = false, bool WillCompile = false)
{
    UBlueprint* B = C.Blueprint.Get(); UClass* Parent = B->ParentClass.Get();
    auto Policy = MakeShared<FJsonObject>(); R.Data->SetObjectField(TEXT("compiler_tick_policy"), Policy);
    Policy->SetObjectField(TEXT("parent_class"), Identity(Parent)); Policy->SetBoolField(TEXT("current_can_ever_tick"), C.CDO->PrimaryActorTick.bCanEverTick); Policy->SetBoolField(TEXT("requested_can_ever_tick"), DesiredCanEverTick);
    Policy->SetBoolField(TEXT("complete"), false); Policy->SetBoolField(TEXT("compatible"), false); Policy->SetStringField(TEXT("scope"), TEXT("existing immediate parent CDO eligibility; parent-false requires no current own ReceiveTick and no ambiguous active source override"));
    R.Data->SetBoolField(TEXT("compiler_tick_policy_checked"), true); R.Stage = TEXT("compiler_tick_policy");
    if (!CurrentDerivedTarget(C, R)) { return false; }
    AActor* ParentCDO = (Native(Parent) || C.GeneratedParents) ? Cast<AActor>(Parent->GetDefaultObject(false)) : nullptr; Policy->SetObjectField(TEXT("parent_cdo"), Identity(ParentCDO));
    if (!Loaded(ParentCDO) || ParentCDO->GetClass() != Parent || ParentCDO->GetOuter() != Parent->GetOuter() || !ParentCDO->HasAllFlags(RF_ClassDefaultObject | RF_ArchetypeObject) || !ParentCDO->IsTemplate() || ParentCDO->GetWorld())
    { return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unobservable"), TEXT("Existing exact current Actor parent CDO required; no parent CDO creation or compile-on-load override.")); }
    const bool ParentCanEverTick = ParentCDO->PrimaryActorTick.bCanEverTick; Policy->SetBoolField(TEXT("parent_can_ever_tick"), ParentCanEverTick);
    if(RawTick&&!WillCompile)
    {
        Policy->SetBoolField(TEXT("complete"),true);Policy->SetBoolField(TEXT("compatible"),true);R.Data->SetBoolField(TEXT("compiler_tick_policy_compatible"),true);
        Policy->SetStringField(TEXT("scope"),TEXT("Original dry-run or compile=False requested CDO value; no compiler dispatched or cold/cook persistence claimed"));return CurrentDerivedTarget(C,R);
    }
    const FName ReceiveTickName(GET_FUNCTION_NAME_CHECKED(AActor, ReceiveTick)); UFunction* OwnTick = B->GeneratedClass->FindFunctionByName(ReceiveTickName, EIncludeSuperFlag::ExcludeSuper);
    Policy->SetObjectField(TEXT("current_own_receive_tick"), Identity(OwnTick)); Policy->SetBoolField(TEXT("current_own_receive_tick_present"), OwnTick != nullptr);
    if (OwnTick && (!Loaded(OwnTick) || OwnTick->GetOuter() != B->GeneratedClass))
    { return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unobservable"), TEXT("Current own ReceiveTick identity is incomplete; no mutation dispatched.")); }
    TArray<TSharedPtr<FJsonValue>> SourceTickRows; Policy->SetArrayField(TEXT("source_receive_tick"), SourceTickRows); bool Ambiguous = false;
    if (!ParentCanEverTick)
    {
        TArray<UEdGraph*> All; B->GetAllGraphs(All); if (All.Num() > MaxGraphs) { return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unobservable"), TEXT("Bounded current source graph inventory required before eligibility decision.")); }
        for (UEdGraph* G : All)
        {
            if (!Loaded(G) || G->GetOutermost() != B->GetOutermost()) { return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unobservable"), TEXT("Current source graph identity incomplete; no mutation dispatched.")); }
            if (G->GetFName() == ReceiveTickName) { Ambiguous = true; }
            for (UEdGraphNode* N : G->Nodes)
            {
                if (UK2Node_FunctionEntry* Entry = Cast<UK2Node_FunctionEntry>(N); Entry && (Entry->CustomGeneratedFunctionName == ReceiveTickName || Entry->FunctionReference.GetMemberName() == ReceiveTickName))
                {
                    auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("node"), Identity(Entry)); Row->SetObjectField(TEXT("graph"), Identity(G)); Row->SetStringField(TEXT("kind"), TEXT("function_entry")); Row->SetBoolField(TEXT("inert_default_event"), false);
                    SourceTickRows.Add(MakeShared<FJsonValueObject>(Row)); Policy->SetArrayField(TEXT("source_receive_tick"), SourceTickRows); Ambiguous = true;
                }
                UK2Node_Event* Event = Cast<UK2Node_Event>(N); if (!Event || (Event->EventReference.GetMemberName() != ReceiveTickName && Event->CustomFunctionName != ReceiveTickName)) { continue; }
                auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("node"), Identity(Event)); Row->SetObjectField(TEXT("graph"), Identity(G)); Row->SetBoolField(TEXT("override_function"), Event->bOverrideFunction); Row->SetNumberField(TEXT("enabled_state"), static_cast<uint8>(Event->GetDesiredEnabledState()));
                Row->SetStringField(TEXT("kind"), TEXT("event"));
                int32 Links = 0; for (const UEdGraphPin* Pin : Event->Pins) { if (!Pin || Pin->GetOwningNodeUnchecked() != Event) { SourceTickRows.Add(MakeShared<FJsonValueObject>(Row)); Policy->SetArrayField(TEXT("source_receive_tick"), SourceTickRows); return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unobservable"), TEXT("Tick source pin ownership incomplete; no mutation dispatched.")); } Links += Pin->LinkedTo.Num(); }
                Row->SetNumberField(TEXT("linked_pin_edges"), Links); const bool Inert = Loaded(Event) && Event->GetClass() == UK2Node_Event::StaticClass() && Event->GetOuter() == G && Event->bOverrideFunction && Event->CustomFunctionName.IsNone() && Event->GetDesiredEnabledState() == ENodeEnabledState::Disabled && Links == 0;
                Row->SetBoolField(TEXT("inert_default_event"), Inert); SourceTickRows.Add(MakeShared<FJsonValueObject>(Row)); Policy->SetArrayField(TEXT("source_receive_tick"), SourceTickRows); Ambiguous |= !Inert;
            }
        }
    }
    Policy->SetBoolField(TEXT("source_ambiguous"), Ambiguous); Policy->SetBoolField(TEXT("complete"), true);
    const bool Compatible = (RawTick||(C.CDO->PrimaryActorTick.bCanEverTick == ParentCanEverTick && DesiredCanEverTick == ParentCanEverTick)) && (ParentCanEverTick || (!OwnTick && !Ambiguous));
    if(RawTick&&Compatible)
    {
        R.Data->SetBoolField(TEXT("raw_tick_expected_compiled_can_ever_tick"),ParentCanEverTick);
        Policy->SetBoolField(TEXT("requested_differs_from_compiler_eligibility"),DesiredCanEverTick!=ParentCanEverTick);
        Policy->SetStringField(TEXT("scope"),TEXT("UE58 KismetCompiler SetCanEverTick: reset to immediate parent; parent-false has no current own/active/ambiguous ReceiveTick. Only this compiler-owned eligibility field may normalize; request and actual remain distinct"));
    }
    Policy->SetBoolField(TEXT("compatible"), Compatible); R.Data->SetBoolField(TEXT("compiler_tick_policy_compatible"), Compatible);
    if (!Compatible) { return Fail(R, TEXT("blueprint_defaults_compiler_tick_policy_unsupported"), TEXT("Current and requested CanEverTick must equal existing immediate parent CDO policy; parent-false own/active Tick overrides remain incomplete. CDO-only overrides can be recomputed on load; no graph mutation or ephemeral restore will be dispatched.")); }
    return true;
}
struct FObservation
{
    TSharedRef<FJsonObject> State = MakeShared<FJsonObject>(), NativeSources = MakeShared<FJsonObject>(), References = MakeShared<FJsonObject>(), DiskState = MakeShared<FJsonObject>(), OtherDirty = MakeShared<FJsonObject>();
    TSharedRef<FJsonObject> ActorTickValues = MakeShared<FJsonObject>(), ActorTickFields = MakeShared<FJsonObject>();
    bool bComplete = false;
};
TSharedRef<FJsonObject> Observation(const FObservation& O)
{
    auto J = MakeShared<FJsonObject>(); J->SetBoolField(TEXT("complete"), O.bComplete); J->SetObjectField(TEXT("state"), O.State); J->SetObjectField(TEXT("native_sources"), O.NativeSources); J->SetObjectField(TEXT("references"), O.References); J->SetObjectField(TEXT("disk"), O.DiskState); J->SetObjectField(TEXT("other_package_dirty"), O.OtherDirty); J->SetObjectField(TEXT("actor_tick_values"), O.ActorTickValues); J->SetObjectField(TEXT("actor_tick_fields"), O.ActorTickFields); return J;
}
bool Capture(FContext& C, FObservation& O, FResult& R, const TCHAR* Key)
{
    O = FObservation{}; // Fresh phase maps preserve every previously published prefix.
    if (!CurrentDerivedTarget(C, R)) { return false; }
    auto J = Observation(O); R.Data->SetObjectField(Key, J); UBlueprint* B = C.Blueprint.Get(); if (!Loaded(B) || !Loaded(C.CDO) || B->GeneratedClass->GetDefaultObject(false) != C.CDO) { return false; }
    O.ActorTickValues = Tick(C.CDO); J->SetObjectField(TEXT("actor_tick_values"), O.ActorTickValues);
    FStructProperty* TickProperty = FindFProperty<FStructProperty>(C.CDO->GetClass(), TEXT("PrimaryActorTick"));
    if (!TickProperty || TickProperty->ArrayDim != 1 || TickProperty->GetOwnerClass() != AActor::StaticClass() || !TickProperty->HasAllPropertyFlags(CPF_Edit | CPF_DisableEditOnInstance)
        || !Loaded(TickProperty->Struct) || TickProperty->Struct->GetPathName() != TEXT("/Script/Engine.ActorTickFunction") || TickProperty->Struct->GetStructureSize() != sizeof(FActorTickFunction)
        || !Properties(TickProperty->Struct, &C.CDO->PrimaryActorTick, C.CDO, {}, O.ActorTickFields)) { return false; }
    O.State->SetObjectField(TEXT("identity"), Identity(B)); O.State->SetObjectField(TEXT("cdo_identity"), Identity(C.CDO)); O.State->SetStringField(TEXT("parent_class"), B->ParentClass->GetPathName()); O.State->SetStringField(TEXT("generated_class"), B->GeneratedClass->GetPathName()); O.State->SetStringField(TEXT("skeleton_class"), B->SkeletonGeneratedClass->GetPathName()); O.State->SetBoolField(TEXT("package_dirty"), B->GetOutermost()->IsDirty()); O.State->SetNumberField(TEXT("blueprint_status"), static_cast<uint8>(B->Status.GetValue()));
    auto Declarations = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("blueprint_declarations"), Declarations);
    for (const FName Field : { FName(TEXT("NewVariables")), FName(TEXT("ImplementedInterfaces")), FName(TEXT("ParentClass")) })
    { FProperty* P = FindFProperty<FProperty>(B->GetClass(), Field); FString S; if (!P || !P->ExportText_Direct(S, P->ContainerPtrToValuePtr<void>(B), P->ContainerPtrToValuePtr<void>(B), B, PPF_None) || S.Len() > MaxText || !CleanText(S)) { return false; } Declarations->SetStringField(Field.ToString(), S); }
    auto CDOProps = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_cdo_properties"), CDOProps);
    if (!Properties(C.CDO, C.bSkeletal ? TSet<FName>{ FName(*C.SelectedName) } : TSet<FName>{ FName(TEXT("PrimaryActorTick")) }, CDOProps)) { return false; }
    if (C.bSkeletal)
    {
        auto Member = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("selected_scs_cdo_member"), Member);
        FProperty* P = FindFProperty<FProperty>(C.CDO->GetClass(), FName(*C.SelectedName)); Member->SetBoolField(TEXT("present"), P != nullptr);
        if (P)
        {
            FObjectPropertyBase* Object = CastField<FObjectPropertyBase>(P);
            Member->SetStringField(TEXT("path"), P->GetPathName()); Member->SetStringField(TEXT("owner"), P->GetOwnerStruct()->GetPathName()); Member->SetStringField(TEXT("property_flags"), LexToString(static_cast<uint64>(P->GetPropertyFlags()))); Member->SetNumberField(TEXT("array_dim"), P->ArrayDim);
            if (!Object || P->ArrayDim != 1 || P->GetOwnerClass() != C.CDO->GetClass() || Object->PropertyClass != USkeletalMeshComponent::StaticClass() || !P->HasAnyPropertyFlags(CPF_BlueprintVisible)) { return false; }
            Member->SetStringField(TEXT("object_class"), Object->PropertyClass->GetPathName()); Member->SetStringField(TEXT("value"), ObjectPath(Object->GetObjectPropertyValue_InContainer(C.CDO)));
        }
    }
    auto Selected = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("selected_values"), Selected);
    if (C.bSkeletal) { if (!SkeletalValues(C.Skeletal, Selected)) { return false; } }
    else
    {
        Selected = Tick(C.CDO); O.State->SetObjectField(TEXT("selected_values"), Selected); auto OtherTick = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("unselected_tick_properties"), OtherTick);
        FStructProperty* P = FindFProperty<FStructProperty>(C.CDO->GetClass(), TEXT("PrimaryActorTick"));
        if (!P || P->ArrayDim != 1 || !Loaded(P->Struct) || P->Struct->GetPathName() != TEXT("/Script/Engine.ActorTickFunction") || P->Struct->GetStructureSize() != sizeof(FActorTickFunction)
            || !Properties(P->Struct, P->ContainerPtrToValuePtr<void>(C.CDO), C.CDO, { TEXT("bCanEverTick"), TEXT("bStartWithTickEnabled"), TEXT("bAllowTickOnDedicatedServer"), TEXT("bTickEvenWhenPaused"), TEXT("TickInterval") }, OtherTick)) { return false; }
    }
    USimpleConstructionScript* SCS = B->SimpleConstructionScript.Get(); TArray<TSharedPtr<FJsonValue>> SCSRows; O.State->SetArrayField(TEXT("local_scs"), SCSRows);
    if (SCS)
    {
        if (!Loaded(SCS) || SCS->GetOuter() != B->GeneratedClass || SCS->GetAllNodes().Num() > MaxNodes) { return false; } O.State->SetObjectField(TEXT("scs_identity"), Identity(SCS)); TSet<USCS_Node*> Nodes;
        for (USCS_Node* N : SCS->GetAllNodes()) { if (!Loaded(N) || N->GetOuter() != SCS || Nodes.Contains(N) || !N->VariableGuid.IsValid() || !Loaded(N->ComponentTemplate) || N->ComponentTemplate->GetOuter() != B->GeneratedClass || !Native(N->ComponentClass) || N->ComponentTemplate->GetClass() != N->ComponentClass) { return false; } Nodes.Add(N); }
        for (USCS_Node* N : SCS->GetAllNodes())
        {
            auto Row = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("identity"), Identity(N)); Row->SetStringField(TEXT("name"), N->GetVariableName().ToString()); Row->SetStringField(TEXT("guid"), N->VariableGuid.ToString()); Row->SetObjectField(TEXT("template_identity"), Identity(N->ComponentTemplate)); SCSRows.Add(MakeShared<FJsonValueObject>(Row)); O.State->SetArrayField(TEXT("local_scs"), SCSRows);
            TArray<TSharedPtr<FJsonValue>> Children; for (USCS_Node* Child : N->GetChildNodes()) { if (!Nodes.Contains(Child)) { return false; } Children.Add(MakeShared<FJsonValueString>(Child->VariableGuid.ToString())); } Row->SetArrayField(TEXT("children"), Children);
            auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("unselected_template_properties"), P);
            TSet<FName> Skip; if (N == C.SelectedNode) { Skip = { TEXT("SkinnedAsset"), TEXT("SkeletalMesh"), TEXT("SkeletalMeshAsset"), TEXT("AnimClass"), TEXT("AnimationMode"), TEXT("PhysicsAssetOverride"), TEXT("BodyInstance"), TEXT("bBlendPhysics"), TEXT("VisibilityBasedAnimTickOption") }; }
            if (!Properties(N->ComponentTemplate, Skip, P)) { return false; }
            if (N == C.SelectedNode)
            {
                auto OtherBody = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("unselected_body_instance_properties"), OtherBody); FStructProperty* Body = FindFProperty<FStructProperty>(C.Skeletal->GetClass(), TEXT("BodyInstance"));
                if (!Body || Body->ArrayDim != 1 || !Loaded(Body->Struct) || Body->Struct->GetPathName() != TEXT("/Script/Engine.BodyInstance") || Body->Struct->GetStructureSize() != sizeof(FBodyInstance)
                    || !Properties(Body->Struct, Body->ContainerPtrToValuePtr<void>(C.Skeletal), C.Skeletal, { TEXT("bSimulatePhysics"), TEXT("bEnableGravity"), TEXT("CollisionEnabled"), TEXT("CollisionResponses") }, OtherBody)) { return false; }
            }
        }
        TArray<TSharedPtr<FJsonValue>> Roots; for (USCS_Node* N : SCS->GetRootNodes()) { if (!Nodes.Contains(N)) { return false; } Roots.Add(MakeShared<FJsonValueString>(N->VariableGuid.ToString())); } O.State->SetArrayField(TEXT("scs_roots"), Roots);
    }
    auto Graph = MakeShared<FJsonObject>(); O.State->SetObjectField(TEXT("graphs"), Graph); if (!Graphs(B, Graph) || !Sources(C.bSkeletal, C.NativeRoot, O.NativeSources) || !ParentSources(C, O.NativeSources, R)) { return false; }
    for (const auto& Strong : C.Assets)
    {
        UObject* Asset = Strong.Get(); auto Row = MakeShared<FJsonObject>(); O.References->SetObjectField(ObjectPath(Asset), Row); Row->SetObjectField(TEXT("identity"), Identity(Asset)); auto P = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("properties"), P); auto D = MakeShared<FJsonObject>(); Row->SetObjectField(TEXT("disk"), D);
        if (!Loaded(Asset) || Asset->GetOutermost()->IsDirty() || !Properties(Asset, {}, P) || !Disk(Asset->GetOutermost()->GetName(), D) || !D->GetBoolField(TEXT("exists")) || D->GetNumberField(TEXT("bytes")) <= 0) { return false; }
    }
    if (!Disk(C.Package, O.DiskState) || !Dirty(C.Package, O.OtherDirty)) { return false; }
    if(C.NativeAnim){if(!CurrentDerivedTarget(C,R)){return false;}if(C.NativeAnim->Ready){auto Source=MakeShared<FJsonObject>();Source->SetObjectField(TEXT("identity"),Identity(C.NativeAnim->Class.Get()));Source->SetObjectField(TEXT("existing_cdo"),Identity(C.NativeAnim->CDO.Get()));Source->SetObjectField(TEXT("fields"),C.NativeAnim->Observed);Source->SetStringField(TEXT("class_flags"),LexToString(static_cast<uint32>(C.NativeAnim->Class->GetClassFlags())));Source->SetBoolField(TEXT("package_dirty"),C.NativeAnim->Class->GetPackage()->IsDirty());Source->SetBoolField(TEXT("compiled_in"),true);O.References->SetObjectField(C.NativeAnim->Class->GetPathName(),Source);}}
    O.bComplete = true; J->SetBoolField(TEXT("complete"), true); return true;
}
// Only a confirmed successful full compile may replace this exact owned CDO
// with the same identity and the precise constructor-cleared load bits.
bool PostcompileCdoEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B)
{
    if (!A || !B || A->Type != EJson::Object || B->Type != EJson::Object) { return false; }
    const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num() || !X->GetBoolField(TEXT("valid")) || !Y->GetBoolField(TEXT("valid"))) { return false; }
    const FString Outer = X->GetStringField(TEXT("outer")), Class = X->GetStringField(TEXT("class"));
    if (!Outer.StartsWith(TEXT("/Game/")) || !Class.StartsWith(Outer + TEXT(".")) || X->GetStringField(TEXT("path")) != Outer + TEXT(".Default__") + FPackageName::ObjectPathToObjectName(Class)) { return false; }
    const FString Old = X->GetStringField(TEXT("object_flags")), Now = Y->GetStringField(TEXT("object_flags")); const uint64 Flags = FCString::Strtoui64(*Old, nullptr, 10);
    const uint64 CdoFlags = static_cast<uint64>(RF_ClassDefaultObject | RF_ArchetypeObject), LoadFlags = static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted);
    if (LexToString(Flags) != Old || (Flags & CdoFlags) != CdoFlags || (Now != Old && Now != LexToString(Flags & ~LoadFlags))) { return false; }
    for (const auto& Field : X->Values) { if (Field.Key.ToView() != TEXT("object_flags") && !Equal(Field.Value, Y->TryGetField(Field.Key.ToView()))) { return false; } } return true;
}
// RefreshAllNodes reconstructs pins. Compare authored logical endpoints and
// defaults across the old/current pin inventories, never physical PinId/order.
bool RefreshedGraphsEqual(const TSharedPtr<FJsonValue>& A,const TSharedPtr<FJsonValue>& B)
{
    if(!A||!B||A->Type!=EJson::Object||B->Type!=EJson::Object)return false;
    auto Copy=[](const TSharedPtr<FJsonValue>& V)->TSharedPtr<FJsonObject>{TSharedPtr<FJsonObject> J;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(V->AsObject().ToSharedRef())),J);return J;};
    auto Normalize=[](const TSharedPtr<FJsonObject>& Graphs)->bool
    {
        if(!Graphs)return false;const TArray<TSharedPtr<FJsonValue>>* Rows=nullptr;if(!Graphs->TryGetArrayField(TEXT("graphs"),Rows))return false;
        struct FPinRole{FString Key,Graph;TSharedPtr<FJsonObject> Pin;};TMap<FString,FPinRole> Inventory;
        for(const auto& GraphValue:*Rows)
        {
            auto G=GraphValue->AsObject();const FString GraphGuid=G->GetStringField(TEXT("guid"));const TArray<TSharedPtr<FJsonValue>>* Nodes=nullptr;if(!G->TryGetArrayField(TEXT("nodes"),Nodes))continue;
            for(const auto& NodeValue:*Nodes)
            {
                auto N=NodeValue->AsObject();const FString NodeGuid=N->GetStringField(TEXT("guid"));const TArray<TSharedPtr<FJsonValue>>* Pins=nullptr;if(!N->TryGetArrayField(TEXT("pins"),Pins))return false;TSet<FString> Names;
                for(const auto& PinValue:*Pins)
                {
                    auto P=PinValue->AsObject();const FString Name=P->GetStringField(TEXT("name")).ToLower();const FString Local=LexToString(static_cast<int32>(P->GetNumberField(TEXT("direction"))))+TEXT(":")+Name;const FString Id=P->GetStringField(TEXT("id"));
                    if(Name.IsEmpty()||Id.IsEmpty()||Names.Contains(Local)||Inventory.Contains(Id)||Inventory.Num()>=MaxPins)return false;Names.Add(Local);
                    FPinRole Role;Role.Key=GraphGuid+TEXT("/")+NodeGuid+TEXT("/")+Local;Role.Graph=GraphGuid;Role.Pin=P;Inventory.Add(Id,MoveTemp(Role));
                }
            }
        }
        // Resolve and check all reciprocal links before mutating this DTO copy.
        TMap<FString,TArray<FString>> Edges;
        for(const auto& Item:Inventory)
        {
            TArray<FString> Links;for(const auto& Link:Item.Value.Pin->GetArrayField(TEXT("links")))
            {
                if(Link->Type!=EJson::String)return false;const FString Id=Link->AsString();const auto* Other=Inventory.Find(Id);
                if(!Other||Other->Graph!=Item.Value.Graph||Other->Pin->GetNumberField(TEXT("direction"))==Item.Value.Pin->GetNumberField(TEXT("direction"))||Links.Contains(Other->Key))return false;
                bool Reciprocal=false;for(const auto& Back:Other->Pin->GetArrayField(TEXT("links")))if(Back->Type==EJson::String&&Back->AsString()==Item.Key)Reciprocal=true;
                if(!Reciprocal)return false;Links.Add(Other->Key);
            }Links.Sort();Edges.Add(Item.Key,MoveTemp(Links));
        }
        for(const auto& GraphValue:*Rows)
        {
            auto G=GraphValue->AsObject();const TArray<TSharedPtr<FJsonValue>>* Nodes=nullptr;if(!G->TryGetArrayField(TEXT("nodes"),Nodes))continue;
            for(const auto& NodeValue:*Nodes)
            {
                auto N=NodeValue->AsObject();auto Logical=MakeShared<FJsonObject>();for(const auto& PinValue:N->GetArrayField(TEXT("pins")))
                {
                    auto P=PinValue->AsObject();const FString Name=P->GetStringField(TEXT("name")).ToLower();const FString Local=LexToString(static_cast<int32>(P->GetNumberField(TEXT("direction"))))+TEXT(":")+Name;const FString Id=P->GetStringField(TEXT("id"));TArray<TSharedPtr<FJsonValue>> Links;
                    for(const auto& Peer:Edges.FindChecked(Id))Links.Add(MakeShared<FJsonValueString>(Peer));P->SetArrayField(TEXT("links"),Links);P->RemoveField(TEXT("id"));P->SetStringField(TEXT("name"),Name);Logical->SetObjectField(Local,P.ToSharedRef());
                }N->RemoveField(TEXT("pins"));N->SetObjectField(TEXT("logical_pins"),Logical);
            }
        }return true;
    };
    auto X=Copy(A),Y=Copy(B);return Normalize(X)&&Normalize(Y)&&Equal(X.ToSharedRef(),Y.ToSharedRef());
}
// Compare the selected mutation scope while retaining every raw before/after field.
bool UnselectedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& ProfilePath, const FString& ExpectedProfileExport, bool AllowCompileCdo = false, const FString& At = FString(), bool AllowRefreshedGraphs = false)
{
    if (!A || !B || A->Type != B->Type) { return false; }
    if (At == TEXT("/selected_values") || At == TEXT("/selected_scs_cdo_member") || At == TEXT("/package_dirty") || At == TEXT("/blueprint_status")) { return true; }
    if (AllowRefreshedGraphs && At == TEXT("/graphs")) { return RefreshedGraphsEqual(A,B); }
    if (AllowCompileCdo && At == TEXT("/cdo_identity")) { return PostcompileCdoEqual(A, B); }
    if (!ProfilePath.IsEmpty() && At == ProfilePath)
    {
        if (A->Type != EJson::Object) { return false; } auto Expected = MakeShared<FJsonObject>();
        for (const auto& P : A->AsObject()->Values) { Expected->SetField(P.Key, P.Value); }
        Expected->SetArrayField(TEXT("values"), { MakeShared<FJsonValueString>(ExpectedProfileExport) });
        return Equal(Expected, B->AsObject().ToSharedRef());
    }
    if (A->Type == EJson::Object) { if (A->AsObject()->Values.Num() != B->AsObject()->Values.Num()) { return false; } for (const auto& P : A->AsObject()->Values) { if (!UnselectedEqual(P.Value, B->AsObject()->TryGetField(P.Key.ToView()), ProfilePath, ExpectedProfileExport, AllowCompileCdo, At + TEXT("/") + FString(P.Key.ToView()), AllowRefreshedGraphs)) { return false; } } return true; }
    if (A->Type == EJson::Array) { if (A->AsArray().Num() != B->AsArray().Num()) { return false; } for (int32 I = 0; I < A->AsArray().Num(); ++I) { if (!UnselectedEqual(A->AsArray()[I], B->AsArray()[I], ProfilePath, ExpectedProfileExport, AllowCompileCdo, At + TEXT("/") + LexToString(I), AllowRefreshedGraphs)) { return false; } } return true; }
    return Equal(A, B);
}
bool Fences(const FObservation& Before, const FObservation& After, FResult& R)
{
    R.Data->SetBoolField(TEXT("observations_complete"), Before.bComplete && After.bComplete); if (!Before.bComplete || !After.bComplete) { return Fail(R, TEXT("blueprint_defaults_observation_incomplete"), TEXT("Current native prefix remains incomplete; no next mutation or save."), R.bAttempted); }
    const bool Source = Equal(Before.NativeSources, After.NativeSources) && Equal(Before.References, After.References);
    FString ProfilePath, ExpectedProfileExport; bool Invalidated = false;
    if (R.Data->TryGetBoolField(TEXT("collision_profile_invalidation_expected"), Invalidated) && Invalidated)
    {
        const FString Selected = R.Data->GetStringField(TEXT("component_name")); const auto& Rows = Before.State->GetArrayField(TEXT("local_scs"));
        for (int32 I = 0; I < Rows.Num(); ++I) { if (Rows[I]->AsObject()->GetStringField(TEXT("name")) == Selected) { ProfilePath = TEXT("/local_scs/") + LexToString(I) + TEXT("/unselected_body_instance_properties/CollisionProfileName"); } }
        if (ProfilePath.IsEmpty() || !R.Data->TryGetStringField(TEXT("expected_collision_profile_export"), ExpectedProfileExport)) { return Fail(R, TEXT("blueprint_skeletal_collision_profile_unobservable"), TEXT("Exact selected collision callback comparison descriptor missing; no generic body normalization.")); }
    }
    const bool Compiled = R.bCompileAttempted && R.Compiled.IsSet() && R.Compiled.GetValue();
    const bool Skeletal = R.Operation == TEXT("set_skeletal_mesh_component_anim_defaults");
    const bool TickFields = !Skeletal || (Equal(Before.ActorTickFields, After.ActorTickFields) && Equal(Before.ActorTickValues, After.ActorTickValues));
    bool Refreshed=false;R.Data->TryGetBoolField(TEXT("raw_tick_refresh_returned"),Refreshed);
    const bool State = TickFields && UnselectedEqual(MakeShared<FJsonValueObject>(Before.State), MakeShared<FJsonValueObject>(After.State), ProfilePath, ExpectedProfileExport, Compiled, FString(), Refreshed); const bool Other = Equal(Before.OtherDirty, After.OtherDirty), D = Equal(Before.DiskState, After.DiskState);
    R.Data->SetBoolField(TEXT("postcompile_owned_cdo_identity_verified"), Compiled && PostcompileCdoEqual(Before.State->TryGetField(TEXT("cdo_identity")), After.State->TryGetField(TEXT("cdo_identity"))));
    R.bKnown = true; R.bChanged = !Equal(Before.State, After.State) || !Source || !Other || !D;
    R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("disk_preserved_before_save"), D);
    return (Source && State && Other && D) || Fail(R, TEXT("blueprint_defaults_preservation_failed"), TEXT("Complete source/unselected graph, defaults, ownership, other-dirty or pre-save disk fence differs; retained actual prefix, no save."));
}
void ClearFinal(FResult& R)
{ for (const TCHAR* K : { TEXT("observations_complete"), TEXT("source_preserved"), TEXT("unselected_state_preserved"), TEXT("independent_readback_passed") }) { R.Data->SetBoolField(K, false); } }
bool SavedEqual(const TSharedPtr<FJsonValue>& A, const TSharedPtr<FJsonValue>& B, const FString& Package, const FString& At = FString())
{
    if (!A || !B || A->Type != B->Type) { return false; } if (At == TEXT("/package_dirty")) { return !B->AsBool(); }
    if (A->Type == EJson::Object)
    {
        const auto X = A->AsObject(), Y = B->AsObject(); if (X->Values.Num() != Y->Values.Num()) { return false; }
        FString Object; const bool Owned = X->TryGetStringField(TEXT("path"), Object) && Object.StartsWith(Package + TEXT("."));
        for (const auto& P : X->Values)
        {
            const auto Q = Y->TryGetField(P.Key.ToView()); if (!Q) { return false; }
            if (Owned && P.Key.ToView() == TEXT("object_flags")) { if (P.Value->Type != EJson::String || Q->Type != EJson::String) { return false; } const uint64 Flags = FCString::Strtoui64(*P.Value->AsString(), nullptr, 10); if (LexToString(Flags) != P.Value->AsString() || Q->AsString() != LexToString(Flags | static_cast<uint64>(RF_WasLoaded | RF_LoadCompleted))) { return false; } }
            else if (!SavedEqual(P.Value, Q, Package, At + TEXT("/") + FString(P.Key.ToView()))) { return false; }
        }
        return true;
    }
    if (A->Type == EJson::Array) { if (A->AsArray().Num() != B->AsArray().Num()) { return false; } for (int32 I = 0; I < A->AsArray().Num(); ++I) { if (!SavedEqual(A->AsArray()[I], B->AsArray()[I], Package, At + TEXT("/") + LexToString(I))) { return false; } } return true; }
    return Equal(A, B);
}
bool Save(FContext& C, const FObservation& BeforeSave, FResult& R)
{
    if (!R.bSaveRequested) { return true; }
    if (!CurrentDerivedTarget(C, R)) { return false; }
    R.Stage = TEXT("save_asset"); R.Data->SetBoolField(TEXT("before_save_observations_complete"), BeforeSave.bComplete); R.Data->SetBoolField(TEXT("before_save_source_preserved"), R.Data->GetBoolField(TEXT("source_preserved"))); R.Data->SetBoolField(TEXT("before_save_unselected_state_preserved"), R.Data->GetBoolField(TEXT("unselected_state_preserved"))); ClearFinal(R);
    R.bAttempted = true; R.bSaveAttempted = true; Count(R, TEXT("save_attempt_count")); auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("asset_path"), C.Blueprint->GetPathName()); Receipt->SetBoolField(TEXT("attempted"), true); R.Data->SetArrayField(TEXT("save_calls"), { MakeShared<FJsonValueObject>(Receipt) });
    R.bSaved = UEditorAssetLibrary::SaveLoadedAsset(C.Blueprint.Get(), false); Receipt->SetBoolField(TEXT("success"), R.bSaved);
    bool LinkedCall=false;R.Data->TryGetBoolField(TEXT("raw_tick_linked_call_profile"),LinkedCall);if(LinkedCall&&!R.bSaved)return Fail(R,TEXT("blueprint_tick_refresh_save_failed"),TEXT("Exact saveFalse receipt retained; no follow-up source/target capture, rollback or retry."),true);
    if (!CurrentDerivedTarget(C, R)) { return false; }
    FObservation After; const bool Complete = Capture(C, After, R, TEXT("after_save")); R.Data->SetBoolField(TEXT("observations_complete"), Complete);
    if (!R.bSaved || !Complete) { return Fail(R, TEXT("blueprint_defaults_save_unobservable_or_failed"), TEXT("One exact save receipt and retained current prefix preserved; no retry or rollback."), true); }
    const bool State = SavedEqual(MakeShared<FJsonValueObject>(BeforeSave.State), MakeShared<FJsonValueObject>(After.State), C.Package) && Equal(BeforeSave.ActorTickValues, After.ActorTickValues) && Equal(BeforeSave.ActorTickFields, After.ActorTickFields), Source = Equal(BeforeSave.NativeSources, After.NativeSources) && Equal(BeforeSave.References, After.References), Other = Equal(BeforeSave.OtherDirty, After.OtherDirty);
    R.bKnown = true; R.bChanged = true; R.Data->SetBoolField(TEXT("source_preserved"), Source); R.Data->SetBoolField(TEXT("unselected_state_preserved"), State && Other); R.Data->SetBoolField(TEXT("independent_readback_passed"), State);
    if (!State || !Source || !Other || !After.DiskState->GetBoolField(TEXT("exists")) || After.DiskState->GetNumberField(TEXT("bytes")) <= 0 || C.Blueprint->GetOutermost()->IsDirty()) { return Fail(R, TEXT("blueprint_defaults_saved_readback_failed"), TEXT("Successful save receipt remains authoritative, but exact finite saved-state/source/file fence failed; do not replay.")); }
    return true;
}
// Fixed-signature stock native BlueprintFunctionLibrary calls only. The Engine
// refresh reconstructs their K2 representation; it never executes the function.
UFunction* TickNativeCallSource(const UK2Node_CallFunction* Call)
{
    if(!Loaded(Call)||Call->GetClass()!=UK2Node_CallFunction::StaticClass()||Call->bWantsEnumToExecExpansion)return nullptr;
    const FMemberReference& Ref=Call->FunctionReference;auto* P=FindFProperty<FObjectProperty>(FMemberReference::StaticStruct(),TEXT("MemberParent"));
    if(!P||P->GetOwnerStruct()!=FMemberReference::StaticStruct()||P->ArrayDim!=1||P->HasSetterOrGetter()||P->GetSize()!=sizeof(TObjectPtr<UObject>)||Ref.IsSelfContext()||Ref.IsLocalScope()||Ref.GetMemberGuid().IsValid())return nullptr;
    const auto* Slot=P->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Ref);if(!Slot->IsResolved()||!Loaded(Slot->Get()))return nullptr;
    UClass* Owner=Cast<UClass>(Slot->Get());if(!Native(Owner)||!Owner->IsChildOf(UBlueprintFunctionLibrary::StaticClass())||Owner->HasAnyClassFlags(CLASS_Interface))return nullptr;
    UFunction* F=Owner->FindFunctionByName(Ref.GetMemberName(),EIncludeSuperFlag::ExcludeSuper);
    if(!Loaded(F)||F->GetOuter()!=Owner||!F->HasAllFunctionFlags(FUNC_Native|FUNC_Public|FUNC_Static|FUNC_BlueprintCallable)||F->HasAnyFunctionFlags(FUNC_Delegate|FUNC_MulticastDelegate|FUNC_Net|FUNC_Event)||Call->bDefaultsToPureFunc!=F->HasAnyFunctionFlags(FUNC_BlueprintPure))return nullptr;
    // These metadata profiles can synthesize/wildcard-expand a signature rather
    // than reconstruct the same declared native parameter schema.
    for(const TCHAR* Key:{TEXT("Latent"),TEXT("CustomThunk"),TEXT("ArrayParm"),TEXT("ArrayTypeDependentParams"),TEXT("CustomStructureParam"),TEXT("DeterminesOutputType"),TEXT("DynamicOutputParam"),TEXT("ExpandEnumAsExecs"),TEXT("ExpandBoolAsExecs"),TEXT("BlueprintInternalUseOnly")})if(F->HasMetaData(Key))return nullptr;
    return F;
}
// The existing schema/Event lease also holds each distinct admitted native call,
// its owner/CDO and complete bounded reflected signature/metadata. No name list.
struct FTickRefreshSources
{
    TStrongObjectPtr<UClass> SchemaClass{UEdGraphSchema_K2::StaticClass()};
    TStrongObjectPtr<UObject> SchemaDefault;
    TSharedRef<FJsonObject> Before=MakeShared<FJsonObject>();
    TArray<TStrongObjectPtr<UFunction>> Functions;
    TArray<TArray<FProperty*>> Fields;
    TArray<TStrongObjectPtr<UObject>> Holds;
    struct FCallSource
    {
        TStrongObjectPtr<UClass> Owner;
        TStrongObjectPtr<UObject> CDO;
        TStrongObjectPtr<UFunction> Function;
        TArray<FProperty*> Fields;
        TArray<TStrongObjectPtr<UObject>> Types;
    };
    TArray<TUniquePtr<FCallSource>> Calls;
    TStrongObjectPtr<UObject> CallNodeDefault;
    static bool RuntimeType(UObject* O)
    {
        return Loaded(O)&&Loaded(O->GetOutermost())&&(O->GetOutermost()->GetName()==TEXT("/Script/CoreUObject")||O->GetOutermost()->GetName()==TEXT("/Script/Engine"))&&O->GetOutermost()->HasAnyPackageFlags(PKG_CompiledIn)&&!O->GetOutermost()->IsDirty();
    }
    bool CallSnapshot(TSharedRef<FJsonObject>& Out,bool Initial)
    {
        if(Calls.IsEmpty())return true;UClass* NodeClass=UK2Node_CallFunction::StaticClass();
        if(!Loaded(NodeClass)||!NodeClass->ClassDefaultObject.IsResolved()||!Loaded(CallNodeDefault.Get())||NodeClass->ClassDefaultObject.Get()!=CallNodeDefault.Get()||CallNodeDefault->GetClass()!=NodeClass||CallNodeDefault->GetOuter()!=NodeClass->GetOuter()||!CallNodeDefault->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return false;
        auto All=MakeShared<FJsonObject>();Out->SetObjectField(TEXT("native_call_sources"),All);All->SetObjectField(TEXT("node_class"),Identity(NodeClass));All->SetObjectField(TEXT("node_cdo"),Identity(CallNodeDefault.Get()));auto NodeFields=MakeShared<FJsonObject>();if(!Properties(CallNodeDefault.Get(),{},NodeFields))return false;All->SetObjectField(TEXT("node_cdo_fields"),NodeFields);
        for(auto& Source:Calls)
        {
            auto& S=*Source;UClass* K=S.Owner.Get();UFunction* F=S.Function.Get();
            if(!Native(K)||!K->IsChildOf(UBlueprintFunctionLibrary::StaticClass())||!Loaded(S.CDO.Get())||!K->ClassDefaultObject.IsResolved()||K->ClassDefaultObject.Get()!=S.CDO.Get()||S.CDO->GetClass()!=K||S.CDO->GetOuter()!=K->GetOuter()||!S.CDO->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject)||!Loaded(F)||F->GetOuter()!=K||K->FindFunctionByName(F->GetFName(),EIncludeSuperFlag::ExcludeSuper)!=F)return false;
            auto Row=MakeShared<FJsonObject>();All->SetObjectField(F->GetPathName(),Row);Row->SetObjectField(TEXT("owner"),Identity(K));Row->SetObjectField(TEXT("owner_cdo"),Identity(S.CDO.Get()));Row->SetObjectField(TEXT("function"),Identity(F));Row->SetStringField(TEXT("owner_flags"),LexToString(static_cast<uint32>(K->GetClassFlags())));Row->SetStringField(TEXT("function_flags"),LexToString(static_cast<uint32>(F->FunctionFlags)));Row->SetNumberField(TEXT("parms_size"),F->ParmsSize);Row->SetNumberField(TEXT("num_parms"),F->NumParms);
            auto OwnerFields=MakeShared<FJsonObject>(),FunctionFields=MakeShared<FJsonObject>();if(!Properties(S.CDO.Get(),{},OwnerFields)||!Properties(F,{},FunctionFields))return false;Row->SetObjectField(TEXT("owner_cdo_fields"),OwnerFields);Row->SetObjectField(TEXT("function_fields"),FunctionFields);
            auto Metadata=MakeShared<FJsonObject>();const auto* Meta=FMetaData::GetMapForObject(F);if(Meta){if(Meta->Num()>MaxProperties)return false;int32 Chars=0;for(const auto& M:*Meta){if((Chars+=M.Key.ToString().Len()+M.Value.Len())>MaxText)return false;Metadata->SetStringField(M.Key.ToString(),M.Value);}}Row->SetObjectField(TEXT("metadata"),Metadata);
            TArray<FProperty*> Current;int32 SignatureFieldCount=0;
            TFunction<bool(FProperty*,const TSharedRef<FJsonObject>&,int32)> Describe;
            Describe=[&](FProperty* P,const TSharedRef<FJsonObject>& V,int32 Depth)->bool
            {
                if(!P||Depth>8||++SignatureFieldCount>MaxProperties||P->ArrayDim!=1||P->HasSetterOrGetter()||P->HasAnyPropertyFlags(CPF_InstancedReference|CPF_ContainsInstancedReference|CPF_Deprecated))return false;
                Current.Add(P);V->SetStringField(TEXT("name"),P->GetName());V->SetStringField(TEXT("kind"),P->GetClass()->GetName());V->SetStringField(TEXT("owner"),P->GetOwnerStruct()?P->GetOwnerStruct()->GetPathName():FString());V->SetStringField(TEXT("owner_field"),P->GetOwner<FProperty>()?P->GetOwner<FProperty>()->GetPathName():FString());V->SetStringField(TEXT("flags"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));V->SetNumberField(TEXT("offset"),P->GetOffset_ForInternal());V->SetNumberField(TEXT("size"),P->GetSize());V->SetNumberField(TEXT("element_size"),P->ElementSize);
                auto FieldMeta=MakeShared<FJsonObject>();const auto* PMap=P->GetMetaDataMap();if(PMap){if(PMap->Num()>MaxProperties)return false;int32 Chars=0;for(const auto& M:*PMap){if((Chars+=M.Key.ToString().Len()+M.Value.Len())>MaxText)return false;FieldMeta->SetStringField(M.Key.ToString(),M.Value);}}V->SetObjectField(TEXT("metadata"),FieldMeta);
                if(auto* Bool=CastField<FBoolProperty>(P)){V->SetNumberField(TEXT("bool_byte_mask"),Bool->GetByteMask());V->SetNumberField(TEXT("bool_field_mask"),Bool->GetFieldMask());}
                auto Type=[&](UObject* O,const TCHAR* Key)->bool{if(!RuntimeType(O))return false;if(Initial)S.Types.Emplace(O);V->SetObjectField(Key,Identity(O));return true;};
                if(auto* C=CastField<FClassProperty>(P)){return C->PropertyClass.IsResolved()&&C->MetaClass.IsResolved()&&Type(C->PropertyClass.Get(),TEXT("property_class"))&&Type(C->MetaClass.Get(),TEXT("meta_class"));}
                if(auto* O=CastField<FObjectProperty>(P))return O->PropertyClass.IsResolved()&&Type(O->PropertyClass.Get(),TEXT("object_class"));
                if(auto* Struct=CastField<FStructProperty>(P))return Struct->Struct.IsResolved()&&Type(Struct->Struct.Get(),TEXT("struct"));
                UEnum* Enum=nullptr;if(auto* E=CastField<FEnumProperty>(P)){Enum=E->GetEnum();auto Under=MakeShared<FJsonObject>();if(!Describe(E->GetUnderlyingProperty(),Under,Depth+1))return false;V->SetObjectField(TEXT("underlying"),Under);}else if(auto* Byte=CastField<FByteProperty>(P))Enum=Byte->Enum;
                if(Enum){if(!Type(Enum,TEXT("enum"))||Enum->NumEnums()>256)return false;auto Values=MakeShared<FJsonObject>();for(int32 I=0;I<Enum->NumEnums();++I)Values->SetStringField(Enum->GetNameByIndex(I).ToString(),LexToString(Enum->GetValueByIndex(I)));V->SetObjectField(TEXT("enum_values"),Values);return true;}
                if(auto* A=CastField<FArrayProperty>(P)){auto Inner=MakeShared<FJsonObject>();if(!Describe(A->Inner,Inner,Depth+1))return false;V->SetObjectField(TEXT("inner"),Inner);return true;}
                if(auto* A=CastField<FSetProperty>(P)){auto Inner=MakeShared<FJsonObject>();if(!Describe(A->ElementProp,Inner,Depth+1))return false;V->SetObjectField(TEXT("element"),Inner);return true;}
                if(auto* A=CastField<FMapProperty>(P)){auto Key=MakeShared<FJsonObject>(),Value=MakeShared<FJsonObject>();if(!Describe(A->KeyProp,Key,Depth+1)||!Describe(A->ValueProp,Value,Depth+1))return false;V->SetObjectField(TEXT("key"),Key);V->SetObjectField(TEXT("value"),Value);return true;}
                return CastField<FNumericProperty>(P)||CastField<FBoolProperty>(P)||CastField<FStrProperty>(P)||CastField<FNameProperty>(P)||CastField<FTextProperty>(P);
            };
            TArray<TSharedPtr<FJsonValue>> Parameters;for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It){FProperty* P=*It;if(P->GetOwnerStruct()!=F||!P->HasAllPropertyFlags(CPF_Parm))return false;auto V=MakeShared<FJsonObject>();if(!Describe(P,V,0))return false;Parameters.Add(MakeShared<FJsonValueObject>(V));}
            if(Initial)S.Fields=Current;else if(S.Fields!=Current)return false;Row->SetArrayField(TEXT("parameters"),Parameters);
        }return true;
    }
    bool Snapshot(TSharedRef<FJsonObject>& Out,bool Initial)
    {
        UClass* Schema=SchemaClass.Get();UObject* Default=SchemaDefault.Get();
        if(!Loaded(Schema)||Schema!=UEdGraphSchema_K2::StaticClass()||!Loaded(Default)||Schema->GetDefaultObject(false)!=Default||Default->GetClass()!=Schema||Default->GetOuter()!=Schema->GetOuter()||!Default->HasAllFlags(RF_ClassDefaultObject|RF_ArchetypeObject))return false;
        Out->SetObjectField(TEXT("schema_class"),Identity(Schema));Out->SetObjectField(TEXT("schema_default"),Identity(Default));
        auto SchemaFields=MakeShared<FJsonObject>();if(!Properties(Default,{},SchemaFields))return false;Out->SetObjectField(TEXT("schema_default_fields"),SchemaFields);
        const TArray<FName> Names={FName(TEXT("ReceiveBeginPlay")),FName(TEXT("ReceiveActorBeginOverlap")),FName(TEXT("ReceiveTick")),FName(TEXT("UserConstructionScript"))};
        for(int32 I=0;I<Names.Num();++I)
        {
            UFunction* F=AActor::StaticClass()->FindFunctionByName(Names[I],EIncludeSuperFlag::ExcludeSuper);
            if(!Loaded(F)||F->GetOuter()!=AActor::StaticClass()||F->GetFName()!=Names[I]||!F->HasAnyFunctionFlags(FUNC_Event)||F->HasAnyFunctionFlags(FUNC_Static|FUNC_Delegate))return false;
            TArray<FProperty*> Now;for(TFieldIterator<FProperty> It(F,EFieldIteratorFlags::ExcludeSuper);It;++It)Now.Add(*It);
            if(Now.Num()!=((I==1||I==2)?1:0)||(!Initial&&(Functions[I].Get()!=F||Now!=Fields[I])))return false;
            if(Initial){Functions.Emplace(F);Fields.Add(Now);}
            auto Row=MakeShared<FJsonObject>();Row->SetObjectField(TEXT("identity"),Identity(F));Row->SetStringField(TEXT("function_flags"),LexToString(static_cast<uint32>(F->FunctionFlags)));
            auto FunctionFields=MakeShared<FJsonObject>();if(!Properties(F,{},FunctionFields))return false;Row->SetObjectField(TEXT("function_properties"),FunctionFields);
            if(!Now.IsEmpty())
            {
                FProperty* P=Now[0];if(P->GetOwnerStruct()!=F||P->ArrayDim!=1||!P->HasAllPropertyFlags(CPF_Parm)||P->HasAnyPropertyFlags(CPF_OutParm|CPF_ReturnParm|CPF_ReferenceParm))return false;
                if(I==1){auto* Object=CastField<FObjectProperty>(P);if(!Object||P->GetClass()!=FObjectProperty::StaticClass()||Object->PropertyClass!=AActor::StaticClass()||P->GetFName()!=TEXT("OtherActor"))return false;}
                if(I==2&&(P->GetClass()!=FFloatProperty::StaticClass()||P->GetFName()!=TEXT("DeltaSeconds")))return false;
                Row->SetStringField(TEXT("parameter_class"),P->GetClass()->GetName());Row->SetStringField(TEXT("parameter_flags"),LexToString(static_cast<uint64>(P->GetPropertyFlags())));Row->SetNumberField(TEXT("parameter_offset"),P->GetOffset_ForInternal());Row->SetNumberField(TEXT("parameter_size"),P->GetSize());
            }
            Out->SetObjectField(Names[I].ToString(),Row);
        }
        return CallSnapshot(Out,Initial);
    }
    bool Bind(FContext& C)
    {
        TArray<UEdGraph*> Graphs;C.Blueprint->GetAllGraphs(Graphs);
        for(UEdGraph* G:Graphs)for(UEdGraphNode* N:G->Nodes)if(auto* Call=Cast<UK2Node_CallFunction>(N))
        {
            UFunction* F=TickNativeCallSource(Call);if(!F)return false;bool Seen=false;for(const auto& S:Calls)Seen|=S->Function.Get()==F;if(Seen)continue;
            auto S=MakeUnique<FCallSource>();S->Function.Reset(F);S->Owner.Reset(Cast<UClass>(F->GetOuter()));S->CDO.Reset(S->Owner->GetDefaultObject(false));Calls.Add(MoveTemp(S));
        }
        if(!Calls.IsEmpty()){CallNodeDefault.Reset(UK2Node_CallFunction::StaticClass()->GetDefaultObject(false));Holds.Emplace(UK2Node_CallFunction::StaticClass());}
        SchemaDefault.Reset(SchemaClass->GetDefaultObject(false));
        for(UObject* O:{static_cast<UObject*>(AActor::StaticClass()),static_cast<UObject*>(UEdGraph::StaticClass()),static_cast<UObject*>(UK2Node_Event::StaticClass()),static_cast<UObject*>(UK2Node_FunctionEntry::StaticClass())})Holds.Emplace(O);
        return Snapshot(Before,true);
    }
    bool Current(){auto Now=MakeShared<FJsonObject>();return Snapshot(Now,false)&&Equal(Before,Now);}
};
bool RefreshTickAdmission(FContext& C,FResult& R)
{
    if(C.GeneratedParents||C.bSkeletal)return Fail(R,TEXT("blueprint_tick_refresh_owner_pending"),TEXT("Generated-parent or skeletal refresh remains incomplete; no writer"));
    TArray<UEdGraph*> GraphsNow;C.Blueprint->GetAllGraphs(GraphsNow);if(GraphsNow.Num()>MaxGraphs)return false;
    int32 TotalPins=0;bool LinkedCall=false;TSet<FGuid> GraphGuids;
    for(UEdGraph* G:GraphsNow)
    {
        if(!Loaded(G)||G->GetClass()!=UEdGraph::StaticClass()||!G->Schema.GetGCPtr().IsResolved()||G->Schema.Get()!=UEdGraphSchema_K2::StaticClass()||G->GetOutermost()!=C.Blueprint->GetOutermost()||G->Nodes.Num()>MaxNodes||!G->GraphGuid.IsValid()||GraphGuids.Contains(G->GraphGuid))
            return Fail(R,TEXT("blueprint_tick_refresh_graph_scope_pending"),TEXT("Exact resident K2-schema stock graph required; external/custom graph schema remains incomplete"));
        GraphGuids.Add(G->GraphGuid);TSet<UEdGraphNode*> Nodes;TSet<UEdGraphPin*> Pins;TSet<FGuid> Guids;
        for(UEdGraphNode* N:G->Nodes){if(!Loaded(N)||Nodes.Contains(N)||!N->NodeGuid.IsValid()||Guids.Contains(N->NodeGuid))return Fail(R,TEXT("blueprint_tick_refresh_node_inventory_pending"),TEXT("Unique held current graph nodes/GUIDs required"));Nodes.Add(N);Guids.Add(N->NodeGuid);for(UEdGraphPin* P:N->Pins){if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||Pins.Contains(P)||++TotalPins>MaxPins)return Fail(R,TEXT("blueprint_tick_refresh_pin_inventory_pending"),TEXT("Bounded unique current node pins required"));Pins.Add(P);}}
        for(UEdGraphNode* N:G->Nodes)
        {
            if(!Loaded(N)||N->GetOuter()!=G||!N->NodeGuid.IsValid()||(N->GetClass()!=UK2Node_Event::StaticClass()&&N->GetClass()!=UK2Node_FunctionEntry::StaticClass()&&N->GetClass()!=UK2Node_CallFunction::StaticClass()))
                return Fail(R,TEXT("blueprint_tick_refresh_node_scope_pending"),TEXT("Exact stock AActor Event, default UserConstructionScript entry1 and fixed-signature stock native CallFunction nodes required; other valid original nodes remain incomplete"));
            const auto* Event=Cast<UK2Node_Event>(N);const auto* Entry=Cast<UK2Node_FunctionEntry>(N);const auto* Call=Cast<UK2Node_CallFunction>(N);
            const FMemberReference& Reference=Event?Event->EventReference:Entry?Entry->FunctionReference:Call->FunctionReference;
            // The public GetMemberParentClass and TSubclassOf getters resolve.
            // Reuse the existing EventApi direct reflected-cell admission first.
            auto* ReferenceStruct=FMemberReference::StaticStruct();auto* ParentProperty=FindFProperty<FObjectProperty>(ReferenceStruct,TEXT("MemberParent"));
            if(!ParentProperty||ParentProperty->GetClass()!=FObjectProperty::StaticClass()||ParentProperty->GetOwnerStruct()!=ReferenceStruct||ParentProperty->ArrayDim!=1||ParentProperty->HasSetterOrGetter()||ParentProperty->GetSize()!=sizeof(TObjectPtr<UObject>))return Fail(R,TEXT("blueprint_tick_refresh_reference_layout_pending"),TEXT("Exact native member-parent reflected slot required; no resolving getter dispatched"));
            const auto* ParentSlot=ParentProperty->ContainerPtrToValuePtr<TObjectPtr<UObject>>(&Reference);
            if(!ParentSlot||!ParentSlot->IsResolved()||(!Call&&ParentSlot->Get()!=AActor::StaticClass())||Reference.IsSelfContext()||Reference.IsLocalScope()||Reference.GetMemberGuid().IsValid())return Fail(R,TEXT("blueprint_tick_refresh_signature_scope_pending"),TEXT("Already-resolved external native Actor or function-library reference required; self/custom/redirected/generated signatures remain incomplete"));
            const FName Member=Reference.GetMemberName();
            if(Call){if(!TickNativeCallSource(Call))return Fail(R,TEXT("blueprint_tick_refresh_call_scope_pending"),TEXT("Resident fixed-signature static native Engine BlueprintFunctionLibrary call required; latent/member/interface/custom-thunk-metadata/wildcard/dynamic-signature/generated profiles remain incomplete"));LinkedCall=true;}

            if(Event&&(!Event->bOverrideFunction||!Event->CustomFunctionName.IsNone()||(Member!=TEXT("ReceiveBeginPlay")&&Member!=TEXT("ReceiveActorBeginOverlap")&&Member!=TEXT("ReceiveTick"))))return Fail(R,TEXT("blueprint_tick_refresh_signature_scope_pending"),TEXT("Only the three resident native Actor default Event signatures are admitted"));
            if(Entry&&(Member!=TEXT("UserConstructionScript")||G->GetFName()!=TEXT("UserConstructionScript")||G->Nodes.Num()!=1||!Entry->CustomGeneratedFunctionName.IsNone()||!Entry->LocalVariables.IsEmpty()||!Entry->UserDefinedPins.IsEmpty()||Entry->GetExtraFlags()!=0||N->Pins.Num()!=1))return Fail(R,TEXT("blueprint_tick_refresh_construction_entry_pending"),TEXT("Exact default UCS entry1 with no local/user pins/custom flags required; other functions remain incomplete"));
            TSet<FString> Names;
            for(UEdGraphPin* P:N->Pins)
            {
                if(!P||P->WasTrashed()||P->GetOwningNodeUnchecked()!=N||(Entry&&!P->LinkedTo.IsEmpty())||P->ParentPin||!P->SubPins.IsEmpty()||P->bOrphanedPin||!P->PinId.IsValid()||(P->Direction!=EGPD_Input&&P->Direction!=EGPD_Output)||!P->DefaultObject.IsResolved())return Fail(R,TEXT("blueprint_tick_refresh_pin_scope_pending"),TEXT("Current unsplit Event/native-call logical pins or unlinked default UCS exec required; no writer"));
                TSet<UEdGraphPin*> Linked;for(UEdGraphPin* Peer:P->LinkedTo){if(!Peer||!Pins.Contains(Peer)||Peer->WasTrashed()||Peer->GetOwningNodeUnchecked()->GetGraph()!=G||Peer->Direction==P->Direction||!Peer->LinkedTo.Contains(P)||Linked.Contains(Peer))return Fail(R,TEXT("blueprint_tick_refresh_edge_pending"),TEXT("Same-graph reciprocal unique opposite-direction logical links required before refresh"));Linked.Add(Peer);}
                if(Entry&&(P->PinName!=UEdGraphSchema_K2::PN_Then||P->Direction!=EGPD_Output||P->PinType.PinCategory!=UEdGraphSchema_K2::PC_Exec))return Fail(R,TEXT("blueprint_tick_refresh_construction_pin_pending"),TEXT("Default UCS output exec then pin required; no full signature expansion"));
                const FString Key=LexToString(static_cast<int32>(P->Direction))+TEXT(":")+P->PinName.ToString().ToLower();if(P->PinName.IsNone()||Names.Contains(Key))return false;Names.Add(Key);
            }
        }
    }
    R.Data->SetBoolField(TEXT("raw_tick_linked_call_profile"),LinkedCall);
    return CurrentDerivedTarget(C,R);
}
bool RefreshTickNodes(FContext& C,const FObservation& Before,FObservation& After,FResult& R,FTickRefreshSources& Source)
{
    if(!RefreshTickAdmission(C,R))return false;
    TArray<TStrongObjectPtr<UObject>> Holds;TArray<UEdGraph*> GraphsNow;C.Blueprint->GetAllGraphs(GraphsNow);
    const auto BPIdentity=Identity(C.Blueprint.Get());TStrongObjectPtr<UClass> Parent(C.Blueprint->ParentClass.Get());
    for(UEdGraph* G:GraphsNow){Holds.Emplace(G);for(UEdGraphNode* N:G->Nodes){Holds.Emplace(N);for(UEdGraphPin* P:N->Pins){Holds.Emplace(P->DefaultObject.Get());Holds.Emplace(P->PinType.PinSubCategoryObject.Get());if(!CurrentDerivedTarget(C,R))return false;}}}
    if(!CurrentDerivedTarget(C,R))return false;
    if(!Source.Current())return Fail(R,TEXT("blueprint_tick_refresh_source_changed_before_sdk"),TEXT("Retained schema/native signature changed after configured writes; no refresh/fullcompile/save"),true);
    R.Stage=TEXT("refresh_tick_nodes");R.bAttempted=true;R.Data->SetNumberField(TEXT("refresh_nodes_calls"),1);R.Data->SetBoolField(TEXT("raw_tick_refresh_returned"),false);
    FBlueprintEditorUtils::RefreshAllNodes(C.Blueprint.Get());
    R.Data->SetBoolField(TEXT("raw_tick_refresh_returned"),true);
    if(!Source.Current())return Fail(R,TEXT("blueprint_tick_refresh_source_changed"),TEXT("Schema/native source changed during refresh; returned prefix retained and no next writer"),true);
    R.Data->SetBoolField(TEXT("raw_tick_refresh_source_preserved"),true);
    // SDK can recompile the skeleton. Admit current roles only after exact asset
    // and parent identity checks; old class/CDO/pin addresses are not reused.
    if(!Loaded(C.Blueprint.Get())||!Equal(BPIdentity,Identity(C.Blueprint.Get()))||C.Blueprint->ParentClass!=Parent.Get())return Fail(R,TEXT("blueprint_tick_refresh_identity_changed"),TEXT("Refresh returned with changed Blueprint/parent identity; no next writer"),true);
    FContext Current;if(!Prepare(C.Blueprint->GetPathName(),FString(),false,Current,R))return Fail(R,TEXT("blueprint_tick_refresh_current_unobservable"),TEXT("Refresh returned but current generated/skeleton/CDO scope is unobservable; no full compile/save"),true);
    Current.Assets=MoveTemp(C.Assets);C=MoveTemp(Current);
    if(!RefreshTickAdmission(C,R)||!Capture(C,After,R,TEXT("after_refresh_nodes")))return Fail(R,TEXT("blueprint_tick_refresh_readback_unobservable"),TEXT("Refresh current roles/logical pins incomplete; no full compile/save"),true);
    return Fences(Before,After,R);
}
bool Compile(FContext& C, const FObservation& Before, FObservation& After, FResult& R, bool RawTick = false)
{
    // r3 Capture detaches phase maps, so this configured tick snapshot stays immutable.
    FObservation ExpectedTick = After;
    if(RawTick)
    {
        bool ExpectedCanTick=false;if(!R.Data->TryGetBoolField(TEXT("raw_tick_expected_compiled_can_ever_tick"),ExpectedCanTick))return Fail(R,TEXT("blueprint_tick_compiler_prediction_missing"),TEXT("Known SDK parent-reset predicate required before full compile"));
        TSharedPtr<FJsonObject> Values,Fields;FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(After.ActorTickValues)),Values);FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(Text(After.ActorTickFields)),Fields);
        FActorTickFunction ExpectedField;ExpectedField.bCanEverTick=ExpectedCanTick;
        FBoolProperty* P=FindFProperty<FBoolProperty>(FActorTickFunction::StaticStruct(),TEXT("bCanEverTick"));FString Export;
        if(!Values||!Fields||!P||P->ArrayDim!=1||!Fields->HasTypedField<EJson::Object>(TEXT("bCanEverTick"))||!P->ExportText_Direct(Export,P->ContainerPtrToValuePtr<void>(&ExpectedField),P->ContainerPtrToValuePtr<void>(&ExpectedField),C.CDO,PPF_None))return Fail(R,TEXT("blueprint_tick_compiler_prediction_unobservable"),TEXT("Exact reflected eligibility field required before compile"));
        Values->SetBoolField(TEXT("can_ever_tick"),ExpectedCanTick);Fields->GetObjectField(TEXT("bCanEverTick"))->SetArrayField(TEXT("values"),{MakeShared<FJsonValueString>(Export)});
        ExpectedTick.ActorTickValues=Values.ToSharedRef();ExpectedTick.ActorTickFields=Fields.ToSharedRef();
    }
    R.Data->SetObjectField(TEXT("postcompile_tick_expected"), ExpectedTick.ActorTickValues);
    R.Stage = TEXT("compile_blueprint"); R.Data->SetBoolField(TEXT("after_configuration_source_preserved"), R.Data->GetBoolField(TEXT("source_preserved"))); R.Data->SetBoolField(TEXT("after_configuration_unselected_state_preserved"), R.Data->GetBoolField(TEXT("unselected_state_preserved"))); ClearFinal(R);
    if (!CurrentDerivedTarget(C, R)) { return false; }
    R.bCompileAttempted = true; Count(R, TEXT("full_compile_attempts")); auto Result = Cubeless::UE58::CompileBlueprint(C.Blueprint->GetPathName(), false, false); R.Data->SetObjectField(TEXT("compile_result"), Result);
    bool Passed = false; if (Result->TryGetBoolField(TEXT("compiled"), Passed)) { R.Compiled = Passed; }
    bool LinkedCall=false;R.Data->TryGetBoolField(TEXT("raw_tick_linked_call_profile"),LinkedCall);
    if(LinkedCall){bool Known=true,Unknown=false;Result->TryGetBoolField(TEXT("change_state_known"),Known);Result->TryGetBoolField(TEXT("outcome_unknown"),Unknown);if(!Known||Unknown||!R.Compiled.IsSet()||!Passed)return Fail(R,TEXT("blueprint_tick_refresh_compile_failed_or_unobservable"),TEXT("Returned compiler prefix retained; failure/missing/unknown status stops before target/source reacquisition or save, without retry."),true);}
    // Only the selected child may be reinstanced. Keep the original parent
    // roles alive and verify them before adopting current child pointers.
    if(C.NativeAnim){C.NativeAnim->Rebinding=true;}
    if (!DerivedBlueprintAsset(C, R)) { return false; }
    FContext Current; if (!Prepare(C.Blueprint->GetPathName(), C.SelectedName, C.bSkeletal, Current, R)) { if(!C.NativeAnim){Capture(C, After, R, TEXT("after_observation"));}else{R.Data->SetBoolField(TEXT("native_anim_postcompile_capture_skipped"),true);} return Fail(R, TEXT("blueprint_defaults_postcompile_context_unobservable"), TEXT("Compile returned but current CDO/local-template context cannot be admitted; actual returned prefix retained, no save."), true); }
    Current.Assets = MoveTemp(C.Assets);
    if(C.NativeAnim){Current.NativeAnim=MoveTemp(C.NativeAnim);Current.Editor=C.Editor;Current.World=C.World;}
    C = MoveTemp(Current);if(C.NativeAnim){C.NativeAnim->Rebinding=false;if(!FreezeNativeAnimTarget(C,R)){return false;}}
    if (!Capture(C, After, R, TEXT("after_compile_before_tick_restore"))) { return Fail(R, TEXT("blueprint_defaults_postcompile_unobservable"), TEXT("Compile attempted once, current bounded prefix incomplete; save suppressed."), true); }
    if (!R.Compiled.IsSet()) { return Fail(R, TEXT("blueprint_defaults_compile_outcome_unobservable"), TEXT("Compiler result missing typed compiled status."), true); }
    R.Data->SetObjectField(TEXT("after_observation"), Observation(After));
    if(RawTick){R.Data->SetObjectField(TEXT("actual_after_compile"),After.ActorTickValues);R.Data->SetBoolField(TEXT("requested_tick_matches_actual_compile"),Equal(After.ActorTickValues,R.Data->GetObjectField(TEXT("after")).ToSharedRef()));}
    R.Data->SetStringField(TEXT("after_compile_before_tick_restore_scope"), TEXT("raw current compiler result; historical phase key retained, no tick restoration/notification/dirty-mark route exists; original requested after remains separate from actual compiler eligibility"));
    if (!Passed) { Fences(Before, After, R); return Fail(R, TEXT("blueprint_defaults_compile_failed"), TEXT("Compiler reported failure; raw tick state retained and no restoration/save.")); }
    if (!Fences(Before, After, R)) { return false; }
    const bool TickPassed = Equal(ExpectedTick.ActorTickValues, After.ActorTickValues) && Equal(ExpectedTick.ActorTickFields, After.ActorTickFields); R.Data->SetBoolField(TEXT("postcompile_actor_tick_preserved"), TickPassed);
    if (!TickPassed) { return Fail(R, TEXT("blueprint_defaults_postcompile_tick_readback_failed"), TEXT("Tick defaults and other reflected fields must match the configured snapshot or the separately predicted raw parent-reset CanEverTick; no restore/value masking/save after mismatch.")); }
    if (Passed && C.bSkeletal && !After.State->GetObjectField(TEXT("selected_scs_cdo_member"))->GetBoolField(TEXT("present"))) { return Fail(R, TEXT("blueprint_skeletal_compiled_member_missing"), TEXT("Full compile reported success but the exact selected own SkeletalMesh object member is absent; no save.")); }
    return Passed || Fail(R, TEXT("blueprint_defaults_compile_failed"), TEXT("Compiler reported failure; retained actual readback and diagnostics, no save."));
}
bool OptionalScalar(const FString& Input, EJson Expected, TSharedPtr<FJsonValue>& V, FResult& R)
{
    if (Input.Len() < 1 || Input.Len() > 4096) { return Fail(R, TEXT("blueprint_defaults_optional_value_invalid"), TEXT("Optional original value must be bounded JSON scalar or null.")); }
    TArray<TSharedPtr<FJsonValue>> Values; if (!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(TEXT("[") + Input + TEXT("]")), Values) || Values.Num() != 1 || !Values[0] || (Values[0]->Type != EJson::Null && Values[0]->Type != Expected)) { return Fail(R, TEXT("blueprint_defaults_optional_value_invalid"), TEXT("Optional original string/bool type or null required; no truthy coercion.")); } V = Values[0]; return true;
}
template<typename T> bool ParseNames(const FString& S, const TArray<TPair<FString, T>>& Values, T& Out)
{ for (const auto& P : Values) { if (S.Equals(P.Key, ESearchCase::IgnoreCase)) { Out = P.Value; return true; } } return false; }
const TArray<TPair<FString, ECollisionEnabled::Type>> CollisionNames = { {TEXT("NoCollision"), ECollisionEnabled::NoCollision}, {TEXT("QueryOnly"), ECollisionEnabled::QueryOnly}, {TEXT("PhysicsOnly"), ECollisionEnabled::PhysicsOnly}, {TEXT("QueryAndPhysics"), ECollisionEnabled::QueryAndPhysics} };
const TArray<TPair<FString, ECollisionResponse>> ResponseNames = { {TEXT("Ignore"), ECR_Ignore}, {TEXT("Overlap"), ECR_Overlap}, {TEXT("Block"), ECR_Block} };
const TArray<TPair<FString, EVisibilityBasedAnimTickOption>> VisibilityNames = { {TEXT("AlwaysTickPoseAndRefreshBones"), EVisibilityBasedAnimTickOption::AlwaysTickPoseAndRefreshBones}, {TEXT("AlwaysTickPose"), EVisibilityBasedAnimTickOption::AlwaysTickPose}, {TEXT("OnlyTickMontagesAndRefreshBonesWhenPlayingMontages"), EVisibilityBasedAnimTickOption::OnlyTickMontagesAndRefreshBonesWhenPlayingMontages}, {TEXT("OnlyTickMontagesWhenNotRendered"), EVisibilityBasedAnimTickOption::OnlyTickMontagesWhenNotRendered}, {TEXT("OnlyTickPoseWhenRendered"), EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered} };
template<typename T> FString EnumName(T Value, const TArray<TPair<FString, T>>& Names)
{ for (const auto& P : Names) { if (P.Value == Value) { return P.Key; } } return TEXT("Unknown"); }
bool LoadAssetReference(const FString& Requested, UClass* Required, FContext& C, UObject*& Asset, FResult& R, bool Class = false)
{
    const FString P = Path(Requested); if (!AssetPath(P)) { return Fail(R, TEXT("blueprint_defaults_asset_reference_invalid"), TEXT("Bounded Game/Engine runtime asset path required; plugin/editor/custom packages remain incomplete.")); }
    auto Receipt = MakeShared<FJsonObject>(); Receipt->SetStringField(TEXT("requested"), Requested); Receipt->SetStringField(TEXT("object_path"), P); Receipt->SetBoolField(TEXT("already_loaded"), FindObject<UObject>(nullptr, *P) != nullptr);
    auto Rows = R.Data->GetArrayField(TEXT("asset_load_calls")); Rows.Add(MakeShared<FJsonValueObject>(Receipt)); R.Data->SetArrayField(TEXT("asset_load_calls"), Rows); Count(R, TEXT("asset_load_count"));
    if (!CurrentDerivedTarget(C, R)) { return false; }
    Asset = LoadObject<UObject>(nullptr, *P);
    if (!CurrentDerivedTarget(C, R)) { Receipt->SetBoolField(TEXT("returned_identity_skipped_after_unsafe_callback"), true); return false; }
    if (C.GeneratedParents && Loaded(Asset)) { C.Assets.Emplace(Asset); }
    Receipt->SetObjectField(TEXT("returned"), Identity(Asset)); Receipt->SetBoolField(TEXT("implicit_load_postload_effects_measured"), false);
    UClass* AC = Class ? Cast<UClass>(Asset) : nullptr;
    if (!Loaded(Asset) || Asset->GetPathName() != P || Asset->GetOutermost()->IsDirty() || Asset->HasAnyFlags(RF_Transient)
        || (!Class && Asset->GetClass() != Required) || (Class && (!AC || !AC->IsChildOf(UAnimInstance::StaticClass()) || AC->HasAnyClassFlags(CLASS_Abstract | CLASS_Deprecated | CLASS_NewerVersionExists) || !Loaded(AC->ClassGeneratedBy) || AC->ClassGeneratedBy->GetOutermost() != AC->GetOutermost())))
    { return Fail(R, TEXT("blueprint_defaults_asset_reference_unsupported"), TEXT("Exact loaded clean persisted native source or generated AnimInstance class required; loaded prefix receipt retained.")); }
    if (auto* Mesh = Cast<USkeletalMesh>(Asset); Mesh && Mesh->IsCompiling()) { return Fail(R, TEXT("blueprint_defaults_mesh_compile_pending"), TEXT("Source skeletal mesh is still compiling; no target Modify.")); }
    auto D = MakeShared<FJsonObject>(); if (!Disk(Asset->GetOutermost()->GetName(), D) || !D->GetBoolField(TEXT("exists")) || D->GetNumberField(TEXT("bytes")) <= 0) { Receipt->SetObjectField(TEXT("disk"), D); return Fail(R, TEXT("blueprint_defaults_source_not_persisted"), TEXT("Selected external asset source must be persisted before target mutation.")); } Receipt->SetObjectField(TEXT("disk"), D);
    auto Audit = ReadPackageDependencies({ Asset->GetOutermost()->GetName() }, true, 1024); Receipt->SetObjectField(TEXT("dependency_audit"), Audit); bool Passed = false;
    if (!Audit->TryGetBoolField(TEXT("passed"), Passed) || !Passed) { return Fail(R, TEXT("blueprint_defaults_source_dependency_unverified"), TEXT("Current saved source dependency metadata incomplete/dirty/tool-owned; no target Modify.")); }
    if (!CurrentDerivedTarget(C, R)) { return false; }
    if (!C.GeneratedParents) { C.Assets.Emplace(Asset); } return true;
}
bool LoadReference(const FString& Requested,UClass* Required,FContext& C,UObject*& Asset,FResult& R,bool Class=false)
{
    if(!Class){return LoadAssetReference(Requested,Required,C,Asset,R,false);}
    FString P=Path(Requested);const bool NativePath=Class&&P.StartsWith(TEXT("/Script/Engine."),ESearchCase::CaseSensitive);
    if(!AssetPath(P)&&!NativePath){return Fail(R,TEXT("blueprint_defaults_asset_reference_invalid"),TEXT("Bounded Game/Engine runtime asset path or exact Engine native animation class required."));}
    if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)||P.Contains(TEXT(":"))||!CleanText(P)){return Fail(R,TEXT("blueprint_defaults_asset_reference_invalid"),TEXT("Bounded exact class/asset object path required."));}
    if(NativePath)
    {
        C.NativeAnim=MakeUnique<FNativeAnimSource>();C.Editor=GEditor;C.World=GEditor?GEditor->GetEditorWorldContext().World():nullptr;
        if(!FreezeNativeAnimTarget(C,R)){return false;}
    }
    FObservation BeforeNativeLoad;if(NativePath&&!Capture(C,BeforeNativeLoad,R,TEXT("before_native_anim_load"))){return Fail(R,TEXT("blueprint_native_anim_before_load_unobservable"),TEXT("Complete target/native/default/disk/dirty baseline required before native class load."));}
    TStrongObjectPtr<UObject> Returned;TSharedPtr<FJsonObject> LastReceipt;
    const auto Load=[&](const FString& ActualPath)->bool
    {
        auto Receipt=MakeShared<FJsonObject>();LastReceipt=Receipt;Receipt->SetStringField(TEXT("requested"),Requested);Receipt->SetStringField(TEXT("object_path"),ActualPath);Receipt->SetBoolField(TEXT("already_loaded"),FindObject<UObject>(nullptr,*ActualPath)!=nullptr);
        Receipt->SetBoolField(TEXT("class_load"),Class);Receipt->SetBoolField(TEXT("implicit_load_postload_effects_measured"),false);
        auto Rows=R.Data->GetArrayField(TEXT("asset_load_calls"));Rows.Add(MakeShared<FJsonValueObject>(Receipt));R.Data->SetArrayField(TEXT("asset_load_calls"),Rows);Count(R,TEXT("asset_load_count"));
        if(!CurrentDerivedTarget(C,R)){return false;}
        Asset=Class?static_cast<UObject*>(LoadObject<UClass>(nullptr,*ActualPath)):LoadObject<UObject>(nullptr,*ActualPath);
        Receipt->SetBoolField(TEXT("returned_nonnull"),Asset!=nullptr);
        if(C.NativeAnim&&!ParentSafe(C)){return Fail(R,TEXT("blueprint_native_anim_load_context_changed"),TEXT("Source loader returned into unsafe Editor context; returned object not observed."),true);}
        if(Loaded(Asset)){Returned.Reset(Asset);}
        if(!CurrentDerivedTarget(C,R)){Receipt->SetBoolField(TEXT("returned_identity_skipped_after_unsafe_callback"),true);return false;}
        Receipt->SetObjectField(TEXT("returned"),Identity(Asset));return true;
    };
    if(!Load(P)){return false;}
    if(Class&&!Asset&&!P.EndsWith(TEXT("_C")))
    {
        P+=TEXT("_C");if(P.Len()>=NAME_SIZE||!FPackageName::IsValidObjectPath(P)){return Fail(R,TEXT("blueprint_defaults_class_fallback_invalid"),TEXT("Expanded class fallback exceeds the pre-load name/path bound."));}
        if(!Load(P)){return false;}
    }
    UClass* AC=Class?Cast<UClass>(Asset):nullptr;
    if(NativePath)
    {
        // No source .uasset exists for a compiled-in class. Target/source baseline
        // is fenced separately from the post-load native class/CDO proof.
        FObservation AfterLoad;if(!Capture(C,AfterLoad,R,TEXT("after_native_anim_load"))){return Fail(R,TEXT("blueprint_native_anim_load_unobservable"),TEXT("Source load returned without complete target/source/disk/dirty observation."),true);}
        bool DirtyOK=true;for(const auto& Q:BeforeNativeLoad.OtherDirty->Values){const auto Now=AfterLoad.OtherDirty->TryGetField(Q.Key.ToView());DirtyOK&=Now&&Now->Type==EJson::Boolean&&Now->AsBool()==Q.Value->AsBool();}
        for(const auto& Q:AfterLoad.OtherDirty->Values){if(!BeforeNativeLoad.OtherDirty->HasField(Q.Key.ToView())){DirtyOK&=Q.Value->Type==EJson::Boolean&&!Q.Value->AsBool();}}
        const bool Preserved=Text(BeforeNativeLoad.State).Equals(Text(AfterLoad.State),ESearchCase::CaseSensitive)&&Text(BeforeNativeLoad.NativeSources).Equals(Text(AfterLoad.NativeSources),ESearchCase::CaseSensitive)
            &&Text(BeforeNativeLoad.References).Equals(Text(AfterLoad.References),ESearchCase::CaseSensitive)&&Text(BeforeNativeLoad.DiskState).Equals(Text(AfterLoad.DiskState),ESearchCase::CaseSensitive)&&DirtyOK;
        R.Data->SetBoolField(TEXT("native_anim_load_target_preserved"),Preserved);
        if(!Preserved){return Fail(R,TEXT("blueprint_native_anim_load_state_changed"),TEXT("Loader changed selected/native/source/disk/unrelated dirty baseline; no target Modify or retry."),true);}
        if(!Loaded(AC)||!AC->GetPathName().Equals(P,ESearchCase::CaseSensitive)){return Fail(R,TEXT("blueprint_native_anim_class_unsupported"),TEXT("Exact native class path did not resolve; no target Modify."));}
        if(!BindNativeAnim(C,AC,R)){return false;}return true;
    }
    if(C.GeneratedParents&&Loaded(Asset)){C.Assets.Emplace(Asset);}
    if(!Loaded(Asset)||Asset->GetPathName()!=P||Asset->GetOutermost()->IsDirty()||Asset->HasAnyFlags(RF_Transient)
        ||(!Class&&Asset->GetClass()!=Required)||(Class&&(!AC||!AC->IsChildOf(UAnimInstance::StaticClass())||AC->HasAnyClassFlags(CLASS_Abstract|CLASS_Deprecated|CLASS_NewerVersionExists)||!Loaded(AC->ClassGeneratedBy)||AC->ClassGeneratedBy->GetOutermost()!=AC->GetOutermost())))
    {return Fail(R,TEXT("blueprint_defaults_asset_reference_unsupported"),TEXT("Exact loaded clean persisted native asset or generated AnimInstance class required; load prefix retained."));}
    if(auto* Mesh=Cast<USkeletalMesh>(Asset);Mesh&&Mesh->IsCompiling()){return Fail(R,TEXT("blueprint_defaults_mesh_compile_pending"),TEXT("Source skeletal mesh is still compiling; no target Modify."));}
    auto D=MakeShared<FJsonObject>();if(!Disk(Asset->GetOutermost()->GetName(),D)||!D->GetBoolField(TEXT("exists"))||D->GetNumberField(TEXT("bytes"))<=0){LastReceipt->SetObjectField(TEXT("disk"),D);return Fail(R,TEXT("blueprint_defaults_source_not_persisted"),TEXT("Selected external asset source must be persisted before target mutation."));}LastReceipt->SetObjectField(TEXT("disk"),D);
    auto Audit=ReadPackageDependencies({Asset->GetOutermost()->GetName()},true,1024);LastReceipt->SetObjectField(TEXT("dependency_audit"),Audit);bool Passed=false;
    if(!Audit->TryGetBoolField(TEXT("passed"),Passed)||!Passed){return Fail(R,TEXT("blueprint_defaults_source_dependency_unverified"),TEXT("Current saved source dependency metadata incomplete/dirty/tool-owned; no target Modify."));}
    if(!CurrentDerivedTarget(C,R)){return false;}if(!C.GeneratedParents){C.Assets.Emplace(Asset);}return true;
}

void LegacySkeletal(FContext& C, const TSharedRef<FJsonObject>& Values, FResult& R)
{
    for (const TCHAR* K : { TEXT("skeletal_mesh"), TEXT("anim_class"), TEXT("physics_asset"), TEXT("simulate_physics"), TEXT("blend_physics"), TEXT("enable_gravity") }) { R.Data->SetField(K, Values->TryGetField(K)); }
    R.Data->SetStringField(TEXT("component_name"), C.SelectedName); R.Data->SetStringField(TEXT("collision_enabled"), EnumName(C.Skeletal->GetCollisionEnabled(), CollisionNames));
    const ECollisionResponse First = C.Skeletal->GetCollisionResponseToChannel(ECC_WorldStatic); bool Uniform = true; for (int32 I = static_cast<int32>(ECC_WorldStatic) + 1; I < static_cast<int32>(ECC_MAX); ++I) { Uniform &= C.Skeletal->GetCollisionResponseToChannel(static_cast<ECollisionChannel>(I)) == First; }
    R.Data->SetStringField(TEXT("collision_response"), Uniform ? EnumName(First, ResponseNames) : TEXT("Mixed")); R.Data->SetStringField(TEXT("visibility_based_anim_tick_option"), EnumName(C.Skeletal->VisibilityBasedAnimTickOption, VisibilityNames)); R.Data->SetBoolField(TEXT("dirty_after"), C.Blueprint->GetOutermost()->IsDirty());
}
}
static FString SetActorTickSettingsWork(const FString& BlueprintPath, double TickHz, double TickInterval,
    bool bCanEverTick, bool bStartWithTickEnabled, bool bAllowTickOnDedicatedServer,
    bool bTickEvenWhenPaused, bool bDryRun, bool bCompile, bool bSave,const FString* SettingsJson)
{
    FResult R;TSharedPtr<FJsonObject> Raw;bool Refresh=false;
    const auto RawBool=[&](const TCHAR* Key,bool Default){bool Value=Default;return Raw&&Raw->TryGetBoolField(Key,Value)?Value:Default;};
    if(SettingsJson)
    {
        bool Nul=false;for(TCHAR Char:*SettingsJson)Nul|=Char==0;
        if(Nul||SettingsJson->Len()>131072||!FJsonSerializer::Deserialize(TJsonReaderFactory<>::Create(*SettingsJson),Raw)||!Raw)
        {Init(R,TEXT("set_blueprint_actor_tick_settings"),false,false);Fail(R,TEXT("blueprint_tick_raw_settings_invalid"),TEXT("Private bounded original settings object required; no loader or writer dispatched"));return Encode(R);}
        bDryRun=RawBool(TEXT("dry_run"),true);bCompile=RawBool(TEXT("compile"),true);bSave=RawBool(TEXT("save"),true);Refresh=RawBool(TEXT("refresh_nodes"),false);
    }
    Init(R, TEXT("set_blueprint_actor_tick_settings"), !bDryRun && bCompile, !bDryRun && bCompile && bSave);
    if(Raw){R.Data->SetObjectField(TEXT("raw_settings"),Raw);R.Data->SetBoolField(TEXT("raw_tick_presence_contract"),true);R.Data->SetStringField(TEXT("raw_tick_scope"),TEXT("Original JSON TryGetBool/TryGetNumber coercion with current CDO fallback for omitted/unreadable settings, frequency then explicit interval float32/clamp0. Existing parent compiler eligibility/lifetime/source/current protection and compile/save core are retained. Actual refresh admits empty graphs/exact native AActor default Events, default unlinked UCS FunctionEntry1 and fixed-signature static native Engine BlueprintFunctionLibrary CallFunction nodes; authored logical pins/defaults/reciprocal links and native signature/metadata sources are checked. Raw no-compile uses the requested CDO flag; full compile checks known parent-reset CanEverTick and reports requested versus actual separately. Parent-false active/ambiguous Tick, generated parents, latent/member/interface/dynamic-signature/custom-node refresh profiles remain incomplete; Named10 unchanged."));}
    R.Data->SetBoolField(TEXT("dry_run"), bDryRun); R.Data->SetBoolField(TEXT("caller_compile_requested"), bCompile); R.Data->SetBoolField(TEXT("caller_save_requested"), bSave); R.Data->SetBoolField(TEXT("refresh_nodes_requested"), Refresh);
    if (!Raw&&(!FMath::IsFinite(TickHz) || !FMath::IsFinite(TickInterval) || FMath::Abs(TickHz) > static_cast<double>(MAX_flt) || FMath::Abs(TickInterval) > static_cast<double>(MAX_flt))) { Fail(R, TEXT("blueprint_tick_numeric_inputs_invalid"), TEXT("Finite native-float-representable original tick values required before resolving or mutation.")); return Encode(R); }
    FContext C; if (!Prepare(BlueprintPath, FString(), false, C, R)) { return Encode(R); } FObservation Before;
    if (!Capture(C, Before, R, TEXT("before_observation"))) { Fail(R, TEXT("blueprint_defaults_preflight_unobservable"), TEXT("Complete initial CDO/native/source/graph/disk/dirty observation required before Modify.")); return Encode(R); }
    auto Desired = Tick(C.CDO); float Interval = C.CDO->PrimaryActorTick.TickInterval;
    if(Raw)
    {
        // BlueprintCommands.cpp1678: missing/unreadable frequency does not invent
        // Named's 60Hz; an explicit negative interval overrides Hz then clamps0.
        double Hz=0,ExplicitInterval=0;const bool HasHz=Raw->TryGetNumberField(TEXT("tick_hz"),Hz),HasInterval=Raw->TryGetNumberField(TEXT("tick_interval"),ExplicitInterval);
        if((HasHz&&Hz>0&&(!FMath::IsFinite(Hz)||Hz>static_cast<double>(MAX_flt)))||(HasInterval&&(!FMath::IsFinite(ExplicitInterval)||FMath::Abs(ExplicitInterval)>static_cast<double>(MAX_flt))))
        {Fail(R,TEXT("blueprint_tick_raw_numeric_scope_pending"),TEXT("Original nonfinite/overflowing float32 conversions remain incomplete; current CDO observed, no Modify/compile/save"));return Encode(R);}
        if(HasHz&&Hz>0)Interval=1.0f/FMath::Max(0.001f,static_cast<float>(Hz));
        if(HasInterval)Interval=static_cast<float>(ExplicitInterval);Interval=FMath::Max(0.0f,Interval);
        bCanEverTick=RawBool(TEXT("can_ever_tick"),C.CDO->PrimaryActorTick.bCanEverTick);
        bStartWithTickEnabled=RawBool(TEXT("start_with_tick_enabled"),C.CDO->PrimaryActorTick.bStartWithTickEnabled);
        bAllowTickOnDedicatedServer=RawBool(TEXT("allow_tick_on_dedicated_server"),C.CDO->PrimaryActorTick.bAllowTickOnDedicatedServer);
        bTickEvenWhenPaused=RawBool(TEXT("tick_even_when_paused"),C.CDO->PrimaryActorTick.bTickEvenWhenPaused);
        auto Presence=MakeShared<FJsonObject>();for(const TCHAR* Key:{TEXT("tick_hz"),TEXT("tick_interval"),TEXT("can_ever_tick"),TEXT("start_with_tick_enabled"),TEXT("allow_tick_on_dedicated_server"),TEXT("tick_even_when_paused"),TEXT("dry_run"),TEXT("compile"),TEXT("save"),TEXT("refresh_nodes")})Presence->SetBoolField(Key,Raw->HasField(Key));R.Data->SetObjectField(TEXT("raw_tick_field_presence"),Presence);
        R.Data->SetBoolField(TEXT("raw_tick_hz_number_read"),HasHz);R.Data->SetBoolField(TEXT("raw_tick_interval_number_read"),HasInterval);
    }
    else
    {if (TickHz > 0) { Interval = 1.0f / FMath::Max(0.001f, static_cast<float>(TickHz)); } if (TickInterval >= 0) { Interval = static_cast<float>(TickInterval); } Interval = FMath::Max(0.0f, Interval);}
    if (!FMath::IsFinite(Interval) || (Interval > 0 && !FMath::IsFinite(1.0f / Interval))) { Fail(R, TEXT("blueprint_tick_numeric_inputs_invalid"), TEXT("Derived interval/frequency must be finite in the original float representation.")); return Encode(R); }
    Desired->SetBoolField(TEXT("can_ever_tick"), bCanEverTick); Desired->SetBoolField(TEXT("start_with_tick_enabled"), bStartWithTickEnabled); Desired->SetBoolField(TEXT("allow_tick_on_dedicated_server"), bAllowTickOnDedicatedServer); Desired->SetBoolField(TEXT("tick_even_when_paused"), bTickEvenWhenPaused); Desired->SetNumberField(TEXT("tick_interval"), Interval); Desired->SetNumberField(TEXT("tick_hz"), Interval > 0 ? 1.0f / Interval : 0.0f);
    R.Data->SetObjectField(TEXT("before"), Before.State->GetObjectField(TEXT("selected_values"))); R.Data->SetObjectField(TEXT("after"), Desired);
    R.Data->SetBoolField(TEXT("can_apply"), false);
    if(Raw){R.Data->SetBoolField(TEXT("after_is_original_requested_settings"),true);R.Data->SetStringField(TEXT("tick_compile_result_scope"),TEXT("Original after is the requested configuration. Actual compile_result.compiled/validation_pass and actual_after_compile describe compiler outcome, independent of requested eligibility; success requires actual preservation checks, not equality of the compiler-owned eligibility flag to request."));}
    TUniquePtr<FTickRefreshSources> RefreshSource;
    if(Raw&&Refresh&&!bDryRun&&bCompile)
    {
        if(!RefreshTickAdmission(C,R))return Encode(R);
        RefreshSource=MakeUnique<FTickRefreshSources>();if(!RefreshSource->Bind(C)){Fail(R,TEXT("blueprint_tick_refresh_source_unobservable"),TEXT("Resident exact K2 schema/native Event and fixed native call signature/CDO sources required; no CDO creation or writer"));return Encode(R);}
    }
    if (!CompilerTickPolicy(C, bCanEverTick, R, Raw.IsValid(), !bDryRun&&bCompile)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("can_apply"), true);
    if (bDryRun)
    {
        FObservation Current; if (!Capture(C, Current, R, TEXT("after_observation"))) { Fail(R, TEXT("blueprint_tick_dry_run_observation_failed"), TEXT("No writes dispatched; repeated preflight observation is incomplete."), true); return Encode(R); }
        if (!Fences(Before, Current, R) || !Equal(Before.State, Current.State)) { Fail(R, TEXT("blueprint_tick_dry_run_observation_failed"), TEXT("No writes dispatched; actual repeated complete observation differs.")); return Encode(R); }
        R.Data->SetBoolField(TEXT("independent_readback_passed"), true); R.Data->SetStringField(TEXT("write_scope"), TEXT("dry_run_no_asset_mutation")); R.Stage = TEXT("complete"); R.bSuccess = true; R.bPost = true; return Encode(R);
    }
    if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    R.Stage = TEXT("set_actor_tick_defaults"); R.bAttempted = true; C.Blueprint->Modify(); Count(R, TEXT("modify_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } C.CDO->Modify(); Count(R, TEXT("modify_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    FActorTickFunction& T = C.CDO->PrimaryActorTick; T.bCanEverTick = bCanEverTick; T.bStartWithTickEnabled = bStartWithTickEnabled; T.bAllowTickOnDedicatedServer = bAllowTickOnDedicatedServer; T.bTickEvenWhenPaused = bTickEvenWhenPaused; T.TickInterval = Interval; Count(R, TEXT("setter_calls"));
    FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get()); Count(R, TEXT("mark_blueprint_modified_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } FObservation Configured;
    if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("blueprint_tick_configuration_unobservable"), TEXT("Actual write prefix incomplete; compilation/save suppressed."), true); return Encode(R); }
    if (!Fences(Before, Configured, R) || !Equal(Desired, Configured.State->GetObjectField(TEXT("selected_values")).ToSharedRef())) { if (R.Code.IsEmpty()) { Fail(R, TEXT("blueprint_tick_readback_failed"), TEXT("Actual dedicated Actor-CDO tick readback differs; no compilation/save.")); } return Encode(R); }
    R.Data->SetBoolField(TEXT("after_configuration_readback_passed"), true); R.Data->SetStringField(TEXT("write_scope"), TEXT("blueprint_actor_cdo_primary_tick_defaults")); FObservation After = Configured;
    if (R.bCompileRequested&&Raw&&Refresh)
    {
        if(!RefreshTickNodes(C,Before,After,R,*RefreshSource))return Encode(R);
        if(!Equal(Desired,After.ActorTickValues)){Fail(R,TEXT("blueprint_tick_refresh_selected_values_changed"),TEXT("Refresh changed configured tick values before full compile; actual prefix retained, no compile/save"));return Encode(R);}
    }
    if (R.bCompileRequested && !Compile(C, Before, After, R, Raw.IsValid())) { return Encode(R); }
    if(RefreshSource&&!RefreshSource->Current()){Fail(R,TEXT("blueprint_tick_refresh_source_changed_after_compile"),TEXT("Retained schema/native signature changed during full compile; actual compile prefix retained, no save"),true);return Encode(R);}
    const TSharedRef<FJsonObject> ExpectedActual=Raw&&R.bCompileRequested?R.Data->GetObjectField(TEXT("postcompile_tick_expected")).ToSharedRef():Desired;
    if (!Equal(ExpectedActual, After.State->GetObjectField(TEXT("selected_values")).ToSharedRef())) { Fail(R, TEXT("blueprint_tick_postcompile_readback_failed"), TEXT("Current generated CDO tick defaults differ after compilation; no save.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("independent_readback_passed"), true); if (!Save(C, After, R)) { return Encode(R); }
    if(RefreshSource&&!RefreshSource->Current()){Fail(R,TEXT("blueprint_tick_refresh_source_changed_after_save"),TEXT("Retained schema/native signature changed across save; actual save receipt retained, no retry"),true);return Encode(R);}
    R.Data->SetBoolField(TEXT("saved"), R.bSaved); R.Stage = TEXT("complete"); R.bSuccess = true; R.bPost = true; return Encode(R);
}

FString SetActorTickSettings(const FString& BlueprintPath,double TickHz,double TickInterval,
    bool bCanEverTick,bool bStartWithTickEnabled,bool bAllowTickOnDedicatedServer,bool bTickEvenWhenPaused,bool bDryRun,bool bCompile,bool bSave)
{return SetActorTickSettingsWork(BlueprintPath,TickHz,TickInterval,bCanEverTick,bStartWithTickEnabled,bAllowTickOnDedicatedServer,bTickEvenWhenPaused,bDryRun,bCompile,bSave,nullptr);}
FString SetActorTickSettingsRaw(const FString& BlueprintPath,const FString& SettingsJson)
{return SetActorTickSettingsWork(BlueprintPath,0,-1,false,false,false,false,true,true,true,&SettingsJson);}

FString SetSkeletalAnimDefaults(const FString& BlueprintPath, const FString& ComponentName,
    const FString& SkeletalMesh, const FString& AnimClass, const FString& PhysicsAssetJson,
    const FString& SimulatePhysicsJson, const FString& CollisionEnabledJson,
    const FString& CollisionResponseJson, const FString& EnableGravityJson,
    const FString& VisibilityBasedAnimTickOptionJson, bool bCompile, bool bSave)
{
    FResult R; Init(R, TEXT("set_skeletal_mesh_component_anim_defaults"), bCompile, bSave);
    R.Data->SetStringField(TEXT("component_name"), ComponentName); R.Data->SetBoolField(TEXT("collision_profile_invalidation_expected"), false);
    TSharedPtr<FJsonValue> Physics, Simulate, Collision, Response, Gravity, Visibility;
    if (!OptionalScalar(PhysicsAssetJson, EJson::String, Physics, R) || !OptionalScalar(SimulatePhysicsJson, EJson::Boolean, Simulate, R) || !OptionalScalar(CollisionEnabledJson, EJson::String, Collision, R)
        || !OptionalScalar(CollisionResponseJson, EJson::String, Response, R) || !OptionalScalar(EnableGravityJson, EJson::Boolean, Gravity, R) || !OptionalScalar(VisibilityBasedAnimTickOptionJson, EJson::String, Visibility, R)) { return Encode(R); }
    ECollisionEnabled::Type CollisionValue = ECollisionEnabled::NoCollision; ECollisionResponse ResponseValue = ECR_Ignore; EVisibilityBasedAnimTickOption VisibilityValue = EVisibilityBasedAnimTickOption::OnlyTickPoseWhenRendered;
    if ((Collision->Type != EJson::Null && !ParseNames(Collision->AsString(), CollisionNames, CollisionValue)) || (Response->Type != EJson::Null && !ParseNames(Response->AsString(), ResponseNames, ResponseValue)) || (Visibility->Type != EJson::Null && !ParseNames(Visibility->AsString(), VisibilityNames, VisibilityValue))) { Fail(R, TEXT("blueprint_skeletal_option_invalid"), TEXT("Original exact case-insensitive collision/response/visibility enum names required; no trim/enum widening.")); return Encode(R); }
    if (SkeletalMesh.IsEmpty() && AnimClass.IsEmpty() && Physics->Type == EJson::Null && Simulate->Type == EJson::Null && Collision->Type == EJson::Null && Response->Type == EJson::Null && Gravity->Type == EJson::Null && Visibility->Type == EJson::Null) { Fail(R, TEXT("blueprint_skeletal_no_defaults"), TEXT("No SkeletalMeshComponent defaults were provided.")); return Encode(R); }
    if(NativeAnimCurrent&&NativeAnimCurrent->NativeAnim){Fail(R,TEXT("blueprint_native_anim_reentry"),TEXT("A native animation source operation is active; nested defaults work rejected before load/SDK."));return Encode(R);}
    FContext C; if (!Prepare(BlueprintPath, ComponentName, true, C, R)) { return Encode(R); }FNativeAnimReadScope NativeRead(C,R);
    UObject* MeshAsset = nullptr; UObject* PhysicsAsset = nullptr; UObject* AnimationClass = nullptr;
    // Preserve the original ordered explicit resolutions before any target Modify.
    if (!SkeletalMesh.IsEmpty() && !LoadReference(SkeletalMesh, USkeletalMesh::StaticClass(), C, MeshAsset, R)) { return Encode(R); }
    if (Physics->Type != EJson::Null && !Physics->AsString().IsEmpty() && !LoadReference(Physics->AsString(), UPhysicsAsset::StaticClass(), C, PhysicsAsset, R)) { return Encode(R); }
    if (!AnimClass.IsEmpty())
    {
        if (!LoadReference(AnimClass, UAnimInstance::StaticClass(), C, AnimationClass, R, true)) { return Encode(R); }
    }
    FObservation Before; if (!Capture(C, Before, R, TEXT("before"))) { Fail(R, TEXT("blueprint_defaults_preflight_unobservable"), TEXT("Complete own template/graph/CDO/native/explicit-source/disk/dirty observation required before Modify.")); return Encode(R); }
    if ((bCompile || bSave) && !CompilerTickPolicy(C, C.CDO->PrimaryActorTick.bCanEverTick, R)) { return Encode(R); }
    FString CollisionProfileExport;
    if (Collision->Type != EJson::Null || Response->Type != EJson::Null)
    {
        FStructProperty* Body = FindFProperty<FStructProperty>(C.Skeletal->GetClass(), TEXT("BodyInstance")); FNameProperty* Profile = Body && Loaded(Body->Struct) ? FindFProperty<FNameProperty>(Body->Struct, TEXT("CollisionProfileName")) : nullptr;
        FName Custom = UCollisionProfile::CustomCollisionProfileName;
        if (!Profile || Profile->ArrayDim != 1 || !Profile->HasAnyPropertyFlags(CPF_Edit) || !Profile->ExportText_Direct(CollisionProfileExport, &Custom, &Custom, C.Skeletal, PPF_None)) { Fail(R, TEXT("blueprint_skeletal_collision_profile_unobservable"), TEXT("Exact reflected public collision callback-name comparison must be admitted before Modify.")); return Encode(R); }
    }
    auto Expected = MakeShared<FJsonObject>(); if (MeshAsset) { Expected->SetStringField(TEXT("skeletal_mesh"), MeshAsset->GetPathName()); }
    if (AnimationClass) { Expected->SetStringField(TEXT("anim_class"), AnimationClass->GetPathName()); Expected->SetNumberField(TEXT("animation_mode"), static_cast<uint8>(EAnimationMode::AnimationBlueprint)); }
    if (Physics->Type != EJson::Null) { Expected->SetStringField(TEXT("physics_asset_override"), ObjectPath(PhysicsAsset)); }
    if (Simulate->Type != EJson::Null) { Expected->SetBoolField(TEXT("simulate_physics"), Simulate->AsBool()); }
    if (Gravity->Type != EJson::Null) { Expected->SetBoolField(TEXT("enable_gravity"), Gravity->AsBool()); }
    if (Collision->Type != EJson::Null) { Expected->SetNumberField(TEXT("collision_enabled_value"), static_cast<uint8>(CollisionValue)); }
    if (Visibility->Type != EJson::Null) { Expected->SetNumberField(TEXT("visibility_based_anim_tick_option_value"), static_cast<uint8>(VisibilityValue)); }
    if (Response->Type != EJson::Null) { TArray<TSharedPtr<FJsonValue>> A; for (int32 I = 0; I < static_cast<int32>(ECC_MAX); ++I) { A.Add(MakeShared<FJsonValueNumber>(static_cast<uint8>(ResponseValue))); } Expected->SetArrayField(TEXT("collision_response_values"), A); }
    R.Data->SetObjectField(TEXT("requested_readback"), Expected);
    auto Match = [&Expected](const TSharedRef<FJsonObject>& Actual) { for (const auto& P : Expected->Values) { if (!Equal(P.Value, Actual->TryGetField(P.Key.ToView()))) { return false; } } return true; };
    if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    R.Stage = TEXT("set_skeletal_defaults"); R.bAttempted = true; C.Blueprint->Modify(); Count(R, TEXT("modify_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } C.Skeletal->Modify(); Count(R, TEXT("modify_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    if (MeshAsset) { C.Skeletal->SetSkeletalMeshAsset(CastChecked<USkeletalMesh>(MeshAsset)); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    if (AnimationClass) { C.Skeletal->SetAnimationMode(EAnimationMode::AnimationBlueprint); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } C.Skeletal->SetAnimInstanceClass(CastChecked<UClass>(AnimationClass)); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    if (Physics->Type != EJson::Null) { C.Skeletal->SetPhysicsAsset(Cast<UPhysicsAsset>(PhysicsAsset), false); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    bool CollisionInvalidated = false;
    if (Collision->Type != EJson::Null) { CollisionInvalidated = C.Skeletal->BodyInstance.GetCollisionEnabled(false) != CollisionValue; C.Skeletal->SetCollisionEnabled(CollisionValue); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    if (Response->Type != EJson::Null)
    {
        for (int32 I = 0; I < static_cast<int32>(ECC_MAX); ++I) { CollisionInvalidated |= C.Skeletal->BodyInstance.GetResponseToChannel(static_cast<ECollisionChannel>(I)) != ResponseValue; }
        C.Skeletal->SetCollisionResponseToAllChannels(ResponseValue); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    }
    if (CollisionInvalidated)
    {
        R.Data->SetBoolField(TEXT("collision_profile_invalidation_expected"), true); R.Data->SetStringField(TEXT("expected_collision_profile_export"), CollisionProfileExport);
        R.Data->SetStringField(TEXT("collision_profile_callback_scope"), TEXT("only exact selected reflected CollisionProfileName row must equal public CustomCollisionProfileName after requested actually-changed collision/response setter; raw before/after untouched; unreflected protected cache fields unobserved"));
    }
    if (Gravity->Type != EJson::Null) { C.Skeletal->SetEnableGravity(Gravity->AsBool()); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    if (Visibility->Type != EJson::Null) { C.Skeletal->VisibilityBasedAnimTickOption = VisibilityValue; Count(R, TEXT("setter_calls")); }
    if (Simulate->Type != EJson::Null) { C.Skeletal->SetSimulatePhysics(Simulate->AsBool()); Count(R, TEXT("setter_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } }
    C.Skeletal->PostEditChange(); Count(R, TEXT("post_edit_change_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); } FBlueprintEditorUtils::MarkBlueprintAsModified(C.Blueprint.Get()); Count(R, TEXT("mark_blueprint_modified_calls")); if (!CurrentDerivedTarget(C, R)) { return Encode(R); }
    FObservation Configured; if (!Capture(C, Configured, R, TEXT("after_configuration"))) { Fail(R, TEXT("blueprint_skeletal_configuration_unobservable"), TEXT("Public setter/notification prefix incomplete; compile/save suppressed."), true); return Encode(R); }
    if (!Fences(Before, Configured, R)) { return Encode(R); } if (!Match(Configured.State->GetObjectField(TEXT("selected_values")).ToSharedRef())) { Fail(R, TEXT("blueprint_skeletal_readback_failed"), TEXT("Actual requested stored defaults differ after public setters; compile/save suppressed.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("after_configuration_readback_passed"), true); FObservation After = Configured;
    if (bCompile && !Compile(C, Before, After, R)) { return Encode(R); }
    if (!bCompile) { R.Data->SetObjectField(TEXT("after_observation"), Observation(After)); R.Data->SetBoolField(TEXT("validation_pass"), false); R.Data->SetStringField(TEXT("validation_note"), TEXT("Save requested without compile is original-supported but not compiler validated.")); }
    if (!Match(After.State->GetObjectField(TEXT("selected_values")).ToSharedRef())) { Fail(R, TEXT("blueprint_skeletal_postcompile_readback_failed"), TEXT("Actual current requested defaults differ after compile; no save.")); return Encode(R); }
    R.Data->SetBoolField(TEXT("independent_readback_passed"), true); LegacySkeletal(C, After.State->GetObjectField(TEXT("selected_values")).ToSharedRef(), R); if (!Save(C, After, R)) { return Encode(R); }
    R.Data->SetBoolField(TEXT("compiled"), R.Compiled.IsSet() && R.Compiled.GetValue()); R.Data->SetBoolField(TEXT("saved"), R.bSaved); if (bCompile) { R.Data->SetBoolField(TEXT("validation_pass"), R.Compiled.IsSet() && R.Compiled.GetValue()); }
    R.Stage = TEXT("complete"); R.bSuccess = true; R.bPost = true; return Encode(R);
}
}
